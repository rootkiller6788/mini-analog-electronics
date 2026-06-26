/*
 * analog_ic_amplifier.h - Analog IC Design: Amplifier Topologies
 */
#ifndef ANALOG_IC_AMPLIFIER_H
#define ANALOG_IC_AMPLIFIER_H
#include "analog_ic_basis.h"

typedef enum {
    AMP_CS = 0, AMP_CE = 1, AMP_CG = 2, AMP_CB = 3,
    AMP_CD = 4, AMP_CC = 5, AMP_CASCODE = 6, AMP_DIFF_PAIR = 7
} amplifier_topology_t;

typedef struct {
    amplifier_topology_t topology;
    mos_transistor_t transistor;
    double Rs, Rd, Rload, Cload, Cbypass, I_bias, Vdd;
} amplifier_spec_t;

typedef struct {
    double Av, Av_dB, Ai, Rin, Rout, BW, GBW;
    double f_3dB_low, f_3dB_high, SR, power;
    double poles[5], zeros[5];
    int n_poles, n_zeros;
    double phase_margin;
} amplifier_result_t;

typedef enum {
    DIFF_INPUT_NMOS = 0, DIFF_INPUT_PMOS = 1,
    DIFF_INPUT_BJT_NPN = 2, DIFF_INPUT_BJT_PNP = 3
} diff_input_type_t;

typedef enum {
    DIFF_LOAD_RESISTOR = 0, DIFF_LOAD_ACTIVE = 1, DIFF_LOAD_CASCODE = 2
} diff_load_type_t;

typedef struct {
    diff_input_type_t input_type;
    diff_load_type_t load_type;
    double I_tail, R_load, W_input, L_input, W_load, L_load;
    double Vdd, Vcm, Cload;
    const ic_technology_t *tech;
    double T;
} diff_pair_spec_t;

typedef struct {
    double Ad, Acm, CMRR, Vin_cm_min, Vin_cm_max, Vin_diff_max;
    double Rout_diff, Rout_cm, I_d1, I_d2, gm1, Vdsat_input;
    double systematic_offset, power, BW, SR, input_noise_density;
} diff_pair_result_t;

typedef enum {
    OPAMP_TOPOLOGY_TWO_STAGE = 0, OPAMP_TOPOLOGY_FOLDED_CASCODE = 1,
    OPAMP_TOPOLOGY_TELESCOPIC = 2, OPAMP_TOPOLOGY_SYMMETRICAL_OTA = 3,
    OPAMP_TOPOLOGY_CURRENT_MIRROR = 4
} opamp_topology_t;

typedef struct {
    opamp_topology_t topology;
    double Av_dc_target, GBW_target, PM_target, SR_target;
    double Cload, Vdd, ICMR_min, ICMR_max;
    double Vout_swing_min, Vout_swing_max;
    double power_budget, noise_target;
    const ic_technology_t *tech;
    double T;
} opamp_spec_t;

typedef struct {
    double Av_dc, Av_dc_dB, GBW, UGF, f_dominant, f_nd;
    double phase_margin, gain_margin, SR;
    double CMRR, PSRR_pos, PSRR_neg, Vos_systematic;
    double Vin_cm_min, Vin_cm_max, Vout_min, Vout_max;
    double power, input_noise_1kHz;
    double Vb1, Vb2, Vb3, Vb4;
    double Id1, Id2, Id3, Id5, Id6, Id7;
    double W1, W3, W5, W6, W7, W8;
    double L1, L3, L5, L6, L7, L8;
    double Cc, Rz;
} opamp_result_t;

typedef struct {
    double gm1, gm2, ro1, ro2, Cc, C1, C2;
} two_stage_params_t;

typedef struct {
    double gm_in, gm_casc, ro_casc, ro_load, I_tail, I_fold;
} folded_cascode_params_t;

typedef struct {
    double gm_cmfb, Vcm_target, Vcm_actual, loop_gain, bandwidth, settling_error;
} cmfb_result_t;

/* Function Declarations */
int amplifier_analyze_cs(const amplifier_spec_t *spec, amplifier_result_t *res);
int amplifier_analyze_cg(const amplifier_spec_t *spec, amplifier_result_t *res);
int amplifier_analyze_cd(const amplifier_spec_t *spec, amplifier_result_t *res);
int amplifier_analyze_cascode(const amplifier_spec_t *spec, amplifier_result_t *res);
int amplifier_analyze_ce(const amplifier_spec_t *spec, amplifier_result_t *res);
int amplifier_analyze_cb(const amplifier_spec_t *spec, amplifier_result_t *res);
int amplifier_analyze_cc(const amplifier_spec_t *spec, amplifier_result_t *res);

int diff_pair_analyze(const diff_pair_spec_t *spec, diff_pair_result_t *res);
int diff_pair_dc_solve(const diff_pair_spec_t *spec, diff_pair_result_t *res);
double diff_pair_gm(const diff_pair_spec_t *spec);
double diff_pair_cmrr(const diff_pair_spec_t *spec);

int opamp_design_two_stage(const opamp_spec_t *spec, opamp_result_t *res);
int opamp_design_folded_cascode(const opamp_spec_t *spec, opamp_result_t *res);
int opamp_design_telescopic(const opamp_spec_t *spec, opamp_result_t *res);

int miller_pole_split(const two_stage_params_t *p, double *p1, double *p2, double *z1);
double opamp_slew_rate(double I_tail, double Cc);
double opamp_systematic_offset(double I_ref, double beta5, double beta7, const ic_technology_t *tech);
double opamp_phase_margin(double ugf, double p1, double p2, double z1);
int opamp_pole_zero_placement(double GBW_target, double PM_target, double Cload, double *Cc, double *gm2);
double opamp_gain_error(double Av_dc);
double opamp_figure_of_merit(const opamp_result_t *r);
int opamp_icmr_calc(const opamp_spec_t *spec, double *vmin, double *vmax);
int opamp_output_swing(const opamp_spec_t *spec, double *vmin, double *vmax);

double opamp_transfer_A0(double gm1, double gm2, double ro1, double ro2);
double opamp_transfer_p1(double gm2, double ro1, double ro2, double Cc, double C1, double C2);
double opamp_transfer_p2(double gm2, double Cc, double C1, double C2);
double opamp_transfer_z1(double gm2, double Cc, double Rz);
double opamp_GBW(double gm1, double Cc);
double opamp_optimal_input_gm(double Cload, double GBW, double gamma);
int opamp_cmfb_analyze(double gm_diff, double ro_diff, double gm_cmfb, double ro_cmfb, double Vcm_target, double Vcm_actual, cmfb_result_t *res);

#endif
