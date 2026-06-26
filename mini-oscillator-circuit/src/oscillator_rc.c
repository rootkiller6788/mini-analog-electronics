/**
 * @file    oscillator_rc.c
 * @brief   RC Oscillator implementations — L5 Design + L6 Canonical Problems
 *
 * @details Implements design and analysis functions for RC phase-shift,
 *          Wien bridge, and Twin-T oscillators. Each design function
 *          solves for component values given a target frequency and
 *          verifies the Barkhausen criterion.
 *
 * Knowledge covered:
 *   L1: RC ladder transfer function, Wien bridge lead-lag network
 *   L3: s-domain transfer functions evaluated as frequency responses
 *   L4: Barkhausen for RC topologies
 *   L5: Component selection, amplitude stabilization
 *   L6: 1 kHz Wien bridge, 10 kHz phase-shift canonical designs
 *
 * Reference: Sedra & Smith (2020), Ch.17.2
 */

#include "oscillator_rc.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ─── Internal helpers ──────────────────────────────────────────────────── */

static void* safe_malloc(size_t sz) {
    void *p = malloc(sz);
    if (!p) { fprintf(stderr, "oscillator_rc: malloc(%zu) failed\n", sz); abort(); }
    return p;
}

/* Standard capacitor decade values (E6 series, pF to uF range) */
static const double e6_caps[] = {
    1.0e-12, 1.5e-12, 2.2e-12, 3.3e-12, 4.7e-12, 6.8e-12,
    1.0e-11, 1.5e-11, 2.2e-11, 3.3e-11, 4.7e-11, 6.8e-11,
    1.0e-10, 1.5e-10, 2.2e-10, 3.3e-10, 4.7e-10, 6.8e-10,
    1.0e-9,  1.5e-9,  2.2e-9,  3.3e-9,  4.7e-9,  6.8e-9,
    1.0e-8,  1.5e-8,  2.2e-8,  3.3e-8,  4.7e-8,  6.8e-8,
    1.0e-7,  1.5e-7,  2.2e-7,  3.3e-7,  4.7e-7,  6.8e-7,
    1.0e-6,  1.5e-6,  2.2e-6,  3.3e-6,  4.7e-6,  6.8e-6
};
#define NUM_E6_CAPS (sizeof(e6_caps) / sizeof(e6_caps[0]))

/* Standard resistor decade values (E12 series) */
static const double e12_resistors[] = {
    1.0, 1.2, 1.5, 1.8, 2.2, 2.7, 3.3, 3.9, 4.7, 5.6, 6.8, 8.2
};
#define NUM_E12_RES (sizeof(e12_resistors) / sizeof(e12_resistors[0]))

/**
 * Pick the nearest standard capacitor value for a target.
 * Scales the E6 series across decades.
 */
static double nearest_standard_cap(double target_f) {
    if (target_f <= 0.0) return 1.0e-9;
    /* Find closest E6 value in any decade */
    double best = e6_caps[0];
    double best_err = fabs(target_f - best) / target_f;
    for (size_t i = 0; i < NUM_E6_CAPS; i++) {
        /* Also try decade-scaled versions */
        for (int dec = -3; dec <= 3; dec++) {
            double val = e6_caps[i] * pow(10.0, (double)dec);
            if (val < 1.0e-12 || val > 1.0e-3) continue;
            double err = fabs(target_f - val) / target_f;
            if (err < best_err) {
                best_err = err;
                best = val;
            }
        }
    }
    return best;
}

/**
 * Pick the nearest standard resistor value for a target.
 */
static double nearest_standard_res(double target_r) {
    if (target_r <= 0.0) return 1000.0;
    double decade = pow(10.0, floor(log10(target_r)));
    double mantissa = target_r / decade;
    double best_m = e12_resistors[0];
    double best_err = fabs(mantissa - best_m);
    for (size_t i = 1; i < NUM_E12_RES; i++) {
        double err = fabs(mantissa - e12_resistors[i]);
        if (err < best_err) {
            best_err = err;
            best_m = e12_resistors[i];
        }
    }
    /* Snap to decade */
    return best_m * decade;
}

/* ─── L5: RC Phase-Shift Oscillator Design ───────────────────────────────── */

/**
 * Design a 3-stage RC phase-shift oscillator.
 *
 * For equal-R, equal-C 3-stage:
 *   f_osc = 1 / (2π·R·C·√6)
 *
 * The three RC high-pass stages each contribute 60° at f_osc,
 * for a total of 180°. Combined with the inverting amplifier's 180°,
 * the total loop phase shift is 360° (0°).
 *
 * The required amplifier gain for start-up is |A| >= 29.
 *
 * Component selection strategy:
 *   For low frequencies (< 10 kHz): choose C first to keep R reasonable (< 1 MΩ)
 *   For high frequencies: choose R first to keep C practical
 */
rc_phase_shift_osc_t rc_phase_shift_design(double freq_hz,
                                              double r_chosen_ohms,
                                              double supply_v)
{
    rc_phase_shift_osc_t osc;
    memset(&osc, 0, sizeof(osc));
    osc.num_stages = 3;

    if (freq_hz <= 0.0) return osc;

    double omega_osc = 2.0 * M_PI * freq_hz;

    if (r_chosen_ohms > 0.0) {
        /* User specified R — compute C */
        osc.r_ohms = nearest_standard_res(r_chosen_ohms);
        double c_calc = 1.0 / (omega_osc * osc.r_ohms * sqrt(6.0));
        osc.c_farads = nearest_standard_cap(c_calc);
    } else {
        /* Auto-select: choose C first based on frequency decade */
        if (freq_hz < 100.0) {
            osc.c_farads = nearest_standard_cap(1.0e-6);  /* ~1 uF */
        } else if (freq_hz < 1000.0) {
            osc.c_farads = nearest_standard_cap(1.0e-7);  /* ~0.1 uF */
        } else if (freq_hz < 10000.0) {
            osc.c_farads = nearest_standard_cap(1.0e-8);  /* ~10 nF */
        } else if (freq_hz < 100000.0) {
            osc.c_farads = nearest_standard_cap(1.0e-9);  /* ~1 nF */
        } else {
            osc.c_farads = nearest_standard_cap(1.0e-10); /* ~100 pF */
        }

        double c_actual = osc.c_farads;
        double r_calc = 1.0 / (omega_osc * c_actual * sqrt(6.0));
        osc.r_ohms = nearest_standard_res(r_calc);
    }

    /* Recompute actual oscillation frequency */
    double r_actual = osc.r_ohms;
    double c_actual = osc.c_farads;
    osc.freq_osc_hz = 1.0 / (2.0 * M_PI * r_actual * c_actual * sqrt(6.0));
    osc.freq_osc_rads = 1.0 / (r_actual * c_actual * sqrt(6.0));
    osc.phase_per_stage_deg = 60.0;

    /* At f_osc, each RC stage gives |H| = 1/√(1+(ωRC)²), and for 3 equal stages,
     * the total |β| = 1/29 at f_osc. */
    double omega_rc = 2.0 * M_PI * osc.freq_osc_hz * r_actual * c_actual;
    double single_stage_mag = 1.0 / sqrt(1.0 + omega_rc * omega_rc);
    osc.attenuation = single_stage_mag * single_stage_mag * single_stage_mag;
    osc.startup_gain_min = 1.0 / osc.attenuation;

    /* Amplifier design: inverting op-amp with gain = -Rf/Rin */
    osc.amp_gain_magnitude = osc.startup_gain_min * 1.2; /* 20% margin */
    osc.r_input_ohms = nearest_standard_res(10000.0);
    osc.r_gain_ohms = nearest_standard_res(osc.r_input_ohms * osc.amp_gain_magnitude);
    osc.amp_gain_magnitude = osc.r_gain_ohms / osc.r_input_ohms;

    return osc;
}

/**
 * Compute transfer function of 3-stage equal-RC ladder at frequency f.
 *
 * For 3 identical RC high-pass stages in cascade:
 *   H(s) = (sRC)³ / ((1 + sRC)³)
 *
 * At s = jω:
 *   |H(jω)| = (ωRC)³ / (√(1+(ωRC)²))³
 *   ∠H(jω) = 3·(90° - arctan(ωRC))
 *
 * We evaluate the high-pass to ground configuration where each stage
 * loads the next. With buffering between stages (ideal), the transfer
 * function simplifies to the cascade above. Without buffering, the full
 * loaded transfer function is more complex — we use the loaded model here.
 *
 * The exact transfer function for the unloaded 3-stage RC ladder
 * (voltage divider form) used in phase-shift oscillators:
 *   H(jω) = 1 / (1 + 6jωRC + 5(jωRC)² + (jωRC)³)
 *
 * This accounts for the loading between stages.
 */
void rc_ladder_transfer(double r_ohms, double c_farads, double freq_hz,
                          double *mag, double *phase_deg)
{
    if (!mag || !phase_deg) return;

    if (r_ohms <= 0.0 || c_farads <= 0.0 || freq_hz <= 0.0) {
        *mag = 0.0;
        *phase_deg = 0.0;
        return;
    }

    double omega = 2.0 * M_PI * freq_hz;
    double wrc = omega * r_ohms * c_farads;

    /* Exact transfer function for 3-stage loaded RC ladder:
     * H(jω) = 1 / (1 + 6jωRC + 5(jωRC)² + (jωRC)³)
     *        = 1 / ((1 - 5ω²R²C²) + j(6ωRC - ω³R³C³))
     */
    double denom_re = 1.0 - 5.0 * wrc * wrc;
    double denom_im = 6.0 * wrc - wrc * wrc * wrc;
    double denom_mag_sq = denom_re * denom_re + denom_im * denom_im;

    if (denom_mag_sq > 0.0) {
        *mag = 1.0 / sqrt(denom_mag_sq);
    } else {
        *mag = 1.0e10; /* near pole — large value */
    }

    *phase_deg = atan2(-denom_im, denom_re) * 180.0 / M_PI;
}

/* ─── L5: Wien Bridge Oscillator Design ───────────────────────────────────── */

/**
 * Design a Wien bridge oscillator.
 *
 * Oscillation frequency: f_osc = 1/(2π·R·C)  (equal R, equal C)
 * Required amplifier gain: A_v = 3 (non-inverting)
 * Feedback network attenuation at f_osc: |β| = 1/3, ∠β = 0°
 *
 * Component selection: choose C based on frequency decade, compute R.
 * For good performance: R between 1 kΩ and 1 MΩ, C between 100 pF and 10 µF.
 */
wien_bridge_osc_t wien_bridge_design(double freq_hz, double c_chosen_f)
{
    wien_bridge_osc_t osc;
    memset(&osc, 0, sizeof(osc));
    osc.equal_rc = 1;

    if (freq_hz <= 0.0) return osc;

    double omega_osc = 2.0 * M_PI * freq_hz;

    if (c_chosen_f > 0.0) {
        osc.c_series_farads = nearest_standard_cap(c_chosen_f);
        osc.c_parallel_farads = osc.c_series_farads;
    } else {
        /* Auto-select C based on frequency */
        if (freq_hz < 100.0) {
            osc.c_series_farads = nearest_standard_cap(1.0e-6);
        } else if (freq_hz < 1000.0) {
            osc.c_series_farads = nearest_standard_cap(1.0e-7);
        } else if (freq_hz < 10000.0) {
            osc.c_series_farads = nearest_standard_cap(1.0e-8);
        } else if (freq_hz < 100000.0) {
            osc.c_series_farads = nearest_standard_cap(1.0e-9);
        } else {
            osc.c_series_farads = nearest_standard_cap(1.0e-10);
        }
        osc.c_parallel_farads = osc.c_series_farads;
    }

    double r_calc = 1.0 / (omega_osc * osc.c_series_farads);
    osc.r_series_ohms = nearest_standard_res(r_calc);
    osc.r_parallel_ohms = osc.r_series_ohms;

    /* Recompute actual frequency */
    osc.freq_osc_hz = 1.0 / (2.0 * M_PI * osc.r_series_ohms * osc.c_series_farads);
    osc.freq_osc_rads = 1.0 / (osc.r_series_ohms * osc.c_series_farads);
    osc.beta_attenuation = 1.0 / 3.0;

    /* Amplifier: non-inverting gain = 1 + Rf/Rg = 3 */
    /* Use Rg = 10kΩ standard, Rf = 2*Rg = 20kΩ */
    osc.r_ground_ohms = 10000.0;
    osc.r_feedback_ohms = 20000.0;
    osc.amp_gain = 1.0 + osc.r_feedback_ohms / osc.r_ground_ohms;

    /* For start-up, gain must slightly exceed 3. In practice,
     * amplitude stabilization (diode/JFET/thermistor) reduces gain
     * to exactly 3 at steady state. We design for gain=3 at amplitude. */
    osc.amp_gain = 3.0; /* nominal; actual circuit adds 5-10% for start-up */

    return osc;
}

/**
 * Compute Wien bridge lead-lag network transfer function.
 *
 * The series RC and parallel RC form a voltage divider:
 *
 *   β(s) = Z_parallel / (Z_series + Z_parallel)
 *
 *   where Z_series = R + 1/(sC), Z_parallel = R || 1/(sC) = (R/sC)/(R+1/sC)
 *
 * After algebra:
 *   β(jω) = (jωRC) / (1 - ω²R²C² + 3jωRC)
 *
 * At ω = 1/(RC): β(jω₀) = 1/3
 */
void wien_bridge_transfer(double r_ohms, double c_farads, double freq_hz,
                            double *mag, double *phase_deg)
{
    if (!mag || !phase_deg) return;

    if (r_ohms <= 0.0 || c_farads <= 0.0 || freq_hz <= 0.0) {
        *mag = 0.0;
        *phase_deg = 0.0;
        return;
    }

    double omega = 2.0 * M_PI * freq_hz;
    double wrc = omega * r_ohms * c_farads;

    /* β(jω) = jωRC / (1 - ω²R²C² + 3jωRC) */
    double num_im = wrc;
    double denom_re = 1.0 - wrc * wrc;
    double denom_im = 3.0 * wrc;

    double denom_mag_sq = denom_re * denom_re + denom_im * denom_im;
    if (denom_mag_sq > 0.0) {
        *mag = fabs(wrc) / sqrt(denom_mag_sq);
    } else {
        *mag = 0.0;
    }

    /* Phase = angle(num) - angle(denom) = 90° - atan2(denom_im, denom_re) */
    double num_phase = M_PI / 2.0; /* angle of jωRC = 90° */
    double denom_phase = atan2(denom_im, denom_re);
    double ph = num_phase - denom_phase;
    /* Normalize to degrees */
    *phase_deg = ph * 180.0 / M_PI;
}

/* ─── L6: Barkhausen Verification ─────────────────────────────────────────── */

/**
 * Verify Barkhausen criterion for RC phase-shift oscillator.
 */
barkhausen_criterion_t rc_phase_shift_verify_barkhausen(const rc_phase_shift_osc_t *osc)
{
    barkhausen_criterion_t result;
    memset(&result, 0, sizeof(result));

    if (!osc || osc->freq_osc_hz <= 0.0) return result;

    /* Compute feedback network transfer at design frequency */
    double beta_mag, beta_phase;
    rc_ladder_transfer(osc->r_ohms, osc->c_farads, osc->freq_osc_hz,
                        &beta_mag, &beta_phase);

    /* Loop gain = |A| * |β|; phase = ∠A + ∠β */
    /* Inverting amplifier contributes 180° phase shift */
    double loop_mag = osc->amp_gain_magnitude * beta_mag;
    double loop_phase = 180.0 + beta_phase; /* -180 from inverting + β phase */

    return barkhausen_evaluate(loop_mag, loop_phase, osc->freq_osc_hz);
}

/**
 * Verify Barkhausen criterion for Wien bridge oscillator.
 */
barkhausen_criterion_t wien_bridge_verify_barkhausen(const wien_bridge_osc_t *osc)
{
    barkhausen_criterion_t result;
    memset(&result, 0, sizeof(result));

    if (!osc || osc->freq_osc_hz <= 0.0 || !osc->equal_rc) return result;

    /* Compute feedback network transfer at design frequency */
    double beta_mag, beta_phase;
    wien_bridge_transfer(osc->r_series_ohms, osc->c_series_farads,
                          osc->freq_osc_hz, &beta_mag, &beta_phase);

    /* Non-inverting amplifier: 0° phase shift */
    double loop_mag = osc->amp_gain * beta_mag;
    double loop_phase = beta_phase; /* 0° from non-inverting amp */

    return barkhausen_evaluate(loop_mag, loop_phase, osc->freq_osc_hz);
}

/* ─── L5: Twin-T Oscillator Design ────────────────────────────────────────── */

/**
 * Design a Twin-T notch oscillator.
 *
 * The Twin-T network provides a notch filter in the negative feedback path.
 * At the notch frequency f_n = 1/(2πRC), the negative feedback is minimal,
 * allowing the positive feedback path to sustain oscillation.
 *
 * Standard Twin-T: two T-sections — one R-C-R (low-pass) and one C-R-C
 * (high-pass). The notch depth and Q depend on the resistor ratio.
 *
 * For Q control, a voltage divider (R_a, R_b) sets the positive feedback.
 */
twin_t_osc_t twin_t_design(double freq_hz, double q_desired)
{
    twin_t_osc_t osc;
    memset(&osc, 0, sizeof(osc));

    if (freq_hz <= 0.0) return osc;

    osc.freq_notch_hz = freq_hz;
    osc.freq_osc_hz = freq_hz;
    osc.q_notch = (q_desired > 0.0) ? q_desired : 10.0;

    /* Choose C first, compute R */
    if (freq_hz < 1000.0) {
        osc.c_farads = 1.0e-7;  /* 100 nF */
    } else if (freq_hz < 100000.0) {
        osc.c_farads = 1.0e-8;  /* 10 nF */
    } else {
        osc.c_farads = 1.0e-9;  /* 1 nF */
    }

    double omega = 2.0 * M_PI * freq_hz;
    osc.r_ohms = nearest_standard_res(1.0 / (omega * osc.c_farads));

    /* Notch depth: The ideal Twin-T has infinite attenuation at f_n.
     * With component tolerances, typically 40-60 dB is achievable. */
    osc.notch_depth_db = 50.0;

    /* Required amplifier gain: must overcome attenuation at notch + provide
     * sufficient positive feedback. Q_divider resistor controls this. */
    osc.amp_gain_required = 2.0 * (1.0 + 1.0 / (4.0 * osc.q_notch * osc.q_notch));
    osc.r_divider_ohms = osc.r_ohms * 0.5;

    return osc;
}

/* ─── L5: Wien Bridge THD Estimate ────────────────────────────────────────── */

/**
 * Estimate THD for a Wien bridge oscillator with diode amplitude limiting.
 *
 * The THD depends on:
 *   - How much the small-signal gain exceeds 3 (excess_gain_db)
 *   - The sharpness of the diode limiting (soft vs hard clipping)
 *
 * For a well-designed Wien bridge with back-to-back diodes and
 * excess gain of ~1 dB, THD can be as low as 0.01% (-80 dB).
 * With simple resistor-only gain-setting, THD rises to 1-3%.
 *
 * Empirical model:
 *   THD(%) ≈ base_thd · 10^{excess_dB/20}
 */
double wien_bridge_estimate_thd(const wien_bridge_osc_t *osc, double excess_gain_db)
{
    if (!osc) return 0.0;

    /* Base THD for a well-stabilized Wien bridge is very low */
    double base_thd = 0.03;  /* 0.03% with good diode stabilization */

    /* Excess gain increases distortion */
    double excess_factor = pow(10.0, excess_gain_db / 10.0);

    /* THD scales approximately linearly with excess loop gain */
    double thd = base_thd * excess_factor;

    /* Clamp to reasonable range */
    if (thd > 10.0) thd = 10.0;
    if (thd < 0.005) thd = 0.005;

    return thd;
}
