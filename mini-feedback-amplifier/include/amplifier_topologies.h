/**
 * @file amplifier_topologies.h
 * @brief Four Fundamental Feedback Amplifier Topologies
 *
 * Complete analysis of the four classical feedback topologies as
 * defined by the method of mixing (series vs. shunt at input) and
 * sampling (series vs. shunt at output):
 *
 *   Topology     Mixing   Sampling  Ideal Transfer  β-Definition
 *   ----------   ------   --------  --------------  ------------
 *   S-S (Volt)   Series   Shunt     A_f = Vo/Vs     V_f/V_o
 *   Sh-Sh (TI)   Shunt    Shunt     A_f = Vo/Is     I_f/V_o
 *   S-Sh (TC)    Series   Series    A_f = Io/Vs     V_f/I_o
 *   Sh-S (Curr)  Shunt    Series    A_f = Io/Is     I_f/I_o
 *
 * Each topology represents a complete feedback amplifier design
 * methodology based on two-port network theory and h-parameter analysis.
 *
 * Reference:
 *   - Sedra & Smith §11.2–§11.5 — Feedback Topologies
 *   - Gray et al. §8.2 — Ideal Feedback Equation
 *   - Cherry & Hooper, "Amplifying Devices and Low-Pass Amplifier
 *     Design" (1968) — Return ratio vs. two-port methods
 *
 * Knowledge Layer: L2 (Topology concepts) + L6 (Design problems)
 */

#ifndef AMPLIFIER_TOPOLOGIES_H
#define AMPLIFIER_TOPOLOGIES_H

#include "feedback_amplifier.h"
#include "stability_analysis.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
 * L1: Topology-Specific Structures
 * -------------------------------------------------------------------------- */

/**
 * @brief β-network parameters (feedback network characterization).
 *
 * The β-network is characterized by its h-parameters (hybrid)
 * or y-parameters (admittance), depending on the topology.
 * This structure captures the essential loading effects:
 *   - Input loading (how β-network loads the amplifier input)
 *   - Output loading (how β-network loads the amplifier output)
 *   - Forward transmission (usually negligible)
 *   - Reverse transmission (the β ratio)
 */
typedef struct {
    double beta;               /**< Feedback factor = V_f/V_o or I_f/I_o      */
    double input_loading_z;    /**< β-network impedance seen at input (Ω)     */
    double output_loading_z;   /**< β-network impedance seen at output (Ω)    */
    double forward_gain;       /**< Forward gain through β-net (ideally 0)    */
} BetaNetwork;

/**
 * @brief h-parameter 2-port representation of the β-network.
 *
 * [V1]   [h11  h12] [I1]
 * [I2] = [h21  h22] [V2]
 *
 * For feedback analysis:
 *   h12 = β (the feedback factor)
 *   h21 is ideally 0 (no feedforward through the β network)
 *   h11 represents input loading
 *   h22 = 1/Z_22 represents output loading
 *
 * Reference: Sedra & Smith §11.2 — Two-port representation
 */
typedef struct {
    double h11;   /**< Input impedance with output shorted (Ω)                */
    double h12;   /**< Reverse voltage transfer = β (dimensionless)          */
    double h21;   /**< Forward current transfer, ideally 0 (dimensionless)   */
    double h22;   /**< Output admittance with input open (S)                  */
} HParameters;

/**
 * @brief Complete topology analysis result.
 *
 * Contains the pre- and post-feedback amplifier parameters,
 * computed according to the specific feedback topology rules.
 */
typedef struct {
    FeedbackTopology  topology;      /**< Which topology was analyzed         */
    AmplifierType     amp_type;      /**< Corresponding amplifier type        */

    /* Basic amplifier parameters (A-circuit, without β loading) */
    double basic_gain;               /**< A (V/V, A/A, S, or Ω)              */
    double basic_input_z;            /**< R_i of basic amplifier (Ω)          */
    double basic_output_z;           /**< R_o of basic amplifier (Ω)          */

    /* Loaded amplifier (A-circuit + β-network loading) */
    double loaded_gain;              /**< A' with loading effects included    */
    double loaded_input_z;           /**< R_i' with loading                   */
    double loaded_output_z;          /**< R_o' with loading                   */

    /* β-network parameters */
    BetaNetwork beta_net;

    /* Closed-loop results */
    double closed_loop_gain;         /**< A_f after closing the loop          */
    double closed_input_z;           /**< R_if with feedback                  */
    double closed_output_z;          /**< R_of with feedback                  */
    double loop_gain;                /**< L = Aβ (loaded gain × beta)        */
    double desensitivity;            /**< D = 1 + Aβ                         */
} TopologyAnalysis;

/* -----------------------------------------------------------------------------
 * L2: Two-Port Network Analysis
 * -------------------------------------------------------------------------- */

/**
 * @brief Compute the A-circuit parameters (basic amplifier without β).
 *
 * For a given feedback topology, this function computes the
 * basic amplifier parameters A, R_i, R_o by removing the β-network
 * and its loading effects.
 *
 * Method (Sedra & Smith §11.3.1):
 *   - For shunt mixing: short-circuit the output of the mixing point
 *   - For series mixing: open-circuit the input to the β-network
 *   - For shunt sampling: short-circuit the output port
 *   - For series sampling: open-circuit the output port
 *
 * Complexity: O(1)
 *
 * @param topology       Feedback topology
 * @param total_gain     Measured gain of the complete amplifier
 * @param total_input_z  Measured input impedance
 * @param total_output_z Measured output impedance
 * @param beta_net       Beta network parameters
 * @param analysis       [out] Completed topology analysis
 */
void topology_extract_basic_amplifier(FeedbackTopology topology,
                                       double total_gain,
                                       double total_input_z,
                                       double total_output_z,
                                       const BetaNetwork *beta_net,
                                       TopologyAnalysis *analysis);

/**
 * @brief Analyze a Series-Shunt (voltage) feedback amplifier.
 *
 * Series mixing at input, shunt sampling at output.
 * This is the most common feedback topology, used in:
 *   - Non-inverting op-amp amplifier
 *   - Emitter follower with feedback
 *   - Voltage regulators
 *
 * Key formulas:
 *   A_f = A / (1 + Aβ)             (closed-loop voltage gain)
 *   R_if = R_i · (1 + Aβ)          (input impedance increases)
 *   R_of = R_o / (1 + Aβ)          (output impedance decreases)
 *
 * Complexity: O(1)
 *
 * @param open_loop_gain    A (V/V, basic amplifier voltage gain)
 * @param beta              β (V/V, feedback network transfer ratio)
 * @param input_z           R_i of basic amplifier (Ω)
 * @param output_z          R_o of basic amplifier (Ω)
 * @param analysis          [out] Completed topology analysis
 */
void topology_analyze_series_shunt(double open_loop_gain,
                                    double beta,
                                    double input_z,
                                    double output_z,
                                    TopologyAnalysis *analysis);

/**
 * @brief Analyze a Shunt-Series (current) feedback amplifier.
 *
 * Shunt mixing at input, series sampling at output.
 * Used in:
 *   - Current mirrors with feedback
 *   - Current-feedback instrumentation amplifiers
 *   - LED/Laser diode drivers
 *
 * Key formulas:
 *   A_f = A / (1 + Aβ)             (closed-loop current gain, A/A)
 *   R_if = R_i / (1 + Aβ)          (input impedance decreases)
 *   R_of = R_o · (1 + Aβ)          (output impedance increases)
 *
 * Complexity: O(1)
 *
 * @param open_loop_gain    A (A/A, basic amplifier current gain)
 * @param beta              β (A/A, feedback network)
 * @param input_z           R_i (Ω)
 * @param output_z          R_o (Ω)
 * @param analysis          [out] Completed topology analysis
 */
void topology_analyze_shunt_series(double open_loop_gain,
                                    double beta,
                                    double input_z,
                                    double output_z,
                                    TopologyAnalysis *analysis);

/**
 * @brief Analyze a Series-Series (transconductance) feedback amplifier.
 *
 * Series mixing at input, series sampling at output.
 * Used in:
 *   - Transconductance amplifiers (voltage-to-current converters)
 *   - OTA-C filter design
 *   - Current-mode signal processing
 *
 * Key formulas:
 *   A_f = A / (1 + Aβ)             (G_mf = I_o/V_s, Siemens)
 *   R_if = R_i · (1 + Aβ)          (input impedance increases)
 *   R_of = R_o · (1 + Aβ)          (output impedance increases)
 *
 * Complexity: O(1)
 *
 * @param open_loop_gain    A (S, basic amplifier transconductance)
 * @param beta              β (V/A = Ω, feedback network)
 * @param input_z           R_i (Ω)
 * @param output_z          R_o (Ω)
 * @param analysis          [out] Completed topology analysis
 */
void topology_analyze_series_series(double open_loop_gain,
                                     double beta,
                                     double input_z,
                                     double output_z,
                                     TopologyAnalysis *analysis);

/**
 * @brief Analyze a Shunt-Shunt (transimpedance) feedback amplifier.
 *
 * Shunt mixing at input, shunt sampling at output.
 * Used in:
 *   - Transimpedance amplifiers (current-to-voltage converters)
 *   - Photodiode amplifiers
 *   - Inverting op-amp configuration
 *
 * Key formulas:
 *   A_f = A / (1 + Aβ)             (R_mf = V_o/I_s, Ω)
 *   R_if = R_i / (1 + Aβ)          (input impedance decreases)
 *   R_of = R_o / (1 + Aβ)          (output impedance decreases)
 *
 * Complexity: O(1)
 *
 * @param open_loop_gain    A (Ω, basic amplifier transimpedance)
 * @param beta              β (A/V = S, feedback network)
 * @param input_z           R_i (Ω)
 * @param output_z          R_o (Ω)
 * @param analysis          [out] Completed topology analysis
 */
void topology_analyze_shunt_shunt(double open_loop_gain,
                                   double beta,
                                   double input_z,
                                   double output_z,
                                   TopologyAnalysis *analysis);

/* -----------------------------------------------------------------------------
 * L5: Loading Effect Analysis
 * -------------------------------------------------------------------------- */

/**
 * @brief Compute the loading effect of the β-network on the amplifier input.
 *
 * The input loading depends on the mixing topology:
 *   - Series mixing:  β-network appears in SERIES with R_i
 *                     effective R_i increases by h11 of β-network
 *   - Shunt mixing:   β-network appears in PARALLEL with R_i
 *                     effective R_i decreases by parallel combination
 *
 * Complexity: O(1)
 *
 * @param topology    Feedback topology
 * @param amp_input_z Basic amplifier input impedance (Ω)
 * @param beta_net    β-network h-parameters
 * @return            Effective input impedance including loading (Ω)
 */
double topology_input_loading(FeedbackTopology topology,
                               double amp_input_z,
                               const BetaNetwork *beta_net);

/**
 * @brief Compute the loading effect of the β-network on the amplifier output.
 *
 * The output loading depends on the sampling topology:
 *   - Shunt sampling:  β-network appears in PARALLEL with R_o
 *                      effective R_o decreases
 *   - Series sampling: β-network appears in SERIES with R_o
 *                      effective R_o increases
 *
 * Complexity: O(1)
 *
 * @param topology     Feedback topology
 * @param amp_output_z Basic amplifier output impedance (Ω)
 * @param beta_net     β-network parameters
 * @return             Effective output impedance including loading (Ω)
 */
double topology_output_loading(FeedbackTopology topology,
                                double amp_output_z,
                                const BetaNetwork *beta_net);

/* -----------------------------------------------------------------------------
 * L6: Canonical Design Problems — Complete Amplifier Design
 * -------------------------------------------------------------------------- */

/**
 * @brief Design a non-inverting voltage amplifier using Series-Shunt feedback.
 *
 * Classic op-amp non-inverting amplifier with resistive divider feedback:
 *
 *   R_f1, R_f2 form the β-network: β = R_f2 / (R_f1 + R_f2)
 *   A_f = 1 + R_f1/R_f2  (for A → ∞)
 *   A_f = A / (1 + Aβ)   (exact, for finite A)
 *
 * This function computes the exact closed-loop parameters given
 * the op-amp open-loop gain, slew rate, GBW, and feedback resistors.
 *
 * Complexity: O(1)
 *
 * @param opamp_gain       Open-loop DC gain A₀ of the op-amp (V/V)
 * @param gbw_hz           Gain-bandwidth product of op-amp (Hz)
 * @param rf1_ohm          Feedback resistor R_f1 (Ω)
 * @param rf2_ohm          Feedback resistor R_f2 (Ω)
 * @param analysis         [out] Topology analysis result
 */
void topology_design_noninverting_amp(double opamp_gain,
                                       double gbw_hz,
                                       double rf1_ohm,
                                       double rf2_ohm,
                                       TopologyAnalysis *analysis);

/**
 * @brief Design a transimpedance amplifier (Shunt-Shunt feedback).
 *
 * Used for photodiode amplification, where the photocurrent is
 * converted to a voltage via feedback resistor R_f:
 *
 *   V_out = -I_in · R_f  (for A → ∞, inverting)
 *   V_out = -I_in · R_f · A/(1+A)  (exact, finite A)
 *
 * The dominant design challenge is stability with the photodiode's
 * junction capacitance C_j at the inverting input, which introduces
 * a pole in the feedback loop.
 *
 * Complexity: O(1)
 *
 * @param opamp_gain       Open-loop gain of the op-amp (V/V)
 * @param gbw_hz           Gain-bandwidth product (Hz)
 * @param rf_ohm           Feedback resistance (Ω)
 * @param photodiode_cap_f Photodiode junction capacitance (F)
 * @param analysis         [out] Topology analysis result
 * @param phase_margin_deg [out] Computed phase margin (may be NULL)
 */
void topology_design_transimpedance_amp(double opamp_gain,
                                         double gbw_hz,
                                         double rf_ohm,
                                         double photodiode_cap_f,
                                         TopologyAnalysis *analysis,
                                         double *phase_margin_deg);

/* -----------------------------------------------------------------------------
 * L2: H-Parameter β-Network Analysis
 * -------------------------------------------------------------------------- */

/**
 * @brief Convert a resistive voltage divider to a β-network.
 *
 * For a resistor divider R1 (top), R2 (bottom) used as a feedback network:
 *
 *   β = R2 / (R1 + R2)                     (feedback factor)
 *   input loading  = R1 || R2              (shunt mixing)
 *   output loading = R1 + R2               (series sampling)
 *
 * Complexity: O(1)
 *
 * @param r1_top_ohm    Top resistor (Ω)
 * @param r2_bottom_ohm Bottom resistor (Ω)
 * @param beta_net      [out] Resulting β-network parameters
 */
void topology_resistive_divider_beta(double r1_top_ohm,
                                      double r2_bottom_ohm,
                                      BetaNetwork *beta_net);

/**
 * @brief Verify that β-network parameters satisfy physical constraints.
 *
 * Checks:
 *   - 0 < β ≤ 1 (feedback factor must be attenuating)
 *   - All impedances are positive and finite
 *   - Forward gain is small in magnitude
 *
 * Complexity: O(1)
 *
 * @param beta_net  β-network parameters
 * @return          0 if valid, negative error code if invalid
 */
int topology_validate_beta_network(const BetaNetwork *beta_net);

/* -----------------------------------------------------------------------------
 * L6: Topology Comparison & Selection
 * -------------------------------------------------------------------------- */

/**
 * @brief Compare all four topologies for a given amplifier specification.
 *
 * This function computes the closed-loop performance for all four
 * topologies given the same basic amplifier parameters, allowing
 * the designer to select the optimal topology based on:
 *   - Gain accuracy
 *   - Input/output impedance requirements
 *   - Bandwidth extension
 *   - Noise and distortion performance
 *
 * Complexity: O(1)
 *
 * @param open_loop_gain  A of basic amplifier (in native units)
 * @param beta            β for each topology (array of 4 values)
 * @param input_z         R_i of basic amplifier (Ω)
 * @param output_z        R_o of basic amplifier (Ω)
 * @param results         [out] Array of 4 topology analyses
 */
void topology_compare_all(double open_loop_gain,
                           const double beta[FB_TOPOLOGY_COUNT],
                           double input_z,
                           double output_z,
                           TopologyAnalysis results[FB_TOPOLOGY_COUNT]);

/**
 * @brief Recommend a topology based on application requirements.
 *
 * Selection criteria:
 *   - High input Z needed → Series mixing (Series-Shunt or Series-Series)
 *   - Low output Z needed → Shunt sampling (Series-Shunt or Shunt-Shunt)
 *   - Current output needed → Series sampling (Shunt-Series or Series-Series)
 *   - Maximum bandwidth → highest desensitivity for minimal β
 *
 * Complexity: O(1)
 *
 * @param need_high_z_in   Non-zero if high input impedance is needed
 * @param need_low_z_out   Non-zero if low output impedance is needed
 * @param need_current_out Non-zero if current output is needed
 * @param need_voltage_out Non-zero if voltage output is needed
 * @return                 Recommended topology
 */
FeedbackTopology topology_recommend(int need_high_z_in,
                                     int need_low_z_out,
                                     int need_current_out,
                                     int need_voltage_out);

#ifdef __cplusplus
}
#endif

#endif /* AMPLIFIER_TOPOLOGIES_H */
