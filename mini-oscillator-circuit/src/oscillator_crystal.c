/**
 * @file    oscillator_crystal.c
 * @brief   Crystal Oscillator implementations — L4 Laws + L5 Design + L6 Problems
 *
 * @details Implements crystal equivalent circuit modeling, Pierce and
 *          Colpitts crystal oscillator design, Barkhausen verification,
 *          drive level checking, and start-up time estimation.
 *
 * Knowledge covered:
 *   L1: Crystal equivalent circuit (Butterworth-Van Dyke model)
 *   L3: Crystal impedance vs frequency (reactance curve)
 *   L4: Series/parallel resonance, pullability formula
 *   L5: Pierce oscillator design, load capacitance selection
 *   L6: 16 MHz MCU clock oscillator, 32.768 kHz watch crystal
 *
 * Reference:
 *   Vittoz et al., "High-Performance Crystal Oscillator Circuits" (1988)
 *   Sedra & Smith (2020), Ch.17.5
 *   E. A. Gerber & A. Ballato, "Precision Frequency Control" (1985)
 */

#include "oscillator_crystal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Boltzmann constant */
#ifndef K_BOLTZMANN
#define K_BOLTZMANN 1.380649e-23
#endif

/* ─── L1: Crystal Model Creation ─────────────────────────────────────────── */

/**
 * Create a crystal equivalent circuit model from nominal frequency.
 *
 * Typical crystal parameters scale with frequency:
 *   C₁ (motional capacitance) ≈ 0.01-0.03 pF for most crystals
 *   L₁ (motional inductance) is chosen so f_s = 1/(2π√(L₁C₁))
 *   R₁ (motional resistance) scales: lower freq → higher R₁
 *   C₀ (shunt capacitance) ≈ 3-7 pF for most packages
 *
 * Q ≈ ω₀·L₁/R₁, typically 10,000 - 2,000,000
 *   AT-cut fundamental: Q ≈ 50,000 - 200,000
 *   Tuning-fork 32.768 kHz: Q ≈ 40,000 - 80,000
 */
crystal_model_t crystal_model_create(double nominal_freq_hz,
                                       double c0_farads, double q_typical)
{
    crystal_model_t crystal;
    memset(&crystal, 0, sizeof(crystal));

    if (nominal_freq_hz <= 0.0) return crystal;

    crystal.nominal_freq_hz = nominal_freq_hz;

    /* Shunt capacitance C₀: typically 1-7 pF */
    if (c0_farads > 0.0) {
        crystal.c0_farads = c0_farads;
    } else {
        /* Typical package + holder capacitance */
        if (nominal_freq_hz > 1.0e6) {
            crystal.c0_farads = 5.0e-12;  /* 5 pF for MHz crystals */
        } else {
            crystal.c0_farads = 1.5e-12;  /* 1.5 pF for kHz tuning forks */
        }
    }

    /* Motional capacitance C₁: typically very small */
    if (nominal_freq_hz > 1.0e6) {
        crystal.c1_farads = 0.02e-12;  /* 20 fF typical for AT-cut */
    } else {
        crystal.c1_farads = 0.003e-12;  /* 3 fF for 32 kHz fork */
    }

    /* Motional inductance L₁: from f_s = 1/(2π√(L₁C₁)) */
    double omega_s = 2.0 * M_PI * nominal_freq_hz;
    crystal.l1_henries = 1.0 / (omega_s * omega_s * crystal.c1_farads);

    /* Series resonance */
    crystal.fs_hz = 1.0 / (2.0 * M_PI * sqrt(crystal.l1_henries * crystal.c1_farads));

    /* Motional resistance R₁: from Q = ωL₁/R₁ */
    if (q_typical > 0.0) {
        crystal.q_factor = q_typical;
    } else {
        /* Typical Q values */
        if (nominal_freq_hz > 1.0e6) {
            crystal.q_factor = 50000.0;
        } else {
            crystal.q_factor = 50000.0;
        }
    }
    crystal.r1_ohms = omega_s * crystal.l1_henries / crystal.q_factor;

    /* Parallel resonance: f_p = f_s · √(1 + C₁/C₀) */
    crystal.fp_hz = crystal.fs_hz * sqrt(1.0 + crystal.c1_farads / crystal.c0_farads);
    crystal.delta_f_hz = crystal.fp_hz - crystal.fs_hz;

    return crystal;
}

/* ─── L1: Crystal Impedance ──────────────────────────────────────────────── */

/**
 * Compute crystal impedance at a given frequency.
 *
 * The crystal equivalent circuit is: (L₁, C₁, R₁ in series) || C₀.
 *
 * Z_motional = R₁ + jωL₁ + 1/(jωC₁)
 * Z_crystal  = Z_motional || (1/(jωC₀))
 *
 * At series resonance (f_s): Z ≈ R₁ (purely resistive, minimum)
 * At parallel resonance (f_p): Z → high impedance peak
 * Between f_s and f_p: Z is inductive (positive reactance)
 * Below f_s and above f_p: Z is capacitive (negative reactance)
 *
 * The crystal oscillates in the inductive region (between f_s and f_p).
 */
void crystal_impedance(const crystal_model_t *crystal, double freq_hz,
                         double *re, double *im)
{
    if (!crystal || !re || !im) return;

    if (freq_hz <= 0.0) {
        *re = 1.0e12;  /* effectively open at DC */
        *im = -1.0e12;
        return;
    }

    double omega = 2.0 * M_PI * freq_hz;

    /* Motional arm impedance: Z_m = R₁ + j(ωL₁ - 1/(ωC₁)) */
    double z_m_re = crystal->r1_ohms;
    double z_m_im = omega * crystal->l1_henries - 1.0 / (omega * crystal->c1_farads);

    /* Shunt capacitance impedance: Z_c0 = -j/(ωC₀) */
    double z_c0_re = 0.0;
    double z_c0_im = -1.0 / (omega * crystal->c0_farads);

    /* Parallel combination: Z_total = Z_m · Z_c0 / (Z_m + Z_c0) */
    /* Product: (a+jb)(-jc) = bc - jac where Z_m=a+jb, Z_c0=-jc */
    double prod_re = z_m_im * (-z_c0_im);  /* = -b*c0_im? Actually: (a+jb)*(0-jc0im)
                                             = a*0 - a*j*c0im + jb*0 - jb*j*c0im
                                             = -j*a*c0im + b*c0im
                                             = b*c0im - j*a*c0im */
    double prod_im = -z_m_re * (-z_c0_im);  /* let me redo */
    /* Z_m = a + jb, Z_c0 = 0 + jc where c = -1/(ωC0) */
    double a = z_m_re;
    double b = z_m_im;
    double c = -1.0 / (omega * crystal->c0_farads);

    /* Product: (a + jb) * (jc) = jac - bc = (-bc) + j(ac) */
    prod_re = -b * c;
    prod_im = a * c;

    /* Sum: Z_m + Z_c0 = a + j(b + c) */
    double sum_re = a;
    double sum_im = b + c;

    /* Division: Z_total = product / sum */
    double denom = sum_re * sum_re + sum_im * sum_im;
    if (denom > 0.0) {
        *re = (prod_re * sum_re + prod_im * sum_im) / denom;
        *im = (prod_im * sum_re - prod_re * sum_im) / denom;
    } else {
        /* At exact series resonance where sum ≈ 0 */
        *re = crystal->r1_ohms;
        *im = 0.0;
    }
}

/* ─── L5: Pierce Oscillator Design ────────────────────────────────────────── */

/**
 * Design a Pierce crystal oscillator — the most common configuration.
 *
 * The Pierce oscillator uses a CMOS inverter (or BJT) with:
 *   - Feedback resistor R_f (biases inverter in linear region)
 *   - Crystal between output and input
 *   - Load capacitors C₁ (output-to-GND), C₂ (input-to-GND)
 *
 * The inverter provides approximately 180° phase shift at its -3dB point.
 * The crystal + load capacitors provide the remaining 180° for a total of
 * 360° (0°), satisfying the phase condition of Barkhausen.
 *
 * Load capacitance: C_L = C₁·C₂/(C₁+C₂) + C_stray
 *
 * Negative resistance of the inverter:
 *   R_neg = -g_m / (ω²·C₁·C₂)
 *
 * For reliable start-up: |R_neg| > 5-10 × R₁(max)
 *
 * Frequency pulling: Δf/f_s ≈ C₁/(2(C₀+C_L))
 */
pierce_osc_t pierce_design(double crystal_freq_hz, double target_cl_farads,
                             double supply_v)
{
    pierce_osc_t osc;
    memset(&osc, 0, sizeof(osc));

    if (crystal_freq_hz <= 0.0) return osc;

    /* Create crystal model */
    osc.crystal = crystal_model_create(crystal_freq_hz, 0.0, 0.0);

    /* Load capacitance: typical 12-20 pF for MHz crystals */
    if (target_cl_farads <= 0.0) {
        if (crystal_freq_hz > 1.0e6) {
            target_cl_farads = 18.0e-12;  /* 18 pF typical */
        } else {
            target_cl_farads = 12.5e-12;  /* 12.5 pF typical for 32 kHz */
        }
    }

    /* C_stray: PCB + pin capacitance, typically 2-5 pF */
    osc.c_stray_farads = 3.0e-12;

    /* For symmetric design: C₁ ≈ C₂. Then C_L = C₁/2 + C_stray.
     * So C₁ = C₂ = 2·(C_L - C_stray) */
    double cl_minus_stray = target_cl_farads - osc.c_stray_farads;
    if (cl_minus_stray < 2.0e-12) cl_minus_stray = 5.0e-12;

    osc.c1_farads = 2.0 * cl_minus_stray;
    osc.c2_farads = 2.0 * cl_minus_stray;

    /* Actual C_L */
    osc.c_load_farads = (osc.c1_farads * osc.c2_farads) / (osc.c1_farads + osc.c2_farads)
                        + osc.c_stray_farads;

    /* Feedback resistor: typically 1 MΩ for CMOS, 100 kΩ-1 MΩ */
    if (crystal_freq_hz > 1.0e6) {
        osc.rf_ohms = 1.0e6;
    } else {
        osc.rf_ohms = 10.0e6;  /* Higher for low-frequency crystals */
    }

    /* Current limiting resistor R_d: often 0, or ~1k for drive level control */
    osc.rd_ohms = 0.0;

    /* Minimum gm for start-up (conservative estimate):
     * g_m_crit = ω²·C₁·C₂·R₁·(1 + C₀/C_L)²
     * This is the critical gm for oscillation. We need 5-10x margin. */
    double omega = 2.0 * M_PI * crystal_freq_hz;
    double factor = 1.0 + osc.crystal.c0_farads / osc.c_load_farads;
    osc.gm_min_siemens = omega * omega * osc.c1_farads * osc.c2_farads
                          * osc.crystal.r1_ohms * factor * factor;

    /* Add start-up margin: 5x minimum */
    double gm_design = osc.gm_min_siemens * 5.0;

    /* Negative resistance: R_neg = -g_m/(ω²·C₁·C₂) */
    osc.r_neg_ohms = -gm_design / (omega * omega * osc.c1_farads * osc.c2_farads);

    /* Frequency pulling from load capacitance */
    osc.freq_pull_ppm = crystal_pullability_ppm(&osc.crystal, osc.c_load_farads);
    osc.freq_osc_hz = osc.crystal.fs_hz * (1.0 + osc.freq_pull_ppm * 1.0e-6);

    /* Drive level estimation:
     * P_xtal ≈ (V_dd²)·(ω·C₁·C₂/(C₁+C₂))²·R₁ / 2  [approximate]
     * More accurately: P = I²_rms · R₁ where I ≈ V_dd·ω·C_L */
    if (supply_v <= 0.0) supply_v = 3.3;
    double i_rms = supply_v * omega * osc.c_load_farads / sqrt(2.0);
    osc.drive_level_uw = i_rms * i_rms * osc.crystal.r1_ohms * 1.0e6;
    osc.drive_level_ok = (osc.drive_level_uw <= 100.0) ? 1 : 0;

    return osc;
}

/* ─── L5: Colpitts Crystal Oscillator Design ──────────────────────────────── */

/**
 * Design a Colpitts crystal oscillator where the crystal replaces
 * the inductor in a standard Colpitts topology.
 *
 * The crystal operates in its inductive region (between f_s and f_p).
 * The effective inductance at the oscillation frequency, together with
 * C₁ and C₂, determines the exact frequency.
 */
colpitts_crystal_osc_t colpitts_crystal_design(double crystal_freq_hz,
                                                  double feedback_ratio)
{
    colpitts_crystal_osc_t osc;
    memset(&osc, 0, sizeof(osc));

    if (crystal_freq_hz <= 0.0) return osc;

    if (feedback_ratio <= 0.0) feedback_ratio = 0.3;
    if (feedback_ratio >= 1.0) feedback_ratio = 0.5;

    /* Crystal model */
    osc.crystal = crystal_model_create(crystal_freq_hz, 0.0, 0.0);

    /* Choose C₁, C₂ based on feedback ratio */
    double c_total = 50.0e-12;  /* 50 pF total is reasonable */
    osc.c2_farads = c_total * feedback_ratio;
    osc.c1_farads = c_total - osc.c2_farads;

    if (osc.c1_farads > 0.0 && osc.c2_farads > 0.0) {
        osc.c_eq_farads = (osc.c1_farads * osc.c2_farads) / (osc.c1_farads + osc.c2_farads);
    }

    /* The oscillation frequency is between f_s and f_p.
     * For the crystal acting as an inductor, the effective inductance
     * L_eff at the operating frequency is found from the condition that
     * the capacitive divider + crystal produce 0° phase shift.
     * Simplification: f_osc is very close to f_s, pulled slightly by C_eq. */
    osc.freq_osc_hz = osc.crystal.fs_hz
                       * (1.0 + osc.crystal.c1_farads
                              / (2.0 * (osc.crystal.c0_farads + osc.c_eq_farads)));

    /* Effective inductance of crystal at f_osc:
     * From f_osc = 1/(2π√(L_eff·C_eq)):
     * L_eff = 1/((2π·f_osc)²·C_eq) */
    double omega_osc = 2.0 * M_PI * osc.freq_osc_hz;
    if (osc.c_eq_farads > 0.0) {
        osc.effective_inductance_h = 1.0 / (omega_osc * omega_osc * osc.c_eq_farads);
    }

    osc.gm_min_siemens = 1.0e-3;

    return osc;
}

/* ─── L4: Crystal Pullability ─────────────────────────────────────────────── */

/**
 * Compute frequency pulling in ppm for a given load capacitance.
 *
 * The oscillation frequency is pulled from f_s toward f_p by the
 * load capacitance C_L:
 *
 *   Δf/f_s = C₁ / (2·(C₀ + C_L))
 *
 * This formula comes from the parallel resonance shift:
 *   f_L = f_s · (1 + C₁/(2·(C₀+C_L)))
 *
 * Pullability is critical for frequency tuning: the oscillator
 * frequency can be adjusted by changing the load capacitance.
 * A trimmer capacitor or varactor can be used for fine tuning.
 *
 * For a typical 10 MHz crystal with C₁=0.02 pF, C₀=5 pF:
 *   C_L=18 pF → pull ≈ 0.02/(2·23) ≈ 435 ppm
 */
double crystal_pullability_ppm(const crystal_model_t *crystal, double c_load_farads)
{
    if (!crystal || crystal->c1_farads <= 0.0) return 0.0;

    double denom = 2.0 * (crystal->c0_farads + c_load_farads);
    if (denom <= 0.0) return 0.0;

    return (crystal->c1_farads / denom) * 1.0e6;  /* convert to ppm */
}

/* ─── L4: Barkhausen Verification ─────────────────────────────────────────── */

/**
 * Verify Barkhausen criterion for Pierce oscillator.
 *
 * The Pierce oscillator satisfies Barkhausen when the inverter provides
 * sufficient negative resistance to overcome the crystal's motional loss.
 *
 * Condition: |R_neg| > R₁·(1 + C₀/C_L)²
 *
 * This accounts for the impedance transformation of the π-network.
 */
barkhausen_criterion_t pierce_verify_barkhausen(const pierce_osc_t *osc)
{
    barkhausen_criterion_t result;
    memset(&result, 0, sizeof(result));

    if (!osc || osc->freq_osc_hz <= 0.0) return result;

    /* Critical resistance to overcome */
    double factor = 1.0 + osc->crystal.c0_farads / osc->c_load_farads;
    double r_crit = osc->crystal.r1_ohms * factor * factor;

    /* Loop gain = |R_neg| / R_crit */
    double r_neg_mag = fabs(osc->r_neg_ohms);
    double loop_mag = (r_crit > 0.0) ? r_neg_mag / r_crit : 100.0;
    double loop_phase = 0.0;

    return barkhausen_evaluate(loop_mag, loop_phase, osc->freq_osc_hz);
}

/* ─── L5: Drive Level Check ───────────────────────────────────────────────── */

/**
 * Check if crystal drive level is within specifications.
 *
 * Excessive drive can cause:
 *   - Frequency drift (heating)
 *   - Increased aging
 *   - Crystal fracture (physical damage)
 *
 * Typical maximum drive levels:
 *   - Standard AT-cut: 100-500 µW
 *   - Tuning fork (32.768 kHz): 1 µW max
 *   - High-stability OCXO crystals: 10-50 µW
 */
int crystal_drive_level_check(const pierce_osc_t *osc, double max_drive_uw)
{
    if (!osc) return 0;
    if (max_drive_uw <= 0.0) max_drive_uw = 100.0;
    return (osc->drive_level_uw <= max_drive_uw) ? 1 : 0;
}

/* ─── L5: Crystal Start-Up Time ───────────────────────────────────────────── */

/**
 * Compute crystal oscillator start-up time.
 *
 * Crystal oscillators have very high Q, leading to long start-up times.
 *
 * The amplitude envelope grows as:
 *   V(t) = V_noise · exp(t/τ) where τ = 2·Q_loaded/ω₀
 *
 * τ_startup = (Q_loaded/π·f₀)·ln(V_final/√(4kTR₁))
 *
 * For a 10 MHz crystal with Q=50,000:
 *   τ ≈ 50,000/(π·10⁷) ≈ 1.6 ms growth time constant
 *   t_start ≈ 1.6ms · ln(3.3V/1µV) ≈ 1.6ms · 15 ≈ 24 ms
 *
 * For a 32.768 kHz crystal with Q=40,000:
 *   τ ≈ 40,000/(π·32768) ≈ 389 ms
 *   t_start ≈ 389ms · ln(3.3V/1µV) ≈ 389ms · 15 ≈ 5.8 seconds!
 */
double crystal_startup_time(const pierce_osc_t *pierce)
{
    if (!pierce || pierce->freq_osc_hz <= 0.0) return 0.0;

    double q_loaded = pierce->crystal.q_factor * 0.8;  /* loaded Q ~80% of unloaded */
    double f0 = pierce->freq_osc_hz;

    /* Thermal noise in the crystal's motional resistance:
     * V_noise_rms = √(4kT·R₁·BW), BW ≈ f₀/Q (resonator bandwidth) */
    double bw = f0 / pierce->crystal.q_factor;
    double v_noise = sqrt(4.0 * K_BOLTZMANN * 290.0 * pierce->crystal.r1_ohms * bw);

    if (v_noise < 1.0e-9) v_noise = 1.0e-9;  /* floor at 1 nV */

    double tau_growth = q_loaded / (M_PI * f0);
    double v_final = 1.0;  /* assume ~1V final amplitude */
    double ratio = v_final / v_noise;

    return tau_growth * log(ratio);
}
