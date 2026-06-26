/**
 * opamp_basic.h - Operational Amplifier Fundamental Definitions and Ideal Models
 *
 * This header defines the core data structures and analytical functions for
 * operational amplifiers, covering both ideal and non-ideal models.
 *
 * Knowledge Coverage:
 *   L1 (Definitions): OpAmpParams, IdealOpAmp, OpAmpNonIdealities structs
 *   L2 (Core Concepts): Virtual short, virtual ground, negative feedback
 *   L3 (Math Structures): s-domain transfer functions, pole-zero models
 *   L4 (Fundamental Laws): Op-amp golden rules, open-loop gain-bandwidth product
 *
 * Reference: Sedra & Smith "Microelectronic Circuits" 8th ed. (2020), Ch. 2, 9, 10
 *            Franco "Design with Op Amps and Analog ICs" 4th ed. (2015), Ch. 1-3
 *            Gray, Hurst, Lewis, Meyer "Analysis and Design of Analog ICs" 5th ed. (2009)
 *
 * Course Mapping:
 *   MIT 6.002 (Circuits and Electronics) - Op-amp fundamentals
 *   Stanford EE101 (Introduction to Circuits) - Ideal op-amp model
 *   Berkeley EE105 (Microelectronic Devices and Circuits) - Non-ideal effects
 *   ETH 227-0436 (Analog Signal Processing) - Precision modeling
 */

#ifndef OPAMP_BASIC_H
#define OPAMP_BASIC_H

#include <stddef.h>
#include <stdint.h>
#define _USE_MATH_DEFINES
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*============================================================================
 * L1: CORE DEFINITIONS - Operational Amplifier Parameters
 *============================================================================*/

/**
 * OpAmpParams - Complete operational amplifier specification parameters.
 *
 * These parameters characterize both DC and AC behavior of a real op-amp.
 * Each parameter corresponds to a datasheet specification.
 *
 * Reference: Sedra & Smith S2.8 (Non-ideal characteristics of op-amps)
 *            TI Application Report SLAA068 (Understanding Op Amp Parameters)
 */
typedef struct {
    /* DC Specifications */
    double A_ol;            /**< Open-loop DC gain (V/V), typically 80-140 dB */
    double V_os;            /**< Input offset voltage (V), typically uV-mV */
    double I_bias;          /**< Input bias current (A), typically nA-uA for BJT */
    double I_os;            /**< Input offset current (A), difference between inputs */
    double R_in_diff;       /**< Differential input resistance (Ohm) */
    double R_in_cm;         /**< Common-mode input resistance (Ohm) */
    double R_out;           /**< Output resistance (Ohm), open-loop */
    double V_oh;            /**< Output high saturation voltage (V) */
    double V_ol;            /**< Output low saturation voltage (V) */
    double I_sc;            /**< Short-circuit output current (A) */
    double PSRR_plus;       /**< Positive power supply rejection ratio (dB) */
    double PSRR_minus;      /**< Negative power supply rejection ratio (dB) */
    double CMRR;            /**< Common-mode rejection ratio (dB) */

    /* AC Specifications */
    double GBW;             /**< Gain-bandwidth product (Hz), unity-gain frequency */
    double SR;              /**< Slew rate (V/s), maximum rate of output change */
    double f_t;             /**< Transition frequency (Hz), where |A| = 1 */
    double f_p1;            /**< First (dominant) pole frequency (Hz) */
    double f_p2;            /**< Second pole frequency (Hz) */
    double f_p3;            /**< Third pole frequency (Hz) */
    double PM;              /**< Phase margin (degrees), at unity-gain frequency */
    double t_settle;        /**< Settling time to 0.1% (s) */
    double BW_full_power;   /**< Full-power bandwidth (Hz), BW = SR/(2*pi*V_pk) */

    /* Noise Specifications */
    double e_n;             /**< Input voltage noise density (V/sqrt(Hz)) at 1 kHz */
    double i_n_plus;        /**< Non-inverting input current noise (A/sqrt(Hz)) */
    double i_n_minus;       /**< Inverting input current noise (A/sqrt(Hz)) */
    double e_n_1f_corner;   /**< 1/f noise corner frequency (Hz) */

    /* Supply */
    double V_supply_min;    /**< Minimum supply voltage (V) */
    double V_supply_max;    /**< Maximum supply voltage (V) */
    double I_q;             /**< Quiescent current per amplifier (A) */
} OpAmpParams;

/**
 * IdealOpAmp - Properties of the mathematical ideal operational amplifier.
 *
 * The ideal op-amp is defined by three golden rules:
 *   1. Infinite open-loop gain: A_ol -> infinity
 *   2. Infinite input impedance: Z_in -> infinity (zero input currents)
 *   3. Zero output impedance: Z_out -> 0
 *
 * Reference: Sedra & Smith S2.1 (The ideal op-amp)
 *            Horowitz & Hill "The Art of Electronics" 3rd ed. S4.1
 */
typedef struct {
    int is_ideal;           /**< Flag: 1 = ideal model active */
    double A_ol_inf;        /**< Conceptual infinite gain marker (> 1e9) */
    double Z_in_inf;        /**< Conceptual infinite input impedance (> 1e12) */
    double Z_out_zero;      /**< Conceptual zero output impedance (< 0.01) */
} IdealOpAmp;

/**
 * OpAmpPoleZeroModel - Pole-zero representation of open-loop transfer function.
 *
 *   A(s) = A_ol / ((1 + s/w_p1)(1 + s/w_p2)(1 + s/w_p3))
 *
 * For dominant-pole compensated op-amps (e.g., 741):
 *   A(s) = A_ol / (1 + s/w_p1) = GBW / (s + w_p1) * A_ol
 *
 * Reference: Sedra & Smith S10.8 (Frequency response of op-amp circuits)
 *            Gray & Meyer S9.4 (Compensation)
 */
typedef struct {
    double A_ol;            /**< DC open-loop gain */
    double f_poles[5];      /**< Pole frequencies (Hz), up to 5 poles */
    double f_zeros[3];      /**< Zero frequencies (Hz), up to 3 zeros */
    int n_poles;            /**< Number of poles in model */
    int n_zeros;            /**< Number of zeros in model */
} OpAmpPoleZeroModel;

/**
 * OpAmpNoiseModel - Comprehensive noise analysis model.
 *
 * Total input-referred noise: e_n_total^2 = e_n^2 + (i_n*R_s)^2 + 4kTR_s
 * where k = 1.380649e-23 J/K (Boltzmann constant)
 *
 * Reference: Motchenbacher & Connelly "Low-Noise Electronic System Design" (1993)
 *            TI Application Report SLVA043 (Noise Analysis in Op Amp Circuits)
 */
typedef struct {
    double e_n_white;       /**< White noise voltage density (V/sqrt(Hz)) */
    double e_n_flicker_1Hz; /**< Flicker noise voltage at 1 Hz (V/sqrt(Hz)) */
    double i_n_white;       /**< White noise current density (A/sqrt(Hz)) */
    double f_ce;            /**< Voltage noise 1/f corner frequency (Hz) */
    double f_ci;            /**< Current noise 1/f corner frequency (Hz) */
} OpAmpNoiseModel;

/*============================================================================
 * L2: CORE CONCEPTS - Ideal Op-Amp Analysis
 *============================================================================*/

/**
 * IdealOpAmpRules - The three golden rules for ideal op-amp analysis.
 *
 * Constitution of ideal op-amp circuits:
 *   Rule 1: v_plus = v_minus (virtual short, when negative feedback present)
 *   Rule 2: i_plus = i_minus = 0 (infinite input impedance)
 *   Rule 3: Output can drive any load (zero output impedance)
 *
 * These rules hold ONLY when:
 *   (a) Negative feedback is present
 *   (b) Op-amp is not saturated (v_out within supply rails)
 *
 * Reference: Horowitz & Hill "The Art of Electronics" S4.1
 */
static inline int opamp_virtual_short_applies(double v_out, double v_sat_pos, double v_sat_neg) {
    /* Virtual short applies only in linear region (not saturated) */
    return (v_out < v_sat_pos - 0.1) && (v_out > v_sat_neg + 0.1);
}

/**
 * opamp_ideal_gain_error - Calculate gain error due to finite open-loop gain.
 *
 * For a non-inverting amplifier with feedback factor beta:
 *   A_cl = A_ol / (1 + A_ol*beta)
 *   Gain error (%) = |A_cl_ideal - A_cl_actual| / A_cl_ideal * 100%
 *
 * @param A_ol    Open-loop DC gain (V/V)
 * @param beta    Feedback factor (0 < beta <= 1)
 * @return        Gain error in percent (%)
 *
 * Reference: Franco S1.4 (Effect of finite open-loop gain)
 */
static inline double opamp_gain_error_percent(double A_ol, double beta) {
    double A_cl_ideal = 1.0 / beta;
    double A_cl_actual = A_ol / (1.0 + A_ol * beta);
    return 100.0 * fabs(A_cl_ideal - A_cl_actual) / A_cl_ideal;
}

/*============================================================================
 * L3: MATHEMATICAL STRUCTURES - s-Domain Transfer Functions
 *============================================================================*/

/**
 * opamp_open_loop_gain_mag - Magnitude of open-loop gain at frequency f.
 *
 * Single-pole model:
 *   |A(jw)| = A_ol / sqrt(1 + (f/f_p1)^2)
 *
 * @param params  Op-amp parameters
 * @param f       Frequency (Hz)
 * @return        |A(j*2*pi*f)| magnitude
 *
 * Reference: Sedra & Smith S10.8.1
 */
static inline double opamp_open_loop_gain_mag(const OpAmpParams *params, double f) {
    if (f <= 0.0) return params->A_ol;
    return params->A_ol / sqrt(1.0 + (f / params->f_p1) * (f / params->f_p1));
}

/**
 * opamp_open_loop_gain_db - Open-loop gain in decibels.
 */
static inline double opamp_open_loop_gain_db(const OpAmpParams *params, double f) {
    double mag = opamp_open_loop_gain_mag(params, f);
    return 20.0 * log10(mag);
}

/**
 * opamp_open_loop_phase - Phase shift of open-loop gain at frequency f.
 *
 * Single-pole model:
 *   phi(f) = -arctan(f/f_p1) (dominant pole)
 *
 * @param params  Op-amp parameters
 * @param f       Frequency (Hz)
 * @return        Phase shift in radians
 */
static inline double opamp_open_loop_phase(const OpAmpParams *params, double f) {
    return -atan(f / params->f_p1);
}

/**
 * opamp_open_loop_phase_deg - Phase shift in degrees.
 */
static inline double opamp_open_loop_phase_deg(const OpAmpParams *params, double f) {
    return opamp_open_loop_phase(params, f) * 180.0 / M_PI;
}

/**
 * opamp_closed_loop_bandwidth - Closed-loop -3dB bandwidth.
 *
 * For dominant-pole compensated op-amp:
 *   f_3dB_cl = GBW / A_cl = f_t * beta
 *
 * @param params  Op-amp parameters
 * @param A_cl    Closed-loop DC gain (V/V)
 * @return        -3dB bandwidth (Hz)
 *
 * Reference: Sedra & Smith S2.7.1 (Gain-bandwidth trade-off)
 */
static inline double opamp_closed_loop_bandwidth(const OpAmpParams *params, double A_cl) {
    if (A_cl <= 0.0) return 0.0;
    return params->GBW / A_cl;
}

/*============================================================================
 * L4: FUNDAMENTAL LAWS - Op-Amp Golden Rules and Constraints
 *============================================================================*/

/**
 * opamp_check_linear_region - Verify op-amp is operating in linear region.
 *
 * For a real op-amp with finite supply rails:
 *   Output saturation occurs when |v_out| >= V_supply - V_headroom
 *
 * @param v_out        Calculated output voltage (V)
 * @param V_supply_pos Positive supply rail (V)
 * @param V_supply_neg Negative supply rail (V)
 * @param V_headroom   Output voltage headroom from rail (V)
 * @return             0 if saturated, 1 if linear
 */
static inline int opamp_check_linear_region(double v_out, double V_supply_pos,
                                             double V_supply_neg, double V_headroom) {
    if (v_out > V_supply_pos - V_headroom) return 0;
    if (v_out < V_supply_neg + V_headroom) return 0;
    return 1;
}

/**
 * opamp_slew_rate_limit - Check if output can track desired signal.
 *
 * Maximum frequency for full-swing sine wave of amplitude V_pk:
 *   f_max = SR / (2*pi * V_pk)
 *
 * This is the full-power bandwidth (FPBW).
 *
 * @param SR     Slew rate (V/s)
 * @param V_pk   Peak amplitude (V)
 * @param f_sig  Signal frequency (Hz)
 * @return       1 if signal can be tracked, 0 if slew-rate limited
 *
 * Reference: Sedra & Smith S2.8.3 (Slew rate)
 */
static inline int opamp_check_slew_rate(double SR, double V_pk, double f_sig) {
    double required_SR = 2.0 * M_PI * f_sig * V_pk;
    return (required_SR <= SR) ? 1 : 0;
}

/**
 * compute_iref_bandgap - Ideal bandgap voltage reference computation.
 *
 * Bandgap reference principle (Widlar 1971, Brokaw 1974):
 *   V_ref = V_BE + K * V_T
 *
 * where V_BE has negative TC (~ -2 mV/C), V_T = kT/q has positive TC (~ +0.085 mV/C),
 * and K is chosen such that dV_ref/dT = 0 at T = 300K.
 *
 * k = 1.380649e-23 J/K, q = 1.602176634e-19 C
 *
 * @param V_BE    Base-emitter voltage (V) at reference temperature
 * @param T       Temperature (K)
 * @param K       Scaling factor for V_T
 * @return        Bandgap reference voltage (V)
 */
static inline double compute_iref_bandgap(double V_BE, double T, double K) {
    const double k = 1.380649e-23;
    const double q = 1.602176634e-19;
    double V_T = k * T / q;
    return V_BE + K * V_T;
}

/*============================================================================
 * L5: ALGORITHMS/METHODS - Basic Op-Amp Circuit Analysis
 *============================================================================*/

/**
 * opamp_output_noise_rti - Output noise referred to input.
 *
 * For a given noise gain NG = 1/beta:
 *   e_n_out = e_n_in * NG
 *
 * @param e_n_in  Input-referred noise density (V/sqrt(Hz))
 * @param NG      Noise gain (V/V)
 * @return        Output-referred noise density (V/sqrt(Hz))
 */
static inline double opamp_output_noise_rti(double e_n_in, double NG) {
    return e_n_in * NG;
}

/*============================================================================
 * L6: CANONICAL PROBLEMS - Standard Op-Amp Transfer Functions
 *============================================================================*/

/**
 * opamp_inverting_transfer - Inverting amplifier closed-loop gain.
 *
 * With finite A_ol:
 *   v_out/v_in = -(R_f/R_i) * A_ol*beta / (1 + A_ol*beta)
 *   where beta = R_i / (R_i + R_f)
 *
 * Ideal case (A_ol -> infinity): v_out/v_in = -R_f/R_i
 *
 * @param R_f         Feedback resistor (Ohm)
 * @param R_i         Input resistor (Ohm)
 * @param A_ol_finite Finite open-loop gain (V/V)
 * @return            Closed-loop gain (V/V)
 *
 * Reference: Sedra & Smith S2.2 (The Inverting Configuration)
 */
static inline double opamp_inverting_transfer(double R_f, double R_i, double A_ol_finite) {
    if (R_i <= 0.0) return -A_ol_finite;
    double beta = R_i / (R_i + R_f);
    double A_ideal = -R_f / R_i;
    double loop_gain = A_ol_finite * beta;
    return A_ideal * loop_gain / (1.0 + loop_gain);
}

/**
 * opamp_non_inverting_transfer - Non-inverting amplifier closed-loop gain.
 *
 * With finite A_ol:
 *   v_out/v_in = (1 + R_f/R_g) * A_ol*beta / (1 + A_ol*beta)
 *   where beta = R_g / (R_g + R_f)
 *
 * @param R_f         Feedback resistor (Ohm)
 * @param R_g         Ground resistor (Ohm)
 * @param A_ol_finite Finite open-loop gain (V/V)
 * @return            Closed-loop gain (V/V)
 *
 * Reference: Sedra & Smith S2.3 (The Noninverting Configuration)
 */
static inline double opamp_non_inverting_transfer(double R_f, double R_g,
                                                    double A_ol_finite) {
    if (R_g <= 0.0) return A_ol_finite / (1.0 + A_ol_finite);
    double beta = R_g / (R_g + R_f);
    double A_ideal = 1.0 + R_f / R_g;
    double loop_gain = A_ol_finite * beta;
    return A_ideal * loop_gain / (1.0 + loop_gain);
}

/**
 * opamp_differential_transfer - Differential amplifier output.
 *
 *   v_out = (R_f/R_i) * (v2 - v1)   (ideal, matched resistors)
 *
 * With mismatched resistors R1=R3, R2=R4:
 *   v_out = (R2/R1)*(v2 - v1) + v_cm * CMRR_error
 *
 * @param R_f         Feedback resistor (Ohm)
 * @param R_i         Input resistor (Ohm)
 * @param v1          Non-inverting input voltage (V)
 * @param v2          Inverting input voltage (V)
 * @param A_ol_finite Finite open-loop gain (V/V)
 * @return            Output voltage (V)
 *
 * Reference: Sedra & Smith S2.4 (The Difference Amplifier)
 */
static inline double opamp_differential_transfer(double R_f, double R_i,
                                                   double v1, double v2,
                                                   double A_ol_finite) {
    if (R_i <= 0.0) return 0.0;
    /* Ideal differential gain with finite A_ol correction */
    double A_d_ideal = R_f / R_i;
    double v_diff = v2 - v1;
    /* Common-mode rejection due to finite CMRR is handled at higher level */
    (void)v1; /* v1 used in v_diff */
    (void)v2; /* v2 used in v_diff */
    double beta = R_i / (R_i + R_f);
    double loop_gain = A_ol_finite * beta;
    double correction = loop_gain / (1.0 + loop_gain);
    return A_d_ideal * v_diff * correction;
}

/*============================================================================
 * Module function declarations (non-inline, implemented in src/)
 *============================================================================*/

/* L1: Initialization & validation */
OpAmpParams opamp_params_init_default(void);
int         opamp_params_validate(const OpAmpParams *p);
void        opamp_params_print(const OpAmpParams *p);

/* L2: Ideal op-amp analysis */
double opamp_ideal_virtual_short_voltage(double v_in, double R_f, double R_g);
int    opamp_ideal_circuit_solve(double *v_nodes, const double *R_values,
                                  const int *topology, int n_nodes);

/* L3: Multi-pole transfer function evaluation */
double opamp_multi_pole_gain_mag(const OpAmpPoleZeroModel *model, double f);
double opamp_multi_pole_gain_phase(const OpAmpPoleZeroModel *model, double f);
void   opamp_multi_pole_bode_data(const OpAmpPoleZeroModel *model,
                                   double f_start, double f_stop,
                                   int n_points,
                                   double *freq, double *mag_db, double *phase_deg);

/* L4: Stability and compensation assessment */
double opamp_loop_gain(double A_ol, double beta);
double opamp_phase_margin(const OpAmpPoleZeroModel *model, double beta);
int    opamp_stability_check(const OpAmpPoleZeroModel *model, double beta);

/* L5: Noise analysis */
double opamp_total_input_noise(const OpAmpNoiseModel *noise, double R_s,
                                double f_low, double f_high);
double opamp_noise_figure(const OpAmpNoiseModel *noise, double R_s, double f);

#endif /* OPAMP_BASIC_H */
