# Course Alignment - mini-bjt-amplifier

## Nine-School Curriculum Mapping

### MIT 6.002
- BJT large-signal model → bjt_ebers_moll_ic
- Operating point → all 6 bias functions
- Small-signal model → bjt_compute_ss_params
- CE/CC amplifiers → bjt_ce_analyze, bjt_cc_analyze

### Berkeley EE105
- Hybrid-pi model → bjt_ss_params_t
- Early effect → bjt_ro
- CB/Cascode → bjt_cb_analyze, bjt_cascode_analyze
- Differential pair → bjt_diff_pair_analyze
- Frequency response → bjt_frequency.c

### Stanford EE101B/EE214
- Two-port parameters → bjt_h_from_ss, bjt_y_from_ss
- Miller theorem → bjt_miller_capacitance
- Cascode design → bjt_cascode_design
- Noise analysis → bjt_noise.c
- Gain-bandwidth → bjt_freq_gbw_product

### Illinois ECE 342
- Bias stability → bjt_bias_beta_sensitivity
- Load-line → bjt_load_line_dc/ac
- Multistage → bjt_multistage_analyze
- Darlington → bjt_darlington_analyze

### Michigan EECS 311
- Temperature effects → bjt_bias_temp_shift
- Power dissipation → bjt_bias_power_dissipation
- Distortion → bjt_harmonic_distortion_hd2/hd3

### Georgia Tech ECE 3042
- SPICE parameters → bjt_spice_params_t
- Junction capacitance → bjt_junction_cap
- Gummel-Poon → bjt_gummel_poon_qb

### TU Munich
- Bias network design → bjt_bias_design_voltage_divider
- Emitter degeneration → bjt_ce_voltage_gain_degenerated

### ETH Zurich
- Precision analog → double precision throughout
- Noise optimization → bjt_noise_optimum_ic
- Stability → bjt_stability_factor

### Tsinghua
- DC/AC analysis → Complete workflow
- Design methodology → bjt_amplifier_design
