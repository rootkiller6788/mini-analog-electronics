/**
 * example_cascode.c ? Cascode Amplifier Example
 *
 * The cascode (CE + CB) eliminates Miller effect, dramatically improving
 * bandwidth while maintaining CE-level voltage gain. This example
 * demonstrates the bandwidth advantage compared to a standard CE stage.
 *
 * Application: RF/IF amplifier in communication receivers ? high gain
 *              with wide bandwidth (AM radio IF: 455 kHz, FM IF: 10.7 MHz).
 *
 * Reference: Sedra & Smith, Sec. 8.6; Gray & Meyer, Ch. 6
 * Course: MIT 6.002, Berkeley EE140, Stanford EE214
 */

#include <stdio.h>
#include <math.h>
#include "../include/bjt_model.h"
#include "../include/bjt_dc_bias.h"
#include "../include/bjt_small_signal.h"
#include "../include/bjt_frequency.h"
#include "../include/bjt_amplifier.h"

int main(void)
{
    bjt_spice_params_t dev;
    bjt_cascode_t cascode;
    bjt_bias_network_t net;

    /* For comparison: standard CE with same bias */
    bjt_qpoint_t qp_ce;
    bjt_ss_params_t ss_ce;
    bjt_amp_metrics_t met_ce;

    printf("\n========================================\n");
    printf("  BJT Cascode Amplifier (CE + CB)\n");
    printf("  Wideband RF/IF Amplifier Example\n");
    printf("========================================\n\n");

    bjt_spice_default_npn(&dev);

    /* Design parameters */
    double VCC       = 15.0;
    double RL        = 10000.0;
    double Rs        = 50.0;
    double IC_target = 1e-3;   /* 1 mA per transistor */
    double RC        = 4700.0;
    double RE        = 1000.0;
    double R1        = 100000.0;
    double R2        = 22000.0;

    printf("[1] Design Parameters:\n");
    printf("    VCC = %.0f V, IC = %.1f mA per transistor\n",
           VCC, IC_target * 1000.0);
    printf("    RC = %.1f k?, RL = %.0f k?, Rs = %.0f ?\n\n",
           RC/1000.0, RL/1000.0, Rs);

    /* Analyze cascode */
    printf("[2] Cascode Analysis:\n");
    bjt_cascode_analyze(&dev, IC_target, VCC, RC, RL, Rs,
                         R1, R2, RE, 300.0, &cascode);

    printf("    CE stage (Q1):\n");
    printf("      IC = %.3f mA, VCE = %.2f V\n",
           cascode.qp1.IC * 1000.0, cascode.qp1.VCE);
    printf("      gm = %.2f mS, rpi = %.1f k?\n",
           cascode.ss1.gm * 1000.0, cascode.ss1.rpi / 1000.0);

    printf("    CB stage (Q2):\n");
    printf("      IC = %.3f mA, VCE = %.2f V\n",
           cascode.qp2.IC * 1000.0, cascode.qp2.VCE);
    printf("      gm = %.2f mS\n", cascode.ss2.gm * 1000.0);

    /* Cascode performance */
    printf("\n[3] Cascode Performance:\n");
    printf("    Av  = %.1f V/V  (%.1f dB)\n",
           cascode.metrics.Av, cascode.metrics.Av_dB);
    printf("    Rin = %.1f k?\n", cascode.metrics.Rin / 1000.0);
    printf("    Rout = %.1f k? (very high!)\n",
           cascode.metrics.Rout / 1000.0);

    /* For comparison: standard CE with same bias */
    printf("\n[4] Comparison: Standard CE (same bias point):\n");
    {
        bjt_bias_network_t net_ce;
        net_ce.R1 = R1;
        net_ce.R2 = R2;
        net_ce.RC = RC;
        net_ce.RE = RE;
        net_ce.VCC = VCC;
        net_ce.Rs  = Rs;
        net_ce.type = BIAS_VOLTAGE_DIVIDER;

        bjt_bias_solve(&dev, &net_ce, 300.0, &qp_ce);
        bjt_compute_ss_params(&qp_ce, &dev, 300.0, &ss_ce);
        bjt_ce_analyze(&ss_ce, RC, RL, RE, Rs, 1, &met_ce);

        printf("    CE Av  = %.1f V/V  (%.1f dB)\n",
               met_ce.Av, met_ce.Av_dB);
        printf("    CE f_H = %.1f kHz (Miller limited)\n",
               met_ce.f_H / 1000.0);

        /* Cascode has similar gain but much higher bandwidth due to
         * elimination of Miller effect */
        printf("    Cascode Av  = %.1f V/V\n", cascode.metrics.Av);
        printf("    Cascode f_H = %.1f MHz (no Miller!)\n",
               cascode.metrics.f_H / 1e6);
    }

    /* Miller effect comparison */
    printf("\n[5] Miller Effect Comparison:\n");
    {
        double av_ce_mag = fabs(met_ce.Av);
        double cm_miller_ce = bjt_miller_capacitance(ss_ce.Cmu, av_ce_mag);

        /* In cascode, Q1 (CE) sees Q2 emitter as load: Z_load ? re2 ? 26 ohms.
         * So Av_Q1 ? gm * 26 ? 0.039 * 26 ? 1.0. Miller factor ? 2! */
        double av_q1_cascode = cascode.ss1.gm * cascode.ss2.re;
        double cm_miller_cascode = bjt_miller_capacitance(
            cascode.ss1.Cmu, fabs(av_q1_cascode));

        printf("    CE alone:  Cmu=%.1fpF, |Av|=%.0f, Miller_Cin=%.1fpF\n",
               ss_ce.Cmu * 1e12, av_ce_mag,
               cm_miller_ce * 1e12);
        printf("    Cascode Q1: Cmu=%.1fpF, |Av_Q1|=%.1f, Miller_Cin=%.1fpF\n",
               cascode.ss1.Cmu * 1e12, fabs(av_q1_cascode),
               cm_miller_cascode * 1e12);
        printf("    Bandwidth improvement: cascode eliminates %.0fx Miller multiplication!\n",
               (1.0 + av_ce_mag) / (1.0 + fabs(av_q1_cascode)));
    }

    /* Gain-Bandwidth comparison */
    printf("\n[6] Gain-Bandwidth Product:\n");
    {
        double gbw_ce      = fabs(met_ce.Av) * met_ce.f_H;
        double gbw_cascode = fabs(cascode.metrics.Av) * cascode.metrics.f_H;

        printf("    CE GBW      = %.1f MHz\n", gbw_ce / 1e6);
        printf("    Cascode GBW = %.1f MHz\n", gbw_cascode / 1e6);
        printf("    Cascode GBW advantage: %.1fx\n",
               gbw_cascode / gbw_ce);
    }

    /* Output impedance */
    printf("\n[7] Output Impedance:\n");
    {
        double rout_ce  = bjt_ce_output_impedance(&ss_ce, RC);
        double rout_cas = cascode.metrics.Rout;

        printf("    CE Rout      = %.1f k?\n", rout_ce / 1000.0);
        printf("    Cascode Rout = %.1f k?\n", rout_cas / 1000.0);
        printf("    Cascode Rout/CE Rout = %.1fx (better current source)\n",
               rout_cas / rout_ce);
    }

    printf("\n========================================\n");
    printf("  Cascode Amplifier Design Complete\n");
    printf("========================================\n\n");

    return 0;
}
