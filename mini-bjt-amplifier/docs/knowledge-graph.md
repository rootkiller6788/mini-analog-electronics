# Knowledge Graph - mini-bjt-amplifier

## L1: Definitions (Complete)

| Term | Struct/Enum |
|------|------------|
| BJT Operating Regions | bjt_region_t |
| BJT Polarity (NPN/PNP) | bjt_polarity_t |
| SPICE Model Parameters | bjt_spice_params_t |
| DC Q-point | bjt_qpoint_t |
| Small-Signal Parameters | bjt_ss_params_t |
| h-Parameters | bjt_h_params_t |
| Bias Configurations (6 types) | bjt_bias_type_t |
| Amplifier Topologies (6 types) | bjt_amp_topology_t |
| Amplifier Metrics | bjt_amp_metrics_t |
| Two-Port Parameters (z/y) | bjt_z_params_t, bjt_y_params_t |
| Noise Metrics | bjt_noise_metrics_t |
| Transfer Function | bjt_transfer_function_t |

## L2: Core Concepts (Complete)

- DC Biasing (6 configurations)
- Small-Signal AC Analysis (CE, CC, CB)
- Load Line Analysis (DC + AC)
- Coupling/Bypass Capacitor Effects
- Frequency Response (low + high)
- Noise Analysis (thermal, shot, flicker)

## L3: Mathematical Structures (Complete)

- Hybrid-pi Model (gm, rpi, ro, Cpi, Cmu)
- T-Model (re, alpha)
- h/y/z-Parameter Two-Port Models
- Gummel-Poon Charge Control Model
- Transfer Function (pole-zero representation)
- Bode Plot Generation

## L4: Fundamental Laws (Complete)

| Law | Function | Verified |
|-----|----------|----------|
| Shockley Diode Equation | bjt_ebers_moll_ic | test |
| gm = IC/VT | bjt_gm | test: 38.7mS at 1mA |
| Early Effect ro = VA/IC | bjt_ro | test: 100k at VA=100V,IC=1mA |
| Miller Theorem | bjt_miller_capacitance | test: 202pF = 2pF*101 |
| Johnson-Nyquist Noise | bjt_noise_thermal_voltage | test: 0.9nV/rtHz at 50ohm |
| Schottky Shot Noise | bjt_noise_shot | test: 18pA/rtHz at 1mA |
| Alpha-Beta | bjt_alpha_from_beta | test: 0.9901 for beta=100 |
| Impedance Reflection | bjt_ce_input_impedance_degenerated | test |
| GBW Constancy | bjt_freq_gbw_product | test: 100MHz |

## L5: Algorithms/Methods (Complete)

- DC Bias Point Iteration (all 6 bias types)
- Small-Signal Gain/Impedance Calculation
- Thevenin Equivalent Reduction
- OCTC Method (high-frequency estimation)
- SCTC Method (low-frequency estimation)
- Noise Figure Calculation
- Bode Plot Generation
- Binary Search Cut-off Frequency
- Dominant Pole Approximation
- Automated Bias Network Design
- Complete Amplifier Design Flow

## L6: Canonical Problems (Complete)

- CE Amplifier (example_ce_amplifier.c)
- CC Emitter Follower (example_cc_amplifier.c)
- CB Amplifier
- Cascode Amplifier (example_cascode.c)
- Differential Pair
- Darlington Pair
- Multistage Cascade
- Load Line Analysis

## L7: Applications (Partial+, 5 apps)

- Audio Preamplifier (CE, 20Hz-20kHz)
- Sensor Buffer (CC, impedance matching)
- RF/IF Amplifier (Cascode, wideband)
- Instrumentation Front-End (Differential pair)
- Low-Noise Amplifier (Noise optimization)

## L8: Advanced Topics (Partial+, 5 topics)

- Temperature-Compensated Biasing
- Beta Sensitivity Analysis
- Distortion Analysis (HD2, HD3, THD)
- Noise Matching (Ropt, NFmin)
- Low-Noise Design Optimization

## L9: Research Frontiers (Documented)

- SiGe HBT (fT > 500 GHz)
- GaAs HBT (wide bandgap)
- THz Amplifiers
- Cryogenic BJT Amplifiers (4K)
