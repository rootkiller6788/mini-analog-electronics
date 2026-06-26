/**
 * example_ce_amplifier.c ? Common-Emitter Amplifier Design Example
 *
 * Demonstrates a complete CE amplifier design flow:
 *   1. Specify requirements (gain, impedance, bandwidth)
 *   2. Design DC bias network
 *   3. Compute small-signal parameters
 *   4. Analyze frequency response
 *   5. Evaluate performance metrics
 *
 * This is the canonical first BJT amplifier design taught in
 * MIT 6.002, Berkeley EE105, and Stanford EE101B.
 *
 * Application: Audio preamplifier ? moderate gain, modest bandwidth,
 *              low cost (discrete design with standard resistors).
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
    bjt_amplifier_design_t design;
    double freq_test_points[] = {10.0, 100.0, 1000.0, 10000.0, 100000.0, 1e6};
    int i, n_freqs = 6;

    printf("\n========================================\n");
    printf("  BJT Common-Emitter Amplifier Design\n");
    printf("  Audio Preamplifier Example\n");
    printf("========================================\n\n");

    /* Initialize device model (2N3904-like) */
    bjt_spice_default_npn(&dev);
    printf("[1] Device: NPN small-signal BJT (2N3904-like)\n");
    printf("    BF = %.0f, VAF = %.1f V, fT ~ 300 MHz\n\n", dev.BF, dev.VAF);

    /* Design specifications */
    double Av_target  = 50.0;    /* Voltage gain of 50 (34 dB) */
    double Rin_target = 10000.0; /* Input impedance > 10 k? */
    double Rout_target= 5000.0;  /* Output impedance < 5 k? */
    double VCC        = 15.0;    /* Single supply */
    double RL         = 100000.0;/* Load impedance (next stage input) */
    double Rs         = 600.0;   /* Source impedance (microphone) */
    double fL_target  = 20.0;    /* Audio low end */
    double fH_target  = 20000.0; /* Audio high end (20 kHz) */

    printf("[2] Design Specifications:\n");
    printf("    Av_target  = %.0f V/V (%.1f dB)\n", Av_target,
           20.0 * log10(Av_target));
    printf("    Rin_target > %.0f k?\n", Rin_target / 1000.0);
    printf("    Rout_target < %.0f k?\n", Rout_target / 1000.0);
    printf("    VCC         = %.0f V\n", VCC);
    printf("    fL_target   = %.0f Hz\n", fL_target);
    printf("    fH_target   = %.0f Hz\n", fH_target);
    printf("    RL          = %.0f k? (load)\n", RL / 1000.0);
    printf("    Rs          = %.0f ? (source)\n\n", Rs);

    /* Run the automated design */
    printf("[3] Designing amplifier...\n");
    int ret = bjt_amplifier_design(AMP_COMMON_EMITTER,
                                    Av_target, Rin_target, Rout_target,
                                    VCC, RL, Rs, fL_target, fH_target,
                                    &dev, 300.0, &design);

    if (ret != 0) {
        printf("ERROR: Design failed (ret=%d)\n", ret);
        return 1;
    }
    printf("    Design complete.\n\n");

    /* Report Q-point */
    printf("[4] DC Operating Point (Q-point):\n");
    printf("    IC    = %.3f mA\n", design.qpoint.IC * 1000.0);
    printf("    IB    = %.3f uA\n", design.qpoint.IB * 1e6);
    printf("    IE    = %.3f mA\n", design.qpoint.IE * 1000.0);
    printf("    VCE   = %.2f V\n", design.qpoint.VCE);
    printf("    VBE   = %.3f V\n", design.qpoint.VBE);
    printf("    P_dc  = %.1f mW\n", design.qpoint.power * 1000.0);
    printf("    Region: %s\n\n",
           design.qpoint.region == BJT_REGION_FORWARD_ACTIVE
           ? "FORWARD ACTIVE" : "OTHER");

    /* Report bias network */
    printf("[5] Bias Network (Voltage Divider):\n");
    printf("    R1  = %.1f k?  (to VCC)\n", design.bias.R1 / 1000.0);
    printf("    R2  = %.1f k?  (to GND)\n", design.bias.R2 / 1000.0);
    printf("    RC  = %.1f k?\n", design.bias.RC / 1000.0);
    printf("    RE  = %.1f k?\n", design.bias.RE / 1000.0);
    printf("    VCC = %.0f V\n\n", design.bias.VCC);

    /* Report small-signal parameters */
    printf("[6] Small-Signal Parameters:\n");
    printf("    gm    = %.2f mS\n", design.ss.gm * 1000.0);
    printf("    rpi   = %.1f k?\n", design.ss.rpi / 1000.0);
    printf("    ro    = %.1f k?\n", design.ss.ro / 1000.0);
    printf("    re    = %.2f ?\n", design.ss.re);
    printf("    beta  = %.0f\n", design.ss.beta);
    printf("    Cpi   = %.1f pF\n", design.ss.Cpi * 1e12);
    printf("    Cmu   = %.1f pF\n", design.ss.Cmu * 1e12);
    printf("    fT    = %.1f MHz\n\n", design.ss.fT / 1e6);

    /* Report performance metrics */
    printf("[7] Amplifier Performance:\n");
    printf("    Av    = %.1f V/V  (%.1f dB)\n",
           design.metrics.Av, design.metrics.Av_dB);
    printf("    Ai    = %.1f A/A  (%.1f dB)\n",
           design.metrics.Ai, design.metrics.Ai_dB);
    printf("    Ap    = %.1f W/W  (%.1f dB)\n",
           design.metrics.Ap, design.metrics.Ap_dB);
    printf("    Rin   = %.1f k?\n", design.metrics.Rin / 1000.0);
    printf("    Rout  = %.1f k?\n", design.metrics.Rout / 1000.0);
    printf("    f_L   = %.1f Hz\n", design.metrics.f_L);
    printf("    f_H   = %.1f kHz\n", design.metrics.f_H / 1000.0);
    printf("    BW    = %.1f kHz\n", design.metrics.BW / 1000.0);
    printf("    GBW   = %.1f MHz\n", design.metrics.GBW / 1e6);
    printf("    V_swing_max = %.2f Vpp (est.)\n\n",
           design.metrics.swing_max);

    /* Coupling capacitors */
    printf("[8] Coupling Capacitors:\n");
    printf("    C_in  = %.1f uF  (input coupling)\n",
           design.coupling_caps[0] * 1e6);
    printf("    C_out = %.1f uF  (output coupling)\n",
           design.coupling_caps[1] * 1e6);
    printf("    C_E   = %.1f uF  (emitter bypass)\n\n",
           design.coupling_caps[2] * 1e6);

    /* Frequency response sweep */
    printf("[9] Frequency Response:\n");
    printf("    %-12s  %-12s  %-12s  %-10s\n",
           "Freq (Hz)", "Gain (V/V)", "Gain (dB)", "Phase(deg)");
    printf("    %-12s  %-12s  %-12s  %-10s\n",
           "----------", "----------", "----------", "----------");

    for (i = 0; i < n_freqs; i++) {
        double f = freq_test_points[i];
        double mag = bjt_amplifier_evaluate(&design, f);
        double phase = bjt_tf_phase(&design.tf, f);
        double mag_db = bjt_gain_to_db(mag);
        double phase_deg = phase * 180.0 / M_PI;

        printf("    %-12.0f  %-12.2f  %-12.1f  %-10.1f\n",
               f, mag, mag_db, phase_deg);
    }

    /* Distortion estimate */
    printf("\n[10] Distortion Estimate:\n");
    {
        double vin_test = 0.010;  /* 10 mV peak input */
        double hd2 = bjt_harmonic_distortion_hd2(vin_test,
                                                   bjt_vt(300.0));
        double hd3 = bjt_harmonic_distortion_hd3(vin_test,
                                                   bjt_vt(300.0));
        double thd = bjt_thd_from_hd(hd2, hd3);
        printf("    For Vin = %.0f mV peak:\n", vin_test * 1000.0);
        printf("    HD2 = %.3f %%\n", hd2 * 100.0);
        printf("    HD3 = %.4f %%\n", hd3 * 100.0);
        printf("    THD = %.3f %%\n", thd * 100.0);
    }

    printf("\n========================================\n");
    printf("  CE Amplifier Design Complete\n");
    printf("========================================\n\n");

    return 0;
}
