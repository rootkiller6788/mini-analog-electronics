/**
 * opamp_nonlinear.c - Nonlinear Op-Amp Circuit Implementations
 *
 * Implements comparator, Schmitt trigger, precision rectifier,
 * peak detector, logarithmic amplifier, and waveform generator circuits.
 *
 * Coverage: L1-L6 (Definitions through Canonical Problems)
 */

#include "opamp_nonlinear.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/*============================================================================
 * L3: HYSTERESIS ANALYSIS
 *============================================================================*/

/**
 * schmitt_thresholds - Compute upper and lower threshold voltages.
 *
 * For a non-inverting Schmitt trigger with hysteresis:
 *
 *   V_plus = V_out * R2/(R1+R2) + V_ref * R1/(R1+R2)
 *
 * When V_out = V_OH: V_TH = V_ref * R1/(R1+R2) + V_OH * R2/(R1+R2)
 * When V_out = V_OL: V_TL = V_ref * R1/(R1+R2) + V_OL * R2/(R1+R2)
 *
 * Hysteresis band:
 *   V_H = V_TH - V_TL = (V_OH - V_OL) * R2/(R1+R2)
 *
 * For an inverting Schmitt trigger, swap the roles.
 * The positive feedback creates the hysteresis that prevents
 * chatter when the input is near the threshold with noise.
 *
 * TYPICAL DESIGN VALUES:
 *   R1 = 10k to 100k Ohm (to limit current from output)
 *   R2 chosen for desired hysteresis band
 *   V_ref = mid-supply or 0V for symmetric thresholds
 *
 * @param cfg   Comparator config with R1, R2, V_ref, V_OH, V_OL set
 *              On output: V_TH and V_TL are computed
 *
 * Reference: Sedra & Smith S17.2 (Schmitt Trigger)
 *            Horowitz & Hill "Art of Electronics" S4.5
 */
void schmitt_thresholds(ComparatorConfig *cfg) {
    if (!cfg) return;

    double R1 = cfg->R1;
    double R2 = cfg->R2;

    if (R1 <= 0.0 || R2 <= 0.0) {
        /* Open-loop comparator: thresholds equal reference */
        cfg->V_TH = cfg->V_ref;
        cfg->V_TL = cfg->V_ref;
        return;
    }

    double total_R = R1 + R2;
    cfg->V_TH = cfg->V_ref * R1 / total_R + cfg->V_OH * R2 / total_R;
    cfg->V_TL = cfg->V_ref * R1 / total_R + cfg->V_OL * R2 / total_R;
}

/**
 * comparator_design_hysteresis - Design comparator with specified thresholds.
 *
 * Given desired threshold voltages, compute the required resistor ratio.
 *
 * From the threshold equations:
 *   V_TH - V_TL = (V_OH - V_OL) * R2/(R1+R2)
 *
 * Therefore:
 *   R2/R1 = (V_TH - V_TL) / (V_OH - V_OL - (V_TH - V_TL))
 *
 * Choose R1 = 100k (typical), compute R2.
 *
 * @param cfg           Comparator config (V_TH_desired, V_TL_desired, V_OH, V_OL used)
 * @param V_TH_desired  Desired upper threshold (V)
 * @param V_TL_desired  Desired lower threshold (V)
 */
void comparator_design_hysteresis(ComparatorConfig *cfg,
                                   double V_TH_desired, double V_TL_desired) {
    if (!cfg) return;

    double V_H = V_TH_desired - V_TL_desired;
    double V_swing = cfg->V_OH - cfg->V_OL;

    if (V_H <= 0.0 || V_swing <= V_H) {
        cfg->has_hysteresis = 0;
        cfg->V_TH = cfg->V_ref;
        cfg->V_TL = cfg->V_ref;
        return;
    }

    cfg->has_hysteresis = 1;
    cfg->V_TH = V_TH_desired;
    cfg->V_TL = V_TL_desired;

    /* Ratio R2/R1 = V_H / (V_swing - V_H) */
    double ratio = V_H / (V_swing - V_H);

    /* Choose R1 = 100k, compute R2 */
    cfg->R1 = 100000.0;
    cfg->R2 = cfg->R1 * ratio;

    /* Center reference voltage */
    /* For symmetric thresholds: V_ref = (V_TH + V_TL)/2 when R1 = R2 */
    cfg->V_ref = (V_TH_desired + V_TL_desired) / 2.0;
}

/**
 * schmitt_trigger_output - Compute Schmitt trigger output for a given input.
 *
 * The Schmitt trigger has memory: the output depends not only on the
 * current input but also on the previous output state.
 *
 * TRANSITION RULES:
 *   If input > V_TH: output goes to V_OH (regardless of previous state)
 *   If input < V_TL: output goes to V_OL (regardless of previous state)
 *   If V_TL < input < V_TH: output stays at previous state
 *
 * This hysteresis creates noise immunity. Any noise smaller than
 * V_H = V_TH - V_TL cannot cause false triggering.
 *
 * @param cfg          Comparator config with thresholds computed
 * @param v_in         Current input voltage (V)
 * @param prev_output  Previous output voltage (V)
 * @return             New output voltage (V)
 */
double schmitt_trigger_output(const ComparatorConfig *cfg, double v_in,
                               double prev_output) {
    if (!cfg) return 0.0;

    if (!cfg->has_hysteresis) {
        /* Simple comparator without hysteresis */
        return (v_in > cfg->V_ref) ? cfg->V_OH : cfg->V_OL;
    }

    if (v_in > cfg->V_TH) {
        return cfg->V_OH;  /* Crossed upper threshold: go high */
    } else if (v_in < cfg->V_TL) {
        return cfg->V_OL;  /* Crossed lower threshold: go low */
    } else {
        /* In hysteresis band: maintain previous state */
        return prev_output;
    }
}

/**
 * window_comparator_in_window - Check if input is within voltage window.
 *
 * Window comparator truth table:
 *   V_TL < v_in < V_TH -> output HIGH (in window)
 *   Otherwise          -> output LOW  (out of window)
 *
 * Applications:
 *   - Over/under voltage detection
 *   - Temperature window alarm
 *   - Go/no-go testing in production
 *   - Li-ion battery charging window (3.0V - 4.2V)
 *
 * @param cfg     Window comparator config
 * @param v_in    Input voltage (V)
 * @return        1 if in window, 0 if out of window
 */
int window_comparator_in_window(const WindowComparatorConfig *cfg, double v_in) {
    if (!cfg) return 0;
    return (v_in >= cfg->V_TH_L && v_in <= cfg->V_TH_H) ? 1 : 0;
}

/*============================================================================
 * L4: POSITIVE FEEDBACK ANALYSIS
 *============================================================================*/

/**
 * positive_feedback_condition - Check if regenerative switching occurs.
 *
 * For a Schmitt trigger to have clean switching, the positive feedback
 * must overcome the negative feedback at the switching point.
 *
 * The condition for regenerative switching (latching):
 *   A_ol * beta_pos > 1  at the switching threshold
 *
 * where beta_pos = R2/(R1+R2) is the positive feedback factor.
 *
 * For the LM741 (A_ol = 200,000): beta_pos needs to be only > 5e-6.
 * For an open-loop comparator (beta_pos = 1, no resistive divider),
 *   this condition is always wildly satisfied — the output slams to
 *   the rail almost instantly.
 *
 * The speed of switching is determined by the op-amp's slew rate:
 *   t_switch = (V_OH - V_OL) / SR
 *
 * For LM741: t_switch = 26V / 0.5e6 V/s = 52 us
 * For modern comparator (e.g., LM311): t_switch ~ 200 ns
 *
 * @param A_ol      Open-loop gain
 * @param beta_pos  Positive feedback factor
 * @param v_in      Input voltage
 * @param V_ref     Reference voltage
 * @return          1 if regenerative switching occurs
 */
int positive_feedback_condition(double A_ol, double beta_pos,
                                 double v_in, double V_ref) {
    (void)v_in;   /* Reserved: threshold-proximity analysis */
    (void)V_ref;  /* Reserved: offset/centering */
    /* Regenerative switching: loop gain > 1 at threshold crossing */
    double loop_gain = A_ol * beta_pos;
    return (loop_gain > 1.0) ? 1 : 0;
}

/*============================================================================
 * L5: PRECISION RECTIFIER
 *============================================================================*/

/**
 * precision_hw_rectifier - Precision half-wave rectifier output.
 *
 * The classic precision half-wave rectifier places a diode inside the
 * op-amp's feedback loop, effectively dividing the diode's forward
 * voltage drop by the open-loop gain:
 *
 *   V_f_effective = V_f / A_ol
 *
 * For LM741: V_f_effective = 0.7V / 200,000 = 3.5 uV!
 *
 * This allows rectification of microvolt-level signals, which is
 * essential for:
 *   - AC voltmeters (true RMS conversion)
 *   - AM demodulation
 *   - Precision signal conditioning
 *
 * @param v_in     Input voltage (V)
 * @param R_f      Feedback resistor (Ohm)
 * @param R_i      Input resistor (Ohm)
 * @param diode_Vf Diode forward voltage drop (V), ~0.7V for Si
 * @return         Rectified output (V)
 */
double precision_hw_rectifier(double v_in, double R_f, double R_i,
                               double diode_Vf) {
    (void)diode_Vf;  /* Diode drop is nearly eliminated by op-amp feedback */
    if (R_i <= 0.0) return 0.0;

    if (v_in >= 0.0) {
        /* Forward-biased: diode conducts, gain = -R_f/R_i */
        return -v_in * R_f / R_i;
    } else {
        /* Reverse-biased: diode blocks, output = 0 */
        return 0.0;
    }
}

/**
 * precision_fw_rectifier - Precision full-wave rectifier (absolute value).
 *
 * Two-stage implementation:
 *   Stage 1 (half-wave): v_hw = -|v_in| for v_in > 0, 0 for v_in < 0
 *   Stage 2 (summing):   v_out = -(v_in + 2*v_hw) = |v_in|
 *
 * Requirement: R values must be precisely matched.
 * For 1% accuracy: R tolerance < 0.5%.
 * For 0.1% accuracy: R tolerance < 0.05%.
 *
 * @param v_in   Input voltage (V)
 * @param R_f    Feedback R for stage 1 (Ohm)
 * @param R_i    Input R for stage 1 (Ohm)
 * @param R_sum  Summing R for stage 2 (Ohm)
 * @param diode_Vf Diode forward drop (V)
 * @return       |v_in| (V)
 */
double precision_fw_rectifier(double v_in, double R_f, double R_i,
                               double R_sum, double diode_Vf) {
    (void)R_f;   (void)R_i;   /* Resistor values reserved for non-ideal model */
    (void)R_sum; (void)diode_Vf;
    /* Ideal full-wave: v_out = |v_in| */
    return fabs(v_in);
}

/**
 * peak_detector_output - Update peak detector state with new sample.
 *
 * The precision peak detector captures and holds the maximum (or minimum)
 * value of a waveform. Applications:
 *   - Audio level metering (VU meter)
 *   - Envelope detection in AM receivers
 *   - Pulse height analysis in nuclear instrumentation
 *   - Automatic gain control (AGC) loops
 *
 * CIRCUIT: Op-amp buffer, diode, hold capacitor, discharge resistor.
 * The op-amp provides active charging (no diode drop loss).
 * The capacitor holds the peak. A parallel resistor provides
 * controlled discharge (droop).
 *
 * Droop rate: dV/dt = V_peak / (R_discharge * C_hold)
 * For 1% droop in 1 second with C=1uF: R = 1/(0.01*1e-6) = 100 MOhm
 *
 * @param v_in          New input sample (V)
 * @param hold_C        Hold capacitor (F)
 * @param R_discharge   Discharge resistor (Ohm), large for slow droop
 * @param dt            Time step since last sample (s)
 * @param prev_peak     Previous peak value (V), updated in place
 * @return              Current peak value (V)
 */
double peak_detector_output(double v_in, double hold_C, double R_discharge,
                             double dt, double *prev_peak) {
    if (!prev_peak) return 0.0;
    if (hold_C <= 0.0 || R_discharge <= 0.0) return fabs(v_in);

    /* Discharge: exponential decay */
    double tau = R_discharge * hold_C;
    double decay = exp(-dt / tau);
    double current_peak = (*prev_peak) * decay;

    /* Charge: capture new peak if higher */
    if (fabs(v_in) > current_peak) {
        current_peak = fabs(v_in);
    }

    *prev_peak = current_peak;
    return current_peak;
}

/*============================================================================
 * L6: LOGARITHMIC AND WAVEFORM GENERATOR CANONICAL PROBLEMS
 *============================================================================*/

/**
 * log_amplifier_output - Compute logarithmic amplifier output.
 *
 * The logarithmic amplifier exploits the exponential I-V characteristic:
 *   I_c = I_s * exp(V_BE / (n*V_T))
 *
 * Placing a BJT in the feedback path:
 *   V_out = -n*V_T * ln(V_in / (R_in * I_s))
 *
 * For V_in = 1V, R_in = 10k, I_s = 1e-14 A, n = 1, V_T = 0.02585 V:
 *   V_out = -0.02585 * ln(1/(10000*1e-14)) = -0.02585 * ln(1e10) = -0.595 V
 *
 * DYNAMIC RANGE: Limited by:
 *   Low end: op-amp offset voltage and bias current
 *   High end: BJT ohmic resistance (r_b) and high-level injection
 *   Typical: 5-6 decades (e.g., 1 mV to 10 V)
 *
 * TEMPERATURE COMPENSATION: V_T = kT/q proportional to T.
 * A temperature-compensated log amp uses a matched transistor
 * and temperature-sensitive gain to cancel the T dependence.
 *
 * @param v_in   Input voltage (V), must be > 0
 * @param cfg    Log amplifier configuration
 * @return       Output voltage (V)
 *
 * Reference: Sheingold "Nonlinear Circuits Handbook" (Analog Devices, 1976)
 */
double log_amplifier_output(double v_in, const LogAmpConfig *cfg) {
    if (!cfg || v_in <= 0.0) return 0.0;
    if (cfg->R_in <= 0.0 || cfg->I_s <= 0.0) return 0.0;

    double arg = v_in / (cfg->R_in * cfg->I_s);
    if (arg <= 0.0) return 0.0;

    /* V_out = -n*V_T * ln(V_in/(R_in*I_s)) */
    return -cfg->n * cfg->V_T * log(arg);
}

/**
 * antilog_amplifier_output - Compute antilog (exponential) amplifier output.
 *
 * Inverse of the log amplifier:
 *   V_out = -R_f * I_s * exp(V_in / (n*V_T))
 *
 * @param v_in   Input voltage (V)
 * @param cfg    Log amplifier configuration
 * @return       Output voltage (V)
 */
double antilog_amplifier_output(double v_in, const LogAmpConfig *cfg) {
    if (!cfg) return 0.0;
    if (cfg->I_s <= 0.0) return 0.0;

    double exp_arg = v_in / (cfg->n * cfg->V_T);
    /* Clamp to avoid overflow */
    if (exp_arg > 100.0) exp_arg = 100.0;

    return -cfg->R_in * cfg->I_s * exp(exp_arg);
}

/**
 * waveform_gen_design - Design triangle/square waveform generator.
 *
 * Uses an integrator and a Schmitt trigger in a loop:
 *   - Integrator output: linear ramp (triangle wave)
 *   - Schmitt trigger output: square wave
 *
 * FREQUENCY: f = 1 / (4 * R_int * C_int) * (R2_st / R1_st)
 *   (for symmetric triangle with V_TH = +V_sat * R1/R2,
 *    V_TL = -V_sat * R1/R2)
 *
 * AMPLITUDE: V_pp(triangle) = 2 * V_sat * (R1_st / R2_st)
 *
 * DUTY CYCLE: 50% for symmetric design.
 *   Asymmetric: use different charge/discharge paths with diodes.
 *
 * @param cfg           Waveform generator config
 * @param target_freq   Desired frequency (Hz)
 * @param target_V_pp   Desired peak-to-peak triangle amplitude (V)
 *
 * Reference: Franco S13.6 (Function Generators)
 *            Carter & Brown "Handbook of Op Amp Applications" (TI)
 */
void waveform_gen_design(WaveformGenConfig *cfg, double target_freq,
                          double target_V_pp) {
    if (!cfg) return;

    cfg->freq = target_freq;
    cfg->V_pp = target_V_pp;

    /* Choose C_int = 10 nF */
    cfg->C_int = 10.0e-9;

    /* R1/R2 = V_pp / (2*V_sat) */
    double ratio = target_V_pp / (2.0 * cfg->V_sat);
    if (ratio <= 0.0 || ratio >= 1.0) ratio = 0.5;

    /* Choose R1 = 10k, compute R2 */
    cfg->R1_st = 10000.0;
    cfg->R2_st = cfg->R1_st / ratio;

    /* f = 1/(4*R_int*C_int) * (R2/R1) => R_int = (R2/R1) / (4*f*C_int) */
    cfg->R_int = (cfg->R2_st / cfg->R1_st) / (4.0 * target_freq * cfg->C_int);

    cfg->duty_cycle = 0.5;  /* Symmetric */
}

/**
 * waveform_gen_triangle_sample - Sample triangle wave at time t.
 *
 * The triangle wave is periodic with period T = 1/f.
 * It rises linearly from -V_pk to +V_pk over T/2, then
 * falls back to -V_pk over the next T/2.
 *
 * v_tri(t) = 2*V_pk * (2*f*t mod 1 - 0.5) * 2  (simplified)
 * v_sq(t)  = V_sat * sign(sin(2*pi*f*t))       (square wave)
 *
 * @param cfg     Waveform generator config
 * @param t       Time (s)
 * @param v_tri   Output: triangle voltage (V)
 * @param v_sq    Output: square voltage (V)
 */
void waveform_gen_triangle_sample(const WaveformGenConfig *cfg,
                                   double t, double *v_tri, double *v_sq) {
    if (!cfg || !v_tri || !v_sq || cfg->freq <= 0.0) return;

    double period = 1.0 / cfg->freq;
    double phase = fmod(t, period) / period;  /* 0 to 1 */
    double V_pk = cfg->V_pp / 2.0;

    /* Triangle wave: linear ramp up then down */
    if (phase < 0.5) {
        /* Rising edge: -V_pk to +V_pk */
        *v_tri = -V_pk + 4.0 * V_pk * phase;
        *v_sq = cfg->V_sat;
    } else {
        /* Falling edge: +V_pk to -V_pk */
        *v_tri = 3.0 * V_pk - 4.0 * V_pk * phase;
        *v_sq = -cfg->V_sat;
    }
}
