/**
 * @file    example_pll_vco.c
 * @brief   Design a 1 GHz PLL frequency synthesizer with LC VCO
 */
#include "oscillator_core.h"
#include "oscillator_pll.h"
#include "oscillator_vco.h"
#include <stdio.h>
int main(void){
printf("=== PLL Synthesizer + VCO Design ===\n\n");
charge_pump_pll_t pll=charge_pump_pll_design(10e6,1e9,500e3,50,100e6,100e-6);
printf("PLL: f_ref=%.0f MHz, f_out=%.0f MHz, N=%d\n",
  pll.system.ref_freq_hz/1e6,pll.system.output_freq_hz/1e6,pll.system.n_divider);
printf("Loop BW=%.0f kHz, PM=%.0f deg, zeta=%.3f\n",
  pll.system.loop_bandwidth_hz/1e3,pll.system.phase_margin_deg,pll.system.damping_factor);
printf("Filter: C1=%.2f nF, R2=%.2f kOhm, C2=%.2f pF\n",
  pll.filter.c_farads*1e9,pll.filter.r2_ohms/1000,pll.filter.c2_farads*1e12);
printf("Lock time: %.2f us, Stable: %s\n",
  pll.system.lock_time_us,pll_stability_check(&pll.system)?"YES":"NO");
double pn10k=charge_pump_pll_phase_noise(&pll,10e3);
double pn1M=charge_pump_pll_phase_noise(&pll,1e6);
printf("PN@10kHz: %.1f dBc/Hz, PN@1MHz: %.1f dBc/Hz\n",pn10k,pn1M);
printf("\n--- VCO for this PLL ---\n");
lc_vco_t vco=lc_vco_design(0.95e9,1.05e9,3.3,15);
printf("VCO: f_center=%.0f MHz, Range=%.1f%%, Kvco=%.0f MHz/V\n",
  vco.freq_center_hz/1e6,vco.tuning_range_percent,vco.kvco_avg_hz_per_v/1e6);
printf("PN@1MHz: %.1f dBc/Hz, FOM=%.1f dBc/Hz\n",
  vco.phase_noise_at_1mhz,vco.figure_of_merit);
printf("Inductor: %.2f nH, C_fixed: %.2f pF\n",
  vco.inductance_h*1e9,vco.c_fixed_farads*1e12);
return 0;
}
