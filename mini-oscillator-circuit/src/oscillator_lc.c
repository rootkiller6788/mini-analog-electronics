/**
 * @file    oscillator_lc.c
 * @brief   LC Oscillator implementations — L4 Laws + L5 Design + L6 Problems
 *
 * @details Implements design and analysis for Colpitts, Hartley, Clapp,
 *          Armstrong, and cross-coupled differential LC oscillators.
 *          Each oscillator type has a distinct feedback mechanism and
 *          frequency formula.
 *
 * Knowledge covered:
 *   L1: LC tank, Colpitts, Hartley, Clapp topologies
 *   L3: LC tank impedance, negative resistance
 *   L4: Oscillation frequency formulas, start-up condition
 *   L5: Component selection, tank Q optimization
 *   L6: 10 MHz Colpitts, 100 MHz Hartley, cross-coupled LC
 *
 * Reference: Sedra & Smith (2020), Ch.17.3
 *            Razavi (2017), Ch.14
 */

#include "oscillator_lc.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ─── Internal helpers ──────────────────────────────────────────────────── */

static void* safe_malloc(size_t sz) {
    void *p = malloc(sz);
    if (!p) { fprintf(stderr, "oscillator_lc: malloc(%zu) failed\n", sz); abort(); }
    return p;
}

/* Standard inductor values (E6, nH to uH range for RF) */
static double nearest_standard_inductor(double target_h) {
    /* For RF oscillators, inductors are typically in nH to uH range */
    static const double std_l[] = {
        1.0e-9, 1.5e-9, 2.2e-9, 3.3e-9, 4.7e-9, 6.8e-9,
        1.0e-8, 1.5e-8, 2.2e-8, 3.3e-8, 4.7e-8, 6.8e-8,
        1.0e-7, 1.5e-7, 2.2e-7, 3.3e-7, 4.7e-7, 6.8e-7,
        1.0e-6, 1.5e-6, 2.2e-6, 3.3e-6, 4.7e-6, 6.8e-6,
        1.0e-5, 1.5e-5, 2.2e-5, 3.3e-5, 4.7e-5, 6.8e-5
    };
    size_t n = sizeof(std_l) / sizeof(std_l[0]);

    if (target_h <= 0.0) return 1.0e-6;  /* default 1 uH */

    double best = std_l[0];
    double best_err = fabs(target_h - best) / target_h;
    for (size_t i = 1; i < n; i++) {
        double err = fabs(target_h - std_l[i]) / target_h;
        if (err < best_err) {
            best_err = err;
            best = std_l[i];
        }
    }
    return best;
}

static double nearest_standard_cap(double target_f) {
    static const double std_c[] = {
        1.0e-12, 1.5e-12, 2.2e-12, 3.3e-12, 4.7e-12, 6.8e-12,
        1.0e-11, 1.5e-11, 2.2e-11, 3.3e-11, 4.7e-11, 6.8e-11,
        1.0e-10, 1.5e-10, 2.2e-10, 3.3e-10, 4.7e-10, 6.8e-10,
        1.0e-9,  1.5e-9,  2.2e-9,  3.3e-9,  4.7e-9,  6.8e-9,
        1.0e-8,  1.5e-8,  2.2e-8,  3.3e-8,  4.7e-8,  6.8e-8,
        1.0e-7,  1.5e-7,  2.2e-7,  3.3e-7,  4.7e-7,  6.8e-7
    };
    size_t n = sizeof(std_c) / sizeof(std_c[0]);

    if (target_f <= 0.0) return 1.0e-10;

    double best = std_c[0];
    double best_err = fabs(target_f - best) / target_f;
    for (size_t i = 1; i < n; i++) {
        double err = fabs(target_f - std_c[i]) / target_f;
        if (err < best_err) {
            best_err = err;
            best = std_c[i];
        }
    }
    return best;
}

/* ─── L1: LC Tank Analysis ───────────────────────────────────────────────── */

/**
 * Analyze an LC tank — compute resonant frequency, Q, characteristic impedance.
 *
 * Parallel LC tank with losses:
 *   f₀ = 1/(2π√(LC)) · √(1 - (CR²/L)) ≈ 1/(2π√(LC)) for high Q
 *   Z₀ = √(L/C)
 *   Q (parallel) = R_p / Z₀
 *   Q (series)   = Z₀ / R_s
 */
lc_tank_t lc_tank_analyze(double l_h, double c_f,
                            double series_r, double parallel_r)
{
    lc_tank_t tank;
    memset(&tank, 0, sizeof(tank));

    if (l_h <= 0.0 || c_f <= 0.0) return tank;

    tank.inductance_h = l_h;
    tank.capacitance_f = c_f;
    tank.series_r_ohms = series_r;
    tank.parallel_r_ohms = parallel_r;

    tank.characteristic_z = sqrt(l_h / c_f);
    double omega0 = 1.0 / sqrt(l_h * c_f);
    tank.resonant_freq_hz = omega0 / (2.0 * M_PI);
    tank.resonant_omega = omega0;

    if (series_r > 0.0) {
        tank.q_unloaded = tank.characteristic_z / series_r;
    } else if (parallel_r > 0.0) {
        tank.q_unloaded = parallel_r / tank.characteristic_z;
        /* Convert parallel resistance to equivalent series for consistency */
        if (tank.q_unloaded > 1.0) {
            tank.series_r_ohms = tank.characteristic_z / tank.q_unloaded;
        }
    } else {
        tank.q_unloaded = 1.0e6;  /* essentially ideal */
    }

    tank.q_loaded = tank.q_unloaded;  /* no external load assumed */
    if (tank.q_loaded > 0.0) {
        tank.bandwidth_3db_hz = tank.resonant_freq_hz / tank.q_loaded;
    }

    if (parallel_r <= 0.0 && tank.q_unloaded > 0.0) {
        tank.parallel_r_ohms = tank.q_unloaded * tank.characteristic_z;
    }

    return tank;
}

/* ─── L5: Colpitts Oscillator Design ─────────────────────────────────────── */

/**
 * Design a Colpitts oscillator.
 *
 * Frequency: f₀ = 1/(2π√(L·C_eq)) where C_eq = C₁·C₂/(C₁+C₂)
 *
 * The capacitive divider provides the feedback voltage to the
 * emitter/source:
 *   β = C₁/(C₁+C₂)  (voltage division ratio)
 *   n = C₂/(C₁+C₂)  (feedback ratio to active device input)
 *
 * For the common-base Colpitts (BJT emitter-input):
 *   Start-up: g_m > ω²·C₁·C₂·R_s
 *   where R_s is the effective series resistance of the tank.
 *
 * Design procedure:
 *   1. Choose L based on frequency and available inductor values
 *   2. Choose feedback ratio n (typically 0.1 to 0.5)
 *   3. Compute C_eq = 1/(ω₀²·L)
 *   4. Compute C₁, C₂ from C_eq and n:
 *      C₂ = C_eq / n
 *      C₁ = C_eq / (1 - n)
 */
colpitts_osc_t colpitts_design(double freq_hz, double feedback_ratio,
                                  double l_chosen_h)
{
    colpitts_osc_t osc;
    memset(&osc, 0, sizeof(osc));
    osc.active_device = 'B';

    if (freq_hz <= 0.0) return osc;

    /* Clamp feedback ratio to reasonable range */
    if (feedback_ratio <= 0.0) feedback_ratio = 0.2;
    if (feedback_ratio >= 1.0) feedback_ratio = 0.5;
    osc.feedback_ratio = feedback_ratio;

    double omega0 = 2.0 * M_PI * freq_hz;

    /* Select inductor */
    if (l_chosen_h > 0.0) {
        osc.inductance_h = nearest_standard_inductor(l_chosen_h);
    } else {
        /* Auto-select L based on frequency */
        if (freq_hz < 1.0e6) {
            osc.inductance_h = nearest_standard_inductor(1.0e-4);  /* ~100 uH for < 1 MHz */
        } else if (freq_hz < 10.0e6) {
            osc.inductance_h = nearest_standard_inductor(1.0e-5);  /* ~10 uH */
        } else if (freq_hz < 100.0e6) {
            osc.inductance_h = nearest_standard_inductor(1.0e-6);  /* ~1 uH */
        } else {
            osc.inductance_h = nearest_standard_inductor(1.0e-7);  /* ~100 nH for > 100 MHz */
        }
    }

    /* Compute C_eq = 1/(ω₀²·L) */
    double c_eq_target = 1.0 / (omega0 * omega0 * osc.inductance_h);

    /* From feedback ratio n = C₂/(C₁+C₂) and C_eq = C₁·C₂/(C₁+C₂):
     *   C_eq = C₁·n where C₁ = C_eq / n? No.
     *   C_eq = C₁·C₂/(C₁+C₂).  Let n = C₂/(C₁+C₂).
     *   Then C₁ = C_eq · (C₁+C₂)/C₂ = C_eq / n.
     *   Wait: C_eq = C₁·C₂/(C₁+C₂) = C₁·n.  So C₁ = C_eq / n.
     *   Then C₂ = n·(C₁+C₂) = n·C₁/(1-n). Or simpler: from n = C₂/(C₁+C₂),
     *   C₁ = C_eq / n, C₂ = C_eq / (1-n).
     */
    if (feedback_ratio > 0.0 && feedback_ratio < 1.0) {
        osc.c1_farads = nearest_standard_cap(c_eq_target / feedback_ratio);
        osc.c2_farads = nearest_standard_cap(c_eq_target / (1.0 - feedback_ratio));
    } else {
        osc.c1_farads = nearest_standard_cap(c_eq_target * 2.0);
        osc.c2_farads = nearest_standard_cap(c_eq_target * 2.0);
    }

    /* Recompute C_eq and frequency */
    if (osc.c1_farads > 0.0 && osc.c2_farads > 0.0) {
        osc.c_eq_farads = (osc.c1_farads * osc.c2_farads) / (osc.c1_farads + osc.c2_farads);
        osc.freq_osc_hz = 1.0 / (2.0 * M_PI * sqrt(osc.inductance_h * osc.c_eq_farads));
        osc.feedback_ratio = osc.c2_farads / (osc.c1_farads + osc.c2_farads);
    }

    /* Minimum gm for start-up: gm_min ≈ ω₀²·C₁·C₂·R_s
     * where R_s is estimated from L and Q (~10Ω for discrete, ~1Ω for IC) */
    double r_s_est = 5.0;  /* Ω, typical series resistance */
    double omega_actual = 2.0 * M_PI * osc.freq_osc_hz;
    osc.gm_min_siemens = omega_actual * omega_actual * osc.c1_farads
                          * osc.c2_farads * r_s_est;

    /* Bias current estimate for BJT: I_C = g_m · V_T (V_T ≈ 26mV) */
    osc.bias_current_ma = osc.gm_min_siemens * 0.026 * 1000.0 * 2.0;

    /* Estimated amplitude */
    osc.amplitude_vpp = 2.0;  /* typical for 5-12V supply */

    return osc;
}

/* ─── L5: Hartley Oscillator Design ──────────────────────────────────────── */

/**
 * Design a Hartley oscillator.
 *
 * Frequency: f₀ = 1/(2π√((L₁+L₂+2M)·C))
 *
 * The tapped inductor provides voltage division feedback.
 * Feedback ratio n = L₂/L_total.
 *
 * For uncoupled inductors (M=0): L_total = L₁+L₂
 * For coupled inductors: L_total = L₁+L₂+2M
 */
hartley_osc_t hartley_design(double freq_hz, double feedback_ratio)
{
    hartley_osc_t osc;
    memset(&osc, 0, sizeof(osc));

    if (freq_hz <= 0.0) return osc;

    if (feedback_ratio <= 0.0) feedback_ratio = 0.2;
    if (feedback_ratio >= 1.0) feedback_ratio = 0.5;
    osc.feedback_ratio = feedback_ratio;

    double omega0 = 2.0 * M_PI * freq_hz;

    /* Choose capacitor first */
    if (freq_hz < 1.0e6) {
        osc.capacitance_f = nearest_standard_cap(1.0e-7);
    } else if (freq_hz < 10.0e6) {
        osc.capacitance_f = nearest_standard_cap(1.0e-9);
    } else if (freq_hz < 100.0e6) {
        osc.capacitance_f = nearest_standard_cap(1.0e-10);
    } else {
        osc.capacitance_f = nearest_standard_cap(1.0e-11);
    }

    /* Compute total inductance */
    double l_total = 1.0 / (omega0 * omega0 * osc.capacitance_f);

    /* From n = L₂/L_total and L_total = L₁+L₂ (uncoupled):
     *   L₂ = n·L_total,  L₁ = L_total - L₂ */
    osc.l2_henries = nearest_standard_inductor(feedback_ratio * l_total);
    osc.l1_henries = nearest_standard_inductor(l_total - osc.l2_henries);
    osc.l_total_henries = osc.l1_henries + osc.l2_henries;
    osc.mutual_m_henries = 0.0;
    osc.coupling_coefficient = 0.0;

    /* Recompute frequency */
    if (osc.l_total_henries > 0.0 && osc.capacitance_f > 0.0) {
        osc.freq_osc_hz = 1.0 / (2.0 * M_PI * sqrt(osc.l_total_henries * osc.capacitance_f));
        osc.feedback_ratio = osc.l2_henries / osc.l_total_henries;
    }

    /* Minimum gm for start-up */
    osc.gm_min_siemens = 1.0e-3;  /* ~1 mS, typical for small-signal BJT */

    return osc;
}

/* ─── L5: Clapp Oscillator Design ─────────────────────────────────────────── */

/**
 * Design a Clapp oscillator (Colpitts with series capacitor for stability).
 *
 * The Clapp adds C₃ in series with the inductor. For C₃ << C₁, C₂:
 *   C_eq ≈ C₃
 *   f₀ ≈ 1/(2π√(L·C₃))
 *
 * The stability factor (C₁·C₂)/C₃² determines how much transistor
 * parasitic capacitance variation affects the frequency. Larger
 * stability factor = better frequency stability.
 */
clapp_osc_t clapp_design(double freq_hz, double stability_factor)
{
    clapp_osc_t osc;
    memset(&osc, 0, sizeof(osc));

    if (freq_hz <= 0.0) return osc;

    if (stability_factor < 1.0) stability_factor = 10.0;
    osc.stability_factor = stability_factor;

    double omega0 = 2.0 * M_PI * freq_hz;

    /* Choose L based on frequency */
    if (freq_hz < 10.0e6) {
        osc.inductance_h = nearest_standard_inductor(1.0e-5);
    } else if (freq_hz < 100.0e6) {
        osc.inductance_h = nearest_standard_inductor(1.0e-6);
    } else {
        osc.inductance_h = nearest_standard_inductor(1.0e-7);
    }

    /* C₃ sets the frequency primarily: C₃ ≈ 1/(ω₀²·L) */
    double c3_target = 1.0 / (omega0 * omega0 * osc.inductance_h);
    osc.c3_farads = nearest_standard_cap(c3_target);

    /* C₁·C₂ = stability_factor · C₃²
     * For simplicity: C₁ = C₂ = √(stability_factor) · C₃ */
    double cx = sqrt(stability_factor) * osc.c3_farads;
    osc.c1_farads = nearest_standard_cap(cx);
    osc.c2_farads = nearest_standard_cap(cx);

    /* Compute actual C_eq */
    double inv_ceq = 1.0/osc.c1_farads + 1.0/osc.c2_farads + 1.0/osc.c3_farads;
    if (inv_ceq > 0.0) {
        osc.c_eq_farads = 1.0 / inv_ceq;
        osc.freq_osc_hz = 1.0 / (2.0 * M_PI * sqrt(osc.inductance_h * osc.c_eq_farads));
    }

    /* Actual stability factor */
    osc.stability_factor = (osc.c1_farads * osc.c2_farads) / (osc.c3_farads * osc.c3_farads);

    /* gm_min estimate */
    osc.gm_min_siemens = 1.0e-3;

    return osc;
}

/* ─── L5: Armstrong Oscillator Design ─────────────────────────────────────── */

/**
 * Design an Armstrong (tickler coil) oscillator.
 *
 * The transformer-coupled feedback via mutual inductance M
 * provides the positive feedback. The oscillation frequency
 * is set by the primary tank LC.
 */
armstrong_osc_t armstrong_design(double freq_hz, double coupling_coeff)
{
    armstrong_osc_t osc;
    memset(&osc, 0, sizeof(osc));

    if (freq_hz <= 0.0) return osc;

    if (coupling_coeff <= 0.0) coupling_coeff = 0.1;
    if (coupling_coeff > 1.0) coupling_coeff = 0.5;
    osc.coupling_coefficient = coupling_coeff;

    double omega0 = 2.0 * M_PI * freq_hz;

    /* Primary tank: choose C, compute L */
    if (freq_hz < 1.0e6) {
        osc.capacitance_f = nearest_standard_cap(1.0e-8);
    } else {
        osc.capacitance_f = nearest_standard_cap(1.0e-10);
    }

    double l_primary = 1.0 / (omega0 * omega0 * osc.capacitance_f);
    osc.l_primary_h = nearest_standard_inductor(l_primary);

    /* Secondary (tickler): typically turns ratio 1:1 to 1:5 */
    osc.turns_ratio = 1.0 / coupling_coeff;  /* larger ratio for weak coupling */
    if (osc.turns_ratio > 10.0) osc.turns_ratio = 5.0;
    osc.l_secondary_h = osc.l_primary_h * osc.turns_ratio * osc.turns_ratio;
    /* Mutual inductance: M = k·√(L₁·L₂) */
    osc.mutual_m_h = coupling_coeff * sqrt(osc.l_primary_h * osc.l_secondary_h);

    /* Recompute frequency */
    if (osc.l_primary_h > 0.0 && osc.capacitance_f > 0.0) {
        osc.freq_osc_hz = 1.0 / (2.0 * M_PI * sqrt(osc.l_primary_h * osc.capacitance_f));
    }

    return osc;
}

/* ─── L5: Cross-Coupled LC Oscillator Design ─────────────────────────────── */

/**
 * Design a cross-coupled differential LC oscillator.
 *
 * This is the dominant topology in CMOS RF ICs. Two cross-coupled
 * transistors provide a negative resistance -2/gm that compensates
 * the parallel tank loss.
 *
 * Start-up condition: gm·R_p > 2  (or gm > 2/R_p)
 *
 * Amplitude: limited by the tail current (current-limited regime)
 *   V_diff_pp ≈ I_tail · R_p  (current-limited)
 *   V_diff_pp ≈ 2·VDD        (voltage-limited — rail-to-rail)
 */
cross_coupled_lc_osc_t cross_coupled_lc_design(double freq_hz,
                                                  double tank_q_target,
                                                  double power_mw)
{
    cross_coupled_lc_osc_t osc;
    memset(&osc, 0, sizeof(osc));

    if (freq_hz <= 0.0) return osc;

    if (tank_q_target <= 0.0) tank_q_target = 10.0;
    if (power_mw <= 0.0) power_mw = 5.0;

    double omega0 = 2.0 * M_PI * freq_hz;

    /* Select L (on-chip spiral: 1-20 nH typical) */
    if (freq_hz > 1.0e9) {
        osc.inductance_h = nearest_standard_inductor(1.0e-9);  /* ~1 nH */
    } else if (freq_hz > 100.0e6) {
        osc.inductance_h = nearest_standard_inductor(5.0e-9);  /* ~5 nH */
    } else {
        osc.inductance_h = nearest_standard_inductor(1.0e-7);  /* ~100 nH */
    }

    /* Total capacitance: C = 1/(ω₀²·L) */
    double c_total = 1.0 / (omega0 * omega0 * osc.inductance_h);
    osc.c_parasitic_f = c_total * 0.1;  /* 10% parasitic */
    osc.capacitance_f = nearest_standard_cap(c_total - osc.c_parasitic_f);

    /* Tank parallel resistance: R_p = Q·ω₀·L = Q·Z₀ */
    double z0 = sqrt(osc.inductance_h / (osc.capacitance_f + osc.c_parasitic_f));
    osc.tank_rp_ohms = tank_q_target * z0;

    /* Required gm per device: gm > 2/R_p → gm = 3/R_p for margin */
    osc.gm_per_device_siemens = 3.0 / osc.tank_rp_ohms;

    /* Bias current from power budget: I_tail = P / VDD */
    double vdd = 1.8;  /* typical CMOS supply */
    osc.bias_current_ma = power_mw / vdd;

    /* Start-up margin */
    osc.startup_margin = (int)(osc.gm_per_device_siemens * osc.tank_rp_ohms / 2.0);
    if (osc.startup_margin < 1) osc.startup_margin = 1;

    /* Negative resistance */
    osc.r_neg_ohms = -2.0 / osc.gm_per_device_siemens;

    /* Oscillation frequency */
    double c_eff = osc.capacitance_f + osc.c_parasitic_f;
    osc.freq_osc_hz = 1.0 / (2.0 * M_PI * sqrt(osc.inductance_h * c_eff));

    /* Amplitude in current-limited regime: V_pp ≈ I_tail·R_p */
    osc.amplitude_vpp = (osc.bias_current_ma * 1.0e-3) * osc.tank_rp_ohms;
    if (osc.amplitude_vpp > 2.0 * vdd) osc.amplitude_vpp = 2.0 * vdd;

    return osc;
}

/* ─── L4: Barkhausen Verification ────────────────────────────────────────── */

/**
 * Verify Barkhausen criterion for Colpitts oscillator.
 *
 * Loop gain: |L| = g_m · R_p · n  where n = C₁/(C₁+C₂)
 * Phase: 0° at resonance (tank + feedback network combined)
 */
barkhausen_criterion_t colpitts_verify_barkhausen(const colpitts_osc_t *osc)
{
    barkhausen_criterion_t result;
    memset(&result, 0, sizeof(result));

    if (!osc || osc->freq_osc_hz <= 0.0) return result;

    /* Compute loop gain: |L| = g_m · Z_tank · β
     * At resonance, Z_tank ≈ R_p. The feedback ratio β = C₁/(C₁+C₂)
     * (voltage divider from collector to emitter for common-base).
     * Actually for Colpitts: positive feedback ratio to emitter is
     * approximately C₂/(C₁+C₂). The loop gain is gm·R_p·(C₁C₂/(C₁+C₂)²)
     *  n = C₂/(C₁+C₂), loop gain ≈ gm·R_p·C₁/(C₁+C₂) = gm·R_p·(1-n) */
    double n = osc->c2_farads / (osc->c1_farads + osc->c2_farads);
    double rp = 1000.0;  /* rough estimate of tank parallel R */
    double loop_mag = osc->gm_min_siemens * rp * (1.0 - n);

    /*
     * Phase at resonance: the LC tank is purely resistive at f₀,
     * so the impedance phase is 0°. The feedback network C₁/C₂
     * provides in-phase feedback for the common-base/common-gate
     * topology, giving 0° total loop phase.
     */
    double loop_phase = 0.0;

    return barkhausen_evaluate(loop_mag, loop_phase, osc->freq_osc_hz);
}

/**
 * Verify Barkhausen for Hartley oscillator.
 */
barkhausen_criterion_t hartley_verify_barkhausen(const hartley_osc_t *osc)
{
    barkhausen_criterion_t result;
    memset(&result, 0, sizeof(result));

    if (!osc || osc->freq_osc_hz <= 0.0) return result;

    /* Similar analysis: loop gain depends on feedback ratio and tank Q */
    double n = osc->l2_henries / osc->l_total_henries;
    double loop_mag = osc->gm_min_siemens * 1000.0 * n;
    double loop_phase = 0.0;

    return barkhausen_evaluate(loop_mag, loop_phase, osc->freq_osc_hz);
}

/**
 * Verify Barkhausen for cross-coupled LC oscillator.
 *
 * Start-up condition: |R_neg| > R_p, i.e., 2/gm < R_p.
 * Equivalent: gm·R_p > 2.
 */
barkhausen_criterion_t cross_coupled_verify_barkhausen(const cross_coupled_lc_osc_t *osc)
{
    barkhausen_criterion_t result;
    memset(&result, 0, sizeof(result));

    if (!osc || osc->freq_osc_hz <= 0.0) return result;

    /* The cross-coupled pair provides a negative resistance -2/gm.
     * For oscillation: |R_neg| > R_p */
    double r_neg_mag = fabs(osc->r_neg_ohms);
    double loop_mag = r_neg_mag / osc->tank_rp_ohms;
    double loop_phase = 0.0;

    return barkhausen_evaluate(loop_mag, loop_phase, osc->freq_osc_hz);
}

/* ─── L3: Frequency Pulling ──────────────────────────────────────────────── */

/**
 * Compute frequency pulling due to load VSWR.
 *
 * When a mismatched load is connected to an oscillator output,
 * the reflection coefficient Γ_L modifies the effective tank impedance,
 * pulling the oscillation frequency.
 *
 * For an LC oscillator with loaded Q, the pulling is approximately:
 *   Δf ≈ (f₀/(2Q_L)) · |Γ_L| · sin(∠Γ_L)
 *
 * In the worst case (sin(∠Γ_L) = ±1):
 *   |Δf_max| ≈ f₀ · |Γ_L| / (2Q_L)
 *
 * VSWR → |Γ| conversion: |Γ| = (VSWR-1)/(VSWR+1)
 */
double lc_oscillator_pulling(double nominal_freq_hz, double q_loaded,
                               double load_vswr)
{
    if (nominal_freq_hz <= 0.0 || q_loaded <= 0.0) return 0.0;

    /* Convert VSWR to reflection coefficient magnitude */
    if (load_vswr < 1.0) load_vswr = 1.0;
    double gamma_mag = (load_vswr - 1.0) / (load_vswr + 1.0);

    /* Worst-case frequency pulling */
    double delta_f = nominal_freq_hz * gamma_mag / (2.0 * q_loaded);

    return delta_f;
}
