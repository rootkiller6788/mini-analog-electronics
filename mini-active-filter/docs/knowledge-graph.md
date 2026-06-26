# Knowledge Graph — Mini Active Filter

## L1: Definitions

| Item | C Implementation | Lean Formalization |
|------|-----------------|-------------------|
| `active_topology_t` | 10 topologies (Sallen-Key, MFB, KHN, etc.) | `ActiveTopology` inductive |
| `active_filter_func_t` | 8 filter functions (LP, HP, BP, BS, etc.) | `FilterFunction` inductive |
| `active_approx_t` | 8 approximation types | `ApproxType` inductive |
| `active_filter_spec_t` | Complete design specification | `FilterSpec` structure |
| `active_biquad_section_t` | Second-order TF parameters | `BiquadSection` structure |
| `active_component_values_t` | RC component values | `ComponentValues` structure |
| `active_sensitivity_t` | Sensitivity analysis results | Sensitivity sum theorems |
| `active_filter_t` | Complete filter structure | `ValidActiveFilter` structure |
| `active_freq_point_t` | Single frequency response point | — |
| `active_freq_response_t` | Full frequency sweep | — |
| `active_opamp_params_t` | Op-amp non-ideality model | — |
| `active_tuning_config_t` | Tuning configuration | — |
| `sc_integrator_t` | Switched-capacitor integrator | — |
| `gmc_params_t` | Gm-C OTA parameters | — |
| `mems_resonator_t` | MEMS resonator BVD model | — |

## L2: Core Concepts

| Concept | Implementation |
|---------|---------------|
| Sallen-Key LP/HP/BP/Notch design | `sallen_key_lp_design()`, etc. |
| MFB LP/HP/BP/Notch design | `mfb_lp_design()`, etc. |
| KHN state-variable design | `khn_state_variable_design()` |
| Tow-Thomas biquad design | `tow_thomas_biquad_design()` |
| Akerberg-Mossberg single-amp biquad | `akerberg_mossberg_design()` |
| Cascade/Leapfrog high-order design | `cascade_filter_design()` |
| First-order active section | `cascade_first_order_section()` |

## L3: Mathematical Structures

| Structure | Implementation |
|-----------|---------------|
| Biquad TF evaluation (complex) | `active_biquad_evaluate()` |
| Cascaded TF product | `active_cascade_evaluate()` |
| Frequency response sweep | `active_freq_response_sweep()` |
| Cutoff frequency search | `active_filter_find_cutoff()` |
| Pole normalization/denormalization | `active_normalize_poles()` |
| Transfer function extraction (SK, MFB) | `sallen_key_transfer_function()`, etc. |
| Component value E-series snap | `active_nearest_eseries()` |
| Impedance scaling | `active_impedance_scale()` |
| Order estimation (Butterworth, Chebyshev) | `active_estimate_order_*()` |
| LC ladder prototype values | `cascade_ladder_prototype()` |

## L4: Fundamental Laws

| Theorem | Lean Statement | C Verification |
|---------|---------------|---------------|
| Sensitivity sum invariance (Fialkow-Gerst) | `sensitivity_sum_invariance` | `sensitivity_sum_theorem()` |
| Q sensitivity sum theorem | `q_sensitivity_sum` | `sensitivity_q_sum_theorem()` |
| Sallen-Key Q-gain relationship | `sallen_key_q_gain_relationship` | Verified in SK LP design |
| MFB unconditional stability | `mfb_stability_condition` | Component range check |
| KHN independent tuning (ω₀/Q) | `khn_independent_tuning` | `khn_state_variable_trimmable()` |
| GBW Q-enhancement effect | — | `gbw_effect_on_poles()` |
| Slew rate limitation | — | `slew_rate_max_frequency()` |
| DC offset propagation | — | `dc_offset_propagation()` |
| Routh-Hurwitz for biquad | `routh_hurwitz_biquad` | Stability check |

## L5: Algorithms/Methods

| Algorithm | Implementation |
|-----------|---------------|
| Sallen-Key LP analytical design | `sallen_key_lp_design()` |
| MFB BP Daryanani design method | `mfb_bp_design()` |
| KHN state-variable design | `khn_state_variable_design()` |
| Tow-Thomas equal-C/R design | `tow_thomas_biquad_design()` |
| Cascade pole computation (Butterworth/Chebyshev/Bessel) | `cascade_*_poles()` |
| Optimal section ordering | `cascade_optimal_ordering()` |
| Gain distribution for dynamic range | `cascade_distribute_gain()` |
| Complete cascade design pipeline | `cascade_filter_design()` |
| Monte Carlo tolerance analysis | `monte_carlo_tolerance()` |
| Worst-case corner analysis | `worst_case_analysis()` |
| E-series component snapping | `sallen_key_snap_to_eseries()` |
| MFB sensitivity optimization | `mfb_optimize_sensitivity()` |

## L6: Canonical Problems

| Problem | Example |
|---------|---------|
| Butterworth Sallen-Key LP design | `ex1_sallen_key_lpf.c` |
| High-Q MFB bandpass for tone detection | `ex2_mfb_bandpass.c` |
| 2-way Linkwitz-Riley audio crossover | `ex3_audio_crossover.c` |
| ECG biomedical signal conditioning chain | `ex4_ecg_filter.c` |
| Cascade verification against spec | `cascade_verify()` |

## L7: Applications

| Application | Keywords/Implementation |
|-------------|------------------------|
| **Audio crossover** (Linkwitz-Riley) | `ex3_audio_crossover.c` — loudspeaker systems |
| **ECG/medical filter** | `ex4_ecg_filter.c` — DC block + anti-alias + notch |
| **Switched-capacitor anti-alias** | `sc_alias_analysis()` — data acquisition |
| **N-path RF BP filter** | `npath_bp_design()` — software-defined radio |
| **Gm-C auto-tuned filter** | `gmc_autotune_design()` — disk drive read channels |
| **MEMS resonator interface** | `mems_filter_interface()` — ultra-narrowband filtering |

## L8: Advanced Topics

| Topic | Implementation |
|-------|---------------|
| Switched-capacitor integrator/biquad | `sc_integrator_design()`, `sc_biquad_design()` |
| Gm-C transconductance filter | `gmc_integrator_design()`, `gmc_biquad_design()` |
| Gm-C harmonic distortion | `gmc_thd_estimate()` |
| Gm-C on-chip automatic tuning | `gmc_autotune_design()` |
| N-path commutated BP filter | `npath_bp_design()` |
| Leapfrog LC ladder simulation | `cascade_leapfrog_design()` |
| Sensitivity Jacobian optimization | `sensitivity_jacobian()` |

## L9: Research Frontiers

| Topic | Documentation |
|-------|--------------|
| MEMS resonator-based filters (Q > 10,000) | `mems_resonator_t`, `mems_filter_interface()` |
| N-path high-Q RF filters (clock-tunable) | `npath_bp_design()` |
| Gm-C GHz-range integrated filters | `gmc_biquad_design()` |
| Auto-calibration for Gm-C PVT compensation | `gmc_autotune_design()` |
| Switched-capacitor biquad cascade in CMOS | `sc_biquad_design()` |
