/**
 * @file    example_colpitts.c
 * @brief   Design a 10 MHz Colpitts oscillator for RF
 */
#include "oscillator_core.h"
#include "oscillator_lc.h"
#include <stdio.h>
int main(void){
printf("=== Colpitts Oscillator Design ===\n\n");
colpitts_osc_t osc=colpitts_design(10e6,0.3,0);
printf("Target: 10.00 MHz, Actual: %.3f MHz\n",osc.freq_osc_hz/1e6);
printf("L=%.3f uH\n",osc.inductance_h*1e6);
printf("C1=%.1f pF, C2=%.1f pF, Ceq=%.2f pF\n",
  osc.c1_farads*1e12,osc.c2_farads*1e12,osc.c_eq_farads*1e12);
printf("Feedback ratio: %.3f\n",osc.feedback_ratio);
printf("Min gm: %.3f mS, Bias: %.2f mA\n",osc.gm_min_siemens*1e3,osc.bias_current_ma);
barkhausen_criterion_t b=colpitts_verify_barkhausen(&osc);
printf("Barkhausen: Mag=%.4f, Phase=%.1f deg\n",b.loop_gain_magnitude,b.loop_phase_deg);
lc_tank_t t=lc_tank_analyze(osc.inductance_h,osc.c_eq_farads,5,0);
printf("Tank: Q=%.1f, f_res=%.2f MHz\n",t.q_unloaded,t.resonant_freq_hz/1e6);
return 0;
}
