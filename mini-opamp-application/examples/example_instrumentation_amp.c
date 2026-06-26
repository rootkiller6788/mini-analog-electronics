/**
 * example_instrumentation_amp.c - Instrumentation Amplifier Design Example
 *
 * Demonstrates the complete design flow for a three-op-amp
 * instrumentation amplifier, including:
 *   1. Specification definition (gain, CMRR, bandwidth)
 *   2. Component value computation
 *   3. CMRR estimation with resistor tolerance
 *   4. Performance verification
 */

#include "opamp_config.h"
#include "opamp_basic.h"
#include <stdio.h>
#include <math.h>

int main(void) {
    printf("============================================================\n");
    printf("  Instrumentation Amplifier Design Example\n");
    printf("  Reference: Analog Devices AD620 / INA128 topology\n");
    printf("============================================================\n\n");

    /* Step 1: Initialize op-amp model (OPA2277 precision op-amp) */
    OpAmpParams opamp = opamp_params_init_default();
    opamp.A_ol = 1.0e6;        /* 120 dB open-loop gain */
    opamp.CMRR = 120.0;        /* 120 dB CMRR */
    opamp.GBW = 8.0e6;         /* 8 MHz GBW */
    opamp.V_os = 20.0e-6;      /* 20 uV offset */

    printf("Op-Amp: OPA2277-like precision op-amp\n");
    printf("  A_ol = %.0f dB\n", 20.0 * log10(opamp.A_ol));
    printf("  GBW = %.1f MHz\n", opamp.GBW * 1e-6);
    printf("  CMRR = %.0f dB\n\n", opamp.CMRR);

    /* Step 2: Define IA specification */
    InstrumentationAmpConfig ia;
    ia.target_gain = 100.0;
    ia.target_CMRR = 100.0;
    ia.target_bw = 10000.0;   /* 10 kHz bandwidth */

    printf("Target Specifications:\n");
    printf("  Gain: %.0f V/V\n", ia.target_gain);
    printf("  CMRR: >= %.0f dB\n", ia.target_CMRR);
    printf("  Bandwidth: >= %.0f kHz\n\n", ia.target_bw * 1e-3);

    /* Step 3: Design the IA */
    double actual_gain, actual_CMRR;
    int success = opamp_ia_design(&ia, &opamp, &actual_gain, &actual_CMRR);

    if (!success) {
        printf("ERROR: IA design failed!\n");
        return 1;
    }

    printf("Design Results:\n");
    printf("  Stage 1 gain G1 = 1 + 2*R_f1/R_gain = %.2f\n",
           1.0 + 2.0 * ia.R_f1 / ia.R_gain);
    printf("    R_f1 = %.1f kOhm\n", ia.R_f1 * 1e-3);
    printf("    R_gain = %.1f Ohm\n", ia.R_gain);
    printf("  Stage 2 gain G2 = R_f2/R_i2 = %.2f\n",
           ia.R_f2 / ia.R_i2);
    printf("    R_f2 = %.1f kOhm\n", ia.R_f2 * 1e-3);
    printf("    R_i2 = %.1f kOhm\n", ia.R_i2 * 1e-3);
    printf("  Total gain: %.2f V/V (%.2f dB)\n\n",
           actual_gain, 20.0 * log10(actual_gain));

    /* Step 4: CMRR analysis with different resistor tolerances */
    printf("CMRR vs Resistor Tolerance:\n");
    printf("  Tolerance     CMRR (dB)\n");
    printf("  ---------     ---------\n");

    double tolerances[] = {0.0001, 0.001, 0.005, 0.01, 0.05};
    const char *tol_labels[] = {"0.01%", "0.1%", "0.5%", "1%", "5%"};

    for (int i = 0; i < 5; i++) {
        double cmrr = opamp_ia_cmrr_estimate(&ia, &opamp, tolerances[i]);
        printf("  %-12s  %.1f\n", tol_labels[i], cmrr);
    }

    /* Step 5: Bandwidth analysis */
    printf("\nBandwidth Analysis:\n");
    double G1 = 1.0 + 2.0 * ia.R_f1 / ia.R_gain;
    double G2 = ia.R_f2 / ia.R_i2;
    (void)G1; (void)G2; /* G1, G2 used in bandwidth analysis below */

    /* Stage 1 BW */
    AmplifierConfig stage1;
    stage1.type = OPAMP_CFG_NON_INVERTING;
    stage1.R_f = ia.R_f1;
    stage1.R_g = ia.R_gain;

    /* Simplified: stage 1 is a non-inverting amplifier */
    /* Each of the two op-amps in stage 1 sees a noise gain */
    /* Stage 1 uses two non-inverting amps, each with gain G1_individual */
    /* Actually stage 1 is differential: each op-amp gain = 1 + R_f1/R_gain */
    /* But the overall stage 1 differential gain G1 = 1 + 2*R_f1/R_gain */

    double ng1 = 1.0 + ia.R_f1 / ia.R_gain;
    double bw1 = opamp.GBW / ng1;

    /* Stage 2 BW */
    stage1.type = OPAMP_CFG_DIFFERENTIAL;
    stage1.R_f = ia.R_f2;
    stage1.R_i = ia.R_i2;
    double ng2 = opamp_cfg_noise_gain(&stage1);
    double bw2 = opamp.GBW / ng2;

    printf("  Stage 1 BW: %.1f kHz\n", bw1 * 1e-3);
    printf("  Stage 2 BW: %.1f kHz\n", bw2 * 1e-3);
    printf("  Overall BW: ~%.1f kHz (limited by slowest stage)\n",
           (bw1 < bw2 ? bw1 : bw2) * 1e-3);

    /* Step 6: Check if target bandwidth is met */
    double overall_bw = (bw1 < bw2) ? bw1 : bw2;
    printf("\nBandwidth check: %.1f kHz %s %.1f kHz target\n",
           overall_bw * 1e-3,
           (overall_bw >= ia.target_bw) ? ">=" : "<",
           ia.target_bw * 1e-3);

    if (overall_bw >= ia.target_bw) {
        printf("PASS: Bandwidth requirement met.\n");
    } else {
        printf("FAIL: Bandwidth insufficient. Increase op-amp GBW or reduce gain.\n");
    }

    printf("\n============================================================\n");
    printf("  Instrumentation Amplifier Design Complete\n");
    printf("============================================================\n");

    return 0;
}
