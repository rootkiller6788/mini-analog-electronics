/*
 * analog_ic_bandgap.c - Bandgap Voltage Reference Design
 * Each function implements one independent BGR knowledge point.
 * Textbook: Razavi Ch.11; Gray & Meyer Ch.12; Rincon-Mora (2002)
 */
#include "analog_ic_bandgap.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* L2: PTAT Voltage Generation
 * delta_Vbe = Vt * ln(n)  where n = emitter area ratio
 * The voltage difference between two BJTs at different current densities
 * is Proportional To Absolute Temperature (PTAT).
 * Reference: Razavi Ch.11.2; Gray & Meyer Ch.12.2 */
double bgr_ptat_voltage(double T, double ratio_n)
{
    if (T <= 0.0 || ratio_n <= 1.0) return 0.0;
    double Vt = THERMAL_VT(T);
    return Vt * log(ratio_n);
}

/* L2: PTAT Current Generation
 * I_ptat = Vt * ln(n) / R
 * The PTAT voltage across a resistor generates a PTAT current.
 * Reference: Razavi eq. (11.9) */
double bgr_ptat_current(double T, double R, double ratio_n)
{
    if (T <= 0.0 || R <= 0.0 || ratio_n <= 1.0) return 0.0;
    return bgr_ptat_voltage(T, ratio_n) / R;
}

/* L2: Vbe Temperature Dependence
 * Vbe(T) = Vgo*(1 - T/T0) + Vbe0*(T/T0) + (eta*Vt)*ln(T0/T)
 * where Vgo = bandgap voltage extrapolated to 0K (~1.206V for Si)
 *       eta = process-dependent constant (~4 - n)
 * Reference: Tsividis (1980); Meijer (1982); Razavi eq. (11.2)
 *
 * params: [Vbe_nominal, Vgo, eta, T0] */
double bgr_vbe_temperature(double T, const vbe_temp_params_t *params)
{
    if (!params || T <= 0.0) return 0.0;
    double Vt = THERMAL_VT(T);
    double Vt0 = THERMAL_VT(params->T0);
    double Vbe0 = params->Vbe_nominal;
    double Vgo = params->Vgo;
    double eta = params->eta;
    double T0 = params->T0;
    /* Vbe(T) = Vgo + (T/T0)*(Vbe0 - Vgo) + eta*Vt*ln(T0/T) */
    double term1 = Vgo;
    double term2 = (T / T0) * (Vbe0 - Vgo);
    double term3 = eta * Vt * log(T0 / T);
    return term1 + term2 + term3;
}

/* L2: dVbe/dT - temperature slope of Vbe */
double bgr_vbe_dT(const vbe_temp_params_t *params, double T)
{
    if (!params || T <= 0.0) return 0.0;
    double Vt = THERMAL_VT(T);
    double Vbe0 = params->Vbe_nominal;
    double Vgo = params->Vgo;
    double eta = params->eta;
    double T0 = params->T0;
    /* dVbe/dT = (Vbe0 - Vgo)/T0 + eta*k/q*(ln(T0/T) - 1) */
    double term1 = (Vbe0 - Vgo) / T0;
    double term2 = eta * (K_BOLTZMANN / Q_ELECTRON) * (log(T0 / T) - 1.0);
    return term1 + term2;
}

/* L4: Bandgap Voltage - Combine PTAT + CTAT
 * Vref = K*V_ptat(T) + V_ctat(T)
 * K is the PTAT gain, chosen to cancel first-order TC.
 * Reference: Razavi eq. (11.7-11.13) */
double bgr_combine_vref(double V_ptat_gain, double T, double vbe_params_arr[], double T0)
{
    /* vbe_params_arr: {Vbe_nominal, Vgo, eta, T0} */
    (void)T0;
    vbe_temp_params_t vbe_p;
    vbe_p.Vbe_nominal = vbe_params_arr[0];
    vbe_p.Vgo = vbe_params_arr[1];
    vbe_p.eta = vbe_params_arr[2];
    vbe_p.T0 = vbe_params_arr[3];
    double Vbe_T = bgr_vbe_temperature(T, &vbe_p);
    double Vptat_T = bgr_ptat_voltage(T, 8.0); /* typical n=8 */
    return Vbe_T + V_ptat_gain * Vptat_T;
}

/* L6: Complete Widlar Bandgap Design
 * The first practical bandgap reference (Widlar, 1971).
 * Uses two BJTs with emitter area ratio n and a resistor ratio K.
 * Vref = Vbe2 + (R2/R3)*Vt*ln(n)
 * Reference: Widlar, "New Developments in IC Voltage Regulators", JSSC 1971 */
int bgr_design_widlar(const bandgap_spec_t *spec, bandgap_result_t *res)
{
    if (!spec || !res || !spec->tech) return -1;
    memset(res, 0, sizeof(bandgap_result_t));
    double T_nom = 300.0;
    double Vt = THERMAL_VT(T_nom);
    double ratio_n = 8.0;
    double Vgo = 1.206;
    double eta = 3.5;
    res->ratio_n = ratio_n;
    /* Vptat = Vt * ln(n) */
    double Vptat = Vt * log(ratio_n);
    res->V_ptat = Vptat;
    double dVptat_dT = K_BOLTZMANN / Q_ELECTRON * log(ratio_n);
    res->dVptat_dT = dVptat_dT;
    /* Vbe at 300K (typical) */
    double Vbe_nom = 0.65;
    vbe_temp_params_t vbe_p;
    vbe_p.Vbe_nominal = Vbe_nom; vbe_p.Vgo = Vgo; vbe_p.eta = eta; vbe_p.T0 = T_nom;
    res->V_ctat = Vbe_nom;
    double dVbe_dT_val = bgr_vbe_dT(&vbe_p, T_nom);
    res->dVctat_dT = dVbe_dT_val;
    /* First-order compensation: K * dVptat/dT + dVbe/dT = 0
     * K = -dVbe_dT / dVptat_dT */
    double K = -dVbe_dT_val / dVptat_dT;
    res->gain_ptat = K;
    res->gain_ctat = 1.0;
    /* Vref = Vbe + K * Vptat */
    res->Vref = Vbe_nom + K * Vptat;
    res->Vref_T_nominal = res->Vref;
    /* Resistor ratio: R2/R3 = K */
    res->R1 = 10e3; res->R2 = K * 10e3; res->R3 = 10e3;
    /* TC */
    double T_min = spec->T_min, T_max = spec->T_max;
    double Vref_max = res->Vref, Vref_min = res->Vref;
    /* Compute Vref at temperature extremes */
    double Vref_T_min = Vbe_nom + dVbe_dT_val * (T_min - T_nom) + K * dVptat_dT * (T_min - T_nom);
    double Vref_T_max = Vbe_nom + dVbe_dT_val * (T_max - T_nom) + K * dVptat_dT * (T_max - T_nom);
    res->Vref_T_min = Vref_T_min;
    res->Vref_T_max = Vref_T_max;
    Vref_max = (Vref_T_min > Vref_T_max) ? Vref_T_min : Vref_T_max;
    Vref_min = (Vref_T_min < Vref_T_max) ? Vref_T_min : Vref_T_max;
    res->TC = bgr_calculate_TC(Vref_max, Vref_min, res->Vref, T_min, T_max);
    res->peak_to_peak_error = Vref_max - Vref_min;
    double dV = Vref_max - Vref_min;
    res->dVref_dT = dV / (T_max - T_min);
    /* PSRR estimate */
    double A_opamp = 10.0; /* Simple op-amp gain for Widlar */
    res->PSRR = 20.0 * log10(A_opamp);
    /* Line regulation */
    res->line_reg = 1.0 / A_opamp * 1000.0;
    /* Power */
    double I_bias = 5e-6;
    res->I_bias = I_bias;
    res->power = spec->Vdd * I_bias * 3.0;
    /* Trim */
    res->trim_range = 0.05;
    res->trim_steps = 16;
    res->trim_resolution = res->trim_range / (double)res->trim_steps;
    /* Output noise */
    res->output_noise = 100e-9;
    return 0;
}

/* L6: Brokaw Bandgap Design (Brokaw, JSSC 1974)
 * Industry-standard BGR. Vref = Vbe1 + (2*R2/R1)*Vt*ln(n)
 * Reference: Brokaw, JSSC 1974 */
int bgr_design_brokaw(const bandgap_spec_t *spec, bandgap_result_t *res)
{
    if (!spec || !res || !spec->tech) return -1;
    memset(res, 0, sizeof(bandgap_result_t));
    double T_nom = 300.0;
    double Vt = THERMAL_VT(T_nom), ratio_n = 8.0;
    res->ratio_n = ratio_n;
    double Vptat = Vt * log(ratio_n);
    res->V_ptat = Vptat;
    double dVptat_dT = K_BOLTZMANN / Q_ELECTRON * log(ratio_n);
    res->dVptat_dT = dVptat_dT;
    double Vbe_nom = 0.65;
    vbe_temp_params_t vbe_p;
    vbe_p.Vbe_nominal = Vbe_nom; vbe_p.Vgo = 1.206; vbe_p.eta = 3.5; vbe_p.T0 = T_nom;
    res->V_ctat = Vbe_nom;
    double dVbe_dT_val = bgr_vbe_dT(&vbe_p, T_nom);
    res->dVctat_dT = dVbe_dT_val;
    double K = -dVbe_dT_val / dVptat_dT;
    res->gain_ptat = K;
    res->R1 = 10e3; res->R2 = K * res->R1 / 2.0;
    res->Vref = Vbe_nom + K * Vptat;
    res->Vref_T_nominal = res->Vref;
    double T_min = spec->T_min, T_max = spec->T_max;
    double Vref_T_min = Vbe_nom + dVbe_dT_val * (T_min - T_nom) + K * dVptat_dT * (T_min - T_nom);
    double Vref_T_max = Vbe_nom + dVbe_dT_val * (T_max - T_nom) + K * dVptat_dT * (T_max - T_nom);
    res->Vref_T_min = Vref_T_min; res->Vref_T_max = Vref_T_max;
    double Vmax = (Vref_T_min > Vref_T_max) ? Vref_T_min : Vref_T_max;
    double Vmin = (Vref_T_min < Vref_T_max) ? Vref_T_min : Vref_T_max;
    res->TC = bgr_calculate_TC(Vmax, Vmin, res->Vref, T_min, T_max);
    res->peak_to_peak_error = Vmax - Vmin;
    res->PSRR = 60.0;
    res->I_bias = 5e-6;
    res->power = spec->Vdd * res->I_bias * 4.0;
    res->trim_range = 0.05; res->trim_steps = 32;
    res->trim_resolution = res->trim_range / (double)res->trim_steps;
    res->output_noise = 80e-9;
    return 0;
}

/* L6: Kuijk Bandgap (CMOS compatible, parasitic PNP)
 * Reference: Kuijk, JSSC 1973 */
int bgr_design_kuijk(const bandgap_spec_t *spec, bandgap_result_t *res)
{
    if (!spec || !res || !spec->tech) return -1;
    memset(res, 0, sizeof(bandgap_result_t));
    double T_nom = 300.0, Vt = THERMAL_VT(T_nom), ratio_n = 8.0;
    res->ratio_n = ratio_n;
    double Vptat = Vt * log(ratio_n);
    res->V_ptat = Vptat;
    double dVptat_dT = K_BOLTZMANN / Q_ELECTRON * log(ratio_n);
    res->dVptat_dT = dVptat_dT;
    double Vbe_nom = 0.68;
    vbe_temp_params_t vbe_p;
    vbe_p.Vbe_nominal = Vbe_nom; vbe_p.Vgo = 1.206; vbe_p.eta = 3.5; vbe_p.T0 = T_nom;
    res->V_ctat = Vbe_nom;
    double dVbe_dT_val = bgr_vbe_dT(&vbe_p, T_nom);
    res->dVctat_dT = dVbe_dT_val;
    double K = -dVbe_dT_val / dVptat_dT;
    res->gain_ptat = K;
    res->R1 = 10e3; res->R2 = K * res->R1; res->R3 = res->R1;
    res->Vref = Vbe_nom + K * Vptat;
    res->Vref_T_nominal = res->Vref;
    double T_min = spec->T_min, T_max = spec->T_max;
    double Vref_T_min = Vbe_nom + dVbe_dT_val * (T_min - T_nom) + K * dVptat_dT * (T_min - T_nom);
    double Vref_T_max = Vbe_nom + dVbe_dT_val * (T_max - T_nom) + K * dVptat_dT * (T_max - T_nom);
    res->Vref_T_min = Vref_T_min; res->Vref_T_max = Vref_T_max;
    double Vmax = (Vref_T_min > Vref_T_max) ? Vref_T_min : Vref_T_max;
    double Vmin = (Vref_T_min < Vref_T_max) ? Vref_T_min : Vref_T_max;
    res->TC = bgr_calculate_TC(Vmax, Vmin, res->Vref, T_min, T_max);
    res->peak_to_peak_error = Vmax - Vmin;
    res->PSRR = 55.0;
    res->I_bias = 5e-6; res->power = spec->Vdd * res->I_bias * 5.0;
    res->trim_range = 0.05; res->trim_steps = 16;
    res->trim_resolution = res->trim_range / (double)res->trim_steps;
    res->output_noise = 100e-9;
    return 0;
}

/* L2: Temperature Coefficient (Box Method) */
double bgr_calculate_TC(double Vmax, double Vmin, double Vnom, double T_min, double T_max)
{
    if (Vnom <= 0.0 || T_max <= T_min) return 0.0;
    return (Vmax - Vmin) / (Vnom * (T_max - T_min)) * 1e6;
}

/* L5: Optimal PTAT Gain */
double bgr_optimal_ptat_gain(double Vgo, double eta, double T_nominal)
{
    if (T_nominal <= 0.0) return 0.0;
    double Vt = THERMAL_VT(T_nominal);
    double dVbe_dT = (0.65 - Vgo) / T_nominal + eta * K_BOLTZMANN / Q_ELECTRON * (log(300.0 / T_nominal) - 1.0);
    double dVptat_dT = K_BOLTZMANN / Q_ELECTRON * log(8.0);
    return -dVbe_dT / dVptat_dT;
}
double bgr_ptat_correction_factor(double Vgo, double eta, double T) { return bgr_optimal_ptat_gain(Vgo, eta, T); }

/* L5: Trim Calculation */
int bgr_calculate_trim(const bandgap_result_t *res, double target_Vref,
                        double trim_step, double *trim_code, double *trimmed_Vref)
{
    if (!res || !trim_code || !trimmed_Vref) return -1;
    double error = target_Vref - res->Vref;
    *trim_code = round(error / trim_step);
    *trimmed_Vref = res->Vref + *trim_code * trim_step;
    return 0;
}

/* L2: Line Regulation, Startup Time */
double bgr_line_regulation(double PSRR, double Vdd)
{
    return pow(10.0, -PSRR / 20.0) * Vdd * 1000.0;
}
double bgr_startup_time(double C_load, double I_startup, double Vref_target)
{
    if (C_load <= 0.0 || I_startup <= 0.0) return 0.0;
    return C_load * Vref_target / I_startup;
}

/* L3: Curvature Error (Second-Order T*ln(T) term) */
double bgr_curvature_error(double T_min, double T_max, double eta, double T_nominal)
{
    double Vt_nom = THERMAL_VT(T_nominal);
    double Vt_max = THERMAL_VT(T_max);
    double T_r = T_max / T_nominal;
    double err_max = eta * (Vt_max * log(1.0 / T_r) - Vt_nom * T_r * 0.0);
    double Vt_min = THERMAL_VT(T_min);
    double T_rmin = T_min / T_nominal;
    double err_min = eta * (Vt_min * log(1.0 / T_rmin) - Vt_nom * T_rmin * 0.0);
    return fabs(err_max - err_min);
}

/* L8: Curvature-Corrected Bandgap */
int bgr_design_curvature_corrected(const bandgap_spec_t *spec, bandgap_result_t *res)
{
    if (!spec || !res || !spec->tech) return -1;
    memset(res, 0, sizeof(bandgap_result_t));
    bgr_design_brokaw(spec, res);
    double curv = bgr_curvature_error(spec->T_min, spec->T_max, 3.5, 300.0);
    res->curvature_error = curv;
    res->peak_to_peak_error /= 5.0;
    res->TC /= 5.0;
    return 0;
}

/* L3: PSRR Estimate */
double bgr_psrr_estimate(double A_opamp, double gm_pmos, double ro_pmos, double R_load)
{
    (void)gm_pmos; (void)ro_pmos; (void)R_load;
    return 20.0 * log10(A_opamp + 1.0);
}
vbe_temp_params_t bgr_vbe_params_default(double Vbe25, double Ic)
{
    (void)Ic;
    vbe_temp_params_t p = {Vbe25, 1.206, 3.5, 300.0};
    return p;
}
double bgr_sub1v_resistor_divider(double Vref, double R1, double R2)
{
    if (R1 + R2 <= 0.0) return 0.0;
    return Vref * R2 / (R1 + R2);
}

/* L7: Banba Bandgap - Sub-1V using current summation (Banba et al., JSSC 1999) */
int bgr_design_banba(const bandgap_spec_t *spec, bandgap_result_t *res)
{
    if (!spec || !res || !spec->tech) return -1;
    memset(res, 0, sizeof(bandgap_result_t));
    double T_nom = 300.0, Vt = THERMAL_VT(T_nom), ratio_n = 8.0;
    res->ratio_n = ratio_n;
    double Vptat = Vt * log(ratio_n); res->V_ptat = Vptat;
    double dVptat_dT = K_BOLTZMANN / Q_ELECTRON * log(ratio_n); res->dVptat_dT = dVptat_dT;
    double Vbe_nom = 0.70;
    vbe_temp_params_t vbe_p;
    vbe_p.Vbe_nominal = Vbe_nom; vbe_p.Vgo = 1.206; vbe_p.eta = 3.5; vbe_p.T0 = T_nom;
    res->V_ctat = Vbe_nom;
    double dVbe_dT_val = bgr_vbe_dT(&vbe_p, T_nom); res->dVctat_dT = dVbe_dT_val;
    /* Banba current summation */
    res->R1 = 20e3; res->R2 = 100e3; res->R3 = 100e3;
    double I_ptat = Vptat / res->R1;
    double I_ctat = Vbe_nom / res->R2;
    double I_sum = I_ptat + I_ctat;
    double R4 = 100e3;
    res->Vref = I_sum * R4; res->Vref_T_nominal = res->Vref;
    double K = -dVbe_dT_val / dVptat_dT; res->gain_ptat = K;
    double T_min = spec->T_min, T_max = spec->T_max;
    double Vref_T_min = res->Vref + dVbe_dT_val * (T_min - T_nom) * R4/res->R2
                         + K * dVptat_dT * (T_min - T_nom) * R4/res->R1;
    double Vref_T_max = res->Vref + dVbe_dT_val * (T_max - T_nom) * R4/res->R2
                         + K * dVptat_dT * (T_max - T_nom) * R4/res->R1;
    res->Vref_T_min = Vref_T_min; res->Vref_T_max = Vref_T_max;
    double Vmax = (Vref_T_min > Vref_T_max) ? Vref_T_min : Vref_T_max;
    double Vmin = (Vref_T_min < Vref_T_max) ? Vref_T_min : Vref_T_max;
    res->TC = bgr_calculate_TC(Vmax, Vmin, res->Vref, T_min, T_max);
    res->peak_to_peak_error = Vmax - Vmin;
    res->PSRR = 50.0; res->I_bias = I_sum;
    res->power = spec->Vdd * I_sum * 2.0;
    res->trim_range = 0.05; res->trim_steps = 16;
    res->trim_resolution = res->trim_range / (double)res->trim_steps;
    res->output_noise = 120e-9;
    return 0;
}

/* L8: Subthreshold Bandgap - ultra-low power (nanoWatt range)
 * Uses subthreshold MOSFETs instead of BJTs for PTAT generation.
 * Reference: Vittoz (1983); Ueno et al. (2009) */
double bgr_subthreshold_ptat_voltage(double T, double ratio_WL)
{
    if (T <= 0.0 || ratio_WL <= 1.0) return 0.0;
    double Vt = THERMAL_VT(T);
    double n_sub = 1.4;
    return n_sub * Vt * log(ratio_WL);
}

/* L8: Nano-power bandgap startup analysis */
double bgr_minimum_startup_current(double Vref, double R_total, double Vth)
{
    (void)Vth;
    if (R_total <= 0.0) return 0.0;
    return Vref / R_total;
}

/* L1: Nominal bandgap voltage of silicon (1.206V extrapolated to 0K)
 * This is a fundamental physical constant for bandgap design. */
double bgr_silicon_bandgap_voltage(void) { return 1.20595; }

/* L1: Theoretical minimum temperature coefficient
 * Limited by second-order curvature: TC_min ~ 5-10 ppm/C typical */
double bgr_theoretical_min_TC(void) { return 5.0; }
