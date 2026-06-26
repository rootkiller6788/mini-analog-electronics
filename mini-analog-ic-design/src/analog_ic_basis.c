/*
 * analog_ic_basis.c - Core Device Models & Biasing Implementation
 *
 * Each function implements one independent knowledge point from:
 *   Razavi "Design of Analog CMOS ICs" Ch.2-4
 *   Gray & Meyer "Analysis & Design of Analog ICs" Ch.1
 *   Sansen "Analog Design Essentials" Ch.1-4
 *
 * L1: Device initialization, physical constants
 * L2: Small-signal extraction, region detection, sizing
 * L3: Drain current equations (square-law, velocity sat, subthreshold, unified)
 * L4: Pelgrom mismatch, bandgap reference
 */
#include "analog_ic_basis.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* L1: Technology Initialization - 180nm CMOS Node */
ic_technology_t *ic_tech_init_180nm(void)
{
    ic_technology_t *t = (ic_technology_t *)malloc(sizeof(ic_technology_t));
    if (!t) return NULL;
    t->Vth_n = 0.50; t->Vth_p = 0.55;
    t->mu_n = 350.0; t->mu_p = 100.0;
    t->tox = 4.1e-9;
    t->Cox = EPS0_SI * EPSR_SIO2 / t->tox;
    t->L_min = 0.18e-6; t->W_min = 0.22e-6;
    t->lambda_n = 0.08; t->lambda_p = 0.12;
    t->gamma_n = 0.55; t->gamma_p = 0.58;
    t->phi_f_n = 0.35; t->phi_f_p = 0.35;
    t->Vdsat_n = 2.5e4; t->Vdsat_p = 2.0e4;
    t->Avt = 5.0; t->Abeta = 1.5;
    return t;
}

/* L1: Technology Initialization - 65nm CMOS Node */
ic_technology_t *ic_tech_init_65nm(void)
{
    ic_technology_t *t = (ic_technology_t *)malloc(sizeof(ic_technology_t));
    if (!t) return NULL;
    t->Vth_n = 0.35; t->Vth_p = 0.38;
    t->mu_n = 250.0; t->mu_p = 80.0;
    t->tox = 1.85e-9;
    t->Cox = EPS0_SI * EPSR_SIO2 / t->tox;
    t->L_min = 0.065e-6; t->W_min = 0.10e-6;
    t->lambda_n = 0.20; t->lambda_p = 0.28;
    t->gamma_n = 0.30; t->gamma_p = 0.32;
    t->phi_f_n = 0.32; t->phi_f_p = 0.32;
    t->Vdsat_n = 1.2e4; t->Vdsat_p = 1.0e4;
    t->Avt = 3.5; t->Abeta = 1.2;
    return t;
}

/* L1: Technology Initialization - 28nm CMOS (HKMG) */
ic_technology_t *ic_tech_init_28nm(void)
{
    ic_technology_t *t = (ic_technology_t *)malloc(sizeof(ic_technology_t));
    if (!t) return NULL;
    t->Vth_n = 0.30; t->Vth_p = 0.32;
    t->mu_n = 180.0; t->mu_p = 60.0;
    t->tox = 1.35e-9;
    t->Cox = EPS0_SI * EPSR_SIO2 / t->tox;
    t->L_min = 0.028e-6; t->W_min = 0.060e-6;
    t->lambda_n = 0.35; t->lambda_p = 0.45;
    t->gamma_n = 0.20; t->gamma_p = 0.22;
    t->phi_f_n = 0.30; t->phi_f_p = 0.30;
    t->Vdsat_n = 0.6e4; t->Vdsat_p = 0.5e4;
    t->Avt = 2.5; t->Abeta = 1.0;
    return t;
}

void ic_tech_free(ic_technology_t *t) { free(t); }

/* L3: MOS Drain Current - Square Law (Shichman-Hodges 1968)
 * Id = (1/2)*mu*Cox*(W/L)*(Vgs-Vth)^2*(1+lambda*Vds)
 * Reference: Razavi eq. (2.14) */
double mos_id_square_law(const ic_technology_t *tech, const mos_transistor_t *m)
{
    if (!tech || !m || m->L <= 0.0 || m->W <= 0.0) return 0.0;
    double mu = (m->type == MOS_TYPE_NMOS) ? tech->mu_n : tech->mu_p;
    double Cox = tech->Cox;
    double Vth = (m->type == MOS_TYPE_NMOS) ? tech->Vth_n : tech->Vth_p;
    double lambda = (m->type == MOS_TYPE_NMOS) ? tech->lambda_n : tech->lambda_p;
    mu *= 1e-4; /* cm2/V/s -> m2/V/s */
    double beta = mu * Cox * (m->W / m->L);
    double Vgs = fabs(m->Vgs), Vds = fabs(m->Vds), Vov = Vgs - Vth;
    if (Vov <= 0.0) return 0.0;
    if (Vds <= Vov)
        return beta * ((Vgs - Vth) * Vds - 0.5 * Vds * Vds) * (1.0 + lambda * Vds);
    else
        return 0.5 * beta * Vov * Vov * (1.0 + lambda * Vds);
}

/* L3: MOS Drain Current - Velocity Saturation (Sodini-Ko-Moll 1984)
 * Accounts for lateral field reducing effective mobility at high Vds.
 * Id = W*Cox*vsat*(Vgs-Vth)^2 / (Vgs-Vth + Ec*L) */
double mos_id_velocity_sat(const ic_technology_t *tech, const mos_transistor_t *m)
{
    if (!tech || !m || m->L <= 0.0 || m->W <= 0.0) return 0.0;
    double mu = (m->type == MOS_TYPE_NMOS) ? tech->mu_n : tech->mu_p;
    double Cox = tech->Cox;
    double Vth = (m->type == MOS_TYPE_NMOS) ? tech->Vth_n : tech->Vth_p;
    double EcL = (m->type == MOS_TYPE_NMOS) ? tech->Vdsat_n : tech->Vdsat_p;
    mu *= 1e-4;
    double Vgs = fabs(m->Vgs), Vov = Vgs - Vth;
    if (Vov <= 0.0) return 0.0;
    double denom = 1.0 + Vov / EcL;
    return (m->W / m->L) * mu * Cox * Vov * Vov / (2.0 * denom) * (double)m->m;
}

/* L3: MOS Drain Current - Subthreshold (Weak Inversion)
 * Id = Id0*(W/L)*exp((Vgs-Vth)/(n*Vt))*(1-exp(-Vds/Vt))
 * Exponential I-V characteristic, key for low-power design.
 * Reference: Swanson & Meindl (1972); Tsividis (1999) Ch.4 */
double mos_id_subthreshold(const ic_technology_t *tech, const mos_transistor_t *m, double T)
{
    if (!tech || !m || m->L <= 0.0 || m->W <= 0.0 || T <= 0.0) return 0.0;
    double Vt = THERMAL_VT(T);
    double mu = (m->type == MOS_TYPE_NMOS) ? tech->mu_n : tech->mu_p;
    double Cox = tech->Cox;
    double Vth = (m->type == MOS_TYPE_NMOS) ? tech->Vth_n : tech->Vth_p;
    mu *= 1e-4;
    double n = 1.4;
    double Id0 = (m->W / m->L) * mu * Cox * (n - 1.0) * Vt * Vt;
    double Vgs = fabs(m->Vgs), Vds = fabs(m->Vds);
    double exp_term = exp((Vgs - Vth) / (n * Vt));
    double vds_term = 1.0 - exp(-Vds / Vt);
    return Id0 * exp_term * vds_term * (double)m->m;
}

/* L3: MOS Drain Current - Unified EKV-like Model
 * Single continuous expression valid from weak through strong inversion.
 * Uses ln^2(1+exp(x/2)) interpolation. No region boundaries.
 * Reference: Enz-Krummenacher-Vittoz (EKV, 1995); Sansen section 1.4 */
double mos_id_unified(const ic_technology_t *tech, const mos_transistor_t *m, double T)
{
    if (!tech || !m || m->L <= 0.0 || m->W <= 0.0 || T <= 0.0) return 0.0;
    double Vt = THERMAL_VT(T);
    double mu = (m->type == MOS_TYPE_NMOS) ? tech->mu_n : tech->mu_p;
    double Cox = tech->Cox;
    double Vth = (m->type == MOS_TYPE_NMOS) ? tech->Vth_n : tech->Vth_p;
    double lambda = (m->type == MOS_TYPE_NMOS) ? tech->lambda_n : tech->lambda_p;
    double n = 1.4;
    mu *= 1e-4;
    double Vgs = fabs(m->Vgs), Vds = fabs(m->Vds);
    double Ispec = 2.0 * n * mu * Cox * (m->W / m->L) * Vt * Vt;
    double vp = (Vgs - Vth) / (n * Vt);
    double if_log = log(1.0 + exp(vp / 2.0));
    double if_val = if_log * if_log;
    double ir_log = log(1.0 + exp((vp - Vds / Vt) / 2.0));
    double ir_val = ir_log * ir_log;
    return Ispec * (if_val - ir_val) * (1.0 + lambda * Vds) * (double)m->m;
}

/* L3: Body Effect - Vth modulation by Vsb
 * Vth = Vth0 + gamma*(sqrt(2*phi_f+Vsb) - sqrt(2*phi_f))
 * Reference: Razavi eq. (2.20-2.25) */
double mos_vth_body_effect(const ic_technology_t *tech, mos_type_t type,
                           double Vth0, double Vsb)
{
    if (!tech) return Vth0;
    if (Vsb < 0.0) Vsb = 0.0;
    double gamma = (type == MOS_TYPE_NMOS) ? tech->gamma_n : tech->gamma_p;
    double phi_f = (type == MOS_TYPE_NMOS) ? tech->phi_f_n : tech->phi_f_p;
    double term0 = sqrt(2.0 * phi_f);
    double term_sb = sqrt(2.0 * phi_f + Vsb);
    return Vth0 + gamma * (term_sb - term0);
}

/* L2: MOS Small-Signal Parameter Extraction
 * Extracts gm, gds, gmb, capacitances from DC operating point.
 * Strong inv: gm = 2*Id/Vov; Weak inv: gm = Id/(n*Vt)
 * Reference: Razavi Ch.2.3, Gray & Meyer Ch.1.4 */
int mos_extract_ss_params(const ic_technology_t *tech, const mos_transistor_t *m,
                          double T, mos_ss_params_t *ss)
{
    if (!m || !ss) return -1;
    if (m->L <= 0.0 || m->W <= 0.0) return -2;
    /* Use default 180nm tech if NULL */
    ic_technology_t default_tech;
    const ic_technology_t *tptr = tech;
    if (!tech) { ic_technology_t *tmp = ic_tech_init_180nm(); default_tech = *tmp; ic_tech_free(tmp); tptr = &default_tech; }
    double Vt = THERMAL_VT(T);
    double Vth = (m->type == MOS_TYPE_NMOS) ? tptr->Vth_n : tptr->Vth_p;
    double lambda = (m->type == MOS_TYPE_NMOS) ? tptr->lambda_n : tptr->lambda_p;
    double gamma = (m->type == MOS_TYPE_NMOS) ? tptr->gamma_n : tptr->gamma_p;
    double phi_f = (m->type == MOS_TYPE_NMOS) ? tptr->phi_f_n : tptr->phi_f_p;
    double Cox = tptr->Cox;
    double Vgs = fabs(m->Vgs), Vsb = fabs(m->Vsb);
    double Id = fabs(m->Id), Vov = Vgs - Vth;
    double n_sub = 1.4;
    memset(ss, 0, sizeof(mos_ss_params_t));

    /* Transconductance gm */
    if (Vov > 0.15)
        ss->gm = (Vov > 1e-12) ? 2.0 * Id / Vov : 0.0;
    else if (Vov > -0.05) {
        double gm_si = (Vov > 1e-12) ? 2.0 * Id / Vov : 0.0;
        double gm_wi = Id / (n_sub * Vt);
        double w = 1.0 / (1.0 + exp(-Vov / 0.02));
        ss->gm = w * gm_si + (1.0 - w) * gm_wi;
    } else
        ss->gm = Id / (n_sub * Vt);

    /* Output conductance and resistance */
    ss->gds = lambda * Id;
    ss->ro = (ss->gds > 1e-15) ? 1.0 / ss->gds : 1e12;
    ss->intrinsic_gain = ss->gm * ss->ro;

    /* Body transconductance gmb */
    double denom_gmb = 2.0 * sqrt(2.0 * phi_f + Vsb);
    ss->gmb = (denom_gmb > 1e-12) ? ss->gm * gamma / denom_gmb : 0.0;

    /* Capacitances - simplified Meyer model */
    double Cgs_total = (2.0/3.0) * m->W * m->L * Cox;
    double Cgd_overlap = 0.2e-9 * m->W;
    ss->Cgs = Cgs_total - Cgd_overlap;
    if (ss->Cgs < 0.0) ss->Cgs = Cgs_total * 0.5;
    ss->Cgd = Cgd_overlap;
    ss->Cdb = 0.5e-3 * m->W * m->L;
    ss->Csb = 0.5e-3 * m->W * m->L;
    ss->Cgb = 0.05e-3 * m->W * m->L;

    /* Transition frequency ft = gm/(2*pi*(Cgs+Cgd)) */
    double C_total = ss->Cgs + ss->Cgd;
    ss->ft = (C_total > 1e-18) ? ss->gm / (2.0 * M_PI * C_total) : 0.0;
    return 0;
}

/* L2: BJT Collector Current - Forward Active (Gummel-Poon simplified)
 * Ic = Is*exp(Vbe/Vt)*(1+Vce/VA)
 * Reference: Sedra/Smith Ch.7.3 */
double bjt_ic(const bjt_transistor_t *bjt)
{
    if (!bjt || bjt->T <= 0.0) return 0.0;
    double Vt = THERMAL_VT(bjt->T);
    double Is = bjt->Is;
    if (bjt->Vbe / Vt > 50.0) return 1.0; /* overflow guard */
    double Ic_ideal = Is * exp(bjt->Vbe / Vt);
    double early_factor = (bjt->VA > 1.0) ? (1.0 + bjt->Vce / bjt->VA) : 1.0;
    return Ic_ideal * early_factor;
}

/* L2: BJT Small-Signal Parameter Extraction
 * gm = Ic/Vt, rpi = beta_f/gm, ro = VA/Ic
 * Reference: Gray & Meyer Ch.1.4 */
int bjt_extract_ss_params(const bjt_transistor_t *bjt, bjt_ss_params_t *ss)
{
    if (!bjt || !ss || bjt->T <= 0.0) return -1;
    if (bjt->Ic <= 0.0) return -2;
    memset(ss, 0, sizeof(bjt_ss_params_t));
    double Vt = THERMAL_VT(bjt->T);
    ss->gm = bjt->Ic / Vt;
    ss->rpi = (bjt->beta_f > 0.0 && ss->gm > 1e-15) ? bjt->beta_f / ss->gm : 1e12;
    ss->ro = (bjt->VA > 0.0 && bjt->Ic > 1e-15) ? bjt->VA / bjt->Ic : 1e12;
    ss->rmu = 10.0 * bjt->beta_f * (bjt->VA > 0.0 ? bjt->VA / bjt->Ic : 1e6);
    double tau_f = 10e-12;
    ss->Cpi = tau_f * ss->gm + 20e-15;
    ss->Cmu = 10e-15;
    double C_sum = ss->Cpi + ss->Cmu;
    ss->ft = (C_sum > 1e-18) ? ss->gm / (2.0 * M_PI * C_sum) : 0.0;
    return 0;
}

/* L2: Detect MOS operating region from terminal voltages */
mos_region_t mos_detect_region(const ic_technology_t *tech, const mos_transistor_t *m)
{
    if (!tech || !m) return MOS_REGION_CUTOFF;
    double Vth = (m->type == MOS_TYPE_NMOS) ? tech->Vth_n : tech->Vth_p;
    double Vgs = fabs(m->Vgs), Vds = fabs(m->Vds), Vov = Vgs - Vth;
    if (Vgs < Vth - 0.05)
        return (Vgs > Vth - 0.15) ? MOS_REGION_SUBTHRESHOLD : MOS_REGION_CUTOFF;
    if (Vds <= Vov - 0.01) return MOS_REGION_TRIODE;
    double EcL = (m->type == MOS_TYPE_NMOS) ? tech->Vdsat_n : tech->Vdsat_p;
    return (Vov > EcL * 0.5) ? MOS_REGION_VELOCITY_SAT : MOS_REGION_SATURATION;
}

/* L2: MOS sizing - compute W/L from Id, Vov
 * W/L = 2*Id / (mu*Cox*Vov^2) */
double mos_sizing_wl(const ic_technology_t *tech, mos_type_t type,
                     double Id, double Vov, double L)
{
    if (!tech || Id <= 0.0 || Vov <= 0.0 || L <= 0.0) return 0.0;
    double mu = (type == MOS_TYPE_NMOS) ? tech->mu_n : tech->mu_p;
    mu *= 1e-4;
    return (2.0 * Id) / (mu * tech->Cox * Vov * Vov);
}

/* L2: Transconductance efficiency gm/Id [1/V]
 * Central parameter in gm/ID design methodology.
 * SI: gm/Id = 2/Vov; WI: gm/Id = 1/(n*Vt) ~ 25 1/V */
double mos_gm_over_id(const ic_technology_t *tech, const mos_transistor_t *m, double T)
{
    if (!tech || !m || m->Id <= 0.0 || T <= 0.0) return 0.0;
    double Vt = THERMAL_VT(T);
    double Vth = (m->type == MOS_TYPE_NMOS) ? tech->Vth_n : tech->Vth_p;
    double Vgs = fabs(m->Vgs), Vov = Vgs - Vth;
    double n_sub = 1.4;
    if (Vov > 0.2) return 2.0 / Vov;
    if (Vov > 0.0) {
        double gm_id_si = 2.0 / (Vov + 1e-12);
        double gm_id_wi = 1.0 / (n_sub * Vt);
        double alpha = 1.0 / (1.0 + exp(-Vov / 0.05));
        return alpha * gm_id_si + (1.0 - alpha) * gm_id_wi;
    }
    return 1.0 / (n_sub * Vt);
}

/* L4: Beta-Multiplier Self-Biasing (constant-gm bias)
 * Iref ~ 2*(1-1/sqrt(K))^2 / (mu*Cox*(W/L)*R^2)
 * Supply-independent reference current generation.
 * Reference: Razavi Ch.11.3; Gray & Meyer Ch.4.5 */
double bias_beta_multiplier_current(const ic_technology_t *tech,
                                     double R, double K, double T)
{
    if (!tech || R <= 0.0 || K <= 1.0 || T <= 0.0) return 0.0;
    (void)T;
    double mu_n = tech->mu_n * 1e-4;
    double Cox = tech->Cox;
    double W_L = 5.0;
    double one_minus_inv_sqrtK = 1.0 - 1.0 / sqrt(K);
    if (one_minus_inv_sqrtK <= 0.0) return 0.0;
    double beta_factor = 2.0 * one_minus_inv_sqrtK * one_minus_inv_sqrtK;
    return beta_factor / (mu_n * Cox * W_L * R * R);
}

/* L4: Widlar Current Source (Widlar, JSSC 1965)
 * Solves transcendental: I_out*R2 = Vt*ln(I_ref/I_out)
 * using Newton-Raphson iteration. */
double bias_widlar_current(double Vcc, double R1, double R2,
                            const bjt_transistor_t *bjt_ref)
{
    if (!bjt_ref || bjt_ref->T <= 0.0 || R1 <= 0.0 || R2 <= 0.0 || Vcc <= 0.0)
        return 0.0;
    double Vt = THERMAL_VT(bjt_ref->T);
    double Vbe = bjt_ref->Vbe;
    double I_ref = (Vcc > Vbe) ? (Vcc - Vbe) / R1 : 0.0;
    if (I_ref <= 0.0) return 0.0;
    double I_out = I_ref * 0.1;
    int iter;
    for (iter = 0; iter < 50; iter++) {
        double f = I_out * R2 - Vt * log(I_ref / I_out);
        double df = R2 + Vt / I_out;
        if (fabs(df) < 1e-30) break;
        double delta = f / df;
        I_out -= delta;
        if (fabs(delta) < 1e-15 * I_out) break;
    }
    if (I_out <= 0.0 || I_out > I_ref) return I_ref * 0.05;
    return I_out;
}

/* L4: Supply-Independent Threshold-Referenced Bias
 * Vbias = Vth + sqrt(2*Iref/(mu*Cox*W/L))
 * Reference: Razavi Ch.11.3.3 */
double bias_supply_independent(const ic_technology_t *tech,
                                double R, double T)
{
    if (!tech || R <= 0.0 || T <= 0.0) return 0.0;
    (void)T;
    double mu_n = tech->mu_n * 1e-4;
    double Cox = tech->Cox;
    double Vth_n = tech->Vth_n;
    double K = 4.0;
    double Iref = bias_beta_multiplier_current(tech, R, K, 300.0);
    if (Iref < 1e-12) Iref = 10e-6;
    double W_L_diode = 10.0;
    double Vov = 0.0;
    if (mu_n * Cox * W_L_diode > 1e-15)
        Vov = sqrt(2.0 * Iref / (mu_n * Cox * W_L_diode));
    return Vth_n + Vov;
}

/* L4: Pelgrom Mismatch Parameters from Technology */
mismatch_params_t mismatch_from_tech(const ic_technology_t *tech)
{
    mismatch_params_t mp;
    memset(&mp, 0, sizeof(mp));
    if (tech) { mp.Avt = tech->Avt; mp.Abeta = tech->Abeta; }
    return mp;
}

/* LCG pseudo-random generator for mismatch sampling (Numerical Recipes) */
static double pelgrom_rand(uint64_t *seed)
{
    *seed = (6364136223846793005ULL * (*seed)) + 1;
    return (double)(*seed >> 33) / (double)(0x7FFFFFFF);
}

/* L4: Pelgrom Mismatch Sample — Box-Muller transform
 * sigma_Vth = Avt/sqrt(W*L), sigma_beta = Abeta/sqrt(W*L)
 * Reference: Pelgrom, Duinmaijer, Welbers, JSSC 1989, Eq. (7), (10) */
mismatch_sample_t mismatch_pelgrom_sample(const mismatch_params_t *params,
                                           double W, double L, uint64_t seed)
{
    mismatch_sample_t ms;
    memset(&ms, 0, sizeof(ms));
    if (!params || W <= 0.0 || L <= 0.0) return ms;
    double area_sqrt = sqrt(W * L);
    double sigma_Vth = 0.0, sigma_beta = 0.0;
    if (area_sqrt > 1e-12) {
        sigma_Vth = (params->Avt * 1e-3) / (area_sqrt * 1e6);
        sigma_beta = (params->Abeta * 1e-2) / (area_sqrt * 1e6);
    }
    uint64_t s1 = seed;
    double u1 = pelgrom_rand(&s1);
    double u2 = pelgrom_rand(&s1);
    if (u1 < 1e-12) u1 = 1e-12;
    double mag = sqrt(-2.0 * log(u1));
    ms.delta_Vth = sigma_Vth * mag * cos(2.0 * M_PI * u2);
    ms.delta_beta = sigma_beta * mag * sin(2.0 * M_PI * u2);
    ms.delta_Id = (ms.delta_beta + 20.0 * ms.delta_Vth) * 1e-6;
    return ms;
}

/* L2: gm/ID Lookup Table Generation — sweeps Vgs, computes gm, ft, gm/Id
 * Reference: Murmann, "Systematic Design of Analog CMOS Circuits" */
int gmid_generate_table(const ic_technology_t *tech, mos_type_t type,
                         double T, double L, int n_points,
                         gmid_data_point_t *table)
{
    if (!tech || !table || n_points <= 0 || L <= 0.0 || T <= 0.0) return -1;
    double Vth = (type == MOS_TYPE_NMOS) ? tech->Vth_n : tech->Vth_p;
    double Vgs_min = Vth - 0.1;
    double Vgs_max = Vth + 0.8;
    double Vgs_step = (Vgs_max - Vgs_min) / (double)(n_points - 1);
    int i;
    for (i = 0; i < n_points; i++) {
        double Vgs = Vgs_min + (double)i * Vgs_step;
        mos_transistor_t m;
        memset(&m, 0, sizeof(m));
        m.type = type; m.W = 10e-6; m.L = L; m.m = 1;
        m.Vgs = Vgs; m.Vds = 0.9; m.Vsb = 0.0;
        m.Id = mos_id_unified(tech, &m, T);
        m.region = mos_detect_region(tech, &m);
        mos_ss_params_t ss;
        mos_extract_ss_params(tech, &m, T, &ss);
        table[i].Vgs = Vgs;
        table[i].gm_over_Id = (m.Id > 1e-15) ? ss.gm / m.Id : 0.0;
        table[i].ft = ss.ft;
        table[i].Vdsat = (Vgs > Vth) ? Vgs - Vth : 0.025;
        table[i].Id_over_W = (m.W > 1e-15) ? m.Id / (m.W / m.L) : 0.0;
        table[i].gm_gds = ss.intrinsic_gain;
    }
    return 0;
}

/* L2: Find Vgs for target gm/Id via binary search */
double gmid_find_vgs(const gmid_data_point_t *table, int n_points,
                      double target_gmid)
{
    if (!table || n_points <= 0) return 0.0;
    int lo = 0, hi = n_points - 1;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (table[mid].gm_over_Id > target_gmid) lo = mid + 1;
        else hi = mid;
    }
    return table[lo].Vgs;
}

/* L2: Apply Process Corner + Temperature Corner Scaling
 * FF: +15% mu, -10% Vth; SS: -15% mu, +10% Vth
 * T(HOT): -30% mu; T(COLD): +20% mu */
void ic_tech_apply_corner(ic_technology_t *tech, process_corner_t corner,
                          temperature_corner_t temp)
{
    if (!tech) return;
    double mu_n_factor = 1.0, mu_p_factor = 1.0;
    double vth_n_shift = 0.0, vth_p_shift = 0.0;
    switch (corner) {
        case CORNER_FF: mu_n_factor = 1.15; mu_p_factor = 1.15;
            vth_n_shift = -0.05; vth_p_shift = -0.05; break;
        case CORNER_SS: mu_n_factor = 0.85; mu_p_factor = 0.85;
            vth_n_shift = +0.05; vth_p_shift = +0.05; break;
        case CORNER_FS: mu_n_factor = 1.15; mu_p_factor = 0.85;
            vth_n_shift = -0.05; vth_p_shift = +0.05; break;
        case CORNER_SF: mu_n_factor = 0.85; mu_p_factor = 1.15;
            vth_n_shift = +0.05; vth_p_shift = -0.05; break;
        default: break;
    }
    switch (temp) {
        case TEMP_CORNER_COLD: mu_n_factor *= 1.20; mu_p_factor *= 1.20;
            vth_n_shift += 0.03; vth_p_shift += 0.03; break;
        case TEMP_CORNER_HOT: mu_n_factor *= 0.70; mu_p_factor *= 0.70;
            vth_n_shift -= 0.04; vth_p_shift -= 0.04; break;
        default: break;
    }
    tech->mu_n *= mu_n_factor; tech->mu_p *= mu_p_factor;
    tech->Vth_n += vth_n_shift; tech->Vth_p += vth_p_shift;
    tech->lambda_n /= mu_n_factor; tech->lambda_p /= mu_p_factor;
}
