/* Example: Current Mirror Design and Comparison
 * Demonstrates simple, cascode, Wilson, and wide-swing current mirrors.
 * Reference: Razavi Ch.5; Gray & Meyer Ch.4
 */
#include "analog_ic_current_mirror.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    printf("=== Current Mirror Design Comparison ===\n\n");

    ic_technology_t *tech = ic_tech_init_180nm();
    if (!tech) { printf("ERROR: tech init failed\n"); return 1; }

    cm_spec_t spec;
    memset(&spec, 0, sizeof(spec));
    spec.device_type = MOS_TYPE_NMOS;
    spec.I_ref = 10e-6;         /* 10 uA reference */
    spec.I_out_target = 20e-6;  /* 20 uA output (ratio 2:1) */
    spec.ratio = 2.0;
    spec.Vdd = 1.8;
    spec.Vout_min = 0.5;
    spec.tech = tech;
    spec.T = 300.0;

    printf("Specs: Iref=%.0f uA, Iout_target=%.0f uA, ratio=%.1f, Vdd=%.1f V\n\n",
           spec.I_ref*1e6, spec.I_out_target*1e6, spec.ratio, spec.Vdd);

    cm_result_t r_simple, r_cascode, r_wilson, r_wide, r_reg;
    memset(&r_simple, 0, sizeof(r_simple));
    memset(&r_cascode, 0, sizeof(r_cascode));
    memset(&r_wilson, 0, sizeof(r_wilson));
    memset(&r_wide, 0, sizeof(r_wide));
    memset(&r_reg, 0, sizeof(r_reg));

    /* Simple mirror */
    printf("--- Simple Current Mirror ---\n");
    cm_design_simple(&spec, &r_simple);
    printf("  Iout = %.2f uA, Ratio = %.3f\n", r_simple.I_out*1e6, r_simple.ratio_actual);
    printf("  Rout = %.2f MOhm\n", r_simple.Rout/1e6);
    printf("  Vout_min (compliance) = %.3f V\n", r_simple.Vout_min_actual);
    printf("  Systematic error = %.2f %%\n\n", r_simple.systematic_error);

    /* Cascode mirror */
    printf("--- Cascode Current Mirror ---\n");
    cm_design_cascode(&spec, &r_cascode);
    printf("  Iout = %.2f uA\n", r_cascode.I_out*1e6);
    printf("  Rout = %.2f MOhm (%.0fx improvement!)\n",
           r_cascode.Rout/1e6, r_cascode.Rout / (r_simple.Rout + 1e-6));
    printf("  Vout_min = %.3f V\n", r_cascode.Vout_min_actual);

    /* Rout comparison */
    double rout_calc = cm_output_resistance_cascode(2e-3, 1e-7, 1e-7, 0.0);
    printf("  Rout (analytic) = %.2f MOhm\n\n", rout_calc/1e6);

    /* Wilson mirror */
    printf("--- Wilson Current Mirror ---\n");
    cm_design_wilson(&spec, &r_wilson);
    printf("  Iout = %.2f uA\n", r_wilson.I_out*1e6);
    printf("  Rout = %.2f MOhm\n", r_wilson.Rout/1e6);
    printf("  Vout_min = %.3f V\n\n", r_wilson.Vout_min_actual);

    /* Wide-swing cascode */
    printf("--- Wide-Swing Cascode Mirror ---\n");
    cm_design_wide_swing(&spec, &r_wide);
    printf("  Iout = %.2f uA\n", r_wide.I_out*1e6);
    printf("  Rout = %.2f MOhm\n", r_wide.Rout/1e6);
    printf("  Vout_min = %.3f V (lowest for cascode type)\n", r_wide.Vout_min_actual);
    printf("  Vb_casc = %.3f V\n\n", r_wide.Vb_casc);

    /* Regulated cascode */
    printf("--- Regulated (Gain-Boosted) Cascode ---\n");
    cm_design_regulated(&spec, &r_reg);
    printf("  Iout = %.2f uA\n", r_reg.I_out*1e6);
    printf("  Rout = %.2f GOhm (extremely high!)\n", r_reg.Rout/1e9);
    printf("  Vout_min = %.3f V\n\n", r_reg.Vout_min_actual);

    /* Mismatch analysis */
    printf("=== Mismatch Analysis (Simple Mirror) ===\n");
    mismatch_params_t mp = mismatch_from_tech(tech);
    double sigma_rand = cm_mismatch_random_sigma(&mp,
        r_simple.W_in, r_simple.L_in, r_simple.W_out, r_simple.L_out, 20.0);
    printf("  Random mismatch sigma = %.2f %%\n", sigma_rand);
    printf("  Systematic error = %.2f %%\n", r_simple.systematic_error);
    printf("  Total mismatch (3-sigma) = %.2f %%\n\n",
           cm_mismatch_total(r_simple.systematic_error, sigma_rand, 3.0));

    /* PSRR */
    printf("=== Power Supply Rejection ===\n");
    double psr_simple = cm_psr_simple(1e-7, r_simple.Rout, 1.8);
    double psr_cascode = cm_psr_cascode(1e-7, r_cascode.Rout, 2e-3, 1.8);
    printf("  PSRR (simple): %.1f dB\n", psr_simple);
    printf("  PSRR (cascode): %.1f dB\n\n", psr_cascode);

    printf("=== Design Complete ===\n");
    printf("Summary: Cascode improves Rout by ~50-100x over simple mirror.\n");
    printf("         Regulated cascode adds another ~50x boost.\n");

    ic_tech_free(tech);
    return 0;
}
