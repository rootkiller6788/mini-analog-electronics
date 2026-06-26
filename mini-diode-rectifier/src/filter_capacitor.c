/**
 * filter_capacitor.c -- Rectifier Output Filter Implementation
 *
 * Implements capacitor-input, LC choke-input, and pi-section filter analysis.
 * These filters smooth the pulsating DC from rectifiers into usable DC.
 *
 * Reference: Sedra & Smith Ch.4, App.E; Erickson & Maksimovic Ch.15;
 *            Schade (1943) "Analysis of Rectifier Operation", Proc. IRE
 */

#include "filter_capacitor.h"
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * L5: Capacitor-Input Filter Ripple Analysis
 * ============================================================================ */

RippleAnalysis filter_capacitor_ripple_analyze(const CapacitorFilterParams *params)
{
    /* Complete ripple analysis for capacitor-input filter.
     *
     * The capacitor charges to V_m through the diode during the conduction
     * interval, then discharges through R_load between conduction pulses.
     *
     * Discharge model (exponential decay):
     *   V_c(t) = V_m * exp(-t / (R*C))
     *   V_dc_min = V_m * exp(-T_discharge / (R*C))
     *   where T_discharge ≈ 1/f_ripple for full-wave, 2/f_ripple for half-wave
     *
     * Peak-to-peak ripple (linear approximation, valid for V_r_pp << V_m):
     *   V_r_pp ≈ I_load / (f_ripple * C)
     *
     * Conduction angle (Schade 1943):
     *   cos(theta_c) ≈ 1 - 2*V_r_pp/V_m
     *   For small ripple: theta_c ≈ 2*sqrt(2*V_r_pp/V_m)
     *
     * Peak repetitive current (Schade's formula):
     *   I_peak ≈ I_load * pi * sqrt(2*V_m/V_r_pp)
     *   This can be 5-20x the average load current!
     *
     * The peak current is the reason electrolytic capacitors must
     * have adequate ripple current ratings (typically 1-10 A RMS for
     * power supply capacitors).
     *
     * Ref: O.H. Schade, "Analysis of Rectifier Operation", Proc. IRE, Vol.31, 1943
     */
    RippleAnalysis r = {0};

    if (!params || params->C <= 0.0 || params->R_load <= 0.0) return r;

    double C = params->C;
    double R = params->R_load;
    double V_m = params->V_m;
    double f_r = params->f_ripple;

    if (f_r <= 0.0) return r;

    /* Discharge time between conduction pulses */
    double T_discharge = 1.0 / f_r;

    /* Exponential decay model */
    double decay_factor = T_discharge / (R * C);
    if (decay_factor > 10.0) decay_factor = 10.0;  /* Prevent underflow */
    r.V_dc_min = V_m * exp(-decay_factor);
    r.V_dc_max = V_m;

    /* Peak-to-peak ripple */
    r.V_r_pp = r.V_dc_max - r.V_dc_min;

    /* Alternative linear approximation for comparison */
    double I_load_est = r.V_dc_min / R;
    /* r.V_r_pp_linear = I_load / (f_r * C)  — stored implicitly */

    /* Average DC voltage: midpoint approximation */
    r.V_dc_avg = (r.V_dc_max + r.V_dc_min) / 2.0;

    /* RMS ripple: triangular approximation ~= V_pp / (2*sqrt(3)) */
    r.V_r_rms = r.V_r_pp / (2.0 * 1.73205080757);

    /* Ripple factor */
    if (r.V_dc_avg > 1e-15)
        r.ripple_factor = r.V_r_rms / r.V_dc_avg;

    /* Conduction angle: cos(theta_c) = 1 - 2*V_r_pp/V_m */
    double cos_theta = 1.0 - 2.0 * r.V_r_pp / V_m;
    if (cos_theta < -1.0) cos_theta = -1.0;
    if (cos_theta > 1.0) cos_theta = 1.0;
    r.conduction_angle_rad = acos(cos_theta);
    r.conduction_duty = r.conduction_angle_rad / (2.0 * M_PI);

    /* Peak repetitive current */
    if (r.V_r_pp > 1e-15) {
        r.I_peak = I_load_est * M_PI * sqrt(2.0 * V_m / r.V_r_pp);
    } else {
        r.I_peak = I_load_est;
    }

    /* RMS and average diode current (single diode in half-wave equivalent) */
    r.I_rms_diode = r.I_peak * sqrt(r.conduction_duty / 3.0);
    r.I_avg_diode = r.I_peak * r.conduction_duty / M_PI;

    /* Minimum C required for the observed ripple */
    if (r.V_r_pp > 1e-15 && f_r > 0.0)
        r.C_min_required = I_load_est / (f_r * r.V_r_pp);
    else
        r.C_min_required = C;

    /* Output impedance at ripple frequency */
    r.output_impedance = 1.0 / (2.0 * M_PI * f_r * C) + params->ESR;

    return r;
}

/* ============================================================================
 * L5: Minimum Capacitance Calculation
 * ============================================================================ */

double filter_minimum_capacitance(double I_load, double f_ripple, double V_r_pp_max)
{
    /* C_min = I_load / (f_ripple * V_r_pp_max)
     *
     * This is the classic "1/RC" rule for capacitor sizing.
     * Derived from: Q = C*V, dQ = I*dt, so C = I*dt/dV ≈ I/(f*dV).
     *
     * Example: 1A load, 120Hz ripple (full-wave 60Hz), 1V ripple target:
     *   C_min = 1 / (120 * 1) = 8333 uF
     * In practice, use 2-3x this value for margin and ESR effects.
     *
     * For half-wave (60Hz ripple at 60Hz line): twice the C needed.
     */
    if (f_ripple <= 0.0 || V_r_pp_max <= 0.0) return 0.0;
    return I_load / (f_ripple * V_r_pp_max);
}

/* ============================================================================
 * L5: LC Choke-Input Filter Analysis
 * ============================================================================ */

LCFilterResult filter_lc_choke_analyze(const LCFilterParams *params)
{
    /* LC choke-input filter analysis.
     *
     * The inductor maintains continuous current flow, which:
     *   - Reduces peak diode currents (good for transformer and diodes)
     *   - Provides better voltage regulation
     *   - Reduces ripple without large capacitors
     *
     * Critical inductance for continuous conduction:
     *   L_crit = R_load / (3 * pi * f_ripple)   for full-wave
     *   L_crit = R_load / (3 * pi * f_ripple/2) for half-wave
     *
     * Below L_crit, the inductor current becomes discontinuous,
     * and output voltage rises toward V_m (capacitor-input behavior).
     *
     * In continuous conduction mode:
     *   V_dc_avg ≈ 2*V_m/pi   (same as ideal full-wave)
     *   Ripple attenuation from LC low-pass:
     *   V_r_out / V_r_in ≈ 1 / ((2*pi*f_r)^2 * L*C)  for f_r >> f_cutoff
     *
     * Ref: Mohan Ch.5.5, Erickson & Maksimovic Ch.15
     */
    LCFilterResult r = {0};

    if (!params) return r;

    double L = params->L;
    double C = params->C;
    double R = params->R_load;
    double V_m = params->V_m;
    double f_r = params->f_ripple;

    if (R <= 0.0) return r;

    /* Critical inductance */
    r.L_critical_actual = R / (3.0 * M_PI * f_r);

    /* Check continuous conduction */
    r.is_continuous = (L >= r.L_critical_actual) ? 1 : 0;

    if (r.is_continuous) {
        r.V_dc_avg = 2.0 * V_m / M_PI;  /* Ideal full-wave average */
        r.I_dc = r.V_dc_avg / R;
    } else {
        /* Discontinuous: behaves like capacitor-input, V_dc approaches V_m */
        r.V_dc_avg = V_m * 0.9;  /* Crude approximation */
        r.I_dc = r.V_dc_avg / R;
    }

    /* Cutoff frequency */
    if (L > 0.0 && C > 0.0) {
        r.f_cutoff = 1.0 / (2.0 * M_PI * sqrt(L * C));
    }

    /* Ripple attenuation */
    if (r.f_cutoff > 0.0 && f_r > r.f_cutoff) {
        /* -40 dB/decade for ideal LC above cutoff */
        double ratio = f_r / r.f_cutoff;
        r.attenuation_db = 40.0 * log10(ratio);
    } else if (L > 0.0 && C > 0.0) {
        r.attenuation_db = 0.0;
    }

    /* Ripple estimation */
    double V_r_in = V_m * 0.48;  /* ~48% ripple input for full-wave */
    double attenuation_linear = pow(10.0, -r.attenuation_db / 20.0);
    r.V_r_pp = V_r_in * attenuation_linear;
    if (r.V_dc_avg > 1e-15)
        r.ripple_factor = (r.V_r_pp / (2.0 * 1.732)) / r.V_dc_avg;

    /* Current estimates */
    r.I_peak = r.I_dc + r.V_r_pp / (2.0 * 2.0 * M_PI * f_r * L);
    r.I_min = r.I_dc - r.V_r_pp / (2.0 * 2.0 * M_PI * f_r * L);
    if (r.I_min < 0.0) {
        r.I_min = 0.0;
        r.is_continuous = 0;
    }

    return r;
}

/* ============================================================================
 * L5: Pi Filter Analysis
 * ============================================================================ */

PiFilterResult filter_pi_analyze(const PiFilterParams *params)
{
    /* Pi-section (C-L-C) filter analysis.
     *
     * Two-stage filtering:
     * Stage 1: C1 smooths the raw rectified waveform (like capacitor-input)
     * Stage 2: L + C2 forms an LC low-pass for additional ripple reduction
     *
     * Total attenuation is the product of stage 1 and stage 2:
     *   Attenuation ≈ 1 / ((2*pi*f_r)^2 * L*C2) * (1 / (2*pi*f_r*R_load*C1))
     *   At very high f_r relative to cutoff: ~ -60 dB/decade
     *
     * Pi filters are common in tube amplifier power supplies (requiring
     * very low ripple for audio) and precision analog circuits.
     */
    PiFilterResult r = {0};

    if (!params) return r;

    double C1 = params->C1;
    double L = params->L;
    double C2 = params->C2;
    double R = params->R_load;
    double f_r = params->f_ripple;

    /* Stage 1: capacitor-input approximation */
    double X_C1 = 1.0 / (2.0 * M_PI * f_r * C1);
    double atten1 = X_C1 / sqrt(R * R + X_C1 * X_C1);

    /* Stage 2: LC filter */
    double f_res = 1.0 / (2.0 * M_PI * sqrt(L * C2));
    r.f_resonance = f_res;
    r.Q_factor = (1.0 / R) * sqrt(L / C2);

    double atten2;
    if (f_r > f_res * 2.0) {
        /* Above resonance: -40 dB/decade */
        atten2 = (f_res / f_r) * (f_res / f_r);
    } else {
        /* Near or below resonance: limited attenuation */
        atten2 = 1.0;
    }

    /* Total attenuation */
    double atten_total = atten1 * atten2;
    r.attenuation_total_db = -20.0 * log10(atten_total);
    if (r.attenuation_total_db < 0.0) r.attenuation_total_db = 0.0;

    /* Ripple estimates */
    double V_r_in = 10.0;  /* Placeholder: raw rectifier ripple */
    r.V_r_pp_intermediate = V_r_in * atten1;
    r.V_r_pp_output = V_r_in * atten_total;

    if (R > 0.0)
        r.V_dc_avg = 2.0 * 170.0 / M_PI;  /* ~108V for 120Vrms example */
    r.ripple_factor = (r.V_r_pp_output / (2.0 * 1.732)) / r.V_dc_avg;

    return r;
}

/* ============================================================================
 * L5: Automated Filter Design
 * ============================================================================ */

FilterDesignResult filter_design_auto(const FilterDesignTarget *target)
{
    /* Automated filter component selection.
     *
     * Algorithm:
     * 1. Calculate minimum C for capacitor-input (simplest case)
     * 2. If C is unreasonably large (>10000 uF), consider LC or active
     * 3. Check peak current constraint
     * 4. Output recommended component values
     *
     * This is a practical design aid, not a full optimizer.
     * Real designs require iteration with SPICE simulation.
     */
    FilterDesignResult r = {0};

    if (!target) return r;

    double C_min = filter_minimum_capacitance(target->I_load, target->f_ripple,
                                              target->V_ripple_max_pp);

    if (C_min < 0.01 && target->preferred_type != FILTER_LC_LOWPASS) {
        /* Capacitor-input is practical */
        r.chosen_type = FILTER_CAPACITOR_INPUT;
        r.C = C_min * 2.0;  /* 2x margin */
        r.L = 0.0;
        r.R = 0.0;
        r.component_count = 1;
    } else if (C_min < 0.1) {
        /* Large but possible: pi filter gives better performance */
        r.chosen_type = FILTER_PI_CAPACITOR;
        r.C = C_min;
        r.L = 0.001;  /* 1 mH typical for low-voltage */
        r.component_count = 3;
    } else {
        /* Impractical capacitance: use LC choke-input */
        r.chosen_type = FILTER_LC_LOWPASS;
        r.L = 0.01;  /* 10 mH */
        r.C = C_min * 0.1;  /* Much smaller C possible with LC */
        r.component_count = 2;
    }

    r.estimated_V_dc = target->V_out_min + target->V_ripple_max_pp / 2.0;
    r.estimated_V_r_pp = target->V_ripple_max_pp;
    r.cost_factor = r.component_count * (r.C * 1e6 + r.L * 1e3);

    return r;
}

/* ============================================================================
 * L3: RC Discharge Model
 * ============================================================================ */

double filter_rc_discharge(double V_initial, double t, double R, double C)
{
    /* V(t) = V_initial * exp(-t / (R * C))
     *
     * The fundamental exponential decay of a capacitor discharging
     * through a resistor. Time constant tau = R * C.
     *
     * After 1 tau: V = 0.368 * V_initial
     * After 3 tau: V = 0.050 * V_initial (95% discharged)
     * After 5 tau: V = 0.007 * V_initial (99.3% discharged)
     *
     * This is the underlying model for ripple voltage estimation
     * in capacitor-input filters.
     */
    if (R <= 0.0 || C <= 0.0) return V_initial;
    if (t < 0.0) t = 0.0;

    return V_initial * exp(-t / (R * C));
}

/* ============================================================================
 * L3: Capacitor Charging Model
 * ============================================================================ */

double filter_charging_voltage(double V_m, double V_c0, double t,
                               double R_effective, double C)
{
    /* V_c(t) = V_m - (V_m - V_c0) * exp(-t / (R_eff * C))
     *
     * Models the charging of a filter capacitor through the diode
     * and transformer winding resistance. R_effective includes:
     *   - Transformer secondary winding resistance
     *   - Diode dynamic resistance (r_d)
     *   - Wiring/trace resistance
     *
     * The charging is typically much faster than discharge because
     * R_effective << R_load, leading to the narrow conduction pulses
     * characteristic of capacitor-input rectifiers.
     */
    if (R_effective <= 0.0 || C <= 0.0) return V_m;
    if (t < 0.0) t = 0.0;

    return V_m - (V_m - V_c0) * exp(-t / (R_effective * C));
}

/* ============================================================================
 * L6: Output Impedance vs Frequency
 * ============================================================================ */

double filter_output_impedance(FilterType type, double C, double L, double ESR,
                               double frequency_Hz)
{
    /* Compute filter output impedance at a given frequency.
     *
     * For capacitor-input: Z ≈ ESR + 1/(j*w*C)
     *   |Z| = sqrt(ESR^2 + 1/(w*C)^2)
     *
     * For LC: Z = (j*w*L + ESR) || (1/(j*w*C) + R_load)
     *
     * Low output impedance is critical for good load transient response.
     * This is why modern supplies often use multi-stage LC filtering
     * and why electrolytic capacitors are chosen for low ESR.
     */
    if (frequency_Hz <= 0.0 || C <= 0.0) return 1e12;  /* Open circuit at DC */

    double w = 2.0 * M_PI * frequency_Hz;
    double X_C = 1.0 / (w * C);

    switch (type) {
        case FILTER_CAPACITOR_INPUT:
        case FILTER_NONE:
            return sqrt(ESR * ESR + X_C * X_C);

        case FILTER_LC_LOWPASS:
            if (L > 0.0) {
                double X_L = w * L;
                double Z_LC = fabs(X_L - X_C);
                return sqrt(ESR * ESR + Z_LC * Z_LC);
            }
            return sqrt(ESR * ESR + X_C * X_C);

        default:
            return sqrt(ESR * ESR + X_C * X_C);
    }
}

/* ============================================================================
 * L6: Load Step Response
 * ============================================================================ */

double filter_load_step_response(double C, double delta_I, double response_time_s)
{
    /* Estimate voltage dip from a sudden load current increase.
     *
     * delta_V = delta_I * t_response / C
     *
     * This is the "charge removal" model: before the control loop
     * can respond, the output capacitor must supply the load current.
     * The voltage sags linearly as charge is removed.
     *
     * Example: 1000 uF cap, 1A step, 10 us response time:
     *   delta_V = 1 * 10e-6 / 1000e-6 = 10 mV sag
     *
     * This is why fast transient response requires either:
     *   - Large output capacitance
     *   - High control loop bandwidth (small t_response)
     *   - Low-ESR capacitors (ceramic MLCC for high frequency)
     */
    if (C <= 0.0) return 1e6;  /* No capacitance = huge dip */
    if (response_time_s < 0.0) response_time_s = 0.0;

    return delta_I * response_time_s / C;
}

/* ============================================================================
 * L2: Filter Type Recommendation
 * ============================================================================ */

FilterType filter_recommend(double I_load, double V_ripple_required,
                            int noise_sensitive, int cost_sensitive)
{
    /* Heuristic filter type recommendation.
     *
     * Decision logic:
     *   <0.1A, cost-sensitive: capacitor-input (cheapest, simplest)
     *   0.1-5A: pi filter (good balance of cost and performance)
     *   >5A: LC choke-input (higher efficiency at high current)
     *   Noise-sensitive: add active post-regulator recommendation
     *
     * Real-world examples:
     *   - iPhone charger (5V/1A): capacitor-input + ceramic MLCC (cost-sensitive)
     *   - Toyota ECU supply: pi filter + LDO (noise-sensitive)
     *   - Tesla on-board charger: active PFC + LLC resonant (high-power)
     */
    if (noise_sensitive && !cost_sensitive)
        return FILTER_ACTIVE_REGULATOR;

    if (cost_sensitive && I_load < 0.5)
        return FILTER_CAPACITOR_INPUT;

    if (I_load > 5.0)
        return FILTER_LC_LOWPASS;

    if (V_ripple_required < 0.001)  /* < 1mV ripple */
        return FILTER_PI_CAPACITOR;

    return FILTER_CAPACITOR_INPUT;  /* Default: simple and effective */
}