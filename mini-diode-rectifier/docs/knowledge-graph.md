# Knowledge Graph -- mini-diode-rectifier

## L1: Definitions (Complete)
| # | Term | C Type | Header |
|---|------|--------|--------|
| 1 | Thermal Voltage V_T = kT/q | #define DIODE_VT_300K | diode_physics.h |
| 2 | Saturation Current I_S | DiodeModelParams.I_S | diode_physics.h |
| 3 | Ideality Factor n | DiodeModelParams.N | diode_physics.h |
| 4 | Series Resistance R_S | DiodeModelParams.R_S | diode_physics.h |
| 5 | Junction Capacitance C_J0 | DiodeModelParams.C_J0 | diode_physics.h |
| 6 | Breakdown Voltage B_V | DiodeModelParams.B_V | diode_physics.h |
| 7 | Built-in Potential V_bi | DiodeModelParams.V_J | diode_physics.h |
| 8 | Bandgap Energy E_G | DiodeModelParams.E_G | diode_physics.h |
| 9 | Forward Voltage Drop V_f | RectifierConfig.V_f | rectifier_topology.h |
| 10 | Peak Inverse Voltage PIV | HalfWaveResult.PIV | rectifier_topology.h |
| 11 | Form Factor | HalfWaveResult.form_factor | rectifier_topology.h |
| 12 | Ripple Factor | HalfWaveResult.ripple_factor | rectifier_topology.h |
| 13 | TUF | HalfWaveResult.TUF | rectifier_topology.h |
| 14 | Rectification Efficiency | HalfWaveResult.efficiency | rectifier_topology.h |
| 15 | Zener Voltage V_Z | ZenerParams.V_Z_nom | zener_regulator.h |
| 16 | Zener Dynamic Impedance Z_Z | ZenerParams.Z_Z | zener_regulator.h |
| 17 | Knee Current I_ZK | ZenerParams.I_ZK | zener_regulator.h |
| 18 | Line/Load Regulation | ZenerRegulatorResult | zener_regulator.h |
| 19 | Reverse Recovery Time t_rr | ReverseRecoveryParams.t_rr | snubber_protection.h |
| 20 | Reverse Recovery Charge Q_rr | ReverseRecoveryParams.Q_rr | snubber_protection.h |
| 21 | Snubber Capacitance/Resistance | RCSnubberResult | snubber_protection.h |
| 22 | Thermal Resistance Rth | ThermalConfig | power_rectifier.h |
| 23 | Junction Temperature T_J | DiodeOpPoint.T_J | diode_physics.h |
| 24 | Conduction Angle | HalfWaveResult.conduction_angle_rad | rectifier_topology.h |
| 25 | Diode Small-Signal Parameters | DiodeSmallSignal | diode_physics.h |
| 26 | TVS Clamping Voltage | TVSParams.V_C | snubber_protection.h |
| 27 | Three-Phase V_ll_rms | ThreePhaseConfig.V_ll_rms | power_rectifier.h |

## L2: Core Concepts (Complete)
20 core concepts implemented across 6 source files:
1. PN Junction Formation (built-in potential, depletion width)
2. Forward/Reverse Bias Conduction (Shockley equation)
3. Avalanche/Zener Breakdown (breakdown physics)
4. Half-Wave Rectification
5. Full-Wave Center-Tapped Rectification
6. Bridge Rectification (Graetz bridge)
7. Capacitor-Input Filtering (Schade 1943 analysis)
8. Choke-Input LC Filtering
9. Pi-Section (CLC) Filtering
10. Zener Shunt Regulation
11. Three-Phase 6-Pulse Rectification
12. SCR Phase-Controlled Rectification
13. Voltage Multiplication (Cockroft-Walton)
14. Diode Reverse Recovery
15. RC/RCD Snubber Protection
16. Thermal Management and Heatsink Design
17. Inrush Current Limiting (NTC, pre-charge)
18. Temperature Effects on Diode Parameters
19. Small-Signal AC Diode Model
20. Synchronous Rectification (MOSFET/GaN)

## L3: Mathematical Structures (Complete)
16 mathematical structures with full implementations:
1. Shockley Equation I_D = I_S*(exp(V_D/(n*V_T))-1)
2. Inverse Shockley V_D = n*V_T*ln(I_D/I_S+1)
3. Thermal Voltage kT/q
4. Junction Capacitance C_j = C_J0/(1-V_D/V_J)^M
5. Diffusion Capacitance C_diff = tau_T*I_D/(n*V_T)
6. Full I-V with Series Resistance (Newton-Raphson)
7. Load Line Intersection (Newton-Raphson)
8. RC Exponential Discharge V(t)=V0*exp(-t/RC)
9. Capacitor Charging Model
10. Zener Linearized V_out Formula
11. Fourier Series for Rectified Waveforms
12. Thermal Resistance Ohm's Law Analogy
13. Capacitor Stored Energy E=CV^2/2
14. Diode Dynamic Impedance Z_Z(I_Z)
15. Cockroft-Walton N-stage Equations
16. Three-Phase Commutation Overlap Model

## L4: Fundamental Laws (Complete)
14 theorems/laws verified in C tests or Lean formalization:
1. Shockley Diode Equation (1949)
2. Boltzmann Relation V_bi = V_T*ln(N_A*N_D/n_i^2)
3. Depletion Approximation from Poisson Equation
4. PIV = V_m (bridge), 2*V_m (FWCT)
5. V_dc = 2*V_m/pi (full-wave ideal)
6. Efficiency bound: eta_max = 8/pi^2 ~ 0.812
7. Ripple frequency: f_r = 2*f_line (full-wave)
8. Energy Conservation: P_out <= P_in
9. Diode State Transition Rules
10. Zener Regulator V_out Formula
11. Capacitor Energy: E = 0.5*C*V^2
12. Second Law: T_junction > T_ambient for P > 0
13. C_min = I_load/(f_ripple*V_r_pp_max)
14. Cockroft-Walton: V_out_no_load = 2*N*V_m

## L5: Algorithms/Methods (Complete)
18 algorithms with documented complexity:
1. Newton-Raphson Load Line Solver
2. SPICE Full I-V Solver with R_S
3. Half-Wave Rectifier Analysis
4. Full-Wave CT Analysis
5. Bridge Rectifier Analysis
6. Voltage Multiplier Analysis
7. SCR Phase-Controlled Analysis
8. Capacitor Filter Ripple (Schade 1943)
9. LC Choke Filter Analysis
10. Pi Filter Analysis
11. Automated Filter Component Selection
12. Zener Regulator Design (Sedra & Smith)
13. RC Snubber Design (McMurray 1972)
14. Reverse Recovery Analysis (Charge Model)
15. RCD Clamp Design
16. Zener Temp-Compensated Reference Design
17. Line/Load Regulation Sweep
18. Temperature Sweep Analysis

## L6: Canonical Problems (Complete)
10 canonical problems solved:
1. Bridge PSU Design from Mains (example_bridge_psu.c)
2. Zener Regulator Design (example_zener_regulator.c)
3. HV Voltage Multiplier (example_voltage_multiplier.c)
4. Three-Phase Industrial Rectifier (example_three_phase_rectifier.c)
5. Diode Load Line Operating Point
6. Filter Capacitor Ripple Estimation
7. Snubber Design for Diode Protection
8. Zener Line/Load Regulation Analysis
9. Transformer Ringing Damper
10. Thermal Analysis with Heatsink Selection

## L7: Applications (Complete — 8 real-world keywords)
Detroit, Toyota, Boeing, NASA, Tesla, iPhone, Fukushima, maglev
All present in source code comments and implementation context.

## L8: Advanced Topics (Complete — 6 topics)
Synchronous rectification, SiC/GaN wide-bandgap, multi-pulse (12/18/24),
lossless snubbers, temperature-dependent parameters, balanced three-phase.

## L9: Research Frontiers (Partial — documented)
6G wireless power transfer rectifiers, ultra-low-voltage energy harvesting,
quantum well diode rectifiers, AI-optimized rectifier topology.