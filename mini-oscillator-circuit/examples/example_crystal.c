/**
 * @file    example_crystal.c
 * @brief   Design a 16 MHz Pierce crystal oscillator for MCU
 */
#include "oscillator_core.h"
#include "oscillator_crystal.h"
#include <stdio.h>
int main(void){
printf("=== Pierce Crystal Oscillator (MCU Clock) ===\n\n");
pierce_osc_t osc=pierce_design(16e6,18e-12,3.3);
printf("Nominal: 16.000 MHz\n");
printf("fs=%.6f MHz, fp=%.6f MHz, Q=%.0f\n",
  osc.crystal.fs_hz/1e6,osc.crystal.fp_hz/1e6,osc.crystal.q_factor);
printf("L1=%.1f mH, C1=%.3f fF, R1=%.1f Ohm\n",
  osc.crystal.l1_henries*1e3,osc.crystal.c1_farads*1e15,osc.crystal.r1_ohms);
printf("C_load=%.1f pF (C1=%.1f, C2=%.1f, C_stray=%.1f)\n",
  osc.c_load_farads*1e12,osc.c1_farads*1e12,osc.c2_farads*1e12,osc.c_stray_farads*1e12);
printf("f_osc=%.6f MHz, Pull=%.1f ppm\n",osc.freq_osc_hz/1e6,osc.freq_pull_ppm);
printf("R_neg=%.1f kOhm, Drive=%.2f uW (max 100 uW)\n",
  -osc.r_neg_ohms/1000,osc.drive_level_uw);
printf("Start-up: %.2f ms\n",crystal_startup_time(&osc)*1e3);
printf("Drive OK: %s, Barkhausen: %s\n",osc.drive_level_ok?"YES":"NO",
  pierce_verify_barkhausen(&osc).magnitude_satisfied?"OK":"FAIL");
return 0;
}
