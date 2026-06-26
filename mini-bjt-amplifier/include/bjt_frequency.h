/**
 * bjt_frequency.h ? BJT Amplifier Frequency Response Analysis
 *
 * BJT amplifiers exhibit frequency-dependent gain due to:
 *   1. Coupling/bypass capacitors ? low-frequency roll-off (high-pass)
 *   2. Internal transistor capacitances (Cpi, Cmu) ? high-frequency roll-off (low-pass)
 *   3. Miller effect multiplicatively amplifies Cmu at the input
 *
 * The combined response is band-pass with mid-band gain region flanked
 * by f_L (low cut-off) and f_H (high cut-off).
 *
 * Reference: Sedra & Smith, Ch. 10; Gray & Meyer, Ch. 7
 *            Bode (1945), "Network Analysis and Feedback Amplifier Design"
 *
 * Knowledge Levels:
 *   L2: Frequency response concepts, Bode plots, -3dB bandwidth
 *   L3: Pole-zero analysis, transfer functions, Laplace domain
 *   L4: Miller theorem, open-circuit time constants (OCTC)
 *   L5: Short-circuit time constants (SCTC), dominant pole approximation
 *   L6: Frequency compensation, gain-bandwidth trade-off
 */

#ifndef BJT_FREQUENCY_H
#define BJT_FREQUENCY_H

#include "bjt_model.h"
#include "bjt_small_signal.h"
#include <math.h>

/* ---- L2: Coupling Capacitor Low-Frequency Analysis ---- */

/* Low cut-off frequency due to input coupling capacitor C_in.
 * f_L_in = 1 / (2*pi * C_in * (Rs + Rin))
 * Forms a high-pass filter with the input resistance.
 * Complexity: O(1). */
double bjt_freq_fL_input_coupling(double C_in, double Rs, double Rin);

/* Low cut-off frequency due to output coupling capacitor C_out.
 * f_L_out = 1 / (2*pi * C_out * (Rout + RL))
 * Complexity: O(1). */
double bjt_freq_fL_output_coupling(double C_out, double Rout, double RL);

/* Low cut-off frequency due to emitter bypass capacitor C_E.
 * f_L_E = 1 / (2*pi * C_E * (RE || re))
 * The bypass capacitor is in parallel with RE, with the
 * resistance seen from its terminals being RE || (re + Rs/(beta+1)).
 * Complexity: O(1). */
double bjt_freq_fL_emitter_bypass(double C_E, double RE,
                                   double re, double Rs, double beta);

/* Total low-frequency cut-off for an amplifier with multiple
 * independent coupling/bypass capacitors. For n poles,
 *   f_L_total ? sqrt( sum_i (f_L_i)^2 )
 * (sum of squares approximation, assuming widely spaced poles).
 * More accurate: the -3dB point is where the product of all
 * high-pass transfer functions equals 1/sqrt(2).
 * Complexity: O(n). */
double bjt_freq_fL_total(const double *fL_individual, int n);

/* ---- L2: High-Frequency Analysis ---- */

/* High cut-off frequency due to Cpi (input capacitance).
 * f_H_pi = 1 / (2*pi * Cpi * (rpi || Rs))
 * Complexity: O(1). */
double bjt_freq_fH_input_pole(const bjt_ss_params_t *ss, double Rs);

/* High cut-off frequency due to Miller-multiplied Cmu (input side).
 * f_H_miller = 1 / (2*pi * Cmu * (1+|Av|) * (rpi || Rs))
 * The Miller-multiplied Cmu dominates f_H in most CE amplifiers.
 * Complexity: O(1). */
double bjt_freq_fH_miller_input(const bjt_ss_params_t *ss,
                                 double Rs, double Av_magnitude);

/* High cut-off frequency due to Cmu at output (Miller output).
 * f_H_mu_out = 1 / (2*pi * Cmu * (RC || RL))
 * Typically much higher than the input Miller pole, not dominant.
 * Complexity: O(1). */
double bjt_freq_fH_miller_output(const bjt_ss_params_t *ss,
                                  double RC, double RL);

/* ---- L5: Open-Circuit Time Constants (OCTC) Method ---- */

/* OCTC: estimate f_H from the sum of open-circuit time constants.
 * f_H ? 1 / (2*pi * sum(R_i0 * C_i))
 * where R_i0 is the resistance seen by capacitor C_i with all other
 * capacitors open-circuited. This is a powerful approximation
 * accurate to within ~10-20%.
 * Reference: Gray, Hurst, Lewis & Meyer, Ch. 7.
 *
 * Inputs:
 *   n_caps: number of (R_i0, C_i) pairs
 *   r_i0[i]: resistance seen by capacitor i (open-circuit)
 *   c_i[i]: capacitance value of capacitor i
 * Complexity: O(n). */
double bjt_freq_fH_octc(const double *r_i0, const double *c_i, int n_caps);

/* ---- L5: Short-Circuit Time Constants (SCTC) Method ---- */

/* SCTC: estimate f_L from the sum of short-circuit time constants.
 * f_L ? (1 / (2*pi)) * sum(1 / (R_is * C_i))
 * where R_is is the resistance seen by capacitor C_i with all other
 * capacitors short-circuited. Used for low-frequency estimation.
 * Complexity: O(n). */
double bjt_freq_fL_sctc(const double *r_is, const double *c_i, int n_caps);

/* ---- L6: Gain-Bandwidth Analysis ---- */

/* Gain-Bandwidth Product (GBW or GBWP).
 * For a dominant-pole amplifier:
 *   GBW = |Av_mid| * f_H
 * The GBW is approximately constant for a given transistor ?
 * trading gain for bandwidth is the fundamental amplifier design choice.
 * For a given BJT: GBW ? fT (the transition frequency) for CE config.
 * Complexity: O(1). */
double bjt_freq_gbw_product(double Av_mid_magnitude, double f_H);

/* Compute the bandwidth shrinkage factor due to adding feedback.
 * For series-shunt feedback with loop gain T:
 *   BW_feedback = BW_open_loop * (1 + T)  (gain-bandwidth constancy)
 *   Av_closed = Av_open / (1 + T)
 * Complexity: O(1). */
double bjt_freq_bandwidth_with_feedback(double BW_open_loop,
                                         double loop_gain_T);

/* ---- L3: Transfer Function Analysis ---- */

/* Maximum number of poles in the transfer function we support. */
#define BJT_MAX_POLES 10
#define BJT_MAX_ZEROS 10

/* Transfer function representation:
 * H(s) = A0 * (1 + s/w_z1)*(1 + s/w_z2)*... / ((1 + s/w_p1)*(1 + s/w_p2)*...)
 *
 * where A0 = DC/mid-band gain,
 *       w_zi = zero angular frequencies [rad/s],
 *       w_pi = pole angular frequencies [rad/s].
 */
typedef struct {
    double A0;                      /* Mid-band gain [V/V] */
    int n_poles;
    double poles_rad_per_s[BJT_MAX_POLES];   /* Pole frequencies [rad/s] */
    int n_zeros;
    double zeros_rad_per_s[BJT_MAX_ZEROS];   /* Zero frequencies [rad/s] */
} bjt_transfer_function_t;

/* Evaluate transfer function magnitude at frequency f [Hz].
 * |H(jw)| = |A0| * prod_i |1 + jw/w_zi| / prod_i |1 + jw/w_pi|
 * where w = 2*pi*f.
 * Complexity: O(n_poles + n_zeros). */
double bjt_tf_magnitude(const bjt_transfer_function_t *tf, double frequency_hz);

/* Evaluate transfer function phase at frequency f [Hz] [radians].
 * angle(H(jw)) = sum_i atan(w/w_zi) - sum_i atan(w/w_pi)
 * (+ pi if A0 < 0 for CE inversion).
 * Complexity: O(n_poles + n_zeros). */
double bjt_tf_phase(const bjt_transfer_function_t *tf, double frequency_hz);

/* Build transfer function for a CE amplifier.
 * Poles:
 *   f_L: input coupling, output coupling, emitter bypass (3 low-freq poles)
 *   f_H: Miller input pole (dominant), output pole
 * Complexity: O(1). */
void bjt_tf_ce_build(double Av_mid, double fL_in, double fL_out,
                      double fL_bypass, double fH_miller, double fH_out,
                      bjt_transfer_function_t *tf);

/* Find the -3dB cut-off frequencies f_L and f_H from the
 * transfer function by binary search. Returns f where |H(f)|/|A0| = 1/sqrt(2).
 * Complexity: O((n_poles+n_zeros) * log(f_range/epsilon)). */
double bjt_tf_find_fL(const bjt_transfer_function_t *tf,
                       double f_min, double f_max, double epsilon);

double bjt_tf_find_fH(const bjt_transfer_function_t *tf,
                       double f_min, double f_max, double epsilon);

/* ---- L5: Bode Plot Generation ---- */

/* Single Bode plot point. */
typedef struct {
    double frequency_hz;
    double magnitude_db;
    double phase_degrees;
} bjt_bode_point_t;

/* Generate Bode magnitude and phase data for log-spaced frequencies.
 * n_points points spaced logarithmically between f_start and f_end.
 * Results stored in pre-allocated points[] array.
 * Complexity: O(n_points * (n_poles+n_zeros)). */
void bjt_bode_generate(const bjt_transfer_function_t *tf,
                        double f_start, double f_end, int n_points,
                        bjt_bode_point_t *points);

/* ---- L6: Dominant Pole Approximation ---- */

/* Find the dominant (lowest-frequency) pole in the transfer function.
 * The dominant pole approximation sets f_H ? f_p_dominant,
 * valid when the dominant pole is at least 4x lower than the next pole.
 * Complexity: O(n_poles). */
double bjt_tf_dominant_pole(const bjt_transfer_function_t *tf);

/* Estimate total bandwidth from dominant pole and second pole.
 * If separation > 4x: BW ? f_dominant.
 * If poles interact: BW correction using root-sum-of-squares.
 * Complexity: O(1). */
double bjt_tf_bandwidth_two_pole(double f_p1, double f_p2);

#endif /* BJT_FREQUENCY_H */
