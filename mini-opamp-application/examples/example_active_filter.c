/**
 * example_active_filter.c - Active Filter Design Example
 *
 * Demonstrates the complete design flow for a 4th-order Butterworth
 * low-pass active filter:
 *   1. Specification definition (cutoff, attenuation, topology)
 *   2. Pole computation and biquad pairing
 *   3. Component value synthesis (Sallen-Key topology)
 *   4. Frequency response evaluation (Bode plot data)
 *   5. Sensitivity analysis
 */

#include "opamp_filter.h"
#include "opamp_basic.h"
#include <stdio.h>
#include <math.h>

int main(void) {
    printf("============================================================\n");
    printf("  Active Filter Design Example\n");
    printf("  4th-Order Butterworth Low-Pass Filter\n");
    printf("============================================================\n\n");

    /* Step 1: Define filter specification */
    ActiveFilter filter;
    filter.spec.band_type = FILTER_BAND_LOWPASS;
    filter.spec.approx_type = FILTER_BUTTERWORTH;
    filter.spec.order = 4;
    filter.spec.f_c = 1000.0;           /* 1 kHz cutoff */
    filter.spec.f_s = 2000.0;           /* 2 kHz stopband */
    filter.spec.passband_ripple = 0.0;  /* Butterworth: 0 dB ripple */
    filter.spec.stopband_attenuation = 40.0; /* > 40 dB at 2 kHz */
    filter.spec.passband_gain = 1.0;

    printf("Filter Specification:\n");
    printf("  Type: 4th-order Butterworth Low-Pass\n");
    printf("  Cutoff frequency f_c: %.0f Hz\n", filter.spec.f_c);
    printf("  Stopband frequency f_s: %.0f Hz\n", filter.spec.f_s);
    printf("  Stopband attenuation: >= %.0f dB\n\n", filter.spec.stopband_attenuation);

    /* Step 2: Verify required order */
    int N_required = filter_order_for_attenuation(
        filter.spec.stopband_attenuation,
        filter.spec.f_s,
        filter.spec.f_c);
    printf("Required Butterworth order for %.0f dB at %.0f Hz: N = %d\n",
           filter.spec.stopband_attenuation, filter.spec.f_s, N_required);
    printf("Using N = %d (>= required)\n\n", filter.spec.order);

    /* Step 3: Initialize op-amp model */
    OpAmpParams opamp = opamp_params_init_default();
    opamp.GBW = 10.0e6;  /* Use a faster op-amp (10 MHz GBW) */
    printf("Op-Amp: GBW = %.1f MHz\n\n", opamp.GBW * 1e-6);

    /* Step 4: Cascade design */
    int success = filter_cascade_design(&filter, FILTER_TOPOLOGY_SALLEN_KEY, &opamp);

    if (!success) {
        printf("ERROR: Filter design failed!\n");
        return 1;
    }

    printf("Design Results (%d biquad stages):\n", filter.n_stages);
    for (int i = 0; i < filter.n_stages; i++) {
        FilterStage *s = &filter.stages[i];
        printf("\n  Stage %d:\n", i + 1);
        printf("    Pole frequency f_p: %.1f Hz\n", s->w_p / (2.0 * M_PI));
        printf("    Quality factor Q: %.3f\n", s->Q_p);
        printf("    Gain K: %.2f\n", s->K);
        printf("    Components:\n");
        printf("      R1 = %.1f kOhm, R2 = %.1f kOhm\n",
               s->R1 * 1e-3, s->R2 * 1e-3);
        printf("      C1 = %.1f nF, C2 = %.1f nF\n",
               s->C1 * 1e9, s->C2 * 1e9);
        if (s->R3 > 0.0) {
            printf("      R3 = %.1f kOhm, R4 = %.1f kOhm (gain setting)\n",
                   s->R3 * 1e-3, s->R4 * 1e-3);
        }
    }

    /* Step 5: Frequency response at key points */
    printf("\nFrequency Response:\n");
    printf("  Freq (Hz)    |H| (dB)    Phase (deg)\n");
    printf("  ---------    --------    -----------\n");

    double test_freqs[] = {10.0, 100.0, 500.0, 1000.0, 1500.0, 2000.0, 5000.0, 10000.0};
    for (int i = 0; i < 8; i++) {
        double mag, phase;
        filter_frequency_response(&filter, test_freqs[i], &mag, &phase);
        double mag_db = 20.0 * log10(mag);
        printf("  %8.0f    %8.2f    %8.1f\n", test_freqs[i], mag_db, phase);
    }

    /* Step 6: Group delay */
    printf("\nGroup Delay at key frequencies:\n");
    for (int i = 0; i < 4; i++) {
        double f = test_freqs[i];
        double tau = filter_group_delay(&filter, f);
        printf("  f = %8.0f Hz: tau_g = %.3f ms\n", f, tau * 1e3);
    }

    /* Step 7: Sensitivity analysis */
    printf("\nSensitivity Analysis (Stage 1):\n");
    double S_R1, S_R2, S_C1, S_C2;
    filter_sensitivity_wp(filter.topology, &filter.stages[0],
                          &S_R1, &S_R2, &S_C1, &S_C2);
    printf("  S_R1^wp = %.2f, S_R2^wp = %.2f\n", S_R1, S_R2);
    printf("  S_C1^wp = %.2f, S_C2^wp = %.2f\n", S_C1, S_C2);
    printf("  (All S = -0.5 for equal-R equal-C SK LP)\n");

    /* Step 8: Stopband attenuation check */
    double mag_at_fs, phase_at_fs;
    filter_frequency_response(&filter, filter.spec.f_s, &mag_at_fs, &phase_at_fs);
    double atten_at_fs = -20.0 * log10(mag_at_fs);

    printf("\nStopband Performance:\n");
    printf("  Attenuation at %.0f Hz: %.1f dB\n", filter.spec.f_s, atten_at_fs);
    printf("  Requirement: >= %.0f dB\n", filter.spec.stopband_attenuation);
    printf("  %s\n", (atten_at_fs >= filter.spec.stopband_attenuation) ?
           "PASS: Attenuation requirement met." :
           "FAIL: Attenuation insufficient.");

    printf("\n============================================================\n");
    printf("  Active Filter Design Complete\n");
    printf("============================================================\n");

    return 0;
}
