/**
 * example_cs_amplifier.c — Common-Source FET Amplifier Design Example
 *
 * Demonstrates end-to-end design of a common-source amplifier:
 * 1. Define design specifications (gain, bandwidth, supply)
 * 2. Design device dimensions and bias network
 * 3. Analyze small-signal performance (gain, BW, impedance)
 * 4. Evaluate noise figure
 * 5. Report design results and verify against specs
 *
 * Reference: Sedra & Smith §6.3, §7.3 — "The Common-Source Amplifier"
 *
 * Application: Audio preamplifier for a guitar pickup (Hi-Z input)
 *              Target: Av = 20dB, BW > 20kHz, Zin > 100kΩ
 */

#include "fet_amplifier.h"
#include "fet_noise.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int main(void) {
    printf("═══════════════════════════════════════════════\n");
    printf("  Common-Source FET Amplifier — Design Example\n");
    printf("  Application: Guitar Pickup Pre-amplifier\n");
    printf("═══════════════════════════════════════════════\n\n");

    /* ── Technology Definition ── */
    printf("── Technology: Discrete NMOS (e.g., 2N7000) ──\n");
    FetTechParams tech;
    memset(&tech, 0, sizeof(tech));

    /* Typical 2N7000 / CD4007 parameters */
    tech.tox    = 50e-9;      /* Thick gate oxide (~50nm, discrete) */
    tech.cox    = fet_cox_from_tox(tech.tox);
    tech.mu_n   = 600.0;       /* cm²/V·s */
    tech.vt_n   = 2.1;         /* V (typical discrete NMOS) */
    tech.lambda = 0.01;        /* 1/V (long channel → low λ) */
    tech.cgso   = 5e-9;        /* F/m overlap */
    tech.cgdo   = 5e-9;
    tech.cj0    = 0.3e-3;
    tech.mj     = 0.5;
    tech.pb     = 0.75;

    printf("  Vth = %.2f V\n", tech.vt_n);
    printf("  Cox = %.2e F/m²\n", tech.cox);
    double kp = fet_kprime(tech.mu_n, tech.cox);
    printf("  k'n = %.2e A/V²\n\n", kp);

    /* ── Design Specifications ── */
    printf("── Design Specifications ──\n");
    AmpDesignSpec spec;
    memset(&spec, 0, sizeof(spec));

    spec.target_gain_db  = 20.0;      /* 10x voltage gain */
    spec.target_bw_hz    = 50e3;      /* > 20kHz audio band */
    spec.rin_target      = 100e3;     /* Hi-Z for guitar pickup */
    spec.rout_target     = 10e3;      /* Moderate output impedance */
    spec.vdd             = 12.0;      /* Single 12V supply */
    spec.power_max       = 100e-3;    /* 100 mW max */
    spec.rs_source       = 10e3;      /* Guitar pickup ~10kΩ */
    spec.rl_load         = 47e3;      /* Next stage input impedance */
    spec.cl_load         = 100e-12;   /* Cable + input capacitance */
    spec.vout_swing_min  = 2.0;       /* 2Vpp min output */
    spec.temperature     = 300.0;

    printf("  Target Gain:    %.0f dB (%.1fx)\n", spec.target_gain_db,
           pow(10.0, spec.target_gain_db / 20.0));
    printf("  Target BW:      %.1f kHz\n", spec.target_bw_hz / 1e3);
    printf("  Input Z target: %.0f kΩ\n", spec.rin_target / 1e3);
    printf("  Supply:         %.1f V\n", spec.vdd);
    printf("  Power budget:   %.1f mW\n\n", spec.power_max * 1e3);

    /* ── Amplifier Design ── */
    printf("── Running CS Amplifier Design ──\n");
    AmpDesignResult result = fet_design_cs_amplifier(&spec, &tech);

    if (!result.qpoint.valid) {
        printf("ERROR: Design failed — Q-point invalid.\n");
        return 1;
    }

    /* ── Results ── */
    printf("\n════════ Design Results ════════\n\n");

    printf("  ┌─ Device ──────────────────────────┐\n");
    printf("  │ W/L ratio:     %.1f               │\n",
           result.fet.geo.w_over_l);
    printf("  │ W:             %.1f μm            │\n",
           result.fet.geo.w * 1e6);
    printf("  │ L:             %.1f μm            │\n",
           result.fet.geo.l * 1e6);
    printf("  └───────────────────────────────────┘\n\n");

    printf("  ┌─ Bias Point ──────────────────────┐\n");
    printf("  │ Vgs:           %.3f V             │\n", result.qpoint.vgs);
    printf("  │ Vds:           %.3f V             │\n", result.qpoint.vds);
    printf("  │ Id:            %.3f mA            │\n", result.qpoint.id * 1e3);
    printf("  │ Vov:           %.0f mV            │\n", result.qpoint.vov * 1e3);
    printf("  │ gm:            %.2f mS            │\n", result.qpoint.gm * 1e3);
    printf("  │ Region:        %s                 │\n",
           result.qpoint.region == FET_REGION_SATURATION ? "SATURATION ✓" : "OTHER");
    printf("  └───────────────────────────────────┘\n\n");

    printf("  ┌─ Bias Network ────────────────────┐\n");
    printf("  │ Topology:      Voltage Divider    │\n");
    printf("  │ Rd:            %.1f kΩ            │\n",
           result.bias.rd / 1e3);
    printf("  │ Rs:            %.0f Ω             │\n", result.bias.rs);
    printf("  │ Rg1:           %.1f kΩ            │\n",
           result.bias.rg1 / 1e3);
    printf("  │ Rg2:           %.1f kΩ            │\n",
           result.bias.rg2 / 1e3);
    printf("  │ Vgg:           %.2f V             │\n", result.bias.vgg);
    printf("  │ Rgg:           %.1f kΩ            │\n",
           result.bias.rgg / 1e3);
    printf("  └───────────────────────────────────┘\n\n");

    printf("  ┌─ AC Performance ──────────────────┐\n");
    printf("  │ Voltage Gain:  %.1f dB (%.2fx)    │\n",
           result.metrics.av_db, fabs(result.metrics.av));
    printf("  │ Bandwidth:     %.1f kHz           │\n",
           result.metrics.bw / 1e3);
    printf("  │ GBW product:   %.1f MHz           │\n",
           result.metrics.gbw / 1e6);
    printf("  │ Input Z:       %.1f kΩ            │\n",
           result.metrics.rin / 1e3);
    printf("  │ Output Z:      %.1f kΩ            │\n",
           result.metrics.rout / 1e3);
    printf("  │ Input C (w/    %.1f pF            │\n",
           result.metrics.cin * 1e12);
    printf("  │   Miller):                        │\n");
    printf("  └───────────────────────────────────┘\n\n");

    printf("  ┌─ Noise ───────────────────────────┐\n");
    printf("  │ NF @ 1kHz:     %.2f dB            │\n",
           result.nf_result.noise_figure_db);
    printf("  │ NFmin:         %.2f dB            │\n",
           result.nf_result.nf_min_db);
    printf("  │ Vn_input:      %.2f nV/√Hz        │\n",
           result.metrics.vn_input * 1e9);
    printf("  └───────────────────────────────────┘\n\n");

    printf("  ┌─ Power ───────────────────────────┐\n");
    printf("  │ DC Power:      %.1f mW            │\n",
           result.power_actual * 1e3);
    printf("  │ Vout Swing:    ~%.2f Vpp           │\n",
           2.0 * fmin(result.qpoint.vds, spec.vdd - result.qpoint.vds));
    printf("  └───────────────────────────────────┘\n\n");

    /* ── Specification Verification ── */
    printf("── Specification Check ──\n");
    printf("  Gain ≥ %.0f dB: %s\n", spec.target_gain_db,
           result.metrics.av_db >= spec.target_gain_db ? "✓ PASS" : "✗ FAIL");
    printf("  BW ≥ %.0f kHz:  %s\n", spec.target_bw_hz / 1e3,
           result.metrics.bw >= spec.target_bw_hz ? "✓ PASS" : "✗ FAIL");
    printf("  Zin ≥ %.0f kΩ:  %s\n", spec.rin_target / 1e3,
           result.metrics.rin >= spec.rin_target ? "✓ PASS" : "✗ FAIL");
    printf("  Pdc ≤ %.0f mW:  %s\n", spec.power_max * 1e3,
           result.power_actual <= spec.power_max ? "✓ PASS" : "✗ FAIL");

    printf("\n  %s\n",
           result.spec_met ? "ALL SPECS MET ✓" : "SOME SPECS FAILED ✗");

    printf("\n═══════════════════════════════════════════════\n");
    printf("  Design Complete — %s\n",
           result.spec_met ? "Ready for schematic capture" : "Needs iteration");
    printf("═══════════════════════════════════════════════\n");

    return 0;
}
