/**
 * @file    oscillator_relaxation.c
 * @brief   Relaxation Oscillator implementations — L4 Laws + L5 Design + L6 Problems
 *
 * @details Implements 555 timer astable, Schmitt trigger RC, op-amp
 *          relaxation, and ring oscillator design and analysis.
 *          Relaxation oscillators produce square/triangle/sawtooth waves
 *          by charging/discharging a timing capacitor between two
 *          threshold voltages.
 *
 * Knowledge covered:
 *   L1: 555 timer astable, Schmitt trigger thresholds, RC charging
 *   L3: Exponential RC charging equation
 *   L4: 555 frequency formula, Schmitt RC frequency formula
 *   L5: Component selection, duty cycle control
 *   L6: 1 kHz 555 oscillator, 100 kHz Schmitt RC oscillator
 *
 * Reference:
 *   Sedra & Smith (2020), Ch.17.4
 *   Horowitz & Hill, "The Art of Electronics" (2015), Ch.7
 */

#include "oscillator_relaxation.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ─── L5: 555 Timer Astable Design ────────────────────────────────────────── */

/**
 * Design a 555 timer astable oscillator.
 *
 * Basic equations (no diode):
 *   t_high = ln(2)·(R₁+R₂)·C ≈ 0.693·(R₁+R₂)·C
 *   t_low  = ln(2)·R₂·C      ≈ 0.693·R₂·C
 *   T = t_high + t_low = ln(2)·(R₁+2R₂)·C
 *   f = 1.44 / ((R₁+2R₂)·C)
 *   D = (R₁+R₂)/(R₁+2R₂) · 100%
 *
 * With diode across R₂ (for 50% duty cycle):
 *   t_high = ln(2)·R₁·C · k_diode (diode forward drop reduces effective voltage)
 *   t_low  = ln(2)·R₂·C
 *   f ≈ 1.44 / ((R₁+R₂)·C)   [simplified]
 *   D ≈ R₁/(R₁+R₂) · 100%
 *
 * Component constraints:
 *   - R₁, R₂: 1kΩ to 10MΩ (outside this range, leakage/bias currents affect timing)
 *   - C: 100pF to 1000µF (dielectric absorption limits accuracy at extremes)
 *   - f_max ≈ 500 kHz (555 timer limitation)
 *   - f_min ≈ 0.001 Hz (practical, limited by C leakage)
 *
 * Design strategy:
 *   1. Choose C based on frequency decade
 *   2. Compute total R = R₁ + 2R₂ from f and C
 *   3. Distribute between R₁ and R₂ based on duty cycle
 */
relaxation_555_t relaxation_555_design(double freq_hz, double duty_cycle_percent)
{
    relaxation_555_t osc;
    memset(&osc, 0, sizeof(osc));

    if (freq_hz <= 0.0) return osc;

    /* Clamp to practical range */
    if (freq_hz > 500000.0) freq_hz = 500000.0;
    if (freq_hz < 0.001) freq_hz = 0.001;

    /* Validate and clamp duty cycle */
    if (duty_cycle_percent < 50.0) {
        /* Without diode, duty cycle is always > 50% in basic 555 astable.
         * Use diode configuration for < 50% or near 50%. */
        osc.has_diode = 1;
        if (duty_cycle_percent < 10.0) duty_cycle_percent = 10.0;
    } else if (duty_cycle_percent > 99.0) {
        duty_cycle_percent = 99.0;
        osc.has_diode = 0;
    } else {
        osc.has_diode = 0;
    }
    osc.duty_cycle_percent = duty_cycle_percent;

    osc.supply_voltage_v = 5.0;
    osc.v_high_threshold = (2.0 / 3.0) * osc.supply_voltage_v;
    osc.v_low_threshold  = (1.0 / 3.0) * osc.supply_voltage_v;
    osc.c_bypass_farads = 10.0e-9;  /* 10 nF standard bypass */

    /* Choose timing capacitor based on frequency */
    if (freq_hz < 1.0) {
        osc.c_farads = 100.0e-6;  /* 100 uF */
    } else if (freq_hz < 10.0) {
        osc.c_farads = 10.0e-6;   /* 10 uF */
    } else if (freq_hz < 100.0) {
        osc.c_farads = 1.0e-6;    /* 1 uF */
    } else if (freq_hz < 1000.0) {
        osc.c_farads = 0.1e-6;    /* 0.1 uF */
    } else if (freq_hz < 10000.0) {
        osc.c_farads = 10.0e-9;   /* 10 nF */
    } else if (freq_hz < 100000.0) {
        osc.c_farads = 1.0e-9;    /* 1 nF */
    } else {
        osc.c_farads = 100.0e-12; /* 100 pF */
    }

    /* Compute total resistance: R₁ + 2R₂ = 1.44/(f·C) [basic]
     * or with diode: R₁ + R₂ = 1.44/(f·C) [approximate] */
    double r_total;
    if (osc.has_diode) {
        r_total = 1.44 / (freq_hz * osc.c_farads);  /* R₁ + R₂ */
    } else {
        r_total = 1.44 / (freq_hz * osc.c_farads);  /* R₁ + 2R₂ */
    }

    if (osc.has_diode) {
        /* For diode configuration: D = R₁/(R₁+R₂)
         * R₁ = D·R_total, R₂ = (1-D)·R_total */
        double d = duty_cycle_percent / 100.0;
        osc.r1_ohms = d * r_total;
        osc.r2_ohms = (1.0 - d) * r_total;
    } else {
        /* Without diode:
         * D = (R₁+R₂)/(R₁+2R₂)
         * Let R₁+2R₂ = r_total, then R₁+R₂ = D·r_total
         * Solving: R₁ = r_total·(2D-1), R₂ = r_total·(1-D) */
        double d = duty_cycle_percent / 100.0;
        if (d <= 0.5) d = 0.51;  /* min duty without diode is 50%+ */
        osc.r1_ohms = r_total * (2.0 * d - 1.0);
        osc.r2_ohms = r_total * (1.0 - d);
    }

    /* Constrain to practical resistor values */
    if (osc.r1_ohms < 1000.0) osc.r1_ohms = 1000.0;
    if (osc.r2_ohms < 1000.0) osc.r2_ohms = 1000.0;
    if (osc.r1_ohms > 10.0e6) osc.r1_ohms = 10.0e6;
    if (osc.r2_ohms > 10.0e6) osc.r2_ohms = 10.0e6;

    /* Recompute actual timing from chosen components */
    if (osc.has_diode) {
        /* With diode: charging through R₁ only, discharging through R₂ only */
        osc.t_high_sec = log(2.0) * osc.r1_ohms * osc.c_farads;
        osc.t_low_sec  = log(2.0) * osc.r2_ohms * osc.c_farads;
    } else {
        /* Without diode: charging through R₁+R₂, discharging through R₂ */
        osc.t_high_sec = log(2.0) * (osc.r1_ohms + osc.r2_ohms) * osc.c_farads;
        osc.t_low_sec  = log(2.0) * osc.r2_ohms * osc.c_farads;
    }

    osc.period_sec = osc.t_high_sec + osc.t_low_sec;
    osc.freq_hz = (osc.period_sec > 0.0) ? 1.0 / osc.period_sec : 0.0;
    osc.duty_cycle_percent = (osc.period_sec > 0.0)
                              ? (osc.t_high_sec / osc.period_sec) * 100.0 : 0.0;

    return osc;
}

/**
 * Validate a 555 oscillator design against target frequency tolerance.
 */
int relaxation_555_validate(const relaxation_555_t *osc, double freq_tol_pct)
{
    if (!osc || osc->freq_hz <= 0.0) return 0;
    if (freq_tol_pct <= 0.0) freq_tol_pct = 5.0;
    /* Actual components give actual frequency. We check if period > 0
     * and components are in reasonable range. */
    if (osc->r1_ohms < 1000.0 || osc->r2_ohms < 1000.0) return 0;
    if (osc->c_farads < 100.0e-12) return 0;
    if (osc->period_sec <= 0.0) return 0;
    return 1;
}

/* ─── L5: Schmitt Trigger RC Relaxation Oscillator ────────────────────────── */

/**
 * Design a Schmitt-trigger inverter (74HC14) based relaxation oscillator.
 *
 * The Schmitt trigger thresholds for 74HC14 at VDD=5V:
 *   V_T+ ≈ 2.7V (positive-going threshold)
 *   V_T- ≈ 1.6V (negative-going threshold)
 *
 * For CMOS Schmitt trigger with typical 1/3-2/3 VDD thresholds:
 *   V_T+ ≈ 2/3 VDD, V_T- ≈ 1/3 VDD
 *
 * Charging equation (output HIGH → capacitor charges toward VDD):
 *   v(t) = VDD - (VDD - V_T-)·exp(-t/(RC))
 *   → t_high = RC·ln((VDD-V_T-)/(VDD-V_T+))
 *
 * Discharging equation (output LOW → capacitor discharges toward 0):
 *   v(t) = V_T+ · exp(-t/(RC))
 *   → t_low = RC·ln(V_T+/V_T-)
 *
 * If V_T+ = VDD - V_T- (symmetric):
 *   t_high = t_low = RC·ln(V_T+/V_T-)
 *   f = 1/(2RC·ln(V_T+/V_T-))
 *
 * For 74HC14 typical: ln(V_T+/V_T-) ≈ ln(2.7/1.6) ≈ 0.523
 *   f ≈ 1/(2RC·0.523) ≈ 0.956/(RC)
 */
relaxation_schmitt_t relaxation_schmitt_design(double freq_hz, double supply_v,
                                                  double r_chosen)
{
    relaxation_schmitt_t osc;
    memset(&osc, 0, sizeof(osc));

    if (freq_hz <= 0.0) return osc;
    if (supply_v <= 0.0) supply_v = 5.0;

    osc.v_supply = supply_v;

    /* Typical CMOS Schmitt trigger thresholds */
    osc.vt_plus  = (2.0 / 3.0) * supply_v;
    osc.vt_minus = (1.0 / 3.0) * supply_v;
    osc.v_hysteresis = osc.vt_plus - osc.vt_minus;

    /* Compute timing: for symmetric thresholds,
     * t_high = t_low = RC·ln(V_T+/V_T-) */
    double ln_ratio = (osc.vt_minus > 0.0) ? log(osc.vt_plus / osc.vt_minus) : 0.693;

    if (r_chosen > 0.0) {
        osc.r_ohms = r_chosen;
        osc.c_farads = 1.0 / (2.0 * freq_hz * osc.r_ohms * ln_ratio);
    } else {
        /* Choose capacitor first */
        if (freq_hz < 1000.0) {
            osc.c_farads = 0.1e-6;
        } else if (freq_hz < 100000.0) {
            osc.c_farads = 1.0e-9;
        } else {
            osc.c_farads = 100.0e-12;
        }
        osc.r_ohms = 1.0 / (2.0 * freq_hz * osc.c_farads * ln_ratio);
        /* Clamp to practical range */
        if (osc.r_ohms < 1000.0) osc.r_ohms = 1000.0;
        if (osc.r_ohms > 1.0e6) osc.r_ohms = 1.0e6;
    }

    /* Recompute actual timings */
    osc.t_high_sec = osc.r_ohms * osc.c_farads
                      * log((osc.v_supply - osc.vt_minus) / (osc.v_supply - osc.vt_plus));
    if (osc.t_high_sec < 0.0) osc.t_high_sec = osc.r_ohms * osc.c_farads * ln_ratio;

    osc.t_low_sec = osc.r_ohms * osc.c_farads
                     * (osc.vt_minus > 0.0 ? log(osc.vt_plus / osc.vt_minus) : 0.693);

    osc.period_sec = osc.t_high_sec + osc.t_low_sec;
    osc.freq_hz = (osc.period_sec > 0.0) ? 1.0 / osc.period_sec : 0.0;
    osc.duty_cycle_percent = (osc.period_sec > 0.0)
                              ? (osc.t_high_sec / osc.period_sec) * 100.0 : 0.0;

    return osc;
}

/* ─── L5: Op-Amp Relaxation Oscillator ───────────────────────────────────── */

/**
 * Design an op-amp relaxation oscillator (triangle + square wave generator).
 *
 * Uses two op-amps: one as a Schmitt trigger, one as an integrator.
 * The integrator ramps between the Schmitt threshold voltages,
 * generating a triangle wave. The Schmitt trigger output is a square wave.
 *
 * Integrator: output slope = ±V_sat/(R·C)
 * Schmitt thresholds: ±V_sat·(R₁/R₂)
 *
 * Period: T = 4·R·C·(R₁/R₂)
 * Frequency: f = R₂/(4·R·C·R₁)
 *
 * Triangle amplitude: V_tri_pp = 2·V_sat·(R₁/R₂)
 */
relaxation_opamp_t relaxation_opamp_design(double freq_hz,
                                              double triangle_amp_vpp,
                                              double supply_v)
{
    relaxation_opamp_t osc;
    memset(&osc, 0, sizeof(osc));

    if (freq_hz <= 0.0) return osc;
    if (supply_v <= 0.0) supply_v = 12.0;

    /* Op-amp saturation: typically 1-2V less than supply rails */
    osc.v_sat = supply_v - 1.5;
    if (osc.v_sat < 1.0) osc.v_sat = 1.0;

    /* From desired triangle amplitude:
     * V_tri_pp = 2·V_sat·(R₁/R₂)  →  R₁/R₂ = V_tri_pp/(2·V_sat) */
    if (triangle_amp_vpp <= 0.0) triangle_amp_vpp = osc.v_sat;
    double r_ratio = triangle_amp_vpp / (2.0 * osc.v_sat);
    if (r_ratio > 0.9) r_ratio = 0.5;  /* limit to avoid saturation */
    if (r_ratio < 0.01) r_ratio = 0.1;

    osc.r2_schmitt_ohms = 10000.0;  /* 10k standard */
    osc.r1_schmitt_ohms = osc.r2_schmitt_ohms * r_ratio;

    /* From frequency: f = R₂/(4·R·C·R₁) → RC = R₂/(4·f·R₁) */
    double rc_product = osc.r2_schmitt_ohms / (4.0 * freq_hz * osc.r1_schmitt_ohms);

    /* Choose C then R */
    if (freq_hz < 100.0) {
        osc.c_integ_farads = 1.0e-6;
    } else if (freq_hz < 1000.0) {
        osc.c_integ_farads = 0.1e-6;
    } else if (freq_hz < 10000.0) {
        osc.c_integ_farads = 10.0e-9;
    } else {
        osc.c_integ_farads = 1.0e-9;
    }
    osc.r_integ_ohms = rc_product / osc.c_integ_farads;
    if (osc.r_integ_ohms < 1000.0) osc.r_integ_ohms = 1000.0;
    if (osc.r_integ_ohms > 1.0e6) osc.r_integ_ohms = 1.0e6;

    /* Recompute actual values */
    osc.v_th_plus  =  osc.v_sat * (osc.r1_schmitt_ohms / osc.r2_schmitt_ohms);
    osc.v_th_minus = -osc.v_sat * (osc.r1_schmitt_ohms / osc.r2_schmitt_ohms);
    osc.period_sec = 4.0 * osc.r_integ_ohms * osc.c_integ_farads
                     * (osc.r1_schmitt_ohms / osc.r2_schmitt_ohms);
    osc.freq_hz = (osc.period_sec > 0.0) ? 1.0 / osc.period_sec : 0.0;
    osc.triangle_amplitude_vpp = 2.0 * osc.v_sat * (osc.r1_schmitt_ohms / osc.r2_schmitt_ohms);
    osc.square_amplitude_vpp = 2.0 * osc.v_sat;

    return osc;
}

/* ─── L5: Ring Oscillator Design ──────────────────────────────────────────── */

/**
 * Design a CMOS ring oscillator.
 *
 * Frequency: f = 1/(2·N·t_pd) where N is odd and >= 3.
 *
 * The propagation delay per inverter depends on:
 *   - Process technology (e.g., 65nm: t_pd ≈ 10-20 ps)
 *   - Supply voltage (higher VDD → lower t_pd)
 *   - Load capacitance (fan-out)
 *
 * Total power: P = N·C_load·VDD²·f
 *   where C_load is the load capacitance at each stage output.
 *
 * Phase noise (simplified model):
 *   L(Δf) ≈ 10·log₁₀( (8kT·N)/(3η·P) · (f₀/Δf)² )
 *   where η is a proportionality constant ~0.5-0.7
 */
ring_osc_t ring_oscillator_design(int num_stages, double t_pd_ps, double supply_v)
{
    ring_osc_t osc;
    memset(&osc, 0, sizeof(osc));

    if (num_stages < 3) num_stages = 3;
    if (num_stages % 2 == 0) num_stages++;  /* must be odd */
    if (supply_v <= 0.0) supply_v = 1.8;    /* typical CMOS */

    osc.num_stages = num_stages;
    osc.t_pd_per_stage_ps = (t_pd_ps > 0.0) ? t_pd_ps : 20.0;
    osc.supply_voltage_v = supply_v;

    osc.total_loop_delay_ps = num_stages * osc.t_pd_per_stage_ps;
    osc.freq_hz = 1.0 / (2.0 * osc.total_loop_delay_ps * 1.0e-12);

    /* Dynamic power per stage: P = C·V²·f
     * Estimate load capacitance per stage: ~5 fF (internal) + fanout */
    double c_load = 10.0e-15;  /* 10 fF per stage */
    osc.power_per_stage_uw = c_load * supply_v * supply_v * osc.freq_hz * 1.0e6;
    osc.total_power_mw = osc.power_per_stage_uw * num_stages / 1000.0;

    /* Phase noise estimate at 1 MHz offset */
    double p_total_w = osc.total_power_mw * 1.0e-3;
    double eta = 0.6;
    if (p_total_w > 0.0) {
        osc.phase_noise_1mhz_dbc = 10.0 * log10(
            (8.0 * 1.38e-23 * 300.0 * num_stages) / (3.0 * eta * p_total_w)
            * pow(osc.freq_hz / 1.0e6, 2.0)
        );
    } else {
        osc.phase_noise_1mhz_dbc = -60.0;
    }

    return osc;
}
