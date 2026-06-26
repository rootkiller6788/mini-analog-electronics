# Course Tree — mini-feedback-amplifier

## Prerequisites (What You Must Know First)

```
mini-circuit-analysis/mini-dc-ac-circuit/
  └── Ohm's Law, KVL, KCL, Thevenin/Norton equivalents
mini-circuit-analysis/mini-two-port-network/
  └── h-parameters, y-parameters, two-port interconnections
mini-circuit-analysis/mini-frequency-response/
  └── Bode plots, poles and zeros, transfer functions
mini-signal-system-theory/mini-fourier-analysis/
  └── Frequency-domain analysis, s-domain (Laplace)
mini-analog-electronics/mini-bjt-amplifier/
  └── Transistor amplifier basics, gain stages
mini-analog-electronics/mini-opamp-application/
  └── Basic op-amp circuits (co-requisite)
```

## This Module's Knowledge Tree

```
mini-feedback-amplifier/
├── L1: Definitions
│   ├── Feedback factor β, loop gain L, desensitivity D
│   ├── Four feedback topologies (Series-Shunt, etc.)
│   └── Stability margins (PM, GM)
│
├── L2: Core Concepts
│   ├── Black's feedback formula: A_f = A/(1+Aβ)
│   ├── Bandwidth extension: f_cl = f_op · D
│   ├── Impedance transformation (×D or ÷D)
│   ├── Distortion reduction: HD_k ∝ 1/D^k
│   └── Gain sensitivity: S = 1/D
│
├── L3: Math Structures
│   ├── Complex transfer functions T(s)
│   ├── Pole-zero maps in s-plane
│   ├── Frequency response vectors
│   └── Routh array construction
│
├── L4: Fundamental Laws
│   ├── Black's formula (1927)
│   ├── Nyquist stability criterion (1932)
│   ├── Bode stability criterion (1945)
│   ├── Routh-Hurwitz criterion (1874/1895)
│   ├── Bode sensitivity integral (waterbed effect)
│   └── Gain-bandwidth product constancy
│
├── L5: Algorithms/Methods
│   ├── Phase/gain margin computation
│   ├── Winding number (Nyquist encirclement count)
│   ├── Routh table construction
│   ├── Root locus computation (2nd/3rd order)
│   ├── Dominant-pole compensation design
│   ├── Miller pole-splitting design
│   ├── Lead/lag compensation design
│   └── Newton iteration for crossover frequencies
│
├── L6: Canonical Problems
│   ├── Non-inverting op-amp amplifier design
│   ├── Transimpedance amplifier (photodiode) design
│   ├── Stability analysis of multi-pole feedback amp
│   └── Four-topology comparison for application selection
│
├── L7: Applications
│   ├── Photodiode TIA (optical receivers)
│   ├── GPS LNA (wireless receivers)
│   ├── Tesla BMS current sensing
│   └── 5G NR OTA-C filter
│
├── L8: Advanced Topics
│   └── Nested Miller compensation (3-stage op-amps)
│
└── L9: Research Frontiers
    ├── Subthreshold feedback amplifiers
    ├── Neuromorphic feedback circuits
    └── Quantum-limited amplifiers
```

## Postrequisites (Modules That Depend On This)

```
mini-analog-electronics/mini-analog-ic-design/
  └── Two-stage op-amp design, compensation, noise analysis
mini-analog-electronics/mini-active-filter/
  └── Feedback-stabilized active filter design
mini-electronic-info/mini-wireless-mobile-comm/
  └── LNA design, RF feedback amplifiers
mini-electronic-info/mini-sensor-measurement/
  └── Transimpedance amplifiers for photodetectors
mini-control-automation/
  └── Control theory: identical concepts (loop gain, PM, GM)
```
