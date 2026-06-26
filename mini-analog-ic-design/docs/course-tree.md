# Course Tree — mini-analog-ic-design

## Prerequisites
- Circuit analysis (KVL, KCL, nodal analysis)
- Basic semiconductor physics (PN junction, MOS capacitor)
- Small-signal modeling concepts
- Laplace/frequency-domain analysis

## Module Dependency Tree
```
mini-analog-ic-design
  |
  +-- Device Models (L1)
  |     +-- MOS: Vth, mu, Cox, lambda, gamma
  |     +-- BJT: Is, beta, VA
  |     +-- Physical constants: k, q, Vt = kT/q
  |
  +-- DC Operating Point (L2)
  |     +-- Drain current models (L3: square law, velocity sat, subthreshold, EKV)
  |     +-- Region detection
  |     +-- Body effect
  |
  +-- Small-Signal Parameters (L2)
  |     +-- gm, gds, gmb extraction
  |     +-- Capacitance modeling
  |     +-- ft, intrinsic gain
  |
  +-- Single-Transistor Amplifiers (L2, L6)
  |     +-- CS/CE: high gain, inverting
  |     +-- CG/CB: wideband, low Rin
  |     +-- CD/CC: buffer, Av~1
  |     +-- Cascode: high gain, high Rout
  |
  +-- Differential Pairs (L6)
  |     +-- DC bias solution
  |     +-- Ad, Acm, CMRR
  |     +-- ICMR, systematic offset
  |
  +-- Current Mirrors (L2, L6)
  |     +-- Simple, Cascode, Wilson
  |     +-- Regulated, Wide-Swing, Low-Voltage
  |     +-- Mismatch, PSRR
  |
  +-- Operational Amplifiers (L6)
  |     +-- Two-Stage Miller
  |     +-- Folded Cascode
  |     +-- Telescopic
  |     +-- CMFB (L7)
  |
  +-- Frequency Compensation (L3-L5)
  |     +-- Miller pole splitting
  |     +-- Phase margin calculation
  |     +-- Pole-zero placement
  |     +-- Slew rate analysis
  |
  +-- Noise Analysis (L2-L5)
  |     +-- Thermal, flicker, shot noise
  |     +-- Input-referred noise
  |     +-- Friis formula
  |     +-- Noise matching, LNA
  |
  +-- Biasing Circuits (L4)
  |     +-- Beta-multiplier
  |     +-- Widlar current source
  |     +-- Supply-independent bias
  |
  +-- Bandgap References (L4, L6)
  |     +-- PTAT + CTAT voltage generation
  |     +-- Widlar, Brokaw, Kuijk topologies
  |     +-- Banba sub-1V topology
  |     +-- TC calculation, trimming
  |
  +-- Advanced Topics (L7-L9)
        +-- PLL charge pump, LDO, ADC driver
        +-- Subthreshold, Chopper, Auto-zero
        +-- Curvature BGR, DEM, Translinear
        +-- Gate driver, Hall sensor (Tesla, ISO 26262)
        +-- Cryo-CMOS, In-memory compute (L9)
```

## Downstream Modules
- mini-opamp-application: Requires op-amp specs from this module
- mini-feedback-amplifier: Requires amplifier analysis
- mini-oscillator-circuit: Requires biasing and mirror knowledge
