/**
 * bench_active_filter.c — Performance Benchmarks
 *
 * Benchmarks the core active filter operations for speed
 * and numerical stability assessment.
 */

#include "active_filter_defs.h"
#include "active_filter_sallen_key.h"
#include "active_filter_mfb.h"
#include "active_filter_cascade.h"
#include <stdio.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double seconds_since(clock_t start) {
    return (double)(clock() - start) / CLOCKS_PER_SEC;
}

int main(void) {
    printf("Active Filter Performance Benchmarks\n");
    printf("====================================\n\n");

    /* Setup: create a filter */
    active_biquad_section_t bq;
    active_biquad_init(2.0*M_PI*1000.0, 0.7071, 1.586,
                        ACTIVE_FUNC_LOWPASS, &bq);

    /* ========== BENCH 1: Biquad Evaluation ========== */
    printf("Bench 1: Biquad evaluation (1M iterations)\n");
    clock_t t0 = clock();
    double sum = 0.0;
    for (int i = 0; i < 1000000; i++) {
        double w = 2.0 * M_PI * (10.0 + i * 0.1);
        sum += cabs(active_biquad_evaluate(&bq, w));
    }
    double dt = seconds_since(t0);
    printf("  Time: %.3f s  (%.0f ns/eval)  [checksum: %g]\n",
           dt, dt * 1e9 / 1000000.0, sum);

    /* ========== BENCH 2: Filter Design ========== */
    printf("\nBench 2: Sallen-Key LP design (100k iterations)\n");
    t0 = clock();
    for (int i = 0; i < 100000; i++) {
        active_component_values_t comp;
        sallen_key_lp_design(2.0*M_PI*1000.0, 0.7071, 1.586,
                              10000.0, &comp);
        sum += comp.r1;
    }
    dt = seconds_since(t0);
    printf("  Time: %.3f s  (%.1f μs/design)  [checksum: %g]\n",
           dt, dt * 1e6 / 100000.0, sum);

    /* ========== BENCH 3: Cascade Evaluation ========== */
    printf("\nBench 3: Cascade response sweep (10k sweeps × 100 pts)\n");
    active_biquad_section_t bqs[4];
    for (int i = 0; i < 4; i++) {
        active_biquad_init(2.0*M_PI*1000.0*(i+1)*0.5, 0.7071*(i+1)*0.7,
                           1.0, ACTIVE_FUNC_LOWPASS, &bqs[i]);
    }
    t0 = clock();
    sum = 0.0;
    for (int sweep = 0; sweep < 10000; sweep++) {
        for (int pt = 0; pt < 100; pt++) {
            double w = 2.0 * M_PI * (10.0 * pow(10.0, pt * 4.0 / 99.0));
            sum += active_cascade_magnitude_db(bqs, 4, w);
        }
    }
    dt = seconds_since(t0);
    printf("  Time: %.3f s  (%.1f ns/cascade-eval)  [checksum: %g]\n",
           dt, dt * 1e9 / (10000.0*100.0), sum);

    /* ========== BENCH 4: Monte Carlo ========== */
    printf("\nBench 4: Monte Carlo tolerance (100 trials)\n");
    active_component_values_t comp;
    sallen_key_lp_design(2.0*M_PI*1000.0, 0.7071, 1.586, 10000.0, &comp);
    t0 = clock();
    double wp_m, wp_s, q_m, q_s, yld;
    for (int run = 0; run < 100; run++) {
        monte_carlo_tolerance(&comp, ACTIVE_TOPO_SALLEN_KEY,
                              ACTIVE_FUNC_LOWPASS,
                              5.0, 100, &wp_m, &wp_s, &q_m, &q_s, &yld);
    }
    dt = seconds_since(t0);
    printf("  Time: %.3f s  (%.1f ms/MC-run)\n", dt, dt * 1e3 / 10.0);

    printf("\n=== Benchmarks Complete ===\n");
    return 0;
}
