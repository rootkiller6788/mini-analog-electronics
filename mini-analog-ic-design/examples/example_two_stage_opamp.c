/* Example: Two-Stage Miller Op-Amp Design for 180nm CMOS
 * Demonstrates a complete design flow from specs to device sizes.
 * Reference: Razavi Ch.10.2
 */
#include "analog_ic_amplifier.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    printf("=== Two-Stage Miller Op-Amp Design (180nm CMOS) ===\n\n");

    ic_technology_t *tech = ic_tech_init_180nm();
    if (!tech) { printf("ERROR: tech init failed\n"); return 1; }

    printf("Technology: 180nm CMOS\n");
    printf("  Vth_n = %.2f V, Vth_p = %.2f V\n", tech->Vth_n, tech->Vth_p);
    printf("  mu_n = %.0f cm2/Vs, mu_p = %.0f cm2/Vs\n", tech->mu_n, tech->mu_p);
    printf("  Cox = %.3f fF/um2\n\n", tech->Cox * 1e15);

    opamp_spec_t spec;
    memset(&spec, 0, sizeof(spec));
    spec.topology = OPAMP_TOPOLOGY_TWO_STAGE;
    spec.Av_dc_target = 80.0;
    spec.GBW_target = 10e6;
    spec.PM_target = 60.0;
    spec.SR_target = 20.0;
    spec.Cload = 5e-12;
    spec.Vdd = 1.8;
    spec.ICMR_min = 0.5;
    spec.ICMR_max = 1.3;
    spec.tech = tech;
    spec.T = 300.0;

    printf("Design Specifications:\n");
    printf("  DC Gain target: %.0f dB\n", spec.Av_dc_target);
    printf("  GBW target: %.1f MHz\n", spec.GBW_target / 1e6);
    printf("  Phase Margin target: %.0f deg\n", spec.PM_target);
    printf("  Slew Rate target: %.0f V/us\n", spec.SR_target);
    printf("  Load Cap: %.1f pF\n", spec.Cload / 1e-12);
    printf("  Supply: %.1f V\n\n", spec.Vdd);

    opamp_result_t res;
    int ret = opamp_design_two_stage(&spec, &res);
    if (ret != 0) { printf("ERROR: design failed\n"); ic_tech_free(tech); return 1; }

    printf("=== Design Results ===\n");
    printf("  DC Gain: %.1f dB\n", res.Av_dc_dB);
    printf("  GBW: %.2f MHz\n", res.GBW / 1e6);
    printf("  Phase Margin: %.1f deg\n", res.phase_margin);
    printf("  Slew Rate: %.1f V/us\n", res.SR);
    printf("  Power: %.1f uW\n", res.power * 1e6);
    printf("  CMRR: %.1f dB\n", res.CMRR);
    printf("  PSRR+: %.1f dB\n", res.PSRR_pos);
    printf("  PSRR-: %.1f dB\n", res.PSRR_neg);
    printf("  ICMR: %.3f V to %.3f V\n", res.Vin_cm_min, res.Vin_cm_max);
    printf("  Output Swing: %.3f V to %.3f V\n", res.Vout_min, res.Vout_max);

    printf("\n  Device Dimensions:\n");
    printf("    M1,M2 (input): W=%.1f um, L=%.2f um\n", res.W1*1e6, res.L1*1e6);
    printf("    M3,M4 (load):  W=%.1f um, L=%.2f um\n", res.W3*1e6, res.L3*1e6);
    printf("    M5 (tail):     W=%.1f um, L=%.2f um\n", res.W5*1e6, res.L5*1e6);
    printf("    M6 (2nd stg):  W=%.1f um, L=%.2f um\n", res.W6*1e6, res.L6*1e6);
    printf("    M7 (2nd load): W=%.1f um, L=%.2f um\n", res.W7*1e6, res.L7*1e6);
    printf("    M8 (bias):     W=%.1f um, L=%.2f um\n", res.W8*1e6, res.L8*1e6);

    printf("\n  Compensation: Cc=%.2f pF, Rz=%.1f kOhm\n",
           res.Cc / 1e-12, res.Rz / 1e3);

    printf("  Dominant pole: %.1f Hz\n", res.f_dominant);
    printf("  Non-dominant pole: %.2f MHz\n", res.f_nd / 1e6);
    printf("  Input noise @1kHz: %.2f nV/rtHz\n", res.input_noise_1kHz * 1e9);

    double fom = opamp_figure_of_merit(&res);
    printf("\n  Figure of Merit: %.2f MHz*pF/mW\n", fom);

    printf("\n=== Design Complete ===\n");
    ic_tech_free(tech);
    return 0;
}
