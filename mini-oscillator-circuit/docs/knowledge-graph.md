# Knowledge Graph — mini-oscillator-circuit

## L1: Definitions ✓ Complete

| # | Definition | C Type / Enum | Status |
|---|-----------|---------------|--------|
| 1 | Oscillation frequency f_osc | `oscillator_params_t.target_freq_hz` | ✓ |
| 2 | Barkhausen criterion | `barkhausen_criterion_t` | ✓ |
| 3 | Loop gain magnitude and phase | `loop_gain_sweep_t` | ✓ |
| 4 | Quality factor Q | `quality_factor_t` | ✓ |
| 5 | Duty cycle | `relaxation_555_t.duty_cycle_percent` | ✓ |
| 6 | Amplitude Vpp | `oscillator_params_t.amplitude_vpp` | ✓ |
| 7 | Start-up gain margin | `barkhausen_criterion_t.start_up_margin_db` | ✓ |
| 8 | Phase noise L(Deltaf) dBc/Hz | `phase_noise_t` | ✓ |
| 9 | Negative resistance | `cross_coupled_lc_osc_t.r_neg_ohms` | ✓ |
| 10 | Oscillator topology enum | `oscillator_type_t` (16 types) | ✓ |
| 11 | Frequency stability | `frequency_stability_t` | ✓ |
| 12 | Allan variance | `allan_variance_t` | ✓ |
| 13 | Jitter metrics | `jitter_metrics_t` | ✓ |
| 14 | THD | Computed by `thd_compute()` | ✓ |
| 15 | Crystal equivalent circuit | `crystal_model_t` | ✓ |
| 16 | Varactor C-V model | `varactor_t` | ✓ |
| 17 | VCO gain K_VCO | `lc_vco_t.kvco_avg_hz_per_v` | ✓ |
| 18 | PLL lock range | `pll_system_t` | ✓ |

## L2: Core Concepts ✓ Complete

| # | Concept | Implementation | Status |
|---|---------|---------------|--------|
| 1 | Positive feedback | Barkhausen phase condition evaluation | ✓ |
| 2 | Frequency-selective networks (RC/LC/crystal) | `oscillator_rc.h`, `oscillator_lc.h`, `oscillator_crystal.h` | ✓ |
| 3 | Amplitude stabilization | THD estimation, Wien bridge limiting | ✓ |
| 4 | Start-up condition | `barkhausen_startup_check()` | ✓ |
| 5 | Steady-state oscillation | Van der Pol limit cycle | ✓ |
| 6 | Frequency pulling/pushing | `lc_oscillator_pulling()`, `crystal_pullability_ppm()` | ✓ |
| 7 | Injection locking | Defined as `OSC_MODE_INJECTION_LOCKED` | ✓ |
| 8 | Relaxation vs harmonic oscillation | Two distinct oscillator families | ✓ |
| 9 | Negative resistance (LC) | Cross-coupled pair analysis | ✓ |
| 10 | PLL frequency synthesis | `charge_pump_pll_design()` | ✓ |

## L3: Mathematical Structures ✓ Complete

| # | Structure | Implementation | Status |
|---|----------|---------------|--------|
| 1 | Van der Pol equation | `van_der_pol_params_t` + RK4 solver | ✓ |
| 2 | Phase space / limit cycle | `van_der_pol_sim_t` trajectory | ✓ |
| 3 | s-domain transfer functions | `rc_ladder_transfer()`, `wien_bridge_transfer()` | ✓ |
| 4 | LC tank impedance | `lc_tank_analyze()` | ✓ |
| 5 | Crystal impedance | `crystal_impedance()` (BVD model) | ✓ |
| 6 | Exponential RC charging | `relaxation_555_t`, `relaxation_schmitt_t` | ✓ |
| 7 | PLL transfer function H(s) | `pll_transfer_magnitude()` | ✓ |
| 8 | Varactor C-V curve | `varactor_capacitance()` | ✓ |
| 9 | Complex pole analysis | Implicit in Barkhausen verification | ✓ |

## L4: Fundamental Laws ✓ Complete

| # | Theorem / Law | Formula | Verification | Status |
|---|--------------|---------|-------------|--------|
| 1 | Barkhausen criterion | |AB|=1, angle AB=0 | `barkhausen_evaluate()` + C tests | ✓ |
| 2 | RC phase-shift freq | f=1/(2pi R C sqrt(6)) | Tested for 1 kHz design | ✓ |
| 3 | Wien bridge freq | f=1/(2pi R C) | Tested via transfer function at f0 | ✓ |
| 4 | Colpitts freq | f=1/(2pi sqrt(L C_eq)), C_eq=C1C2/(C1+C2) | Tested for 10 MHz | ✓ |
| 5 | Hartley freq | f=1/(2pi sqrt(L_total C)) | Tested for 1 MHz | ✓ |
| 6 | 555 astable freq | f=1.44/((R1+2R2)C) | Tested for 1 kHz | ✓ |
| 7 | Crystal series resonance | f_s=1/(2pi sqrt(L1C1)) | Crystal model validated | ✓ |
| 8 | Crystal parallel resonance | f_p=f_s sqrt(1+C1/C0) | Model validated | ✓ |
| 9 | Leeson phase noise | L(Deltaf)=10log[(FkT/2Ps)(1+(f0/2QDeltaf)^2)(1+fc/Deltaf)] | `leeson_phase_noise_compute()` | ✓ |
| 10 | Hajimiri LTV model | L(Deltaomega)=10log(Gamma^2_rms i^2_n/(2q^2_max Deltaomega^2)) | `hajimiri_phase_noise_compute()` | ✓ |
| 11 | Crystal pullability | Deltaf/fs = C1/(2(C0+CL)) | `crystal_pullability_ppm()` | ✓ |
| 12 | Nyquist stability (PLL) | Phase margin > 0 | `pll_stability_check()` | ✓ |
| 13 | PLL Type-II zero error | Steady-state phase error = 0 | Stated in Lean | ✓ |
| 14 | Parseval-like energy | Implicit in THD computation | ✓ |

## L5: Algorithms/Methods ✓ Complete

| # | Algorithm/Method | Implementation | Complexity | Status |
|---|-----------------|---------------|-----------|--------|
| 1 | Runge-Kutta 4 integration | `van_der_pol_simulate()` | O(N) time | ✓ |
| 2 | Leeson phase noise computation | `leeson_phase_noise_compute()` | O(1) | ✓ |
| 3 | Hajimiri LTV phase noise | `hajimiri_phase_noise_compute()` | O(1) | ✓ |
| 4 | Bode analysis with crossover finding | `bode_analyze()`, `bode_find_crossings()` | O(N) | ✓ |
| 5 | THD from harmonic amplitudes | `thd_compute()` | O(N) | ✓ |
| 6 | Allan variance computation | `allan_variance_compute()` | O(M) | ✓ |
| 7 | Phase noise to jitter integration | `phase_noise_to_rms_jitter()` | O(N) | ✓ |
| 8 | Oscillator design methodology | Design functions for all 16 topologies | O(1) | ✓ |
| 9 | Charge-pump PLL design | `charge_pump_pll_design()` | O(1) | ✓ |
| 10 | VCO tuning curve computation | `lc_vco_tuning_curve()` | O(N) | ✓ |
| 11 | Frequency stability analysis | `frequency_stability_analyze()` | O(1) | ✓ |
| 12 | Tolerance analysis | `oscillator_tolerance_analyze()` | O(1) | ✓ |

## L6: Canonical Problems ✓ Complete

| # | Problem | Implementation | Example | Status |
|---|---------|---------------|---------|--------|
| 1 | 1 kHz Wien bridge oscillator | `wien_bridge_design(1000)` | `example_wien_bridge.c` | ✓ |
| 2 | 10 MHz Colpitts oscillator | `colpitts_design(10e6)` | `example_colpitts.c` | ✓ |
| 3 | 16 MHz Pierce crystal oscillator | `pierce_design(16e6)` | `example_crystal.c` | ✓ |
| 4 | 1 kHz 555 timer astable | `relaxation_555_design(1000)` | `example_555_relaxation.c` | ✓ |
| 5 | 1 GHz PLL frequency synthesizer | `charge_pump_pll_design(10e6,1e9)` | `example_pll_vco.c` | ✓ |
| 6 | 10 kHz RC phase-shift oscillator | `rc_phase_shift_design(10000)` | Tested | ✓ |
| 7 | 2-3 GHz LC VCO | `lc_vco_design(2e9,3e9)` | Tested | ✓ |
| 8 | Cross-coupled 5 GHz LC oscillator | `cross_coupled_lc_design(5e9)` | Tested | ✓ |

## L7: Applications ✓ Partial+

| # | Application | Keywords | Implementation | Status |
|---|------------|----------|---------------|--------|
| 1 | GPS L1 1.575 GHz VCO | GPS | `lc_vco_design()` with GPS frequency | ✓ |
| 2 | MCU clock oscillator (16 MHz) | MCU | `pierce_design(16e6)` example | ✓ |
| 3 | Frequency synthesizer | PLL | `charge_pump_pll_design()` | ✓ |
| 4 | Audio tone generator (Wien bridge) | Audio | `example_wien_bridge.c` | ✓ |
| 5 | RF carrier generation (Colpitts) | RF | `example_colpitts.c` | ✓ |

## L8: Advanced Topics ✓ Partial+

| # | Topic | Implementation | Status |
|---|-------|---------------|--------|
| 1 | PLL phase noise analysis | `charge_pump_pll_phase_noise()` | ✓ |
| 2 | Hajimiri LTV phase noise model | `hajimiri_phase_noise_compute()` | ✓ |
| 3 | Van der Pol limit cycle analysis | RK4 simulation + phase space | ✓ |
| 4 | Charge-pump PLL with loop filter optimization | `pll_loop_filter_design()` | ✓ |
| 5 | VCO tuning linearity optimization | Varactor grading coefficient selection | ✓ |
| 6 | Injection locking | Defined but not fully simulated | Partial |

## L9: Research Frontiers ✓ Partial

| # | Topic | Status | Notes |
|---|-------|--------|-------|
| 1 | MEMS oscillator comparison | Partial | Documented, crystal Q comparison |
| 2 | NEMS oscillator scaling | Partial | Documented in gap-report |
| 3 | Quantum-limited phase noise | Partial | Leeson floor = quantum limit |
| 4 | Optoelectronic oscillators | Partial | Documented in course-tree |
| 5 | 6G THz oscillator challenges | Partial | Documented in gap-report |
