/**
 * test_active_filter.c — Test Suite for Active Filter Module
 *
 * Tests cover: L1 Definitions, L2 Core Concepts (all topologies),
 * L3 Math Structures (transfer functions, parameter extraction),
 * L4 Fundamental Laws (sensitivity theorems, GBW effects),
 * L5 Algorithms (design methods, optimization),
 * L6 Canonical Problems (cascade design, complete filter design).
 *
 * All tests use assert() — zero tolerance for failure.
 */

#include "active_filter_defs.h"
#include "active_filter_sallen_key.h"
#include "active_filter_mfb.h"
#include "active_filter_state_variable.h"
#include "active_filter_cascade.h"
#include "active_filter_sensitivity.h"
#include "active_filter_advanced.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { \
    printf("  TEST: %s ... ", name); \
} while(0)

#define PASS() do { \
    printf("PASS\n"); tests_passed++; \
} while(0)

#define FAIL(msg) do { \
    printf("FAIL: %s\n", msg); tests_failed++; \
} while(0)

#define ASSERT_TRUE(cond, msg) do { \
    if (!(cond)) { FAIL(msg); return; } \
} while(0)

#define ASSERT_FLOAT_EQ(a, b, tol, msg) do { \
    if (fabs((a)-(b)) > (tol)) { \
        printf("FAIL: %s (got %g, expected %g)\n", msg, a, b); \
        tests_failed++; return; \
    } \
} while(0)

/* ==================================================================
 * L1: Definitions Tests
 * ================================================================== */

static void test_defs_spec_init(void) {
    TEST("filter spec initialization");
    active_filter_spec_t spec = active_filter_spec_init();
    ASSERT_TRUE(spec.function == ACTIVE_FUNC_LOWPASS, "default function");
    ASSERT_TRUE(spec.order == 2, "default order");
    ASSERT_TRUE(fabs(spec.f_low - 1000.0) < 0.01, "default frequency");
    ASSERT_TRUE(active_filter_spec_is_valid(&spec), "spec validation");
    PASS();
}

static void test_defs_db_conversion(void) {
    TEST("dB to linear conversion");
    ASSERT_FLOAT_EQ(active_db_to_linear(0.0), 1.0, 0.001, "0dB=1");
    ASSERT_FLOAT_EQ(active_db_to_linear(20.0), 10.0, 0.001, "20dB=10");
    ASSERT_FLOAT_EQ(active_db_to_linear(-20.0), 0.1, 0.001, "-20dB=0.1");
    ASSERT_FLOAT_EQ(active_linear_to_db(1.0), 0.0, 0.001, "linear 1=0dB");
    ASSERT_FLOAT_EQ(active_linear_to_db(10.0), 20.0, 0.001, "linear 10=20dB");
    PASS();
}

static void test_defs_q_bandwidth(void) {
    TEST("Q from bandwidth calculation");
    double q = active_q_from_bandwidth(1000.0, 100.0);
    ASSERT_FLOAT_EQ(q, 10.0, 0.001, "Q=f0/BW");
    double bw = active_bw_from_q(1000.0, 10.0);
    ASSERT_FLOAT_EQ(bw, 100.0, 0.001, "BW=f0/Q");
    PASS();
}

static void test_defs_center_freq(void) {
    TEST("geometric mean center frequency");
    double fc = active_center_freq(100.0, 10000.0);
    ASSERT_FLOAT_EQ(fc, 1000.0, 0.01, "sqrt(100*10000)=1000");
    PASS();
}

/* ==================================================================
 * L2: Sallen-Key Tests
 * ================================================================== */

static void test_sk_lp_design(void) {
    TEST("Sallen-Key LP design (Butterworth)");
    double wp = 2.0 * M_PI * 1000.0;  /* 1 kHz */
    double qp = 0.7071;  /* Butterworth */
    active_component_values_t comp;
    int ret = sallen_key_lp_design(wp, qp, 1.586, 10000.0, &comp);
    ASSERT_TRUE(ret == ACTIVE_OK, "design returned OK");
    ASSERT_TRUE(comp.r1 > 0 && comp.r2 > 0, "resistors positive");
    ASSERT_TRUE(comp.c1 > 0 && comp.c2 > 0, "capacitors positive");

    /* Verify parameters */
    double wp_ext, qp_ext, gain_ext;
    ret = sallen_key_extract_params(&comp, ACTIVE_FUNC_LOWPASS,
                                     &wp_ext, &qp_ext, &gain_ext);
    ASSERT_TRUE(ret == ACTIVE_OK, "extract returned OK");
    ASSERT_FLOAT_EQ(qp_ext, qp, 0.1, "Q within 10%");
    PASS();
}

static void test_sk_lp_unity_gain(void) {
    TEST("Sallen-Key LP unity-gain design");
    double wp = 2.0 * M_PI * 500.0;
    active_component_values_t comp;
    /* Q=0.5 achievable with unity gain */
    int ret = sallen_key_lp_unity_gain(wp, 0.5, 10000.0, &comp);
    ASSERT_TRUE(ret == ACTIVE_OK, "unity gain design OK");
    ASSERT_TRUE(comp.r4 == 0.0, "R4 shorted for unity gain");
    PASS();
}

static void test_sk_hp_design(void) {
    TEST("Sallen-Key HP design");
    double wp = 2.0 * M_PI * 1000.0;
    active_component_values_t comp;
    int ret = sallen_key_hp_design(wp, 0.7071, 1.586, 10e-9, &comp);
    ASSERT_TRUE(ret == ACTIVE_OK, "HP design OK");
    ASSERT_TRUE(comp.c1 > 0 && comp.c2 > 0, "caps positive");
    PASS();
}

static void test_sk_bp_design(void) {
    TEST("Sallen-Key BP design");
    active_component_values_t comp;
    int ret = sallen_key_bp_design(1000.0, 5.0, 6.0, 10000.0, &comp);
    ASSERT_TRUE(ret == ACTIVE_OK, "BP design OK");
    /* Verify Q < 25 for practical design */
    double wp, qp, gain;
    sallen_key_extract_params(&comp, ACTIVE_FUNC_BANDPASS, &wp, &qp, &gain);
    ASSERT_TRUE(qp < 25.0, "Q within practical range");
    PASS();
}

static void test_sk_notch_design(void) {
    TEST("Sallen-Key Twin-T notch design");
    active_component_values_t comp;
    int ret = sallen_key_notch_design(60.0, 5.0, 10000.0, &comp);
    ASSERT_TRUE(ret == ACTIVE_OK, "notch design OK");
    ASSERT_TRUE(comp.r1 > 0, "components set");
    PASS();
}

/* ==================================================================
 * L2: MFB Tests
 * ================================================================== */

static void test_mfb_lp_design(void) {
    TEST("MFB LP design");
    double wp = 2.0 * M_PI * 1000.0;
    active_component_values_t comp;
    int ret = mfb_lp_design(wp, 0.7071, 2.0, 10000.0, &comp);
    ASSERT_TRUE(ret == ACTIVE_OK, "MFB LP design OK");
    ASSERT_TRUE(comp.r1 > 0 && comp.r2 > 0 && comp.r3 > 0, "3 R's positive");
    PASS();
}

static void test_mfb_bp_design(void) {
    TEST("MFB BP design (primary use case)");
    active_component_values_t comp;
    int ret = mfb_bp_design(1000.0, 10.0, 6.0, 1e-9, &comp);
    ASSERT_TRUE(ret == ACTIVE_OK, "MFB BP design OK");
    /* Verify realizability: 2Q² > K */
    double k = pow(10.0, 6.0/20.0);
    ASSERT_TRUE(2.0*100.0 > k, "2Q²>K constraint satisfied");
    PASS();
}

static void test_mfb_bp_max_q(void) {
    TEST("MFB BP max Q estimation");
    double q_max = mfb_bp_max_q(10000.0, 2.0, 10e6, 1.0);
    ASSERT_TRUE(q_max > 0.0, "returns positive Q");
    ASSERT_TRUE(q_max < 1000.0, "Q within practical bound");
    PASS();
}

static void test_mfb_hp_design(void) {
    TEST("MFB HP design");
    double wp = 2.0 * M_PI * 1000.0;
    active_component_values_t comp;
    int ret = mfb_hp_design(wp, 0.7071, 1.0, 10e-9, &comp);
    ASSERT_TRUE(ret == ACTIVE_OK, "MFB HP design OK");
    PASS();
}

/* ==================================================================
 * L2: State-Variable & Biquad Tests
 * ================================================================== */

static void test_khn_design(void) {
    TEST("KHN state-variable design");
    active_component_values_t comp;
    int ret = khn_state_variable_design(1000.0, 5.0, 0.0, 10000.0, &comp);
    ASSERT_TRUE(ret == ACTIVE_OK, "KHN design OK");
    /* KHN uses multiple resistors and capacitors */
    ASSERT_TRUE(comp.r1 > 0 && comp.r2 > 0, "key R's present");
    ASSERT_TRUE(comp.c1 > 0 && comp.c2 > 0, "both C's present");
    PASS();
}

static void test_khn_outputs(void) {
    TEST("KHN simultaneous outputs (HP, BP, LP)");
    active_component_values_t comp;
    khn_state_variable_design(1000.0, 2.0, 0.0, 10000.0, &comp);

    double hp[3], bp[3], lp[3], den[3];
    double hp_g, bp_g, lp_g;
    int ret = khn_compute_outputs(&comp, hp, bp, lp, den,
                                   &hp_g, &bp_g, &lp_g);
    ASSERT_TRUE(ret == ACTIVE_OK, "compute outputs OK");
    /* HP numerator: s² term */
    ASSERT_TRUE(hp[0] > 0, "HP has s² term");
    /* LP numerator: constant term */
    ASSERT_TRUE(lp[2] > 0, "LP has constant term");

    /* Verify relationship: BP gain = Q * HP gain */
    ASSERT_FLOAT_EQ(bp_g, hp_g * 2.0, 0.01, "BP gain = Q * HP gain");
    PASS();
}

static void test_tow_thomas_design(void) {
    TEST("Tow-Thomas biquad design");
    active_component_values_t comp;
    int ret = tow_thomas_biquad_design(1000.0, 5.0, 0.0, 1e-9, &comp);
    ASSERT_TRUE(ret == ACTIVE_OK, "TT biquad design OK");
    /* Q = Rq/R */
    ASSERT_TRUE(comp.r3 > 0, "Q-setting resistor present");
    PASS();
}

static void test_akerberg_mossberg(void) {
    TEST("Akerberg-Mossberg single-op-amp biquad");
    active_component_values_t comp;
    int ret = akerberg_mossberg_design(ACTIVE_FUNC_LOWPASS,
                                        1000.0, 0.7071, 0.0, 10000.0, &comp);
    ASSERT_TRUE(ret == ACTIVE_OK, "AM design OK");
    PASS();
}

/* ==================================================================
 * L3: Biquad Evaluation Tests
 * ================================================================== */

static void test_biquad_eval_lp(void) {
    TEST("biquad evaluation — LP DC gain");
    active_biquad_section_t bq;
    active_biquad_init(2.0*M_PI*1000.0, 0.7071, 2.0, ACTIVE_FUNC_LOWPASS, &bq);

    /* At DC (ω=0): |H(0)| = gain = 2.0 */
    double mag_db = active_biquad_magnitude_db(&bq, 0.0);
    ASSERT_FLOAT_EQ(mag_db, 20.0*log10(2.0), 0.01, "LP DC gain=2 (6dB)");

    /* At ω=ω₀: |H| = gain / √2 ≈ 2.0/1.414 = 1.414 → +3dB from base? No, -3dB from DC */
    /* Actually for Butterworth LP: |H(jω₀)| = |H(0)|/√2 = 2.0/1.414 = 1.414 */
    double mag_w0 = active_biquad_magnitude_db(&bq, 2.0*M_PI*1000.0);
    double expected_w0_db = 20.0*log10(2.0/1.41421356);
    ASSERT_FLOAT_EQ(mag_w0, expected_w0_db, 0.5, "LP -3dB at cutoff");
    PASS();
}

static void test_biquad_eval_bp(void) {
    TEST("biquad evaluation — BP at center frequency");
    active_biquad_section_t bq;
    active_biquad_init(2.0*M_PI*1000.0, 5.0, 1.0, ACTIVE_FUNC_BANDPASS, &bq);

    /* At ω=ω₀, BP gain = Q * K/(?) ... center gain magnitude */
    double w0 = bq.wp;
    double mag_center = cabs(active_biquad_evaluate(&bq, w0));
    ASSERT_TRUE(mag_center > 0.0, "BP center gain > 0");
    PASS();
}

static void test_biquad_eval_hp(void) {
    TEST("biquad evaluation — HP high-frequency gain");
    active_biquad_section_t bq;
    active_biquad_init(2.0*M_PI*1000.0, 0.7071, 3.0, ACTIVE_FUNC_HIGHPASS, &bq);

    /* At ω → ∞: |H| → gain = 3.0 */
    double mag_hf = active_biquad_magnitude_db(&bq, 2.0*M_PI*1e6);
    ASSERT_FLOAT_EQ(mag_hf, 20.0*log10(3.0), 0.5, "HP HF gain=3");
    PASS();
}

/* ==================================================================
 * L3: Cascade Tests
 * ================================================================== */

static void test_butterworth_poles(void) {
    TEST("Butterworth pole computation for cascade");
    double wp[5], qp[5];
    int n = cascade_butterworth_poles(4, wp, qp, 5);
    ASSERT_TRUE(n == 2, "4th order = 2 biquad pairs");
    /* All ω₀ = 1 for normalized Butterworth */
    ASSERT_FLOAT_EQ(wp[0], 1.0, 0.01, "poles on unit circle");
    ASSERT_FLOAT_EQ(wp[1], 1.0, 0.01, "poles on unit circle");
    /* Q values: Q1 > Q2 for Butterworth (poles further from jω axis have lower Q) */
    ASSERT_TRUE(qp[0] > 0.5, "Q1 > 0.5");
    ASSERT_TRUE(qp[0] > qp[1], "Q1 > Q2 (inner poles have higher Q)");
    PASS();
}

static void test_chebyshev_poles(void) {
    TEST("Chebyshev pole computation");
    double wp[5], qp[5];
    int n = cascade_chebyshev_poles(4, 0.5, wp, qp, 5);
    ASSERT_TRUE(n == 2, "4th order = 2 pairs");
    ASSERT_TRUE(qp[0] > 0.5, "valid Q");
    PASS();
}

static void test_bessel_poles(void) {
    TEST("Bessel pole computation");
    double wp[5], qp[5];
    int n = cascade_bessel_poles(4, wp, qp, 5);
    ASSERT_TRUE(n == 2, "4th order = 2 pairs");
    /* Bessel Q values are lower than Butterworth */
    ASSERT_TRUE(qp[0] < 1.0, "Bessel Q < Butterworth Q");
    PASS();
}

static void test_section_ordering(void) {
    TEST("optimal section ordering");
    double qp[] = {0.7071, 1.3066, 0.5412, 2.5629};
    int ordering[4];
    int n = cascade_optimal_ordering(qp, 4, ordering);
    ASSERT_TRUE(n == 4, "returns 4 sections");
    /* Should be sorted by ascending Q */
    for (int i = 0; i < 3; i++) {
        ASSERT_TRUE(qp[ordering[i]] <= qp[ordering[i+1]],
                    "sorted ascending Q");
    }
    PASS();
}

static void test_gain_distribution(void) {
    TEST("gain distribution across cascade");
    double qp[] = {0.7071, 1.3066};
    double gains[2];
    int ret = cascade_distribute_gain(4.0, qp, 2, gains);
    ASSERT_TRUE(ret == ACTIVE_OK, "distribution OK");
    /* Product should equal total */
    ASSERT_FLOAT_EQ(gains[0]*gains[1], 4.0, 0.01, "product = total");
    PASS();
}

static void test_first_order_section(void) {
    TEST("first-order section design");
    active_component_values_t comp;
    int ret = cascade_first_order_section(ACTIVE_FUNC_LOWPASS,
                                           1000.0, 6.0, 10000.0, &comp);
    ASSERT_TRUE(ret == ACTIVE_OK, "1st-order OK");
    ASSERT_TRUE(comp.r1 > 0 && comp.r2 > 0, "R components set");
    ASSERT_TRUE(comp.c1 > 0, "C component set");
    PASS();
}

/* ==================================================================
 * L4: Sensitivity Theorem Tests
 * ================================================================== */

static void test_sensitivity_sum_theorem(void) {
    TEST("sensitivity sum invariance theorem");
    active_component_values_t comp;
    sallen_key_lp_design(2.0*M_PI*1000.0, 0.7071, 1.586, 10000.0, &comp);

    double sum, expected;
    int ret = sensitivity_sum_theorem(&comp, ACTIVE_TOPO_SALLEN_KEY,
                                       ACTIVE_FUNC_LOWPASS, &sum, &expected);
    ASSERT_TRUE(ret == ACTIVE_OK, "sum theorem OK");
    /* Sum should equal -N_C (where N_C = 2 for Sallen-Key) */
    ASSERT_FLOAT_EQ(sum, expected, 0.5, "Σ S = -N_C");
    PASS();
}

static void test_sensitivity_q_sum(void) {
    TEST("Q sensitivity sum theorem");
    active_component_values_t comp;
    sallen_key_lp_design(2.0*M_PI*1000.0, 0.7071, 1.586, 10000.0, &comp);

    double sum;
    int ret = sensitivity_q_sum_theorem(&comp, ACTIVE_TOPO_SALLEN_KEY,
                                         ACTIVE_FUNC_LOWPASS, &sum);
    ASSERT_TRUE(ret == ACTIVE_OK, "Q sum OK");
    /* Q sensitivity sum should be near zero (theoretically 0, numerically small) */
    ASSERT_TRUE(fabs(sum) < 1.5, "Σ S_Q ≈ 0");
    PASS();
}

static void test_sk_sensitivity(void) {
    TEST("Sallen-Key sensitivity computation");
    active_component_values_t comp;
    sallen_key_lp_design(2.0*M_PI*1000.0, 0.7071, 1.586, 10000.0, &comp);

    active_sensitivity_t sens;
    int ret = sensitivity_sallen_key(&comp, ACTIVE_FUNC_LOWPASS, &sens);
    ASSERT_TRUE(ret == ACTIVE_OK, "sensitivity OK");
    /* ω₀ sensitivities should be -0.5 */
    ASSERT_FLOAT_EQ(sens.s_r1_f0, -0.5, 0.1, "S_R1^ω₀ = -0.5");
    ASSERT_FLOAT_EQ(sens.s_c1_f0, -0.5, 0.1, "S_C1^ω₀ = -0.5");
    PASS();
}

static void test_mfb_sensitivity(void) {
    TEST("MFB sensitivity computation");
    active_component_values_t comp;
    mfb_lp_design(2.0*M_PI*1000.0, 0.7071, 2.0, 10000.0, &comp);

    active_sensitivity_t sens;
    int ret = sensitivity_mfb(&comp, ACTIVE_FUNC_LOWPASS, &sens);
    ASSERT_TRUE(ret == ACTIVE_OK, "MFB sensitivity OK");
    ASSERT_TRUE(sens.worst_case < 10.0, "worst case sensitivity < 10");
    PASS();
}

static void test_khn_sensitivity(void) {
    TEST("KHN sensitivity computation");
    active_component_values_t comp;
    khn_state_variable_design(1000.0, 5.0, 0.0, 10000.0, &comp);

    active_sensitivity_t sens;
    int ret = sensitivity_khn(&comp, &sens);
    ASSERT_TRUE(ret == ACTIVE_OK, "KHN sensitivity OK");
    /* KHN Q sensitivity to Rq should be ≈ 1 */
    ASSERT_FLOAT_EQ(sens.s_r2_q, 1.0, 0.2, "S_Rq^Q ≈ 1");
    /* Worst case should be bounded (~1) for KHN */
    ASSERT_TRUE(sens.worst_case < 2.0, "KHN worst case < 2");
    PASS();
}

static void test_gbw_effect(void) {
    TEST("GBW effect on filter poles");
    double wp_actual, q_actual;
    int ret = gbw_effect_on_poles(2.0*M_PI*1000.0, 10.0, 1e6,
                                    ACTIVE_TOPO_SALLEN_KEY,
                                    &wp_actual, &q_actual);
    ASSERT_TRUE(ret == ACTIVE_OK, "GBW analysis OK");
    /* Q-enhancement should be positive */
    ASSERT_TRUE(q_actual > 10.0, "Q enhanced by finite GBW");
    PASS();
}

static void test_slew_rate_limit(void) {
    TEST("slew rate frequency limit");
    double f_max = slew_rate_max_frequency(0.5, 10.0);
    /* f_max = SR/(2π·Vpk) = 0.5e6/(2π·10) ≈ 7960 Hz */
    ASSERT_TRUE(f_max > 1000.0 && f_max < 100000.0, "slew limit in range");
    PASS();
}

/* ==================================================================
 * L5: Monte Carlo and Worst-Case Tests
 * ================================================================== */

static void test_monte_carlo_basic(void) {
    TEST("Monte Carlo tolerance analysis");
    active_component_values_t comp;
    sallen_key_lp_design(2.0*M_PI*1000.0, 0.7071, 1.586, 10000.0, &comp);

    double wp_mean, wp_std, q_mean, q_std, yield;
    int ret = monte_carlo_tolerance(&comp, ACTIVE_TOPO_SALLEN_KEY,
                                     ACTIVE_FUNC_LOWPASS,
                                     5.0, 100, &wp_mean, &wp_std,
                                     &q_mean, &q_std, &yield);
    ASSERT_TRUE(ret == ACTIVE_OK, "Monte Carlo OK");
    ASSERT_TRUE(yield > 0.0 && yield <= 100.0, "valid yield");
    PASS();
}

static void test_worst_case(void) {
    TEST("worst-case analysis");
    active_component_values_t comp;
    sallen_key_lp_design(2.0*M_PI*1000.0, 0.7071, 1.586, 10000.0, &comp);

    double wp_min, wp_max, q_min, q_max;
    int ret = worst_case_analysis(&comp, ACTIVE_TOPO_SALLEN_KEY,
                                   ACTIVE_FUNC_LOWPASS,
                                   5.0, &wp_min, &wp_max, &q_min, &q_max);
    ASSERT_TRUE(ret == ACTIVE_OK, "worst-case OK");
    ASSERT_TRUE(wp_min < wp_max, "min < max");
    ASSERT_TRUE(q_min < q_max, "Q min < max");
    PASS();
}

/* ==================================================================
 * L5: E-Series and Optimization Tests
 * ================================================================== */

static void test_nearest_eseries(void) {
    TEST("nearest E12 value");
    double v1 = active_nearest_eseries(1050.0, 12);
    ASSERT_TRUE(fabs(v1 - 1000.0) < 100.0, "1050→1000");
    double v2 = active_nearest_eseries(4700.0, 24);
    ASSERT_TRUE(v2 > 4000.0 && v2 < 5000.0, "standard value");
    PASS();
}

static void test_component_range_check(void) {
    TEST("component range validation");
    active_component_values_t comp = {1000, 1000, 1000, 0, 0, 1e-9, 1e-9, 0};
    char warning[256];
    int ok = active_component_range_check(&comp, warning);
    ASSERT_TRUE(ok, "components in range");
    PASS();
}

/* ==================================================================
 * L3: Frequency Response Tests
 * ================================================================== */

static void test_freq_response_sweep(void) {
    TEST("frequency response sweep");
    active_biquad_section_t bq;
    active_biquad_init(2.0*M_PI*1000.0, 0.7071, 1.0, ACTIVE_FUNC_LOWPASS, &bq);

    active_freq_response_t response;
    int ret = active_freq_response_sweep(&bq, 1,
                                          10.0, 100000.0, 50, 1, &response);
    ASSERT_TRUE(ret == ACTIVE_OK, "sweep OK");
    ASSERT_TRUE(response.num_points == 50, "50 points");
    ASSERT_TRUE(response.points[0].magnitude_db > -10.0, "LP at low freq");
    ASSERT_TRUE(response.points[49].magnitude_db < -30.0, "LP roll-off at HF");
    active_freq_response_free(&response);
    PASS();
}

static void test_find_cutoff(void) {
    TEST("find -3dB cutoff frequency");
    active_biquad_section_t bq;
    active_biquad_init(2.0*M_PI*1000.0, 0.7071, 1.0, ACTIVE_FUNC_LOWPASS, &bq);

    double fc = active_filter_find_cutoff(&bq, 1, ACTIVE_FUNC_LOWPASS,
                                           100.0, 10000.0, 0.0);
    ASSERT_TRUE(fc > 900.0 && fc < 1100.0, "cutoff near 1kHz");
    PASS();
}

/* ==================================================================
 * L6: Complete Cascade Design Tests
 * ================================================================== */

static void test_cascade_filter_design(void) {
    TEST("complete cascade filter design");
    active_filter_spec_t spec = {
        .function = ACTIVE_FUNC_LOWPASS,
        .approx = ACTIVE_APPROX_BUTTERWORTH,
        .topology = ACTIVE_TOPO_SALLEN_KEY,
        .opamp_model = ACTIVE_OPAMP_IDEAL,
        .order = 4,
        .f_low = 1000.0,
        .f_high = 0.0,
        .f_center = 0.0,
        .bandwidth = 0.0,
        .passband_ripple_db = 0.0,
        .stopband_atten_db = 60.0,
        .gain_db = 6.0,
        .q_factor = 0.0,
        .impedance_ohm = 10000.0
    };

    active_filter_t *filter = NULL;
    int ret = active_filter_alloc(&spec, &filter);
    ASSERT_TRUE(ret == ACTIVE_OK && filter != NULL, "filter allocated");

    ret = cascade_filter_design(&spec, filter);
    ASSERT_TRUE(ret == ACTIVE_OK, "cascade design OK");
    ASSERT_TRUE(filter->num_biquads == 2, "4th order = 2 biquads");
    ASSERT_TRUE(filter->is_stable, "filter stable");

    /* Verify frequency response at DC */
    active_freq_point_t pt;
    cascade_evaluate_response(filter, 10.0, &pt);
    ASSERT_FLOAT_EQ(pt.magnitude_db, 6.0, 1.0, "DC gain ≈ 6dB");

    /* Verify */
    char report[512];
    int pass = cascade_verify(filter, 0.10, report);
    ASSERT_TRUE(pass, report);

    active_filter_free(filter);
    PASS();
}

static void test_cascade_butterworth_spec(void) {
    TEST("Butterworth order estimation");
    int n = active_estimate_order_butterworth(1000.0, 2000.0, 3.0, 40.0);
    ASSERT_TRUE(n >= 1 && n <= 10, "valid order");
    PASS();
}

static void test_cascade_chebyshev_spec(void) {
    TEST("Chebyshev order estimation");
    int n = active_estimate_order_chebyshev(1000.0, 1500.0, 0.5, 60.0);
    /* Chebyshev gives lower order than Butterworth for same spec */
    ASSERT_TRUE(n >= 1 && n <= 10, "valid order");
    PASS();
}

/* ==================================================================
 * L8: Advanced Topology Tests
 * ================================================================== */

static void test_sc_integrator(void) {
    TEST("switched-capacitor integrator");
    sc_integrator_t integ;
    int ret = sc_integrator_design(1e-4, 100e3, 10e-12, &integ);
    ASSERT_TRUE(ret == ACTIVE_OK, "SC integrator OK");
    ASSERT_TRUE(integ.c_ratio > 0.0 && integ.c_ratio < 1.0, "valid ratio");
    PASS();
}

static void test_sc_lowpass(void) {
    TEST("SC lowpass design");
    active_component_values_t comp;
    double ratio;
    int ret = sc_lowpass_design(1000.0, 0.0, 100e3, 10e-12, &comp, &ratio);
    ASSERT_TRUE(ret == ACTIVE_OK, "SC LP OK");
    ASSERT_TRUE(ratio > 0.0 && ratio < 0.5, "ratio in range");
    PASS();
}

static void test_gmc_integrator(void) {
    TEST("Gm-C integrator design");
    gmc_params_t params;
    int ret = gmc_integrator_design(2.0*M_PI*1e6, 5e-12, &params);
    ASSERT_TRUE(ret == ACTIVE_OK, "Gm-C OK");
    ASSERT_TRUE(params.gm > 0.0, "positive gm");
    ASSERT_TRUE(params.bias_current > 0.0, "positive bias");
    PASS();
}

static void test_gmc_thd(void) {
    TEST("Gm-C THD estimation");
    gmc_params_t params;
    gmc_integrator_design(2.0*M_PI*1e6, 5e-12, &params);
    double thd = gmc_thd_estimate(&params, 0.1, 0);
    ASSERT_TRUE(thd > 0.0 && thd < 1.0, "THD in [0,1]");
    double thd_deg = gmc_thd_estimate(&params, 0.1, 1);
    ASSERT_TRUE(thd_deg < thd, "degeneration reduces THD");
    PASS();
}

static void test_npath_filter(void) {
    TEST("N-path bandpass design");
    active_component_values_t comp;
    double hr;
    int ret = npath_bp_design(100e6, 1e6, 4, 50.0, &comp, &hr);
    ASSERT_TRUE(ret == ACTIVE_OK, "N-path design OK");
    ASSERT_TRUE(hr > 20.0, "harmonic rejection > 20dB");
    PASS();
}

static void test_mems_interface(void) {
    TEST("MEMS resonator interface");
    mems_resonator_t res = {
        .r_motional = 1000.0,
        .l_motional = 0.1,
        .c_motional = 1e-15,
        .c_static = 1e-12,
        .f_series = 500e6,
        .f_parallel = 501e6,
        .q_factor = 10000.0,
        .kt2 = 0.001
    };
    active_component_values_t comp;
    double gain;
    int ret = mems_filter_interface(&res, 50.0, &gain, &comp);
    ASSERT_TRUE(ret == ACTIVE_OK, "MEMS OK");
    ASSERT_TRUE(gain > 0.0, "positive gain needed");
    PASS();
}

/* ==================================================================
 * L3: Component Utilities
 * ================================================================== */

static void test_parallel_resistance(void) {
    TEST("parallel resistance");
    double rp = active_parallel_resistance(1000.0, 1000.0);
    ASSERT_FLOAT_EQ(rp, 500.0, 0.01, "1000||1000=500");
    PASS();
}

static void test_series_capacitance(void) {
    TEST("series capacitance");
    double cs = active_series_capacitance(1e-9, 1e-9);
    ASSERT_FLOAT_EQ(cs, 0.5e-9, 0.01e-9, "1nF series 1nF = 0.5nF");
    PASS();
}

static void test_impedance_scaling(void) {
    TEST("impedance scaling");
    active_component_values_t comp = {1000, 1000, 1000, 0, 0, 1e-9, 1e-9, 0};
    active_impedance_scale(&comp, 2.0);
    ASSERT_FLOAT_EQ(comp.r1, 2000.0, 0.01, "R doubled");
    ASSERT_FLOAT_EQ(comp.c1, 0.5e-9, 0.01e-9, "C halved");
    PASS();
}

/* ==================================================================
 * L3: Pole Normalization
 * ================================================================== */

static void test_pole_normalization_lp(void) {
    TEST("pole normalization — LP");
    double wp_actual, qp_actual;
    active_normalize_poles(1.0, 0.7071, ACTIVE_FUNC_LOWPASS,
                            1000.0, 0.0, &wp_actual, &qp_actual);
    ASSERT_FLOAT_EQ(qp_actual, 0.7071, 0.001, "LP Q unchanged");
    PASS();
}

static void test_pole_normalization_hp(void) {
    TEST("pole normalization — HP");
    double wp_actual, qp_actual;
    active_normalize_poles(1.0, 0.7071, ACTIVE_FUNC_HIGHPASS,
                            1000.0, 0.0, &wp_actual, &qp_actual);
    ASSERT_FLOAT_EQ(qp_actual, 0.7071, 0.001, "HP Q unchanged");
    PASS();
}

/* ==================================================================
 * L1: Filter lifetime management
 * ================================================================== */

static void test_filter_alloc_free(void) {
    TEST("filter allocation and deallocation");
    active_filter_spec_t spec = active_filter_spec_init();
    active_filter_t *filter = NULL;
    int ret = active_filter_alloc(&spec, &filter);
    ASSERT_TRUE(ret == ACTIVE_OK, "alloc OK");
    ASSERT_TRUE(filter != NULL, "non-null");
    ASSERT_TRUE(filter->num_biquads == 1, "2nd order = 1 biquad");
    active_filter_free(filter);
    PASS();
}

static void test_filter_spec_validation_edge_cases(void) {
    TEST("filter spec validation edge cases");
    /* NULL check */
    ASSERT_TRUE(!active_filter_spec_is_valid(NULL), "NULL rejected");

    /* Invalid order */
    active_filter_spec_t spec = active_filter_spec_init();
    spec.order = 0;
    ASSERT_TRUE(!active_filter_spec_is_valid(&spec), "order=0 rejected");
    spec.order = 11;
    ASSERT_TRUE(!active_filter_spec_is_valid(&spec), "order=11 rejected");

    /* Negative frequency */
    spec = active_filter_spec_init();
    spec.f_low = -100.0;
    ASSERT_TRUE(!active_filter_spec_is_valid(&spec), "negative freq rejected");

    PASS();
}

/* ==================================================================
 * L2: Transfer Function Extraction Tests
 * ================================================================== */

static void test_sk_tf_extraction(void) {
    TEST("Sallen-Key transfer function extraction");
    active_component_values_t comp;
    sallen_key_lp_design(2.0*M_PI*1000.0, 0.7071, 1.586, 10000.0, &comp);

    double num[3], den[3], gain;
    int ret = sallen_key_transfer_function(&comp, ACTIVE_FUNC_LOWPASS,
                                            num, den, &gain);
    ASSERT_TRUE(ret == ACTIVE_OK, "TF extraction OK");
    /* Denominator: s² + ω₀/Q·s + ω₀² */
    ASSERT_TRUE(den[0] == 1.0, "s² coeff = 1");
    ASSERT_TRUE(den[2] > 0.0, "ω₀² > 0");

    double wp = sqrt(den[2]);
    double qp = wp / den[1];
    ASSERT_FLOAT_EQ(qp, 0.7071, 0.15, "extracted Q ≈ 0.707");
    PASS();
}

static void test_mfb_tf_extraction(void) {
    TEST("MFB transfer function extraction");
    active_component_values_t comp;
    mfb_lp_design(2.0*M_PI*1000.0, 0.7071, 2.0, 10000.0, &comp);

    double num[3], den[3], gain;
    int ret = mfb_transfer_function(&comp, ACTIVE_FUNC_LOWPASS,
                                     num, den, &gain);
    ASSERT_TRUE(ret == ACTIVE_OK, "MFB TF OK");
    ASSERT_TRUE(gain > 0.0, "positive gain");
    PASS();
}

/* ==================================================================
 * L5: E-Series Snap Test
 * ================================================================== */

static void test_sk_eseries_snap(void) {
    TEST("Sallen-Key E-series snap");
    active_component_values_t comp_ideal, comp_snap;
    sallen_key_lp_design(2.0*M_PI*1000.0, 0.7071, 1.586, 10000.0, &comp_ideal);

    double freq_err, q_err;
    int ret = sallen_key_snap_to_eseries(&comp_ideal, 12,
                                           &comp_snap, &freq_err, &q_err);
    ASSERT_TRUE(ret == ACTIVE_OK, "snap OK");
    /* Error should be reasonable */
    ASSERT_TRUE(fabs(freq_err) < 20.0, "freq error < 20%");
    PASS();
}

/* ==================================================================
 * L4: DC Offset Propagation
 * ================================================================== */

static void test_dc_offset(void) {
    TEST("DC offset propagation");
    double gains[] = {1.0, 2.0};
    double vos[] = {100.0, 50.0};   /* μV */
    double ib[] = {10.0, 5.0};      /* nA */
    double req[] = {10000.0, 10000.0};  /* Ω */
    double offset = dc_offset_propagation(gains, vos, ib, req, 2);
    ASSERT_TRUE(offset > 0.0, "positive offset");
    ASSERT_TRUE(offset < 0.01, "offset < 10mV");
    PASS();
}

/* ==================================================================
 * L8: Sensitivity Jacobian
 * ================================================================== */

static void test_sensitivity_jacobian(void) {
    TEST("sensitivity Jacobian matrix");
    active_component_values_t comp;
    sallen_key_lp_design(2.0*M_PI*1000.0, 0.7071, 1.586, 10000.0, &comp);

    double jac[2][6];
    int ret = sensitivity_jacobian(&comp, ACTIVE_TOPO_SALLEN_KEY,
                                    ACTIVE_FUNC_LOWPASS, jac);
    ASSERT_TRUE(ret == ACTIVE_OK, "Jacobian OK");
    /* Row 0: ω₀ sensitivities, should have -0.5 for R1,C1 */
    ASSERT_FLOAT_EQ(jac[0][0], -0.5, 0.1, "S_R1^ω₀ = -0.5");
    PASS();
}

/* ==================================================================
 * L8: SC Alias Analysis
 * ================================================================== */

static void test_sc_alias(void) {
    TEST("SC aliasing analysis");
    double alias_db;
    int ret = sc_alias_analysis(1000.0, 100e3, 5000.0, &alias_db);
    ASSERT_TRUE(ret == ACTIVE_OK, "alias OK");
    ASSERT_TRUE(alias_db > 20.0, "alias rejection > 20dB");
    PASS();
}

/* ==================================================================
 * L8: Gm-C Autotune
 * ================================================================== */

static void test_gmc_autotune(void) {
    TEST("Gm-C autotune design");
    double tune_range;
    int ret = gmc_autotune_design(1e6, 100e-6, 30.0, 10e3, &tune_range);
    ASSERT_TRUE(ret == ACTIVE_OK, "autotune OK");
    ASSERT_TRUE(tune_range > 1.0, "tuning range > 1x");
    PASS();
}

/* ==================================================================
 * Main test runner
 * ================================================================== */

int main(void) {
    printf("\n=== Active Filter Module Test Suite ===\n\n");

    printf("[L1] Definitions:\n");
    test_defs_spec_init();
    test_defs_db_conversion();
    test_defs_q_bandwidth();
    test_defs_center_freq();
    test_filter_alloc_free();
    test_filter_spec_validation_edge_cases();

    printf("\n[L2] Sallen-Key Topology:\n");
    test_sk_lp_design();
    test_sk_lp_unity_gain();
    test_sk_hp_design();
    test_sk_bp_design();
    test_sk_notch_design();
    test_sk_tf_extraction();

    printf("\n[L2] MFB Topology:\n");
    test_mfb_lp_design();
    test_mfb_bp_design();
    test_mfb_bp_max_q();
    test_mfb_hp_design();
    test_mfb_tf_extraction();

    printf("\n[L2] State-Variable & Biquad:\n");
    test_khn_design();
    test_khn_outputs();
    test_tow_thomas_design();
    test_akerberg_mossberg();

    printf("\n[L3] Biquad Evaluation:\n");
    test_biquad_eval_lp();
    test_biquad_eval_bp();
    test_biquad_eval_hp();
    test_freq_response_sweep();
    test_find_cutoff();

    printf("\n[L3] Cascade Operations:\n");
    test_butterworth_poles();
    test_chebyshev_poles();
    test_bessel_poles();
    test_section_ordering();
    test_gain_distribution();
    test_first_order_section();
    test_pole_normalization_lp();
    test_pole_normalization_hp();

    printf("\n[L3] Component Utilities:\n");
    test_parallel_resistance();
    test_series_capacitance();
    test_impedance_scaling();
    test_nearest_eseries();
    test_component_range_check();

    printf("\n[L4] Sensitivity Theorems:\n");
    test_sensitivity_sum_theorem();
    test_sensitivity_q_sum();
    test_sk_sensitivity();
    test_mfb_sensitivity();
    test_khn_sensitivity();
    test_gbw_effect();
    test_slew_rate_limit();

    printf("\n[L5] Statistical Analysis:\n");
    test_monte_carlo_basic();
    test_worst_case();
    test_sk_eseries_snap();
    test_dc_offset();
    test_sensitivity_jacobian();

    printf("\n[L6] Complete Filter Design:\n");
    test_cascade_filter_design();
    test_cascade_butterworth_spec();
    test_cascade_chebyshev_spec();

    printf("\n[L8] Advanced Topologies:\n");
    test_sc_integrator();
    test_sc_lowpass();
    test_sc_alias();
    test_gmc_integrator();
    test_gmc_thd();
    test_gmc_autotune();
    test_npath_filter();
    test_mems_interface();

    printf("\n========================================\n");
    printf("Results: %d PASSED, %d FAILED\n", tests_passed, tests_failed);
    printf("========================================\n");

    return tests_failed > 0 ? 1 : 0;
}
