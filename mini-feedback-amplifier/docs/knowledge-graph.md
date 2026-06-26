# Knowledge Graph — mini-feedback-amplifier

## L1: Definitions ✓ COMPLETE

| # | Definition | C Implementation | Lean Definition |
|---|-----------|-----------------|-----------------|
| 1 | Feedback factor β | `FeedbackAmplifier.feedback_factor` | `FeedbackAmpParams.beta` |
| 2 | Open-loop gain A | `FeedbackAmplifier.open_loop_gain` | `FeedbackAmpParams.A` |
| 3 | Loop gain L = Aβ | `feedback_compute_loop_gain()` | `loopGain` |
| 4 | Closed-loop gain A_f | `feedback_compute_closed_loop_gain()` | `closedLoopGain` |
| 5 | Desensitivity D = 1+Aβ | `feedback_compute_desensitivity()` | `desensitivity` |
| 6 | Feedback topology (4 types) | `FeedbackTopology` enum | `FeedbackTopology` inductive |
| 7 | Feedback polarity | `FeedbackPolarity` enum | `FeedbackPolarity` inductive |
| 8 | Phase margin PM | `StabilityMargins.phase_margin_deg` | — |
| 9 | Gain margin GM | `StabilityMargins.gain_margin_db` | — |
| 10 | Input/output impedance | `FeedbackAmplifier.input/output_impedance` | — |
| 11 | Pole-zero map | `PoleZeroMap` struct | — |
| 12 | Transfer function | `TransferFunction` struct | — |

## L2: Core Concepts ✓ COMPLETE

| # | Concept | Implementation |
|---|---------|---------------|
| 1 | Negative feedback principles | `feedback_core.c` — all core computations |
| 2 | Gain desensitization | `feedback_compute_gain_sensitivity()` |
| 3 | Bandwidth extension | `feedback_compute_bandwidth_extension()` |
| 4 | GBW product constancy | `feedback_compute_gbw_product()` |
| 5 | Impedance transformation (all 4 topologies) | `feedback_compute_input/output_impedance()` |
| 6 | Distortion reduction | `feedback_compute_distortion_reduction()` |
| 7 | Series-Shunt topology | `topology_analyze_series_shunt()` |
| 8 | Shunt-Series topology | `topology_analyze_shunt_series()` |
| 9 | Series-Series topology | `topology_analyze_series_series()` |
| 10 | Shunt-Shunt topology | `topology_analyze_shunt_shunt()` |
| 11 | Damping ratio vs PM | `stability_second_order_analysis()` |
| 12 | Minimum-phase systems | `stability_is_minimum_phase()` |
| 13 | Loading effects | `topology_input/output_loading()` |

## L3: Mathematical Structures ✓ COMPLETE

| # | Structure | Implementation |
|---|----------|---------------|
| 1 | Complex transfer function | `TransferFunction` + `feedback_eval_transfer_function()` |
| 2 | Pole-zero analysis | `PoleZeroMap` + `feedback_find_dominant_pole()` |
| 3 | Frequency response vectors | `FrequencyResponse` + `FrequencyPoint` |
| 4 | Bode plot data | `stability_compute_magnitude/phase_at_freq()` |
| 5 | Nyquist contour | `NyquistResult` + `stability_generate_nyquist()` |
| 6 | Routh array | `RouthHurwitzResult` + `stability_routh_hurwitz()` |
| 7 | Root locus | `RootLocus` + `stability_compute_root_locus()` |
| 8 | h-parameter 2-port | `HParameters` struct |
| 9 | Polynomial representation | Coefficient arrays in `TransferFunction` |

## L4: Fundamental Laws ✓ COMPLETE

| # | Theorem/Law | C Verification | Lean Formalization |
|---|------------|---------------|-------------------|
| 1 | Black's feedback formula A_f = A/(1±Aβ) | `feedback_compute_closed_loop_gain()` | `closedLoopGain` + `ideal_feedback_limit` |
| 2 | Bode sensitivity integral | `feedback_bode_sensitivity_integral()` | — |
| 3 | Nyquist stability criterion Z = N + P | `stability_count_encirclements()` | `NyquistResult` + `nyquistMinimumPhase` |
| 4 | Bode stability criterion (PM, GM) | `stability_compute_margins()` | — |
| 5 | Routh-Hurwitz criterion | `stability_routh_hurwitz()` | `routh_hurwitz_quadratic/cubic` |
| 6 | Gain-bandwidth product conservation | `feedback_compute_gbw_product()` | `gbw_product_conservation` |
| 7 | Barkhausen oscillation criterion | — | `barkhausen_condition` |
| 8 | Gain sensitivity formula | `feedback_compute_gain_sensitivity()` | `gain_sensitivity_formula` |

## L5: Algorithms/Methods ✓ COMPLETE

| # | Algorithm | Implementation |
|---|----------|---------------|
| 1 | Phase margin from pole frequencies | `stability_assess_from_poles()` |
| 2 | Nyquist winding number computation | `stability_count_encirclements()` |
| 3 | Routh-Hurwitz array construction | `stability_routh_hurwitz()` |
| 4 | Root locus (2nd/3rd order) | `stability_compute_root_locus()` |
| 5 | Dominant-pole compensation | `comp_design_dominant_pole()` |
| 6 | Miller (pole-splitting) compensation | `comp_design_miller()` |
| 7 | Lead compensation | `comp_design_lead()` |
| 8 | Lag compensation | `comp_design_lag()` |
| 9 | Lead-lag compensation | `comp_design_lead_lag()` |
| 10 | Pole-zero -3dB bandwidth estimation | `feedback_find_dominant_pole()` |
| 11 | Cardano's cubic root formula | In `stability_compute_root_locus()` |
| 12 | Newton iteration for unity-gain frequency | In `stability_assess_from_poles()` |

## L6: Canonical Problems ✓ COMPLETE

| # | Problem | Implementation |
|---|--------|---------------|
| 1 | Non-inverting voltage amplifier design | `topology_design_noninverting_amp()` + `example1` |
| 2 | Transimpedance amplifier design | `topology_design_transimpedance_amp()` + `example3` |
| 3 | Stability analysis of multi-pole amplifier | `example2_stability_compensation.c` |
| 4 | Four-topology comparative analysis | `topology_compare_all()` + `example3` |
| 5 | β-network extraction from resistor divider | `topology_resistive_divider_beta()` |

## L7: Applications ✓ PARTIAL+ (3 applications)

| # | Application | Implementation |
|---|-----------|---------------|
| 1 | Photodiode transimpedance amplifier (TIA) | `topology_design_transimpedance_amp()` |
| 2 | GPS receiver LNA voltage amplifier | `example3` — documented application mapping |
| 3 | Tesla BMS current sensor | `example3` — documented application mapping |
| 4 | OTA-C filter for 5G NR | `example3` — documented application mapping |

## L8: Advanced Topics ✓ PARTIAL+ (1 topic)

| # | Topic | Implementation |
|---|-------|---------------|
| 1 | Nested Miller compensation (3-stage) | `comp_design_nested_miller()` + `comp_is_nmc_applicable()` |

## L9: Research Frontiers ✓ PARTIAL

| # | Topic | Documentation |
|---|-------|-------------|
| 1 | Subthreshold feedback amplifiers | Referenced in course-tree.md |
| 2 | Neuromorphic feedback circuits | Referenced in course-tree.md |
