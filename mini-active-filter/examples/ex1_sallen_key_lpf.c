/**
 * ex1_sallen_key_lpf.c — Sallen-Key Butterworth Lowpass Filter Design
 *
 * L6 Canonical Problem: Design a 2nd-order Butterworth LP filter
 * using the Sallen-Key topology. Compute component values, verify
 * the frequency response, and analyze sensitivity.
 *
 * This is the most fundamental active filter design problem.
 * The Sallen-Key Butterworth LP is the "hello world" of active filters.
 *
 * Reference: Sallen & Key (1955), Sedra & Smith (2020) §16.3
 */

#include "active_filter_defs.h"
#include "active_filter_sallen_key.h"
#include "active_filter_sensitivity.h"
#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int main(void) {
    printf("Sallen-Key Butterworth Lowpass Filter Design\n");
    printf("============================================\n\n");

    /* Specification: 2nd-order Butterworth LP, 1 kHz cutoff, 0 dB gain */
    double fc = 1000.0;       /* Cutoff frequency (Hz) */
    double q  = 0.70710678;   /* Butterworth Q = 1/√2 */
    double wp = 2.0 * M_PI * fc;
    double r_scale = 10000.0; /* 10kΩ impedance level */

    /* The classic design: K = 3 - 1/Q = 3 - √2 ≈ 1.586 */
    double k_butterworth = 3.0 - 1.0 / q;
    printf("Required amplifier gain: K = 3 - 1/Q = %.4f\n", k_butterworth);
    printf("Gain-setting ratio R4/R3 = K - 1 = %.4f\n", k_butterworth - 1.0);

    /* Design the filter */
    active_component_values_t comp;
    int ret = sallen_key_lp_design(wp, q, k_butterworth, r_scale, &comp);
    if (ret != ACTIVE_OK) {
        printf("ERROR: Design failed with code %d\n", ret);
        return 1;
    }

    printf("\n--- Component Values ---\n");
    printf("R1 = %.2f Ω\n", comp.r1);
    printf("R2 = %.2f Ω\n", comp.r2);
    printf("R3 = %.2f Ω (gain-setting, to ground)\n", comp.r3);
    printf("R4 = %.2f Ω (gain-setting, feedback)\n", comp.r4);
    printf("C1 = %.2f nF\n", comp.c1 * 1e9);
    printf("C2 = %.2f nF\n", comp.c2 * 1e9);

    /* Verify the design by extracting parameters */
    double wp_ext, qp_ext, gain_ext;
    sallen_key_extract_params(&comp, ACTIVE_FUNC_LOWPASS,
                               &wp_ext, &qp_ext, &gain_ext);
    printf("\n--- Extracted Parameters ---\n");
    printf("ω₀ = %.2f rad/s (f₀ = %.2f Hz)\n",
           wp_ext, wp_ext / (2.0 * M_PI));
    printf("Q  = %.4f (target: %.4f)\n", qp_ext, q);
    printf("K  = %.4f (target: %.4f)\n", gain_ext, k_butterworth);

    /* Frequency response at key points */
    printf("\n--- Frequency Response ---\n");
    double freqs[] = {10, 100, 500, 1000, 2000, 5000, 10000, 50000};
    for (int i = 0; i < 8; i++) {
        double omega = 2.0 * M_PI * freqs[i];
        double num[3], den[3], gain;
        sallen_key_transfer_function(&comp, ACTIVE_FUNC_LOWPASS,
                                      num, den, &gain);

        /* Evaluate H(jω) */
        double w2 = omega * omega;
        double denom_real = den[2] - w2;
        double denom_imag = den[1] * omega;
        double mag = gain * den[2] / sqrt(denom_real*denom_real +
                                           denom_imag*denom_imag);
        double mag_db = 20.0 * log10(mag > 1e-15 ? mag : 1e-15);
        double phase = atan2(-denom_imag, denom_real) * 180.0 / M_PI;

        printf("f = %6.0f Hz  |H| = %7.2f dB  φ = %7.2f°\n",
               freqs[i], mag_db, phase);
    }

    /* Sensitivity analysis */
    printf("\n--- Sensitivity Analysis ---\n");
    active_sensitivity_t sens;
    sensitivity_sallen_key(&comp, ACTIVE_FUNC_LOWPASS, &sens);
    printf("S_R1^ω₀ = %.3f  S_R1^Q = %.3f\n", sens.s_r1_f0, sens.s_r1_q);
    printf("S_R2^ω₀ = %.3f  S_R2^Q = %.3f\n", sens.s_r2_f0, sens.s_r2_q);
    printf("S_C1^ω₀ = %.3f  S_C1^Q = %.3f\n", sens.s_c1_f0, sens.s_c1_q);
    printf("S_C2^ω₀ = %.3f  S_C2^Q = %.3f\n", sens.s_c2_f0, sens.s_c2_q);
    printf("Worst-case sensitivity: |S|_max = %.3f\n", sens.worst_case);

    /* Verify sensitivity sum theorem */
    double sum_f0 = sens.s_r1_f0 + sens.s_r2_f0 + sens.s_c1_f0 + sens.s_c2_f0;
    printf("\nSensitivity sum: Σ S^ω₀ = %.3f (expected: -2)\n", sum_f0);

    printf("\n=== Design Complete ===\n");
    return 0;
}
