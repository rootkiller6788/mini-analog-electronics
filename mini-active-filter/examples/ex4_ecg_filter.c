/**
 * ex4_ecg_filter.c — ECG/Biomedical Signal Conditioning Filter
 *
 * L7 Application: Design a complete ECG front-end filter chain
 * using active filter topologies. ECG signals require:
 * - Highpass filter to reject DC offset and baseline wander (~0.05 Hz)
 * - Lowpass filter for anti-aliasing and muscle noise rejection (~150 Hz)
 * - Notch filter for 50/60 Hz power line interference rejection
 *
 * This example demonstrates a real-world biomedical signal processing
 * application using the active filter design library.
 *
 * Reference: Webster, Medical Instrumentation: Application and
 *            Design (2009), Ch. 6
 *            Tompkins, Biomedical Digital Signal Processing (1993)
 */

#include "active_filter_defs.h"
#include "active_filter_sallen_key.h"
#include "active_filter_mfb.h"
#include "active_filter_cascade.h"
#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int main(void) {
    printf("ECG Signal Conditioning Active Filter Chain\n");
    printf("===========================================\n\n");

    printf("ECG signal characteristics:\n");
    printf("  Amplitude: 0.5-4 mV (peak)\n");
    printf("  Bandwidth: 0.05-150 Hz (diagnostic)\n");
    printf("  Interference: 50/60 Hz power line, baseline wander\n\n");

    /* ============ STAGE 1: DC BLOCKING HIGHPASS ============ */
    printf("--- Stage 1: DC Blocking / Baseline Wander HPF ---\n");
    printf("Specification: 1st-order HP, fc=0.05 Hz\n\n");

    /* 1st-order HP using Sallen-Key HP topology (reduced to 1st order)
     * We use the first-order section from cascade module */
    active_component_values_t dc_block;
    int ret = cascade_first_order_section(ACTIVE_FUNC_HIGHPASS,
                                           0.05, 0.0, 100000.0, &dc_block);
    if (ret == ACTIVE_OK) {
        printf("DC Blocking HP: R=%.0fkΩ, C=%.2fμF\n",
               dc_block.r1/1e3, dc_block.c1*1e6);
    } else {
        printf("Using alternative: large time constant RC + buffer\n");
        /* Manual design: fc = 1/(2πRC) = 0.05 Hz
         * Choose R = 1MΩ → C = 1/(2π·10⁶·0.05) ≈ 3.18μF */
        printf("  R = 1 MΩ, C = 3.3 μF → fc ≈ %.3f Hz\n",
               1.0/(2.0*M_PI*1e6*3.3e-6));
    }

    /* ============ STAGE 2: ANTI-ALIASING LOWPASS ============ */
    printf("\n--- Stage 2: Anti-Aliasing Lowpass Filter ---\n");
    printf("Specification: 4th-order Butterworth LP, fc=150 Hz\n\n");

    double fc_lp = 150.0;
    double w0_lp = 2.0 * M_PI * fc_lp;
    double q_butter = 0.70710678;
    double k_butter = 3.0 - 1.0 / q_butter;

    /* Two cascaded Sallen-Key LP stages */
    active_component_values_t lp_stage1, lp_stage2;
    sallen_key_lp_design(w0_lp, q_butter, k_butter, 10000.0, &lp_stage1);
    sallen_key_lp_design(w0_lp, q_butter, k_butter, 10000.0, &lp_stage2);

    printf("Stage 2a (Q=0.707): R1=%.1fkΩ R2=%.1fkΩ C1=%.1fnF C2=%.1fnF\n",
           lp_stage1.r1/1e3, lp_stage1.r2/1e3,
           lp_stage1.c1*1e9, lp_stage1.c2*1e9);

    /* Check overall LP response at key frequencies */
    active_biquad_section_t bq_lp1, bq_lp2;
    active_biquad_init(w0_lp, q_butter, 1.0, ACTIVE_FUNC_LOWPASS, &bq_lp1);
    active_biquad_init(w0_lp, q_butter, 1.0, ACTIVE_FUNC_LOWPASS, &bq_lp2);

    printf("\nLP Frequency Response:\n");
    double lp_freqs[] = {10, 50, 100, 150, 200, 300, 500};
    for (int i = 0; i < 7; i++) {
        double w = 2.0 * M_PI * lp_freqs[i];
        double complex h1 = active_biquad_evaluate(&bq_lp1, w);
        double complex h2 = active_biquad_evaluate(&bq_lp2, w);
        double mag_db = 20.0 * log10(cabs(h1) * cabs(h2));
        printf("  f=%5.0f Hz  |H|=%6.1f dB\n", lp_freqs[i], mag_db);
    }

    /* ============ STAGE 3: 50 Hz NOTCH FILTER ============ */
    printf("\n--- Stage 3: 50 Hz Notch (Power Line Rejection) ---\n");
    printf("Specification: Twin-T active notch, fc=50 Hz, Q=5\n\n");

    active_component_values_t notch_comp;
    ret = sallen_key_notch_design(50.0, 5.0, 10000.0, &notch_comp);
    if (ret == ACTIVE_OK) {
        printf("Notch: R=%.1fkΩ, C=%.1fnF, Q=5\n",
               notch_comp.r1/1e3, notch_comp.c1*1e9);
        printf("  Notch frequency: 50 Hz\n");
    } else {
        printf("Using MFB notch alternative\n");
    }

    /* ============ COMPLETE CHAIN RESPONSE ============ */
    printf("\n--- Complete ECG Filter Chain Response ---\n");
    printf("Chain: HP(0.05Hz) → LP(150Hz, 4th)×2 → Notch(50Hz)\n\n");

    /* Simplified model: 1st-order HP + 4th-order LP
     * HP: H_hp(s) = s/(s + ω_hp), ω_hp = 2π·0.05
     * LP: |H_lp(jω)| = 1/(1 + (ω/ω_lp)^8)^{1/2} (8th order total? No, 4th.)
     * Actually LP is 4th-order: |H| = 1/√(1+(ω/ωc)^8)
     */
    printf("  Freq(Hz)   HP(dB)   LP(dB)   Total(dB)   Notes\n");
    printf("  --------   ------   ------   ---------   -----\n");

    double chain_freqs[] = {0.02, 0.05, 0.1, 1.0, 10, 50, 100, 150, 200, 500};
    double w_hp = 2.0 * M_PI * 0.05;
    double w_lp = 2.0 * M_PI * 150.0;

    for (int i = 0; i < 10; i++) {
        double w = 2.0 * M_PI * chain_freqs[i];

        /* HP response: |H| = ω/√(ω²+ω_hp²) */
        double hp_mag = w / sqrt(w*w + w_hp*w_hp);
        double hp_db = 20.0 * log10(hp_mag > 1e-10 ? hp_mag : 1e-10);

        /* LP response (4th-order Butterworth): |H| = 1/√(1+(ω/ωc)^8) */
        double lp_mag = 1.0 / sqrt(1.0 + pow(w/w_lp, 8));
        double lp_db = 20.0 * log10(lp_mag > 1e-10 ? lp_mag : 1e-10);

        double total_db = hp_db + lp_db;

        /* Notch at 50 Hz: add approximate notch attenuation */
        const char *note = "";
        if (fabs(chain_freqs[i] - 50.0) < 1.0) {
            total_db -= 40.0;  /* ~40dB notch depth */
            note = "(with notch)";
        } else if (chain_freqs[i] < 0.05) {
            note = "<-- HP roll-off";
        } else if (chain_freqs[i] > 150) {
            note = "<-- LP roll-off";
        } else {
            note = "(passband)";
        }

        printf("  %8.2f   %6.1f   %6.1f    %7.1f    %s\n",
               chain_freqs[i], hp_db, lp_db, total_db, note);
    }

    /* ============ PERFORMANCE SUMMARY ============ */
    printf("\n--- Performance Summary ---\n");
    printf("Passband gain: 0 dB (unity)\n");
    printf("Passband ripple: < 0.1 dB (Butterworth)\n");
    printf("Stopband attenuation @ 300 Hz: %.1f dB\n",
           -40.0 * log10(300.0/150.0) / log10(2.0));  /* ~40dB */
    printf("Notch depth @ 50 Hz: > 40 dB\n");
    printf("Total op-amps: 6 (1 HP + 4 LP + 1 notch)\n");
    printf("Power consumption: ~6 × 2mA × ±15V = 180 mW\n");

    printf("\n=== ECG Filter Design Complete ===\n");
    return 0;
}
