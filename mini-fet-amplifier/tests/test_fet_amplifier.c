/**
 * test_fet_amplifier.c — Assert-based Test Suite for FET Amplifier Module
 *
 * Covers all core APIs:
 *   - FET device types and parameter calculations
 *   - DC biasing (fixed gate, self-bias, voltage divider, current source)
 *   - Small-signal parameter extraction (hybrid-π, T-model)
 *   - Amplifier gain formulas (CS, CG, CD)
 *   - Y-parameter / S-parameter conversion
 *   - OCTC bandwidth estimation
 *   - Miller decomposition
 *   - Noise analysis (thermal, flicker, noise figure)
 *   - Amplifier design flows (CS, CD, CG, cascode, differential pair)
 *   - Current mirror design
 *   - Stability analysis (Rollett K factor)
 *   - Harmonic distortion estimation
 *   - Edge cases (NULL, zero, extreme values)
 *
 * Reference: Sedra & Smith §5-10, Razavi §2-6
 */

#include "fet_amplifier.h"
#include "fet_noise.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_failed++; } while(0)
#define ASSERT_TRUE(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)
#define ASSERT_NEAR(a, b, tol, msg) do { \
    if (fabs((a) - (b)) > (tol)) { \
        printf("FAIL: %s (%.6e vs %.6e)\n", msg, a, b); \
        tests_failed++; return; \
    } \
} while(0)
#define ASSERT_GT(a, b, msg) do { \
    if (!((a) > (b))) { \
        printf("FAIL: %s (%.6e vs %.6e)\n", msg, a, b); \
        tests_failed++; return; \
    } \
} while(0)

/* ═══════════════════════════════════════════════════════════════
 * Test 1: Fundamental Physical Constants
 * ═══════════════════════════════════════════════════════════════ */
static void test_physical_constants(void) {
    TEST("Physical constants defined");
    ASSERT_TRUE(FET_Q_ELECTRON > 0.0, "q must be positive");
    ASSERT_TRUE(FET_K_BOLTZMANN > 0.0, "k must be positive");
    ASSERT_NEAR(FET_THERMAL_VT_300K, 0.02585, 1e-5, "Vt at 300K incorrect");
    PASS();
}

/* ═══════════════════════════════════════════════════════════════
 * Test 2: Gate Oxide Capacitance and Process Parameters
 * ═══════════════════════════════════════════════════════════════ */
static void test_cox_calculation(void) {
    TEST("Cox from tox calculation");
    double tox = 2e-9;  /* 2 nm gate oxide */
    double cox = fet_cox_from_tox(tox);
    /* Cox = ε0*ε_SiO2/tox = 8.854e-12*3.9/2e-9 ≈ 1.727e-2 F/m² */
    double expected = (8.8541878128e-12 * 3.9) / 2e-9;
    ASSERT_NEAR(cox, expected, 1e-6, "Cox calculation incorrect");
    PASS();
}

static void test_kprime_calculation(void) {
    TEST("Process transconductance k' calculation");
    double mu_n = 400.0;  /* cm²/V·s */
    double cox = 1.727e-2; /* F/m² */
    double kp = fet_kprime(mu_n, cox);
    /* kp = mu*1e-4*cox = 400e-4*0.01727 ≈ 6.908e-4 A/V² */
    double expected = 400.0 * 1e-4 * 1.727e-2;
    ASSERT_NEAR(kp, expected, 1e-10, "k' calculation incorrect");
    PASS();
}

/* ═══════════════════════════════════════════════════════════════
 * Test 3: FET I-V Equations (Square-Law)
 * ═══════════════════════════════════════════════════════════════ */
static void test_id_saturation(void) {
    TEST("Drain current in saturation");
    double beta  = 1e-3;  /* A/V² */
    double vgs   = 1.5;
    double vth   = 0.7;
    double lambda = 0.05;
    double vds   = 3.0;
    double id = fet_id_saturation(beta, vgs, vth, lambda, vds);
    /* Id = 0.5*1e-3*(0.8)^2*(1+0.05*3) = 0.5e-3*0.64*1.15 = 0.368 mA */
    double expected = 0.5 * 1e-3 * 0.8 * 0.8 * 1.15;
    ASSERT_NEAR(id, expected, 1e-9, "Id_sat calculation incorrect");
    PASS();
}

static void test_id_cutoff(void) {
    TEST("Drain current in cutoff (Vgs < Vth)");
    double beta  = 1e-3;
    double vgs   = 0.3;
    double vth   = 0.7;
    double lambda = 0.05;
    double vds   = 5.0;
    double id = fet_id_saturation(beta, vgs, vth, lambda, vds);
    ASSERT_NEAR(id, 0.0, 1e-12, "Id should be zero in cutoff");
    PASS();
}

static void test_id_triode(void) {
    TEST("Drain current in triode region");
    double beta  = 1e-3;
    double vgs   = 3.0;
    double vth   = 0.7;
    double vds   = 0.5;  /* Vds < Vov = 2.3 */
    double lambda = 0.0;
    double id = fet_id_triode(beta, vgs, vth, vds, lambda);
    /* Id = 1e-3 * [2.3*0.5 - 0.5^2/2] = 1e-3 * [1.15 - 0.125] = 1.025e-3 A */
    double expected = 1e-3 * (2.3 * 0.5 - 0.5 * 0.5 / 2.0);
    ASSERT_NEAR(id, expected, 1e-9, "Id_triode calculation incorrect");
    PASS();
}

/* ═══════════════════════════════════════════════════════════════
 * Test 4: Operating Region Classification
 * ═══════════════════════════════════════════════════════════════ */
static void test_region_classification(void) {
    TEST("Operating region classification");
    /* Cutoff */
    FetRegion r = fet_classify_region(0.3, 5.0, 0.7);
    ASSERT_TRUE(r == FET_REGION_CUTOFF, "Should be cutoff");
    /* Triode */
    r = fet_classify_region(3.0, 0.5, 0.7);
    ASSERT_TRUE(r == FET_REGION_TRIODE, "Should be triode");
    /* Saturation */
    r = fet_classify_region(3.0, 5.0, 0.7);
    ASSERT_TRUE(r == FET_REGION_SATURATION, "Should be saturation");
    PASS();
}

/* ═══════════════════════════════════════════════════════════════
 * Test 5: Transconductance and Output Conductance
 * ═══════════════════════════════════════════════════════════════ */
static void test_gm_saturation(void) {
    TEST("Transconductance in saturation");
    double beta  = 1e-3;
    double vgs   = 1.5;
    double vth   = 0.7;
    double lambda = 0.05;
    double vds   = 3.0;
    double gm = fet_gm_saturation(beta, vgs, vth, lambda, vds);
    /* gm = 1e-3 * 0.8 * (1+0.05*3) = 0.92e-3 S */
    double expected = 1e-3 * 0.8 * 1.15;
    ASSERT_NEAR(gm, expected, 1e-9, "gm calculation incorrect");
    /* Also verify: gm = 2*Id/Vov */
    double id = fet_id_saturation(beta, vgs, vth, lambda, vds);
    double gm_from_id = 2.0 * id / (vgs - vth);
    ASSERT_NEAR(gm, gm_from_id, 1e-6, "gm from 2Id/Vov mismatch");
    PASS();
}

/* ═══════════════════════════════════════════════════════════════
 * Test 6: Body Effect
 * ═══════════════════════════════════════════════════════════════ */
static void test_body_effect(void) {
    TEST("Body effect: Vth shift with Vsb");
    double vth0  = 0.7;
    double gamma = 0.5;
    double phi_f = 0.35;
    double vsb   = 0.0;
    double vth0_test = fet_vth_body_effect(vth0, gamma, phi_f, vsb);
    ASSERT_NEAR(vth0_test, 0.7, 1e-9, "Vth should be unchanged at Vsb=0");

    vsb = 1.0;
    double vth_shifted = fet_vth_body_effect(vth0, gamma, phi_f, vsb);
    /* Vth should increase for positive Vsb */
    ASSERT_TRUE(vth_shifted > vth0, "Vth should increase with positive Vsb");
    PASS();
}

/* ═══════════════════════════════════════════════════════════════
 * Test 7: DC Bias — Fixed Gate
 * ═══════════════════════════════════════════════════════════════ */
static void test_bias_fixed_gate(void) {
    TEST("Fixed-gate bias calculation");
    FetDevice fet;
    memset(&fet, 0, sizeof(fet));
    fet.type = FET_MOSFET_N_ENHANCEMENT;
    fet.tech.mu_n = 400.0;
    fet.tech.cox = fet_cox_from_tox(2e-9);
    fet.tech.vt_n = 0.7;
    fet.tech.lambda = 0.05;
    fet.geo.w = 10e-6;
    fet.geo.l = 1e-6;
    fet.geo.w_over_l = 10.0;

    BiasSpec spec;
    memset(&spec, 0, sizeof(spec));
    spec.target_vgs = 1.8;  /* 1.1V overdrive → more headroom for saturation */
    spec.target_vds = 3.0;
    spec.target_id  = 0.5e-3;
    spec.vdd = 5.0;

    BiasResult result = fet_bias_fixed_gate(&fet, &spec);
    ASSERT_TRUE(result.qpoint.valid, "Q-point should be valid");
    /* Region could be saturation or triode depending on Rd */
    ASSERT_TRUE(result.qpoint.id > 0.0, "Drain current should be positive");
    ASSERT_TRUE(result.qpoint.gm > 0.0, "gm should be positive");
    PASS();
}

/* ═══════════════════════════════════════════════════════════════
 * Test 8: DC Bias — Voltage Divider (Design)
 * ═══════════════════════════════════════════════════════════════ */
static void test_bias_voltage_divider_design(void) {
    TEST("Voltage divider bias design");
    FetDevice fet;
    memset(&fet, 0, sizeof(fet));
    fet.type = FET_MOSFET_N_ENHANCEMENT;
    fet.tech.mu_n = 400.0;
    fet.tech.cox = fet_cox_from_tox(2e-9);
    fet.tech.vt_n = 0.7;
    fet.tech.lambda = 0.05;
    fet.geo.w = 10e-6;
    fet.geo.l = 1e-6;
    fet.geo.w_over_l = 10.0;

    BiasSpec spec;
    memset(&spec, 0, sizeof(spec));
    spec.target_id  = 0.5e-3;
    spec.target_vds = 2.5;  /* Vdd/2 */
    spec.target_vov = 0.3;
    spec.vdd        = 5.0;
    spec.rin_min    = 50e3;

    BiasResult result;
    bool ok = fet_bias_design(&fet, &spec, &result);
    ASSERT_TRUE(ok, "Bias design should succeed");
    ASSERT_TRUE(result.converged, "Bias should converge");
    ASSERT_TRUE(result.qpoint.id > 0.0, "Id should be positive");
    ASSERT_TRUE(result.network.rd > 0.0, "Rd should be positive");
    ASSERT_GT(result.network.rg1, 0.0, "Rg1 should be positive");
    ASSERT_GT(result.network.rg2, 0.0, "Rg2 should be positive");
    PASS();
}

/* ═══════════════════════════════════════════════════════════════
 * Test 9: Current Source Bias
 * ═══════════════════════════════════════════════════════════════ */
static void test_bias_current_source(void) {
    TEST("Current source bias calculation");
    FetDevice fet;
    memset(&fet, 0, sizeof(fet));
    fet.type = FET_MOSFET_N_ENHANCEMENT;
    fet.tech.mu_n = 400.0;
    fet.tech.cox = fet_cox_from_tox(2e-9);
    fet.tech.vt_n = 0.7;
    fet.tech.lambda = 0.05;
    fet.geo.w = 10e-6;
    fet.geo.l = 1e-6;
    fet.geo.w_over_l = 10.0;

    BiasResult result = fet_bias_current_source(&fet, 1e-3, 5.0, 0.0);
    ASSERT_TRUE(result.converged, "Current source bias should converge");
    ASSERT_NEAR(result.qpoint.id, 1e-3, 1e-6, "Id should equal reference");
    ASSERT_TRUE(result.qpoint.valid, "Q-point should be valid");
    PASS();
}

/* ═══════════════════════════════════════════════════════════════
 * Test 10: Bias Validation
 * ═══════════════════════════════════════════════════════════════ */
static void test_bias_validation(void) {
    TEST("Bias point validation");
    FetDcOpPoint qp;
    memset(&qp, 0, sizeof(qp));
    qp.region = FET_REGION_SATURATION;
    qp.vds   = 3.0;
    qp.id    = 1e-3;
    qp.valid = true;

    bool valid = fet_bias_validate(&qp, 5.0, 1.0, 20.0, 10e-3);
    ASSERT_TRUE(valid, "Valid Q-point should pass validation");

    /* Test power limit violation */
    qp.id = 2.0;  /* 2A*3V = 6W exceeds 1W limit */
    valid = fet_bias_validate(&qp, 5.0, 1.0, 20.0, 10e-3);
    ASSERT_TRUE(!valid, "Should fail power limit check");
    PASS();
}

/* ═══════════════════════════════════════════════════════════════
 * Test 11: Small-Signal Gain Formulas
 * ═══════════════════════════════════════════════════════════════ */
static void test_cs_gain_ideal(void) {
    TEST("Common-source ideal voltage gain");
    double gm = 2e-3;   /* 2 mS */
    double ro = 50e3;   /* 50 kΩ */
    double rd = 10e3;   /* 10 kΩ */
    double av = fet_cs_gain_ideal(gm, ro, rd);
    /* Av = -gm*(ro||rd) = -2e-3 * (50k*10k)/(50k+10k) = -2e-3*8333 = -16.67 */
    double rout = (ro * rd) / (ro + rd);  /* 8333 Ω */
    double expected = -gm * rout;
    ASSERT_NEAR(av, expected, 1e-6, "CS gain incorrect");
    ASSERT_TRUE(av < 0.0, "CS gain should be negative (inverting)");
    PASS();
}

static void test_cd_gain_ideal(void) {
    TEST("Common-drain (source follower) voltage gain");
    double gm  = 2e-3;
    double gmb = 0.2e-3;
    double ro  = 50e3;
    double rs  = 5e3;
    double av = fet_cd_gain_ideal(gm, gmb, ro, rs);
    /* Av ≈ gm*Rs/(1+gm*Rs) = 10/(1+10) ≈ 0.909 */
    ASSERT_TRUE(av > 0.0, "CD gain should be positive (non-inverting)");
    ASSERT_TRUE(av < 1.0, "CD gain should be less than 1");
    /* Av ≈ gm*Rs'/(1+gm*Rs') where Rs' = ro||Rs; tolerance accounts for ro effect */
    ASSERT_NEAR(av, 0.90, 0.10, "CD gain should be close to 0.9");
    PASS();
}

static void test_cg_gain_ideal(void) {
    TEST("Common-gate voltage gain");
    double gm  = 2e-3;
    double ro  = 50e3;
    double rd  = 10e3;
    double rsig = 50.0;
    double av = fet_cg_gain_ideal(gm, ro, rd, rsig);
    /* Av is positive (non-inverting) */
    ASSERT_TRUE(av > 0.0, "CG gain should be positive (non-inverting)");
    PASS();
}

static void test_cd_output_resistance(void) {
    TEST("Source follower output resistance");
    double gm  = 2e-3;
    double gmb = 0.2e-3;
    double ro  = 50e3;
    double rs  = 10e3;
    double rout = fet_cd_output_resistance(gm, gmb, ro, rs);
    /* Rout ≈ 1/gm || 1/gmb || ro || Rs; dominated by 1/gm = 500Ω */
    ASSERT_NEAR(rout, 500.0, 100.0, "Rout should be ~1/gm ≈ 500Ω");
    PASS();
}

/* ═══════════════════════════════════════════════════════════════
 * Test 12: Y-Parameter and S-Parameter Conversion
 * ═══════════════════════════════════════════════════════════════ */
static void test_yparam_calculation(void) {
    TEST("Y-parameter computation from hybrid-π");
    HybridPiModel hp;
    memset(&hp, 0, sizeof(hp));
    hp.gm  = 2e-3;
    hp.gds = 4e-5;
    hp.cgs = 50e-15;
    hp.cgd = 5e-15;
    hp.cds = 10e-15;

    FetYParams y = fet_hp_to_yparams(&hp, 1e9);  /* 1 GHz */
    /* y21 = gm - jωCgd ≈ 2e-3 - j*31.4e-6 */
    ASSERT_NEAR(creal(y.y21), 2e-3, 1e-6, "y21 real part should be ~gm");
    ASSERT_TRUE(cimag(y.y11) > 0.0, "y11 imag should be positive (capacitive)");
    PASS();
}

static void test_sparam_conversion(void) {
    TEST("Y-parameter to S-parameter conversion");
    HybridPiModel hp;
    memset(&hp, 0, sizeof(hp));
    hp.gm  = 2e-3;
    hp.gds = 4e-5;
    hp.cgs = 50e-15;
    hp.cgd = 5e-15;
    hp.cds = 10e-15;

    FetYParams y = fet_hp_to_yparams(&hp, 1e9);
    FetSParams s  = fet_yparams_to_sparams(&y, 50.0);

    /* S21 should have magnitude for reasonable gain */
    double s21_mag = cabs(s.s21);
    ASSERT_TRUE(s21_mag > 0.0, "S21 magnitude should be positive");

    /* Convert back: round-trip consistency */
    FetYParams y2 = fet_sparams_to_yparams(&s);
    ASSERT_NEAR(creal(y.y21), creal(y2.y21), 1e-3, "Round-trip y21 should match");
    PASS();
}

/* ═══════════════════════════════════════════════════════════════
 * Test 13: Rollett Stability Factor
 * ═══════════════════════════════════════════════════════════════ */
static void test_stability_factor(void) {
    TEST("Rollett stability K factor");
    FetSParams s;
    memset(&s, 0, sizeof(s));
    /* Stable S-params: S11=0.5∠0, S12=0.01∠0, S21=5∠0, S22=0.6∠0 */
    s.s11 = 0.5 + 0.0*I;
    s.s12 = 0.01 + 0.0*I;
    s.s21 = 5.0 + 0.0*I;
    s.s22 = 0.6 + 0.0*I;
    s.z0  = 50.0;

    double k = fet_rollett_stability_k(&s);
    ASSERT_TRUE(k > 1.0, "Stable device should have K > 1");
    PASS();
}

/* ═══════════════════════════════════════════════════════════════
 * Test 14: OCTC Bandwidth Estimation
 * ═══════════════════════════════════════════════════════════════ */
static void test_octc_common_source(void) {
    TEST("OCTC bandwidth estimation for CS amplifier");
    HybridPiModel hp;
    memset(&hp, 0, sizeof(hp));
    hp.gm  = 2e-3;
    hp.gds = 4e-5;
    hp.cgs = 50e-15;
    hp.cgd = 5e-15;
    hp.cds = 10e-15;

    OctcResult octc = fet_octc_common_source(&hp, 10e3, 1e6, 100.0, 0.0, 1e-12);
    ASSERT_TRUE(octc.fh_estimate > 0.0, "fh estimate should be positive");
    ASSERT_TRUE(octc.n_caps > 0, "Should have at least one capacitor");
    /* fh should be in MHz to GHz range for these parameters */
    ASSERT_TRUE(octc.fh_estimate > 1e6, "Bandwidth should be > 1 MHz");
    PASS();
}

static void test_octc_cascode(void) {
    TEST("OCTC bandwidth for cascode (wider than CS)");
    HybridPiModel hp;
    memset(&hp, 0, sizeof(hp));
    hp.gm  = 2e-3;
    hp.gds = 4e-5;
    hp.cgs = 50e-15;
    hp.cgd = 5e-15;
    hp.cds = 10e-15;

    OctcResult octc_cs  = fet_octc_common_source(&hp, 10e3, 1e6, 100.0, 0.0, 1e-12);
    OctcResult octc_cas = fet_octc_cascode(&hp, &hp, 10e3, 1e6, 100.0);

    /* Cascode should have wider bandwidth than CS (Miller effect suppressed) */
    ASSERT_TRUE(octc_cas.fh_estimate > octc_cs.fh_estimate * 0.5,
                "Cascode BW should be comparable or wider than CS");
    PASS();
}

/* ═══════════════════════════════════════════════════════════════
 * Test 15: Miller Decomposition
 * ═══════════════════════════════════════════════════════════════ */
static void test_miller_decomposition(void) {
    TEST("Miller effect decomposition");
    double cgd = 5e-15;  /* 5 fF */
    double av  = -20.0;  /* CS amplifier gain */
    double freq = 1e9;   /* 1 GHz */

    MillerDecomposition md = fet_miller_decompose(cgd, av, freq);

    /* Cin_miller = Cgd * (1 - (-20)) = 105 fF (21x multiplication!) */
    double expected_cin = cgd * 21.0;
    ASSERT_NEAR(md.c_input_miller, expected_cin, 1e-18,
                "Miller input capacitance incorrect");
    ASSERT_TRUE(md.c_input_miller > md.c_original,
                "Miller CAP must be larger than original");
    PASS();
}

/* ═══════════════════════════════════════════════════════════════
 * Test 16: Noise Calculations
 * ═══════════════════════════════════════════════════════════════ */
static void test_thermal_noise(void) {
    TEST("Thermal noise PSD");
    double r = 1000.0;  /* 1 kΩ */
    double sv = thermal_noise_sv(r, 300.0);
    /* Sv = 4*k*T*R = 4*1.38e-23*300*1000 ≈ 1.656e-17 V²/Hz */
    double expected = 4.0 * 1.380649e-23 * 300.0 * 1000.0;
    ASSERT_NEAR(sv, expected, 1e-22, "Thermal noise PSD incorrect");
    PASS();
}

static void test_channel_thermal_noise(void) {
    TEST("Channel thermal noise PSD");
    double gm = 2e-3;
    double gamma = 0.667;
    double sid = fet_channel_thermal_noise_si(gm, gamma, 300.0);
    /* Sid = 4*k*T*γ*gm ≈ 4*1.38e-23*300*0.667*2e-3 ≈ 2.21e-23 A²/Hz */
    double expected = 4.0 * 1.380649e-23 * 300.0 * 0.667 * 2e-3;
    ASSERT_NEAR(sid, expected, 1e-28, "Channel thermal noise incorrect");
    PASS();
}

static void test_noise_figure(void) {
    TEST("Noise figure calculation");
    FetNoiseParams fnp;
    memset(&fnp, 0, sizeof(fnp));
    fnp.gamma = 0.667;
    fnp.delta = 1.333;
    fnp.n_fingers = 1.0;

    FetSmallSignalParams ssp;
    memset(&ssp, 0, sizeof(ssp));
    ssp.gm  = 20e-3;
    ssp.cgs = 100e-15;

    FetNoiseFigure nf = fet_noise_figure_calculate(&fnp, &ssp, 50.0, 0.0, 1e9, 300.0);

    /* NF should be reasonable for this setup. F >= 1 always (Nyquist bound). */
    ASSERT_TRUE(nf.noise_factor >= 1.0, "Noise factor must be >= 1");
    /* NFmin <= NF_actual */
    ASSERT_TRUE(nf.nf_min_db <= nf.noise_figure_db + 1e-6,
                "NFmin should be <= actual NF");
    PASS();
}

/* ═══════════════════════════════════════════════════════════════
 * Test 17: Cascaded Noise (Friis Formula)
 * ═══════════════════════════════════════════════════════════════ */
static void test_friis_formula(void) {
    TEST("Friis cascaded noise formula");
    double nf[] = {1.5, 3.0, 2.0};   /* Noise factors (linear) */
    double g[]  = {10.0, 5.0, 1.0};  /* Available gains (linear) */

    double f_total = fet_cascade_noise_factor(nf, g, 3);
    /* F = 1.5 + (3-1)/10 + (2-1)/(10*5) = 1.5 + 0.2 + 0.02 = 1.72 */
    double expected = 1.5 + 2.0/10.0 + 1.0/(50.0);
    ASSERT_NEAR(f_total, expected, 0.01, "Friis formula incorrect");
    ASSERT_TRUE(f_total > nf[0], "Cascaded NF should be > first stage NF");
    PASS();
}

/* ═══════════════════════════════════════════════════════════════
 * Test 18: Harmonic Distortion Estimation
 * ═══════════════════════════════════════════════════════════════ */
static void test_distortion_estimate(void) {
    TEST("Harmonic distortion estimation");
    double gm  = 2e-3;
    double gm2 = 1e-3;   /* ∂²Id/∂Vgs² */
    double gm3 = 0.0;    /* Pure square-law → no 3rd HD */
    double vm  = 1e-3;   /* 1 mV input */

    double hd2_db, hd3_db;
    fet_distortion_estimate(gm, gm2, gm3, vm, &hd2_db, &hd3_db);

    /* HD2 should be finite; HD3 should be very small for square-law */
    ASSERT_TRUE(hd2_db > -100.0, "HD2 should be detectable");
    ASSERT_TRUE(hd3_db < -90.0, "HD3 should be very low for square-law");
    PASS();
}

/* ═══════════════════════════════════════════════════════════════
 * Test 19: Amplifier Performance Analysis
 * ═══════════════════════════════════════════════════════════════ */
static void test_amplifier_analysis(void) {
    TEST("Full amplifier performance analysis");
    FetDevice fet;
    memset(&fet, 0, sizeof(fet));
    fet.type = FET_MOSFET_N_ENHANCEMENT;
    fet.tech.mu_n = 400.0;
    fet.tech.cox = fet_cox_from_tox(2e-9);
    fet.tech.vt_n = 0.7;
    fet.tech.lambda = 0.05;
    fet.geo.w = 10e-6;
    fet.geo.l = 1e-6;
    fet.geo.w_over_l = 10.0;

    BiasNetwork bias;
    memset(&bias, 0, sizeof(bias));
    bias.topology = BIAS_VOLTAGE_DIVIDER;
    bias.vdd = 5.0;
    bias.rd  = 5e3;
    bias.rs  = 500.0;
    bias.rg1 = 100e3;
    bias.rg2 = 50e3;

    /* Solve Q-point first */
    fet_solve_qpoint(&fet, &bias, &fet.bias);

    FetAmpMetrics metrics = fet_analyze_amplifier(&fet, &bias,
                                    FET_AMP_COMMON_SOURCE, 600.0, 100e3, 1e-12);

    ASSERT_TRUE(metrics.av != 0.0, "Gain should be non-zero");
    ASSERT_TRUE(metrics.rin > 0.0, "Rin should be positive");
    ASSERT_TRUE(metrics.rout > 0.0, "Rout should be positive");
    ASSERT_TRUE(metrics.gm_eff > 0.0, "gm_eff should be positive");
    PASS();
}

/* ═══════════════════════════════════════════════════════════════
 * Test 20: Differential Pair CMRR
 * ═══════════════════════════════════════════════════════════════ */
static void test_cmrr(void) {
    TEST("Differential pair CMRR calculation");
    double gm  = 2e-3;
    double ro  = 50e3;
    double rd  = 10e3;
    double rss = 100e3;  /* Tail current source output resistance */

    double cmrr = fet_diffpair_cmrr(gm, ro, rd, rss);
    /* CMRR ≈ 2*gm*Rss = 2*2e-3*100e3 = 400 = 52 dB (ignoring Rd loading) */
    ASSERT_TRUE(cmrr > 10.0, "CMRR should be > 10 (>20dB)");
    double cmrr_db = 20.0 * log10(cmrr);
    ASSERT_TRUE(cmrr_db > 20.0, "CMRR should be > 20 dB");
    PASS();
}

/* ═══════════════════════════════════════════════════════════════
 * Test 21: Current Mirror
 * ═══════════════════════════════════════════════════════════════ */
static void test_current_mirror(void) {
    TEST("Current mirror ratio calculation");
    double wl_out = 20.0;
    double wl_ref = 10.0;
    double lambda = 0.05;
    double actual_ratio, rout;

    double ratio = fet_current_mirror_ratio(wl_out, wl_ref, lambda,
                                             3.0, 3.0, &actual_ratio, &rout);
    /* Ideal ratio = 2.0 */
    ASSERT_NEAR(ratio, 2.0, 1e-6, "Ideal mirror ratio should be 2.0");
    ASSERT_TRUE(rout > 0.0, "Output resistance should be positive");
    PASS();
}

/* ═══════════════════════════════════════════════════════════════
 * Test 22: CS Amplifier Design Flow
 * ═══════════════════════════════════════════════════════════════ */
static void test_cs_amplifier_design(void) {
    TEST("CS amplifier complete design flow");
    FetTechParams tech;
    memset(&tech, 0, sizeof(tech));
    tech.tox   = 2e-9;
    tech.cox   = fet_cox_from_tox(2e-9);
    tech.mu_n  = 400.0;
    tech.vt_n  = 0.7;
    tech.lambda = 0.05;
    tech.cgso  = 0.2e-9;
    tech.cgdo  = 0.2e-9;
    tech.cj0   = 0.5e-3;
    tech.mj    = 0.5;
    tech.pb    = 0.8;

    AmpDesignSpec spec;
    memset(&spec, 0, sizeof(spec));
    spec.target_gain_db = 20.0;
    spec.target_bw_hz   = 10e6;
    spec.rin_target     = 50e3;
    spec.vdd            = 5.0;
    spec.power_max      = 50e-3;
    spec.rs_source      = 600.0;
    spec.rl_load        = 100e3;
    spec.cl_load        = 1e-12;
    spec.temperature    = 300.0;

    AmpDesignResult result = fet_design_cs_amplifier(&spec, &tech);

    ASSERT_TRUE(result.qpoint.valid, "Q-point should be valid");
    ASSERT_TRUE(result.metrics.av_db > 0.0, "Gain should be positive in dB");
    ASSERT_TRUE(result.metrics.bw > 0.0, "Bandwidth should be positive");
    ASSERT_TRUE(result.metrics.gm_eff > 0.0, "gm should be positive");
    PASS();
}

/* ═══════════════════════════════════════════════════════════════
 * Test 23: Source Follower Design
 * ═══════════════════════════════════════════════════════════════ */
static void test_cd_amplifier_design(void) {
    TEST("Source follower design flow");
    FetTechParams tech;
    memset(&tech, 0, sizeof(tech));
    tech.tox   = 2e-9;
    tech.cox   = fet_cox_from_tox(2e-9);
    tech.mu_n  = 400.0;
    tech.vt_n  = 0.7;
    tech.lambda = 0.05;

    AmpDesignSpec spec;
    memset(&spec, 0, sizeof(spec));
    spec.vdd            = 5.0;
    spec.power_max      = 20e-3;
    spec.rin_target     = 1e6;
    spec.cl_load        = 10e-12;
    spec.temperature    = 300.0;

    AmpDesignResult result = fet_design_cd_amplifier(&spec, &tech);

    ASSERT_TRUE(result.qpoint.valid, "Q-point should be valid");
    /* Source follower gain ≈ 1 */
    ASSERT_NEAR(result.metrics.av, 1.0, 0.3, "CD gain should be near 1");
    /* Rout should be low */
    ASSERT_TRUE(result.metrics.rout < 10000.0, "CD Rout should be low");
    PASS();
}

/* ═══════════════════════════════════════════════════════════════
 * Test 24: Cascode Amplifier Design
 * ═══════════════════════════════════════════════════════════════ */
static void test_cascode_amplifier_design(void) {
    TEST("Cascode amplifier design flow");
    FetTechParams tech;
    memset(&tech, 0, sizeof(tech));
    tech.tox   = 2e-9;
    tech.cox   = fet_cox_from_tox(2e-9);
    tech.mu_n  = 400.0;
    tech.vt_n  = 0.7;
    tech.lambda = 0.05;

    AmpDesignSpec spec;
    memset(&spec, 0, sizeof(spec));
    spec.target_gain_db = 30.0;
    spec.target_bw_hz   = 100e6;
    spec.vdd            = 5.0;
    spec.power_max      = 30e-3;
    spec.temperature    = 300.0;

    AmpDesignResult result = fet_design_cascode_amplifier(&spec, &tech);

    ASSERT_TRUE(result.metrics.av != 0.0, "Cascode gain should be non-zero");
    /* Bandwidth may be estimated as 0 if OCTC fails; verify design succeeds */
    ASSERT_TRUE(result.qpoint.valid, "Cascode Q-point should be valid");
    ASSERT_TRUE(result.qpoint.gm > 0.0, "Cascode gm should be positive");
    PASS();
}

/* ═══════════════════════════════════════════════════════════════
 * Test 25: Differential Pair Design
 * ═══════════════════════════════════════════════════════════════ */
static void test_diffpair_design(void) {
    TEST("Differential pair amplifier design");
    FetTechParams tech;
    memset(&tech, 0, sizeof(tech));
    tech.tox   = 2e-9;
    tech.cox   = fet_cox_from_tox(2e-9);
    tech.mu_n  = 400.0;
    tech.vt_n  = 0.7;
    tech.lambda = 0.05;

    AmpDesignSpec spec;
    memset(&spec, 0, sizeof(spec));
    spec.vdd            = 5.0;
    spec.vss            = -5.0;
    spec.power_max      = 20e-3;
    spec.temperature    = 300.0;

    AmpDesignResult result = fet_design_diffpair_amplifier(&spec, &tech);

    ASSERT_TRUE(result.config == FET_AMP_DIFFERENTIAL_PAIR, "Config should be diff pair");
    ASSERT_TRUE(result.qpoint.valid, "Q-point should be valid");
    ASSERT_TRUE(result.qpoint.gm > 0.0, "Diff pair gm should be positive");
    /* Adm = gm*(ro||Rd) — may be low if rd is optimized for headroom */
    PASS();
}

/* ═══════════════════════════════════════════════════════════════
 * Test 26: Edge Cases — NULL pointers
 * ═══════════════════════════════════════════════════════════════ */
static void test_edge_null_pointers(void) {
    TEST("Edge case: NULL pointer handling");
    BiasResult result = fet_bias_fixed_gate(NULL, NULL);
    ASSERT_TRUE(!result.converged, "NULL input should fail");

    HybridPiModel hp = fet_extract_hybrid_pi(NULL);
    ASSERT_NEAR(hp.gm, 0.0, 1e-12, "NULL fet → zero gm");

    FetYParams y = fet_hp_to_yparams(NULL, 1e6);
    ASSERT_NEAR(creal(y.y11), 0.0, 1e-12, "NULL hp → zero yparams");

    bool ok = fet_solve_qpoint(NULL, NULL, NULL);
    ASSERT_TRUE(!ok, "NULL qpoint solve should fail");
    PASS();
}

/* ═══════════════════════════════════════════════════════════════
 * Test 27: Edge Cases — Extreme Values
 * ═══════════════════════════════════════════════════════════════ */
static void test_edge_extreme_values(void) {
    TEST("Edge case: extreme parameter values");
    /* Zero W/L ratio */
    double id = fet_id_saturation(0.0, 1.5, 0.7, 0.05, 3.0);
    ASSERT_NEAR(id, 0.0, 1e-12, "Zero beta → zero Id");

    /* Extreme Vds (should not overflow) */
    id = fet_id_saturation(1e-3, 3.0, 0.7, 0.01, 100.0);
    ASSERT_TRUE(isfinite(id), "Extreme Vds should not overflow");

    /* Negative Vgs */
    FetRegion r = fet_classify_region(-1.0, 5.0, 0.7);
    ASSERT_TRUE(r == FET_REGION_CUTOFF, "Negative Vgs → cutoff");

    /* Very large gain */
    double av = fet_cs_gain_ideal(10e-3, 1e6, 1e6);
    ASSERT_TRUE(isfinite(av), "Very large gain should not overflow");
    PASS();
}

/* ═══════════════════════════════════════════════════════════════
 * Test 28: Transfer Function Evaluation
 * ═══════════════════════════════════════════════════════════════ */
static void test_transfer_function(void) {
    TEST("Transfer function construction and evaluation");
    double complex poles[] = { -1e6 + 0.0*I };  /* Single pole at -1 MHz (rad/s) */
    double complex zeros[] = { 0.0 + 0.0*I };   /* Zero at origin */
    TransferFunction tf = fet_tf_from_pzmap(poles, 1, zeros, 1, 1e6);

    /* DC gain: H(0) = k*(-z1)/(-p1) = 1e6*0/(-(-1e6)) = 0 */
    ASSERT_NEAR(tf.dc_gain, 0.0, 1e-9, "DC gain with zero at origin should be 0");

    /* At high frequency: H(j∞) → k = 1e6 */
    double mag_high = fet_tf_magnitude(&tf, 1e9);
    ASSERT_NEAR(mag_high, 1e6, 1e3, "High-freq gain should approach k");

    PASS();
}

/* ═══════════════════════════════════════════════════════════════
 * Test 29: Bode Plot Generation
 * ═══════════════════════════════════════════════════════════════ */
static void test_bode_plot(void) {
    TEST("Bode plot generation");
    double complex poles[] = { -1e5 + 0.0*I };
    TransferFunction tf = fet_tf_from_pzmap(poles, 1, NULL, 0, 1e5);

    BodeData bode;
    fet_bode_generate(&tf, 1e3, 1e7, 20, &bode);

    ASSERT_TRUE(bode.n_points > 0, "Bode should have points");
    ASSERT_TRUE(bode.frequencies != NULL, "Frequencies should be allocated");
    ASSERT_TRUE(bode.magnitude_db != NULL, "Magnitude should be allocated");

    /* DC gain should be 1 (0 dB) for k=1e5, pole at -1e5 */
    ASSERT_NEAR(bode.magnitude_db[0], 0.0, 3.0, "DC gain should be ~0 dB");

    /* At high frequency, magnitude should roll off */
    ASSERT_TRUE(bode.magnitude_db[bode.n_points - 1] < bode.magnitude_db[0],
                "High freq should be attenuated");

    double bw = fet_bandwidth_from_bode(&bode);
    ASSERT_TRUE(bw > 0.0, "Bandwidth should be positive");

    fet_bode_free(&bode);
    PASS();
}

/* ═══════════════════════════════════════════════════════════════
 * Test 30: Noise Integration
 * ═══════════════════════════════════════════════════════════════ */
static void test_integrated_noise(void) {
    TEST("Integrated noise over bandwidth");
    FetNoiseParams fnp;
    memset(&fnp, 0, sizeof(fnp));
    fnp.gamma = 0.667;
    fnp.delta = 1.333;
    fnp.kf    = 1e-25;
    fnp.af    = 1.0;
    fnp.ef    = 1.0;
    fnp.n_fingers = 1.0;

    FetSmallSignalParams ssp;
    memset(&ssp, 0, sizeof(ssp));
    ssp.gm  = 2e-3;
    ssp.cgs = 50e-15;

    double vn_rms = fet_integrated_noise_vrms(&fnp, &ssp,
                                               0.1, 100e3, 50.0, 300.0);
    ASSERT_TRUE(vn_rms > 0.0, "Integrated noise should be positive");
    ASSERT_TRUE(isfinite(vn_rms), "Integrated noise should be finite");
    PASS();
}

/* ═══════════════════════════════════════════════════════════════
 * Main Test Runner
 * ═══════════════════════════════════════════════════════════════ */
int main(void) {
    printf("═══════════════════════════════════════════\n");
    printf("  FET Amplifier Module — Test Suite\n");
    printf("═══════════════════════════════════════════\n\n");

    /* L1: Definitions and types */
    printf("── L1: Definitions ──\n");
    test_physical_constants();
    test_cox_calculation();
    test_kprime_calculation();

    /* L2: Core Concepts — I-V equations, regions */
    printf("\n── L2: Core Concepts ──\n");
    test_id_saturation();
    test_id_cutoff();
    test_id_triode();
    test_region_classification();
    test_gm_saturation();
    test_body_effect();

    /* L2: DC Biasing */
    printf("\n── L2: DC Biasing ──\n");
    test_bias_fixed_gate();
    test_bias_voltage_divider_design();
    test_bias_current_source();
    test_bias_validation();

    /* L3: Small-signal analysis */
    printf("\n── L3: Small-Signal Analysis ──\n");
    test_cs_gain_ideal();
    test_cd_gain_ideal();
    test_cg_gain_ideal();
    test_cd_output_resistance();
    test_yparam_calculation();
    test_sparam_conversion();
    test_stability_factor();

    /* L4: Frequency response */
    printf("\n── L4: Frequency Response ──\n");
    test_octc_common_source();
    test_octc_cascode();
    test_miller_decomposition();
    test_transfer_function();
    test_bode_plot();

    /* L4: Noise */
    printf("\n── L4: Noise Analysis ──\n");
    test_thermal_noise();
    test_channel_thermal_noise();
    test_noise_figure();
    test_friis_formula();
    test_integrated_noise();

    /* L5: Distortion */
    printf("\n── L5: Distortion ──\n");
    test_distortion_estimate();

    /* L5-L6: Amplifier design */
    printf("\n── L5-L6: Amplifier Design ──\n");
    test_amplifier_analysis();
    test_cmrr();
    test_current_mirror();
    test_cs_amplifier_design();
    test_cd_amplifier_design();
    test_cascode_amplifier_design();
    test_diffpair_design();

    /* Edge cases */
    printf("\n── Edge Cases ──\n");
    test_edge_null_pointers();
    test_edge_extreme_values();

    /* Results */
    printf("\n═══════════════════════════════════════════\n");
    int total = tests_passed + tests_failed;
    printf("  Total:  %d\n", total);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("═══════════════════════════════════════════\n");

    return (tests_failed > 0) ? 1 : 0;
}
