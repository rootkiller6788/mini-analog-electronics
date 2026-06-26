/*
 * analog_ic_current_mirror.c - Current Mirror Circuits
 * Each function implements one distinct mirror topology or analysis.
 * Textbook: Razavi Ch.5; Gray & Meyer Ch.4; Baker Ch.20
 */
#include "analog_ic_current_mirror.h"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* L2: Simple Current Mirror Design
 * Iout = Iref * (W/L)_out / (W/L)_in
 * Rout = 1/(lambda*Iout), Vout_min = Vdsat
 * Reference: Razavi Ch.5.2 */
int cm_design_simple(const cm_spec_t *spec, cm_result_t *res)
{
    if (!spec || !res || !spec->tech) return -1;
    memset(res, 0, sizeof(cm_result_t));
    const ic_technology_t *tech = spec->tech;
    double I_ref = spec->I_ref;
    double Vov = 0.15;
    double mu = (spec->device_type == MOS_TYPE_NMOS) ? tech->mu_n : tech->mu_p;
    mu *= 1e-4;
    double Cox = tech->Cox;
    double WL_in = mos_sizing_wl(tech, spec->device_type, I_ref, Vov, 1e-6);
    res->W_in = 10e-6; res->L_in = res->W_in / WL_in;
    if (res->L_in < tech->L_min) { res->L_in = tech->L_min; res->W_in = WL_in * tech->L_min; }
    double I_out_target = spec->I_out_target;
    double WL_out = WL_in * spec->ratio;
    res->W_out = res->W_in * spec->ratio; res->L_out = res->L_in;
    res->I_out = I_ref * spec->ratio;
    res->ratio_actual = res->I_out / I_ref;
    double lambda = (spec->device_type == MOS_TYPE_NMOS) ? tech->lambda_n : tech->lambda_p;
    res->Rout = 1.0 / (lambda * res->I_out + 1e-15);
    res->Vout_min_actual = Vov;
    res->Vdsat_in = Vov; res->Vdsat_out = Vov;
    double Vds_in = Vov;
    double Vds_out = spec->Vout_min;
    res->systematic_error = 100.0 * lambda * (Vds_out - Vds_in);
    res->power = spec->Vdd * I_ref * (1.0 + spec->ratio);
    return 0;
}

/* L2: Cascode Current Mirror
 * Rout = gm_casc * ro_casc * ro_in (much higher than simple)
 * Vout_min = Vdsat_in + Vdsat_casc
 * Reference: Razavi Ch.5.3.1 */
int cm_design_cascode(const cm_spec_t *spec, cm_result_t *res)
{
    if (!spec || !res || !spec->tech) return -1;
    memset(res, 0, sizeof(cm_result_t));
    const ic_technology_t *tech = spec->tech;
    double I_ref = spec->I_ref;
    double Vov = 0.15;
    double WL_in = mos_sizing_wl(tech, spec->device_type, I_ref, Vov, 1e-6);
    res->W_in = 10e-6; res->L_in = res->W_in / WL_in;
    if (res->L_in < tech->L_min) { res->L_in = tech->L_min; res->W_in = WL_in * tech->L_min; }
    res->W_out = res->W_in * spec->ratio; res->L_out = res->L_in;
    res->W_casc = res->W_out; res->L_casc = res->L_out;
    res->I_out = I_ref * spec->ratio;
    res->ratio_actual = res->I_out / I_ref;
    double lambda = (spec->device_type == MOS_TYPE_NMOS) ? tech->lambda_n : tech->lambda_p;
    double gds_in = lambda * I_ref;
    double gds_casc = lambda * res->I_out;
    double ro_casc = 1.0 / (gds_casc + 1e-15);
    double ro_in = 1.0 / (gds_in + 1e-15);
    double gm_casc = 2.0 * res->I_out / Vov;
    res->Rout = gm_casc * ro_casc * ro_in;
    res->Vout_min_actual = Vov + Vov;
    res->Vdsat_in = Vov; res->Vdsat_out = Vov;
    res->systematic_error = 0.5;
    res->power = spec->Vdd * I_ref * (1.0 + spec->ratio);
    res->Vb_casc = Vov + ((spec->device_type == MOS_TYPE_NMOS) ? tech->Vth_n : tech->Vth_p);
    return 0;
}

/* L2: Wilson Current Mirror (BJT or MOS)
 * BJT: Iout = Iref * (1 - 2/beta); Rout = beta*ro/2 (very high)
 * MOS: Rout = gm3*ro3*ro2 (similar to cascode)
 * Reference: Wilson (1968); Gray & Meyer Ch.4.4 */
int cm_design_wilson(const cm_spec_t *spec, cm_result_t *res)
{
    if (!spec || !res || !spec->tech) return -1;
    memset(res, 0, sizeof(cm_result_t));
    const ic_technology_t *tech = spec->tech;
    double I_ref = spec->I_ref;
    double Vov = 0.15;
    double WL_in = mos_sizing_wl(tech, spec->device_type, I_ref, Vov, 1e-6);
    res->W_in = 10e-6; res->L_in = res->W_in / WL_in;
    if (res->L_in < tech->L_min) { res->L_in = tech->L_min; res->W_in = WL_in * tech->L_min; }
    res->W_out = res->W_in * spec->ratio; res->L_out = res->L_in;
    res->W_casc = res->W_out; res->L_casc = res->L_out;
    double I_out = I_ref * spec->ratio;
    res->I_out = I_out;
    res->ratio_actual = I_out / I_ref;
    double lambda = (spec->device_type == MOS_TYPE_NMOS) ? tech->lambda_n : tech->lambda_p;
    double gm3 = 2.0 * I_out / Vov;
    double ro3 = 1.0 / (lambda * I_out + 1e-15);
    double ro2 = ro3;
    res->Rout = gm3 * ro3 * ro2;
    res->Vout_min_actual = 2.0 * Vov;
    res->systematic_error = 0.3;
    res->power = spec->Vdd * I_ref * (1.0 + spec->ratio);
    return 0;
}

/* L2: Output resistance of simple current mirror */
double cm_output_resistance_simple(double gds, double gm, double gmb)
{
    (void)gm; (void)gmb;
    return (gds > 1e-15) ? 1.0 / gds : 1e12;
}

/* L3: Output resistance of cascode current mirror
 * Rout = gm_casc/(gds_in*gds_casc) approx */
double cm_output_resistance_cascode(double gm_casc, double gds_casc, double gds_in, double gmb_casc)
{
    (void)gmb_casc;
    double denom = gds_in * gds_casc;
    return (denom > 1e-30) ? gm_casc / denom : 1e12;
}

/* L3: Output resistance of Wilson current mirror */
double cm_output_resistance_wilson(double gm1, double gds1, double gm2, double gds2, double gds3)
{
    double prod1 = gm1 * gm2;
    double prod2 = gds1 * gds2 * gds3;
    (void)prod2;
    double Rout = prod1 / (gds1 * gds2 + 1e-30);
    (void)gds3;
    return (Rout > 0.0) ? Rout : 1e12;
}

/* L3: Output resistance of regulated (gain-boosted) cascode */
double cm_output_resistance_regulated(double gm_boost, double gm_main, double gds_main, double gds_load)
{
    double A_boost = gm_boost / (gds_main + 1e-15);
    double Rout_main = gm_main / (gds_main * gds_load + 1e-30);
    return A_boost * Rout_main;
}

/* L2: Regulated (gain-boosted) cascode current mirror
 * Adds an auxiliary amplifier to boost output impedance.
 * Rout = A_boost * gm*casc * ro_casc * ro_in (extremely high)
 * Reference: Bult & Geelen (1992); Razavi Ch.5.3.3 */
int cm_design_regulated(const cm_spec_t *spec, cm_result_t *res)
{
    if (!spec || !res || !spec->tech) return -1;
    memset(res, 0, sizeof(cm_result_t));
    const ic_technology_t *tech = spec->tech;
    double I_ref = spec->I_ref, Vov = 0.15;
    double WL_in = mos_sizing_wl(tech, spec->device_type, I_ref, Vov, 1e-6);
    res->W_in = 10e-6; res->L_in = res->W_in / WL_in;
    if (res->L_in < tech->L_min) { res->L_in = tech->L_min; res->W_in = WL_in * tech->L_min; }
    res->W_out = res->W_in * spec->ratio; res->L_out = res->L_in;
    res->W_casc = res->W_out; res->L_casc = res->L_out;
    res->I_out = I_ref * spec->ratio;
    double lambda = (spec->device_type == MOS_TYPE_NMOS) ? tech->lambda_n : tech->lambda_p;
    double gm_main = 2.0 * res->I_out / Vov;
    double gm_boost = gm_main * 5.0;
    double gds_main = lambda * res->I_out;
    double gds_load = gds_main;
    res->Rout = cm_output_resistance_regulated(gm_boost, gm_main, gds_main, gds_load);
    res->Vout_min_actual = Vov + Vov;
    res->power = spec->Vdd * I_ref * (1.0 + spec->ratio) * 1.2;
    return 0;
}

/* L2: Wide-Swing Cascode Current Mirror (Sooch design)
 * Uses a separate bias network to keep cascode at Vdsat edge.
 * Vout_min = 2*Vdsat (lowest for cascode mirrors)
 * Reference: Sooch (1985); Babanezhad & Gregorian (1987); Razavi Ch.5.3.2 */
int cm_design_wide_swing(const cm_spec_t *spec, cm_result_t *res)
{
    if (!spec || !res || !spec->tech) return -1;
    memset(res, 0, sizeof(cm_result_t));
    const ic_technology_t *tech = spec->tech;
    double I_ref = spec->I_ref, Vov = 0.15;
    double WL_in = mos_sizing_wl(tech, spec->device_type, I_ref, Vov, 1e-6);
    res->W_in = 10e-6; res->L_in = res->W_in / WL_in;
    if (res->L_in < tech->L_min) { res->L_in = tech->L_min; res->W_in = WL_in * tech->L_min; }
    res->W_out = res->W_in * spec->ratio; res->L_out = res->L_in;
    res->W_casc = res->W_out * 0.25; res->L_casc = res->L_out;
    res->I_out = I_ref * spec->ratio;
    res->ratio_actual = res->I_out / I_ref;
    double lambda = (spec->device_type == MOS_TYPE_NMOS) ? tech->lambda_n : tech->lambda_p;
    double gm_casc = 2.0 * res->I_out / Vov;
    double ro_casc = 1.0 / (lambda * res->I_out + 1e-15);
    double ro_in = 1.0 / (lambda * I_ref + 1e-15);
    res->Rout = gm_casc * ro_casc * ro_in;
    res->Vout_min_actual = 2.0 * Vov;
    res->Vdsat_in = Vov; res->Vdsat_out = Vov;
    res->power = spec->Vdd * I_ref * (1.0 + spec->ratio);
    res->Vb_casc = 2.0 * Vov + ((spec->device_type == MOS_TYPE_NMOS) ? tech->Vth_n : tech->Vth_p);
    return 0;
}

/* L2: Low-Voltage Cascode */
int cm_design_low_voltage(const cm_spec_t *spec, cm_result_t *res)
{
    if (!spec || !res || !spec->tech) return -1;
    memset(res, 0, sizeof(cm_result_t));
    const ic_technology_t *tech = spec->tech;
    double I_ref = spec->I_ref, Vov = 0.15;
    double WL_in = mos_sizing_wl(tech, spec->device_type, I_ref, Vov, 1e-6);
    res->W_in = 10e-6; res->L_in = res->W_in / WL_in;
    if (res->L_in < tech->L_min) { res->L_in = tech->L_min; res->W_in = WL_in * tech->L_min; }
    res->W_out = res->W_in * spec->ratio; res->L_out = res->L_in;
    res->W_casc = res->W_out; res->L_casc = res->L_out;
    res->I_out = I_ref * spec->ratio;
    double lambda = (spec->device_type == MOS_TYPE_NMOS) ? tech->lambda_n : tech->lambda_p;
    res->Rout = 10e6;
    res->Vout_min_actual = Vov;
    res->power = spec->Vdd * I_ref * (1.0 + spec->ratio);
    return 0;
}

/* L2: Compliance voltage calculations */
double cm_compliance_simple(double Vdsat) { return Vdsat; }
double cm_compliance_cascode(double Vdsat_in, double Vdsat_casc, double Vth) { (void)Vth; return Vdsat_in + Vdsat_casc; }
double cm_compliance_wide_swing(double Vdsat) { return 2.0 * Vdsat; }
double cm_compliance_low_voltage(double Vdsat_in, double Vdsat_casc) { return Vdsat_in + Vdsat_casc; }

/* L4: Current mirror mismatch - systematic from Vds difference */
double cm_mismatch_systematic(double I_ref, double Vds_in, double Vds_out, double lambda)
{
    if (I_ref <= 0.0) return 0.0;
    return 100.0 * lambda * (Vds_out - Vds_in);
}

/* L4: Random mismatch sigma from Pelgrom model */
double cm_mismatch_random_sigma(const mismatch_params_t *mp, double W1, double L1, double W2, double L2, double gm_Id)
{
    if (!mp) return 0.0;
    double area1 = W1 * L1, area2 = W2 * L2;
    double sigma_vth_sq = (mp->Avt * 1e-3) * (mp->Avt * 1e-3) * (1.0/area1 + 1.0/area2) * 1e-12;
    double sigma_beta_sq = (mp->Abeta * 1e-2) * (mp->Abeta * 1e-2) * (1.0/area1 + 1.0/area2) * 1e-12;
    return sqrt(sigma_vth_sq * gm_Id * gm_Id + sigma_beta_sq) * 100.0;
}

/* L4: Total mismatch at given confidence level */
double cm_mismatch_total(double systematic, double random_sigma, double confidence)
{
    (void)confidence;
    return fabs(systematic) + 3.0 * fabs(random_sigma);
}

/* L2: Generate cascode bias voltages */
int cm_generate_cascode_bias(const ic_technology_t *tech, double I_bias, double Vdd, double T, cascode_bias_t *bias)
{
    if (!tech || !bias || I_bias <= 0.0) return -1;
    (void)Vdd; (void)T;
    double Vov = 0.15;
    double Vth = tech->Vth_n;
    bias->Vb = Vth + 2.0 * Vov;
    bias->I_bias = I_bias;
    bias->W_bias = 5e-6;
    bias->L_bias = 2e-6;
    return 0;
}

/* L5: Optimal sizing for minimum current mismatch */
int cm_optimal_sizing_min_mismatch(const cm_spec_t *spec, double target_sigma, double *W, double *L)
{
    if (!spec || !spec->tech || !W || !L || target_sigma <= 0.0) return -1;
    const ic_technology_t *tech = spec->tech;
    double Avt = tech->Avt * 1e-3;
    double area_min = (Avt * Avt) / (target_sigma * target_sigma);
    *L = spec->device_type == MOS_TYPE_NMOS ? tech->L_min * 3.0 : tech->L_min * 3.0;
    *W = area_min / *L;
    if (*W < tech->W_min) *W = tech->W_min;
    return 0;
}

/* L2: Current mirror bandwidth */
double cm_bandwidth_simple(double gm, double Cgs, double Cgd, double Cdb)
{
    double C_total = Cgs + Cgd + Cdb;
    return (C_total > 1e-18) ? gm / (2.0 * M_PI * C_total) : 0.0;
}

/* L2: Current transfer error due to finite Rout */
double cm_current_transfer_error(double Rout, double Rload)
{
    if (Rout <= 0.0 || Rload <= 0.0) return 0.0;
    return 100.0 / (1.0 + Rout / Rload);
}

/* L3: BJT current mirror - output resistance */
double cm_bjt_output_resistance(double VA, double Ic) { return (Ic > 1e-15) ? VA / Ic : 1e12; }

/* L3: BJT current mirror - error from finite beta */
double cm_bjt_current_error(double beta_f, double ratio) { return 100.0 * (ratio + 1.0) / beta_f; }

/* L3: BJT Wilson mirror error */
double cm_bjt_wilson_error(double beta_f)
{
    double b2 = beta_f * beta_f;
    return (b2 > 1.0) ? 100.0 * 2.0 / b2 : 100.0;
}

/* L7: Power supply rejection of current mirrors */
double cm_psr_simple(double gds, double Rout, double Vdd)
{
    (void)Vdd;
    return 20.0 * log10(gds * Rout + 1e-15);
}
double cm_psr_cascode(double gds_casc, double Rout, double gm_casc, double Vdd)
{
    (void)Vdd;
    return 20.0 * log10(gds_casc * Rout / (gm_casc + 1e-15) + 1e-15);
}
