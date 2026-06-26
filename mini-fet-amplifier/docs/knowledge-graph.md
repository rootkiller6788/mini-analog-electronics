# Knowledge Graph — FET Amplifier Module

## L1: Definitions

| # | Definition | Type | Status |
|---|-----------|------|--------|
| 1 | FET device types (JFET, MOSFET, MESFET, HEMT, FinFET) | `FetType` enum | COMPLETE |
| 2 | Operating regions (cutoff, triode, saturation, subthreshold) | `FetRegion` enum | COMPLETE |
| 3 | Amplifier topologies (CS, CG, CD, cascode, differential, mirror) | `FetAmpConfig` enum | COMPLETE |
| 4 | Device technology parameters (tox, Cox, μ, Vth, λ, γ, φf) | `FetTechParams` struct | COMPLETE |
| 5 | Device geometry (W, L, W/L, area, perimeter, fingers) | `FetGeometry` struct | COMPLETE |
| 6 | DC operating point (Vgs, Vds, Id, gm, gds, Vov) | `FetDcOpPoint` struct | COMPLETE |
| 7 | Small-signal parameters (gm, rds, Cgs, Cgd, Cds, ft) | `FetSmallSignalParams` struct | COMPLETE |
| 8 | Bias topologies (fixed, self, divider, current source, β-multiplier) | `BiasTopology` enum | COMPLETE |
| 9 | Noise mechanisms (thermal, flicker, induced gate, shot, RTS) | `NoiseMechanism` enum | COMPLETE |
| 10 | Complex admittance (Y-parameters) for two-port | `FetYParams` struct | COMPLETE |
| 11 | Scattering parameters (S-parameters) for RF | `FetSParams` struct | COMPLETE |
| 12 | Noise figure / noise factor definitions | `FetNoiseFigure` struct | COMPLETE |

## L2: Core Concepts

| # | Concept | Implementation | Status |
|---|---------|---------------|--------|
| 1 | FET as voltage-controlled current source | `fet_id_saturation()` | COMPLETE |
| 2 | Channel pinch-off and saturation | `fet_classify_region()` | COMPLETE |
| 3 | Square-law I-V relationship | `fet_id_saturation()`, `fet_id_triode()` | COMPLETE |
| 4 | Transconductance gm = ∂Id/∂Vgs | `fet_gm_saturation()` | COMPLETE |
| 5 | Output conductance gds = ∂Id/∂Vds | `fet_gds_saturation()` | COMPLETE |
| 6 | Body effect — Vth shift with Vsb | `fet_vth_body_effect()` | COMPLETE |
| 7 | Small-signal equivalent circuit (hybrid-π) | `fet_extract_hybrid_pi()` | COMPLETE |
| 8 | T-model for CG/CD analysis | `fet_hybrid_pi_to_tmodel()` | COMPLETE |
| 9 | DC load line analysis | `fet_compute_load_line()` | COMPLETE |
| 10 | Miller effect — Cgd multiplication | `fet_miller_decompose()` | COMPLETE |
| 11 | Cascode: Miller mitigation + Rout enhancement | `fet_octc_cascode()`, `fet_design_cascode_amplifier()` | COMPLETE |
| 12 | Differential pair: balanced operation, CMRR | `fet_diffpair_cmrr()` | COMPLETE |

## L3: Mathematical Structures

| # | Structure | Implementation | Status |
|---|-----------|---------------|--------|
| 1 | Square-law equations (saturation, triode, subthreshold) | `fet_id_saturation()`, etc. | COMPLETE |
| 2 | Hybrid-π small-signal model (complex admittances) | `HybridPiModel`, `fet_extract_hybrid_pi()` | COMPLETE |
| 3 | Y-parameter matrix (complex 2×2) | `FetYParams`, `fet_hp_to_yparams()` | COMPLETE |
| 4 | S-parameter matrix (complex 2×2) | `FetSParams`, `fet_yparams_to_sparams()` | COMPLETE |
| 5 | Rational transfer function (Laplace domain) | `TransferFunction`, `fet_tf_from_pzmap()` | COMPLETE |
| 6 | Bode plot data (magnitude/phase vs frequency) | `BodeData`, `fet_bode_generate()` | COMPLETE |
| 7 | Noise spectral densities (frequency-dependent) | `NoisePSD`, noise calculation functions | COMPLETE |
| 8 | Taylor series for distortion (gm, gm2, gm3) | `fet_distortion_estimate()` | COMPLETE |
| 9 | gm/Id lookup table (design-space mapping) | `GmIdTable`, `fet_design_gmid()` | COMPLETE |

## L4: Fundamental Laws/Theorems

| # | Law/Theorem | Implementation | Verification | Status |
|---|------------|---------------|-------------|--------|
| 1 | Shockley square-law for MOSFET | `fet_id_saturation()` | tests + Lean | COMPLETE |
| 2 | Nyquist thermal noise theorem | `thermal_noise_sv()`, `thermal_noise_si()` | test_thermal_noise | COMPLETE |
| 3 | Miller theorem | `fet_miller_decompose()` | test_miller + Lean theorem | COMPLETE |
| 4 | Friis noise formula for cascaded stages | `fet_cascade_noise_factor()` | test_friis_formula | COMPLETE |
| 5 | Rollett stability criterion (K-factor) | `fet_rollett_stability_k()` | test_stability_factor | COMPLETE |
| 6 | OCTC bandwidth estimation method | `fet_octc_*()` functions | test_octc_* | COMPLETE |
| 7 | Gain-bandwidth trade-off | `fet_gain_bandwidth_product()`, `gbw_identity` (Lean) | tests + Lean | COMPLETE |
| 8 | Noise factor lower bound F ≥ 1 | `noise_factor_lower_bound` (Lean) | Lean proof | COMPLETE |
| 9 | Cascode Rout enhancement theorem | `cascode_enhances_rout` (Lean) | Lean proof | COMPLETE |

## L5: Algorithms/Methods

| # | Algorithm | Implementation | Status |
|---|-----------|---------------|--------|
| 1 | Newton-Raphson bias point solver | `bias_newton_solve()` (static) | COMPLETE |
| 2 | Voltage divider bias design procedure | `fet_bias_voltage_divider()`, `fet_bias_design()` | COMPLETE |
| 3 | Small-signal parameter extraction from DC | `fet_extract_hybrid_pi()` | COMPLETE |
| 4 | Y→S parameter conversion algorithm | `fet_yparams_to_sparams()` | COMPLETE |
| 5 | S→Y parameter conversion algorithm | `fet_sparams_to_yparams()` | COMPLETE |
| 6 | OCTC bandwidth estimation procedure | `fet_octc_common_source()`, etc. | COMPLETE |
| 7 | gm/Id design methodology | `fet_design_gmid()`, `fet_gmid_interpolate()` | COMPLETE |
| 8 | Input-referred noise calculation | `fet_noise_figure_calculate()` | COMPLETE |
| 9 | Integrated noise calculation | `fet_integrated_noise_vrms()` | COMPLETE |
| 10 | Harmonic distortion estimation (power series) | `fet_distortion_estimate()` | COMPLETE |
| 11 | CS amplifier design methodology | `fet_design_cs_amplifier()` | COMPLETE |
| 12 | Cascode amplifier design procedure | `fet_design_cascode_amplifier()` | COMPLETE |

## L6: Canonical Problems

| # | Problem | Solution | Status |
|---|---------|----------|--------|
| 1 | Design CS amplifier for specified gain/BW | `example_cs_amplifier.c` + `fet_design_cs_amplifier()` | COMPLETE |
| 2 | Design source follower for impedance buffering | `example_source_follower.c` + `fet_design_cd_amplifier()` | COMPLETE |
| 3 | Design cascode for wide bandwidth | `example_cascode.c` + `fet_design_cascode_amplifier()` | COMPLETE |
| 4 | Differential pair with specified CMRR | `fet_design_diffpair_amplifier()` | COMPLETE |
| 5 | Current mirror with cascode output | `fet_cascode_mirror_rout()`, `fet_wilson_mirror_rout()` | COMPLETE |
| 6 | Wide-swing cascode bias generation | `fet_wide_swing_cascode_bias()` | COMPLETE |

## L7: Applications

| # | Application | Implementation | Status |
|---|------------|---------------|--------|
| 1 | Guitar pickup preamplifier (Hi-Z CS amp) | `example_cs_amplifier.c` | COMPLETE |
| 2 | MEMS microphone buffer (source follower) | `example_source_follower.c` | COMPLETE |
| 3 | Wideband RF pre-amplifier (cascode) | `example_cascode.c` | COMPLETE |
| 4 | Low-Noise Amplifier (LNA) for GPS | `fet_design_lna()` | COMPLETE |
| 5 | Power Amplifier (PA) for handset | `fet_design_pa()` | COMPLETE |
| 6 | Instrumentation amplifier (ECG front-end) | `fet_design_instrumentation_amp()` | COMPLETE |

## L8: Advanced Topics

| # | Topic | Implementation | Status |
|---|-------|---------------|--------|
| 1 | S-parameter stability analysis (Rollett, μ-factor) | `fet_rollett_stability_k()`, `fet_max_available_gain()` | COMPLETE |
| 2 | Induced gate noise correlation in RF LNAs | `fet_induced_gate_noise_si()` | COMPLETE |
| 3 | gm/Id methodology for submicron design | `GmIdTable`, `fet_design_gmid()` | COMPLETE |
| 4 | Time-varying / temperature-dependent bias analysis | `fet_bias_temperature_drift()` | COMPLETE |
| 5 | Systematic offset in differential pairs | `fet_diffpair_systematic_offset()` | COMPLETE |

## L9: Research Frontiers

| # | Topic | Status | Notes |
|---|-------|--------|-------|
| 1 | FinFET / GAAFET amplifier design | PARTIAL | Device types defined; advanced modeling deferred |
| 2 | Cryogenic FET amplifiers (quantum computing) | PARTIAL | Temperature model exists; noise at 4K not modeled |
| 3 | Subthreshold / weak-inversion amplifier design | PARTIAL | `fet_id_subthreshold()` exists; full design flow deferred |
| 4 | Terahertz FET amplifier (ft/fmax limits) | DOCUMENTED | Documented in course-alignment.md |
