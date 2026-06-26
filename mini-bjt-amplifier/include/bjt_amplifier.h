/**
 * bjt_amplifier.h ? Complete BJT Amplifier Design & Multistage Analysis
 *
 * This header ties together DC bias, small-signal analysis, and frequency
 * response into a unified amplifier design framework. It covers the
 * canonical amplifier configurations as complete systems and multistage
 * cascade analysis.
 *
 * Reference: Sedra & Smith, Ch. 8, Ch. 9 (multistage), Ch. 13 (cascode)
 *            Gray & Meyer, Ch. 6 (cascode), Ch. 9 (feedback)
 *
 * Knowledge Levels:
 *   L6: Cascode amplifier, differential pair, Darlington pair
 *   L7: Audio preamplifier design, RF amplifier front-end
 *   L8: Noise-optimized biasing, distortion reduction
 */

#ifndef BJT_AMPLIFIER_H
#define BJT_AMPLIFIER_H

#include "bjt_model.h"
#include "bjt_dc_bias.h"
#include "bjt_small_signal.h"
#include "bjt_frequency.h"

/* ---- L6: Cascode Amplifier (CE + CB) ---- */

/* Cascode amplifier parameters.
 * The cascode consists of a CE stage (Q1) driving a CB stage (Q2).
 * Advantages: eliminates Miller effect (Q1 sees low impedance at collector
 * because Q2's emitter has low input impedance), high output impedance,
 * wide bandwidth. */
typedef struct {
    bjt_qpoint_t qp1;       /* Q-point of CE transistor (Q1) */
    bjt_qpoint_t qp2;       /* Q-point of CB transistor (Q2) */
    bjt_ss_params_t ss1;    /* Small-signal params for Q1 */
    bjt_ss_params_t ss2;    /* Small-signal params for Q2 */
    bjt_amp_metrics_t metrics;
} bjt_cascode_t;

/* Analyze a cascode amplifier.
 * Assumes both transistors are identical, biased with IC1 = IC2.
 * Q1 (CE): base input, collector loads into Q2 emitter.
 * Q2 (CB): emitter input (from Q1 collector), collector output.
 *
 * Voltage gain: Av ? -gm * (RC || RL)  (same as CE but with much wider BW!)
 * The key insight: Q1's collector load is re2 ? 1/gm2 (very low),
 * so Miller multiplication of Cmu1 is nearly eliminated.
 *
 * Rout_cascode ? beta * ro  (much higher than single CE).
 * Complexity: O(1). */
void bjt_cascode_analyze(const bjt_spice_params_t *dev,
                          double IC_target, double VCC,
                          double RC, double RL, double Rs,
                          double R1, double R2, double RE,
                          double temperature_kelvin,
                          bjt_cascode_t *result);

/* Design a cascode amplifier for specified gain and bandwidth.
 * Returns 0 on success. */
int bjt_cascode_design(double Av_target, double BW_target,
                         double VCC, double RL, double Rs,
                         const bjt_spice_params_t *dev,
                         double temperature_kelvin,
                         bjt_cascode_t *result,
                         bjt_bias_network_t *net);

/* ---- L6: Differential Pair ---- */

/* Differential pair configuration. */
typedef struct {
    bjt_qpoint_t qp1, qp2;  /* Q-points (equal for matched pair) */
    bjt_ss_params_t ss1, ss2;
    double I_EE;             /* Tail current source [A] */
    double R_EE;             /* Tail resistance (degeneration) [Ohm] */
    double RC1, RC2;         /* Collector resistors [Ohm] */
    double Av_dm;            /* Differential-mode gain [V/V] */
    double Av_cm;            /* Common-mode gain [V/V] */
    double CMRR;             /* Common-mode rejection ratio [V/V] */
    double CMRR_dB;          /* CMRR [dB] */
    double Rid;              /* Differential input resistance [Ohm] */
    double Ric;              /* Common-mode input resistance [Ohm] */
    double Rod;              /* Differential output resistance [Ohm] */
} bjt_diff_pair_t;

/* Analyze a differential pair with resistive load.
 * Differential-mode gain (balanced output):
 *   Av_dm = gm * RC
 * Common-mode gain (balanced output, with tail R_EE):
 *   Av_cm ? -RC / (2*R_EE)
 * CMRR = |Av_dm / Av_cm| ? 2 * gm * R_EE
 *
 * For matched transistors with ideal current source (R_EE -> infinity),
 * CMRR -> infinity (in practice limited by transistor mismatch).
 * Complexity: O(1). */
void bjt_diff_pair_analyze(const bjt_spice_params_t *dev,
                            double I_EE, double RC,
                            double R_EE, double VCC,
                            double VEE, double temperature_kelvin,
                            bjt_diff_pair_t *result);

/* Compute CMRR mismatch-limited value due to RC tolerance.
 * If the two RC resistors differ by delta_RC/RC:
 *   CMRR_mismatch ? 1 / (delta_RC/RC)
 * Complexity: O(1). */
double bjt_diff_pair_cmrr_mismatch(double delta_RC_ratio);

/* ---- L6: Darlington Pair ---- */

/* Darlington pair parameters (composite transistor).
 * Two NPN BJTs connected so their current gains multiply:
 *   beta_eff ? beta1 * beta2
 *   VBE_eff = VBE1 + VBE2 ? 1.4 V
 */
typedef struct {
    double beta_eff;         /* Effective current gain */
    double VBE_eff;          /* Total base-emitter voltage */
    bjt_ss_params_t ss_eq;   /* Equivalent small-signal parameters */
} bjt_darlington_t;

/* Compute Darlington pair equivalent parameters.
 * gm_eff ? gm2 / 2 (if both transistors at same IC)
 * rpi_eff ? 2 * beta1 * beta2 / gm1
 * Complexity: O(1). */
void bjt_darlington_analyze(const bjt_ss_params_t *ss1,
                             const bjt_ss_params_t *ss2,
                             bjt_darlington_t *result);

/* ---- L7: Complete Amplifier Design ---- */

/* Complete single-stage amplifier design specification. */
typedef struct {
    bjt_amp_topology_t topology;
    bjt_bias_network_t bias;
    bjt_qpoint_t qpoint;
    bjt_ss_params_t ss;
    bjt_amp_metrics_t metrics;
    bjt_transfer_function_t tf;
    double coupling_caps[3];  /* Cin, Cout, CE [F] */
    int num_coupling_caps;
} bjt_amplifier_design_t;

/* Design a complete BJT amplifier from specifications.
 * Given: topology, target gain, impedance specs, supply voltage.
 * Returns: complete design with all component values.
 * This is the top-level design function.
 * Complexity: O(1). */
int bjt_amplifier_design(bjt_amp_topology_t topology,
                          double Av_target, double Rin_target,
                          double Rout_target, double VCC,
                          double RL, double Rs,
                          double fL_target, double fH_target,
                          const bjt_spice_params_t *dev,
                          double temperature_kelvin,
                          bjt_amplifier_design_t *design);

/* Evaluate the complete amplifier at a given frequency.
 * Returns the voltage gain magnitude at frequency f.
 * Includes effects of all coupling capacitors and transistor capacitances.
 * Complexity: O(n_poles+n_zeros). */
double bjt_amplifier_evaluate(const bjt_amplifier_design_t *design,
                               double frequency_hz);

/* ---- L6: Multistage Analysis ---- */

/* Maximum number of stages in multistage analysis. */
#define BJT_MAX_STAGES 6

/* Multistage amplifier configuration. */
typedef struct {
    int n_stages;
    bjt_amplifier_design_t stages[BJT_MAX_STAGES];
    bjt_amp_metrics_t overall_metrics;
    bjt_transfer_function_t overall_tf;
} bjt_multistage_t;

/* Analyze a multistage BJT amplifier (cascade of individual stages).
 * Stage N output = Stage N+1 input (with inter-stage coupling).
 *
 * Overall gain: Av_total = product of individual stage gains.
 * Bandwidth: dominated by the slowest stage (narrowest bandwidth).
 *   f_H_total ? 1 / sqrt(sum_i (1/f_H_i)^2)  (approximate)
 * Complexity: O(n_stages * (n_poles+n_zeros)). */
void bjt_multistage_analyze(bjt_multistage_t *amp);

/* Add a stage to a multistage amplifier.
 * Returns the new stage index, or -1 if max stages reached. */
int bjt_multistage_add_stage(bjt_multistage_t *amp,
                              const bjt_amplifier_design_t *stage);

/* ---- L8: Distortion Analysis ---- */

/* Compute second harmonic distortion HD2 for a CE amplifier.
 * From the nonlinear IC-VBE relationship:
 *   HD2 ? (1/4) * (Vin_peak / VT)
 * for small input signals where Vin_peak << VT.
 * Reference: Sedra & Smith, Ch. 8.
 * Complexity: O(1). */
double bjt_harmonic_distortion_hd2(double vin_peak, double vt);

/* Compute third harmonic distortion HD3 for a CE amplifier.
 *   HD3 ? (1/24) * (Vin_peak / VT)^2
 * Complexity: O(1). */
double bjt_harmonic_distortion_hd3(double vin_peak, double vt);

/* Compute total harmonic distortion (THD) from HD2 and HD3.
 *   THD ? sqrt(HD2^2 + HD3^2)
 * (higher-order terms typically negligible for small signals).
 * Complexity: O(1). */
double bjt_thd_from_hd(double hd2, double hd3);

/* Estimate the maximum input signal for a given THD tolerance.
 * Using the dominant HD2 term:
 *   Vin_max ? 4 * VT * THD_target
 * Example: THD < 1% at VT=26mV ? Vin_max ? 1.04 mV.
 * Complexity: O(1). */
double bjt_max_input_for_thd(double thd_target, double vt);

/* ---- L9: Research-Area Stubs (documented, not implemented) ---- */

/* SiGe HBT figure-of-merit: fT * BV_CEO product.
 * Research frontier: SiGe HBTs achieve fT > 500 GHz in advanced nodes.
 * Documented in docs/knowledge-graph.md ? see L9 entries. */

#endif /* BJT_AMPLIFIER_H */
