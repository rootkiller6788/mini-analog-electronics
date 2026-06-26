/**
 * example_voltage_multiplier.c -- Cockroft-Walton Voltage Multiplier
 *
 * Designs a 3-stage voltage multiplier to generate ~1000V from 120VAC.
 * Used in: cathode ray tube supplies, Geiger counters, particle accelerators.
 *
 * Knowledge: L6 Canonical Problem -- Voltage Multiplier Design
 * Reference: Cockroft & Walton (1932), Proc. Royal Society A
 */
#include <stdio.h>
#include <math.h>
#include "../include/diode_physics.h"
#include "../include/rectifier_topology.h"
#include "../include/filter_capacitor.h"

int main(void) {
    printf("=== Cockroft-Walton Voltage Multiplier Design ===\n");
    printf("Application: High-voltage DC from low-voltage AC\n\n");

    double V_in_rms = 120.0;
    double V_in_peak = V_in_rms * 1.41421356;
    double f_line = 60.0;
    double V_f = 0.7;

    printf("Input: %.0f Vrms (%.0f Vpeak) at %.0f Hz\n\n", V_in_rms, V_in_peak, f_line);

    printf("Stage-by-stage analysis:\n");
    printf("%-6s %-12s %-12s %-12s %-12s\n", "N", "Vnl(V)", "Vload(V)", "Ripple(V)", "Zout(ohm)");

    for (int n = 1; n <= 5; n++) {
        VoltageMultiplierConfig vm_cfg = {
            .n_stages = n,
            .V_in_peak = V_in_peak,
            .freq_Hz = f_line,
            .C_stage = 10e-6,
            .I_load = 0.001,
            .V_f = V_f
        };

        VoltageMultiplierResult vm = rectifier_voltage_multiplier_analyze(&vm_cfg);
        printf("%-6d %-12.0f %-12.1f %-12.2f %-12.0f\n",
               n, vm.V_out_no_load, vm.V_out_load,
               vm.V_ripple_pp, vm.output_impedance);
    }

    printf("\nDetailed 3-stage design:\n");
    VoltageMultiplierConfig design = {3, V_in_peak, f_line, 10e-6, 0.001, V_f};
    VoltageMultiplierResult result = rectifier_voltage_multiplier_analyze(&design);

    printf("  No-load output: %.0f V\n", result.V_out_no_load);
    printf("  Loaded output (1mA): %.1f V\n", result.V_out_load);
    printf("  Voltage drop under load: %.1f V\n", result.V_drop);
    printf("  Ripple: %.2f Vpp\n", result.V_ripple_pp);
    printf("  Output impedance: %.0f ohm\n", result.output_impedance);
    printf("  PIV per stage: %.0f V (use 1N4007, 1000V)\n", result.PIV_per_stage);

    printf("\nCapacitor sizing analysis (fixed 3 stages):\n");
    printf("%-12s %-12s %-12s\n", "C(uF)", "Ripple(V)", "Vdrop(V)");
    double caps[] = {1e-6, 4.7e-6, 10e-6, 22e-6, 47e-6};
    for (int i = 0; i < 5; i++) {
        design.C_stage = caps[i];
        result = rectifier_voltage_multiplier_analyze(&design);
        printf("%-12.1f %-12.2f %-12.1f\n",
               caps[i] * 1e6, result.V_ripple_pp, result.V_drop);
    }

    printf("\nNotes:\n");
    printf("  - Ripple and voltage drop scale with I_load\n");
    printf("  - Larger C reduces ripple but increases inrush\n");
    printf("  - Output impedance ~ N^3, limiting practical stages\n");
    printf("  - Applications: photomultiplier tubes, Nixie displays,\n");
    printf("    electrostatic speakers, insect zappers\n");
    printf("  - Modern alternative: flyback converter for >10W\n");

    printf("\n=== Design Complete ===\n");
    return 0;
}