/**
 * example_three_phase_rectifier.c -- Three-Phase Industrial Rectifier
 *
 * Analyzes a 480V three-phase bridge rectifier for industrial DC supply.
 * Demonstrates commutation overlap, thermal analysis, and efficiency.
 *
 * Knowledge: L7 Application -- Industrial Three-Phase Rectifier
 * Reference: Mohan, Undeland & Robbins Ch.5; IEEE 519-2014
 *
 * Applications: Tesla Supercharger, Toyota factory DC bus, Boeing 787 APU
 */
#include <stdio.h>
#include <math.h>
#include "../include/diode_physics.h"
#include "../include/rectifier_topology.h"
#include "../include/filter_capacitor.h"
#include "../include/power_rectifier.h"
#include "../include/snubber_protection.h"

int main(void) {
    printf("=== Three-Phase Industrial Rectifier Analysis ===\n");
    printf("Application: 480V/60Hz, 50kW DC bus for motor drive\n\n");

    ThreePhaseConfig cfg = {
        .V_ll_rms = 480.0,
        .freq_Hz = 60.0,
        .R_load = 6.0,
        .L_source = 0.0001,
        .V_f = 1.2,
        .connection = THREEPHASE_WYE,
        .use_half_wave = 0
    };

    ThreePhaseResult result = power_three_phase_analyze(&cfg);

    printf("Rectifier Output:\n");
    printf("  V_dc = %.1f V (ideal: ~648V without losses)\n", result.V_dc);
    printf("  I_dc = %.1f A\n", result.I_dc);
    printf("  P_dc = %.1f kW\n", result.P_dc / 1000.0);
    printf("  Ripple: %.2f Vpp (%.2f%%)\n", result.V_r_pp, result.ripple_factor * 100.0);
    printf("  Ripple frequency: %.0f Hz\n", result.ripple_freq_Hz);
    printf("  PIV per diode: %.0f V\n", result.PIV);

    printf("\nCommutation:\n");
    printf("  Overlap angle: %.1f degrees\n", result.commutation_angle_rad * 180.0 / 3.14159);
    printf("  Voltage drop from commutation: %.2f V\n", result.V_drop_commutation);

    printf("\nDiode Stresses:\n");
    printf("  I_avg per diode: %.1f A\n", result.I_diode_avg);
    printf("  I_rms per diode: %.1f A\n", result.I_diode_rms);
    printf("  I_peak repetitive: %.1f A\n", result.I_diode_peak);

    printf("\nThermal Analysis:\n");
    ThermalConfig therm = {
        .Ta = 323.0, .Tj_max = 423.0,
        .Rth_jc = 1.5, .Rth_cs = 0.5, .Rth_sa = 5.0,
        .P_dissipated = cfg.V_f * result.I_diode_avg * 2.0
    };
    ThermalResult tr = power_thermal_analyze(&therm);
    printf("  T_junction = %.0f K (%.0f C)\n", tr.Tj, tr.Tj - 273.15);
    printf("  Thermal margin = %.0f K\n", tr.margin_K);
    printf("  Heatsink required: %s\n", tr.needs_heatsink ? "YES" : "NO");
    printf("  Required Rth_sa: %.2f K/W\n", tr.Rth_sa_required);
    printf("  Relative MTBF factor: %.3f\n", tr.MTBF_factor);

    printf("\nEfficiency Comparison:\n");
    double eff_1ph, eff_3ph;
    power_efficiency_compare(result.V_dc, result.I_dc, &eff_1ph, &eff_3ph);
    printf("  Single-phase equivalent: %.1f%%\n", eff_1ph * 100.0);
    printf("  Three-phase (this design): %.1f%%\n", eff_3ph * 100.0);
    printf("  Advantage: three-phase is %.1f%% more efficient\n", (eff_3ph - eff_1ph) * 100.0);

    printf("\nHarmonic Analysis:\n");
    printf("  Fundamental ripple: %.0f Hz (6-pulse)\n", result.ripple_freq_Hz);
    printf("  IEEE 519 compliance: uses 6-pulse + 5th/7th harmonic filters\n");

    printf("\nInrush Considerations:\n");
    InrushAnalysis inr = power_inrush_analyze(480.0 * 1.4142, 0.01, 0.05, 0.0, 10.0);
    printf("  Peak inrush (without NTC): %.0f A\n", inr.I_peak_inrush);
    printf("  With NTC (10 ohm cold): %.0f A\n",
           480.0 * 1.4142 / (0.05 + 10.0));
    printf("  Pre-charge circuit: %s\n", inr.needs_precharge ? "REQUIRED" : "optional");

    printf("\n=== Analysis Complete ===\n");
    printf("Tesla Supercharger note: Modern designs use active front-end\n");
    printf("(PWM rectifier) instead of diode bridge for:\n");
    printf("  - Bidirectional power flow (V2G)\n");
    printf("  - Unity power factor\n");
    printf("  - Regulated DC bus voltage\n");
    return 0;
}