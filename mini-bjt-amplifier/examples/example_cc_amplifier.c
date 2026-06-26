/**
 * example_cc_amplifier.c ? Common-Collector (Emitter Follower) Example
 *
 * The CC amplifier is the canonical voltage buffer: Av ? 1, high Rin,
 * low Rout. This example designs an emitter follower for impedance
 * matching between a high-impedance sensor and a low-impedance load.
 *
 * Application: Sensor buffer ? piezoelectric sensor (high Z) driving
 *              an ADC input or audio line (low Z).
 *
 * Reference: Sedra & Smith, Sec. 7.3
 * Course: MIT 6.002, Berkeley EE105, Stanford EE101B
 */

#include <stdio.h>
#include <math.h>
#include "../include/bjt_model.h"
#include "../include/bjt_dc_bias.h"
#include "../include/bjt_small_signal.h"
#include "../include/bjt_noise.h"

int main(void)
{
    bjt_spice_params_t dev;
    bjt_bias_network_t net;
    bjt_qpoint_t qp;
    bjt_ss_params_t ss;
    bjt_amp_metrics_t metrics;

    printf("\n========================================\n");
    printf("  BJT Common-Collector (Emitter Follower)\n");
    printf("  Sensor Buffer / Impedance Transformer\n");
    printf("========================================\n\n");

    bjt_spice_default_npn(&dev);

    /* Design for: VCC=12V, IC=2mA, RE=3.3k (drives 10k load) */
    double VCC = 12.0;
    double RE  = 3300.0;
    double RL  = 10000.0;
    double Rs  = 100000.0;  /* High-impedance piezoelectric sensor */
    double IC_target = 2e-3;

    printf("[1] Design Parameters:\n");
    printf("    VCC = %.0f V\n", VCC);
    printf("    RE  = %.1f k?\n", RE/1000.0);
    printf("    RL  = %.0f k? (load)\n", RL/1000.0);
    printf("    Rs  = %.0f k? (high-Z sensor)\n\n", Rs/1000.0);

    /* Voltage divider bias for CC */
    net.R1 = 100000.0;
    net.R2 = 22000.0;
    net.RC = 0.0;   /* No collector resistor in CC */
    net.RE = RE;
    net.VCC = VCC;
    net.VEE = 0.0;
    net.Rs  = Rs;
    net.type = BIAS_VOLTAGE_DIVIDER;

    bjt_bias_solve(&dev, &net, 300.0, &qp);
    bjt_compute_ss_params(&qp, &dev, 300.0, &ss);

    printf("[2] Q-point:\n");
    printf("    IC  = %.2f mA\n", qp.IC * 1000.0);
    printf("    VCE = %.2f V\n", qp.VCE);
    printf("    VBE = %.3f V\n\n", qp.VBE);

    /* Analyze */
    bjt_cc_analyze(&ss, RE, RL, Rs, &metrics);

    printf("[3] Small-Signal Performance:\n");
    printf("    Voltage Gain  Av = %.4f V/V  (%.2f dB)\n",
           metrics.Av, metrics.Av_dB);
    printf("    Current Gain  Ai = %.1f A/A  (%.1f dB)\n",
           metrics.Ai, metrics.Ai_dB);
    printf("    Power Gain    Ap = %.1f W/W  (%.1f dB)\n",
           metrics.Ap, metrics.Ap_dB);
    printf("    Input Z       Rin = %.1f k?\n", metrics.Rin/1000.0);
    printf("    Output Z      Rout = %.1f ?\n", metrics.Rout);

    /* Impedance transformation analysis */
    printf("\n[4] Impedance Transformation:\n");
    {
        double Rin_cc = bjt_cc_input_impedance(&ss, RE, RL);
        double Rout_cc = bjt_cc_output_impedance(&ss, RE, Rs);

        printf("    Sensor sees:  Rin = %.1f k? (high, minimal loading)\n",
               Rin_cc/1000.0);
        printf("    Load sees:    Rout = %.1f ? (low, good drive)\n",
               Rout_cc);
        printf("    Impedance ratio: %.0f:1\n", Rin_cc / Rout_cc);
    }

    /* Signal swing */
    printf("\n[5] Signal Handling:\n");
    {
        double vbe = qp.VBE;
        double v_emitter_max = VCC - vbe - 0.5;  /* Keep Q1 active */
        double v_emitter_min = 0.5;               /* Keep above saturation */
        printf("    Output swing: %.1f to %.1f V (%.1f Vpp)\n",
               v_emitter_min, v_emitter_max,
               v_emitter_max - v_emitter_min);
    }

    /* Noise analysis for sensor application */
    printf("\n[6] Noise Analysis (sensor buffer):\n");
    {
        bjt_noise_sources_t ns;
        bjt_noise_compute_sources(&dev, &qp, Rs, 1000.0, 300.0, &ns);

        double vn_dens = bjt_noise_input_voltage_density(&ns, &ss, Rs);
        double in_dens = bjt_noise_input_current_density(&ns, &ss);
        double nf = bjt_noise_figure(&ns, &ss, Rs, 300.0);
        double Ropt = bjt_noise_optimum_source_resistance(vn_dens, in_dens);
        double IC_opt = bjt_noise_optimum_ic(Rs, 300.0);

        printf("    Input noise voltage: %.2f nV/sqrt(Hz)\n",
               vn_dens * 1e9);
        printf("    Input noise current: %.2f pA/sqrt(Hz)\n",
               in_dens * 1e12);
        printf("    Noise Figure: %.2f (%.2f dB)\n",
               nf, 10.0 * log10(nf));
        printf("    Optimum Rs for min NF: %.1f k?\n",
               Ropt/1000.0);
        printf("    Optimum IC for Rs=%.0f k?: %.2f mA\n",
               Rs/1000.0, IC_opt * 1000.0);
    }

    printf("\n========================================\n");
    printf("  Emitter Follower Design Complete\n");
    printf("========================================\n\n");

    return 0;
}
