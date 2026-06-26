/**
 * example_source_follower.c — Common-Drain (Source Follower) Design Example
 *
 * Demonstrates end-to-end design of a source follower:
 * 1. Define buffer specifications (unity gain, low output Z)
 * 2. Design device and bias network
 * 3. Analyze impedance transformation (high Zin → low Zout)
 * 4. Report key buffer metrics
 *
 * Reference: Sedra & Smith §7.4 — "The Source Follower"
 *
 * Application: Capacitive sensor buffer (MEMS microphone front-end)
 *              Target: Av ≈ 0.95, Zout < 500Ω, Zin > 1GΩ
 *              Drives: 1m cable (200pF load)
 */

#include "fet_amplifier.h"
#include "fet_noise.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int main(void) {
    printf("═══════════════════════════════════════════════\n");
    printf("  Source Follower (CD) — Design Example\n");
    printf("  Application: MEMS Microphone Buffer\n");
    printf("═══════════════════════════════════════════════\n\n");

    /* ── Technology ── */
    FetTechParams tech;
    memset(&tech, 0, sizeof(tech));
    tech.tox    = 2e-9;       /* Thin oxide (IC process) */
    tech.cox    = fet_cox_from_tox(tech.tox);
    tech.mu_n   = 400.0;
    tech.vt_n   = 0.7;
    tech.lambda = 0.05;
    tech.gamma_body = 0.3;
    tech.phi_f  = 0.35;

    printf("── Technology: 0.18μm CMOS ──\n");
    printf("  Vth = %.2f V, Tox = %.1f nm\n\n", tech.vt_n, tech.tox * 1e9);

    /* ── Design Specifications ── */
    AmpDesignSpec spec;
    memset(&spec, 0, sizeof(spec));

    spec.vdd            = 3.3;     /* Low-voltage supply */
    spec.power_target   = 5e-3;    /* 5 mW target */
    spec.rin_target     = 1e9;     /* GΩ input (MEMS sensor) */
    spec.rout_target    = 500.0;   /* Drive 200pF+1m cable */
    spec.rs_source      = 1e6;     /* Sensor source impedance ~1MΩ */
    spec.rl_load        = 10e3;    /* Next stage load */
    spec.cl_load        = 200e-12; /* Cable capacitance + input */
    spec.target_bw_hz   = 1e6;     /* > 1 MHz (MEMS bandwidth) */
    spec.temperature    = 300.0;

    printf("── Buffer Specifications ──\n");
    printf("  Vdd:            %.1f V\n", spec.vdd);
    printf("  Power target:   %.1f mW\n", spec.power_target * 1e3);
    printf("  Input Z target: %.0f MΩ\n", spec.rin_target / 1e6);
    printf("  Output Z max:   %.0f Ω\n", spec.rout_target);
    printf("  Load:           %.0f pF || %.1f kΩ\n",
           spec.cl_load * 1e12, spec.rl_load / 1e3);
    printf("  Target BW:      %.1f MHz\n\n", spec.target_bw_hz / 1e6);

    /* ── Design ── */
    printf("── Running Source Follower Design ──\n");
    AmpDesignResult result = fet_design_cd_amplifier(&spec, &tech);

    if (!result.qpoint.valid) {
        printf("ERROR: Design failed.\n");
        return 1;
    }

    printf("\n════════ Design Results ════════\n\n");

    printf("  ┌─ Device ──────────────────────────┐\n");
    printf("  │ W/L:           %.1f               │\n",
           result.fet.geo.w_over_l);
    printf("  │ W:             %.1f μm            │\n",
           result.fet.geo.w * 1e6);
    printf("  └───────────────────────────────────┘\n\n");

    printf("  ┌─ Bias Point ──────────────────────┐\n");
    printf("  │ Id:            %.2f mA            │\n",
           result.qpoint.id * 1e3);
    printf("  │ Vgs:           %.3f V             │\n", result.qpoint.vgs);
    printf("  │ Vds:           %.3f V             │\n", result.qpoint.vds);
    printf("  │ gm:            %.2f mS            │\n",
           result.qpoint.gm * 1e3);
    printf("  └───────────────────────────────────┘\n\n");

    printf("  ┌─ Key Metrics ─────────────────────┐\n");
    printf("  │ Voltage Gain:  %.4f (%.3f dB)     │\n",
           result.metrics.av, result.metrics.av_db);
    printf("  │ Output Z:      %.1f Ω             │\n",
           result.metrics.rout);
    printf("  │ Input Z:       > 1 GΩ             │\n");
    printf("  │ Input C:       %.2f pF            │\n",
           result.hp_model.cgs * 1e12);
    printf("  │ BW (-3dB):     %.1f MHz           │\n",
           result.metrics.bw / 1e6);
    printf("  │ DC Power:      %.2f mW            │\n",
           result.power_actual * 1e3);
    printf("  └───────────────────────────────────┘\n\n");

    /* ── Buffer Performance Analysis ── */
    printf("── Impedance Transformation ──\n");
    double zin  = result.metrics.rin;
    double zout = result.metrics.rout;
    printf("  Zin / Zout = %.0f MΩ / %.0f Ω = %.0f:1 impedance ratio\n",
           zin / 1e6, zout, zin / zout);
    printf("  → Provides ~%.0f dB of impedance buffering\n",
           20.0 * log10(zin / zout));

    /* ── Driving Cable Analysis ── */
    printf("\n── Cable Drive Analysis ──\n");
    double cable_cap = 200e-12;  /* ~200pF for 1m coax */
    double cable_bw  = 1.0 / (2.0 * M_PI * zout * cable_cap);
    printf("  With 200pF (1m cable):\n");
    printf("    Output pole:  %.1f MHz\n", cable_bw / 1e6);
    printf("    Rise time:    %.1f ns (10-90%%)\n",
           0.35 / cable_bw * 1e9);
    printf("    → %s for 1 MHz signal\n",
           cable_bw > 5e6 ? "Adequate" : "Marginal");

    printf("\n═══════════════════════════════════════════════\n");
    printf("  Source Follower Design Complete\n");

    /* Compare: without buffer, sensor drives cable directly */
    double direct_bw = 1.0 / (2.0 * M_PI * spec.rs_source * cable_cap);
    printf("  Without buffer: BW = %.1f kHz (too low!)\n", direct_bw / 1e3);
    printf("  With buffer:    BW = %.1f MHz (%.0fx improvement)\n",
           cable_bw / 1e6, cable_bw / direct_bw);
    printf("═══════════════════════════════════════════════\n");

    return 0;
}
