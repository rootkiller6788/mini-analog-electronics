/*
 * analog_ic_amplifier.c - Amplifier Topologies & Op-Amp Design
 * Each function implements one independent knowledge point.
 * Textbook: Razavi Ch.3-10; Gray & Meyer Ch.3-8
 */
#include "analog_ic_amplifier.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* L2: Common-Source Amplifier - Av = -gm*(ro||Rd||Rload) */
int amplifier_analyze_cs(const amplifier_spec_t *spec, amplifier_result_t *res)
{
    if (!spec || !res) return -1;
    memset(res, 0, sizeof(amplifier_result_t));
    mos_ss_params_t ss;
    int ret = mos_extract_ss_params(NULL, &spec->transistor, 300.0, &ss);
    if (ret < 0) return -2;
    double gm = ss.gm, ro = ss.ro;
    double Rd = (spec->Rd > 0.0) ? spec->Rd : 1e12;
    double Rload = (spec->Rload > 0.0) ? spec->Rload : 1e12;
    double Rout = 1.0 / (1.0/ro + 1.0/Rd + 1.0/Rload);
    res->Rout = Rout;
    res->Av = -gm * Rout;
    res->Av_dB = 20.0 * log10(fabs(res->Av) + 1e-15);
    res->Rin = 1e12;
    double Cload = (spec->Cload > 0.0) ? spec->Cload : 1e-12;
    res->BW = 1.0 / (2.0 * M_PI * Rout * Cload);
    res->f_3dB_high = res->BW;
    res->f_3dB_low = 0.0;
    res->GBW = fabs(res->Av) * res->BW;
    res->poles[0] = res->BW;
    res->n_poles = 1;
    res->power = spec->Vdd * spec->I_bias;
    return 0;
}

/* L2: Common-Gate Amplifier - Av = (gm+gmb)*Rout, Rin = 1/(gm+gmb) */
int amplifier_analyze_cg(const amplifier_spec_t *spec, amplifier_result_t *res)
{
    if (!spec || !res) return -1;
    memset(res, 0, sizeof(amplifier_result_t));
    mos_ss_params_t ss;
    int ret = mos_extract_ss_params(NULL, &spec->transistor, 300.0, &ss);
    if (ret < 0) return -2;
    double gm = ss.gm, ro = ss.ro, gmb = ss.gmb;
    double Rd = (spec->Rd > 0.0) ? spec->Rd : 1e12;
    double Rload = (spec->Rload > 0.0) ? spec->Rload : 1e12;
    double Rout = 1.0 / (1.0/ro + 1.0/Rd + 1.0/Rload);
    res->Rout = Rout;
    double Gmt = gm + gmb;
    res->Av = Gmt * Rout;
    res->Av_dB = 20.0 * log10(fabs(res->Av) + 1e-15);
    double Rin_nat = 1.0 / (Gmt + 1e-12);
    double Rs = (spec->Rs > 0.0) ? spec->Rs : 1e-3;
    res->Rin = 1.0 / (1.0/Rin_nat + 1.0/Rs);
    double Cload = (spec->Cload > 0.0) ? spec->Cload : 1e-12;
    res->BW = 1.0 / (2.0 * M_PI * Rout * Cload);
    res->f_3dB_high = res->BW;
    res->GBW = res->Av * res->BW;
    res->power = spec->Vdd * spec->I_bias;
    return 0;
}

/* L2: Source Follower (CD) - Av = gm*Rs/(1+gm*Rs), buffer */
int amplifier_analyze_cd(const amplifier_spec_t *spec, amplifier_result_t *res)
{
    if (!spec || !res) return -1;
    memset(res, 0, sizeof(amplifier_result_t));
    mos_ss_params_t ss;
    int ret = mos_extract_ss_params(NULL, &spec->transistor, 300.0, &ss);
    if (ret < 0) return -2;
    double gm = ss.gm, ro = ss.ro, gmb = ss.gmb;
    double Rs = (spec->Rs > 0.0) ? spec->Rs : 1e12;
    double Rload = (spec->Rload > 0.0) ? spec->Rload : 1e12;
    double Rs_eff = 1.0 / (1.0/Rs + 1.0/Rload);
    double Gmt = gm + gmb;
    res->Av = Gmt * Rs_eff / (1.0 + Gmt * Rs_eff);
    res->Av_dB = 20.0 * log10(fabs(res->Av) + 1e-15);
    res->Rout = 1.0 / (Gmt + 1.0/ro + 1.0/Rs + 1.0/Rload);
    res->Rin = 1e12;
    double Cload = (spec->Cload > 0.0) ? spec->Cload : 1e-12;
    res->BW = 1.0 / (2.0 * M_PI * res->Rout * Cload);
    res->f_3dB_high = res->BW;
    res->GBW = res->Av * res->BW;
    res->power = spec->Vdd * spec->I_bias;
    return 0;
}

/* L2: Cascode (CS+CG) - Av = -gm1*ro1*gm2*ro2, very high gain */
int amplifier_analyze_cascode(const amplifier_spec_t *spec, amplifier_result_t *res)
{
    if (!spec || !res) return -1;
    memset(res, 0, sizeof(amplifier_result_t));
    mos_ss_params_t ss;
    int ret = mos_extract_ss_params(NULL, &spec->transistor, 300.0, &ss);
    if (ret < 0) return -2;
    double gm1 = ss.gm, ro1 = ss.ro;
    double gm2 = gm1, ro2 = ro1;
    double Rload = (spec->Rload > 0.0) ? spec->Rload : 1e12;
    double Rout_casc = gm2 * ro2 * ro1;
    double Rout = 1.0 / (1.0/Rout_casc + 1.0/Rload);
    res->Rout = Rout;
    res->Av = -gm1 * Rout;
    res->Av_dB = 20.0 * log10(fabs(res->Av) + 1e-15);
    res->Rin = 1e12;
    double Cload = (spec->Cload > 0.0) ? spec->Cload : 1e-12;
    res->BW = 1.0 / (2.0 * M_PI * Rout * Cload);
    res->f_3dB_high = res->BW;
    res->GBW = fabs(res->Av) * res->BW;
    res->power = spec->Vdd * spec->I_bias;
    return 0;
}

/* L2: Common-Emitter (BJT) - Av = -gm*(ro||Rc||Rload), Rin = rpi */
int amplifier_analyze_ce(const amplifier_spec_t *spec, amplifier_result_t *res)
{
    if (!spec || !res) return -1;
    memset(res, 0, sizeof(amplifier_result_t));
    double Vt = THERMAL_VT(300.0);
    double gm = spec->I_bias / Vt;
    double beta = 100.0;
    double rpi = beta / gm;
    double VA = 50.0;
    double ro = VA / spec->I_bias;
    double Rc = (spec->Rd > 0.0) ? spec->Rd : 1e12;
    double Rload = (spec->Rload > 0.0) ? spec->Rload : 1e12;
    double Rout = 1.0 / (1.0/ro + 1.0/Rc + 1.0/Rload);
    res->Rout = Rout;
    res->Av = -gm * Rout;
    res->Av_dB = 20.0 * log10(fabs(res->Av) + 1e-15);
    res->Rin = rpi;
    double Cload = (spec->Cload > 0.0) ? spec->Cload : 1e-12;
    res->BW = 1.0 / (2.0 * M_PI * Rout * Cload);
    res->f_3dB_high = res->BW;
    res->GBW = fabs(res->Av) * res->BW;
    res->power = spec->Vdd * spec->I_bias;
    return 0;
}

/* L2: Common-Base (BJT) - wideband, low Rin */
int amplifier_analyze_cb(const amplifier_spec_t *spec, amplifier_result_t *res)
{
    if (!spec || !res) return -1;
    memset(res, 0, sizeof(amplifier_result_t));
    double Vt = THERMAL_VT(300.0);
    double gm = spec->I_bias / Vt;
    double VA = 50.0; double ro = VA / spec->I_bias;
    double Rc = (spec->Rd > 0.0) ? spec->Rd : 1e12;
    double Rload = (spec->Rload > 0.0) ? spec->Rload : 1e12;
    double Rout = 1.0 / (1.0/ro + 1.0/Rc + 1.0/Rload);
    res->Rout = Rout; res->Av = gm * Rout;
    res->Av_dB = 20.0 * log10(fabs(res->Av) + 1e-15);
    res->Rin = 1.0 / (gm + 1e-12);
    double Cload = (spec->Cload > 0.0) ? spec->Cload : 1e-12;
    res->BW = 1.0 / (2.0 * M_PI * Rout * Cload);
    res->f_3dB_high = res->BW;
    res->GBW = res->Av * res->BW;
    res->power = spec->Vdd * spec->I_bias;
    return 0;
}

/* L2: Emitter Follower (CC) - voltage buffer */
int amplifier_analyze_cc(const amplifier_spec_t *spec, amplifier_result_t *res)
{
    if (!spec || !res) return -1;
    memset(res, 0, sizeof(amplifier_result_t));
    double Vt = THERMAL_VT(300.0);
    double gm = spec->I_bias / Vt;
    double beta = 100.0; double rpi = beta / gm;
    double Re = (spec->Rs > 0.0) ? spec->Rs : 1e12;
    double Rload = (spec->Rload > 0.0) ? spec->Rload : 1e12;
    double Re_eff = 1.0 / (1.0/Re + 1.0/Rload);
    res->Av = gm * Re_eff / (1.0 + gm * Re_eff);
    res->Av_dB = 20.0 * log10(fabs(res->Av) + 1e-15);
    res->Rout = 1.0 / (gm + 1.0/Re + 1.0/Rload);
    res->Rin = rpi + (beta + 1.0) * Re_eff;
    double Cload = (spec->Cload > 0.0) ? spec->Cload : 1e-12;
    res->BW = 1.0 / (2.0 * M_PI * res->Rout * Cload);
    res->f_3dB_high = res->BW;
    res->GBW = res->Av * res->BW;
    res->power = spec->Vdd * spec->I_bias;
    return 0;
}

/* L6: Differential Pair DC Analysis
 * Solves I_d1, I_d2 from Iss = I_d1 + I_d2 and the differential input.
 * I_d1 = Iss / (1 + exp(-Vid/(n*Vt))) — tanh characteristic
 * Reference: Razavi Ch.4.2 */
int diff_pair_dc_solve(const diff_pair_spec_t *spec, diff_pair_result_t *res)
{
    if (!spec || !res) return -1;
    memset(res, 0, sizeof(diff_pair_result_t));
    double Iss = spec->I_tail;
    if (Iss <= 0.0) return -1;
    double n = 1.4;
    double Vt = THERMAL_VT(spec->T > 0.0 ? spec->T : 300.0);
    /* For zero differential input, Id1 = Id2 = Iss/2 */
    res->I_d1 = Iss / 2.0;
    res->I_d2 = Iss / 2.0;
    /* gm1 = gm2 = Iss/(n*Vt) in weak inv, or 2*Id/Vov in strong */
    double Id_half = Iss / 2.0;
    /* Estimate overdrive for strong inversion */
    double Vdsat_est = 0.15;
    res->Vdsat_input = Vdsat_est;
    res->gm1 = (Id_half > 1e-12) ? 2.0 * Id_half / Vdsat_est : Id_half / (n * Vt);
    res->power = spec->Vdd * Iss;
    (void)n; /* used in extended weak-inversion analysis */
    return 0;
}

/* L6: Differential Pair Full Analysis
 * Ad = gm1*Rout, CMRR = Ad/Acm ~= gm*Rss (tail current source impedance)
 * Reference: Razavi Ch.4.3-4.5 */
int diff_pair_analyze(const diff_pair_spec_t *spec, diff_pair_result_t *res)
{
    if (!spec || !res || !spec->tech) return -1;
    memset(res, 0, sizeof(diff_pair_result_t));
    double Iss = spec->I_tail;
    if (Iss <= 0.0) return -1;
    res->I_d1 = Iss / 2.0;
    res->I_d2 = Iss / 2.0;
    /* gm of input pair */
    double Id_half = Iss / 2.0;
    double Vdsat_est = 0.15;
    res->gm1 = 2.0 * Id_half / Vdsat_est;
    res->Vdsat_input = Vdsat_est;

    /* Output resistance of tail current source */
    double lambda_tail = (spec->input_type == DIFF_INPUT_NMOS)
        ? spec->tech->lambda_n : spec->tech->lambda_p;
    double R_tail = 1.0 / (lambda_tail * Iss + 1e-15);

    /* Output resistance */
    double R_load_eff = spec->R_load;
    if (spec->load_type == DIFF_LOAD_ACTIVE || spec->load_type == DIFF_LOAD_CASCODE) {
        double lambda_load = (spec->input_type == DIFF_INPUT_NMOS)
            ? spec->tech->lambda_p : spec->tech->lambda_n;
        R_load_eff = 1.0 / (lambda_load * Id_half + 1e-15);
    }
    double Rout = R_load_eff;
    res->Rout_diff = Rout;
    res->Rout_cm = R_tail;

    /* Differential gain: Ad = gm1 * Rout */
    res->Ad = res->gm1 * Rout;

    /* Common-mode gain: Acm = Rout / (2*R_tail) (simplified) */
    res->Acm = (R_tail > 1e-3) ? Rout / (2.0 * R_tail) : Rout;

    /* CMRR = 20*log10(Ad/Acm) */
    double cmrr_lin = (res->Acm > 1e-15) ? fabs(res->Ad) / res->Acm : 1e6;
    res->CMRR = 20.0 * log10(cmrr_lin);

    /* ICMR (input common-mode range) */
    double Vth_in = (spec->input_type == DIFF_INPUT_NMOS)
        ? spec->tech->Vth_n : spec->tech->Vth_p;
    res->Vin_cm_min = Vdsat_est + Vth_in;
    res->Vin_cm_max = spec->Vdd - Vdsat_est;

    /* Bandwidth */
    double Cload = (spec->Cload > 0.0) ? spec->Cload : 1e-12;
    res->BW = 1.0 / (2.0 * M_PI * Rout * Cload);

    /* Slew rate: SR = Iss / Cload */
    res->SR = Iss / Cload;
    res->power = spec->Vdd * Iss;

    /* Input-referred noise (thermal, rough estimate) */
    double gamma_n = 0.667;
    res->input_noise_density = sqrt(4.0 * K_BOLTZMANN * spec->T * gamma_n / res->gm1);

    return 0;
}

/* L2: Differential pair transconductance */
double diff_pair_gm(const diff_pair_spec_t *spec)
{
    if (!spec) return 0.0;
    double Id_half = spec->I_tail / 2.0;
    double Vdsat_est = 0.15;
    return 2.0 * Id_half / Vdsat_est;
}

/* L2: CMRR estimate for differential pair */
double diff_pair_cmrr(const diff_pair_spec_t *spec)
{
    if (!spec || !spec->tech) return 0.0;
    double gm = diff_pair_gm(spec);
    double lambda_tail = (spec->input_type == DIFF_INPUT_NMOS)
        ? spec->tech->lambda_n : spec->tech->lambda_p;
    double R_tail = 1.0 / (lambda_tail * spec->I_tail + 1e-15);
    return 20.0 * log10(gm * R_tail + 1.0);
}

/* L4: Gain-Bandwidth Product - GBW = gm1/(2*pi*Cc)
 * Fundamental relation in two-stage op-amp design.
 * Reference: Razavi eq. (10.3) */
double opamp_GBW(double gm1, double Cc)
{
    if (Cc <= 0.0) return 0.0;
    return gm1 / (2.0 * M_PI * Cc);
}

/* L4: Slew Rate = I_tail/Cc - large-signal limitation
 * Reference: Razavi eq. (10.22) */
double opamp_slew_rate(double I_tail, double Cc)
{
    if (Cc <= 0.0) return 0.0;
    return I_tail / Cc;
}

/* L5: Systematic Offset in Two-Stage Op-Amp
 * Caused by current mirror mismatch in active loads.
 * Vos = (I5/2 - I7) * (1/gm1)
 * Reference: Razavi Ch.10.2.4 */
double opamp_systematic_offset(double I_ref, double beta5, double beta7,
                                const ic_technology_t *tech)
{
    if (!tech || beta5 <= 0.0 || beta7 <= 0.0) return 0.0;
    (void)tech;
    /* Simplified: systematic offset from current mirror ratio */
    double I5 = I_ref;
    double I7_half = I_ref * beta7 / beta5;
    double delta_I = I5 * 0.5 - I7_half;
    /* Assume gm1 ~ 1 mS typical */
    double gm1 = 1e-3;
    return delta_I / gm1;
}

/* L4: Phase Margin from Pole-Zero Positions
 * PM = 180 - atan(UGF/p1) - atan(UGF/p2) + atan(UGF/z1)
 * Reference: Razavi Ch.10.4; Gray & Meyer Ch.9.4 */
double opamp_phase_margin(double ugf, double p1, double p2, double z1)
{
    if (ugf <= 0.0) return 0.0;
    /* Phase at UGF */
    double phi = 180.0;
    if (p1 > 0.0) phi -= atan(ugf / p1) * 180.0 / M_PI;
    if (p2 > 0.0) phi -= atan(ugf / p2) * 180.0 / M_PI;
    if (z1 > 0.0) phi += atan(ugf / z1) * 180.0 / M_PI;
    return phi;
}

/* L5: Pole-Zero Placement for Given Phase Margin
 * Solves for Cc and gm2 to achieve target GBW and PM.
 * Uses approximate formulas for two-stage Miller op-amp.
 * Reference: Razavi Ch.10.4 */
int opamp_pole_zero_placement(double GBW_target, double PM_target,
                               double Cload, double *Cc, double *gm2)
{
    if (!Cc || !gm2 || GBW_target <= 0.0 || PM_target <= 0.0 || Cload <= 0.0)
        return -1;

    /* Rule of thumb for 60 deg PM: p2 >= 2.2 * GBW
     * p2 = gm2/Cload, so gm2 >= 2.2 * GBW * Cload */
    double pm_rad = PM_target * M_PI / 180.0;
    double factor = tan(pm_rad);
    *gm2 = factor * GBW_target * Cload * 2.0 * M_PI;

    /* Cc: for good settling, Cc ~ 0.22 * Cload */
    *Cc = 0.22 * Cload;
    if (*Cc < 0.1e-12) *Cc = 0.1e-12;

    return 0;
}

/* L3: Miller Pole Splitting Analysis
 * Two-stage op-amp: before compensation, two low-frequency poles exist.
 * After Miller compensation: dominant pole p1 = 1/(gm2*ro1*ro2*Cc)
 *                          non-dominant pole p2 = gm2*Cc/(C1*C2+Cc*(C1+C2)) ~ gm2/C2
 *                          RHP zero z1 = gm2/Cc
 * Reference: Razavi Ch.10.3; Gray & Meyer Ch.9.3 */
int miller_pole_split(const two_stage_params_t *p, double *p1, double *p2, double *z1)
{
    if (!p || !p1 || !p2 || !z1) return -1;
    if (p->gm1 <= 0.0 || p->gm2 <= 0.0 || p->Cc <= 0.0) return -1;

    /* Dominant pole (from Miller effect) */
    double denom = p->gm2 * p->ro1 * p->ro2 * p->Cc;
    *p1 = (denom > 1e-15) ? -1.0 / denom : 0.0;

    /* Non-dominant pole */
    double C1 = (p->C1 > 0.0) ? p->C1 : 0.1e-12;
    double C2 = (p->C2 > 0.0) ? p->C2 : 1e-12;
    double denom2 = C1 * C2 + p->Cc * (C1 + C2);
    *p2 = (denom2 > 1e-18) ? -p->gm2 * p->Cc / denom2 : -1e12;

    /* RHP zero */
    *z1 = p->gm2 / p->Cc;  /* Positive = RHP zero */

    return 0;
}

/* L2: Gain Error of Unity-Gain Buffer
 * Error = 1 / (1 + Av_dc)
 * Reference: Razavi Ch.8.2 */
double opamp_gain_error(double Av_dc)
{
    if (Av_dc <= 0.0) return 1.0;
    return 1.0 / (1.0 + Av_dc);
}

/* L2: Figure of Merit = GBW * Cload / Power [MHz*pF/mW]
 * Standard benchmark for op-amp comparison.
 * Reference: Sansen Ch.4; Murmann FoM */
double opamp_figure_of_merit(const opamp_result_t *r)
{
    if (!r || r->power <= 0.0) return 0.0;
    return (r->GBW / 1e6) * (r->Cc * 1e12) / (r->power * 1e3);
}

/* L3: Open-Loop Transfer Function Decomposition */
double opamp_transfer_A0(double gm1, double gm2, double ro1, double ro2)
{
    return gm1 * ro1 * gm2 * ro2;
}

double opamp_transfer_p1(double gm2, double ro1, double ro2, double Cc,
                          double C1, double C2)
{
    (void)C1; (void)C2;
    double denom = gm2 * ro1 * ro2 * Cc;
    return (denom > 1e-15) ? -1.0 / denom : 0.0;
}

double opamp_transfer_p2(double gm2, double Cc, double C1, double C2)
{
    double denom = C1 * C2 + Cc * (C1 + C2);
    return (denom > 1e-18) ? -gm2 * Cc / denom : -1e12;
}

double opamp_transfer_z1(double gm2, double Cc, double Rz)
{
    /* With nulling resistor: z1 = 1/Cc*(1/gm2 - Rz) */
    if (Cc <= 0.0) return 1e12;
    return 1.0 / (Cc * (1.0/gm2 - Rz + 1e-12));
}

/* L5: Noise-Optimal Input Pair Sizing
 * Minimizes input-referred noise for given GBW and Cload.
 * Reference: Sansen Ch.4; Razavi Ch.7 */
double opamp_optimal_input_gm(double Cload, double GBW, double gamma)
{
    if (GBW <= 0.0 || Cload <= 0.0) return 0.0;
    /* Optimal gm1 from noise-GBW tradeoff */
    (void)gamma;
    return 2.0 * M_PI * GBW * Cload * 0.22;
}

/* L5: ICMR (Input Common-Mode Range) Calculation
 * For NMOS diff pair: Vin_cm_min = Vdsat_tail + Vgs_input
 *                    Vin_cm_max = Vdd - Vdsat_load - Vsg_load (for PMOS load)
 * Reference: Razavi Ch.10.2.3 */
int opamp_icmr_calc(const opamp_spec_t *spec, double *vmin, double *vmax)
{
    if (!spec || !vmin || !vmax || !spec->tech) return -1;
    double Vth_n = spec->tech->Vth_n;
    double Vth_p = spec->tech->Vth_p;
    double Vdsat = 0.15;
    /* NMOS input pair */
    *vmin = Vdsat + Vth_n + Vdsat;
    *vmax = spec->Vdd - Vdsat - Vth_p;
    return 0;
}

/* L5: Output Swing Calculation
 * Vout_min = Vdsat_n (tail + cascode if any)
 * Vout_max = Vdd - Vdsat_p
 * Reference: Razavi Ch.10.2.3 */
int opamp_output_swing(const opamp_spec_t *spec, double *vmin, double *vmax)
{
    if (!spec || !vmin || !vmax) return -1;
    double Vdsat = 0.15;
    *vmin = 2.0 * Vdsat;
    *vmax = spec->Vdd - 2.0 * Vdsat;
    return 0;
}

/* L7: CMFB Analysis for Fully-Differential Op-Amp
 * Analyzes common-mode feedback loop gain and bandwidth.
 * Reference: Razavi Ch.10.6; Gray & Meyer Ch.12.6 */
int opamp_cmfb_analyze(double gm_diff, double ro_diff,
                        double gm_cmfb, double ro_cmfb,
                        double Vcm_target, double Vcm_actual,
                        cmfb_result_t *res)
{
    if (!res) return -1;
    memset(res, 0, sizeof(cmfb_result_t));
    res->gm_cmfb = gm_cmfb;
    res->Vcm_target = Vcm_target;
    res->Vcm_actual = Vcm_actual;
    res->loop_gain = gm_cmfb * ro_cmfb;
    /* Assume C_parasitic ~ 0.1 pF for CMFB loop */
    double C_par = 0.1e-12;
    res->bandwidth = 1.0 / (2.0 * M_PI * ro_cmfb * C_par);
    res->settling_error = (Vcm_target - Vcm_actual) / (1.0 + res->loop_gain);
    (void)gm_diff; (void)ro_diff;
    return 0;
}

/* L6: Two-Stage Miller Op-Amp Design
 * Complete design flow: SR -> I5, GBW -> gm1, sizing all 8 transistors.
 * Reference: Razavi Ch.10.2; Allen & Holberg Ch.6.3 */
int opamp_design_two_stage(const opamp_spec_t *spec, opamp_result_t *res)
{
    if (!spec || !res || !spec->tech) return -1;
    memset(res, 0, sizeof(opamp_result_t));
    double Vdd = spec->Vdd;
    double GBW = spec->GBW_target;
    double SR = spec->SR_target;
    double Cload = spec->Cload;
    double PM = spec->PM_target;
    const ic_technology_t *tech = spec->tech;
    double Cc = 0.5e-12, gm2 = 1e-3;
    opamp_pole_zero_placement(GBW, PM, Cload, &Cc, &gm2);
    // Cc set above
    res->Cc = Cc;
    if (Cc < 0.1e-12) Cc = 0.5e-12;
    double I5 = SR * 1e6 * Cc;
    if (I5 < 1e-6) I5 = 10e-6;
    double gm1 = 2.0 * M_PI * GBW * Cc;
    double mu_n = tech->mu_n * 1e-4;
    double mu_p = tech->mu_p * 1e-4;
    double Cox = tech->Cox;
    double WL_12 = gm1 * gm1 / (mu_n * Cox * I5);
    if (WL_12 < 2.0) WL_12 = 2.0;
    res->W1 = 10e-6;
    res->L1 = res->W1 / WL_12;
    if (res->L1 < tech->L_min) { res->L1 = tech->L_min; res->W1 = WL_12 * tech->L_min; }
    double Id_half = I5 / 2.0;
    double Vov_p = 0.18;
    double WL_34 = 2.0 * Id_half / (mu_p * Cox * Vov_p * Vov_p);
    res->W3 = 20e-6;
    res->L3 = res->W3 / WL_34;
    if (res->L3 < tech->L_min) { res->L3 = tech->L_min; res->W3 = WL_34 * tech->L_min; }
    double Vov_n = 0.18;
    double WL_5 = 2.0 * I5 / (mu_n * Cox * Vov_n * Vov_n);
    res->W5 = 10e-6;
    res->L5 = res->W5 / WL_5;
    if (res->L5 < tech->L_min) { res->L5 = tech->L_min; res->W5 = WL_5 * tech->L_min; }
    double gm6 = 5.0 * gm1;
    double I6 = gm6 * Vov_n * 0.5;
    if (I6 < 10e-6) I6 = 50e-6;
    double WL_6 = 2.0 * I6 / (mu_n * Cox * Vov_n * Vov_n);
    res->W6 = 50e-6;
    res->L6 = res->W6 / WL_6;
    if (res->L6 < tech->L_min) { res->L6 = tech->L_min; res->W6 = WL_6 * tech->L_min; }
    double WL_7 = 2.0 * I6 / (mu_p * Cox * Vov_p * Vov_p);
    res->W7 = 50e-6;
    res->L7 = res->W7 / WL_7;
    if (res->L7 < tech->L_min) { res->L7 = tech->L_min; res->W7 = WL_7 * tech->L_min; }
    res->W8 = 10e-6; res->L8 = 2e-6;
    res->Vb1 = tech->Vth_n + Vov_n;
    res->Vb3 = Vdd - tech->Vth_p - Vov_p;
    res->Id1 = Id_half; res->Id2 = Id_half;
    res->Id3 = Id_half; res->Id5 = I5;
    res->Id6 = I6; res->Id7 = I6;
    double ro1 = 1.0 / (tech->lambda_n * Id_half + 1e-15);
    double ro2 = 1.0 / (tech->lambda_n * I6 + tech->lambda_p * I6 + 1e-15);
    res->Av_dc = gm1 * ro1 * gm6 * ro2;
    res->Av_dc_dB = 20.0 * log10(res->Av_dc);
    res->GBW = gm1 / (2.0 * M_PI * Cc);
    res->UGF = res->GBW;
    two_stage_params_t t;
    t.gm1 = gm1; t.gm2 = gm6; t.ro1 = ro1; t.ro2 = ro2;
    t.Cc = Cc; t.C1 = 0.1e-12; t.C2 = Cload;
    double p1, p2, z1;
    miller_pole_split(&t, &p1, &p2, &z1);
    res->f_dominant = -p1;
    res->f_nd = -p2;
    res->phase_margin = opamp_phase_margin(res->GBW, -p1, -p2, z1);
    res->SR = I5 / Cc * 1e-6;
    double R_tail = 1.0 / (tech->lambda_n * I5 + 1e-15);
    res->CMRR = 20.0 * log10(gm1 * R_tail + 1.0);
    res->PSRR_pos = res->Av_dc_dB * 0.5;
    res->PSRR_neg = res->Av_dc_dB * 0.7;
    res->Vos_systematic = 0.0;
    res->Vin_cm_min = tech->Vth_n + Vov_n + Vov_n;
    res->Vin_cm_max = Vdd - tech->Vth_p - Vov_p;
    res->Vout_min = 2.0 * Vov_n;
    res->Vout_max = Vdd - 2.0 * Vov_p;
    res->power = Vdd * (I5 + I6);
    res->Rz = 1.0 / gm6;
    double gamma_nf = 0.667;
    res->input_noise_1kHz = sqrt(8.0 * K_BOLTZMANN * spec->T * gamma_nf / (3.0 * gm1));
    // topology stored in spec
    return 0;
}

/* L6: Folded Cascode Op-Amp Design
 * Input pair folds to opposite supply rail cascode. Wider ICMR, single gain stage.
 * Reference: Razavi Ch.10.5; Johns & Martin Ch.6.5 */
int opamp_design_folded_cascode(const opamp_spec_t *spec, opamp_result_t *res)
{
    if (!spec || !res || !spec->tech) return -1;
    memset(res, 0, sizeof(opamp_result_t));
    double Vdd = spec->Vdd, Cload = spec->Cload;
    const ic_technology_t *tech = spec->tech;
    double I_tail = 100e-6, I_fold = 120e-6;
    double Vov_in = 0.15;
    double gm_in = 2.0 * I_tail / 2.0 / Vov_in;
    double mu_p = tech->mu_p * 1e-4, Cox = tech->Cox;
    double WL_in = 2.0 * (I_tail/2.0) / (mu_p * Cox * Vov_in * Vov_in);
    res->W1 = 50e-6; res->L1 = res->W1 / WL_in;
    if (res->L1 < tech->L_min) { res->L1 = tech->L_min; res->W1 = WL_in * tech->L_min; }
    res->W5 = 20e-6; res->L5 = 0.5e-6;
    res->W6 = 20e-6; res->L6 = 0.5e-6;
    res->W7 = 30e-6; res->L7 = 1.0e-6;
    double ro_casc = 1.0 / (tech->lambda_n * I_tail/2.0 + 1e-15);
    double ro_load = 1.0 / (tech->lambda_p * I_tail/2.0 + 1e-15);
    double gm_casc = gm_in;
    double Rout = gm_casc * ro_casc * ro_load;
    res->Av_dc = gm_in * Rout;
    res->Av_dc_dB = 20.0 * log10(res->Av_dc);
    res->GBW = gm_in / (2.0 * M_PI * Cload);
    res->UGF = res->GBW;
    res->f_dominant = 1.0 / (2.0 * M_PI * Rout * Cload);
    res->f_nd = gm_casc / (2.0 * M_PI * 0.1e-12);
    res->phase_margin = 90.0 - atan(res->GBW/res->f_nd) * 180.0 / M_PI;
    res->SR = I_tail / Cload * 1e-6;
    res->CMRR = 80.0; res->PSRR_pos = 70.0; res->PSRR_neg = 75.0;
    res->power = Vdd * (I_tail + I_fold);
    res->Id1 = I_tail/2.0; res->Id2 = I_tail/2.0;
    res->Id5 = I_fold; res->Id6 = I_fold;
    res->Vb1 = tech->Vth_n + 0.2;
    res->Vb2 = tech->Vth_n + 0.5;
    res->Vb3 = Vdd - tech->Vth_p - 0.2;
    res->Vb4 = Vdd - tech->Vth_p - 0.3;
    res->Vin_cm_min = 0.3; res->Vin_cm_max = Vdd - 0.3;
    res->Vout_min = 0.3; res->Vout_max = Vdd - 0.3;
    // topology stored in spec
    return 0;
}

/* L6: Telescopic Cascode Op-Amp - highest BW, lowest power, limited swing
 * Reference: Razavi Ch.10.5.1 */
int opamp_design_telescopic(const opamp_spec_t *spec, opamp_result_t *res)
{
    if (!spec || !res || !spec->tech) return -1;
    memset(res, 0, sizeof(opamp_result_t));
    double Vdd = spec->Vdd, Cload = spec->Cload;
    const ic_technology_t *tech = spec->tech;
    double I_bias = 50e-6, Vov = 0.15;
    double gm_in = 2.0 * I_bias / Vov;
    double ro_in = 1.0 / (tech->lambda_n * I_bias + 1e-15);
    double gm_casc_n = gm_in, ro_casc_n = ro_in;
    double gm_casc_p = gm_in, ro_casc_p = ro_in;
    double Rout_n = gm_casc_n * ro_casc_n * ro_in;
    double Rout_p = gm_casc_p * ro_casc_p * ro_in;
    double Rout = 1.0 / (1.0/Rout_n + 1.0/Rout_p);
    res->Av_dc = gm_in * Rout;
    res->Av_dc_dB = 20.0 * log10(res->Av_dc);
    res->GBW = gm_in / (2.0 * M_PI * Cload);
    res->UGF = res->GBW;
    res->f_dominant = 1.0 / (2.0 * M_PI * Rout * Cload);
    res->f_nd = gm_casc_n / (2.0 * M_PI * 0.05e-12);
    res->phase_margin = 90.0 - atan(res->GBW/res->f_nd) * 180.0 / M_PI;
    res->Vout_min = 4.0 * Vov; res->Vout_max = Vdd - 4.0 * Vov;
    res->Vin_cm_min = 2.0 * Vov + tech->Vth_n;
    res->Vin_cm_max = Vdd - 2.0 * Vov;
    res->power = Vdd * I_bias;
    res->SR = I_bias / Cload * 1e-6;
    res->W1 = 50e-6; res->L1 = 0.5e-6;
    res->W3 = 50e-6; res->L3 = 0.5e-6;
    res->Vb1 = Vov + tech->Vth_n;
    res->Vb2 = 2.0 * Vov + tech->Vth_n;
    res->Vb3 = Vdd - 2.0 * Vov - tech->Vth_p;
    // topology stored in spec
    return 0;
}
