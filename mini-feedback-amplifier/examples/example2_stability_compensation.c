/**
 * @file example2_stability_compensation.c
 * @brief Example 2: Stability Analysis and Compensation Design
 *
 * This example demonstrates the complete stability analysis and
 * compensation design workflow for a two-stage op-amp that is
 * marginally stable without compensation.
 *
 * The example shows:
 *   1. Stability assessment of the uncompensated amplifier
 *   2. Dominant-pole compensation design
 *   3. Miller (pole-splitting) compensation design
 *   4. Lead compensation design
 *   5. Lag compensation design
 *   6. Comparison of all compensation strategies
 *
 * This mirrors the workflow used in professional IC design:
 * analyze → compensate → verify → iterate.
 *
 * The μA741 op-amp (introduced 1968) uses a 30 pF Miller capacitor
 * for internal compensation. This example recreates that design
 * problem with modern parameters.
 *
 * Reference: Sedra & Smith §11.7, Example 11.10
 */

#include "feedback_amplifier.h"
#include "stability_analysis.h"
#include "frequency_compensation.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static void print_margins(const char *label, const StabilityMargins *m)
{
    printf("  %s:\n", label);
    printf("    Phase Margin:     %.1f°\n", m->phase_margin_deg);
    printf("    Gain Margin:      %.1f dB\n", m->gain_margin_db);
    printf("    Unity-gain freq:  %.1f Hz\n", m->unity_gain_freq_hz);
    printf("    Phase crossover:  %.1f Hz\n", m->phase_crossover_hz);
    printf("    Classification:   %s\n",
           m->classification == STABILITY_STABLE ? "STABLE ✓" :
           m->classification == STABILITY_MARGINALLY_STABLE ?
           "MARGINAL ⚠" : "UNSTABLE ✗");
}

int main(void)
{
    printf("=== Example 2: Stability Analysis and Compensation Design ===\n\n");

    /* --- Define a challenging amplifier ---
     *
     * Three-stage amplifier with:
     *   - High DC gain (120 dB)
     *   - Pole 1 at 1 kHz (from high-impedance node)
     *   - Pole 2 at 2 MHz (from gain stage)
     *   - Pole 3 at 20 MHz (from output stage)
     *   - Unity-gain frequency ≈ 10 MHz
     *
     * With feedback factor β = 1 (unity-gain configuration):
     *   Loop gain at low freq = 10^6
     *   Unity-gain freq will be near 10 MHz
     *   Phase at 10 MHz: -90° - arctan(10/2) - arctan(10/20)
     *                   ≈ -90° - 78.7° - 26.6° = -195.3°
     *   PM ≈ 180° - 195.3° = -15.3°  → UNSTABLE!
     */

    FeedbackAmplifier amp;
    memset(&amp, 0, sizeof(amp));
    amp.open_loop_gain   = 1.0e6;       /* 120 dB */
    amp.feedback_factor  = 1.0;         /* β = 1 (worst-case unity-gain) */
    amp.polarity         = FB_NEGATIVE;
    amp.topology         = FB_SERIES_SHUNT;
    amp.input_impedance  = 1.0e6;
    amp.output_impedance = 100.0;
    amp.dominant_pole_hz = 1.0e3;       /* 1 kHz */
    amp.second_pole_hz   = 2.0e6;       /* 2 MHz */
    amp.third_pole_hz    = 20.0e6;      /* 20 MHz */
    amp.unity_gain_freq_hz = 10.0e6;    /* ~10 MHz */

    printf("--- Amplifier Under Analysis ---\n");
    printf("  Open-loop gain:  %.0f (%.0f dB)\n",
           amp.open_loop_gain, 20.0 * log10(amp.open_loop_gain));
    printf("  Feedback factor β: %.2f (unity-gain config)\n",
           amp.feedback_factor);
    printf("  Poles: f_p1 = %.1f kHz, f_p2 = %.1f MHz, f_p3 = %.1f MHz\n",
           amp.dominant_pole_hz / 1.0e3,
           amp.second_pole_hz / 1.0e6,
           amp.third_pole_hz / 1.0e6);

    /* Step 1: Assess uncompensated stability */
    printf("\n--- Step 1: Uncompensated Stability Assessment ---\n");
    StabilityMargins before;
    stability_assess_from_poles(&amp, &before);
    print_margins("BEFORE compensation", &before);

    if (before.classification == STABILITY_UNSTABLE) {
        printf("  → Amplifier is UNSTABLE! Compensation required.\n");
    }

    /* Step 2: Dominant-pole compensation */
    printf("\n--- Step 2: Dominant-Pole Compensation ---\n");
    CompensationResult dp_result = comp_design_dominant_pole(&amp, 60.0);
    print_margins("AFTER dominant-pole compensation", &dp_result.margins_after);
    printf("  Dominant pole placed at: %.2f Hz\n",
           dp_result.network.dominant.dominant_pole_hz);
    printf("  New unity-gain freq: %.1f kHz\n",
           dp_result.network.dominant.new_unity_gain_hz / 1.0e3);
    printf("  Bandwidth reduction: %.1f → %.1f Hz (factor %.0f×)\n",
           dp_result.margins_after.unity_gain_freq_hz,
           before.unity_gain_freq_hz,
           before.unity_gain_freq_hz / dp_result.margins_after.unity_gain_freq_hz);

    /* Step 3: Miller compensation */
    printf("\n--- Step 3: Miller (Pole-Splitting) Compensation ---\n");
    CompensationResult miller_result = comp_design_miller(
        &amp, 100.0,   /* Stage 2 gain = 40 dB */
        1.0e5,        /* R_in = 100 kΩ at compensation node */
        10.0e3,       /* R_out = 10 kΩ */
        60.0);        /* Target PM = 60° */

    print_margins("AFTER Miller compensation", &miller_result.margins_after);
    printf("  C_miller = %.2f pF\n",
           miller_result.network.miller.miller_cap_f * 1.0e12);
    printf("  Original poles: %.1f Hz, %.1f MHz\n",
           miller_result.network.miller.original_pole1_hz,
           miller_result.network.miller.original_pole2_hz / 1.0e6);
    printf("  New poles:      %.2f Hz, %.1f MHz (split!)\n",
           miller_result.network.miller.new_pole1_hz,
           miller_result.network.miller.new_pole2_hz / 1.0e6);
    printf("  Pole 1 moved DOWN by %.0f×, Pole 2 moved UP by %.0f×\n",
           miller_result.network.miller.original_pole1_hz /
               miller_result.network.miller.new_pole1_hz,
           miller_result.network.miller.new_pole2_hz /
               miller_result.network.miller.original_pole2_hz);

    /* Step 4: Lead compensation */
    printf("\n--- Step 4: Lead Compensation ---\n");
    CompensationResult lead_result = comp_design_lead(&amp, 60.0);
    printf("  Lead zero at:  %.1f Hz\n",
           lead_result.network.lead.zero_freq_hz);
    printf("  Lead pole at:  %.1f Hz\n",
           lead_result.network.lead.pole_freq_hz);
    printf("  Max phase lead: %.1f° at %.1f Hz\n",
           lead_result.network.lead.max_phase_lead_deg,
           lead_result.network.lead.max_phase_freq_hz);
    printf("  Network: R1=%.1f kΩ, R2=%.1f kΩ, C=%.1f pF\n",
           lead_result.network.lead.r1_ohm / 1000.0,
           lead_result.network.lead.r2_ohm / 1000.0,
           lead_result.network.lead.cap_f * 1.0e12);

    /* Step 5: Lag compensation */
    printf("\n--- Step 5: Lag Compensation ---\n");
    CompensationResult lag_result = comp_design_lag(&amp, 45.0);
    printf("  Lag pole at:   %.1f Hz\n",
           lag_result.network.lag.pole_freq_hz);
    printf("  Lag zero at:   %.1f Hz\n",
           lag_result.network.lag.zero_freq_hz);
    printf("  DC attenuation: %.1f dB\n",
           lag_result.network.lag.dc_attenuation_db);
    printf("  Network: R1=%.1f kΩ, R2=%.1f kΩ, C=%.3f μF\n",
           lag_result.network.lag.r1_ohm / 1000.0,
           lag_result.network.lag.r2_ohm / 1000.0,
           lag_result.network.lag.cap_f * 1.0e6);

    /* Step 6: Lead-Lag combined */
    printf("\n--- Step 6: Lead-Lag Compensation ---\n");
    CompensationResult ll_result = comp_design_lead_lag(&amp, 60.0);
    printf("  Combined PM improvement: %.1f°\n",
           ll_result.pm_improvement_deg);

    /* --- Comparison Table --- */
    printf("\n--- Compensation Strategy Comparison ---\n");
    printf("  %-20s %10s %10s %10s %10s\n",
           "Strategy", "PM (°)", "GM (dB)", "BW (Hz)", "PM Gain (°)");
    printf("  %-20s %10s %10s %10s %10s\n",
           "--------------------", "------", "------", "------", "------");
    printf("  %-20s %10.1f %10.1f %10.1f %10s\n",
           "Uncompensated", before.phase_margin_deg,
           before.gain_margin_db, before.unity_gain_freq_hz, "---");
    printf("  %-20s %10.1f %10.1f %10.1f %10.1f\n",
           "Dominant Pole", dp_result.margins_after.phase_margin_deg,
           dp_result.margins_after.gain_margin_db,
           dp_result.margins_after.unity_gain_freq_hz,
           dp_result.pm_improvement_deg);
    printf("  %-20s %10.1f %10.1f %10.1f %10.1f\n",
           "Miller", miller_result.margins_after.phase_margin_deg,
           miller_result.margins_after.gain_margin_db,
           miller_result.margins_after.unity_gain_freq_hz,
           miller_result.pm_improvement_deg);
    printf("  %-20s %10.1f %10.1f %10.1f %10.1f\n",
           "Lead", lead_result.margins_after.phase_margin_deg,
           lead_result.margins_after.gain_margin_db,
           lead_result.margins_after.unity_gain_freq_hz,
           lead_result.pm_improvement_deg);
    printf("  %-20s %10.1f %10.1f %10.1f %10.1f\n",
           "Lag", lag_result.margins_after.phase_margin_deg,
           lag_result.margins_after.gain_margin_db,
           lag_result.margins_after.unity_gain_freq_hz,
           lag_result.pm_improvement_deg);

    printf("\n--- Design Recommendation ---\n");
    printf("  For this 3-stage amplifier with high DC gain (120 dB):\n");
    printf("  → Miller compensation is preferred (standard for op-amps)\n");
    printf("  → Dominant-pole if bandwidth is NOT critical\n");
    printf("  → Nested Miller if amplifier has 3 distinct stages\n");
    printf("  → Lead-lag for complex Nyquist contours with conditional stability\n");

    return 0;
}
