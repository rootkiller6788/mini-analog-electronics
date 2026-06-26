#include <string.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
/* Test: Noise Analysis */
#include "analog_ic_noise.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static int test_count = 0, pass_count = 0;
#define TEST(n) do { test_count++; printf("  %s ... ", n); } while(0)
#define PASS() do { pass_count++; printf("PASS\n"); } while(0)

int main(void)
{
    printf("=== Test: analog_ic_noise ===\n");

    /* L2: Thermal noise */
    TEST("noise_thermal_resistor > 0");
    resistor_noise_params_t rp = {1000.0, 300.0};
    double Svv_r = noise_thermal_resistor(&rp);
    assert(Svv_r > 0.0); /* Non-zero at finite T */
    PASS();

    TEST("noise_thermal_resistor = 0 at T=0");
    rp.T = 0.0;
    double Svv_r0 = noise_thermal_resistor(&rp);
    assert(Svv_r0 == 0.0);
    PASS();

    /* L2: MOS thermal noise */
    TEST("noise_thermal_mos_drain > 0");
    mos_thermal_noise_params_t mp = {1e-3, 1e-6, 0.667, 300.0};
    double Sid = noise_thermal_mos_drain(&mp);
    assert(Sid > 0.0);
    PASS();

    /* L2: Flicker noise */
    TEST("noise_flicker_mos > 0 at low freq");
    mos_flicker_noise_params_t fp;
    memset(&fp, 0, sizeof(fp));
    fp.gm = 1e-3; fp.Cox = 8.42e-3; fp.W = 10e-6; fp.L = 1e-6;
    fp.Kf = 1e-25; fp.Af = 1.0; fp.Ef = 1.0; fp.f = 1e3;
    double Svv_f = noise_flicker_mos(&fp);
    assert(Svv_f > 0.0);
    PASS();

    /* L2: Shot noise */
    TEST("noise_shot_diode > 0");
    double Sid_shot = noise_shot_diode(1e-3, 300.0);
    assert(Sid_shot > 0.0);
    PASS();

    /* L3: Noise bandwidth */
    TEST("noise_equivalent_noise_bandwidth");
    double enbw = noise_equivalent_noise_bandwidth(1000.0, 1);
    assert(enbw > 0.0);
    PASS();

    /* L4: Input-referred noise (CS) */
    TEST("noise_input_referred_cs");
    noise_result_t nr;
    int ret = noise_input_referred_cs(MOS_TYPE_NMOS, 1e-3, 0.0, 100.0,
                                       NULL, 50e-6, 10e-6, 1e-6, 1e3, 300.0, &nr);
    assert(ret == 0);
    assert(nr.Sv_in > 0.0);
    PASS();

    /* L4: Friis formula */
    TEST("noise_friis_factor");
    double f_total = noise_friis_factor(2.0, 3.0, 4.0, 10.0, 8.0);
    assert(f_total > 2.0); /* Total > first stage */
    PASS();

    /* L4: Noise figure */
    TEST("noise_figure_from_factor");
    double nf = noise_figure_from_factor(2.0);
    assert(nf > 0.0);
    PASS();

    /* L2: kT/C noise */
    TEST("noise_ktc > 0");
    double ktc = noise_ktc(1e-12, 300.0);
    assert(ktc > 0.0);
    PASS();

    /* L3: Noise correlation */
    TEST("noise_correlation_diff");
    double svv_d, svv_c;
    ret = noise_correlation_diff(1e-12, 1e-12, 0.5, &svv_d, &svv_c);
    assert(ret == 0);
    assert(svv_c > svv_d); /* Common-mode > diff-mode for positive rho */
    PASS();

    /* L5: Noise matching */
    TEST("noise_match_optimal");
    double Gopt, Bopt, NFmin;
    ret = noise_match_optimal(50.0, 0.02, 0.0, &Gopt, &Bopt, &NFmin);
    assert(ret == 0);
    PASS();

    /* L7: LNA noise */
    TEST("noise_lna_cs_inductive_degen");
    noise_result_t lna_nr;
    ret = noise_lna_cs_inductive_degen(50e-3, 0.5e-9, 5e-9, 50.0,
                                         2.0*M_PI*2.4e9, 0.667, 1.33, 0.395, 0.85, 300.0, &lna_nr);
    assert(ret == 0);
    assert(lna_nr.noise_figure > 0.0);
    PASS();

    /* L2: SNR */
    TEST("noise_snr_from_result");
    nr.integrated_noise = 10e-6; nr.Svv_total = 1e-16;
    (void)noise_snr_from_result(&nr, 1.0, 1.0, 100e3);
    PASS();

    printf("\n=== Results: %d/%d tests passed ===\n", pass_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}
