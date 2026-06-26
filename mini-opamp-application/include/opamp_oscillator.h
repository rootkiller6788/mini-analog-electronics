#ifndef OPAMP_OSCILLATOR_H
#define OPAMP_OSCILLATOR_H

/**
 * opamp_oscillator.h - Sinusoidal and Relaxation Oscillators with Op-Amps
 *
 * Covers oscillator circuits that generate periodic waveforms using
 * operational amplifiers with positive feedback and frequency-selective
 * networks.
 *
 * Knowledge Coverage:
 *   L1 (Definitions): OscillatorConfig, WienBridge, PhaseShift structs
 *   L2 (Core Concepts): Barkhausen criterion, frequency-selective feedback
 *   L3 (Math Structures): Positive feedback loop analysis, root locus
 *   L4 (Fundamental Laws): Barkhausen stability/oscillation criterion
 *   L5 (Algorithms/Methods): Oscillator design for specific frequency/amplitude
 *   L6 (Canonical Problems): Wien bridge, phase-shift, quadrature oscillators
 *   L7 (Applications): Function generator, clock generation, lock-in amplifier ref
 *   L8 (Advanced Topics): Amplitude stabilization, DDFS, injection locking
 *
 * Reference: Sedra & Smith Ch.17, Franco Ch.13
 *            Horowitz & Hill "The Art of Electronics" Ch.5
 *            Carter & Brown "Handbook of Operational Amplifier Applications" (TI)
 *
 * Course: MIT 6.002, Berkeley EE105, Stanford EE101, TUM EI
 */

#include "opamp_basic.h"
#include "opamp_nonlinear.h"

/*============================================================================
 * L1: DEFINITIONS - Oscillator Types and Parameters
 *============================================================================*/

/** Oscillator topology types */
typedef enum {
    OSC_WIEN_BRIDGE = 0,        /**< Wien bridge (RC) oscillator */
    OSC_PHASE_SHIFT,            /**< Phase-shift oscillator */
    OSC_QUADRATURE,             /**< Quadrature (sine/cosine) oscillator */
    OSC_TWIN_T,                 /**< Twin-T notch oscillator */
    OSC_BUBBA,                  /**< Bubba oscillator (ultra-low distortion) */
    OSC_COUNT
} OscillatorType;

/**
 * OscillatorConfig - Oscillator design parameters.
 *
 * All sinusoidal oscillators satisfy the Barkhausen criterion:
 *   |A(jw) * beta(jw)| = 1  (magnitude condition)
 *   phase(A(jw) * beta(jw)) = 0 (or 360 deg)  (phase condition)
 *
 * At exactly one frequency w_0 where both conditions hold, oscillation
 * is sustained.
 */
typedef struct {
    OscillatorType type;        /**< Oscillator topology */
    double f_osc;               /**< Oscillation frequency (Hz) */
    double w_osc;               /**< Oscillation frequency (rad/s) */
    double V_amplitude;         /**< Output amplitude (V) */
    double THD;                 /**< Total harmonic distortion (fraction) */
    double startup_time;        /**< Startup time estimate (s) */
    /* Wien bridge specific */
    double R_freq;              /**< Frequency-setting R (Ohm) */
    double C_freq;              /**< Frequency-setting C (F) */
    double R_neg_fb;            /**< Negative feedback R (Ohm) for gain control */
    double R_pos_fb;            /**< Positive feedback R (Ohm) */
    /* Phase shift specific */
    double R_ps[3];             /**< Phase-shift resistors (Ohm) */
    double C_ps[3];             /**< Phase-shift capacitors (F) */
    int n_stages;               /**< Number of phase-shift stages (3 or 4) */
    /* Amplitude stabilization */
    int has_agc;                /**< 1 = amplitude stabilization used */
    double agc_threshold;       /**< AGC threshold voltage (V) */
    double agc_time_constant;   /**< AGC time constant (s) */
} OscillatorConfig;

/*============================================================================
 * L2: CORE CONCEPTS - Barkhausen Criterion
 *============================================================================*/

/**
 * barkhausen_criterion_check - Verify oscillation conditions.
 *
 * The Barkhausen criterion states that for sustained oscillations:
 *   1. |A_ol(jw_0) * beta(jw_0)| = 1  (loop gain magnitude = 1)
 *   2. angle(A_ol(jw_0) * beta(jw_0)) = 0 or 360 deg  (loop phase shift = n*360)
 *
 * In practice, the loop gain is designed slightly >1 at startup so
 * oscillations build up, and an AGC/limiter reduces it to exactly 1
 * for steady-state.
 *
 * @param A_ol_mag   Open-loop gain magnitude at w_osc
 * @param A_ol_phase Open-loop phase (deg) at w_osc
 * @param beta_mag   Feedback network magnitude at w_osc
 * @param beta_phase Feedback network phase (deg) at w_osc
 * @param tolerance  Acceptable deviation from unity (e.g., 0.05 = 5%)
 * @return           1 if criterion satisfied, 0 otherwise
 *
 * Reference: Barkhausen "Lehrbuch der Elektronen-Rohren" (1935)
 */
static inline int barkhausen_criterion_check(double A_ol_mag, double A_ol_phase,
                                               double beta_mag, double beta_phase,
                                               double tolerance) {
    double loop_mag = A_ol_mag * beta_mag;
    double loop_phase = A_ol_phase + beta_phase;
    /* Normalize phase to [0, 360) */
    while (loop_phase < 0.0) loop_phase += 360.0;
    while (loop_phase >= 360.0) loop_phase -= 360.0;
    int mag_ok = (loop_mag >= 1.0 - tolerance) && (loop_mag <= 1.0 + tolerance);
    int phase_ok = (loop_phase <= tolerance) || (loop_phase >= 360.0 - tolerance);
    return mag_ok && phase_ok;
}

/**
 * oscillation_startup_check - Verify startup condition.
 *
 * For oscillation to build from noise, the initial loop gain must
 * be greater than 1. Typical design: loop gain = 2-3 at startup.
 * As amplitude grows, nonlinearity or AGC reduces the loop gain to 1.
 *
 * @param loop_gain   |A_ol * beta| at w_osc
 * @param phase_deg   Loop phase shift at w_osc (deg)
 * @return            1 if oscillation will start, 0 otherwise
 */
static inline int oscillation_startup_check(double loop_gain, double phase_deg) {
    /* Phase must be near 0 or 360; loop gain must exceed 1 */
    double phase_norm = fmod(fabs(phase_deg), 360.0);
    int phase_ok = (phase_norm < 5.0) || (phase_norm > 355.0);
    return (loop_gain > 1.1) && phase_ok;
}

/*============================================================================
 * L3: MATHEMATICAL STRUCTURES - Frequency-Selective Networks
 *============================================================================*/

/**
 * wien_bridge_transfer - Wien bridge network transfer function.
 *
 * The Wien bridge consists of a series RC and parallel RC network:
 *   beta(s) = (R||C) / ((R+C) + (R||C))
 *           = s*R*C / (s^2*R^2*C^2 + 3*s*R*C + 1)
 *
 * At w_0 = 1/(R*C):
 *   |beta| = 1/3
 *   phase(beta) = 0
 *
 * Thus, the amplifier must provide gain = 3 for oscillation.
 *
 * @param R       Resistance (Ohm)
 * @param C       Capacitance (F)
 * @param f       Frequency (Hz)
 * @param mag     Output: |beta(j*2*pi*f)|
 * @param phase   Output: phase(beta) in degrees
 *
 * Reference: Wien "A New Type of Oscillator" (1939), Hewlett HP200A (1939)
 */
void wien_bridge_transfer(double R, double C, double f,
                           double *mag, double *phase);

/**
 * phase_shift_transfer - Phase-shift network transfer function.
 *
 * Three identical RC stages in cascade, each providing 60 degrees
 * phase shift at the oscillation frequency:
 *
 *   f_osc = 1 / (2*pi*R*C * sqrt(6))  for 3-stage
 *   beta = 1/29 at f_osc (requires amplifier gain >= 29)
 *
 * @param R       Resistance per stage (Ohm)
 * @param C       Capacitance per stage (F)
 * @param n_stages Number of stages (3 or 4)
 * @param f       Frequency (Hz)
 * @param mag     Output: |beta|
 * @param phase   Output: phase(beta) in degrees
 */
void phase_shift_transfer(double R, double C, int n_stages, double f,
                           double *mag, double *phase);

/*============================================================================
 * L4: FUNDAMENTAL LAWS - Sustained Oscillation
 *============================================================================*/

/**
 * minimum_gain_for_oscillation - Compute required amplifier gain.
 *
 * The amplifier gain must compensate for the attenuation of the
 * frequency-selective feedback network at w_osc.
 *
 * Wien bridge:   A_min = 3
 * Phase-shift (3): A_min = 29
 * Phase-shift (4): A_min = 18.4 (approx)
 *
 * If A < A_min: no oscillation (damped).
 * If A > A_min: amplitude grows until clipping/AGC (distorted sine).
 * If A = A_min exactly: sustained sine wave (unstable, requires AGC).
 *
 * @param type    Oscillator type
 * @return        Minimum amplifier gain for oscillation
 */
double minimum_gain_for_oscillation(OscillatorType type);

/*============================================================================
 * L5: ALGORITHMS/METHODS - Oscillator Design
 *============================================================================*/

/**
 * wien_bridge_design - Design a Wien bridge oscillator.
 *
 * Design steps:
 *   1. Choose C (typically 1 nF - 1 uF) based on frequency range
 *   2. Compute R = 1/(2*pi*f_osc*C)
 *   3. Set amplifier gain slightly > 3 for startup: R_neg_fb/R_pos_fb > 2
 *   4. Add amplitude stabilization (diode limiter, JFET AGC, or thermistor)
 *
 * For ultra-low distortion (< 0.001%), use a thermistor or JFET AGC
 * (e.g., Hewlett-Packard HP200A oscillator — Bill Hewlett's 1939 thesis).
 *
 * @param cfg           Oscillator config (f_osc must be set)
 * @param opamp         Op-amp parameters
 * @return              1 on success, 0 if infeasible
 *
 * Reference: Hewlett "A New Type Resistance-Capacity Oscillator" (1939)
 *            Williams "A Linear Thermistor AGC Wien Bridge Oscillator" (TI)
 */
int wien_bridge_design(OscillatorConfig *cfg, const OpAmpParams *opamp);

/**
 * phase_shift_oscillator_design - Design a phase-shift oscillator.
 *
 * Design steps:
 *   1. Choose C based on f_osc and available values
 *   2. Compute R = 1/(2*pi*f_osc*C*sqrt(6)) for 3-stage
 *   3. Set R_neg_fb >= 29 * R for 3-stage (amplifier gain >= 29)
 *   4. Verify op-amp GBW sufficient: GBW >> f_osc * gain
 *
 * Phase-shift oscillators are simpler than Wien bridge but have higher THD
 * (typically 1-5% without AGC).
 *
 * @param cfg           Oscillator config (f_osc and n_stages set)
 * @param opamp         Op-amp parameters
 * @return              1 on success
 */
int phase_shift_oscillator_design(OscillatorConfig *cfg, const OpAmpParams *opamp);

/**
 * quadrature_oscillator_design - Design a quadrature oscillator.
 *
 * Produces both sine and cosine outputs simultaneously:
 *   v_sine(t) = V_pk * sin(w_osc * t)
 *   v_cosine(t) = V_pk * cos(w_osc * t)
 *
 * Uses two integrators in a loop, providing exactly 90 degrees phase shift
 * and the oscillation condition is inherently satisfied.
 *
 * @param cfg           Oscillator config (f_osc must be set)
 * @param opamp         Op-amp parameters
 * @return              1 on success
 *
 * Reference: Franco S13.4 (Quadrature Oscillators)
 */
int quadrature_oscillator_design(OscillatorConfig *cfg, const OpAmpParams *opamp);

/*============================================================================
 * L6: CANONICAL PROBLEMS - Oscillator Waveforms
 *============================================================================*/

/**
 * oscillator_output_sample - Sample oscillator output at time t.
 *
 * Generic function to compute oscillator output waveform.
 * For Wien bridge and phase-shift: v_out(t) = V_amplitude * sin(2*pi*f_osc * t)
 * For quadrature: also provides v_cos(t)
 *
 * @param cfg      Oscillator configuration
 * @param t        Time (s)
 * @param v_sin    Output: sine output (V)
 * @param v_cos    Output: cosine output (V), valid for quadrature only
 */
void oscillator_output_sample(const OscillatorConfig *cfg, double t,
                               double *v_sin, double *v_cos);

/**
 * oscillator_thd_estimate - Estimate total harmonic distortion.
 *
 * THD is estimated based on the excess loop gain and AGC quality.
 * Without AGC: THD is dominated by clipping (5-10%)
 * With diode AGC: THD ~ 0.1-1%
 * With JFET/thermistor AGC: THD < 0.01%
 *
 * @param cfg      Oscillator configuration
 * @return         Estimated THD (fraction, 0.01 = 1%)
 */
double oscillator_thd_estimate(const OscillatorConfig *cfg);

/**
 * oscillator_startup_time - Estimate time for oscillation to reach 90% amplitude.
 *
 * tau_startup ~ Q / (pi * f_osc * (loop_gain - 1))
 * where Q is the effective Q of the frequency-selective network.
 *
 * Wien bridge: Q = 1/3 (low Q, fast startup ~ 10-100 cycles)
 * Phase-shift: Q ~ 0.5 (moderate)
 * Quadrature: Q ~ 0.707
 *
 * @param cfg      Oscillator configuration
 * @return         Startup time estimate (seconds)
 */
double oscillator_startup_time(const OscillatorConfig *cfg);

#endif /* OPAMP_OSCILLATOR_H */
