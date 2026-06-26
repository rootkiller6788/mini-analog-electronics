/*
 * analog_ic_noise.h - Noise Analysis in Analog IC Design
 *
 * L1: Noise source types and definitions
 * L2: Thermal noise, flicker noise, shot noise models
 * L3: Power spectral density (PSD), autocorrelation, Wiener-Khinchin
 * L4: Friis formula, noise figure, input-referred noise
 * L5: Noise optimization, noise matching, correlated noise analysis
 * L6: Noise analysis of common-source, cascode, diff pair stages
 * L7: Low-noise amplifier (LNA) design for RF receivers
 *
 * Course: Berkeley EE240 Ch.7, EE242 (RF IC)
 *         Stanford EE314 (RF IC Design)
 *         MIT 6.776 (RF/Analog)
 *
 * Textbook: Razavi "Design of Analog CMOS ICs" (2017) Ch.7
 *           Gray & Meyer (2009) Ch.11
 *           Lee "The Design of CMOS Radio-Frequency ICs" (2004) Ch.11
 */
#ifndef ANALOG_IC_NOISE_H
#define ANALOG_IC_NOISE_H

#include "analog_ic_basis.h"

typedef enum {
    NOISE_TYPE_THERMAL = 0,
    NOISE_TYPE_FLICKER = 1,
    NOISE_TYPE_SHOT = 2,
    NOISE_TYPE_BURST = 3,
    NOISE_TYPE_AVALANCHE = 4
} noise_type_t;

typedef struct {
    noise_type_t type;
    double Kf, Af, Ef;
    double gamma;
} noise_coefficients_t;

typedef struct {
    noise_type_t type;
    double Svv, Sii, Sv_in, Si_in;
    double corner_freq;
} noise_source_t;

typedef struct {
    double gm, gds, gamma, T;
} mos_thermal_noise_params_t;

typedef struct {
    double gm, Cox, W, L, Kf, Af, Ef, f;
} mos_flicker_noise_params_t;

typedef struct {
    double Svv_total, Svv_thermal, Svv_flicker, Svv_shot;
    double Sv_in, Si_in;
    double corner_freq;
    double integrated_noise;
    double noise_figure, noise_factor;
    double NF_min, Rn, Gopt, Bopt;
} noise_result_t;

typedef struct {
    double R, T;
} resistor_noise_params_t;

typedef struct {
    double Ic, Ib, gm, rb, T;
} bjt_noise_params_t;

/* Function Declarations */

/* L2: Fundamental noise PSD calculations */
double noise_thermal_resistor(const resistor_noise_params_t *p);
double noise_thermal_mos_drain(const mos_thermal_noise_params_t *p);
double noise_thermal_mos_gate(double Rg, double T);
double noise_flicker_mos(const mos_flicker_noise_params_t *p);
double noise_shot_diode(double I_dc, double T);
double noise_shot_bjt_collector(double Ic, double T);
double noise_shot_bjt_base(double Ib, double T);

/* L3: Noise bandwidth and integration */
double noise_integrated_total(const noise_result_t *nr, double f_low, double f_high);
double noise_rms_voltage(const noise_result_t *nr, double f_low, double f_high);
double noise_equivalent_noise_bandwidth(double f_3dB, int n_poles);
double noise_combined_psd(double Sv_thermal, double Sv_flicker, double f_corner, double f);

/* L4: Input-referred noise of amplifier stages */
int noise_input_referred_cs(mos_type_t type, double gm, double gmb, double Rs,
                             const ic_technology_t *tech, double Id, double W, double L,
                             double f, double T, noise_result_t *nr);
int noise_input_referred_diff_pair(double gm1, double gm3, double gamma1, double gamma3,
                                    const mos_flicker_noise_params_t *fp1,
                                    const mos_flicker_noise_params_t *fp3,
                                    double f, noise_result_t *nr);
int noise_input_referred_cascode(double gm_cs, double gm_cg, double gamma_cs, double gamma_cg,
                                  const mos_flicker_noise_params_t *fp_cs,
                                  double f, noise_result_t *nr);

/* L4: Friis formula for cascaded stages (Friis, 1944) */
double noise_friis_factor(double F1, double F2, double F3, double G1, double G2);
double noise_friis_temperature(double Te1, double Te2, double Te3, double G1, double G2);
double noise_figure_from_factor(double F);
double noise_factor_from_dB(double NF_dB);
double noise_temperature_from_factor(double F, double T0);

/* L5: Noise matching */
int noise_match_optimal(double Rn, double Gcor, double Bcor, double *Gopt, double *Bopt, double *NF_min);
double noise_optimal_area_min_flicker(double Kf, double Cox, double target_Sv);

/* L7: LNA noise analysis (inductively degenerated CS) */
int noise_lna_cs_inductive_degen(double gm, double Ls, double Lg, double Rs, double omega,
                                  double gamma, double delta, double c, double alpha, double T,
                                  noise_result_t *nr);

/* L2: SNR/SINAD from noise */
double noise_snr_from_result(const noise_result_t *nr, double signal_amplitude, double f_low, double f_high);
double noise_sinad(const noise_result_t *nr, double signal_rms, double f_low, double f_high);

/* L3: Noise correlation for differential circuits */
int noise_correlation_diff(double Svv1, double Svv2, double rho, double *Svv_diff, double *Svv_cm);

/* L2: kT/C noise ¡ª fundamental sampling noise */
double noise_ktc(double C, double T);

#endif
