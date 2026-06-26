#ifndef OPAMP_NONLINEAR_H
#define OPAMP_NONLINEAR_H

/**
 * opamp_nonlinear.h - Nonlinear Op-Amp Application Circuits
 *
 * Covers circuits where the op-amp operates outside its linear region,
 * including comparators, Schmitt triggers, precision rectifiers,
 * peak detectors, log/antilog amplifiers, and waveform generators.
 *
 * Knowledge Coverage:
 *   L1 (Definitions): Comparator hysteresis, precision rectifier modes
 *   L2 (Core Concepts): Positive feedback, regenerative switching, threshold detection
 *   L3 (Math Structures): Hysteresis loop analysis, threshold equations
 *   L4 (Fundamental Laws): Positive feedback principle, Barkhausen criterion
 *   L5 (Algorithms/Methods): Hysteresis design, precision rectification
 *   L6 (Canonical Problems): Zero-crossing detector, window comparator
 *
 * Reference: Sedra & Smith Ch.17, Franco Ch.12,
 *            Horowitz & Hill "The Art of Electronics" Ch.4
 *            Carter & Brown "Handbook of Operational Amplifier Applications" (TI)
 *
 * Course: MIT 6.002, Berkeley EE105, Stanford EE101, Georgia Tech ECE 3042
 */

#include "opamp_basic.h"

/*============================================================================
 * L1: DEFINITIONS - Nonlinear Circuit Structures
 *============================================================================*/

/**
 * ComparatorConfig - Comparator / Schmitt trigger configuration.
 *
 * A comparator compares two input voltages and outputs a binary result.
 * Adding hysteresis creates a Schmitt trigger with noise immunity.
 *
 * V_OH = output high, V_OL = output low (saturation voltages)
 * V_TH = upper threshold, V_TL = lower threshold
 * Hysteresis = V_TH - V_TL
 */
typedef struct {
    int has_hysteresis;         /**< 1 = Schmitt trigger, 0 = open-loop comparator */
    double V_ref;               /**< Reference voltage (V) */
    double R1;                  /**< Feedback resistor from output (Ohm) */
    double R2;                  /**< Resistor to reference (Ohm) */
    double V_OH;                /**< Output high voltage (V) */
    double V_OL;                /**< Output low voltage (V) */
    double V_TH;                /**< Upper threshold (V), computed */
    double V_TL;                /**< Lower threshold (V), computed */
} ComparatorConfig;

/**
 * PrecisionRectifierConfig - Precision (super) diode configuration.
 *
 * Standard diode rectifiers have ~0.7V forward drop, making them
 * useless for sub-volt signals. Precision rectifiers use op-amps
 * to eliminate the diode drop.
 *
 * Configurations:
 *   Half-wave: rectifies one polarity only
 *   Full-wave: |v_in| output (absolute value)
 *   Precision peak detector: captures peak value
 */
typedef struct {
    int mode;                   /**< 0=half-wave, 1=full-wave, 2=peak detector */
    double R1, R2, R_f;         /**< Resistor values (Ohm) */
    double diode_Vf;            /**< Diode forward voltage drop (V) */
    double hold_C;              /**< Hold capacitor for peak detector (F) */
    double droop_rate;          /**< Droop rate of peak detector (V/s) */
} PrecisionRectifierConfig;

/**
 * LogAmpConfig - Logarithmic amplifier configuration.
 *
 * Uses the exponential I-V characteristic of a diode/BJT:
 *   I_d = I_s * (exp(V_d/(n*V_T)) - 1)
 *   => V_out = -n*V_T * ln(V_in / (R_in*I_s))
 *
 * Typically 1-6 decades of dynamic range (e.g., 1 mV to 10 V).
 *
 * Reference: Sheingold "Nonlinear Circuits Handbook" (Analog Devices, 1976)
 */
typedef struct {
    double R_in;                /**< Input resistor (Ohm) */
    double I_s;                 /**< Saturation current of logging transistor (A) */
    double n;                   /**< Ideality factor (1-2) */
    double V_T;                 /**< Thermal voltage kT/q (V) at 300K = 0.02585 */
    double temp_C;              /**< Operating temperature (Celsius) */
    double scale_factor;        /**< K = n*V_T for V_out = -K * ln(V_in/(R_in*I_s)) */
} LogAmpConfig;

/**
 * WaveformGenConfig - Waveform generator configuration.
 *
 * Triangle/square wave generator using op-amp integrator + Schmitt trigger.
 *
 * Frequency: f = 1/(4*R*C) * (R2/R1) for symmetric triangle wave
 * Amplitude: V_pp = 2 * V_sat * (R1/R2)
 */
typedef struct {
    double R_int;               /**< Integrator resistor (Ohm) */
    double C_int;               /**< Integrator capacitor (F) */
    double R1_st;               /**< Schmitt trigger R1 (feedback) (Ohm) */
    double R2_st;               /**< Schmitt trigger R2 (to gnd/ref) (Ohm) */
    double V_sat;               /**< Op-amp saturation voltage (V) */
    double freq;                /**< Oscillation frequency (Hz) */
    double V_pp;                /**< Peak-to-peak amplitude (V) */
    double duty_cycle;          /**< Duty cycle (0.5 = symmetric) */
} WaveformGenConfig;

/**
 * WindowComparatorConfig - Window comparator (dual-threshold) config.
 *
 * Detects when input voltage is within a voltage window.
 * Uses two comparators and AND logic.
 *
 * Output is HIGH when V_TH_L < V_in < V_TH_H.
 */
typedef struct {
    double V_TH_L;              /**< Lower threshold (V) */
    double V_TH_H;              /**< Upper threshold (V) */
    double V_OH;                /**< Output high level (V) */
    double V_OL;                /**< Output low level (V) */
} WindowComparatorConfig;

/*============================================================================
 * L2: CORE CONCEPTS - Comparator and Positive Feedback
 *============================================================================*/

/**
 * comparator_output - Determine open-loop comparator output.
 *
 * For an ideal comparator: v_out = V_OH if v_plus > v_minus, else V_OL.
 * Real comparators have finite gain and propagation delay.
 *
 * @param v_plus     Non-inverting input voltage (V)
 * @param v_minus    Inverting input voltage (V)
 * @param V_OH       Output high level (V)
 * @param V_OL       Output low level (V)
 * @return           Comparator output voltage (V)
 */
static inline double comparator_output(double v_plus, double v_minus,
                                        double V_OH, double V_OL) {
    return (v_plus > v_minus) ? V_OH : V_OL;
}

/*============================================================================
 * L3: MATHEMATICAL STRUCTURES - Hysteresis Analysis
 *============================================================================*/

/**
 * schmitt_thresholds - Compute Schmitt trigger threshold voltages.
 *
 * For non-inverting Schmitt trigger:
 *   V_TH = V_ref * (1 + R1/R2) + V_OH * (R1/R2)
 *   V_TL = V_ref * (1 + R1/R2) + V_OL * (R1/R2)
 *
 * Hysteresis band: V_H = V_TH - V_TL = (V_OH - V_OL) * (R1/R2)
 *
 * Note: R1 is from output to non-inverting input,
 *       R2 is from non-inverting input to V_ref.
 *
 * @param cfg    Comparator configuration (R1, R2, V_ref, V_OH, V_OL set)
 *               V_TH and V_TL are filled on output
 */
void schmitt_thresholds(ComparatorConfig *cfg);

/**
 * schmitt_hysteresis_band - Width of the hysteresis band.
 *
 * Application: The hysteresis band should be larger than the
 * peak-to-peak noise on the input signal to prevent chatter.
 *
 * @param cfg    Comparator configuration
 * @return       Hysteresis band V_H = V_TH - V_TL (V)
 */
static inline double schmitt_hysteresis_band(const ComparatorConfig *cfg) {
    return cfg->V_TH - cfg->V_TL;
}

/**
 * schmitt_noise_margin - Verify hysteresis exceeds noise floor.
 *
 * Rule of thumb: hysteresis >= 5 * V_noise_rms for reliable switching.
 *
 * @param cfg         Comparator configuration
 * @param V_noise_rms RMS noise at input (V)
 * @return            1 if noise margin adequate, 0 otherwise
 */
static inline int schmitt_noise_margin(const ComparatorConfig *cfg,
                                        double V_noise_rms) {
    return (schmitt_hysteresis_band(cfg) >= 5.0 * V_noise_rms) ? 1 : 0;
}

/*============================================================================
 * L4: FUNDAMENTAL LAWS - Positive Feedback Dynamics
 *============================================================================*/

/**
 * positive_feedback_condition - Check if circuit will latch.
 *
 * Positive feedback exists when the feedback signal reinforces the
 * input signal. For a Schmitt trigger:
 *   The positive feedback fraction beta_pos = R2/(R1+R2) determines
 *   the fraction of output change that reaches the + input.
 *
 * For regenerative switching:
 *   A_ol * beta_pos > 1 at the switching point
 *
 * This is the same Barkhausen criterion but for DC instead of AC.
 *
 * @param A_ol      Op-amp open-loop gain (V/V)
 * @param beta_pos  Positive feedback factor (dimensionless)
 * @param v_in      Input voltage (V)
 * @param V_ref     Reference voltage (V)
 * @return          1 if regenerative switching occurs, 0 otherwise
 */
int positive_feedback_condition(double A_ol, double beta_pos,
                                 double v_in, double V_ref);

/*============================================================================
 * L5: ALGORITHMS/METHODS - Precision Rectification
 *============================================================================*/

/**
 * precision_half_wave_output - Compute ideal precision half-wave rectifier output.
 *
 * For an ideal precision half-wave rectifier:
 *   v_out = 0     for v_in < 0
 *   v_out = -v_in for v_in >= 0  (inverting configuration)
 *
 * The op-amp eliminates the 0.7V diode drop by placing the diode
 * inside the feedback loop.
 *
 * @param v_in   Input voltage (V)
 * @param mode   0 = non-inverting, 1 = inverting
 * @return       Rectified output voltage (V)
 */
static inline double precision_half_wave_output(double v_in, int mode) {
    if (mode == 0) { /* Non-inverting half-wave: passes positive, blocks negative */
        return (v_in >= 0.0) ? v_in : 0.0;
    } else { /* Inverting half-wave: outputs -|v_in| for v_in > 0 */
        return (v_in >= 0.0) ? -v_in : 0.0;
    }
}

/**
 * precision_full_wave_output - Ideal precision full-wave rectifier.
 *
 * v_out = |v_in| (absolute value)
 *
 * Implemented typically with two op-amps:
 *   Stage 1: Half-wave rectifier
 *   Stage 2: Summing amplifier that adds 2*|half_wave| + v_in
 *
 * @param v_in        Input voltage (V)
 * @param half_wave   Output from first stage
 * @param gain        Stage 2 gain
 * @return            |v_in| (V)
 */
static inline double precision_full_wave_output(double v_in, double half_wave,
                                                  double gain) {
    (void)half_wave; (void)gain;  /* Parameters reserved for two-stage fw rectifier */
    /* Full-wave: v_out = |v_in|, typically gain=1 */
    return fabs(v_in);
}

/*============================================================================
 * L6: CANONICAL PROBLEMS - Nonlinear Circuits
 *============================================================================*/

/* Forward declarations for src/ implementations */

/* Comparator / Schmitt trigger design */
void comparator_design_hysteresis(ComparatorConfig *cfg,
                                   double V_TH_desired, double V_TL_desired);
double schmitt_trigger_output(const ComparatorConfig *cfg, double v_in,
                               double prev_output);
int window_comparator_in_window(const WindowComparatorConfig *cfg, double v_in);

/* Precision rectifier */
double precision_hw_rectifier(double v_in, double R_f, double R_i, double diode_Vf);
double precision_fw_rectifier(double v_in, double R_f, double R_i,
                               double R_sum, double diode_Vf);
double peak_detector_output(double v_in, double hold_C, double R_discharge,
                             double dt, double *prev_peak);

/* Logarithmic amplifier */
double log_amplifier_output(double v_in, const LogAmpConfig *cfg);
double antilog_amplifier_output(double v_in, const LogAmpConfig *cfg);

/* Waveform generator */
void waveform_gen_design(WaveformGenConfig *cfg, double target_freq, double target_V_pp);
void waveform_gen_triangle_sample(const WaveformGenConfig *cfg,
                                   double t, double *v_tri, double *v_sq);

#endif /* OPAMP_NONLINEAR_H */
