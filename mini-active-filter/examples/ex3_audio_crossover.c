/**
 * ex3_audio_crossover.c — Active Audio Crossover Network
 *
 * L7 Application: Design a 2-way active crossover for a
 * loudspeaker system using Sallen-Key filters.
 *
 * Crossover frequency: 2.5 kHz
 * - Lowpass channel: 4th-order Linkwitz-Riley (cascaded Butterworth)
 * - Highpass channel: 4th-order Linkwitz-Riley
 *
 * Linkwitz-Riley alignment: cascaded Butterworth sections produce
 * a -6 dB point at the crossover with both channels in-phase.
 * Total acoustic output sums flat (allpass characteristic).
 *
 * Reference: Linkwitz, "Active Crossover Networks for Noncoincident
 *            Drivers," JAES, vol. 24, no. 1, 1976.
 */

#include "active_filter_defs.h"
#include "active_filter_sallen_key.h"
#include "active_filter_cascade.h"
#include "active_filter_sensitivity.h"
#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int main(void) {
    printf("2-Way Active Crossover — Linkwitz-Riley 4th Order\n");
    printf("==================================================\n\n");

    double fx = 2500.0;     /* Crossover frequency */
    double w0 = 2.0 * M_PI * fx;
    double r_scale = 10000.0;

    /* Linkwitz-Riley = two cascaded Butterworth 2nd-order sections.
     * Each Butterworth section: Q = 0.7071, K = 1.586 for Sallen-Key.
     * Two in cascade: -6 dB at fx, in-phase output. */
    double q_butter = 0.70710678;
    double k_butter = 3.0 - 1.0 / q_butter;  /* ≈ 1.586 */

    printf("Crossover frequency: %.0f Hz\n", fx);
    printf("Alignment: Linkwitz-Riley (4th order)\n");
    printf("Each channel: Two cascaded 2nd-order Butterworth sections\n\n");

    /* ============= LOWPASS CHANNEL ============= */
    printf("--- LOWPASS Channel ---\n");

    active_component_values_t lp_stage1, lp_stage2;

    int ret1 = sallen_key_lp_design(w0, q_butter, k_butter,
                                     r_scale, &lp_stage1);
    int ret2 = sallen_key_lp_design(w0, q_butter, k_butter,
                                     r_scale, &lp_stage2);

    if (ret1 != ACTIVE_OK || ret2 != ACTIVE_OK) {
        printf("LP design failed\n");
        return 1;
    }

    printf("Stage 1: R1=%.1fk R2=%.1fk C1=%.1fnF C2=%.1fnF K=%.2f\n",
           lp_stage1.r1/1e3, lp_stage1.r2/1e3,
           lp_stage1.c1*1e9, lp_stage1.c2*1e9, k_butter);

    /* ============= HIGHPASS CHANNEL ============= */
    printf("\n--- HIGHPASS Channel ---\n");

    double c_scale = 1.0 / (w0 * r_scale);
    active_component_values_t hp_stage1, hp_stage2;

    ret1 = sallen_key_hp_design(w0, q_butter, k_butter,
                                 c_scale, &hp_stage1);
    ret2 = sallen_key_hp_design(w0, q_butter, k_butter,
                                 c_scale, &hp_stage2);

    if (ret1 != ACTIVE_OK || ret2 != ACTIVE_OK) {
        printf("HP design failed\n");
        return 1;
    }

    printf("Stage 1: R1=%.1fk R2=%.1fk C1=%.1fnF C2=%.1fnF K=%.2f\n",
           hp_stage1.r1/1e3, hp_stage1.r2/1e3,
           hp_stage1.c1*1e9, hp_stage1.c2*1e9, k_butter);

    /* ============= FREQUENCY RESPONSE AT KEY POINTS ============= */
    printf("\n--- Crossover Response ---\n");
    printf("  f(Hz)   LP(dB)   HP(dB)   Sum(dB)   Phase Diff(°)\n");
    printf("  ------  -------  -------  -------   -------------\n");

    /* Build biquad representations for response calculation */
    active_biquad_section_t bq_lp1, bq_lp2, bq_hp1, bq_hp2;
    active_biquad_init(w0, q_butter, 1.0, ACTIVE_FUNC_LOWPASS, &bq_lp1);
    active_biquad_init(w0, q_butter, 1.0, ACTIVE_FUNC_LOWPASS, &bq_lp2);
    active_biquad_init(w0, q_butter, 1.0, ACTIVE_FUNC_HIGHPASS, &bq_hp1);
    active_biquad_init(w0, q_butter, 1.0, ACTIVE_FUNC_HIGHPASS, &bq_hp2);

    double test_freqs[] = {100, 500, 1000, 2000, 2500, 3000, 4000, 10000};
    for (int i = 0; i < 8; i++) {
        double w = 2.0 * M_PI * test_freqs[i];

        /* LP cascade: 2 stages */
        double complex h_lp1 = active_biquad_evaluate(&bq_lp1, w);
        double complex h_lp2 = active_biquad_evaluate(&bq_lp2, w);
        double complex h_lp = h_lp1 * h_lp2;
        double lp_db = 20.0 * log10(cabs(h_lp));

        /* HP cascade: 2 stages */
        double complex h_hp1 = active_biquad_evaluate(&bq_hp1, w);
        double complex h_hp2 = active_biquad_evaluate(&bq_hp2, w);
        double complex h_hp = h_hp1 * h_hp2;
        double hp_db = 20.0 * log10(cabs(h_hp));

        /* Sum (acoustic sum — magnitudes add in power, not voltage) */
        double sum_linear = cabs(h_lp) + cabs(h_hp);
        double sum_db = 20.0 * log10(sum_linear);

        /* Phase difference */
        double phase_lp = atan2(cimag(h_lp), creal(h_lp));
        double phase_hp = atan2(cimag(h_hp), creal(h_hp));
        double phase_diff = (phase_lp - phase_hp) * 180.0 / M_PI;

        printf("  %6.0f   %6.1f   %6.1f   %6.1f    %7.1f\n",
               test_freqs[i], lp_db, hp_db, sum_db, phase_diff);
    }

    /* ============= CROSSOVER VERIFICATION ============= */
    printf("\n--- Crossover Verification ---\n");

    /* At crossover frequency, each channel should be -6 dB */
    double w_x = 2.0 * M_PI * fx;
    double complex h_lp_x = active_biquad_evaluate(&bq_lp1, w_x) *
                             active_biquad_evaluate(&bq_lp2, w_x);
    double complex h_hp_x = active_biquad_evaluate(&bq_hp1, w_x) *
                             active_biquad_evaluate(&bq_hp2, w_x);

    double lp_x_db = 20.0 * log10(cabs(h_lp_x));
    double hp_x_db = 20.0 * log10(cabs(h_hp_x));

    printf("At crossover (%.0f Hz):\n", fx);
    printf("  LP level: %.2f dB (target: -6.0 dB)\n", lp_x_db);
    printf("  HP level: %.2f dB (target: -6.0 dB)\n", hp_x_db);
    printf("  Sum:      %.2f dB (target: ~0 dB)\n",
           20.0 * log10(cabs(h_lp_x) + cabs(h_hp_x)));

    /* Phase check: both channels have same phase at crossover (by design) */
    double phase_lp_x = atan2(cimag(h_lp_x), creal(h_lp_x));
    double phase_hp_x = atan2(cimag(h_hp_x), creal(h_hp_x));
    printf("  Phase diff: %.1f° (target: 0° for in-phase)\n",
           (phase_lp_x - phase_hp_x) * 180.0 / M_PI);

    printf("\n=== Crossover Design Complete ===\n");
    return 0;
}
