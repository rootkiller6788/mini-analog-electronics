/**
 * @file    example_555_relaxation.c
 * @brief   Design a 1 kHz 555 timer astable oscillator
 */
#include "oscillator_core.h"
#include "oscillator_relaxation.h"
#include <stdio.h>
int main(void){
printf("=== 555 Timer Astable Oscillator ===\n\n");
relaxation_555_t osc=relaxation_555_design(1000,60);
printf("Target: 1 kHz, 60%% duty cycle\n");
printf("f=%.2f Hz, D=%.1f %%\n",osc.freq_hz,osc.duty_cycle_percent);
printf("R1=%.2f kOhm, R2=%.2f kOhm, C=%.2f uF\n",
  osc.r1_ohms/1000,osc.r2_ohms/1000,osc.c_farads*1e6);
printf("t_high=%.4f ms, t_low=%.4f ms, T=%.4f ms\n",
  osc.t_high_sec*1e3,osc.t_low_sec*1e3,osc.period_sec*1e3);
printf("V_th+: %.2f V, V_th-: %.2f V\n",osc.v_high_threshold,osc.v_low_threshold);
printf("Design valid: %s\n",relaxation_555_validate(&osc,5)?"YES":"NO");
return 0;
}
