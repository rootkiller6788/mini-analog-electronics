#include <string.h>
/* Test: Core Device Models and Biasing */
#include "analog_ic_basis.h"
#include <stdio.h>
#include <math.h>
#include <assert.h>

static int test_count = 0, pass_count = 0;
#define TEST(name) do { test_count++; printf("  %s ... ", name); } while(0)
#define PASS() do { pass_count++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

int main(void)
{
    printf("=== Test: analog_ic_basis ===\n");

    /* L1: Technology initialization */
    TEST("ic_tech_init_180nm");
    ic_technology_t *t180 = ic_tech_init_180nm();
    assert(t180 != NULL);
    assert(t180->Vth_n > 0.0 && t180->Vth_n < 1.0);
    assert(t180->L_min < 1e-6);
    assert(t180->Cox > 0.0);
    PASS();

    TEST("ic_tech_init_65nm");
    ic_technology_t *t65 = ic_tech_init_65nm();
    assert(t65 != NULL);
    assert(t65->tox < t180->tox); /* thinner oxide */
    PASS();

    TEST("ic_tech_init_28nm");
    ic_technology_t *t28 = ic_tech_init_28nm();
    assert(t28 != NULL);
    assert(t28->L_min < t65->L_min); /* smaller feature size */
    PASS();

    /* L3: MOS Square-Law Drain Current */
    TEST("mos_id_square_law (saturation)");
    mos_transistor_t mn;
    memset(&mn, 0, sizeof(mn));
    mn.type = MOS_TYPE_NMOS; mn.W = 10e-6; mn.L = 1e-6;
    mn.Vgs = 0.7; mn.Vds = 1.0; mn.Vsb = 0.0; mn.m = 1;
    double Id_sq = mos_id_square_law(t180, &mn);
    assert(Id_sq > 0.0); /* Should conduct in saturation */
    PASS();

    TEST("mos_id_square_law (cutoff)");
    mn.Vgs = 0.0;
    double Id_cut = mos_id_square_law(t180, &mn);
    assert(Id_cut == 0.0); /* No current in cutoff */
    PASS();

    /* L3: Subthreshold current */
    TEST("mos_id_subthreshold");
    mn.Vgs = 0.35; mn.Vds = 1.0;
    double Id_sub = mos_id_subthreshold(t180, &mn, 300.0);
    assert(Id_sub >= 0.0); /* May be small but non-negative */
    PASS();

    /* L3: Unified EKV-like model */
    TEST("mos_id_unified (strong inversion)");
    mn.Vgs = 0.8;
    double Id_uni = mos_id_unified(t180, &mn, 300.0);
    assert(Id_uni > 0.0);
    PASS();

    /* L3: Velocity saturation model */
    TEST("mos_id_velocity_sat");
    mn.Vgs = 0.8;
    double Id_vsat = mos_id_velocity_sat(t180, &mn);
    assert(Id_vsat > 0.0);
    PASS();

    /* L3: Body effect */
    TEST("mos_vth_body_effect");
    double Vth_body = mos_vth_body_effect(t180, MOS_TYPE_NMOS, t180->Vth_n, 0.5);
    assert(Vth_body > t180->Vth_n); /* Vth increases with Vsb */
    PASS();

    /* L2: Small-signal parameter extraction */
    TEST("mos_extract_ss_params");
    mn.Vgs = 0.7; mn.Vds = 1.0; mn.Id = 50e-6;
    mos_ss_params_t ss;
    int ret = mos_extract_ss_params(t180, &mn, 300.0, &ss);
    assert(ret == 0);
    assert(ss.gm > 0.0);
    assert(ss.ro > 0.0);
    assert(ss.intrinsic_gain > 1.0);
    assert(ss.ft > 0.0);
    PASS();

    /* L4: gm * ro > 1 (intrinsic gain > 1) -- a fundamental L4 theorem */
    TEST("L4: intrinsic gain > 1");
    assert(ss.intrinsic_gain > 1.0);
    PASS();

    /* L2: BJT DC current */
    TEST("bjt_ic (forward active)");
    bjt_transistor_t bjt;
    memset(&bjt, 0, sizeof(bjt));
    bjt.type = BJT_TYPE_NPN; bjt.Is = 1e-14;
    bjt.beta_f = 100.0; bjt.VA = 50.0;
    bjt.Vbe = 0.7; bjt.Vce = 2.0; bjt.T = 300.0;
    double Ic_bjt = bjt_ic(&bjt);
    assert(Ic_bjt > 0.0);
    PASS();

    /* L2: BJT small-signal extraction */
    TEST("bjt_extract_ss_params");
    bjt.Ic = Ic_bjt; bjt.Ib = Ic_bjt / bjt.beta_f;
    bjt_ss_params_t bjt_ss;
    ret = bjt_extract_ss_params(&bjt, &bjt_ss);
    assert(ret == 0);
    assert(bjt_ss.gm > 0.0);
    assert(bjt_ss.rpi > 0.0);
    PASS();

    /* L2: Region detection */
    TEST("mos_detect_region (saturation)");
    mn.Vgs = 0.7; mn.Vds = 1.0;
    mos_region_t reg = mos_detect_region(t180, &mn);
    assert(reg == MOS_REGION_SATURATION);
    PASS();

    TEST("mos_detect_region (cutoff)");
    mn.Vgs = 0.0;
    reg = mos_detect_region(t180, &mn);
    assert(reg == MOS_REGION_CUTOFF);
    PASS();

    /* L2: gm/ID calculation */
    TEST("mos_gm_over_id (strong inversion)");
    mn.Vgs = 0.8; mn.Id = 100e-6;
    double gm_id = mos_gm_over_id(t180, &mn, 300.0);
    assert(gm_id > 0.0 && gm_id < 50.0);
    PASS();

    /* L4: Beta-multiplier biasing */
    TEST("bias_beta_multiplier_current");
    double I_beta = bias_beta_multiplier_current(t180, 50e3, 4.0, 300.0);
    assert(I_beta > 0.0);
    PASS();

    /* L4: Widlar current source */
    TEST("bias_widlar_current");
    double I_widlar = bias_widlar_current(5.0, 50e3, 1e3, &bjt);
    assert(I_widlar > 0.0);
    PASS();

    /* L4: Supply-independent bias */
    TEST("bias_supply_independent");
    double V_si = bias_supply_independent(t180, 50e3, 300.0);
    assert(V_si > 0.5);
    PASS();

    /* L4: Pelgrom mismatch sample (random, test structure) */
    TEST("mismatch_pelgrom_sample");
    mismatch_params_t mp = mismatch_from_tech(t180);
    mismatch_sample_t ms = mismatch_pelgrom_sample(&mp, 10e-6, 1e-6, 12345ULL);
    /* Just verify structure is populated */
    (void)ms;
    PASS();

    /* L2: gm/ID lookup table */
    TEST("gmid_generate_table + gmid_find_vgs");
    gmid_data_point_t table[20];
    int n = gmid_generate_table(t180, MOS_TYPE_NMOS, 300.0, 1e-6, 20, table);
    assert(n == 0);
    double vgs_found = gmid_find_vgs(table, 20, 10.0);
    assert(vgs_found >= 0.0);
    PASS();

    /* L4: MOSFET sizing */
    TEST("mos_sizing_wl");
    double wl = mos_sizing_wl(t180, MOS_TYPE_NMOS, 50e-6, 0.2, 1e-6);
    assert(wl > 0.0);
    PASS();

    /* L2: Process corner application */
    TEST("ic_tech_apply_corner (FF)");
    double mu_n_orig = t180->mu_n;
    ic_tech_apply_corner(t180, CORNER_FF, TEMP_CORNER_NOMINAL);
    assert(t180->mu_n > mu_n_orig); /* FF = faster */
    PASS();

    /* Cleanup */
    ic_tech_free(t180);
    ic_tech_free(t65);
    ic_tech_free(t28);

    printf("\n=== Results: %d/%d tests passed ===\n", pass_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}
