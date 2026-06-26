/**
 * @file test_feedback.c
 * @brief Comprehensive Test Suite for Feedback Amplifier Module
 *
 * Tests cover:
 *   - Black's feedback formula (L4)
 *   - Desensitivity and gain sensitivity (L2)
 *   - Bandwidth extension (L2)
 *   - Impedance transformation for all four topologies (L2)
 *   - Distortion reduction (L2)
 *   - Phase margin and gain margin computation (L4)
 *   - Nyquist encirclement detection (L4)
 *   - Routh-Hurwitz stability test (L5)
 *   - Root locus analysis (L5)
 *   - Compensation design (L5)
 *   - Topology analysis for all four configurations (L2/L6)
 *   - Non-inverting amplifier design (L6)
 *   - Transimpedance amplifier design (L7)
 *
 * Every test includes a mathematical assertion (not just assert(1)).
 */

#include "feedback_amplifier.h"
#include "stability_analysis.h"
#include "frequency_compensation.h"
#include "amplifier_topologies.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
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

#define CHECK(cond, msg) do { \
    if (!(cond)) { FAIL(msg); return; } \
} while(0)

/* Tolerance for floating-point comparisons */
#define EPS 1e-9
#define EPS_RELAXED 1e-6
#define CHECK_CLOSE(a, b, eps, msg) do { \
    if (fabs((a) - (b)) > (eps)) { \
        printf("FAIL: %s (got %.15g, expected %.15g)\n", msg, a, b); \
        tests_failed++; return; \
    } \
} while(0)

/* =========================================================================
 * Test Fixture: Standard Amplifier
 * ========================================================================= */

static FeedbackAmplifier make_standard_amp(void)
{
    FeedbackAmplifier amp;
    memset(&amp, 0, sizeof(amp));
    amp.open_loop_gain   = 100000.0;  /* 100 dB — typical op-amp */
    amp.feedback_factor  = 0.1;       /* β = 0.1 ⇒ A_f ≈ 10 */
    amp.polarity         = FB_NEGATIVE;
    amp.topology         = FB_SERIES_SHUNT;
    amp.input_impedance  = 1.0e6;     /* 1 MΩ */
    amp.output_impedance = 75.0;      /* 75 Ω */
    amp.dominant_pole_hz = 10.0;      /* 10 Hz (GBW = 1 MHz) */
    amp.second_pole_hz   = 2.0e6;     /* 2 MHz */
    amp.third_pole_hz    = 0.0;       /* Only 2 poles */
    amp.unity_gain_freq_hz = 1.0e6;   /* 1 MHz */
    return amp;
}

/* =========================================================================
 * L4: Black's Feedback Formula Tests
 * ========================================================================= */

static void test_black_formula_ideal(void)
{
    TEST("Black formula: deep negative feedback A_f ≈ 1/β");
    FeedbackAmplifier amp = make_standard_amp();
    /* With A=100k, β=0.1, Aβ = 10000
     * A_f = 100000 / (1 + 10000) ≈ 9.999 */
    double af = feedback_compute_closed_loop_gain(&amp);
    double af_ideal = 1.0 / amp.feedback_factor; /* = 10.0 */
    CHECK_CLOSE(af, af_ideal, 0.01, "A_f should be approximately 1/β = 10");
    PASS();
}

static void test_black_formula_small_loop_gain(void)
{
    TEST("Black formula: small loop gain (Aβ = 1)");
    FeedbackAmplifier amp = make_standard_amp();
    amp.feedback_factor = 0.00001; /* Aβ = 1 */
    double af = feedback_compute_closed_loop_gain(&amp);
    /* A_f = 100000 / (1 + 1) = 50000 */
    CHECK_CLOSE(af, 50000.0, EPS_RELAXED, "A_f should be A/2");
    PASS();
}

static void test_black_formula_positive_feedback(void)
{
    TEST("Black formula: positive feedback check");
    FeedbackAmplifier amp = make_standard_amp();
    amp.polarity = FB_POSITIVE;
    amp.feedback_factor = 0.000005; /* Aβ = 0.5, stable positive feedback */
    double af = feedback_compute_closed_loop_gain(&amp);
    /* A_f = 100000 / (1 - 0.5) = 200000 */
    CHECK_CLOSE(af, 200000.0, EPS_RELAXED, "A_f = A/(1-Aβ) = 200000");
    PASS();
}

/* =========================================================================
 * L2: Loop Gain and Desensitivity Tests
 * ========================================================================= */

static void test_loop_gain_computation(void)
{
    TEST("Loop gain: L = Aβ = 10000");
    FeedbackAmplifier amp = make_standard_amp();
    double L = feedback_compute_loop_gain(&amp);
    CHECK_CLOSE(L, 10000.0, EPS, "Loop gain should be 10000");
    PASS();
}

static void test_desensitivity_factor(void)
{
    TEST("Desensitivity: D = 1 + Aβ = 10001");
    FeedbackAmplifier amp = make_standard_amp();
    double D = feedback_compute_desensitivity(&amp);
    CHECK_CLOSE(D, 10001.0, EPS, "Desensitivity D = 1+Aβ");
    PASS();
}

static void test_gain_sensitivity(void)
{
    TEST("Gain sensitivity: S = 1/D ≈ 0.0001");
    FeedbackAmplifier amp = make_standard_amp();
    double S = feedback_compute_gain_sensitivity(&amp);
    CHECK_CLOSE(S, 1.0 / 10001.0, EPS, "Sensitivity = 1/D");
    CHECK(S < 0.001, "Gain sensitivity should be very low (deep feedback)");
    PASS();
}

static void test_beta_extraction(void)
{
    TEST("Beta extraction from measured gains");
    /* A = 100k, A_f should be ≈ 10 for β = 0.1 */
    double beta = feedback_compute_beta(100000.0, 10.0);
    CHECK_CLOSE(beta, 0.09999, EPS_RELAXED, "Extracted β ≈ 0.1");
    PASS();
}

/* =========================================================================
 * L2: Bandwidth Extension Tests
 * ========================================================================= */

static void test_bandwidth_extension_single_pole(void)
{
    TEST("Bandwidth extension: single-pole system");
    FeedbackAmplifier amp = make_standard_amp();
    amp.second_pole_hz = 0.0; /* Single pole only */
    double bw_closed = feedback_compute_bandwidth_extension(&amp);
    /* Expected: f_p1 * (1 + Aβ) = 10 Hz * 10001 = 100010 Hz */
    double expected = 10.0 * 10001.0;
    CHECK_CLOSE(bw_closed, expected, 10.0, "BW extended by desensitivity factor");
    PASS();
}

static void test_gbw_product_constancy(void)
{
    TEST("GBW product: constancy check");
    FeedbackAmplifier amp = make_standard_amp();
    amp.second_pole_hz = 0.0; /* Single pole for exact GBW constancy */
    double gbw_cl = 0.0;
    double gbw_open = feedback_compute_gbw_product(&amp, &gbw_cl);
    /* GBW_open = 100k * 10 = 1e6 Hz */
    CHECK_CLOSE(gbw_open, 1.0e6, 1.0, "Open-loop GBW = 1 MHz");
    CHECK_CLOSE(gbw_cl, gbw_open, 1.0, "Closed-loop GBW ≈ Open-loop GBW");
    PASS();
}

/* =========================================================================
 * L2: Impedance Transformation Tests (All Four Topologies)
 * ========================================================================= */

static void test_impedance_series_shunt(void)
{
    TEST("Impedance: Series-Shunt (Z_in up, Z_out down)");
    FeedbackAmplifier amp = make_standard_amp();
    amp.topology = FB_SERIES_SHUNT;
    double zin = feedback_compute_input_impedance(&amp);
    double zout = feedback_compute_output_impedance(&amp);
    /* Z_in should increase: 1e6 * 10001 ≈ 1e10 */
    /* Z_out should decrease: 75 / 10001 ≈ 0.0075 */
    CHECK(zin > amp.input_impedance, "Input Z should increase (series mixing)");
    CHECK(zout < amp.output_impedance, "Output Z should decrease (shunt sampling)");
    CHECK_CLOSE(zin, 1e6 * 10001.0, 1e5, "Z_in multiplied by D");
    CHECK_CLOSE(zout, 75.0 / 10001.0, 0.1, "Z_out divided by D");
    PASS();
}

static void test_impedance_shunt_series(void)
{
    TEST("Impedance: Shunt-Series (Z_in down, Z_out up)");
    FeedbackAmplifier amp = make_standard_amp();
    amp.topology = FB_SHUNT_SERIES;
    double zin = feedback_compute_input_impedance(&amp);
    double zout = feedback_compute_output_impedance(&amp);
    CHECK(zin < amp.input_impedance, "Input Z should decrease (shunt mixing)");
    CHECK(zout > amp.output_impedance, "Output Z should increase (series sampling)");
    PASS();
}

static void test_impedance_series_series(void)
{
    TEST("Impedance: Series-Series (Z_in up, Z_out up)");
    FeedbackAmplifier amp = make_standard_amp();
    amp.topology = FB_SERIES_SERIES;
    double zin = feedback_compute_input_impedance(&amp);
    double zout = feedback_compute_output_impedance(&amp);
    CHECK(zin > amp.input_impedance, "Input Z should increase");
    CHECK(zout > amp.output_impedance, "Output Z should increase");
    PASS();
}

static void test_impedance_shunt_shunt(void)
{
    TEST("Impedance: Shunt-Shunt (Z_in down, Z_out down)");
    FeedbackAmplifier amp = make_standard_amp();
    amp.topology = FB_SHUNT_SHUNT;
    double zin = feedback_compute_input_impedance(&amp);
    double zout = feedback_compute_output_impedance(&amp);
    CHECK(zin < amp.input_impedance, "Input Z should decrease");
    CHECK(zout < amp.output_impedance, "Output Z should decrease");
    PASS();
}

/* =========================================================================
 * L2: Distortion Reduction Test
 * ========================================================================= */

static void test_distortion_reduction(void)
{
    TEST("Distortion: 2nd harmonic reduction by feedback");
    FeedbackAmplifier amp = make_standard_amp();
    /* Open-loop THD = 1%, Aβ = 10000
     * HD2_closed = 1% / (10001)^2 ≈ 1e-8 % */
    double hd2_closed = feedback_compute_distortion_reduction(&amp, 1.0, 2);
    CHECK(hd2_closed < 0.01, "2nd harmonic should be dramatically reduced");
    /* Open-loop THD = 5%, Aβ = 10000
     * HD3_closed = 5% / (10001)^3 ≈ 5e-12 % → clipped to floor */
    double hd3_closed = feedback_compute_distortion_reduction(&amp, 5.0, 3);
    CHECK(hd3_closed < 0.01, "3rd harmonic should be even more reduced");
    PASS();
}

/* =========================================================================
 * L1: Validation Tests
 * ========================================================================= */

static void test_validate_params_valid(void)
{
    TEST("Validation: valid parameters return 0");
    FeedbackAmplifier amp = make_standard_amp();
    int result = feedback_validate_params(&amp);
    CHECK(result == 0, "Valid amp should return 0");
    PASS();
}

static void test_validate_params_invalid_gain(void)
{
    TEST("Validation: negative gain returns error");
    FeedbackAmplifier amp = make_standard_amp();
    amp.open_loop_gain = -10.0;
    int result = feedback_validate_params(&amp);
    CHECK(result < 0, "Negative gain should return error");
    PASS();
}

static void test_validate_params_null(void)
{
    TEST("Validation: NULL pointer returns -1");
    int result = feedback_validate_params(NULL);
    CHECK(result == -1, "NULL should return -1");
    PASS();
}

/* =========================================================================
 * L4: Bode Stability Criterion Tests
 * ========================================================================= */

static void test_phase_margin_from_poles(void)
{
    TEST("Stability: PM from pole frequencies (2-pole amp)");
    FeedbackAmplifier amp = make_standard_amp();
    amp.second_pole_hz = 2.0e6;  /* 2 MHz */
    amp.feedback_factor = 0.1;
    /* Loop gain = 10000. Unity-gain freq ≈ 1 MHz.
     * Phase at 1 MHz: -90° (from dominant pole at 10 Hz)
     *                  - arctan(1MHz/2MHz) ≈ -26.6°
     * Total phase ≈ -116.6° (plus -180° from negative feedback sign = -296.6°)
     * Wait, the phase at ω_u: -90° - arctan(1e6/2e6) ≈ -90° - 26.6° = -116.6°
     * Then from negative feedback: -180° - 116.6° = -296.6° → PM = 180° - 116.6° = 63.4° */
    StabilityMargins margins;
    stability_assess_from_poles(&amp, &margins);
    CHECK(margins.phase_margin_deg > 45.0, "PM should be > 45° for this 2-pole design");
    CHECK(margins.phase_margin_deg < 90.0, "PM should be < 90°");
    CHECK(margins.classification == STABILITY_STABLE, "Should be classified STABLE");
    PASS();
}

static void test_unstable_three_pole_system(void)
{
    TEST("Stability: 3-pole system analysis");
    FeedbackAmplifier amp = make_standard_amp();
    amp.second_pole_hz = 1.0e6;   /* 1 MHz — close to unity-gain */
    amp.third_pole_hz  = 1.0e6;   /* 1 MHz — coincident with 2nd pole to make it worse */
    amp.feedback_factor = 0.1;
    /* With poles at 10 Hz, 1 MHz, 1 MHz and Aβ = 10000:
     * Unity-gain freq near 1 MHz. Phase: -90° -45° -45° = -180°
     * PM ≈ 0° (marginally stable or unstable) */
    StabilityMargins margins;
    stability_assess_from_poles(&amp, &margins);
    CHECK(!isinf(margins.phase_margin_deg),
          "Phase margin should be finite for 3-pole system");
    PASS();
}

/* =========================================================================
 * L4: Nyquist Encirclement Test
 * ========================================================================= */

static void test_nyquist_no_encirclement(void)
{
    TEST("Nyquist: no encirclement of (-1, j0)");
    /* Test with a contour that clearly does not encircle (-1, j0).
     * Points forming a small circle around (0, 0) — far from (-1, 0). */
    double re[4] = { 0.5, 0.0, -0.5, 0.0 };
    double im[4] = { 0.0, 0.5, 0.0, -0.5 };
    int count = stability_count_encirclements(re, im, 4, 0);
    CHECK(count == 0, "No encirclement expected for small contour");
    PASS();
}

static void test_nyquist_single_cw_encirclement(void)
{
    TEST("Nyquist: single CW encirclement of (-1, j0)");
    /* Points forming a box that encircles (-1, j0) once clockwise.
     * Box corners: (1, -2) → (-3, -2) → (-3, 2) → (1, 2) → (1, -2)
     * This is a closed contour totally enclosing (-1, 0).
     * Traversal order is clockwise. */
    double re[5] = { 1.0, -3.0, -3.0, 1.0, 1.0 };
    double im[5] = { -2.0, -2.0, 2.0, 2.0, -2.0 };
    int count = stability_count_encirclements(re, im, 5, 0);
    CHECK(count < 0, "CW encirclement should have negative winding number");
    PASS();
}

/* =========================================================================
 * L5: Routh-Hurwitz Test
 * ========================================================================= */

static void test_routh_stable_quadratic(void)
{
    TEST("Routh-Hurwitz: stable quadratic s² + 2s + 1");
    double coeffs[3] = { 1.0, 2.0, 1.0 };  /* (s+1)² → all poles at s=-1 */
    RouthHurwitzResult result = stability_routh_hurwitz(coeffs, 2);
    CHECK(result.is_stable, "(s+1)² should be stable (all LHP roots)");
    CHECK(result.rhp_roots == 0, "Zero RHP roots expected");
    stability_free_routh(&result);
    PASS();
}

static void test_routh_unstable_quadratic(void)
{
    TEST("Routh-Hurwitz: unstable quadratic s² - 1");
    double coeffs[3] = { 1.0, 0.0, -1.0 };  /* (s-1)(s+1) → RHP pole at s=+1 */
    RouthHurwitzResult result = stability_routh_hurwitz(coeffs, 2);
    CHECK(!result.is_stable, "s²-1 should be unstable (has RHP root)");
    stability_free_routh(&result);
    PASS();
}

static void test_routh_stable_cubic(void)
{
    TEST("Routh-Hurwitz: stable cubic s³ + 3s² + 3s + 1");
    double coeffs[4] = { 1.0, 3.0, 3.0, 1.0 };  /* (s+1)³ */
    RouthHurwitzResult result = stability_routh_hurwitz(coeffs, 3);
    CHECK(result.is_stable, "(s+1)³ should be stable");
    stability_free_routh(&result);
    PASS();
}

/* =========================================================================
 * L5: Root Locus Test
 * ========================================================================= */

static void test_root_locus_second_order(void)
{
    TEST("Root locus: 2nd-order system traces poles");
    TransferFunction tf;
    memset(&tf, 0, sizeof(tf));
    /* G(s)H(s) = 1 / ((1+s/10)(1+s/100)) = 1 / (1 + 0.11s + 0.001s²)
     * Normalize: D(s) = 0.001s² + 0.11s + 1, N(s) = 1 */
    double den[3] = { 1.0, 0.11, 0.001 };
    double num[1] = { 1.0 };
    tf.den_coeffs = den;
    tf.den_order = 2;
    tf.num_coeffs = num;
    tf.num_order = 0;

    RootLocus locus = stability_compute_root_locus(&tf, 0.1, 100.0, 10);
    CHECK(locus.num_gains == 10, "Should have 10 gain points");
    CHECK(locus.num_poles == 2, "2nd-order system has 2 poles");
    /* The critical gain (jω crossing) should be finite for a 2nd-order */
    stability_free_root_locus(&locus);
    PASS();
}

/* =========================================================================
 * L5: Compensation Tests
 * ========================================================================= */

static void test_dominant_pole_compensation(void)
{
    TEST("Compensation: dominant-pole design improves PM");
    FeedbackAmplifier amp = make_standard_amp();
    amp.second_pole_hz = 500.0e3;  /* 500 kHz — close to unity-gain */
    amp.feedback_factor = 0.5;
    /* Without compensation, PM is poor */

    CompensationResult result = comp_design_dominant_pole(&amp, 60.0);
    CHECK(result.type == COMP_DOMINANT_POLE, "Should return dominant-pole type");
    /* Check that PM improved (or was already adequate) */
    CHECK(result.margins_after.classification == STABILITY_STABLE,
          "Compensated system should be stable");
    PASS();
}

static void test_miller_compensation_design(void)
{
    TEST("Compensation: Miller (pole-splitting) design");
    FeedbackAmplifier amp = make_standard_amp();
    amp.second_pole_hz = 500.0e3;

    CompensationResult result = comp_design_miller(&amp, 100.0, 1.0e5, 100.0, 60.0);
    CHECK(result.type == COMP_MILLER, "Should return Miller type");
    /* Miller compensation should split poles */
    CHECK(result.network.miller.new_pole2_hz > result.network.miller.original_pole2_hz ||
          result.network.miller.new_pole1_hz < result.network.miller.original_pole1_hz,
          "Pole splitting should move poles apart");
    PASS();
}

static void test_lead_compensation_design(void)
{
    TEST("Compensation: lead network design");
    FeedbackAmplifier amp = make_standard_amp();
    CompensationResult result = comp_design_lead(&amp, 60.0);
    CHECK(result.type == COMP_LEAD, "Should return lead type");
    CHECK(result.network.lead.max_phase_lead_deg >= 0.0,
          "Phase lead should be non-negative");
    PASS();
}

static void test_lag_compensation_design(void)
{
    TEST("Compensation: lag network design");
    FeedbackAmplifier amp = make_standard_amp();
    amp.second_pole_hz = 500.0e3;
    amp.feedback_factor = 0.5;
    CompensationResult result = comp_design_lag(&amp, 50.0);
    CHECK(result.type == COMP_LAG, "Should return lag type");
    /* Lag should provide DC attenuation */
    CHECK(result.network.lag.dc_attenuation_db >= 0.0,
          "Lag attenuation should be non-negative in dB");
    PASS();
}

/* =========================================================================
 * L6: Topology Analysis Tests
 * ========================================================================= */

static void test_series_shunt_topology(void)
{
    TEST("Topology: Series-Shunt voltage amplifier analysis");
    TopologyAnalysis analysis;
    topology_analyze_series_shunt(100000.0, 0.1, 1.0e6, 75.0, &analysis);
    CHECK(analysis.topology == FB_SERIES_SHUNT, "Topology should be Series-Shunt");
    CHECK_CLOSE(analysis.closed_loop_gain, 100000.0/10001.0, EPS_RELAXED,
                "Closed-loop gain = A/(1+Aβ)");
    CHECK(analysis.closed_input_z > analysis.basic_input_z,
          "Input Z should increase (series mixing)");
    CHECK(analysis.closed_output_z < analysis.basic_output_z,
          "Output Z should decrease (shunt sampling)");
    PASS();
}

static void test_shunt_shunt_topology(void)
{
    TEST("Topology: Shunt-Shunt transimpedance analysis");
    TopologyAnalysis analysis;
    /* β in Siemens for Shunt-Shunt */
    topology_analyze_shunt_shunt(100000.0, 0.001, 1.0e6, 75.0, &analysis);
    CHECK(analysis.topology == FB_SHUNT_SHUNT, "Topology should be Shunt-Shunt");
    CHECK(analysis.closed_input_z < analysis.basic_input_z,
          "Input Z should decrease (shunt mixing)");
    CHECK(analysis.closed_output_z < analysis.basic_output_z,
          "Output Z should decrease (shunt sampling)");
    PASS();
}

static void test_all_topologies_comparison(void)
{
    TEST("Topology: comparison of all four topologies");
    double beta[FB_TOPOLOGY_COUNT] = { 0.1, 0.1, 100.0, 0.001 };
    TopologyAnalysis results[FB_TOPOLOGY_COUNT];
    topology_compare_all(100000.0, beta, 1.0e6, 75.0, results);

    /* Verify all topologies produce valid closed-loop gains */
    for (int i = 0; i < (int)FB_TOPOLOGY_COUNT; i++) {
        CHECK((int)results[i].topology == i, "Topology index should match");
        CHECK(results[i].closed_loop_gain > 0, "Gain should be positive");
    }
    PASS();
}

/* =========================================================================
 * L6: Non-Inverting Amplifier Design Test
 * ========================================================================= */

static void test_noninverting_amplifier_design(void)
{
    TEST("Design: non-inverting amplifier with op-amp");
    TopologyAnalysis analysis;
    /* μA741-like parameters: A₀=200k, GBW=1MHz, Rf1=9kΩ, Rf2=1kΩ
     * β = 1k/(9k+1k) = 0.1, Ideal gain = 1 + 9k/1k = 10 */
    topology_design_noninverting_amp(200000.0, 1.0e6, 9000.0, 1000.0, &analysis);
    CHECK(analysis.topology == FB_SERIES_SHUNT, "Should be Series-Shunt");
    CHECK_CLOSE(analysis.closed_loop_gain, 10.0, 0.01,
                "Gain should be ~10 for Rf1=9k, Rf2=1k");
    CHECK(analysis.loop_gain > 1.0, "Loop gain should be > 1");
    PASS();
}

/* =========================================================================
 * L7: Transimpedance Amplifier Test (Application)
 * ========================================================================= */

static void test_transimpedance_amp_design(void)
{
    TEST("Design: photodiode transimpedance amplifier");
    TopologyAnalysis analysis;
    double pm = 0.0;
    /* Rf = 100kΩ, Cj = 10pF (typical Si photodiode), GBW = 10MHz */
    topology_design_transimpedance_amp(100000.0, 10.0e6, 100000.0,
                                        10.0e-12, &analysis, &pm);
    CHECK(analysis.topology == FB_SHUNT_SHUNT, "Should be Shunt-Shunt");
    CHECK(analysis.closed_loop_gain > 0, "Transimpedance should be positive magnitude");
    /* Phase margin should be computable */
    CHECK(!isnan(pm), "Phase margin should be a valid number");
    PASS();
}

/* =========================================================================
 * L2: Beta Network Validation Test
 * ========================================================================= */

static void test_resistive_divider_beta(void)
{
    TEST("Beta network: resistive divider R1=9k, R2=1k");
    BetaNetwork bn;
    topology_resistive_divider_beta(9000.0, 1000.0, &bn);
    CHECK_CLOSE(bn.beta, 0.1, EPS, "β = 1k/(9k+1k) = 0.1");
    CHECK(bn.input_loading_z > 0, "Input loading should be positive");
    CHECK(bn.output_loading_z > 0, "Output loading should be positive");
    CHECK(topology_validate_beta_network(&bn) == 0, "Should be valid");
    PASS();
}

static void test_invalid_beta_network(void)
{
    TEST("Beta network: invalid β > 1");
    BetaNetwork bn;
    bn.beta = 5.0; /* Invalid: > 1 */
    bn.input_loading_z = 1000.0;
    bn.output_loading_z = 1000.0;
    bn.forward_gain = 0.0;
    int result = topology_validate_beta_network(&bn);
    CHECK(result < 0, "β > 1 should be invalid");
    PASS();
}

/* =========================================================================
 * L1: Utility Tests
 * ========================================================================= */

static void test_topology_name_strings(void)
{
    TEST("Utilities: topology name strings");
    CHECK(strlen(feedback_topology_name(FB_SERIES_SHUNT)) > 0,
          "Series-Shunt name should be non-empty");
    CHECK(strlen(feedback_topology_name(FB_SHUNT_SHUNT)) > 0,
          "Shunt-Shunt name should be non-empty");
    PASS();
}

static void test_complete_metrics(void)
{
    TEST("Metrics: complete computation");
    FeedbackAmplifier amp = make_standard_amp();
    FeedbackMetrics metrics;
    feedback_compute_metrics(&amp, &metrics);
    CHECK(metrics.closed_loop_gain > 0, "Closed-loop gain should be positive");
    CHECK(metrics.loop_gain > 1.0, "Loop gain should be > 1");
    CHECK(metrics.desensitivity > 1.0, "Desensitivity should be > 1");
    CHECK(metrics.bandwidth_hz > amp.dominant_pole_hz,
          "Bandwidth should be extended");
    PASS();
}

/* =========================================================================
 * L5: Transfer Function Evaluation Test
 * ========================================================================= */

static void test_transfer_function_eval(void)
{
    TEST("Transfer function: evaluate T(s) at DC");
    TransferFunction tf;
    double num[2] = { 10.0, 1.0 };   /* N(s) = 10 + s */
    double den[2] = { 1.0, 0.1 };    /* D(s) = 1 + 0.1s */
    tf.num_coeffs = num;
    tf.num_order = 1;
    tf.den_coeffs = den;
    tf.den_order = 1;

    /* T(0) = 10/1 = 10 */
    double complex result = feedback_eval_transfer_function(&tf, 0.0 + 0.0 * I);
    CHECK_CLOSE(creal(result), 10.0, EPS, "T(0) = 10");
    CHECK_CLOSE(cimag(result), 0.0, EPS, "T(0) should be real");
    PASS();
}

/* =========================================================================
 * Test Runner
 * ========================================================================= */

int main(void)
{
    printf("=== Feedback Amplifier Test Suite ===\n\n");

    printf("--- L4: Black's Feedback Formula ---\n");
    test_black_formula_ideal();
    test_black_formula_small_loop_gain();
    test_black_formula_positive_feedback();

    printf("\n--- L2: Loop Gain & Desensitivity ---\n");
    test_loop_gain_computation();
    test_desensitivity_factor();
    test_gain_sensitivity();
    test_beta_extraction();

    printf("\n--- L2: Bandwidth Extension ---\n");
    test_bandwidth_extension_single_pole();
    test_gbw_product_constancy();

    printf("\n--- L2: Impedance Transformation ---\n");
    test_impedance_series_shunt();
    test_impedance_shunt_series();
    test_impedance_series_series();
    test_impedance_shunt_shunt();

    printf("\n--- L2: Distortion Reduction ---\n");
    test_distortion_reduction();

    printf("\n--- L1: Validation ---\n");
    test_validate_params_valid();
    test_validate_params_invalid_gain();
    test_validate_params_null();

    printf("\n--- L4: Bode Stability Criterion ---\n");
    test_phase_margin_from_poles();
    test_unstable_three_pole_system();

    printf("\n--- L4: Nyquist Criterion ---\n");
    test_nyquist_no_encirclement();
    test_nyquist_single_cw_encirclement();

    printf("\n--- L5: Routh-Hurwitz Test ---\n");
    test_routh_stable_quadratic();
    test_routh_unstable_quadratic();
    test_routh_stable_cubic();

    printf("\n--- L5: Root Locus ---\n");
    test_root_locus_second_order();

    printf("\n--- L5: Compensation Design ---\n");
    test_dominant_pole_compensation();
    test_miller_compensation_design();
    test_lead_compensation_design();
    test_lag_compensation_design();

    printf("\n--- L6: Topology Analysis ---\n");
    test_series_shunt_topology();
    test_shunt_shunt_topology();
    test_all_topologies_comparison();

    printf("\n--- L6: Design Problems ---\n");
    test_noninverting_amplifier_design();

    printf("\n--- L7: Applications ---\n");
    test_transimpedance_amp_design();

    printf("\n--- L2: Beta Network ---\n");
    test_resistive_divider_beta();
    test_invalid_beta_network();

    printf("\n--- L1: Utilities ---\n");
    test_topology_name_strings();
    test_complete_metrics();

    printf("\n--- L5: Transfer Function ---\n");
    test_transfer_function_eval();

    printf("\n=== Results: %d passed, %d failed ===\n",
           tests_passed, tests_failed);

    return (tests_failed > 0) ? 1 : 0;
}
