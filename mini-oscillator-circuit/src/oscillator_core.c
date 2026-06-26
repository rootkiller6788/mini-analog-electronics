/**
 * @file    oscillator_core.c
 * @brief   Core oscillator implementations — L1 Defs + L4 Barkhausen Criterion
 *
 * @details Implements the Barkhausen criterion evaluation, oscillator
 *          parameter initialization, quality factor computation, start-up
 *          time estimation, tolerance analysis, and THD estimation.
 *
 * Knowledge covered:
 *   L1: Barkhausen criterion, Q factor, frequency stability
 *   L2: Start-up condition, loop gain analysis
 *   L4: Barkhausen as fundamental law, verified numerically
 *
 * Reference:
 *   Sedra & Smith, "Microelectronic Circuits" (2020), Ch.17
 *   Razavi, "Design of Analog CMOS Integrated Circuits" (2017), Ch.14
 */

#include "oscillator_core.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ─── Internal helpers ──────────────────────────────────────────────────── */

/* Internal helpers removed — using direct malloc/free in this file */

/* ─── L4: Barkhausen Criterion ───────────────────────────────────────────── */

/**
 * Evaluate the Barkhausen criterion at a specific frequency.
 *
 * The phase condition tolerance is set to 1 degree and magnitude
 * condition tolerance to 0.01 (1%) as practical engineering defaults.
 */
barkhausen_criterion_t barkhausen_evaluate(double magnitude, double phase_deg,
                                             double freq_hz)
{
    barkhausen_criterion_t result;
    memset(&result, 0, sizeof(result));

    result.loop_gain_magnitude = magnitude;
    result.loop_phase_deg = phase_deg;
    result.freq_hz = freq_hz;

    /* Normalize phase to [-180, 180] for checking near 0 */
    double phase_norm = fmod(phase_deg, 360.0);
    if (phase_norm > 180.0) phase_norm -= 360.0;
    if (phase_norm < -180.0) phase_norm += 360.0;

    /* Phase condition: ∠L(f) ≈ 0° (or integer multiples of 360°) */
    double phase_err = fabs(phase_norm);
    if (phase_err > 180.0) phase_err = 360.0 - phase_err;
    result.phase_satisfied = (phase_err <= 1.0) ? 1 : 0;

    /* Magnitude condition: |L(f)| >= 1.0 */
    result.magnitude_satisfied = (magnitude >= 0.99) ? 1 : 0;

    /* Start-up margin in dB */
    if (magnitude > 1e-20) {
        result.start_up_margin_db = 20.0 * log10(magnitude);
    } else {
        result.start_up_margin_db = -400.0;
    }

    return result;
}

/**
 * Check the start-up condition for oscillation.
 *
 * Different topologies have different recommended gain margins:
 *   - RC phase-shift: needs |L| >= 29 (29.2 dB) for equal-RC 3-stage
 *   - Wien bridge:    needs |L| >= 3  (9.5 dB)
 *   - LC oscillators: needs |L| >= 2-3 (6-10 dB) depending on loading
 *   - Crystal:        needs |R_neg| > R_motional (margin ~2-5x)
 */
int barkhausen_startup_check(double gain_db, oscillator_type_t osc_type)
{
    double min_margin_db;

    switch (osc_type) {
        case OSC_TYPE_RC_PHASE_SHIFT:
            min_margin_db = 29.2;  /* |A| >= 29 for equal R,C */
            break;
        case OSC_TYPE_WIEN_BRIDGE:
            min_margin_db = 9.5;   /* gain == 3 exactly, margin just above */
            break;
        case OSC_TYPE_COLPITTS:
        case OSC_TYPE_HARTLEY:
        case OSC_TYPE_CLAPP:
        case OSC_TYPE_ARMSTRONG:
        case OSC_TYPE_CROSS_COUPLED_LC:
            min_margin_db = 3.0;   /* |L| >= 1.4 linear ≈ 3 dB */
            break;
        case OSC_TYPE_PIERCE_CRYSTAL:
        case OSC_TYPE_COLPITTS_CRYSTAL:
            min_margin_db = 3.0;
            break;
        case OSC_TYPE_RING:
            min_margin_db = 0.0;   /* ring oscillators self-start easily */
            break;
        default:
            min_margin_db = 6.0;   /* conservative default */
            break;
    }

    return (gain_db >= min_margin_db) ? 1 : 0;
}

/**
 * Find oscillation frequency from loop gain sweep via phase zero-crossing.
 *
 * Scans the phase array for a sign change, then linearly interpolates
 * the exact frequency where the phase crosses 0° (mod 360°).
 */
double barkhausen_find_osc_freq(const loop_gain_sweep_t *sweep)
{
    if (!sweep || sweep->num_points < 2 || !sweep->phase_deg || !sweep->freq_hz)
        return -1.0;

    /* Look for phase crossing through 0° or 360° */
    for (size_t i = 0; i < sweep->num_points - 1; i++) {
        double p1 = sweep->phase_deg[i];
        double p2 = sweep->phase_deg[i + 1];

        /* Normalize both to [-180, 180] */
        double p1n = fmod(p1, 360.0);
        if (p1n > 180.0) p1n -= 360.0;
        if (p1n < -180.0) p1n += 360.0;

        double p2n = fmod(p2, 360.0);
        if (p2n > 180.0) p2n -= 360.0;
        if (p2n < -180.0) p2n += 360.0;

        /* Check for zero-crossing */
        if (p1n * p2n <= 0.0) {
            /* Linear interpolation */
            if (fabs(p2n - p1n) > 1e-20) {
                double t = -p1n / (p2n - p1n);
                return sweep->freq_hz[i] + t * (sweep->freq_hz[i + 1] - sweep->freq_hz[i]);
            }
            return sweep->freq_hz[i];
        }
    }

    return -1.0;  /* No zero-crossing found — Barkhausen not satisfied */
}

/* ─── L1: Parameter Initialization ───────────────────────────────────────── */

/**
 * Initialize oscillator parameters with sensible defaults for each topology.
 */
oscillator_params_t oscillator_params_init(oscillator_type_t type,
                                             double freq_hz, double supply_v)
{
    oscillator_params_t p;
    memset(&p, 0, sizeof(p));

    p.type = type;
    p.target_freq_hz = freq_hz;
    p.actual_freq_hz = freq_hz;
    p.supply_voltage_v = supply_v;

    /* Set topology-specific defaults */
    switch (type) {
        case OSC_TYPE_RC_PHASE_SHIFT:
            p.mode = OSC_MODE_HARMONIC;
            p.active = ACTIVE_OPAMP;
            p.amplitude_vpp = supply_v * 0.6;
            p.thd_percent = 1.0;
            p.phase_noise_dbc_hz = -80.0;
            break;
        case OSC_TYPE_WIEN_BRIDGE:
            p.mode = OSC_MODE_HARMONIC;
            p.active = ACTIVE_OPAMP;
            p.amplitude_vpp = supply_v * 0.5;
            p.thd_percent = 0.1;
            p.phase_noise_dbc_hz = -90.0;
            break;
        case OSC_TYPE_COLPITTS:
        case OSC_TYPE_CLAPP:
            p.mode = OSC_MODE_HARMONIC;
            p.active = ACTIVE_BJT;
            p.amplitude_vpp = supply_v * 0.8;
            p.thd_percent = 2.0;
            p.phase_noise_dbc_hz = -110.0;
            break;
        case OSC_TYPE_HARTLEY:
            p.mode = OSC_MODE_HARMONIC;
            p.active = ACTIVE_BJT;
            p.amplitude_vpp = supply_v * 0.7;
            p.thd_percent = 3.0;
            p.phase_noise_dbc_hz = -105.0;
            break;
        case OSC_TYPE_CROSS_COUPLED_LC:
            p.mode = OSC_MODE_HARMONIC;
            p.active = ACTIVE_FET;
            p.amplitude_vpp = supply_v * 0.9;
            p.thd_percent = 1.5;
            p.phase_noise_dbc_hz = -120.0;
            break;
        case OSC_TYPE_PIERCE_CRYSTAL:
            p.mode = OSC_MODE_HARMONIC;
            p.active = ACTIVE_CMOS_INV;
            p.amplitude_vpp = supply_v * 0.9;
            p.thd_percent = 0.01;
            p.phase_noise_dbc_hz = -150.0;
            break;
        case OSC_TYPE_RELAXATION_555:
            p.mode = OSC_MODE_RELAXATION;
            p.active = ACTIVE_COMPARATOR;
            p.amplitude_vpp = supply_v * 0.85;
            p.thd_percent = 10.0;
            p.phase_noise_dbc_hz = -40.0;
            break;
        case OSC_TYPE_RELAXATION_SCHMITT:
            p.mode = OSC_MODE_RELAXATION;
            p.active = ACTIVE_CMOS_INV;
            p.amplitude_vpp = supply_v;
            p.thd_percent = 8.0;
            p.phase_noise_dbc_hz = -35.0;
            break;
        case OSC_TYPE_RING:
            p.mode = OSC_MODE_HARMONIC;
            p.active = ACTIVE_CMOS_INV;
            p.amplitude_vpp = supply_v;
            p.thd_percent = 5.0;
            p.phase_noise_dbc_hz = -70.0;
            break;
        case OSC_TYPE_VCO_LC:
            p.mode = OSC_MODE_HARMONIC;
            p.active = ACTIVE_FET;
            p.amplitude_vpp = supply_v * 0.6;
            p.thd_percent = 3.0;
            p.phase_noise_dbc_hz = -110.0;
            break;
        case OSC_TYPE_VCO_RING:
            p.mode = OSC_MODE_HARMONIC;
            p.active = ACTIVE_CMOS_INV;
            p.amplitude_vpp = supply_v;
            p.thd_percent = 6.0;
            p.phase_noise_dbc_hz = -65.0;
            break;
        default:
            p.mode = OSC_MODE_HARMONIC;
            p.active = ACTIVE_OPAMP;
            p.amplitude_vpp = supply_v * 0.5;
            p.thd_percent = 5.0;
            p.phase_noise_dbc_hz = -60.0;
            break;
    }

    /* Estimate start-up time */
    p.startup_time_us = 100.0;  /* default rough estimate */
    p.power_dissipation_mw = 10.0;
    p.supply_current_ma = p.power_dissipation_mw / supply_v;
    p.dc_offset_v = supply_v / 2.0;

    return p;
}

/* ─── L1: Quality Factor Computation ─────────────────────────────────────── */

/**
 * Compute Quality Factor from RLC component values.
 *
 * For parallel RLC:  Q = R/√(L/C) = ω₀·R·C = R/(ω₀·L)
 * For series RLC:    Q = √(L/C)/R = ω₀·L/R = 1/(ω₀·R·C)
 *
 * Boundary cases:
 *   - If L=0 (RC only): Q concept differs; use bandwidth-based approach
 *   - If R=0: Q → ∞ (ideal, lossless), return large finite value
 */
quality_factor_t quality_factor_compute(double r, double l, double c,
                                          int is_parallel)
{
    quality_factor_t q;
    memset(&q, 0, sizeof(q));

    if (r <= 0.0 || c <= 0.0) return q;

    /* RC-only circuit (no inductor): estimate Q via effective bandwidth */
    if (l <= 0.0) {
        q.resonant_freq_hz = 1.0 / (2.0 * M_PI * r * c);
        q.bandwidth_3db_hz = q.resonant_freq_hz;  /* RC: BW ≈ f_c */
        q.q_value = 0.5;  /* RC roll-off is inherently low Q */
        q.damping_ratio = 1.0;
        q.equivalent_resistance = r;
        return q;
    }

    double omega0 = 1.0 / sqrt(l * c);
    q.resonant_freq_hz = omega0 / (2.0 * M_PI);
    double char_z = sqrt(l / c);

    if (is_parallel) {
        q.q_value = r / char_z;
        q.equivalent_resistance = r;
    } else {
        q.q_value = char_z / r;
        q.equivalent_resistance = r;
    }

    if (q.q_value > 0.0) {
        q.bandwidth_3db_hz = q.resonant_freq_hz / q.q_value;
        q.damping_ratio = 0.5 / q.q_value;
    }

    /* Estimate energy storage (arbitrary reference: 1V across tank) */
    double v_ref = 1.0;
    q.energy_stored_j = 0.5 * c * v_ref * v_ref;  /* peak capacitive energy */
    if (q.q_value > 0.0) {
        q.energy_lost_per_cycle_j = 2.0 * M_PI * q.energy_stored_j / q.q_value;
    }

    return q;
}

/**
 * Compute Q from resonant frequency and 3-dB bandwidth.
 *
 * Q = f₀ / BW_3dB
 */
double quality_factor_from_bandwidth(double f0_hz, double bw_hz)
{
    if (bw_hz <= 0.0 || f0_hz <= 0.0) return 0.0;
    return f0_hz / bw_hz;
}

/* ─── L2: Start-Up Time Estimation ───────────────────────────────────────── */

/**
 * Estimate start-up time based on exponential envelope growth.
 *
 * The oscillation amplitude grows as V_env(t) = V_noise · exp(t/τ_growth)
 * where τ_growth ≈ 2Q/ω₀ = Q/(π·f₀).
 *
 * To reach final amplitude V_final from noise floor V_noise:
 *   t_start = τ_growth · ln(V_final / V_noise)
 *
 * V_noise is typically the thermal noise of the resonator:
 *   V_noise_rms = √(4kT·R·BW) ≈ several µV
 */
double oscillator_startup_time_estimate(double q_factor, double freq_hz,
                                          double v_noise_uv, double v_final_v)
{
    if (freq_hz <= 0.0 || q_factor <= 0.0 || v_noise_uv <= 0.0 || v_final_v <= 0.0)
        return -1.0;

    double tau_growth = q_factor / (M_PI * freq_hz);
    double v_noise = v_noise_uv * 1.0e-6;
    double ratio = v_final_v / v_noise;

    if (ratio <= 1.0) return 0.0;

    return tau_growth * log(ratio);
}

/* ─── L1: THD Estimation ─────────────────────────────────────────────────── */

/**
 * Compute THD estimate based on oscillator topology and empirical models.
 *
 * The THD depends on:
 *   - Amplitude limiting mechanism (soft vs hard clipping)
 *   - Excess loop gain (more excess → more distortion)
 *   - Resonator Q (higher Q filters harmonics better)
 */
double oscillator_estimate_thd(const oscillator_params_t *params)
{
    if (!params) return 0.0;

    /* Base THD from topology (empirical) */
    double base_thd;
    switch (params->type) {
        case OSC_TYPE_WIEN_BRIDGE:
            base_thd = 0.05;  /* Diode-stabilized Wien bridge - very low THD */
            break;
        case OSC_TYPE_PIERCE_CRYSTAL:
        case OSC_TYPE_COLPITTS_CRYSTAL:
            base_thd = 0.01;  /* Crystal has extremely high Q */
            break;
        case OSC_TYPE_RC_PHASE_SHIFT:
            base_thd = 1.0;
            break;
        case OSC_TYPE_COLPITTS:
        case OSC_TYPE_CLAPP:
            base_thd = 2.0;
            break;
        case OSC_TYPE_HARTLEY:
            base_thd = 3.0;
            break;
        case OSC_TYPE_CROSS_COUPLED_LC:
            base_thd = 1.5;
            break;
        case OSC_TYPE_RELAXATION_555:
            base_thd = 10.0;
            break;
        case OSC_TYPE_RELAXATION_SCHMITT:
            base_thd = 8.0;
            break;
        case OSC_TYPE_RELAXATION_OPAMP:
            base_thd = 5.0;
            break;
        case OSC_TYPE_RING:
            base_thd = 5.0;
            break;
        default:
            base_thd = 3.0;
            break;
    }

    return base_thd;
}

/* ─── L1: Tolerance Analysis ──────────────────────────────────────────────── */

/**
 * Analyze frequency deviation due to component tolerances using worst-case
 * sensitivity analysis.
 *
 * For an RC oscillator where f ∝ 1/(RC):
 *   Δf/f_max = |Δtolerance_R| + |Δtolerance_C|
 *
 * For an LC oscillator where f ∝ 1/√(LC):
 *   Δf/f_max = 0.5·|Δtolerance_L| + 0.5·|Δtolerance_C|
 *
 * For a crystal oscillator:
 *   Crystal initial tolerance dominates; external component effects are small.
 */
tolerance_analysis_t oscillator_tolerance_analyze(double nominal_freq_hz,
                                                    const tolerance_analysis_t *tol)
{
    tolerance_analysis_t result;
    memset(&result, 0, sizeof(result));

    if (!tol || nominal_freq_hz <= 0.0) return result;

    result.r_tolerance_percent = tol->r_tolerance_percent;
    result.c_tolerance_percent = tol->c_tolerance_percent;
    result.l_tolerance_percent = tol->l_tolerance_percent;
    result.monte_carlo_samples = tol->monte_carlo_samples;

    /* Worst-case frequency deviation for RC and LC combined sensitivity */
    double r_sens = (tol->r_tolerance_percent > 0.0) ? 1.0 : 0.0;
    double c_sens = (tol->c_tolerance_percent > 0.0) ? 1.0 : 0.0;
    double l_sens = (tol->l_tolerance_percent > 0.0) ? 0.5 : 0.0;

    /* For LC, frequency ∝ 1/√(LC) */
    double total_fractional;
    if (tol->l_tolerance_percent > 0.0) {
        /* LC oscillator: f ∝ 1/√(LC) */
        total_fractional = l_sens * (tol->l_tolerance_percent / 100.0)
                         + 0.5 * (tol->c_tolerance_percent / 100.0);
    } else {
        /* RC oscillator: f ∝ 1/(RC) */
        total_fractional = r_sens * (tol->r_tolerance_percent / 100.0)
                         + c_sens * (tol->c_tolerance_percent / 100.0);
    }

    double max_dev = total_fractional * nominal_freq_hz;
    result.freq_dev_max_hz = nominal_freq_hz + max_dev;
    result.freq_dev_min_hz = nominal_freq_hz - max_dev;
    if (result.freq_dev_min_hz < 0.0) result.freq_dev_min_hz = 0.0;
    result.freq_dev_ppm = total_fractional * 1.0e6;

    return result;
}

/* ─── Memory Management ─────────────────────────────────────────────────── */

void loop_gain_sweep_free(loop_gain_sweep_t *sweep) {
    if (sweep) {
        free(sweep->freq_hz);
        free(sweep->magnitude);
        free(sweep->phase_deg);
        free(sweep->magnitude_db);
        sweep->freq_hz = NULL;
        sweep->magnitude = NULL;
        sweep->phase_deg = NULL;
        sweep->magnitude_db = NULL;
        sweep->num_points = 0;
    }
}
