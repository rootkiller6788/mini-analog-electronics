/**
 * example_cascode.c — Cascode FET Amplifier Design Example
 *
 * Demonstrates the cascode topology (CS + CG):
 * 1. Design CS stage for gain, CG stage for bandwidth
 * 2. Compare Miller effect in CS vs cascode
 * 3. Show output resistance enhancement
 * 4. Frequency response comparison
 *
 * Reference: Sedra & Smith §7.5 — "The Cascode Amplifier"
 *            Razavi §3.5 — "Cascode Stage"
 *
 * Application: Wideband RF pre-amplifier
 *              Target: Av = 26dB, BW > 200MHz, driving 50Ω
 */

#include "fet_amplifier.h"
#include "fet_noise.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int main(void) {
    printf("═══════════════════════════════════════════════\n");
    printf("  Cascode FET Amplifier — Design Example\n");
    printf("  Application: Wideband RF Pre-amplifier\n");
    printf("═══════════════════════════════════════════════\n\n");

    /* ── Technology ── */
    FetTechParams tech;
    memset(&tech, 0, sizeof(tech));
    tech.tox    = 2e-9;
    tech.cox    = fet_cox_from_tox(tech.tox);
    tech.mu_n   = 400.0;
    tech.vt_n   = 0.5;
    tech.lambda = 0.1;      /* Short-channel: more CLM */
    tech.cgso   = 0.3e-9;
    tech.cgdo   = 0.3e-9;
    tech.gamma_body = 0.2;
    tech.phi_f  = 0.35;

    printf("── Technology: 65nm CMOS RF ──\n\n");

    /* ── Design Specifications ── */
    AmpDesignSpec spec;
    memset(&spec, 0, sizeof(spec));

    spec.target_gain_db  = 26.0;
    spec.target_bw_hz    = 200e6;    /* 200 MHz */
    spec.vdd             = 1.8;
    spec.power_max       = 20e-3;
    spec.rs_source       = 50.0;     /* RF: 50Ω system */
    spec.rl_load         = 50.0;
    spec.cl_load         = 0.5e-12;  /* 0.5 pF */
    spec.temperature     = 300.0;

    printf("── Specifications ──\n");
    printf("  Target Gain:    %.0f dB\n", spec.target_gain_db);
    printf("  Target BW:      %.0f MHz\n", spec.target_bw_hz / 1e6);
    printf("  Supply:         %.1f V\n", spec.vdd);
    printf("  Source/Load:    50 Ω (RF matched)\n\n");

    /* ── Design Cascode ── */
    printf("── Designing Cascode Amplifier ──\n");
    AmpDesignResult result = fet_design_cascode_amplifier(&spec, &tech);

    printf("\n════════ Cascode Results ════════\n\n");

    printf("  ┌─ Device (both transistors) ───────┐\n");
    printf("  │ W/L:           %.1f               │\n",
           result.fet.geo.w_over_l);
    printf("  │ W:             %.1f μm            │\n",
           result.fet.geo.w * 1e6);
    printf("  │ L:             %.0f nm            │\n",
           result.fet.geo.l * 1e9);
    printf("  └───────────────────────────────────┘\n\n");

    printf("  ┌─ Bias Point ──────────────────────┐\n");
    printf("  │ Id:            %.2f mA            │\n",
           result.qpoint.id * 1e3);
    printf("  │ Vov:           %.0f mV            │\n",
           result.qpoint.vov * 1e3);
    printf("  │ gm:            %.1f mS            │\n",
           result.qpoint.gm * 1e3);
    printf("  │ rds:           %.1f kΩ            │\n",
           result.qpoint.gds > 0.0 ?
           1.0 / result.qpoint.gds / 1e3 : INFINITY);
    printf("  └───────────────────────────────────┘\n\n");

    printf("  ┌─ AC Performance ──────────────────┐\n");
    printf("  │ Voltage Gain:  %.1f dB (%.1fx)    │\n",
           result.metrics.av_db, fabs(result.metrics.av));
    printf("  │ Bandwidth:     %.1f MHz           │\n",
           result.metrics.bw / 1e6);
    printf("  │ GBW:           %.1f GHz           │\n",
           result.metrics.gbw / 1e9);
    printf("  │ Output Z:      %.1f kΩ            │\n",
           result.metrics.rout / 1e3);
    printf("  └───────────────────────────────────┘\n\n");

    /* ── Comparison: CS vs Cascode ── */
    printf("── Miller Effect Comparison ──\n");

    HybridPiModel hp = result.hp_model;
    double gm_val     = result.qpoint.gm;

    /* CS alone: Miller multiplies Cgd by (1 + |Av|) */
    double av_cs = fabs(fet_cs_gain_ideal(gm_val,
                    1.0 / result.qpoint.gds,
                    result.bias.rd));
    double c_miller_cs = hp.cgd * (1.0 + fabs(av_cs));
    printf("  ┌─ Common-Source (standalone) ──────┐\n");
    printf("  │ Cgd:           %.2f fF            │\n",
           hp.cgd * 1e15);
    printf("  │ CS gain:       %.1fx              │\n", fabs(av_cs));
    printf("  │ Cin_miller:    %.2f fF            │\n",
           c_miller_cs * 1e15);
    printf("  │ Cgd multiplier: %.0fx             │\n",
           1.0 + fabs(av_cs));
    /* BW estimate: fp = 1/(2π * Rsig * Cin_miller) */
    double bw_cs = 1.0 / (2.0 * M_PI * 50.0 * c_miller_cs);
    printf("  │ BW (Miller lim): %.1f MHz         │\n",
           bw_cs / 1e6);
    printf("  └───────────────────────────────────┘\n\n");

    /* Cascode: CS gain ≈ -1 into CG source → minimal Miller */
    printf("  ┌─ Cascode (CS + CG) ───────────────┐\n");
    double av_cs_to_cg = -1.0;  /* CS drives CG source (~1/gm load) */
    double c_miller_cascode = hp.cgd * (1.0 - av_cs_to_cg);
    printf("  │ CS gain to CG: ≈ -1x              │\n");
    printf("  │ Cin_miller:    %.2f fF            │\n",
           c_miller_cascode * 1e15);
    printf("  │ Cgd multiplier: %.0fx             │\n",
           1.0 - av_cs_to_cg);
    printf("  │ BW improvement: %.0fx             │\n",
           c_miller_cs / c_miller_cascode);
    printf("  └───────────────────────────────────┘\n\n");

    /* ── OCTC Analysis ── */
    printf("── OCTC Time Constants ──\n");
    OctcResult octc = result.octc;
    printf("  %-20s %8s %12s %12s\n",
           "Capacitor", "C [fF]", "R_seen [Ω]", "τ [ps]");
    printf("  ────────────────────────────────────────\n");
    for (uint32_t i = 0; i < octc.n_caps; i++) {
        printf("  %-20s %8.2f %12.0f %12.2f\n",
               octc.tc[i].name,
               octc.tc[i].capacitance * 1e15,
               octc.tc[i].resistance_seen,
               octc.tc[i].time_constant * 1e12);
    }
    printf("  ────────────────────────────────────────\n");
    printf("  Στ = %.2f ps → fh = %.1f MHz\n",
           octc.sum_tau * 1e12, octc.fh_estimate / 1e6);

    /* ── Output Resistance Enhancement ── */
    printf("\n── Output Resistance Enhancement ──\n");
    double ro1 = 1.0 / result.qpoint.gds;
    double ro2 = ro1;  /* Same size devices */
    double ro_cascode = gm_val * ro1 * ro2;
    printf("  Single FET ro:      %.1f kΩ\n", ro1 / 1e3);
    printf("  Cascode Rout ≈      %.1f kΩ\n", ro_cascode / 1e3);
    printf("  Enhancement factor: %.0fx\n", ro_cascode / ro1);
    printf("  This enables %.0f dB more gain with same bandwidth.\n",
           20.0 * log10(ro_cascode / ro1));

    printf("\n═══════════════════════════════════════════════\n");
    printf("  Cascode Design Complete\n");
    printf("  Key advantage: %.0fx wider BW for same gain\n",
           c_miller_cs / c_miller_cascode);
    printf("═══════════════════════════════════════════════\n");

    return 0;
}
