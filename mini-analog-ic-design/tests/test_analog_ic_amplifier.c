#include <string.h>
/* Test: Amplifier Topologies and Op-Amp Design */
#include "analog_ic_amplifier.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static int test_count = 0, pass_count = 0;
#define TEST(n) do { test_count++; printf("  %s ... ", n); } while(0)
#define PASS() do { pass_count++; printf("PASS\n"); } while(0)

int main(void)
{
    printf("=== Test: analog_ic_amplifier ===\n");

    ic_technology_t *tech = ic_tech_init_180nm();
    assert(tech != NULL);

    /* Setup a standard NMOS transistor */
    mos_transistor_t mn;
    memset(&mn, 0, sizeof(mn));
    mn.type = MOS_TYPE_NMOS; mn.W = 10e-6; mn.L = 1e-6;
    mn.Vgs = 0.7; mn.Vds = 1.0; mn.Vsb = 0.0; mn.m = 1;
    mn.Id = mos_id_square_law(tech, &mn);

    amplifier_spec_t spec;
    memset(&spec, 0, sizeof(spec));
    spec.transistor = mn; spec.Rd = 10e3; spec.Rload = 100e3;
    spec.Cload = 1e-12; spec.Vdd = 1.8; spec.I_bias = 100e-6;

    /* L2: Common-Source amplifier */
    TEST("amplifier_analyze_cs");
    amplifier_result_t res;
    int ret = amplifier_analyze_cs(&spec, &res);
    assert(ret == 0);
    assert(fabs(res.Av) > 1.0);  /* CS has gain */
    assert(res.Av < 0.0);         /* Inverting */
    assert(res.Rout > 0.0);
    assert(res.Rin > 1e6);        /* High input impedance */
    PASS();

    /* L2: Common-Gate amplifier */
    TEST("amplifier_analyze_cg");
    ret = amplifier_analyze_cg(&spec, &res);
    assert(ret == 0);
    assert(res.Av > 0.0);         /* Non-inverting */
    PASS();

    /* L2: Source Follower */
    TEST("amplifier_analyze_cd");
    spec.Rs = 10e3;
    ret = amplifier_analyze_cd(&spec, &res);
    assert(ret == 0);
    assert(res.Av > 0.0 && res.Av <= 1.0); /* Av ~ 1 */
    PASS();

    /* L2: Cascode amplifier */
    TEST("amplifier_analyze_cascode");
    ret = amplifier_analyze_cascode(&spec, &res);
    assert(ret == 0);
    assert(fabs(res.Av) > 10.0);  /* Cascode has high gain */
    PASS();

    /* L2: Common-Emitter (BJT) */
    TEST("amplifier_analyze_ce");
    ret = amplifier_analyze_ce(&spec, &res);
    assert(ret == 0);
    assert(fabs(res.Av) > 1.0);
    PASS();

    /* L2: Common-Base */
    TEST("amplifier_analyze_cb");
    ret = amplifier_analyze_cb(&spec, &res);
    assert(ret == 0);
    PASS();

    /* L2: Common-Collector */
    TEST("amplifier_analyze_cc");
    ret = amplifier_analyze_cc(&spec, &res);
    assert(ret == 0);
    PASS();

    /* L6: Differential Pair */
    TEST("diff_pair_dc_solve");
    diff_pair_spec_t dp;
    memset(&dp, 0, sizeof(dp));
    dp.input_type = DIFF_INPUT_NMOS; dp.load_type = DIFF_LOAD_RESISTOR;
    dp.I_tail = 100e-6; dp.R_load = 10e3;
    dp.W_input = 10e-6; dp.L_input = 1e-6;
    dp.Vdd = 1.8; dp.Vcm = 0.9; dp.Cload = 1e-12; dp.tech = tech; dp.T = 300.0;
    diff_pair_result_t dp_res;
    ret = diff_pair_dc_solve(&dp, &dp_res);
    assert(ret == 0);
    assert(dp_res.I_d1 > 0.0);
    assert(dp_res.I_d2 > 0.0);
    PASS();

    TEST("diff_pair_analyze");
    ret = diff_pair_analyze(&dp, &dp_res);
    assert(ret == 0);
    assert(dp_res.Ad > 1.0);
    assert(dp_res.CMRR > 0.0);
    PASS();

    /* L6: Two-Stage Miller Op-Amp Design */
    TEST("opamp_design_two_stage");
    opamp_spec_t ospec;
    memset(&ospec, 0, sizeof(ospec));
    ospec.topology = OPAMP_TOPOLOGY_TWO_STAGE;
    ospec.Av_dc_target = 80.0; ospec.GBW_target = 10e6;
    ospec.PM_target = 60.0; ospec.SR_target = 10.0;
    ospec.Cload = 5e-12; ospec.Vdd = 1.8;
    ospec.tech = tech; ospec.T = 300.0;
    opamp_result_t ores;
    ret = opamp_design_two_stage(&ospec, &ores);
    assert(ret == 0);
    assert(ores.Av_dc_dB > 40.0);  /* Reasonable DC gain */
    assert(ores.GBW > 1e6);        /* MHz-range GBW */
    assert(ores.phase_margin > 0.0);
    assert(ores.power > 0.0);
    PASS();

    /* L6: Folded Cascode */
    TEST("opamp_design_folded_cascode");
    ret = opamp_design_folded_cascode(&ospec, &ores);
    assert(ret == 0);
    assert(ores.Av_dc_dB > 30.0);
    PASS();

    /* L6: Telescopic */
    TEST("opamp_design_telescopic");
    ret = opamp_design_telescopic(&ospec, &ores);
    assert(ret == 0);
    assert(ores.Av_dc_dB > 30.0);
    PASS();

    /* L4: GBW theorem */
    TEST("opamp_GBW (gm1/(2*pi*Cc) > 0)");
    double gbw = opamp_GBW(1e-3, 1e-12);
    assert(gbw > 0.0);
    PASS();

    /* L4: Slew rate */
    TEST("opamp_slew_rate > 0");
    double sr = opamp_slew_rate(100e-6, 1e-12);
    assert(sr > 0.0);
    PASS();

    /* L4: Phase margin */
    TEST("opamp_phase_margin > 0");
    double pm = opamp_phase_margin(10e6, 1e3, 100e6, 200e6);
    assert(pm > 0.0 && pm < 180.0);
    PASS();

    /* L3: Miller pole split */
    TEST("miller_pole_split");
    two_stage_params_t tsp;
    tsp.gm1 = 100e-6; tsp.gm2 = 500e-6;
    tsp.ro1 = 1e6; tsp.ro2 = 100e3;
    tsp.Cc = 1e-12; tsp.C1 = 0.1e-12; tsp.C2 = 5e-12;
    double p1, p2, z1;
    ret = miller_pole_split(&tsp, &p1, &p2, &z1);
    assert(ret == 0);
    assert(p1 < 0.0); /* Dominant pole should be negative */
    assert(z1 > 0.0); /* RHP zero */
    PASS();

    /* L2: Gain error */
    TEST("opamp_gain_error < 1");
    double err = opamp_gain_error(1000.0);
    assert(err < 1.0);
    PASS();

    /* L2: Figure of merit */
    TEST("opamp_figure_of_merit");
    opamp_result_t fm_res;
    memset(&fm_res, 0, sizeof(fm_res));
    fm_res.GBW = 10e6; fm_res.Cc = 1e-12; fm_res.power = 1e-3;
    double fom = opamp_figure_of_merit(&fm_res);
    assert(fom > 0.0);
    PASS();

    /* L5: Pole-zero placement */
    TEST("opamp_pole_zero_placement");
    double Cc_out, gm2_out;
    ret = opamp_pole_zero_placement(10e6, 60.0, 5e-12, &Cc_out, &gm2_out);
    assert(ret == 0);
    assert(Cc_out > 0.0);
    assert(gm2_out > 0.0);
    PASS();

    /* L5: ICMR */
    TEST("opamp_icmr_calc");
    double vmin, vmax;
    memset(&ospec, 0, sizeof(ospec));
    ospec.Vdd = 1.8; ospec.tech = tech;
    ret = opamp_icmr_calc(&ospec, &vmin, &vmax);
    assert(ret == 0);
    assert(vmin < vmax);
    PASS();

    /* L7: CMFB */
    TEST("opamp_cmfb_analyze");
    cmfb_result_t cmfb;
    ret = opamp_cmfb_analyze(1e-3, 1e6, 2e-3, 1e6, 0.9, 0.89, &cmfb);
    assert(ret == 0);
    assert(cmfb.loop_gain > 0.0);
    PASS();

    ic_tech_free(tech);

    printf("\n=== Results: %d/%d tests passed ===\n", pass_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}
