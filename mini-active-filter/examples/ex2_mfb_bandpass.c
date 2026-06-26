/**
 * ex2_mfb_bandpass.c — MFB Bandpass Filter Design
 *
 * L6 Canonical Problem: Design a high-Q bandpass filter using the
 * MFB topology. This is the primary MFB use case — where it excels
 * over Sallen-Key.
 *
 * Application: 1 kHz tone detector (Q=10) for DTMF or test equipment.
 * The MFB BP achieves Q=10 with a single op-amp, where Sallen-Key
 * would struggle due to gain-Q coupling near oscillation.
 */

#include "active_filter_defs.h"
#include "active_filter_mfb.h"
#include "active_filter_sensitivity.h"
#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int main(void) {
    printf("MFB Bandpass Filter — 1 kHz Tone Detector (Q=10)\n");
    printf("=================================================\n\n");

    double f0 = 1000.0;    /* Center frequency */
    double q  = 10.0;      /* Quality factor */
    double gain_db = 6.0;  /* +6 dB gain at center */
    double c_scale = 10e-9; /* 10nF capacitors */

    /* Check realizability: need 2Q² > K for MFB BP */
    double k = pow(10.0, gain_db / 20.0);  /* ~2.0 */
    printf("Gain K = %.2f  |  2Q² = %.1f  →  %s\n",
           k, 2.0 * q * q,
           (2.0 * q * q > k) ? "REALIZABLE" : "NOT REALIZABLE (reduce gain)");

    if (2.0 * q * q <= k) {
        printf("Adjusting gain to %.1f dB for realizability...\n",
               20.0 * log10(2.0 * q * q * 0.9));
        gain_db = 20.0 * log10(2.0 * q * q * 0.9);
    }

    /* Design */
    active_component_values_t comp;
    int ret = mfb_bp_design(f0, q, gain_db, c_scale, &comp);
    if (ret != ACTIVE_OK) {
        printf("ERROR: Design failed with code %d\n", ret);
        return 1;
    }

    printf("\n--- Component Values ---\n");
    printf("R1 = %.2f Ω (input resistor)\n", comp.r1);
    printf("R2 = %.2f Ω (ground resistor)\n", comp.r2);
    printf("R3 = %.2f Ω (feedback resistor)\n", comp.r3);
    printf("C1 = %.2f nF\n", comp.c1 * 1e9);
    printf("C2 = %.2f nF\n", comp.c2 * 1e9);

    /* Verify design by computing transfer function */
    double num[3], den[3], gain_tf;
    mfb_transfer_function(&comp, ACTIVE_FUNC_BANDPASS,
                           num, den, &gain_tf);

    double w0 = sqrt(den[2]);
    double q_ext = w0 / den[1];
    printf("\n--- Designed Parameters ---\n");
    printf("f0 = %.2f Hz (target: %.2f Hz)\n", w0 / (2.0 * M_PI), f0);
    printf("Q  = %.2f (target: %.2f)\n", q_ext, q);
    printf("K  = %.2f (%.1f dB)\n", gain_tf, 20.0*log10(gain_tf));

    /* Frequency response around center */
    printf("\n--- Frequency Response ---\n");
    printf("  f(Hz)    |H|(dB)   Phase(°)\n");
    printf("  ------   -------   --------\n");

    double freqs[] = {800, 900, 950, 990, 1000, 1010, 1050, 1100, 1200};
    for (int i = 0; i < 9; i++) {
        double w = 2.0 * M_PI * freqs[i];
        double w2 = w * w;

        /* Evaluate |H(jω)| = |num[1]*jω| / |den[0]*(jω)² + den[1]*jω + den[2]| */
        double num_mag = fabs(num[1]) * w;
        double denom_real = den[2] - w2;
        double denom_imag = den[1] * w;
        double denom_mag = sqrt(denom_real*denom_real + denom_imag*denom_imag);

        double mag = num_mag / denom_mag;
        double mag_db = 20.0 * log10(mag > 1e-15 ? mag : 1e-15);
        double phase = (atan2(num[1]*w, 0.0) -
                        atan2(denom_imag, denom_real)) * 180.0 / M_PI;

        printf("  %6.0f    %7.2f    %7.2f\n", freqs[i], mag_db, phase);
    }

    /* -3dB bandwidth */
    printf("\n--- Bandwidth ---\n");
    double bw_hz = f0 / q;
    printf("Theoretical BW (-3dB) = %.2f Hz\n", bw_hz);
    printf("Q = f0/BW = %.1f\n", f0 / bw_hz);

    /* Sensitivity */
    printf("\n--- Sensitivity ---\n");
    active_sensitivity_t sens;
    sensitivity_mfb(&comp, ACTIVE_FUNC_BANDPASS, &sens);
    printf("S_R1^Q = %.3f  S_R2^Q = %.3f  S_R3^Q = %.3f\n",
           sens.s_r1_q, sens.s_r2_q, sens.s_r3_q);
    printf("S_C1^Q = %.3f  S_C2^Q = %.3f\n",
           sens.s_c1_q, sens.s_c2_q);
    printf("Worst-case |S| = %.3f\n", sens.worst_case);

    /* Estimate max Q for this frequency */
    double q_max = mfb_bp_max_q(f0, k, 10e6, 1.0);
    printf("\nMax achievable Q (estimated): %.1f\n", q_max);

    printf("\n=== Design Complete ===\n");
    return 0;
}
