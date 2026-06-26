/**
 * @file    oscillator_pll.c
 * @brief   PLL Oscillator implementations — L5 Design + L6 Problems + L7 Applications
 *
 * @details Implements charge-pump PLL design (Type-II), Type-I PLL design,
 *          loop filter component selection, phase noise computation,
 *          lock time estimation, and stability checking.
 *
 * Knowledge covered:
 *   L1: PLL components (PD, loop filter, VCO, divider)
 *   L3: PLL transfer functions, s-domain analysis
 *   L4: PLL lock condition, stability criterion
 *   L5: Charge-pump PLL design, loop filter optimization
 *   L6: 1 GHz frequency synthesizer
 *   L7: GPS receiver LO, 5G NR frequency synthesis
 *
 * Reference:
 *   Gardner, "Phaselock Techniques" (2005)
 *   Best, "Phase-Locked Loops" (2007)
 *   Razavi (2017), Ch.15
 */

#include "oscillator_pll.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#ifndef K_BOLTZMANN
#define K_BOLTZMANN 1.380649e-23
#endif

/* ─── L5: Charge-Pump PLL Design ──────────────────────────────────────────── */

/**
 * Design a Type-II charge-pump PLL frequency synthesizer.
 *
 * Charge-pump PLL with Phase-Frequency Detector (PFD) is the dominant
 * architecture in modern frequency synthesizers due to:
 *   - Infinite pull-in range (frequency + phase detection)
 *   - Zero steady-state phase error (Type-II)
 *   - Compatibility with CMOS IC design
 *
 * Design procedure:
 *   1. Determine divider ratio N = f_out / f_ref
 *   2. Choose loop bandwidth ω_c = 2π·f_c (f_c < f_ref/10 for stability)
 *   3. Choose phase margin φ_m (45°-60°, 50° typical)
 *   4. Compute loop filter components from ω_c, φ_m, K_d, K_vco, N
 *
 * Key equations:
 *   K_d = I_cp / (2π)  [A/rad] for PFD+charge pump
 *   K_vco = 2π·K_vco_hz  [rad/s/V]
 *
 *   For φ_m at ω_c:
 *     τ₁ = (sec(φ_m) - tan(φ_m)) / ω_c
 *     τ₂ = 1 / (ω_c²·τ₁)
 *
 *   C₁ = (K_d·K_vco / (N·ω_c²)) · sqrt((1+ω_c²τ²₂)/(1+ω_c²τ²₁))
 *   R₂ = τ₂ / C₁
 *   C₂ = C₁ / 10  (secondary pole for ripple suppression)
 */
charge_pump_pll_t charge_pump_pll_design(double ref_freq_hz,
                                            double output_freq_hz,
                                            double loop_bw_hz,
                                            double phase_margin_deg,
                                            double k_vco_hz_per_v,
                                            double i_cp_a)
{
    charge_pump_pll_t pll;
    memset(&pll, 0, sizeof(pll));

    if (ref_freq_hz <= 0.0 || output_freq_hz <= 0.0) return pll;

    /* Basic PLL system parameters */
    pll.system.pll_type = PLL_TYPE_II_CHARGE_PUMP;
    pll.system.ref_freq_hz = ref_freq_hz;
    pll.system.output_freq_hz = output_freq_hz;
    pll.system.vco_free_run_hz = output_freq_hz * 0.95;  /* VCO near target */

    /* Divider ratio */
    pll.system.n_divider = (int)(output_freq_hz / ref_freq_hz + 0.5);
    if (pll.system.n_divider < 1) pll.system.n_divider = 1;
    pll.system.r_divider = 1;

    /* Charge pump current */
    pll.charge_pump_current_a = (i_cp_a > 0.0) ? i_cp_a : 100.0e-6;

    /* Phase detector gain (PFD + charge pump): K_d = I_cp/(2π) A/rad */
    pll.system.kd_v_per_rad = pll.charge_pump_current_a / (2.0 * M_PI);

    /* VCO parameters */
    pll.system.kvco_hz_per_v = k_vco_hz_per_v;
    /* K_vco in rad/s/V */
    double kvco_rad = 2.0 * M_PI * k_vco_hz_per_v;

    /* Loop bandwidth (clamp to reasonable range) */
    if (loop_bw_hz <= 0.0) loop_bw_hz = ref_freq_hz / 20.0;  /* typical: f_ref/10 to f_ref/100 */
    if (loop_bw_hz > ref_freq_hz / 10.0) loop_bw_hz = ref_freq_hz / 10.0;
    pll.system.loop_bandwidth_hz = loop_bw_hz;
    double omega_c = 2.0 * M_PI * loop_bw_hz;

    /* Phase margin */
    if (phase_margin_deg <= 0.0) phase_margin_deg = 50.0;
    if (phase_margin_deg > 80.0) phase_margin_deg = 80.0;
    pll.system.phase_margin_deg = phase_margin_deg;
    double phi_m = phase_margin_deg * M_PI / 180.0;

    /* Compute τ₁ and τ₂ for given φ_m and ω_c */
    double sec_phi = 1.0 / cos(phi_m);
    double tan_phi = tan(phi_m);
    double tau1 = (sec_phi - tan_phi) / omega_c;
    double tau2 = 1.0 / (omega_c * omega_c * tau1);

    pll.filter.tau1_sec = tau1;
    pll.filter.tau2_sec = tau2;

    /* Loop filter component values (for charge pump PLL):
     * C₁ = (K_d·K_vco/(N·ω_c²)) · sqrt((1+ω²_cτ²₂)/(1+ω²_cτ²₁))
     * Note: some derivations express this differently. We use the
     * standard charge-pump PLL design equation. */
    double N = (double)pll.system.n_divider;
    double kd_kvco = pll.system.kd_v_per_rad * kvco_rad;
    double omega_c2 = omega_c * omega_c;
    double sqrt_term = sqrt((1.0 + omega_c2 * tau2 * tau2)
                          / (1.0 + omega_c2 * tau1 * tau1));
    pll.filter.c_farads = (kd_kvco / (N * omega_c2)) * sqrt_term;
    if (pll.filter.c_farads < 1.0e-12) pll.filter.c_farads = 1.0e-12;  /* min practical */

    pll.filter.r2_ohms = tau2 / pll.filter.c_farads;
    pll.filter.r1_ohms = tau1 / pll.filter.c_farads - pll.filter.r2_ohms;
    if (pll.filter.r1_ohms < 0.0) pll.filter.r1_ohms = 0.0;

    /* Secondary capacitor for ripple reduction: C₂ ≈ C₁/10 */
    pll.filter.c2_farads = pll.filter.c_farads / 10.0;
    pll.filter.is_active = 0;  /* passive filter */

    /* Zero/pole frequencies */
    if (pll.filter.r2_ohms > 0.0 && pll.filter.c_farads > 0.0) {
        pll.filter.zero_freq_hz = 1.0 / (2.0 * M_PI * pll.filter.r2_ohms * pll.filter.c_farads);
    }
    if (pll.filter.c2_farads > 0.0 && pll.filter.r2_ohms > 0.0) {
        pll.filter.pole_freq_hz = 1.0 / (2.0 * M_PI * pll.filter.r2_ohms
                                          * (pll.filter.c_farads * pll.filter.c2_farads
                                             / (pll.filter.c_farads + pll.filter.c2_farads)));
    }

    /* Natural frequency and damping factor */
    pll.system.natural_freq_hz = omega_c / (2.0 * M_PI * sqrt(2.0 * tau2 / tau1));
    pll.system.damping_factor = 0.5 * omega_c * tau2;

    /* Lock time estimate for a frequency step */
    pll.system.freq_step_hz = output_freq_hz * 0.01;  /* 1% step */
    pll.system.lock_time_us = pll_lock_time_estimate(&pll.system,
                                                       pll.system.freq_step_hz) * 1.0e6;

    /* Lock/capture range for charge-pump PLL:
     * With PFD, pull-in range is theoretically infinite.
     * Practical limit: VCO tuning range. */
    pll.system.lock_range_hz = k_vco_hz_per_v * 3.0;  /* VCO tuning voltage range */
    pll.system.capture_range_hz = pll.system.lock_range_hz;  /* PFD has no cycle-slip */

    /* Reference spur estimate (simplified) */
    pll.system.spur_level_dbc = -40.0;  /* typical without special care */
    pll.pfd_dead_zone_ps = 50.0;        /* typical PFD dead zone */

    /* Phase noise estimates */
    pll.in_band_pn_dbc_hz = -90.0;  /* typical in-band noise floor */
    pll.vco_pn_at_1mhz_dbc = -120.0;

    return pll;
}

/* ─── L5: Type-I PLL Design ───────────────────────────────────────────────── */

/**
 * Design a basic Type-I PLL with analog multiplier phase detector
 * and passive loop filter.
 *
 * Type-I PLL has one integrator (the VCO) and uses a simple
 * low-pass filter. It has a non-zero steady-state phase error
 * for a frequency step and finite hold-in range.
 *
 * Transfer function (open loop):
 *   G(s) = K_d · F(s) · K_vco / s
 *
 * With a simple RC low-pass filter F(s) = 1/(1+s·τ):
 *   G(s) = K / (s·(1+sτ))  where K = K_d·K_vco/N
 *
 * Closed loop (2nd order):
 *   H(s) = ω²_n / (s² + 2ζω_n s + ω²_n)
 *
 * where ω_n = √(K/τ), ζ = 1/(2√(Kτ))
 */
pll_system_t pll_type1_design(double ref_freq_hz, double output_freq_hz,
                                double kd_v_per_rad, double kvco_hz_per_v)
{
    pll_system_t pll;
    memset(&pll, 0, sizeof(pll));

    if (ref_freq_hz <= 0.0 || output_freq_hz <= 0.0) return pll;

    pll.pll_type = PLL_TYPE_I;
    pll.ref_freq_hz = ref_freq_hz;
    pll.output_freq_hz = output_freq_hz;
    pll.kd_v_per_rad = kd_v_per_rad;
    pll.kvco_hz_per_v = kvco_hz_per_v;

    /* Divider ratio */
    pll.n_divider = (int)(output_freq_hz / ref_freq_hz + 0.5);
    if (pll.n_divider < 1) pll.n_divider = 1;
    pll.r_divider = 1;

    /* For Type-I: choose loop bandwidth < f_ref/20 */
    pll.loop_bandwidth_hz = ref_freq_hz / 50.0;
    pll.damping_factor = 0.707;
    pll.natural_freq_hz = pll.loop_bandwidth_hz / 2.0;
    pll.phase_margin_deg = 65.0;  /* Type-I typically has good PM */

    /* Lock range: Δω_L = K_d·K_vco·F(0)/N = K_d·K_vco/N (F(0)=1 for passive filter) */
    double kvco_rad = 2.0 * M_PI * kvco_hz_per_v;
    pll.lock_range_hz = kd_v_per_rad * kvco_hz_per_v / (double)pll.n_divider;

    /* Capture range (approximate for Type-I): Δω_c ≈ √(2ζω_n·Δω_L) */
    if (pll.lock_range_hz > 0.0 && pll.natural_freq_hz > 0.0) {
        pll.capture_range_hz = sqrt(2.0 * pll.damping_factor * pll.natural_freq_hz
                                     * pll.lock_range_hz);
    }

    pll.lock_time_us = 500.0;  /* Type-I slower than Type-II */

    return pll;
}

/* ─── L3: PLL Transfer Function ───────────────────────────────────────────── */

/**
 * Compute PLL closed-loop transfer function magnitude at a given frequency.
 *
 * For a Type-II charge-pump PLL:
 *   G(s) = (I_cp/(2π)) · Z_f(s) · (K_vco/s) · (1/N)
 *
 *   Z_f(s) = (1 + s·R₂·C₁) / (s·(C₁+C₂) + s²·R₂·C₁·C₂)
 *           ≈ (1 + s·R₂·C₁) / (s·C₁)  for C₂ << C₁
 *
 *   H(s) = G(s) / (1 + G(s))
 */
double pll_transfer_magnitude(const pll_system_t *pll, double freq_hz)
{
    if (!pll || freq_hz <= 0.0) return 0.0;

    double s_mag = 2.0 * M_PI * freq_hz;  /* |s| = ω */

    /* Simplified Type-II with ideal integrator loop filter:
     * G(s) ≈ K/(s²) · (1+sτ₂)/(1+sτ₁) where K = K_d·K_vco/(N·τ₁)
     * This is a simplification for magnitude estimation. */

    double K = pll->kd_v_per_rad * (2.0 * M_PI * pll->kvco_hz_per_v)
               / (double)pll->n_divider;

    /* For a basic 2nd-order Type-II PLL:
     * |H(jω)| = 1/sqrt((1 - ω²/ω²_n)² + (2ζω/ω_n)²)  (low-pass behavior) */
    if (pll->natural_freq_hz <= 0.0) return 1.0;

    double omega_n = 2.0 * M_PI * pll->natural_freq_hz;
    double omega = s_mag;
    double omega_ratio = omega / omega_n;
    double zeta = pll->damping_factor;

    double denom = sqrt(pow(1.0 - omega_ratio * omega_ratio, 2.0)
                        + pow(2.0 * zeta * omega_ratio, 2.0));

    return (denom > 0.0) ? 1.0 / denom : 0.0;
}

/* ─── L5: PLL Phase Noise ─────────────────────────────────────────────────── */

/**
 * Compute output phase noise of a charge-pump PLL at a given offset.
 *
 * The PLL output phase noise has contributions from:
 *   1. Reference noise (shaped by low-pass PLL response)
 *   2. PFD/CP noise (in-band noise floor)
 *   3. VCO noise (shaped by high-pass PLL response)
 *   4. Divider noise
 *
 * The PLL filters the reference/PFD noise with its closed-loop
 * low-pass response and the VCO noise with a high-pass response:
 *
 *   S_out(f) = S_ref·|H(f)|² + S_pfd·|H(f)|²/N² + S_vco·|1-H(f)|²
 *
 * Simplified: at offset << loop BW, VCO noise is suppressed.
 * At offset >> loop BW, reference noise is suppressed.
 */
double charge_pump_pll_phase_noise(const charge_pump_pll_t *pll, double offset_hz)
{
    if (!pll || offset_hz <= 0.0) return 0.0;

    /* |H(f)|: closed-loop transfer function magnitude at this offset */
    double H_mag = pll_transfer_magnitude(&pll->system, offset_hz);
    double H_mag_sq = H_mag * H_mag;
    double one_minus_H_sq = (1.0 - H_mag) * (1.0 - H_mag);

    /* Reference noise: assume -140 dBc/Hz at PFD input (typical TCXO) */
    double ref_noise = -140.0;
    double ref_noise_linear = pow(10.0, ref_noise / 10.0);

    /* PFD/CP in-band noise: ~ -210 to -220 dBc/Hz normalized to 1 Hz,
     * converted to output by adding 20·log(N) */
    double pfd_noise_normalized = -215.0;
    double N = (double)pll->system.n_divider;
    double pfd_noise = pfd_noise_normalized + 20.0 * log10(N);
    double pfd_noise_linear = pow(10.0, pfd_noise / 10.0);

    /* VCO noise: simplified f² model;
     * PN at offset = PN_1MHz + 20·log(f_offset/1MHz) */
    double vco_noise = pll->vco_pn_at_1mhz_dbc
                        + 20.0 * log10(offset_hz / 1.0e6);
    double vco_noise_linear = pow(10.0, vco_noise / 10.0);

    /* Total output noise */
    double total_linear = ref_noise_linear * H_mag_sq * N * N
                          + pfd_noise_linear * H_mag_sq
                          + vco_noise_linear * one_minus_H_sq;

    if (total_linear > 1.0e-30) {
        return 10.0 * log10(total_linear);
    }
    return -200.0;
}

/* ─── L5: Lock Time Estimation ────────────────────────────────────────────── */

/**
 * Estimate PLL lock time for a frequency step.
 *
 * For a second-order PLL with damping factor ζ:
 *   t_lock ≈ 4/(ζ·ω_n)  (to settle within ~1% of final value)
 *
 * More precise formula (including frequency step magnitude):
 *   t_lock ≈ -ln(tolerance·√(1-ζ²)) / (ζ·ω_n)
 *
 * where tolerance is the acceptable remaining frequency error as a
 * fraction of the frequency step.
 */
double pll_lock_time_estimate(const pll_system_t *pll, double freq_step_hz)
{
    if (!pll || pll->natural_freq_hz <= 0.0) return 0.0;

    double omega_n = 2.0 * M_PI * pll->natural_freq_hz;
    double zeta = pll->damping_factor;
    if (zeta <= 0.0) zeta = 0.707;

    /* Tolerance: settle to 1% of step */
    double tolerance = 0.01;
    double sqrt_term = sqrt(fabs(1.0 - zeta * zeta));
    double lock_time;

    if (zeta < 1.0) {
        /* Underdamped: exponential envelope decay */
        lock_time = -log(tolerance * sqrt_term) / (zeta * omega_n);
    } else if (zeta > 1.0) {
        /* Overdamped: slower of the two real poles */
        double pole_slow = omega_n * (zeta - sqrt(zeta * zeta - 1.0));
        lock_time = -log(tolerance) / pole_slow;
    } else {
        /* Critically damped */
        lock_time = 4.0 / (zeta * omega_n);
    }

    return lock_time;
}

/* ─── L5: Loop Filter Design ──────────────────────────────────────────────── */

/**
 * Design loop filter components for a charge-pump PLL.
 *
 * Given the loop bandwidth ω_c and phase margin φ_m, computes
 * the resistor and capacitor values.
 */
void pll_loop_filter_design(charge_pump_pll_t *pll)
{
    if (!pll) return;

    double omega_c = 2.0 * M_PI * pll->system.loop_bandwidth_hz;
    double phi_m = pll->system.phase_margin_deg * M_PI / 180.0;

    /* τ₁ = (sec(φ_m) - tan(φ_m)) / ω_c */
    double sec_phi = 1.0 / cos(phi_m);
    double tan_phi = tan(phi_m);
    double tau1 = (sec_phi - tan_phi) / omega_c;
    double tau2 = 1.0 / (omega_c * omega_c * tau1);

    pll->filter.tau1_sec = tau1;
    pll->filter.tau2_sec = tau2;

    /* K_d = I_cp/(2π), K_vco in rad/s/V */
    double kd = pll->charge_pump_current_a / (2.0 * M_PI);
    double kvco_rad = 2.0 * M_PI * pll->system.kvco_hz_per_v;
    double N = (double)pll->system.n_divider;

    /* C₁ calculation */
    double omega_c2 = omega_c * omega_c;
    double sqrt_term = sqrt((1.0 + omega_c2 * tau2 * tau2)
                          / (1.0 + omega_c2 * tau1 * tau1));
    pll->filter.c_farads = (kd * kvco_rad / (N * omega_c2)) * sqrt_term;
    if (pll->filter.c_farads < 1.0e-12) pll->filter.c_farads = 1.0e-12;

    pll->filter.r2_ohms = tau2 / pll->filter.c_farads;
    pll->filter.r1_ohms = tau1 / pll->filter.c_farads - pll->filter.r2_ohms;
    if (pll->filter.r1_ohms < 0.0) pll->filter.r1_ohms = 0.0;

    /* Secondary capacitor */
    pll->filter.c2_farads = pll->filter.c_farads / 10.0;

    /* Zero and pole frequencies */
    if (pll->filter.r2_ohms > 0.0 && pll->filter.c_farads > 0.0) {
        pll->filter.zero_freq_hz = 1.0 / (2.0 * M_PI * pll->filter.r2_ohms
                                            * pll->filter.c_farads);
    }
    if (pll->filter.c2_farads > 0.0) {
        double c_series = (pll->filter.c_farads * pll->filter.c2_farads)
                          / (pll->filter.c_farads + pll->filter.c2_farads);
        if (pll->filter.r2_ohms > 0.0 && c_series > 0.0) {
            pll->filter.pole_freq_hz = 1.0 / (2.0 * M_PI * pll->filter.r2_ohms * c_series);
        }
    }
}

/* ─── L4: PLL Stability Check ─────────────────────────────────────────────── */

/**
 * Check PLL loop stability.
 *
 * A PLL is stable if:
 *   1. Phase margin > 0° (typically > 45° for good transient response)
 *   2. Gain margin > 0 dB
 *   3. Loop bandwidth < f_ref/10 (for discrete-time approximation to hold)
 */
int pll_stability_check(const pll_system_t *pll)
{
    if (!pll) return 0;

    /* Phase margin check */
    if (pll->phase_margin_deg <= 0.0) return 0;

    /* Loop bandwidth vs reference frequency */
    if (pll->loop_bandwidth_hz > pll->ref_freq_hz / 10.0) return 0;

    /* Damping factor reasonable */
    if (pll->damping_factor <= 0.0 || pll->damping_factor > 5.0) return 0;

    /* Divider ratio valid */
    if (pll->n_divider < 1) return 0;

    return 1;
}

/* ─── L4: PLL Range Computation ───────────────────────────────────────────── */

/**
 * Compute lock range and capture range for a PLL.
 *
 * Type-I PLL:
 *   Hold-in (lock) range:  Δω_H = ±K_vco·K_d·F(0)
 *   Pull-in range:         Δω_P ≈ Δω_H (Type-I tracks frequency errors)
 *   Capture range:         Δω_c ≈ √(2ζω_n·Δω_H)  (no cycle slipping)
 *
 * Type-II PLL:
 *   Hold-in range:  limited by VCO tuning range
 *   Capture range:  equal to hold-in range (PFD detects frequency)
 *   Pull-in range:  infinite in theory (PFD prevents cycle slipping)
 */
void pll_range_compute(pll_system_t *pll)
{
    if (!pll) return;

    double kvco_rad = 2.0 * M_PI * pll->kvco_hz_per_v;

    if (pll->pll_type == PLL_TYPE_I) {
        /* Type-I: lock range determined by DC loop gain */
        pll->lock_range_hz = pll->kd_v_per_rad * pll->kvco_hz_per_v
                              / (double)pll->n_divider;

        /* Capture range approximation */
        if (pll->natural_freq_hz > 0.0) {
            pll->capture_range_hz = sqrt(2.0 * pll->damping_factor
                                          * pll->natural_freq_hz
                                          * pll->lock_range_hz);
        } else {
            pll->capture_range_hz = pll->lock_range_hz * 0.1;
        }
    } else {
        /* Type-II: lock range = VCO tuning range (limited by VCO, not loop) */
        pll->lock_range_hz = pll->kvco_hz_per_v * 2.0;  /* assume 2V tuning */
        pll->capture_range_hz = pll->lock_range_hz;
    }
}
