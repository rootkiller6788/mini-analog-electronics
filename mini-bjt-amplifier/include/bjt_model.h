/**
 * bjt_model.h ? BJT Device Models and Physical Parameters
 *
 * Implements the fundamental mathematical models of the Bipolar Junction
 * Transistor used throughout amplifier analysis. Covers:
 *   - Ebers-Moll injection model (large-signal)
 *   - Hybrid-pi small-signal model
 *   - T-model (re-model) small-signal equivalent
 *   - h-parameter two-port model
 *   - Gummel-Poon charge-control model (simplified)
 *
 * Reference: Sedra & Smith, "Microelectronic Circuits", 8th ed., Ch. 6-7
 *            Gray, Hurst, Lewis & Meyer, "Analysis and Design of Analog ICs"
 *
 * Knowledge Levels:
 *   L1: BJT terminal definitions, NPN vs PNP, operating regions
 *   L2: DC operating point concept, small-signal vs large-signal
 *   L3: Hybrid-pi matrix, T-model topology, two-port parameters
 *   L4: Shockley diode => IC-VBE exponential law, gm=IC/VT
 */

#ifndef BJT_MODEL_H
#define BJT_MODEL_H

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Thermal voltage at 300K (26 mV nominal) */
#define BJT_VT_300K   0.025851991
#define BJT_K_OVER_Q  8.617333262e-5
#define BJT_VGO       1.205

/* Operating region enumeration */
typedef enum {
    BJT_REGION_CUTOFF         = 0,
    BJT_REGION_FORWARD_ACTIVE = 1,
    BJT_REGION_REVERSE_ACTIVE = 2,
    BJT_REGION_SATURATION     = 3,
} bjt_region_t;

typedef enum {
    BJT_NPN = 0,
    BJT_PNP = 1,
} bjt_polarity_t;

/* BJT SPICE-like Model Parameters */
typedef struct {
    double IS, BF, BR, NF, NR;
    double VAF, VAR, IKF, IKR;
    double ISE, ISC, NE, NC;
    double CJE, CJC, CJS;
    double VJE, VJC, VJS;
    double MJE, MJC, MJS;
    double TF, TR;
    double RB, RC, RE;
    double KF, AF, XTI, EG, TNOM;
} bjt_spice_params_t;

/* BJT DC Operating Point (Q-point) */
typedef struct {
    double IC, IB, IE;
    double VCE, VBE, VBC;
    double power;
    bjt_region_t region;
} bjt_qpoint_t;

/* BJT Small-Signal Parameters (Hybrid-pi + T-model) */
typedef struct {
    double gm, rpi, ro, rmu;
    double Cpi, Cmu;
    double re, alpha, beta;
    double fT, f_beta;
} bjt_ss_params_t;

/* h-parameter Two-Port Model (common-emitter) */
typedef struct {
    double h11, h12, h21, h22;
} bjt_h_params_t;

/* DC Bias Configuration Types */
typedef enum {
    BIAS_FIXED_BASE           = 0,
    BIAS_EMITTER_STABILIZED   = 1,
    BIAS_VOLTAGE_DIVIDER      = 2,
    BIAS_COLLECTOR_FEEDBACK   = 3,
    BIAS_CURRENT_SOURCE       = 4,
    BIAS_DUAL_SUPPLY          = 5,
} bjt_bias_type_t;

/* BJT Amplifier Configuration Types */
typedef enum {
    AMP_COMMON_EMITTER        = 0,
    AMP_COMMON_COLLECTOR      = 1,
    AMP_COMMON_BASE           = 2,
    AMP_CASCODE               = 3,
    AMP_DIFFERENTIAL_PAIR     = 4,
    AMP_DARLINGTON            = 5,
} bjt_amp_topology_t;

/* BJT amplifier DC bias resistor network */
typedef struct {
    double R1, R2, RC, RE, RB, Rs;
    double VCC, VEE;
    bjt_bias_type_t type;
} bjt_bias_network_t;

/* Amplifier Performance Metrics */
typedef struct {
    double Av, Av_dB;
    double Ai, Ai_dB;
    double Ap, Ap_dB;
    double Rin, Rout;
    double f_L, f_H, BW, GBW;
    double THD, swing_max, efficiency;
} bjt_amp_metrics_t;

/* Two-Port Network Parameters */
typedef struct {
    double z11, z12, z21, z22;
} bjt_z_params_t;

typedef struct {
    double y11, y12, y21, y22;
} bjt_y_params_t;

/* ---- L4: Fundamental Law Functions ---- */

/* Thermal voltage VT = kT/q at temperature T [K]. */
double bjt_vt(double temperature_kelvin);

/* Transconductance: gm = IC / VT.
 * Fundamental derivative of the Shockley diode equation. */
double bjt_gm(double ic, double vt);

/* Intrinsic emitter resistance: re = VT / IE. */
double bjt_re(double ie, double vt);

/* Base-emitter input resistance: rpi = beta * VT / IC. */
double bjt_rpi(double beta, double gm);

/* Output resistance from Early effect: ro = VA / IC. */
double bjt_ro(double va_early, double ic);

/* alpha from beta: alpha = beta/(beta+1). */
double bjt_alpha_from_beta(double beta);

/* beta from alpha: beta = alpha/(1-alpha). */
double bjt_beta_from_alpha(double alpha);

/* Full Ebers-Moll equation for IC(VBE, VBC).
 * Reference: Ebers & Moll (1954), IRE Proc., vol. 42, pp. 1761-1772. */
double bjt_ebers_moll_ic(double vbe, double vbc, double vt,
                          double IS, double BF, double BR,
                          double NF, double NR, double VAF, double VAR);

/* Inverse Shockley: VBE computed from IC. */
double bjt_vbe_from_ic(double ic, double IS, double vt, double nf);

/* Determine operating region from VBE, VBC. */
bjt_region_t bjt_determine_region(double vbe, double vbc,
                                   double vt, bjt_polarity_t pol);

/* Initialize default SPICE parameters (2N3904-like NPN). */
void bjt_spice_default_npn(bjt_spice_params_t *p);

/* Initialize default SPICE parameters (2N3906-like PNP). */
void bjt_spice_default_pnp(bjt_spice_params_t *p);

/* Compute small-signal parameters from Q-point and SPICE model. */
void bjt_compute_ss_params(const bjt_qpoint_t *qp,
                            const bjt_spice_params_t *sp,
                            double temperature_kelvin,
                            bjt_ss_params_t *ss);

/* h-parameters from hybrid-pi parameters. */
void bjt_h_from_ss(const bjt_ss_params_t *ss, bjt_h_params_t *hp);

/* y-parameters from hybrid-pi at a given frequency. */
void bjt_y_from_ss(const bjt_ss_params_t *ss, double frequency_hz,
                    bjt_y_params_t *yp);

/* z-parameters as inverse of y-parameters (2x2 matrix inversion). */
void bjt_z_from_ss(const bjt_ss_params_t *ss, double frequency_hz,
                    bjt_z_params_t *zp);

/* Transition frequency fT = gm / (2*pi*(Cpi+Cmu)). */
double bjt_compute_fT(const bjt_ss_params_t *ss);

/* Beta cut-off frequency: f_beta = fT / beta. */
double bjt_compute_f_beta(const bjt_ss_params_t *ss);

/* Gummel-Poon base charge factor qb. */
double bjt_gummel_poon_qb(double vbe, double vbc, double vt,
                           double VAF, double VAR, double IKF, double IKR,
                           double IS, double NF);

/* Junction capacitance at applied voltage (SPICE model). */
double bjt_junction_cap(double cj0, double vj, double v_applied,
                         double mj);

/* Convert linear gain to dB: 20*log10(fabs(Av)). */
double bjt_gain_to_db(double gain_linear);

/* Convert dB to linear voltage gain: pow(10, dB/20). */
double bjt_gain_from_db(double gain_db);

/* Effective base resistance including spreading resistance rbb. */
double bjt_rbb_effect(double rbb, double rpi);

/* Diffusion capacitance: Cd = TF * gm (charge-control model). */
double bjt_diffusion_capacitance(double tf, double gm);

#endif /* BJT_MODEL_H */
