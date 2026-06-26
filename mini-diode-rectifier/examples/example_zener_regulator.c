/**
 * example_zener_regulator.c -- Zener Shunt Regulator Design
 *
 * Designs a 5V Zener shunt regulator for a 9-12V unregulated input.
 * Complete design with line/load regulation analysis.
 *
 * Knowledge: L6 Canonical Problem -- Zener Voltage Regulator
 * Reference: Sedra & Smith Example 4.7
 * Application: Toyota ECU 5V supply, iPhone charger pre-regulator
 */
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "../include/diode_physics.h"
#include "../include/zener_regulator.h"

int main(void) {
    printf("=== Zener Shunt Regulator Design ===\n");
    printf("Spec: Vin=9-12V, Vout=5V, Iload=0-50mA\n\n");

    ZenerParams zener = {
        .V_Z_nom = 5.1,
        .I_ZT = 0.020,
        .Z_Z = 7.0,
        .I_ZK = 0.001,
        .I_ZM = 0.100,
        .P_max = 1.0,
        .T_C = 0.001,
        .T_J_max = 423.0,
        .RTH_JA = 100.0
    };

    ZenerRegulatorDesign design = {
        .V_in_min = 9.0,
        .V_in_max = 12.0,
        .V_out_target = 5.0,
        .I_load_min = 0.0,
        .I_load_max = 0.050,
        .V_ripple_in_pp = 1.0
    };

    ZenerRegulatorResult result;
    zener_regulator_design(&zener, &design, &result);

    printf("Design Results:\n");
    printf("  Feasible: %s\n", result.is_feasible ? "YES" : "NO");
    printf("  R_S = %.1f ohm (use %.1f ohm standard)\n", result.R_S, result.R_S);
    printf("  P_RS_max = %.3f W (use 0.5W resistor)\n", result.P_RS_max);
    printf("\nWorst-case analysis:\n");
    printf("  I_Z_min = %.1f mA (at Vin=9V, Iload=50mA)\n", result.I_Z_min * 1000.0);
    printf("  I_Z_max = %.1f mA (at Vin=12V, Iload=0mA)\n", result.I_Z_max * 1000.0);
    printf("  P_Z_max = %.0f mW\n", result.P_Z_max * 1000.0);
    printf("  Margin above I_ZK: %.1f mA\n", result.margin_I_ZK * 1000.0);

    printf("\nRegulation Performance:\n");
    printf("  V_out range: %.3f to %.3f V\n", result.V_out_min, result.V_out_max);
    printf("  Line regulation: %.3f mV/V\n", result.line_regulation * 1000.0);
    printf("  Load regulation: %.1f mV/mA\n", result.load_regulation * 1000.0);
    printf("  Efficiency: %.1f%% to %.1f%%\n",
           result.efficiency_min * 100.0, result.efficiency_max * 100.0);

    printf("\nLine Regulation Sweep (Vin=9-15V, Iload=25mA):\n");
    LineRegulationSweep lr = {0};
    zener_line_regulation_sweep(9.0, 15.0, 7, result.R_S, 0.025, &zener, &lr);
    printf("  Vin(V)  Vout(V)\n");
    for (int i = 0; i < lr.n_points; i++) {
        printf("  %5.1f   %6.3f\n", lr.V_in_array[i], lr.V_out_array[i]);
    }
    printf("  Line reg slope: %.3f mV/V\n", lr.line_reg_slope * 1000.0);
    free(lr.V_in_array);
    free(lr.V_out_array);

    printf("\nLoad Regulation Sweep (Vin=10V, Iload=0-50mA):\n");
    LoadRegulationSweep ld = {0};
    zener_load_regulation_sweep(0.0, 0.050, 6, 10.0, result.R_S, &zener, &ld);
    printf("  Iload(mA)  Vout(V)\n");
    for (int i = 0; i < ld.n_points; i++) {
        printf("  %7.1f  %6.3f\n", ld.I_load_array[i] * 1000.0, ld.V_out_array[i]);
    }
    printf("  Load reg slope: %.1f mV/mA\n", -ld.load_reg_slope * 1000.0);
    free(ld.I_load_array);
    free(ld.V_out_array);

    printf("\nThermal Check:\n");
    int power_ok = zener_check_power(&zener, result.P_Z_max, 313.0);
    printf("  T_ambient=40C: %s\n", power_ok ? "PASS" : "FAIL (needs heatsink)");
    printf("  T_J_max = %.0f C\n", result.T_J_max_op - 273.15);

    printf("\n=== Design Complete ===\n");
    printf("BOM: 1x 5.1V/1W Zener, 1x %.0f ohm/0.5W resistor\n", result.R_S);
    return 0;
}