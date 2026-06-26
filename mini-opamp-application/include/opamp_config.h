#ifndef OPAMP_CONFIG_H
#define OPAMP_CONFIG_H

/**
 * opamp_config.h - Operational Amplifier Configuration Topologies
 *
 * Defines all standard amplifier configurations built around operational
 * amplifiers. Each configuration is treated as a complete knowledge unit
 * covering the circuit topology, equations, design procedure, and
 * non-ideal corrections.
 *
 * Knowledge Coverage:
 *   L1 (Definitions): AmplifierConfig, InstrumentationAmpParams structs
 *   L2 (Core Concepts): Inverting/non-inverting/differential topologies
 *   L3 (Math Structures): Closed-loop transfer functions in s-domain
 *   L4 (Fundamental Laws): Feedback theory, Black's formula, loop gain
 *   L5 (Algorithms/Methods): Gain calculation, impedance analysis
 *   L6 (Canonical Problems): Standard amplifier design
 *
 * Reference: Sedra & Smith Ch.2, Franco Ch.2-4,
 *            Karki "Designing for High Gain" TI SLOA011
 * Course: MIT 6.002, Stanford EE101, Berkeley EE105, TUM EI
 */

#include "opamp_basic.h"

/*============================================================================
 * L1: DEFINITIONS - Amplifier Configuration Types
 *============================================================================*/

/** Amplifier topology enumeration */
typedef enum {
    OPAMP_CFG_INVERTING = 0,
    OPAMP_CFG_NON_INVERTING,
    OPAMP_CFG_DIFFERENTIAL,
    OPAMP_CFG_INSTRUMENTATION,
    OPAMP_CFG_SUMMING_INVERTING,
    OPAMP_CFG_SUMMING_NON_INVERTING,
    OPAMP_CFG_INTEGRATOR,
    OPAMP_CFG_DIFFERENTIATOR,
    OPAMP_CFG_VOLTAGE_FOLLOWER,
    OPAMP_CFG_TRANSCONDUCTANCE,
    OPAMP_CFG_TRANSIMPEDANCE,
    OPAMP_CFG_CURRENT_SOURCE_HOWLAND,
    OPAMP_CFG_COUNT
} OpAmpConfigType;

/**
 * AmplifierConfig - Complete specification for a single op-amp stage.
 *
 * This struct captures all design parameters for any op-amp configuration.
 * It is the primary user-facing data type for amplifier design.
 */
typedef struct {
    OpAmpConfigType type;       /**< Configuration topology */
    /* Resistor values (Ohm) */
    double R_f;                 /**< Feedback resistor */
    double R_i;                 /**< Input resistor (inverting path) */
    double R_g;                 /**< Ground resistor (non-inverting path) */
    double R_load;              /**< Load resistance */
    double R_source;            /**< Source resistance */
    /* Capacitor values for active filters / integrators */
    double C_f;                 /**< Feedback capacitor (F) */
    double C_i;                 /**< Input capacitor (F) */
    /* Performance targets */
    double target_gain;         /**< Desired closed-loop gain (V/V) */
    double target_bw;           /**< Desired -3dB bandwidth (Hz) */
    double target_Z_in;         /**< Desired input impedance (Ohm) */
    double target_Z_out;        /**< Desired output impedance (Ohm) */
} AmplifierConfig;

/**
 * InstrumentationAmpConfig - Three-op-amp instrumentation amplifier.
 *
 * Standard topology:
 *   Stage 1: Two non-inverting amplifiers (buffers) with gain:
 *     G1 = 1 + 2*R_f1/R_gain
 *   Stage 2: Difference amplifier with gain:
 *     G2 = R_f2/R_i2
 *   Total gain: G = G1 * G2
 *
 * CMRR of IA >> CMRR of single diff-amp due to the bootstrapped input stage.
 *
 * Reference: Sedra & Smith S2.4.2
 *            Kitchin & Counts "A Designer's Guide to Instrumentation Amplifiers" (Analog Devices)
 */
typedef struct {
    double R_gain;              /**< Gain-setting resistor (Ohm) */
    double R_f1;                /**< Stage 1 feedback resistors (Ohm) */
    double R_f2;                /**< Stage 2 feedback resistor (Ohm) */
    double R_i2;                /**< Stage 2 input resistors (Ohm) */
    double R_3;                 /**< Stage 2 voltage divider resistor (Ohm) */
    double target_gain;         /**< Desired total gain (V/V) */
    double target_CMRR;         /**< Minimum CMRR (dB) */
    double target_bw;           /**< Desired bandwidth (Hz) */
} InstrumentationAmpConfig;

/**
 * SummingAmpConfig - Summing amplifier configuration.
 *
 * Inverting summing amplifier:
 *   v_out = -R_f * (v1/R1 + v2/R2 + ... + vN/RN)
 *
 * Weighted summing with independent gain per channel.
 */
typedef struct {
    double R_f;                 /**< Feedback resistor (Ohm) */
    double R_inputs[8];         /**< Input resistors per channel (Ohm) */
    double input_weights[8];    /**< Desired weight per channel */
    int n_channels;             /**< Number of input channels */
} SummingAmpConfig;

/*============================================================================
 * L2: CORE CONCEPTS - Configuration Transfer Functions
 *============================================================================*/

/**
 * opamp_cfg_closed_loop_gain - Compute closed-loop DC gain for any config.
 *
 * The generalized feedback formula (Black 1934):
 *   A_cl = A_forward / (1 + A_ol * beta)
 *
 * where A_forward = forward gain without feedback,
 *       beta = feedback factor (fraction of output fed back to input)
 *
 * @param cfg         Amplifier configuration
 * @param opamp       Op-amp parameters
 * @return            Closed-loop DC gain (V/V), positive for non-inverting
 *
 * Reference: Black "Stabilized Feedback Amplifiers" (1934)
 */
double opamp_cfg_closed_loop_gain(const AmplifierConfig *cfg, const OpAmpParams *opamp);

/**
 * opamp_cfg_input_impedance - Compute input impedance for configuration.
 *
 * Inverting:     Z_in = R_i (virtual ground at inverting input)
 * Non-inverting: Z_in = R_in_cm || (R_in_diff * (1 + A_ol*beta))
 *                (bootstrapped by feedback)
 * Differential:  Z_in_diff = 2*R_i (balanced)
 *
 * @param cfg         Amplifier configuration
 * @param opamp       Op-amp parameters
 * @return            Input impedance (Ohm)
 *
 * Reference: Franco S2.5 (Input/Output Impedances)
 */
double opamp_cfg_input_impedance(const AmplifierConfig *cfg, const OpAmpParams *opamp);

/**
 * opamp_cfg_output_impedance - Compute output impedance with feedback.
 *
 *   Z_out_cl = Z_out_ol / (1 + A_ol * beta)
 *
 * Negative feedback reduces output impedance by the loop gain factor.
 * For voltage-series feedback (non-inverting, voltage follower):
 *   Z_out is reduced by factor (1 + A_ol*beta)
 *
 * @param cfg         Amplifier configuration
 * @param opamp       Op-amp parameters
 * @return            Closed-loop output impedance (Ohm)
 *
 * Reference: Sedra & Smith S10.10 (Effect of feedback on Ro)
 */
double opamp_cfg_output_impedance(const AmplifierConfig *cfg, const OpAmpParams *opamp);

/*============================================================================
 * L3: MATHEMATICAL STRUCTURES - Transfer Function Analysis
 *============================================================================*/

/**
 * opamp_cfg_feedback_factor - Compute feedback factor beta.
 *
 * For inverting:   beta = R_i / (R_i + R_f)
 * For non-invert:  beta = R_g / (R_g + R_f)
 * For follower:    beta = 1.0
 *
 * @param cfg    Amplifier configuration
 * @return       Feedback factor (dimensionless)
 */
double opamp_cfg_feedback_factor(const AmplifierConfig *cfg);

/**
 * opamp_cfg_noise_gain - Compute noise gain.
 *
 * Noise gain NG = 1/beta = closed-loop gain of non-inverting
 * configuration with same resistor network.
 *
 * Important: For inverting amplifier:
 *   Signal gain = -R_f/R_i
 *   Noise gain   = 1 + R_f/R_i
 *
 * This means an inverting amplifier has more output noise than a
 * non-inverting amplifier with the same |signal gain|.
 *
 * @param cfg    Amplifier configuration
 * @return       Noise gain (V/V)
 */
double opamp_cfg_noise_gain(const AmplifierConfig *cfg);

/**
 * opamp_cfg_bandwidth - Compute closed-loop -3dB bandwidth.
 *
 * Uses the gain-bandwidth product:
 *   BW_cl = GBW / NG  (for dominant-pole compensated op-amps)
 *
 * @param cfg         Amplifier configuration
 * @param opamp       Op-amp parameters
 * @return            -3dB bandwidth (Hz)
 */
double opamp_cfg_bandwidth(const AmplifierConfig *cfg, const OpAmpParams *opamp);

/*============================================================================
 * L4: FUNDAMENTAL LAWS - Design with Feedback Theory
 *============================================================================*/

/**
 * opamp_cfg_design_resistors - Design resistor values for target gain.
 *
 * Given a target closed-loop gain and a total resistance constraint,
 * compute optimal R_f and R_i/R_g values.
 *
 * Design procedure (Sedra & Smith S2.6):
 *   1. Choose configuration type
 *   2. Select R_i for desired Z_in (or noise constraint)
 *   3. Compute R_f = |Gain| * R_i
 *   4. Verify within op-amp drive capability (R_f + R_i >> R_out)
 *   5. Check resistor values are practical (100 Ohm to 1 MOhm)
 *
 * @param cfg            Configuration to fill (type and constraints must be set)
 * @param opamp          Op-amp parameters
 * @param gain           Target closed-loop gain magnitude
 * @param total_R        Total resistance constraint (0 = no constraint)
 * @return               1 on success, 0 if infeasible
 */
int opamp_cfg_design_resistors(AmplifierConfig *cfg, const OpAmpParams *opamp,
                                double gain, double total_R);

/**
 * opamp_cfg_sensitivity_analysis - Compute gain sensitivity to resistor tolerance.
 *
 * For non-inverting: dA_cl/A_cl = dR_f/R_f * R_f/(R_f+R_g) - dR_g/R_g * R_g/(R_f+R_g)
 * For inverting:     dA_cl/A_cl = dR_f/R_f - dR_i/R_i
 *
 * @param cfg            Amplifier configuration
 * @param tolerance_pct  Resistor tolerance (%)
 * @param gain_min       Output: minimum gain
 * @param gain_max       Output: maximum gain
 */
void opamp_cfg_sensitivity_analysis(const AmplifierConfig *cfg,
                                     double tolerance_pct,
                                     double *gain_min, double *gain_max);

/*============================================================================
 * L5: ALGORITHMS - Instrumentation Amplifier Design
 *============================================================================*/

/**
 * opamp_ia_design - Design a three-op-amp instrumentation amplifier.
 *
 * Design procedure:
 *   1. Choose R_gain for stage-1 gain: G1 = 1 + 2*R_f1/R_gain
 *   2. Choose R_i2, R_f2 for stage-2 gain: G2 = R_f2/R_i2
 *   3. Ensure R_3 = R_i2 for CMRR matching
 *   4. Verify total gain G = G1 * G2 meets target
 *   5. Check bandwidth: BW_cl = GBW / G (per stage)
 *
 * @param config   Instrumentation amplifier config (targets must be set)
 * @param opamp    Op-amp parameters (used for all three op-amps)
 * @param actual_gain  Output: achieved gain
 * @param actual_CMRR  Output: estimated CMRR (dB)
 * @return         1 on success, 0 on infeasible design
 *
 * Reference: Analog Devices MT-061 (Instrumentation Amplifier Basics)
 *            TI SBOA283 (Three Op Amp Instrumentation Amplifier)
 */
int opamp_ia_design(InstrumentationAmpConfig *config, const OpAmpParams *opamp,
                     double *actual_gain, double *actual_CMRR);

/**
 * opamp_ia_cmrr_estimate - Estimate CMRR of instrumentation amplifier.
 *
 * For a three-op-amp IA with resistor mismatch epsilon:
 *   CMRR_total (dB) = -20*log10(4*epsilon/(G1))
 *
 * where epsilon is the fractional resistor mismatch in stage 2.
 * Stage 1 CMRR is very high (bootstrapped), so stage 2 mismatch dominates.
 *
 * Typical CMRR values: 80-120 dB with 0.1% resistors, 60-80 dB with 1% resistors.
 *
 * @param config   IA configuration
 * @param opamp    Op-amp CMRR (dB)
 * @param mismatch Resistor fractional mismatch (e.g., 0.001 for 0.1%)
 * @return         Estimated total CMRR (dB)
 */
double opamp_ia_cmrr_estimate(const InstrumentationAmpConfig *config,
                               const OpAmpParams *opamp, double mismatch);

/*============================================================================
 * L6: CANONICAL PROBLEMS - Standard Amplifier Solutions
 *============================================================================*/

/**
 * opamp_cfg_solve_dc_operating_point - Solve DC operating point.
 *
 * For a given input voltage and configuration, compute all node voltages
 * and branch currents at DC.
 *
 * Implemented using Modified Nodal Analysis (MNA) for op-amp circuits:
 *   The op-amp is replaced by a VCVS (voltage-controlled voltage source)
 *   with gain A_ol, and the MNA matrix is solved.
 *
 * @param cfg      Amplifier configuration
 * @param opamp    Op-amp parameters
 * @param v_in     Input voltage (V)
 * @param v_nodes  Output array: [v_plus, v_minus, v_out, v_load]
 * @param i_branch Output array: [i_in, i_fb, i_load]
 * @return         1 if op-amp in linear region, 0 if saturated
 *
 * Reference: Ho, Ruehli, Brennan "The Modified Nodal Approach to Network Analysis" (1975)
 */
int opamp_cfg_solve_dc_operating_point(const AmplifierConfig *cfg,
                                        const OpAmpParams *opamp,
                                        double v_in,
                                        double v_nodes[4],
                                        double i_branch[3]);

/**
 * opamp_cfg_load_drive_check - Verify op-amp can drive the load.
 *
 * Checks:
 *   1. Output current < I_sc (short-circuit current limit)
 *   2. Output voltage swing sufficient
 *   3. Load impedance not too low (causing excessive output current)
 *
 * @param cfg      Amplifier configuration
 * @param opamp    Op-amp parameters
 * @param v_out    Required output voltage (V)
 * @return         1 if load can be driven, 0 if insufficient
 */
int opamp_cfg_load_drive_check(const AmplifierConfig *cfg,
                                const OpAmpParams *opamp,
                                double v_out);

#endif /* OPAMP_CONFIG_H */
