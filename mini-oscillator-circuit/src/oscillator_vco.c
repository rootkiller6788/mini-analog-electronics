/**
 * @file    oscillator_vco.c
 * @brief   Voltage-Controlled Oscillator implementations — L5 Design + L6 Problems + L7 Apps
 *
 * @details Implements varactor-tuned LC VCO design, ring VCO design,
 *          tuning curve computation, K_VCO calculation, phase noise
 *          estimation, and VCO figure of merit.
 *
 * Knowledge covered:
 *   L1: Varactor model, K_VCO, tuning range
 *   L3: C-V curve, f-V relationship
 *   L4: K_VCO = df/dV_tune fundamental definition
 *   L5: VCO design methodology, tuning linearity optimization
 *   L6: 2-3 GHz LC VCO design
 *   L7: GPS L1 1.575 GHz VCO
 *
 * Reference: Razavi (2017), Ch.14-15
 *            Rohde, "Microwave and Wireless Synthesizers" (1997)
 */

#include "oscillator_vco.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef K_BOLTZMANN
#define K_BOLTZMANN 1.380649e-23
#endif

/* ─── L1: Varactor Model ──────────────────────────────────────────────────── */

/**
 * Initialize a varactor with typical parameters.
 *
 * For a hyperabrupt varactor (m ≈ 1.0-2.0), the C-V curve is steeper,
 * giving wider tuning range but poorer linearity.
 *
 * For an abrupt junction varactor (m ≈ 0.5), linearity is better
 * but tuning range is narrower.
 */
varactor_t varactor_init(double cj0_farads, double tuning_ratio, double v_tune_max)
{
    varactor_t var;
    memset(&var, 0, sizeof(var));

    if (cj0_farads <= 0.0) cj0_farads = 1.0e-12;  /* 1 pF default */
    if (tuning_ratio < 1.0) tuning_ratio = 2.0;
    if (v_tune_max <= 0.0) v_tune_max = 3.3;

    var.cj0_farads = cj0_farads;
    var.phi_v = 0.7;  /* typical silicon junction potential */
    var.v_min = 0.3;  /* avoid forward bias */
    var.v_max = v_tune_max;
    var.c_max_farads = cj0_farads;
    var.c_min_farads = cj0_farads / tuning_ratio;
    var.tuning_ratio = tuning_ratio;

    /* Solve for grading coefficient m from tuning ratio:
     * C(V_max)/C(0) = 1/(1+V_max/φ)^m = 1/tuning_ratio
     * → (1+V_max/φ)^m = tuning_ratio
     * → m = ln(tuning_ratio) / ln(1+V_max/φ) */
    double ratio_arg = 1.0 + var.v_max / var.phi_v;
    if (ratio_arg > 1.0 && tuning_ratio > 1.0) {
        var.m_grading = log(tuning_ratio) / log(ratio_arg);
    } else {
        var.m_grading = 0.5;  /* default abrupt junction */
    }

    /* Quality factor at 1 GHz (estimate) */
    var.series_r_ohms = 1.0;  /* ~1Ω series resistance */
    var.q_at_1ghz = 1.0 / (2.0 * M_PI * 1.0e9 * var.cj0_farads * var.series_r_ohms);

    return var;
}

/**
 * Compute varactor capacitance at a given reverse bias voltage.
 *
 * C(V) = C_j0 / (1 + V/φ)^m
 *
 * This is the standard PN junction capacitance model.
 * For V >> φ (strong reverse bias), C ∝ V^{-m}.
 */
double varactor_capacitance(const varactor_t *varactor, double v_bias)
{
    if (!varactor) return 0.0;

    if (v_bias < 0.0) v_bias = 0.0;  /* avoid forward bias */
    if (v_bias > varactor->v_max) v_bias = varactor->v_max;

    double ratio = 1.0 + v_bias / varactor->phi_v;
    if (ratio <= 0.0) return varactor->c_max_farads;

    double c = varactor->cj0_farads / pow(ratio, varactor->m_grading);

    /* Clamp to physical range */
    if (c > varactor->c_max_farads) c = varactor->c_max_farads;
    if (c < varactor->c_min_farads) c = varactor->c_min_farads;

    return c;
}

/* ─── L5: LC VCO Design ──────────────────────────────────────────────────── */

/**
 * Design an LC VCO with varactor tuning.
 *
 * The VCO frequency is:
 *   f(V_tune) = 1 / (2π · √(L · C_total(V_tune)))
 *
 * where C_total = C_fixed + C_varactor(V_tune) + C_parasitic.
 *
 * Design steps:
 *   1. Choose inductor L based on frequency and Q requirements
 *   2. Choose varactor with appropriate C-V range
 *   3. Compute C_fixed to center the tuning range
 *   4. Estimate K_VCO and phase noise
 */
lc_vco_t lc_vco_design(double freq_min_hz, double freq_max_hz,
                          double supply_v, double power_mw_target)
{
    lc_vco_t vco;
    memset(&vco, 0, sizeof(vco));

    if (freq_min_hz <= 0.0 || freq_max_hz <= freq_min_hz) return vco;
    if (supply_v <= 0.0) supply_v = 3.3;
    if (power_mw_target <= 0.0) power_mw_target = 10.0;

    vco.supply_voltage_v = supply_v;
    vco.power_mw = power_mw_target;

    double f_center = sqrt(freq_min_hz * freq_max_hz);
    vco.freq_center_hz = f_center;
    vco.freq_min_hz = freq_min_hz;
    vco.freq_max_hz = freq_max_hz;
    vco.tuning_range_percent = (freq_max_hz - freq_min_hz) / f_center * 100.0;

    /* Choose inductor based on frequency */
    if (f_center > 5.0e9) {
        vco.inductance_h = 0.5e-9;  /* 0.5 nH for >5 GHz */
    } else if (f_center > 1.0e9) {
        vco.inductance_h = 2.0e-9;  /* 2 nH for 1-5 GHz */
    } else if (f_center > 100.0e6) {
        vco.inductance_h = 10.0e-9; /* 10 nH for 100 MHz-1 GHz */
    } else {
        vco.inductance_h = 100.0e-9; /* 100 nH for <100 MHz */
    }

    /* Compute total capacitance needed at f_center:
     * C_total = 1/((2πf)²·L) */
    double omega_center = 2.0 * M_PI * f_center;
    double c_total_center = 1.0 / (omega_center * omega_center * vco.inductance_h);

    /* Varactor: choose C_j0 such that the varactor covers the tuning range.
     * C_total_min = C_fixed + C_par + C_var_min → gives f_max
     * C_total_max = C_fixed + C_par + C_var_max → gives f_min */
    double c_total_max = 1.0 / (pow(2.0 * M_PI * freq_min_hz, 2.0) * vco.inductance_h);
    double c_total_min = 1.0 / (pow(2.0 * M_PI * freq_max_hz, 2.0) * vco.inductance_h);

    /* Varactor should provide the capacitance range difference */
    double c_var_range_needed = c_total_max - c_total_min;

    /* Choose varactor C_j0 at ~3x the needed range for margin */
    double cj0 = c_var_range_needed * 3.0;
    if (cj0 < 0.2e-12) cj0 = 0.5e-12;
    double tuning_ratio = c_total_max / c_total_min;
    if (tuning_ratio < 1.1) tuning_ratio = 2.0;
    if (tuning_ratio > 10.0) tuning_ratio = 5.0;

    vco.varactor = varactor_init(cj0, tuning_ratio, supply_v);

    /* Parasitic capacitance: ~10% of C_total */
    vco.c_parasitic_farads = 0.1 * c_total_center;

    /* Fixed capacitance to center the tuning range */
    double c_var_center = varactor_capacitance(&vco.varactor, supply_v / 2.0);
    vco.c_fixed_farads = c_total_center - c_var_center - vco.c_parasitic_farads;
    if (vco.c_fixed_farads < 0.0) vco.c_fixed_farads = 0.0;

    /* Recompute actual min and max frequencies */
    double c_at_vmin = vco.c_fixed_farads + varactor_capacitance(&vco.varactor, vco.varactor.v_min)
                       + vco.c_parasitic_farads;
    double c_at_vmax = vco.c_fixed_farads + varactor_capacitance(&vco.varactor, vco.varactor.v_max)
                       + vco.c_parasitic_farads;

    vco.freq_max_hz = 1.0 / (2.0 * M_PI * sqrt(vco.inductance_h * c_at_vmin));
    vco.freq_min_hz = 1.0 / (2.0 * M_PI * sqrt(vco.inductance_h * c_at_vmax));
    vco.freq_center_hz = (vco.freq_min_hz + vco.freq_max_hz) / 2.0;
    vco.tuning_range_percent = (vco.freq_max_hz - vco.freq_min_hz)
                                / vco.freq_center_hz * 100.0;

    /* Compute K_VCO at mid-range */
    double v_mid = supply_v / 2.0;
    vco.kvco_avg_hz_per_v = lc_vco_kvco(&vco, v_mid);

    /* K_VCO min and max over tuning range */
    vco.kvco_max_hz_per_v = lc_vco_kvco(&vco, vco.varactor.v_min);
    vco.kvco_min_hz_per_v = lc_vco_kvco(&vco, vco.varactor.v_max);
    if (vco.kvco_max_hz_per_v < vco.kvco_min_hz_per_v) {
        double tmp = vco.kvco_max_hz_per_v;
        vco.kvco_max_hz_per_v = vco.kvco_min_hz_per_v;
        vco.kvco_min_hz_per_v = tmp;
    }
    vco.kvco_ratio = (vco.kvco_min_hz_per_v > 0.0)
                      ? vco.kvco_max_hz_per_v / fabs(vco.kvco_min_hz_per_v) : 1.0;

    /* Phase noise estimate at 1 MHz offset */
    vco.phase_noise_at_1mhz = lc_vco_phase_noise_estimate(&vco, 1.0e6, 10.0, 2.0);

    /* Figure of merit */
    vco.figure_of_merit = vco_figure_of_merit(&vco);

    return vco;
}

/**
 * Compute VCO frequency at a given tuning voltage.
 */
double lc_vco_frequency(const lc_vco_t *vco, double v_tune)
{
    if (!vco || vco->inductance_h <= 0.0) return 0.0;

    double c_var = varactor_capacitance(&vco->varactor, v_tune);
    double c_total = vco->c_fixed_farads + c_var + vco->c_parasitic_farads;

    if (c_total <= 0.0) return 1.0e12;  /* very high frequency if C ≈ 0 */

    return 1.0 / (2.0 * M_PI * sqrt(vco->inductance_h * c_total));
}

/**
 * Compute K_VCO at a given tuning voltage by numerical differentiation.
 *
 * K_VCO = df/dV_tune ≈ (f(V+ΔV) - f(V-ΔV)) / (2·ΔV)
 *
 * Using central difference for O(ΔV²) accuracy.
 */
double lc_vco_kvco(const lc_vco_t *vco, double v_tune)
{
    if (!vco) return 0.0;

    double dv = 0.001;  /* 1 mV step for differentiation */
    double v_plus  = v_tune + dv;
    double v_minus = v_tune - dv;

    if (v_minus < vco->varactor.v_min) v_minus = vco->varactor.v_min;
    if (v_plus > vco->varactor.v_max) v_plus = vco->varactor.v_max;

    double f_plus  = lc_vco_frequency(vco, v_plus);
    double f_minus = lc_vco_frequency(vco, v_minus);

    double effective_dv = v_plus - v_minus;
    if (effective_dv <= 0.0) return 0.0;

    return (f_plus - f_minus) / effective_dv;
}

/**
 * Compute VCO tuning curve (frequency and K_VCO vs tuning voltage).
 */
void lc_vco_tuning_curve(const lc_vco_t *vco, size_t num_points,
                           double *v_tune_arr, double *freq_arr, double *kvco_arr)
{
    if (!vco || !v_tune_arr || !freq_arr || !kvco_arr || num_points < 2)
        return;

    double v_min = vco->varactor.v_min;
    double v_max = vco->varactor.v_max;
    double dv = (v_max - v_min) / (double)(num_points - 1);

    for (size_t i = 0; i < num_points; i++) {
        double v = v_min + (double)i * dv;
        v_tune_arr[i] = v;
        freq_arr[i] = lc_vco_frequency(vco, v);
        kvco_arr[i] = lc_vco_kvco(vco, v);
    }
}

/* ─── L5: LC VCO Phase Noise Estimation ──────────────────────────────────── */

/**
 * Estimate LC VCO phase noise using a simplified Leeson model.
 *
 * For an LC VCO with cross-coupled topology:
 *   L(Δf) ≈ 10·log₁₀( F·k·T·R_p·(1+m) / (2·V²_pp) · (f₀/(2·Q·Δf))² )
 *
 * where:
 *   m is a factor accounting for tail current source noise (~1-3)
 *   V_pp is the differential peak-to-peak amplitude
 *   R_p is the tank parallel resistance
 *
 * Simplified form used here:
 *   L(Δf) ≈ PN_floor + 20·log₁₀(f₀/(2·Q·Δf))
 *
 * where PN_floor = 10·log₁₀(F·k·T/(2·P_sig))
 */
double lc_vco_phase_noise_estimate(const lc_vco_t *vco, double offset_hz,
                                      double loaded_q, double noise_factor)
{
    if (!vco || offset_hz <= 0.0) return 0.0;

    double f0 = vco->freq_center_hz;
    if (loaded_q <= 0.0) loaded_q = 10.0;
    if (noise_factor <= 0.0) noise_factor = 2.0;

    /* Signal power: estimate from VCO power budget and amplitude */
    double p_sig = vco->power_mw * 1.0e-3 * 0.3;  /* ~30% efficiency */

    /* Noise floor */
    double floor_linear = noise_factor * K_BOLTZMANN * 290.0 / (2.0 * p_sig);
    double floor_db = 10.0 * log10(floor_linear);

    /* 1/f² region phase noise */
    double offset_factor = f0 / (2.0 * loaded_q * offset_hz);
    double pn_db = floor_db + 20.0 * log10(offset_factor);

    /* Clamp to reasonable range */
    if (pn_db < -200.0) pn_db = -200.0;
    if (pn_db > 0.0) pn_db = 0.0;

    return pn_db;
}

/* ─── L5: VCO Figure of Merit ────────────────────────────────────────────── */

/**
 * Compute VCO figure of merit.
 *
 * FOM = L(Δf) - 20·log₁₀(f₀/Δf) + 10·log₁₀(P_dc/1mW)  [dBc/Hz]
 *
 * This normalizes phase noise by carrier frequency offset and power
 * consumption, allowing fair comparison between different VCO designs.
 *
 * Typical values:
 *   - CMOS LC VCO (good):  FOM < -185 dBc/Hz
 *   - CMOS LC VCO (state-of-art): FOM < -195 dBc/Hz
 *   - Ring VCO:  FOM < -160 dBc/Hz
 *   - Discrete LC VCO: FOM < -175 dBc/Hz
 */
double vco_figure_of_merit(const lc_vco_t *vco)
{
    if (!vco || vco->freq_center_hz <= 0.0) return 0.0;

    double f0 = vco->freq_center_hz;
    double df = 1.0e6;  /* 1 MHz offset standard */
    double pn = vco->phase_noise_at_1mhz;
    double p_dc_mw = vco->power_mw;

    if (p_dc_mw <= 0.0) p_dc_mw = 1.0;

    double fom = pn - 20.0 * log10(f0 / df) + 10.0 * log10(p_dc_mw);

    return fom;
}

/* ─── L5: Ring VCO Design ────────────────────────────────────────────────── */

/**
 * Design a voltage-controlled ring oscillator.
 *
 * Ring VCOs use odd number of delay stages. The delay per stage is
 * modulated by the control voltage (typically through current starving).
 *
 * Frequency: f(V) = I_bias(V) / (N · C_node · V_swing)
 *
 * where I_bias is the tail current per stage, C_node is the load
 * capacitance at each stage output, and V_swing is the voltage swing.
 */
ring_vco_t ring_vco_design(double freq_center_hz, double kvco_target,
                              double supply_v)
{
    ring_vco_t vco;
    memset(&vco, 0, sizeof(vco));

    if (freq_center_hz <= 0.0) return vco;
    if (supply_v <= 0.0) supply_v = 1.8;

    vco.num_stages = 3;  /* minimum for ring oscillator */
    vco.supply_voltage_v = supply_v;

    /* Propagation delay needed: t_pd = 1/(2·N·f) */
    double t_pd_total = 1.0 / (2.0 * vco.num_stages * freq_center_hz);
    vco.t_pd_min_ps = t_pd_total * 0.7 * 1.0e12;
    vco.t_pd_max_ps = t_pd_total * 1.5 * 1.0e12;

    vco.freq_max_hz = 1.0 / (2.0 * vco.num_stages * vco.t_pd_min_ps * 1.0e-12);
    vco.freq_min_hz = 1.0 / (2.0 * vco.num_stages * vco.t_pd_max_ps * 1.0e-12);

    vco.kvco_hz_per_v = (kvco_target > 0.0) ? kvco_target
                         : (vco.freq_max_hz - vco.freq_min_hz) / supply_v;

    /* Power estimate: P = N·C_node·VDD²·f */
    double c_node = 10.0e-15;  /* 10 fF per stage */
    vco.power_uw_per_mhz = vco.num_stages * c_node * supply_v * supply_v * 1.0e6;

    /* Phase noise estimate (ring VCOs are noisy) */
    vco.phase_noise_at_1mhz = -60.0;  /* typical for ring VCO at 1 MHz */

    return vco;
}
