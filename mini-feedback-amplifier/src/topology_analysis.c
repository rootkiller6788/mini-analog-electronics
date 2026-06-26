/**
 * @file topology_analysis.c
 * @brief Four Feedback Amplifier Topologies — Complete Analysis
 *
 * Implements the analysis of all four classical feedback topologies
 * based on two-port network theory and h-parameter extraction.
 * Each topology analysis is a complete, self-contained design
 * methodology following Sedra & Smith §11.2–§11.5.
 *
 * The four topologies form a complete classification:
 *   1. Series-Shunt  (voltage amplifier):  high R_in, low R_out
 *   2. Shunt-Series  (current amplifier):  low R_in, high R_out
 *   3. Series-Series (transconductance):   high R_in, high R_out
 *   4. Shunt-Shunt   (transimpedance):     low R_in, low R_out
 *
 * Each topology implements a different impedance transformation
 * pattern via the feedback network, making each suited to specific
 * application requirements.
 *
 * Knowledge coverage:
 *   L2: Complete — all four topologies analyzed
 *   L6: Complete — design problems (non-inverting amp, transimpedance amp)
 *   L7: Partial — application to photodiode amplifiers
 */

#include "amplifier_topologies.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* =========================================================================
 * L2: Extract Basic Amplifier (A-circuit) from Measurement
 * ========================================================================= */

void topology_extract_basic_amplifier(FeedbackTopology topology,
                                       double total_gain,
                                       double total_input_z,
                                       double total_output_z,
                                       const BetaNetwork *beta_net,
                                       TopologyAnalysis *analysis)
{
    /* Given measured parameters of the complete (closed-loop) amplifier
     * and knowledge of the β-network, extract the basic amplifier
     * (A-circuit) parameters. This is the inverse problem to amplifier
     * design.
     *
     * Method (Sedra & Smith §11.3.1):
     *
     * 1. Determine the loading effects of the β-network on the
     *    input and output of the A-circuit.
     *
     * 2. Remove the loading to obtain A, R_i, R_o:
     *    - For shunt at input:  R_i' = R_i || h11, so R_i = R_i'·h11/(h11 - R_i')
     *    - For series at input: R_i' = R_i + h11, so R_i = R_i' - h11
     *    - For shunt at output: R_o' = R_o || (1/h22), so R_o = R_o'·(1/h22)/((1/h22) - R_o')
     *    - For series at output: R_o' = R_o + (1/h22), so R_o = R_o' - (1/h22)
     *
     * 3. Compute the basic gain A from the loaded gain and β:
     *    A_loaded = total_gain_loaded
     *    A = A_loaded · (1 - β·A_loaded)  [reverse Black's formula]
     */

    if (!analysis || !beta_net) return;

    memset(analysis, 0, sizeof(TopologyAnalysis));
    analysis->topology = topology;
    analysis->beta_net = *beta_net;

    /* Determine amplifier type from topology */
    switch (topology) {
        case FB_SERIES_SHUNT:  analysis->amp_type = AMP_VOLTAGE;       break;
        case FB_SHUNT_SERIES:  analysis->amp_type = AMP_CURRENT;       break;
        case FB_SERIES_SERIES: analysis->amp_type = AMP_TRANSCONDUCT; break;
        case FB_SHUNT_SHUNT:   analysis->amp_type = AMP_TRANSIMPED;   break;
        default:               analysis->amp_type = AMP_VOLTAGE;       break;
    }

    /* Step 1: Loaded input impedance */
    double ri_loaded, ro_loaded;
    switch (topology) {
        case FB_SERIES_SHUNT:
        case FB_SERIES_SERIES:
            /* Series mixing: β-network loads the amplifier INPUT in series
             * The total input Z includes the β-network h11 in series:
             * R_i_total = R_i + h11
             * Actually for series mixing, the β-network IS in series,
             * but h11 is the input impedance of β with output SHORTED.
             * The effective input loading is more subtle.
             *
             * For the purpose of extraction: R_i_loaded = total_input_z
             * But we need to account for β-network loading. */
            ri_loaded = total_input_z;
            if (beta_net->input_loading_z > 0) {
                /* Series: R_i' = R_i + Z_in_loading */
                ri_loaded = total_input_z - beta_net->input_loading_z;
            }
            break;

        case FB_SHUNT_SERIES:
        case FB_SHUNT_SHUNT:
            /* Shunt mixing: β-network appears in parallel with input */
            ri_loaded = total_input_z;
            if (beta_net->input_loading_z > 0 &&
                beta_net->input_loading_z > total_input_z) {
                /* Parallel combination: R_i' = R_i || Z_load
                 * 1/R_i' = 1/R_i + 1/Z_load
                 * 1/R_i = 1/R_i' - 1/Z_load */
                ri_loaded = 1.0 / (1.0/total_input_z -
                                   1.0/beta_net->input_loading_z);
            }
            break;

        default:
            ri_loaded = total_input_z;
            break;
    }

    /* Step 2: Loaded output impedance */
    switch (topology) {
        case FB_SERIES_SHUNT:
        case FB_SHUNT_SHUNT:
            /* Shunt sampling: β-network appears in parallel with output */
            ro_loaded = total_output_z;
            if (beta_net->output_loading_z > 0 &&
                beta_net->output_loading_z > total_output_z) {
                ro_loaded = 1.0 / (1.0/total_output_z -
                                   1.0/beta_net->output_loading_z);
            }
            break;

        case FB_SHUNT_SERIES:
        case FB_SERIES_SERIES:
            /* Series sampling: β-network appears in series with output */
            ro_loaded = total_output_z;
            if (beta_net->output_loading_z > 0) {
                ro_loaded = total_output_z - beta_net->output_loading_z;
            }
            break;

        default:
            ro_loaded = total_output_z;
            break;
    }

    /* Clamp to physically valid values */
    if (ri_loaded <= 0) ri_loaded = total_input_z;
    if (ro_loaded <= 0) ro_loaded = total_output_z;

    analysis->loaded_input_z = ri_loaded;
    analysis->loaded_output_z = ro_loaded;

    /* Step 3: Loaded gain (accounting for β-network loading)
     * For the measured total gain with feedback:
     *   A_f = total_gain = A_loaded / (1 + A_loaded · β)
     *   → A_loaded = total_gain / (1 - total_gain · β)
     *
     * This is the reverse of Black's formula applied to the
     * loaded gain (which already accounts for loading).
     */
    double beta = beta_net->beta;
    if (beta > 1e-15 && total_gain * beta < 1.0) {
        analysis->loaded_gain = total_gain / (1.0 - total_gain * beta);
    } else if (beta <= 1e-15) {
        analysis->loaded_gain = total_gain; /* No feedback */
    } else {
        analysis->loaded_gain = total_gain; /* Deep feedback or invalid */
    }

    /* Basic gain = loaded_gain (loading is already removed above) */
    analysis->basic_gain = analysis->loaded_gain;
    analysis->basic_input_z = ri_loaded;
    analysis->basic_output_z = ro_loaded;

    /* Closed-loop results */
    analysis->closed_loop_gain = total_gain;
    analysis->loop_gain = analysis->loaded_gain * beta;
    analysis->desensitivity = 1.0 + analysis->loop_gain;

    /* Impedance transformation */
    switch (topology) {
        case FB_SERIES_SHUNT:
            analysis->closed_input_z = ri_loaded * analysis->desensitivity;
            analysis->closed_output_z = ro_loaded / analysis->desensitivity;
            break;
        case FB_SHUNT_SERIES:
            analysis->closed_input_z = ri_loaded / analysis->desensitivity;
            analysis->closed_output_z = ro_loaded * analysis->desensitivity;
            break;
        case FB_SERIES_SERIES:
            analysis->closed_input_z = ri_loaded * analysis->desensitivity;
            analysis->closed_output_z = ro_loaded * analysis->desensitivity;
            break;
        case FB_SHUNT_SHUNT:
            analysis->closed_input_z = ri_loaded / analysis->desensitivity;
            analysis->closed_output_z = ro_loaded / analysis->desensitivity;
            break;
        default:
            break;
    }

    analysis->beta_net.beta = beta;
}

/* =========================================================================
 * L2: Series-Shunt Topology Analysis (Voltage Amplifier)
 * ========================================================================= */

void topology_analyze_series_shunt(double open_loop_gain,
                                    double beta,
                                    double input_z,
                                    double output_z,
                                    TopologyAnalysis *analysis)
{
    /* Series-Shunt Feedback (Voltage Amplifier)
     *
     * Configuration:
     *   - Series mixing at input: the feedback voltage V_f is applied
     *     in SERIES with the input voltage V_s. The error voltage
     *     V_ε = V_s - V_f drives the basic amplifier.
     *   - Shunt sampling at output: the output voltage V_o is sampled
     *     in parallel (shunt) with the load.
     *
     * Circuit structure (ideal):
     *   - β-network: resistive voltage divider (R1, R2)
     *   - β = R2 / (R1 + R2) = V_f / V_o
     *   - 1/β = 1 + R1/R2 = ideal closed-loop gain (for A → ∞)
     *
     * Properties:
     *   A_f = A_V / (1 + A_V·β)          [voltage gain, V/V]
     *   R_if = R_i · (1 + Aβ)            [input Z increases]
     *   R_of = R_o / (1 + Aβ)            [output Z decreases]
     *
     * This topology produces a precision voltage amplifier with:
     *   - High input impedance (good for voltage sensing)
     *   - Low output impedance (good for voltage driving)
     *
     * Classic example: Non-inverting op-amp configuration
     *
     * Reference: Sedra & Smith §11.3 — Series-Shunt Feedback
     */

    if (!analysis) return;

    memset(analysis, 0, sizeof(TopologyAnalysis));
    analysis->topology = FB_SERIES_SHUNT;
    analysis->amp_type = AMP_VOLTAGE;

    analysis->basic_gain = open_loop_gain;
    analysis->basic_input_z = input_z;
    analysis->basic_output_z = output_z;

    /* β-network: resistive divider is ideal (no loading for series-shunt?) */
    analysis->beta_net.beta = beta;
    /* For an ideal resistive divider:
     *   Input loading (at amp input, series): negligible for ideal op-amp
     *   Output loading (at amp output, shunt): R1 + R2 in parallel with load
     */
    analysis->beta_net.input_loading_z = INFINITY;  /* Series → infinite Z */
    analysis->beta_net.output_loading_z = INFINITY;  /* Ideal β-network has no loading */

    /* Loaded parameters (same as A-circuit since β-network is ideal) */
    double a_loaded = open_loop_gain * input_z / (input_z + 0);
    analysis->loaded_gain = a_loaded > 0 ? a_loaded : open_loop_gain;
    analysis->loaded_input_z = input_z;
    analysis->loaded_output_z = output_z;

    /* Loop gain */
    analysis->loop_gain = open_loop_gain * beta;
    analysis->desensitivity = 1.0 + analysis->loop_gain;

    /* Closed-loop results */
    if (analysis->desensitivity > 1e-15) {
        analysis->closed_loop_gain = open_loop_gain / analysis->desensitivity;
        analysis->closed_input_z = input_z * analysis->desensitivity;
        analysis->closed_output_z = output_z / analysis->desensitivity;
    } else {
        analysis->closed_loop_gain = INFINITY;
        analysis->closed_input_z = INFINITY;
        analysis->closed_output_z = 0;
    }
}

/* =========================================================================
 * L2: Shunt-Series Topology Analysis (Current Amplifier)
 * ========================================================================= */

void topology_analyze_shunt_series(double open_loop_gain,
                                    double beta,
                                    double input_z,
                                    double output_z,
                                    TopologyAnalysis *analysis)
{
    /* Shunt-Series Feedback (Current Amplifier)
     *
     * Configuration:
     *   - Shunt mixing at input: the feedback current I_f is applied
     *     in SHUNT (parallel) with the input current I_s. The error
     *     current I_ε = I_s - I_f drives the basic amplifier.
     *   - Series sampling at output: the output current I_o is sampled
     *     in SERIES with the load, typically via a current-sensing
     *     resistor R_sense.
     *
     * Circuit structure:
     *   - β-network: typically a resistive T or Π network
     *   - β = I_f / I_o (current transfer ratio, dimensionless)
     *
     * Properties:
     *   A_f = A_I / (1 + A_I·β)          [current gain, A/A]
     *   R_if = R_i / (1 + Aβ)            [input Z decreases]
     *   R_of = R_o · (1 + Aβ)            [output Z increases]
     *
     * This topology produces a precision current amplifier with:
     *   - Low input impedance (current sensing)
     *   - High output impedance (current sourcing)
     *   - Ideal for: current mirrors, LED drivers, current DACs
     *
     * Reference: Sedra & Smith §11.4 — Shunt-Series Feedback
     */

    if (!analysis) return;

    memset(analysis, 0, sizeof(TopologyAnalysis));
    analysis->topology = FB_SHUNT_SERIES;
    analysis->amp_type = AMP_CURRENT;

    analysis->basic_gain = open_loop_gain;
    analysis->basic_input_z = input_z;
    analysis->basic_output_z = output_z;

    analysis->beta_net.beta = beta;
    analysis->beta_net.input_loading_z = INFINITY;   /* Shunt → ideal short */
    analysis->beta_net.output_loading_z = 0.0;       /* Series → ideal open */

    analysis->loaded_gain = open_loop_gain;
    analysis->loaded_input_z = input_z;
    analysis->loaded_output_z = output_z;

    analysis->loop_gain = open_loop_gain * beta;
    analysis->desensitivity = 1.0 + analysis->loop_gain;

    if (analysis->desensitivity > 1e-15) {
        analysis->closed_loop_gain = open_loop_gain / analysis->desensitivity;
        analysis->closed_input_z = input_z / analysis->desensitivity;
        analysis->closed_output_z = output_z * analysis->desensitivity;
    } else {
        analysis->closed_loop_gain = INFINITY;
        analysis->closed_input_z = 0;
        analysis->closed_output_z = INFINITY;
    }
}

/* =========================================================================
 * L2: Series-Series Topology Analysis (Transconductance Amplifier)
 * ========================================================================= */

void topology_analyze_series_series(double open_loop_gain,
                                     double beta,
                                     double input_z,
                                     double output_z,
                                     TopologyAnalysis *analysis)
{
    /* Series-Series Feedback (Transconductance Amplifier)
     *
     * Configuration (a.k.a. current-series feedback):
     *   - Series mixing at input: feedback voltage V_f is in series
     *     with input voltage V_s.
     *   - Series sampling at output: output current I_o is sensed
     *     by a series resistor, producing V_f = I_o · R_sense.
     *
     * Circuit structure:
     *   - Feedback network: typically a resistor R_E (emitter degeneration)
     *     or R_S (source degeneration) in series with the output.
     *   - β = V_f / I_o = R_sense (Ω)
     *
     * Properties:
     *   A_f = G_mf = G_m / (1 + G_m·β)   [transconductance, S = A/V]
     *   R_if = R_i · (1 + Aβ)            [input Z increases]
     *   R_of = R_o · (1 + Aβ)            [output Z increases]
     *
     * Note: β has units of Ω (V/A), and G_m·β is dimensionless.
     *
     * This topology is the basis of:
     *   - Emitter/source degeneration in BJT/FET amplifiers
     *   - Operational Transconductance Amplifiers (OTA)
     *   - Voltage-to-current converters
     *   - Gm-C filter design
     *
     * The key benefit is linearizing the transconductance:
     *   G_mf = 1/β = 1/R_E  (for G_m·R_E >> 1)
     * making I_o = V_in / R_E, independent of transistor gm.
     *
     * Reference: Sedra & Smith §11.5 — Series-Series Feedback
     */

    if (!analysis) return;

    memset(analysis, 0, sizeof(TopologyAnalysis));
    analysis->topology = FB_SERIES_SERIES;
    analysis->amp_type = AMP_TRANSCONDUCT;

    analysis->basic_gain = open_loop_gain;
    analysis->basic_input_z = input_z;
    analysis->basic_output_z = output_z;

    analysis->beta_net.beta = beta;  /* β = R_sense (Ω) */
    analysis->beta_net.input_loading_z = INFINITY;
    analysis->beta_net.output_loading_z = 0.0;

    analysis->loaded_gain = open_loop_gain;
    analysis->loaded_input_z = input_z;
    analysis->loaded_output_z = output_z;

    analysis->loop_gain = open_loop_gain * beta;
    analysis->desensitivity = 1.0 + analysis->loop_gain;

    if (analysis->desensitivity > 1e-15) {
        analysis->closed_loop_gain = open_loop_gain / analysis->desensitivity;
        analysis->closed_input_z = input_z * analysis->desensitivity;
        analysis->closed_output_z = output_z * analysis->desensitivity;
    } else {
        analysis->closed_loop_gain = INFINITY;
        analysis->closed_input_z = INFINITY;
        analysis->closed_output_z = INFINITY;
    }
}

/* =========================================================================
 * L2: Shunt-Shunt Topology Analysis (Transimpedance Amplifier)
 * ========================================================================= */

void topology_analyze_shunt_shunt(double open_loop_gain,
                                   double beta,
                                   double input_z,
                                   double output_z,
                                   TopologyAnalysis *analysis)
{
    /* Shunt-Shunt Feedback (Transimpedance Amplifier)
     *
     * Configuration:
     *   - Shunt mixing at input: feedback current I_f is mixed
     *     in parallel with input current I_s.
     *   - Shunt sampling at output: output voltage V_o is sampled
     *     and converted to I_f via a resistor R_f.
     *
     * Circuit structure:
     *   - A single feedback resistor R_f from output to inverting input
     *   - β = I_f / V_o = -1/R_f (Siemens, A/V)
     *   - The negative sign reflects the inverting nature.
     *
     * Properties:
     *   A_f = R_mf = R_m / (1 + R_m·β)   [transimpedance, Ω = V/A]
     *   R_if = R_i / (1 + Aβ)            [input Z decreases]
     *   R_of = R_o / (1 + Aβ)            [output Z decreases]
     *
     * Note: β has units of S (A/V), and R_m·β is dimensionless.
     * For large loop gain: R_mf ≈ 1/β = -R_f
     *
     * This is the inverting amplifier configuration and is the
     * standard topology for:
     *   - Photodiode transimpedance amplifiers (TIA)
     *   - Current-input ADC drivers
     *   - Charge amplifiers (with capacitive feedback)
     *   - Inverting voltage amplifiers (with R_f/R_in)
     *
     * Key design challenge: stability with input capacitance
     * (photodiode junction capacitance + op-amp input capacitance)
     * forms a pole with R_f, requiring careful compensation.
     *
     * Reference: Sedra & Smith §11.5 — Shunt-Shunt Feedback
     */

    if (!analysis) return;

    memset(analysis, 0, sizeof(TopologyAnalysis));
    analysis->topology = FB_SHUNT_SHUNT;
    analysis->amp_type = AMP_TRANSIMPED;

    analysis->basic_gain = open_loop_gain;
    analysis->basic_input_z = input_z;
    analysis->basic_output_z = output_z;

    analysis->beta_net.beta = beta;  /* β = 1/R_f (S) */
    analysis->beta_net.input_loading_z = INFINITY;
    analysis->beta_net.output_loading_z = INFINITY;

    analysis->loaded_gain = open_loop_gain;
    analysis->loaded_input_z = input_z;
    analysis->loaded_output_z = output_z;

    analysis->loop_gain = open_loop_gain * beta;
    analysis->desensitivity = 1.0 + analysis->loop_gain;

    if (analysis->desensitivity > 1e-15) {
        analysis->closed_loop_gain = open_loop_gain / analysis->desensitivity;
        analysis->closed_input_z = input_z / analysis->desensitivity;
        analysis->closed_output_z = output_z / analysis->desensitivity;
    } else {
        analysis->closed_loop_gain = INFINITY;
        analysis->closed_input_z = 0;
        analysis->closed_output_z = 0;
    }
}

/* =========================================================================
 * L5: Loading Effect Analysis
 * ========================================================================= */

double topology_input_loading(FeedbackTopology topology,
                               double amp_input_z,
                               const BetaNetwork *beta_net)
{
    /* Compute the effective input impedance of the amplifier
     * including the loading effect of the β-network.
     *
     * The loading effect is determined by the mixing topology:
     *
     * For Series mixing (FB_SERIES_SHUNT, FB_SERIES_SERIES):
     *   The β-network appears in SERIES with the amplifier input.
     *   Its h11 parameter adds directly:
     *     Z_in_loaded = R_i + h11
     *   If the β-network is ideal (no loading), h11 = 0.
     *
     * For Shunt mixing (FB_SHUNT_SERIES, FB_SHUNT_SHUNT):
     *   The β-network appears in PARALLEL with the amplifier input.
     *   Its h11 parameter shunts the input:
     *     Z_in_loaded = R_i || h11 = (R_i · h11) / (R_i + h11)
     *   If h11 = 0 (ideal), Z_in_loaded = 0 (virtual ground).
     *
     * Reference: Sedra & Smith §11.3, Table 11.1 — Loading Effects
     */

    if (!beta_net) return amp_input_z;
    if (amp_input_z <= 0) return 0;

    double z_load = beta_net->input_loading_z;
    if (z_load <= 0 || isinf(z_load)) return amp_input_z;

    switch (topology) {
        case FB_SERIES_SHUNT:
        case FB_SERIES_SERIES:
            /* Series loading: Z_eff = R_i + Z_load */
            return amp_input_z + z_load;

        case FB_SHUNT_SERIES:
        case FB_SHUNT_SHUNT:
            /* Shunt loading: Z_eff = R_i || Z_load */
            return (amp_input_z * z_load) / (amp_input_z + z_load);

        default:
            return amp_input_z;
    }
}

double topology_output_loading(FeedbackTopology topology,
                                double amp_output_z,
                                const BetaNetwork *beta_net)
{
    /* Compute the effective output impedance of the amplifier
     * including the loading effect of the β-network.
     *
     * For Shunt sampling (FB_SERIES_SHUNT, FB_SHUNT_SHUNT):
     *   The β-network appears in PARALLEL with the output.
     *   Z_out_loaded = R_o || Z_out_load
     *
     * For Series sampling (FB_SHUNT_SERIES, FB_SERIES_SERIES):
     *   The β-network appears in SERIES with the output.
     *   Z_out_loaded = R_o + Z_out_load
     *
     * Reference: Sedra & Smith §11.3, Table 11.1
     */

    if (!beta_net) return amp_output_z;
    if (amp_output_z <= 0) return 0;

    double z_load = beta_net->output_loading_z;
    if (z_load <= 0 || isinf(z_load)) return amp_output_z;

    switch (topology) {
        case FB_SERIES_SHUNT:
        case FB_SHUNT_SHUNT:
            /* Shunt loading: Z_eff = R_o || Z_load */
            return (amp_output_z * z_load) / (amp_output_z + z_load);

        case FB_SHUNT_SERIES:
        case FB_SERIES_SERIES:
            /* Series loading: Z_eff = R_o + Z_load */
            return amp_output_z + z_load;

        default:
            return amp_output_z;
    }
}

/* =========================================================================
 * L6: Canonical Design — Non-Inverting Voltage Amplifier
 * ========================================================================= */

void topology_design_noninverting_amp(double opamp_gain,
                                       double gbw_hz,
                                       double rf1_ohm,
                                       double rf2_ohm,
                                       TopologyAnalysis *analysis)
{
    /* Design a non-inverting voltage amplifier using an op-amp
     * with resistive feedback (Series-Shunt topology).
     *
     * Circuit:
     *   R_f1 from output to inverting input (feedback resistor)
     *   R_f2 from inverting input to ground
     *
     *   β = R_f2 / (R_f1 + R_f2)
     *   A_f_ideal = 1 + R_f1/R_f2 = 1/β
     *
     * With finite op-amp open-loop gain A₀:
     *   A_f = A₀ / (1 + A₀·β)
     *
     * Bandwidth:
     *   f_{3dB,closed} = GBW / A_f  (single-pole op-amp approximation)
     *
     * This is a canonical design problem in analog electronics,
     * illustrating the fundamental tradeoff between gain and
     * bandwidth in feedback amplifiers.
     *
     * Reference: Sedra & Smith §2.3 — Inverting and Non-Inverting Amplifiers
     */

    if (!analysis || rf1_ohm <= 0 || rf2_ohm <= 0) return;

    /* Compute β from resistive divider */
    double beta = rf2_ohm / (rf1_ohm + rf2_ohm);

    /* Build amplifier parameters */
    FeedbackAmplifier amp;
    memset(&amp, 0, sizeof(amp));
    amp.open_loop_gain = opamp_gain;
    amp.feedback_factor = beta;
    amp.polarity = FB_NEGATIVE;
    amp.topology = FB_SERIES_SHUNT;
    amp.input_impedance = 1e6;   /* Typical op-amp: 1 MΩ differential */
    amp.output_impedance = 75.0; /* Typical op-amp: 75 Ω open-loop */
    amp.dominant_pole_hz = gbw_hz / opamp_gain; /* From GBW product */
    amp.unity_gain_freq_hz = gbw_hz;

    /* Analyze as Series-Shunt topology */
    topology_analyze_series_shunt(opamp_gain, beta,
                                   amp.input_impedance,
                                   amp.output_impedance,
                                   analysis);

    /* Performance metrics */
    analysis->closed_loop_gain = feedback_compute_closed_loop_gain(&amp);
    analysis->closed_input_z = feedback_compute_input_impedance(&amp);
    analysis->closed_output_z = feedback_compute_output_impedance(&amp);
    analysis->loop_gain = opamp_gain * beta;
    analysis->desensitivity = 1.0 + analysis->loop_gain;
}

/* =========================================================================
 * L7: Application — Transimpedance Amplifier Design (Photodiode)
 * ========================================================================= */

void topology_design_transimpedance_amp(double opamp_gain,
                                         double gbw_hz,
                                         double rf_ohm,
                                         double photodiode_cap_f,
                                         TopologyAnalysis *analysis,
                                         double *phase_margin_deg)
{
    /* Design a transimpedance amplifier (TIA) for photodiode
     * current-to-voltage conversion. This is a Shunt-Shunt
     * feedback topology and is one of the most common
     * applications of negative feedback in sensor interfaces.
     *
     * Circuit:
     *   R_f from op-amp output to inverting input
     *   Photodiode connected to inverting input (cathode to -input
     *   for photovoltaic mode)
     *
     *   β = -1/R_f  (feedback factor in A/V = S)
     *   Transimpedance gain: V_out/I_in = -R_f (ideal)
     *
     * Stability challenge:
     *   The photodiode capacitance C_j at the inverting input
     *   creates a pole with the feedback resistor R_f:
     *     f_p = 1 / (2π · R_f · C_j)
     *
     *   This pole can cause oscillation if it lies within the
     *   op-amp's bandwidth. Compensation capacitor C_f across
     *   R_f adds a zero to cancel the pole:
     *     C_f = 1 / (2π · R_f · f_u)  (for 45° PM)
     *   where f_u is the unity-gain frequency of the op-amp.
     *
     * Reference:
     *   - Graeme, J. "Photodiode Amplifiers: Op Amp Solutions" (1996)
     *   - Sedra & Smith §11.5, Example 11.9
     *   - Texas Instruments AN-1803 "Design Considerations for a
     *     Transimpedance Amplifier"
     */

    if (!analysis) return;

    double beta = 1.0 / rf_ohm; /* β = 1/R_f in Siemens */

    /* Analyze as Shunt-Shunt topology */
    FeedbackAmplifier amp;
    memset(&amp, 0, sizeof(amp));
    amp.open_loop_gain = opamp_gain;   /* V/V */
    amp.feedback_factor = beta;        /* A/V (= S) */
    amp.polarity = FB_NEGATIVE;
    amp.topology = FB_SHUNT_SHUNT;
    amp.input_impedance = 1e6;         /* Op-amp input Z */
    amp.output_impedance = 75.0;       /* Op-amp output Z */
    amp.dominant_pole_hz = gbw_hz / opamp_gain;
    amp.unity_gain_freq_hz = gbw_hz;

    /* The photodiode capacitance creates a pole at the inverting input:
     * f_pd = 1 / (2π · R_f · C_j) */
    double f_pd = (photodiode_cap_f > 0 && rf_ohm > 0)
        ? 1.0 / (2.0 * M_PI * rf_ohm * photodiode_cap_f)
        : INFINITY;

    /* If this pole is within the op-amp bandwidth, it affects stability */
    if (!isinf(f_pd) && f_pd < gbw_hz) {
        /* The photodiode pole acts as an additional pole in the loop */
        amp.second_pole_hz = f_pd;
    }

    /* Analyze topology */
    topology_analyze_shunt_shunt(opamp_gain, beta,
                                  amp.input_impedance,
                                  amp.output_impedance,
                                  analysis);

    /* Compute phase margin with photodiode pole */
    if (phase_margin_deg) {
        StabilityMargins margins;
        stability_assess_from_poles(&amp, &margins);
        *phase_margin_deg = margins.phase_margin_deg;
    }
}

/* =========================================================================
 * L2: Resistive Divider as β-Network
 * ========================================================================= */

void topology_resistive_divider_beta(double r1_top_ohm,
                                      double r2_bottom_ohm,
                                      BetaNetwork *beta_net)
{
    /* Convert a resistive voltage divider to β-network parameters.
     *
     * R1 (top): from output to feedback node
     * R2 (bottom): from feedback node to ground
     *
     * For Series-Shunt topology (voltage amplifier):
     *   β = V_f / V_o = R2 / (R1 + R2)
     *
     * Loading effects:
     *   Input loading (at amplifier input, series mixing):
     *     To find h11, short the output (V_o = 0):
     *       R1 and R2 appear in parallel at the feedback input.
     *       h11 = R1 || R2
     *
     *   Output loading (at amplifier output, shunt sampling):
     *     To find 1/h22, open the input (I_i = 0):
     *       R1 + R2 appear across the output.
     *       1/h22 = R1 + R2
     *
     * For an ideal resistive divider used as a feedback network,
     * the loading is non-zero and must be accounted for in
     * precision design.
     *
     * Reference: Sedra & Smith §11.3.2, Figure 11.12
     */

    if (!beta_net) return;
    if (r1_top_ohm <= 0 || r2_bottom_ohm <= 0) {
        memset(beta_net, 0, sizeof(BetaNetwork));
        return;
    }

    memset(beta_net, 0, sizeof(BetaNetwork));

    /* β = R2 / (R1 + R2) */
    beta_net->beta = r2_bottom_ohm / (r1_top_ohm + r2_bottom_ohm);

    /* Input loading (looking into β-network at the input port):
     * With output shorted (shunt sampling): R1 || R2 */
    beta_net->input_loading_z = (r1_top_ohm * r2_bottom_ohm) /
                                 (r1_top_ohm + r2_bottom_ohm);

    /* Output loading (looking into β-network at the output port):
     * With input open (series mixing): R1 + R2 */
    beta_net->output_loading_z = r1_top_ohm + r2_bottom_ohm;

    /* Forward transmission is ideally zero (no feedforward) */
    beta_net->forward_gain = 0.0;
}

int topology_validate_beta_network(const BetaNetwork *beta_net)
{
    /* Validate β-network parameters for physical consistency.
     *
     * A valid β-network must satisfy:
     *   1. 0 < β ≤ 1 (passive feedback networks attenuate)
     *   2. All impedances are positive or infinite
     *   3. Forward gain is negligible or zero (no feedforward)
     *
     * Returns 0 if valid, negative error code otherwise.
     */

    if (!beta_net) return -1;

    if (beta_net->beta <= 0.0 || beta_net->beta > 1.0) return -2;
    if (beta_net->input_loading_z < 0.0) return -3;
    if (beta_net->output_loading_z < 0.0) return -4;
    /* Forward gain should ideally be 0; non-zero values indicate
     * feedforward through the β-network, which is typically undesirable
     * but not necessarily invalid. */

    return 0;
}

/* =========================================================================
 * L6: Topology Comparison
 * ========================================================================= */

void topology_compare_all(double open_loop_gain,
                           const double beta[FB_TOPOLOGY_COUNT],
                           double input_z,
                           double output_z,
                           TopologyAnalysis results[FB_TOPOLOGY_COUNT])
{
    /* Compute and compare all four feedback topologies for the
     * same basic amplifier parameters.
     *
     * This enables systematic selection of the optimal topology
     * based on application requirements:
     *
     *   Application           → Recommended Topology
     *   ─────────────────────────────────────────────
     *   Voltage amplification → Series-Shunt
     *   Current amplification → Shunt-Series
     *   Voltage-to-current    → Series-Series
     *   Current-to-voltage    → Shunt-Shunt
     *   High input impedance  → Series-Shunt or Series-Series
     *   Low input impedance   → Shunt-Series or Shunt-Shunt
     *   Low output impedance  → Series-Shunt or Shunt-Shunt
     *   High output impedance → Shunt-Series or Series-Series
     *
     * The comparison reveals the fundamental tradeoffs in feedback
     * topology selection and validates the impedance transformation
     * rules.
     */

    if (!beta || !results) return;

    topology_analyze_series_shunt(open_loop_gain, beta[FB_SERIES_SHUNT],
                                   input_z, output_z,
                                   &results[FB_SERIES_SHUNT]);

    topology_analyze_shunt_series(open_loop_gain, beta[FB_SHUNT_SERIES],
                                   input_z, output_z,
                                   &results[FB_SHUNT_SERIES]);

    topology_analyze_series_series(open_loop_gain, beta[FB_SERIES_SERIES],
                                    input_z, output_z,
                                    &results[FB_SERIES_SERIES]);

    topology_analyze_shunt_shunt(open_loop_gain, beta[FB_SHUNT_SHUNT],
                                  input_z, output_z,
                                  &results[FB_SHUNT_SHUNT]);
}

FeedbackTopology topology_recommend(int need_high_z_in,
                                     int need_low_z_out,
                                     int need_current_out,
                                     int need_voltage_out)
{
    /* Recommend a feedback topology based on application requirements.
     *
     * Selection logic (from Sedra & Smith §11.2, Table 11.3):
     *
     *   High Z_in + Low Z_out  = Series-Shunt (voltage amplifier)
     *   Low Z_in  + High Z_out = Shunt-Series (current amplifier)
     *   High Z_in + High Z_out = Series-Series (transconductance)
     *   Low Z_in  + Low Z_out  = Shunt-Shunt (transimpedance)
     *
     * The topology is determined entirely by the impedance requirements,
     * which are set by the signal source and load characteristics.
     */

    if (need_high_z_in && need_low_z_out) {
        return FB_SERIES_SHUNT;  /* Voltage amplifier */
    }
    if (!need_high_z_in && !need_low_z_out) {
        return FB_SHUNT_SERIES;  /* Current amplifier */
    }
    if (need_high_z_in && !need_low_z_out) {
        return FB_SERIES_SERIES; /* Transconductance */
    }
    if (!need_high_z_in && need_low_z_out) {
        return FB_SHUNT_SHUNT;   /* Transimpedance */
    }

    /* Default: if voltage output is explicitly needed, use Series-Shunt */
    if (need_voltage_out) {
        return FB_SERIES_SHUNT;
    }
    if (need_current_out) {
        return FB_SHUNT_SERIES;
    }

    /* Default fallback */
    return FB_SERIES_SHUNT;
}
