/**
 * @file    example_wien_bridge.c
 * @brief   Design a 1 kHz Wien bridge oscillator
 */
#include "oscillator_core.h"
#include "oscillator_rc.h"
#include <stdio.h>
int main(void){
printf("=== Wien Bridge Oscillator Design ===\n\n");
wien_bridge_osc_t osc=wien_bridge_design(1000,0);
printf("Target: 1000 Hz, Actual: %.2f Hz\n",osc.freq_osc_hz);
printf("R=%.2f kOhm, C=%.2f nF\n",osc.r_series_ohms/1000,osc.c_series_farads*1e9);
printf("Beta=%.4f (should be 0.3333)\n",osc.beta_attenuation);
printf("Gain needed: %.2f\n",osc.amp_gain);
barkhausen_criterion_t b=wien_bridge_verify_barkhausen(&osc);
printf("Barkhausen: Mag=%s, Phase=%s, Margin=%.2f dB\n",
  b.magnitude_satisfied?"OK":"FAIL",b.phase_satisfied?"OK":"FAIL",b.start_up_margin_db);
return 0;
}
