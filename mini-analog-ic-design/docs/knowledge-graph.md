# Knowledge Graph — mini-analog-ic-design

## L1: Definitions (Complete — 19 items)
- MOS types (NMOS/PMOS), BJT types (NPN/PNP)
- Operating regions (cutoff, triode, saturation, subthreshold, velocity saturation)
- Technology parameters (Vth, mu, Cox, lambda, gamma, phi_f, Vdsat)
- Small-signal parameters (gm, gds, gmb, Cgs/Cgd/Cdb/Csb/Cgb, ft, ro, intrinsic_gain)
- BJT small-signal (gm, rpi, ro, rmu, Cpi, Cmu, ft)
- Biasing topologies (resistor divider, current source, self-bias, beta-multiplier, constant-gm, Widlar)
- Amplifier topologies (CS/CE/CG/CB/CD/CC/Cascode/DiffPair)
- Op-amp topologies (two-stage, folded cascode, telescopic, symmetrical OTA, current-mirror)
- Current mirror topologies (simple, cascode, Wilson, regulated, wide-swing, low-voltage, active-feedback)
- Bandgap topologies (Widlar, Brokaw, Kuijk, Banba, subthreshold, curvature-corrected)
- Noise types (thermal, flicker, shot, burst, avalanche)
- Process corners (TT, FF, SS, FS, SF)
- Temperature corners (nominal 25C, cold -40C, hot +125C)
- gm/ID data point structure
- Pelgrom mismatch parameters (Avt, Abeta)
- Physical constants (k, q, epsilon0, epsilon_sio2)
- Bias specifications (Vdd, Id_target, Vdsat_target, power_budget)
- Bandgap Vbe temperature parameters (Vbe0, Vgo, eta, T0)
- 3 technology node initializations (180nm, 65nm, 28nm)

## L2: Core Concepts (Complete — 22 items)
- MOS small-signal extraction from DC OP
- BJT small-signal extraction from DC OP
- MOS operating region detection
- MOS sizing: W/L from Id and Vov
- gm/ID transconductance efficiency
- gm/ID lookup table + binary search Vgs
- Process corner scaling application
- 7 single-transistor amplifier analyses (CS/CG/CD/CE/CB/CC/Cascode)
- 6 current mirror design procedures
- Cascode bias voltage generation
- Current mirror bandwidth calculation
- Unity-gain buffer gain error
- Op-amp Figure of Merit (FoM = GBW*Cload/Power)
- PTAT voltage/current generation
- Vbe temperature dependence
- Bandgap TC calculation (box method)
- Bandgap line regulation and startup time
- Fundamental noise PSD (thermal resistor, MOS drain/gate, flicker, shot)
- SNR and SINAD from noise analysis
- kT/C sampling noise
- Differential pair gm and CMRR
- gm/ID lookup table generation and search

## L3: Mathematical Structures (Complete — 13 items)
- Shichman-Hodges square-law drain current
- Sodini-Ko-Moll velocity saturation model
- Swanson-Meindl subthreshold (weak inversion) current
- EKV unified model (continuous from weak to strong inversion)
- Body effect: Vth vs Vsb (sqrt relationship)
- Miller pole splitting (dominant + non-dominant + RHP zero)
- Open-loop transfer function (A0, p1, p2, z1)
- Current mirror output resistance (simple/cascode/Wilson/regulated)
- Noise bandwidth integration (thermal + flicker)
- Equivalent noise bandwidth (n-pole system)
- Noise correlation (differential-mode vs common-mode)
- Curvature error (2nd-order T*ln(T) term)
- PSRR estimation from op-amp gain

## L4: Fundamental Laws (Complete — 14 items + 14 Lean theorems)
- Shichman-Hodges square law: Id = (1/2)*mu*Cox*(W/L)*(Vgs-Vth)^2
- BJT exponential: Ic = Is*exp(Vbe/Vt)*(1+Vce/VA)
- Pelgrom mismatch: sigma_Vth = Avt/sqrt(W*L)
- Beta-multiplier supply independence
- Widlar equation: Iout*R2 = Vt*ln(Iref/Iout)
- Miller theorem: Cin = Cgs + (1+|Av|)*Cgd
- GBW = gm1/(2*pi*Cc)
- Friis formula: F_total = F1 + (F2-1)/G1 + (F3-1)/(G1*G2)
- kT/C noise: Vn^2 = kT/C
- Bandgap principle: Vref = Vbe + K*Vptat, dVref/dT=0 @ T0
- Johnson-Nyquist: Svv = 4kTR
- Schottky shot noise: Sid = 2qI
- gm/Id fundamental limit: gm/Id_max = 1/(n*Vt) ~ 28 S/A
- Subthreshold swing: SS = n*Vt*ln(10)

## L5: Algorithms/Methods (Complete — 12 items)
- Beta-multiplier reference design
- Widlar current source (Newton-Raphson iteration)
- Pelgrom Monte Carlo mismatch (Box-Muller transform)
- gm/ID lookup table generation + binary search
- Pole-zero placement for target PM
- ICMR calculation
- Output swing calculation
- Noise matching (optimal source impedance)
- Optimal PTAT gain for first-order TC cancellation
- Bandgap trim calculation (binary-weighted)
- Current mirror optimal sizing (minimum mismatch)
- Subthreshold design: inversion coefficient calculation

## L6: Canonical Problems (Complete — 10 items)
- Two-stage Miller op-amp design (8 transistors sized)
- Folded cascode op-amp design
- Telescopic cascode op-amp design
- Widlar bandgap (1971) — first practical BGR
- Brokaw bandgap (1974) — industry standard (AD580, REF01)
- Kuijk bandgap (1973) — CMOS-compatible parasitic PNP
- Banba bandgap (1999) — sub-1V current summation
- Differential pair DC + AC analysis
- Current mirror design comparison (6 topologies)
- LNA noise analysis (inductive degeneration)

## L7: Applications (Complete — 8 items)
- PLL charge pump (voltage step, phase error)
- LDO pass device (dropout voltage, efficiency, power dissipation)
- ADC driver (settling time, GBW requirement)
- CMFB analysis (loop gain, bandwidth, settling error)
- Current mirror PSRR calculation
- Gate driver peak current (Tesla/SpaceX motor drives)
- Hall-effect current sensor (Tesla BMS)
- SC-CMFB, pipeline MDAC, sigma-delta integrator leakage

## L8: Advanced Topics (Complete — 6 items)
- Subthreshold design (swing, gm/Id max, noise PSD, inversion coefficient)
- Chopper stabilization (minimum frequency, residual offset)
- Auto-zeroing (noise PSD, offset reduction)
- Translinear circuit principle (Gilbert cell current product)
- Curvature-corrected bandgap (2nd-order compensation)
- DEM mismatch shaping (sigma improvement by OSR)

## L9: Research Frontiers (Partial — 4 items)
- Cryogenic CMOS (4K operation for quantum computing readout)
- In-memory analog compute (AI accelerator, crossbar arrays)
- Sub-1V bandgap (Banba 1999 implemented; advanced topologies documented)
- ISO 26262 ASIL-D functional safety (TMR analysis for automotive analog)
