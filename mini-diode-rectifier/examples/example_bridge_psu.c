/**
 * example_bridge_psu.c -- Complete Bridge Rectifier DC Power Supply Design
 *
 * Designs a 12V DC power supply from 120V/60Hz mains using a bridge rectifier
 * with capacitor filter. Demonstrates end-to-end design workflow.
 *
 * Knowledge: L6 Canonical Problem -- Bridge PSU Design
 * Reference: Sedra & Smith Example 4.8
 */
#include <stdio.h>
#include <math.h>
#include "../include/diode_physics.h"
#include "../include/rectifier_topology.h"
#include "../include/filter_capacitor.h"

int main(void) {
    printf("=== Bridge Rectifier DC Power Supply Design ===\n");
    printf("Spec: 120Vrms/60Hz input, 12VDC/1A output, <5%% ripple\n\n");

    double V_in_rms = 120.0;
    double V_in_peak = V_in_rms * 1.41421356;
    double V_f = 0.7;
    double I_load = 1.0;
    double R_load = 12.0;
    double f_line = 60.0;
    double V_ripple_target = 0.05 * 12.0;

    printf("Step 1: Transformer selection\n");
    double V_secondary_rms = 12.0 / (2.0 * 1.4142 / 3.14159) + 2.0 * V_f;
    printf("  Required secondary: %.1f Vrms (use 12.6V CT transformer)\n", V_secondary_rms);

    printf("\nStep 2: Rectifier analysis\n");
    RectifierConfig cfg = {RECTIFIER_BRIDGE, 12.6, 12.6*1.4142, f_line, R_load, V_f, 1.0};
    BridgeResult br = rectifier_bridge_analyze(&cfg);
    printf("  V_dc (no filter): %.2f V\n", br.V_dc);
    printf("  PIV per diode: %.1f V (use 1N4004, 400V rating)\n", br.PIV);
    printf("  Ripple frequency: %.0f Hz\n", br.ripple_freq_Hz);
    printf("  Efficiency: %.1f%%\n", br.efficiency * 100.0);

    printf("\nStep 3: Filter capacitor selection\n");
    double C_min = filter_minimum_capacitance(I_load, br.ripple_freq_Hz, V_ripple_target);
    printf("  C_min for %.2fV ripple: %.0f uF\n", V_ripple_target, C_min * 1e6);
    double C_chosen = C_min * 2.5;
    printf("  Chosen C (2.5x margin): %.0f uF (use 22000 uF / 25V)\n", C_chosen * 1e6);

    CapacitorFilterParams cfp = {C_chosen, R_load, br.V_dc, br.ripple_freq_Hz, 0.05, V_f};
    RippleAnalysis ra = filter_capacitor_ripple_analyze(&cfp);
    printf("\n  Final ripple analysis:\n");
    printf("  V_dc_avg: %.2f V\n", ra.V_dc_avg);
    printf("  V_ripple_pp: %.3f V (%.1f%%)\n", ra.V_r_pp, ra.ripple_factor * 100.0);
    printf("  I_peak (repetitive): %.1f A\n", ra.I_peak);
    printf("  Conduction angle: %.1f deg\n", ra.conduction_angle_rad * 180.0 / 3.14159);

    printf("\nStep 4: Component selection summary\n");
    printf("  Transformer: 120V -> 12.6V CT, 2A rating\n");
    printf("  Diodes: 4x 1N4004 (400V, 1A)\n");
    printf("  Capacitor: 22000 uF / 25V electrolytic\n");
    printf("  Fuse: 1.5A slow-blow on primary\n");

    printf("\nStep 5: Loss and efficiency\n");
    double P_out = ra.V_dc_avg * I_load;
    double P_diode_loss = 2.0 * V_f * I_load;
    double P_total = P_out + P_diode_loss;
    printf("  P_out: %.2f W\n", P_out);
    printf("  P_diode_loss: %.2f W\n", P_diode_loss);
    printf("  System efficiency: %.1f%%\n", P_out / P_total * 100.0);

    printf("\n=== Design Complete ===\n");
    return 0;
}