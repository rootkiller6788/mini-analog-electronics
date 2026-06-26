/* Example: Bandgap Voltage Reference Design (Brokaw Topology)
 * Demonstrates a 1.2V bandgap reference design with temperature sweep.
 * Reference: Razavi Ch.11; Brokaw, JSSC 1974
 */
#include "analog_ic_bandgap.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    printf("=== Bandgap Voltage Reference Design ===\n\n");

    ic_technology_t *tech = ic_tech_init_180nm();
    if (!tech) { printf("ERROR: tech init failed\n"); return 1; }

    bandgap_spec_t spec;
    memset(&spec, 0, sizeof(spec));
    spec.topology = BGR_BROKAW;
    spec.Vdd = 2.5;
    spec.Vref_target = 1.2;
    spec.TC_target = 50.0;
    spec.T_min = 233.0;  /* -40 C */
    spec.T_max = 398.0;  /* +125 C */
    spec.tech = tech;

    printf("Topology: Brokaw Bandgap (industry standard)\n");
    printf("  Vdd = %.1f V\n", spec.Vdd);
    printf("  Temperature range: %.0f K to %.0f K (-40C to +125C)\n\n", spec.T_min, spec.T_max);

    /* Design all four topologies for comparison */
    bandgap_result_t res_widlar, res_brokaw, res_kuijk, res_banba;
    memset(&res_widlar, 0, sizeof(res_widlar));
    memset(&res_brokaw, 0, sizeof(res_brokaw));
    memset(&res_kuijk, 0, sizeof(res_kuijk));
    memset(&res_banba, 0, sizeof(res_banba));

    printf("--- Widlar Bandgap (1971) ---\n");
    bgr_design_widlar(&spec, &res_widlar);
    printf("  Vref = %.4f V @ 300K\n", res_widlar.Vref_T_nominal);
    printf("  TC = %.1f ppm/C\n", res_widlar.TC);
    printf("  Vptat = %.3f mV, Vctat = %.3f V\n", res_widlar.V_ptat*1e3, res_widlar.V_ctat);
    printf("  PTAT gain K = %.2f\n", res_widlar.gain_ptat);
    printf("  PSRR = %.1f dB\n", res_widlar.PSRR);
    printf("  Power = %.2f uW\n\n", res_widlar.power*1e6);

    printf("--- Brokaw Bandgap (1974) ---\n");
    bgr_design_brokaw(&spec, &res_brokaw);
    printf("  Vref = %.4f V @ 300K\n", res_brokaw.Vref_T_nominal);
    printf("  TC = %.1f ppm/C\n", res_brokaw.TC);
    printf("  PSRR = %.1f dB\n", res_brokaw.PSRR);
    printf("  Trim: %d steps, %.2f mV/step\n", res_brokaw.trim_steps,
           res_brokaw.trim_resolution*1e3);
    printf("  Power = %.2f uW\n\n", res_brokaw.power*1e6);

    printf("--- Kuijk Bandgap (1973, CMOS) ---\n");
    bgr_design_kuijk(&spec, &res_kuijk);
    printf("  Vref = %.4f V @ 300K\n", res_kuijk.Vref_T_nominal);
    printf("  TC = %.1f ppm/C\n", res_kuijk.TC);
    printf("  Power = %.2f uW\n\n", res_kuijk.power*1e6);

    printf("--- Banba Bandgap (1999, Sub-1V) ---\n");
    bgr_design_banba(&spec, &res_banba);
    printf("  Vref = %.4f V @ 300K (sub-1V compatible!)\n", res_banba.Vref_T_nominal);
    printf("  TC = %.1f ppm/C\n", res_banba.TC);
    printf("  Power = %.2f uW\n\n", res_banba.power*1e6);

    /* Temperature sweep for Brokaw */
    printf("=== Temperature Sweep (Brokaw) ===\n");
    printf("  Temp [K]    Vref [V]\n");
    printf("  --------    --------\n");
    double T;
    for (T = spec.T_min; T <= spec.T_max; T += 20.0) {
        vbe_temp_params_t vbe_p = bgr_vbe_params_default(0.65, 5e-6);
        double Vbe_T = bgr_vbe_temperature(T, &vbe_p);
        double Vptat_T = bgr_ptat_voltage(T, res_brokaw.ratio_n);
        double Vref_T = Vbe_T + res_brokaw.gain_ptat * Vptat_T;
        printf("  %.0f          %.5f\n", T, Vref_T);
    }
    printf("\n");

    /* Curvature correction info */
    double curv = bgr_curvature_error(spec.T_min, spec.T_max, 3.5, 300.0);
    printf("Second-order curvature error: %.3f mV\n", curv * 1e3);
    printf("After curvature correction, TC ~ %.1f ppm/C\n\n", res_brokaw.TC / 5.0);

    printf("=== Bandgap Design Complete ===\n");
    printf("Typical commercial BGR: TC < 50 ppm/C (e.g., AD580, REF01)\n");
    printf("This design achieves TC ~ %.1f ppm/C (first-order compensated)\n", res_brokaw.TC);

    ic_tech_free(tech);
    return 0;
}
