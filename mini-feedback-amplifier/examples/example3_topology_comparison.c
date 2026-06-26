/**
 * @file example3_topology_comparison.c
 * @brief Example 3: Comparison of All Four Feedback Topologies
 *
 * This example analyzes the same basic amplifier in all four feedback
 * topologies, demonstrating the fundamental impedance transformation
 * rules and helping designers select the optimal topology for their
 * application.
 *
 * The example covers:
 *   1. Series-Shunt (voltage amplifier) — for high-Z_in, low-Z_out
 *   2. Shunt-Series (current amplifier) — for low-Z_in, high-Z_out
 *   3. Series-Series (transconductance) — for high-Z_in, high-Z_out
 *   4. Shunt-Shunt (transimpedance) — for low-Z_in, low-Z_out
 *
 * Application mapping:
 *   - GPS receiver LNA → Series-Shunt (voltage gain, 50Ω match)
 *   - Photodiode amplifier → Shunt-Shunt (current-to-voltage)
 *   - LED driver → Shunt-Series (current regulation)
 *   - OTA-C filter → Series-Series (voltage-to-current)
 *
 * Reference: Sedra & Smith §11.2–§11.5, Table 11.3
 */

#include "feedback_amplifier.h"
#include "amplifier_topologies.h"
#include "stability_analysis.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static void print_topology_analysis(const TopologyAnalysis *ta)
{
    printf("  ────────────────────────────────────────\n");
    printf("  Topology:        %s\n", feedback_topology_name(ta->topology));
    printf("  Gain unit:       %s\n", feedback_gain_unit(ta->amp_type));
    printf("  ────────────────────────────────────────\n");
    printf("  Basic gain (A):           %.2f %s\n",
           ta->basic_gain, feedback_gain_unit(ta->amp_type));
    printf("  Loaded gain (A'):         %.2f %s\n",
           ta->loaded_gain, feedback_gain_unit(ta->amp_type));
    printf("  Closed-loop gain (A_f):   %.4f %s\n",
           ta->closed_loop_gain, feedback_gain_unit(ta->amp_type));
    printf("  ────────────────────────────────────────\n");
    printf("  β (feedback factor):      %.6f\n", ta->beta_net.beta);
    printf("  Loop gain (L = Aβ):       %.2f (%.1f dB)\n",
           ta->loop_gain, 20.0 * log10(ta->loop_gain));
    printf("  Desensitivity (D = 1+L):  %.2f\n", ta->desensitivity);
    printf("  ────────────────────────────────────────\n");
    printf("  Basic Z_in:               %.2f kΩ\n",
           ta->basic_input_z / 1000.0);
    printf("  Closed-loop Z_in:         %.2f kΩ\n",
           ta->closed_input_z / 1000.0);
    printf("  Basic Z_out:              %.2f Ω\n", ta->basic_output_z);
    printf("  Closed-loop Z_out:        %.4f Ω\n", ta->closed_output_z);
    printf("  Z_in change:              %s by %.0f×\n",
           ta->closed_input_z > ta->basic_input_z ? "INCREASED" : "DECREASED",
           ta->closed_input_z > ta->basic_input_z ?
           ta->closed_input_z / ta->basic_input_z :
           ta->basic_input_z / ta->closed_input_z);
    printf("  Z_out change:             %s by %.0f×\n",
           ta->closed_output_z > ta->basic_output_z ? "INCREASED" : "DECREASED",
           ta->closed_output_z > ta->basic_output_z ?
           ta->closed_output_z / ta->basic_output_z :
           ta->basic_output_z / ta->closed_output_z);
    printf("  ────────────────────────────────────────\n\n");
}

int main(void)
{
    printf("=== Example 3: Comparison of All Four Feedback Topologies ===\n\n");

    /* Common basic amplifier parameters (a BJT common-emitter stage) */
    double a_basic  = 200.0;    /* ~46 dB voltage gain */
    double ri       = 2500.0;   /* 2.5 kΩ input impedance (r_π) */
    double ro       = 50000.0;  /* 50 kΩ output impedance (r_o) */

    /* β values for each topology (dimensionless or with units) */
    double beta_values[FB_TOPOLOGY_COUNT];

    /* Series-Shunt: β = 0.1 (voltage divider, e.g., R2/(R1+R2))
     *   Ideal gain: A_f ≈ 10 V/V = 20 dB */
    beta_values[FB_SERIES_SHUNT]  = 0.1;     /* V/V — dimensionless */

    /* Shunt-Series: β = 0.05 (current transfer ratio)
     *   Ideal gain: A_f ≈ 20 A/A */
    beta_values[FB_SHUNT_SERIES]  = 0.05;    /* A/A — dimensionless */

    /* Series-Series: β = 100 Ω (current-sense resistor)
     *   G_mf ≈ 1/100 = 10 mS */
    beta_values[FB_SERIES_SERIES] = 100.0;   /* Ω */

    /* Shunt-Shunt: β = 1/10000 = 0.0001 S (feedback resistor 10kΩ)
     *   R_mf ≈ -10000 Ω */
    beta_values[FB_SHUNT_SHUNT]  = 0.0001;   /* S */

    /* Analyze all four topologies */
    printf("--- Common Basic Amplifier Parameters ---\n");
    printf("  Basic gain (all topologies): %.1f\n", a_basic);
    printf("  R_i = %.1f kΩ, R_o = %.1f kΩ\n\n", ri / 1000.0, ro / 1000.0);

    TopologyAnalysis results[FB_TOPOLOGY_COUNT];
    topology_compare_all(a_basic, beta_values, ri, ro, results);

    printf("--- Topology 1: Series-Shunt (Voltage Amplifier) ---\n");
    print_topology_analysis(&results[FB_SERIES_SHUNT]);

    printf("--- Topology 2: Shunt-Series (Current Amplifier) ---\n");
    print_topology_analysis(&results[FB_SHUNT_SERIES]);

    printf("--- Topology 3: Series-Series (Transconductance) ---\n");
    print_topology_analysis(&results[FB_SERIES_SERIES]);

    printf("--- Topology 4: Shunt-Shunt (Transimpedance) ---\n");
    print_topology_analysis(&results[FB_SHUNT_SHUNT]);

    /* Design guidance */
    printf("=== Topology Selection Guide ===\n\n");

    printf("  Desired Impedances        → Topology            → Application\n");
    printf("  ─────────────────────────    ──────────────────    ───────────\n");
    printf("  High Z_in, Low Z_out        Series-Shunt          Voltage amp\n");
    printf("  Low Z_in,  High Z_out       Shunt-Series          Current amp\n");
    printf("  High Z_in, High Z_out       Series-Series         Transconductance\n");
    printf("  Low Z_in,  Low Z_out        Shunt-Shunt           Transimpedance\n\n");

    /* Test topology recommendation function */
    printf("--- Topology Recommendations ---\n");

    struct {
        const char *desc;
        int hi_z_in, lo_z_out, cur_out, volt_out;
    } requirements[] = {
        {"Voltage buffer/follower ", 1, 1, 0, 1},
        {"Current mirror driver   ", 0, 0, 1, 0},
        {"Transconductance stage  ", 1, 0, 1, 0},
        {"Photodiode preamplifier ", 0, 1, 0, 1},
    };

    for (int i = 0; i < 4; i++) {
        FeedbackTopology rec = topology_recommend(
            requirements[i].hi_z_in,
            requirements[i].lo_z_out,
            requirements[i].cur_out,
            requirements[i].volt_out);
        printf("  %-25s → %s\n",
               requirements[i].desc,
               feedback_topology_name(rec));
    }

    /* Specific application examples */
    printf("\n--- Application Examples ---\n\n");

    printf("  GPS Receiver LNA (Low Noise Amplifier):\n");
    printf("    • Need: High input Z (antenna match), low output Z (mixer drive)\n");
    printf("    • Topology: Series-Shunt\n");
    printf("    • Design: A_f ≈ 20 dB with β = 0.1\n");
    printf("    • Benefit: Input Z increased for 50Ω match via feedback\n\n");

    printf("  Photodiode Transimpedance Amplifier (TIA):\n");
    printf("    • Need: Low input Z (virtual ground), low output Z (ADC drive)\n");
    printf("    • Topology: Shunt-Shunt\n");
    printf("    • Design: R_f = 100 kΩ, β = 10 μS\n");
    printf("    • Benefit: Current-to-voltage conversion with BW > 10 MHz\n");
    printf("    • Reference: Graeme, 'Photodiode Amplifiers' (1996)\n\n");

    printf("  Tesla Battery Management Current Sensor:\n");
    printf("    • Need: Current sensing with high output Z (ADC input)\n");
    printf("    • Topology: Shunt-Series\n");
    printf("    • Design: Current-sense amp with β = 0.01 (A/A)\n");
    printf("    • Benefit: Accurate current measurement +/-0.1%% over -40 to 125 C\n\n");

    printf("  OTA-C Filter for 5G NR Baseband:\n");
    printf("    • Need: Voltage-to-current conversion, high Z_in and Z_out\n");
    printf("    • Topology: Series-Series\n");
    printf("    • Design: G_m = 1 mS with source degeneration\n");
    printf("    • Benefit: Linearized transconductance for accurate filter poles\n");

    return 0;
}
