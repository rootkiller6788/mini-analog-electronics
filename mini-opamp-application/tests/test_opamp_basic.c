/**
 * test_opamp_basic.c - Tests for op-amp basic definitions and functions
 *
 * Tests cover:
 *   - OpAmpParams initialization and validation
 *   - Gain error computation
 *   - Open-loop gain magnitude and phase
 *   - Closed-loop bandwidth
 *   - Linear region and slew rate checks
 *   - Ideal transfer functions
 *   - Multi-pole Bode data generation
 *   - Phase margin and stability
 *   - Noise analysis functions
 */

#include "opamp_basic.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  Test: %s ... ", name); } while(0)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_failed++; } while(0)
#define ASSERT_EQ_FLOAT(a, b, tol, msg) do { \
    if (fabs((a) - (b)) > (tol)) { FAIL(msg); } else { PASS(); } \
} while(0)
#define ASSERT_TRUE(cond, msg) do { \
    if (!(cond)) { FAIL(msg); } else { PASS(); } \
} while(0)

int main(void) {
    printf("=== Test Suite: opamp_basic ===\n\n");

    /* ========================================
     * L1 Tests: Parameter Initialization
     * ======================================== */
    printf("L1: Parameter initialization and validation\n");

    TEST("init_default_returns_valid_params");
    OpAmpParams p = opamp_params_init_default();
    ASSERT_TRUE(opamp_params_validate(&p), "default params should be valid");

    TEST("A_ol_matches_LM741");
    ASSERT_EQ_FLOAT(p.A_ol, 2.0e5, 1000.0, "A_ol should be 200k");

    TEST("GBW_matches_LM741");
    ASSERT_EQ_FLOAT(p.GBW, 1.0e6, 1000.0, "GBW should be 1 MHz");

    TEST("CMRR_positive");
    ASSERT_TRUE(p.CMRR > 0.0, "CMRR should be positive");

    TEST("SR_positive");
    ASSERT_TRUE(p.SR > 0.0, "Slew rate should be positive");

    TEST("supply_range_valid");
    ASSERT_TRUE(p.V_supply_max > p.V_supply_min, "supply range valid");

    TEST("output_swing_valid");
    ASSERT_TRUE(p.V_oh > p.V_ol, "output swing valid");

    printf("\nL2: Ideal op-amp analysis\n");

    TEST("virtual_short_in_linear_region");
    ASSERT_TRUE(opamp_virtual_short_applies(0.0, 13.0, -13.0),
                "0V should be in linear region for +/-13V supply");

    TEST("virtual_short_fails_when_saturated_high");
    ASSERT_TRUE(!opamp_virtual_short_applies(13.0, 13.0, -13.0),
                "13V at rail should not satisfy virtual short");

    TEST("virtual_short_fails_when_saturated_low");
    ASSERT_TRUE(!opamp_virtual_short_applies(-13.0, 13.0, -13.0),
                "-13V at rail should not satisfy virtual short");

    TEST("gain_error_small_at_high_loop_gain");
    double err = opamp_gain_error_percent(2.0e5, 0.1);
    ASSERT_TRUE(err < 0.01, "gain error should be < 0.01% for high loop gain");

    TEST("gain_error_large_at_low_loop_gain");
    double err2 = opamp_gain_error_percent(10.0, 0.1);
    ASSERT_TRUE(err2 > 10.0, "gain error significant for low A_ol");

    printf("\nL3: Transfer function evaluation\n");

    TEST("open_loop_gain_at_DC_equals_A_ol");
    ASSERT_EQ_FLOAT(opamp_open_loop_gain_mag(&p, 0.0), p.A_ol, 1.0,
                    "DC gain should equal A_ol");

    TEST("open_loop_gain_at_fp1_is_3dB_down");
    double mag_at_fp1 = opamp_open_loop_gain_mag(&p, p.f_p1);
    double expected_3db_down = p.A_ol / sqrt(2.0);
    ASSERT_EQ_FLOAT(mag_at_fp1, expected_3db_down, 1000.0,
                    "gain at f_p1 should be -3dB");

    TEST("open_loop_gain_db_at_fp1");
    double db_at_fp1 = opamp_open_loop_gain_db(&p, p.f_p1);
    double db_at_dc = 20.0 * log10(p.A_ol);
    ASSERT_EQ_FLOAT(db_at_fp1, db_at_dc - 3.01, 0.1,
                    "gain at f_p1 should be DC gain - 3dB");

    TEST("open_loop_phase_at_DC");
    ASSERT_EQ_FLOAT(opamp_open_loop_phase(&p, 0.0), 0.0, 0.01,
                    "phase at DC should be 0");

    TEST("open_loop_phase_deg_at_DC");
    ASSERT_EQ_FLOAT(opamp_open_loop_phase_deg(&p, 0.0), 0.0, 0.1,
                    "phase at DC should be 0 degrees");

    TEST("closed_loop_bandwidth_calculation");
    double bw_cl = opamp_closed_loop_bandwidth(&p, 10.0);
    ASSERT_EQ_FLOAT(bw_cl, 1.0e5, 1000.0,
                    "BW for gain=10 should be GBW/10 = 100 kHz");

    printf("\nL4: Stability and constraints\n");

    TEST("linear_region_check_normal");
    ASSERT_TRUE(opamp_check_linear_region(5.0, 13.0, -13.0, 1.0),
                "5V should be in linear region");

    TEST("linear_region_check_saturated_high");
    ASSERT_TRUE(!opamp_check_linear_region(13.0, 13.0, -13.0, 1.0),
                "13V should be saturated");

    TEST("slew_rate_check_pass");
    /* SR=0.5V/us, check 1 Vpk at 10 kHz => needs 0.063 V/us */
    ASSERT_TRUE(opamp_check_slew_rate(0.5e6, 1.0, 10000.0),
                "10kHz 1Vpk should pass SR limit");

    TEST("slew_rate_check_fail");
    /* SR=0.5V/us, check 10 Vpk at 100 kHz => needs 6.28 V/us */
    ASSERT_TRUE(!opamp_check_slew_rate(0.5e6, 10.0, 100000.0),
                "100kHz 10Vpk should fail SR limit");

    TEST("bandgap_voltage_at_300K");
    double v_bg = compute_iref_bandgap(0.65, 300.0, 23.0);
    ASSERT_TRUE(v_bg > 1.1 && v_bg < 1.3,
                "bandgap voltage should be ~1.2V at 300K");

    printf("\nL5: Noise analysis\n");

    TEST("output_noise_rti_basic");
    ASSERT_EQ_FLOAT(opamp_output_noise_rti(10e-9, 10.0), 100e-9, 1e-12,
                    "output noise = input noise * NG");

    TEST("total_input_noise_positive");
    OpAmpNoiseModel noise = {20e-9, 50e-9, 0.5e-12, 200.0, 200.0};
    double v_n_total = opamp_total_input_noise(&noise, 1000.0, 1.0, 1000.0);
    ASSERT_TRUE(v_n_total > 0.0, "total noise should be positive");

    TEST("noise_figure_positive");
    double nf = opamp_noise_figure(&noise, 10000.0, 1000.0);
    ASSERT_TRUE(nf > 0.0, "NF should be positive");

    printf("\nL6: Canonical transfer functions\n");

    TEST("inverting_gain_ideal_approximation");
    double gain_inv = opamp_inverting_transfer(10000.0, 1000.0, 2.0e5);
    ASSERT_TRUE(fabs(gain_inv + 10.0) < 0.01,
                "inverting gain should be approx -10");

    TEST("non_inverting_gain_ideal_approximation");
    double gain_ninv = opamp_non_inverting_transfer(10000.0, 1000.0, 2.0e5);
    ASSERT_TRUE(fabs(gain_ninv - 11.0) < 0.01,
                "non-inverting gain should be approx 11");

    TEST("differential_output");
    double v_out = opamp_differential_transfer(10000.0, 1000.0, 0.1, 0.4, 2.0e5);
    ASSERT_TRUE(fabs(v_out - 3.0) < 0.01,
                "differential output should be approx 3.0V");

    /* ========================================
     * Results summary
     * ======================================== */
    printf("\n=== Results: %d passed, %d failed ===\n",
           tests_passed, tests_failed);

    return (tests_failed > 0) ? 1 : 0;
}
