/**
 * @file example1_noninverting_amp.c
 * @brief Example 1: Design and Analysis of a Non-Inverting Voltage Amplifier
 *
 * This example demonstrates the complete design of a non-inverting
 * amplifier using Series-Shunt feedback with an op-amp.
 *
 * Circuit:
 *   R_f1 (feedback resistor) from output to inverting input
 *   R_f2 from inverting input to ground
 *   Signal applied to non-inverting (+) input
 *
 * Design Goals:
 *   - Closed-loop voltage gain: A_f = 1 + R_f1/R_f2 ≈ 21 V/V (26.4 dB)
 *   - Analyze bandwidth extension
 *   - Compute input/output impedance transformation
 *   - Verify stability margins
 *
 * The non-inverting amplifier is the most fundamental application of
 * Series-Shunt feedback, used extensively in:
 *   - Audio preamplifiers (high input Z, low output Z)
 *   - Sensor signal conditioning (buffer + gain)
 *   - Active filter building blocks
 *   - Precision voltage amplification
 *
 * Reference: Sedra & Smith §2.3, Example 2.2
 */

#include "feedback_amplifier.h"
#include "stability_analysis.h"
#include "amplifier_topologies.h"
#include <math.h>
#include <stdio.h>

int main(void)
{
    printf("=== Example 1: Non-Inverting Voltage Amplifier Design ===\n\n");

    /* --- Design Parameters --- */
    double rf1     = 20000.0;  /* Feedback resistor (Ω), 20 kΩ */
    double rf2     = 1000.0;   /* Ground resistor (Ω), 1 kΩ    */
    double a_ol    = 200000.0; /* Op-amp open-loop gain (V/V), 106 dB */
    double gbw     = 1.0e6;    /* Gain-bandwidth product (Hz), 1 MHz */
    double ri      = 2.0e6;    /* Op-amp differential input impedance (Ω) */
    double ro      = 75.0;     /* Op-amp open-loop output impedance (Ω) */

    printf("--- Design Specifications ---\n");
    printf("  R_f1 (feedback)      = %.1f kΩ\n", rf1 / 1000.0);
    printf("  R_f2 (to ground)     = %.2f kΩ\n", rf2 / 1000.0);
    printf("  Op-amp open-loop gain A₀ = %.0f (%.1f dB)\n",
           a_ol, 20.0 * log10(a_ol));
    printf("  Gain-bandwidth product GBW = %.1f MHz\n", gbw / 1.0e6);

    /* Compute feedback factor β */
    double beta = rf2 / (rf1 + rf2);
    double af_ideal = 1.0 / beta;

    printf("\n--- Feedback Analysis ---\n");
    printf("  β = R_f2/(R_f1+R_f2) = %.4f\n", beta);
    printf("  Ideal closed-loop gain (A→∞): A_f = %.1f V/V (%.1f dB)\n",
           af_ideal, 20.0 * log10(af_ideal));

    /* Build amplifier model */
    FeedbackAmplifier amp;
    amp.open_loop_gain   = a_ol;
    amp.feedback_factor  = beta;
    amp.polarity         = FB_NEGATIVE;
    amp.topology         = FB_SERIES_SHUNT;
    amp.input_impedance  = ri;
    amp.output_impedance = ro;
    amp.dominant_pole_hz = gbw / a_ol;  /* f_p1 ≈ 5 Hz from GBW = A₀·f_p1 */
    amp.second_pole_hz   = gbw / 3.0;    /* Assume 2nd pole at ~330 kHz */
    amp.third_pole_hz    = 0.0;
    amp.unity_gain_freq_hz = gbw;

    /* Compute performance metrics */
    FeedbackMetrics metrics;
    feedback_compute_metrics(&amp, &metrics);

    printf("\n--- Closed-Loop Performance ---\n");
    printf("  A_f (exact)  = %.4f V/V\n", metrics.closed_loop_gain);
    printf("  Error from ideal = %.4f %%\n",
           100.0 * fabs(metrics.closed_loop_gain - af_ideal) / af_ideal);
    printf("  Loop gain L = A₀β = %.0f (%.1f dB)\n",
           metrics.loop_gain, 20.0 * log10(metrics.loop_gain));
    printf("  Desensitivity factor D = 1+L = %.0f\n", metrics.desensitivity);
    printf("  Gain sensitivity S = 1/D = %.6f\n", metrics.gain_sensitivity);
    printf("    (10%% change in A₀ → %.4f%% change in A_f)\n",
           10.0 * metrics.gain_sensitivity * 100.0);

    /* Bandwidth analysis */
    printf("\n--- Bandwidth Analysis ---\n");
    printf("  Open-loop f_{-3dB}  = %.1f Hz\n", amp.dominant_pole_hz);
    printf("  Closed-loop f_{-3dB} = %.1f Hz (%.2f kHz)\n",
           metrics.bandwidth_hz, metrics.bandwidth_hz / 1000.0);
    printf("  BW extension factor  = %.0f×\n",
           metrics.bandwidth_hz / amp.dominant_pole_hz);

    double gbw_cl;
    double gbw_open = feedback_compute_gbw_product(&amp, &gbw_cl);
    printf("  GBW (open-loop)  = %.2f MHz\n", gbw_open / 1.0e6);
    printf("  GBW (closed-loop)= %.2f MHz\n", gbw_cl / 1.0e6);
    printf("  GBW conservation: ratio = %.6f (should ≈ 1.0)\n",
           gbw_cl / gbw_open);

    /* Impedance transformation */
    printf("\n--- Impedance Transformation ---\n");
    printf("  Input Z:  %.2f MΩ → %.2f MΩ  (×%.0f)\n",
           amp.input_impedance / 1.0e6,
           metrics.closed_input_z / 1.0e6,
           metrics.closed_input_z / amp.input_impedance);
    printf("  Output Z: %.2f Ω → %.4f Ω  (÷%.0f)\n",
           amp.output_impedance,
           metrics.closed_output_z,
           amp.output_impedance / metrics.closed_output_z);

    /* Distortion */
    printf("\n--- Distortion Analysis ---\n");
    printf("  Open-loop THD estimate: 1%%\n");
    double hd2 = feedback_compute_distortion_reduction(&amp, 1.0, 2);
    double hd3 = feedback_compute_distortion_reduction(&amp, 1.0, 3);
    printf("  Closed-loop HD2 ≈ %.6f %% (reduced by %.1f dB)\n",
           hd2, -20.0 * log10(metrics.desensitivity) * 2.0);
    printf("  Closed-loop HD3 ≈ %.6f %% (reduced by %.1f dB)\n",
           hd3, -20.0 * log10(metrics.desensitivity) * 3.0);

    /* Stability check */
    printf("\n--- Stability Verification ---\n");
    StabilityMargins margins;
    stability_assess_from_poles(&amp, &margins);
    printf("  Phase margin: %.1f°\n", margins.phase_margin_deg);
    printf("  Gain margin:  %.1f dB\n", margins.gain_margin_db);
    printf("  Unity-gain frequency: %.1f kHz\n",
           margins.unity_gain_freq_hz / 1000.0);
    printf("  Classification: %s\n",
           margins.classification == STABILITY_STABLE ? "STABLE ✓" :
           (margins.classification == STABILITY_MARGINALLY_STABLE ?
            "MARGINALLY STABLE ⚠" : "UNSTABLE ✗"));

    /* Topology analysis */
    printf("\n--- Topology: Series-Shunt (Voltage Amplifier) ---\n");
    TopologyAnalysis ta;
    topology_design_noninverting_amp(a_ol, gbw, rf1, rf2, &ta);
    printf("  Topology:          %s\n", feedback_topology_name(ta.topology));
    printf("  Loaded gain A':    %.1f dB\n", 20.0 * log10(ta.loaded_gain));
    printf("  Closed-loop gain:  %.1f (%.1f dB)\n",
           ta.closed_loop_gain, 20.0 * log10(ta.closed_loop_gain));
    printf("  Z_in loaded:       %.2f MΩ\n", ta.loaded_input_z / 1.0e6);
    printf("  Z_in closed:       %.2f MΩ\n", ta.closed_input_z / 1.0e6);
    printf("  Z_out loaded:      %.1f Ω\n", ta.loaded_output_z);
    printf("  Z_out closed:      %.3f Ω\n", ta.closed_output_z);

    printf("\n=== Design Summary ===\n");
    printf("The non-inverting amplifier achieves:\n");
    printf("  • Precise gain set by resistor ratio (A_f ≈ %.1f V/V)\n", af_ideal);
    printf("  • High input impedance (%.1f MΩ) — ideal for voltage sensing\n",
           metrics.closed_input_z / 1.0e6);
    printf("  • Low output impedance (%.3f Ω) — ideal for driving loads\n",
           metrics.closed_output_z);
    printf("  • %.1f kHz bandwidth for %.1f gain\n",
           metrics.bandwidth_hz / 1000.0, metrics.closed_loop_gain);
    printf("  • Excellent linearity via %.0f× distortion reduction\n",
           metrics.desensitivity);

    return 0;
}
