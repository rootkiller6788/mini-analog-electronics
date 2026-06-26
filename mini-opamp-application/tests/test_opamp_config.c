/**
 * test_opamp_config.c - Tests for amplifier configurations
 *
 * Tests cover feedback factor, noise gain, bandwidth, gain calculations,
 * impedance analysis, IA design, and DC operating point solving.
 */

#include "opamp_config.h"
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
    printf("=== Test Suite: opamp_config ===\n\n");

    OpAmpParams opamp = opamp_params_init_default();

    /* ========================================
     * Non-inverting amplifier tests
     * ======================================== */
    printf("L2-L3: Non-inverting amplifier\n");

    AmplifierConfig cfg_ninv;
    cfg_ninv.type = OPAMP_CFG_NON_INVERTING;
    cfg_ninv.R_f = 10000.0;
    cfg_ninv.R_g = 1000.0;
    cfg_ninv.R_load = 10000.0;
    cfg_ninv.target_gain = 11.0;

    TEST("non_inv_feedback_factor");
    double beta = opamp_cfg_feedback_factor(&cfg_ninv);
    ASSERT_EQ_FLOAT(beta, 1.0/11.0, 0.001, "beta=R_g/(R_g+R_f)=1/11");

    TEST("non_inv_closed_loop_gain");
    double gain = opamp_cfg_closed_loop_gain(&cfg_ninv, &opamp);
    ASSERT_TRUE(fabs(gain - 11.0) < 0.01, "gain approx 11");

    TEST("non_inv_noise_gain");
    double ng = opamp_cfg_noise_gain(&cfg_ninv);
    ASSERT_EQ_FLOAT(ng, 11.0, 0.01, "noise gain = 11 for non-inverting");

    TEST("non_inv_bandwidth");
    double bw = opamp_cfg_bandwidth(&cfg_ninv, &opamp);
    ASSERT_EQ_FLOAT(bw, 1.0e6/11.0, 1000.0, "BW = GBW/NG");

    TEST("non_inv_input_impedance_high");
    double zin = opamp_cfg_input_impedance(&cfg_ninv, &opamp);
    ASSERT_TRUE(zin > 1.0e6, "Zin should be > 1 MOhm (bootstrapped)");

    /* ========================================
     * Inverting amplifier tests
     * ======================================== */
    printf("\nL2-L3: Inverting amplifier\n");

    AmplifierConfig cfg_inv;
    cfg_inv.type = OPAMP_CFG_INVERTING;
    cfg_inv.R_f = 10000.0;
    cfg_inv.R_i = 1000.0;
    cfg_inv.R_load = 10000.0;
    cfg_inv.target_gain = -10.0;

    TEST("inv_feedback_factor");
    beta = opamp_cfg_feedback_factor(&cfg_inv);
    ASSERT_EQ_FLOAT(beta, 1.0/11.0, 0.001, "beta=R_i/(R_i+R_f)=1/11");

    TEST("inv_closed_loop_gain");
    gain = opamp_cfg_closed_loop_gain(&cfg_inv, &opamp);
    ASSERT_TRUE(fabs(gain + 10.0) < 0.01, "gain approx -10");

    TEST("inv_noise_gain_gt_signal_gain");
    ng = opamp_cfg_noise_gain(&cfg_inv);
    ASSERT_TRUE(ng > 10.0, "noise gain > |signal gain| for inverting");

    TEST("inv_input_impedance_equals_Ri");
    zin = opamp_cfg_input_impedance(&cfg_inv, &opamp);
    ASSERT_EQ_FLOAT(zin, 1000.0, 1.0, "Zin = R_i for inverting");

    /* ========================================
     * Voltage follower tests
     * ======================================== */
    printf("\nL2-L3: Voltage follower\n");

    AmplifierConfig cfg_fol;
    cfg_fol.type = OPAMP_CFG_VOLTAGE_FOLLOWER;
    cfg_fol.R_load = 10000.0;

    TEST("follower_feedback_factor");
    beta = opamp_cfg_feedback_factor(&cfg_fol);
    ASSERT_EQ_FLOAT(beta, 1.0, 0.001, "beta=1 for follower");

    TEST("follower_gain");
    gain = opamp_cfg_closed_loop_gain(&cfg_fol, &opamp);
    ASSERT_TRUE(fabs(gain - 1.0) < 0.001, "gain approx 1");

    TEST("follower_output_impedance_low");
    double zout = opamp_cfg_output_impedance(&cfg_fol, &opamp);
    ASSERT_TRUE(zout < 1.0, "Zout should be < 1 Ohm");

    /* ========================================
     * Resistor design tests
     * ======================================== */
    printf("\nL4: Resistor design\n");

    TEST("design_resistors_non_inv");
    AmplifierConfig cfg_design;
    cfg_design.type = OPAMP_CFG_NON_INVERTING;
    int ok = opamp_cfg_design_resistors(&cfg_design, &opamp, 10.0, 0.0);
    ASSERT_TRUE(ok, "design should succeed");
    ASSERT_TRUE(fabs(cfg_design.R_f/cfg_design.R_g - 9.0) < 0.01,
                "R_f/R_g should be gain-1 = 9");

    TEST("design_resistors_inv");
    cfg_design.type = OPAMP_CFG_INVERTING;
    ok = opamp_cfg_design_resistors(&cfg_design, &opamp, 10.0, 0.0);
    ASSERT_TRUE(ok, "design should succeed");

    /* ========================================
     * Sensitivity analysis
     * ======================================== */
    printf("\nL4: Sensitivity analysis\n");

    TEST("sensitivity_analysis_inv_1pct");
    double gain_min, gain_max;
    cfg_inv.target_gain = -10.0;
    opamp_cfg_sensitivity_analysis(&cfg_inv, 1.0, &gain_min, &gain_max);
    ASSERT_TRUE(gain_min < -10.0, "1% tolerance creates gain range");

    /* ========================================
     * Instrumentation amplifier
     * ======================================== */
    printf("\nL5: Instrumentation amplifier\n");

    InstrumentationAmpConfig ia_cfg;
    ia_cfg.target_gain = 100.0;
    ia_cfg.target_CMRR = 100.0;
    ia_cfg.target_bw = 10000.0;

    TEST("ia_design_success");
    double actual_gain, actual_CMRR;
    ok = opamp_ia_design(&ia_cfg, &opamp, &actual_gain, &actual_CMRR);
    ASSERT_TRUE(ok, "IA design should succeed");
    ASSERT_TRUE(fabs(actual_gain - 100.0) < 20.0, "gain within 20% of target");

    TEST("ia_cmrr_estimate");
    double cmrr = opamp_ia_cmrr_estimate(&ia_cfg, &opamp, 0.001);
    ASSERT_TRUE(cmrr > 60.0, "CMRR should be > 60 dB with 0.1% resistors");

    /* ========================================
     * DC operating point
     * ======================================== */
    printf("\nL6: DC operating point\n");

    TEST("dc_op_non_inv");
    double v_nodes[4], i_branch[3];
    int linear = opamp_cfg_solve_dc_operating_point(&cfg_ninv, &opamp, 0.5,
                                                      v_nodes, i_branch);
    ASSERT_TRUE(linear, "should be in linear region");
    ASSERT_TRUE(fabs(v_nodes[2] - 5.5) < 0.1, "v_out should be 5.5V for v_in=0.5 gain=11");

    TEST("dc_op_inv");
    linear = opamp_cfg_solve_dc_operating_point(&cfg_inv, &opamp, 0.5,
                                                  v_nodes, i_branch);
    ASSERT_TRUE(linear, "should be in linear region");
    ASSERT_TRUE(fabs(v_nodes[2] + 5.0) < 0.1, "v_out should be -5V for v_in=0.5 gain=-10");

    /* ========================================
     * Summary
     * ======================================== */
    printf("\n=== Results: %d passed, %d failed ===\n",
           tests_passed, tests_failed);

    return (tests_failed > 0) ? 1 : 0;
}
