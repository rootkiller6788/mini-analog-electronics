/**
 * bjt_small_signal.h ? BJT Small-Signal AC Analysis
 *
 * Small-signal analysis linearizes the BJT around its DC Q-point, enabling
 * gain and impedance calculations using linear circuit theory. This is the
 * core of amplifier design: once biased, the transistor is replaced by its
 * small-signal model, and standard linear analysis (KVL, KCL, superposition)
 * yields closed-form expressions for gain, input/output impedance.
 *
 * The three fundamental amplifier configurations (CE, CC, CB) each have
 * distinct small-signal properties optimized for different applications.
 *
 * Reference: Sedra & Smith, Ch. 7; Gray & Meyer, Ch. 4
 *            Middlebrook, "Design-Oriented Circuit Analysis"
 *
 * Knowledge Levels:
 *   L2: Small-signal equivalent circuits, gain/impedance definitions
 *   L3: Linear network analysis, two-port representations
 *   L4: Miller Theorem, impedance reflection rules
 *   L5: Gain calculation algorithms, impedance analysis methods
 *   L6: CE/CC/CB canonical amplifier analysis
 */

#ifndef BJT_SMALL_SIGNAL_H
#define BJT_SMALL_SIGNAL_H

#include "bjt_model.h"

/* ---- L2: Common-Emitter (CE) Amplifier Analysis ---- */

/* CE amplifier voltage gain (no emitter degeneration, RE bypassed).
 * Av = -gm * (RC || RL || ro) * (rpi / (Rs + rpi))
 * The negative sign indicates 180-degree phase shift.
 * If RL is large (open), RL_eff = RC || ro.
 * Complexity: O(1). */
double bjt_ce_voltage_gain(const bjt_ss_params_t *ss,
                            double RC, double RL, double Rs);

/* CE amplifier voltage gain with emitter degeneration (unbypassed RE).
 * Adding RE reduces gain but improves linearity and bandwidth.
 * Av ? - (RC || RL) / (re + RE) ? - (RC || RL) / RE  for RE >> re.
 * Exact formula with Early effect:
 *   Av = -gm*(RC||RL||ro) / (1 + gm*RE + (RC||RL||ro)/ro + RE/re)
 *
 * This is the trade-off: lower gain for better linearity.
 * Reference: Sedra & Smith, Eq. (7.117). */
double bjt_ce_voltage_gain_degenerated(const bjt_ss_params_t *ss,
                                        double RC, double RL, double RE);

/* CE amplifier input impedance (looking into base).
 * Without degeneration: Rin = rpi (for CE, base input).
 * With source resistance: Rin_total = rpi.
 * Complexity: O(1). */
double bjt_ce_input_impedance(const bjt_ss_params_t *ss);

/* CE amplifier input impedance with emitter degeneration.
 * Rin = rpi + (beta + 1) * RE
 * The emitter resistance is reflected to the base multiplied by (beta+1).
 * This is the "impedance reflection rule" ? a core BJT concept.
 * Complexity: O(1). */
double bjt_ce_input_impedance_degenerated(const bjt_ss_params_t *ss,
                                           double RE);

/* CE amplifier output impedance (looking into collector).
 * Without load: Rout = RC || ro.
 * With Early effect: Rout ? ro for high RC.
 * Complexity: O(1). */
double bjt_ce_output_impedance(const bjt_ss_params_t *ss, double RC);

/* CE amplifier output impedance with emitter degeneration.
 * Degeneration increases output impedance:
 *   Rout = ro * (1 + gm*RE) || RC ? (beta*ro) || RC for RE >> re.
 * This "bootstrapping" effect makes the CE with degeneration
 * approximate a current source.
 * Complexity: O(1). */
double bjt_ce_output_impedance_degenerated(const bjt_ss_params_t *ss,
                                            double RC, double RE);

/* CE amplifier current gain.
 * Ai = beta * (RC / (RC + RL)) * (Rs / (Rs + rpi))
 * The current division at output and input reduces the ideal beta.
 * Complexity: O(1). */
double bjt_ce_current_gain(const bjt_ss_params_t *ss,
                            double RC, double RL, double Rs);

/* ---- L2: Common-Collector (CC) / Emitter-Follower Analysis ---- */

/* CC amplifier voltage gain.
 * Av = ( (beta+1)*(RE||RL) ) / ( rpi + (beta+1)*(RE||RL) )
 * ? (RE||RL) / (re + (RE||RL)) ? 1  (close to unity, slightly < 1).
 * No phase inversion (positive gain).
 * Complexity: O(1). */
double bjt_cc_voltage_gain(const bjt_ss_params_t *ss,
                            double RE, double RL, double Rs);

/* CC amplifier input impedance (looking into base).
 * Rin = rpi + (beta+1)*(RE||RL)
 * Very high input impedance ? the emitter load is reflected to the
 * base multiplied by (beta+1). This makes the CC an excellent buffer.
 * Complexity: O(1). */
double bjt_cc_input_impedance(const bjt_ss_params_t *ss,
                               double RE, double RL);

/* CC amplifier output impedance (looking into emitter).
 * Rout = RE || ( (rpi + Rs) / (beta + 1) )
 * ? RE || (re + Rs/(beta+1))
 * Very low output impedance ? another key CC advantage.
 * Complexity: O(1). */
double bjt_cc_output_impedance(const bjt_ss_params_t *ss,
                                double RE, double Rs);

/* CC amplifier current gain.
 * Ai = (beta+1) * (RE/(RE+RL)) * (Rs/(Rs + Rin))
 * Approximately beta+1, making it a good current buffer.
 * Complexity: O(1). */
double bjt_cc_current_gain(const bjt_ss_params_t *ss,
                            double RE, double RL, double Rs);

/* ---- L2: Common-Base (CB) Amplifier Analysis ---- */

/* CB amplifier voltage gain.
 * Av = gm * (RC || RL) * ( RE||re / (Rs + RE||re) )
 * Non-inverting (positive gain). High voltage gain, low current gain (~alpha).
 * Complexity: O(1). */
double bjt_cb_voltage_gain(const bjt_ss_params_t *ss,
                            double RC, double RL, double RE, double Rs);

/* CB amplifier input impedance (looking into emitter).
 * Rin = RE || re ? re  (very low ? tens of ohms).
 * This low input impedance limits CB use but provides excellent
 * high-frequency performance (no Miller effect on input).
 * Complexity: O(1). */
double bjt_cb_input_impedance(const bjt_ss_params_t *ss, double RE);

/* CB amplifier output impedance (looking into collector).
 * Rout = RC || ro  (same as CE, similar collector-side topology).
 * Complexity: O(1). */
double bjt_cb_output_impedance(const bjt_ss_params_t *ss, double RC);

/* ---- L4: Miller Theorem for BJT ---- */

/* Compute Miller capacitance at base-collector junction.
 * C_miller = Cmu * (1 - Av_reverse) where Av_reverse is the
 * voltage gain from C to B (negative for CE, making 1+|Av| multiplier).
 * For CE: C_miller_in = Cmu * (1 + |Av|)
 *         C_miller_out = Cmu * (1 + 1/|Av|) ? Cmu
 * The Miller effect dramatically reduces high-frequency gain.
 * Reference: Miller (1920), "Dependence of the input impedance of a
 *   three-electrode vacuum tube upon the load", NBS Scientific Papers.
 * Complexity: O(1). */
double bjt_miller_capacitance(double Cmu, double voltage_gain_magnitude);

/* Compute Miller multiplication factor M = 1 + |Av|.
 * For a CE with Av = -100, M ? 101: the 2pF Cbc looks like ~200pF at input.
 * Complexity: O(1). */
double bjt_miller_factor(double voltage_gain_magnitude);

/* ---- L5: Small-Signal Complete Analysis ---- */

/* Compute complete CE amplifier metrics (gain, impedance, GBW).
 * Combines Q-point, bias network, load, and source impedance.
 * Fills all fields of bjt_amp_metrics_t.
 * Complexity: O(1). */
void bjt_ce_analyze(const bjt_ss_params_t *ss,
                     double RC, double RL, double RE, double Rs,
                     int re_bypassed,
                     bjt_amp_metrics_t *metrics);

/* Compute complete CC amplifier metrics.
 * Complexity: O(1). */
void bjt_cc_analyze(const bjt_ss_params_t *ss,
                     double RE, double RL, double Rs,
                     bjt_amp_metrics_t *metrics);

/* Compute complete CB amplifier metrics.
 * Complexity: O(1). */
void bjt_cb_analyze(const bjt_ss_params_t *ss,
                     double RC, double RL, double RE, double Rs,
                     bjt_amp_metrics_t *metrics);

/* ---- L3: Two-Port Network Conversion ---- */

/* Convert CE amplifier to z-parameters at frequency f.
 * Uses the hybrid-pi model to compute z-parameters of the
 * amplifier including external resistors.
 * Complexity: O(1). */
void bjt_ce_to_z_params(const bjt_ss_params_t *ss,
                         double RC, double frequency_hz,
                         bjt_z_params_t *zp);

/* Convert CE amplifier to y-parameters.
 * Complexity: O(1). */
void bjt_ce_to_y_params(const bjt_ss_params_t *ss,
                         double RC, double frequency_hz,
                         bjt_y_params_t *yp);

/* Convert between two-port parameter types using standard formulas.
 * z-params to y-params: Y = Z^-1 (2x2 matrix inversion).
 * Complexity: O(1). Returns 0 on success, -1 if singular (det=0). */
int bjt_z_to_y(const bjt_z_params_t *zp, bjt_y_params_t *yp);

/* y-params to z-params: Z = Y^-1.
 * Complexity: O(1). Returns 0 on success, -1 if singular. */
int bjt_y_to_z(const bjt_y_params_t *yp, bjt_z_params_t *zp);

/* ---- L5: Impedance Matching Considerations ---- */

/* Compute maximum available power gain (MAG) from y-parameters.
 * MAG = |y21/y12| * (k - sqrt(k^2 - 1)) where k is the stability factor.
 * For unconditionally stable amplifiers.
 * Complexity: O(1). */
double bjt_max_available_gain(const bjt_y_params_t *yp);

/* Compute Rollett stability factor K from y-parameters.
 * K = (2*Re(y11)*Re(y22) - Re(y12*y21)) / |y12*y21|
 * K > 1 implies unconditional stability.
 * Complexity: O(1). */
double bjt_stability_factor(const bjt_y_params_t *yp);

#endif /* BJT_SMALL_SIGNAL_H */
