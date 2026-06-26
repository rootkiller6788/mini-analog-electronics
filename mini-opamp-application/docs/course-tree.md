# Course Tree — mini-opamp-application

## Prerequisite Dependency Tree

```
mini-opamp-application (OP-AMP APPLICATIONS)
│
├── [PREREQ] mini-circuit-analysis / mini-dc-ac-circuit
│   └── Kirchhoff's laws, nodal analysis, superposition
│
├── [PREREQ] mini-circuit-analysis / mini-network-theorem
│   └── Thevenin/Norton equivalents, source transformations
│
├── [PREREQ] mini-circuit-analysis / mini-frequency-response
│   └── Bode plots, s-domain analysis, transfer functions
│
├── [PREREQ] mini-analog-electronics / mini-bjt-amplifier
│   └── Transistor amplifier stages, biasing, small-signal models
│
├── [PREREQ] mini-analog-electronics / mini-feedback-amplifier
│   └── Feedback theory, loop gain, stability concepts
│
├── [CORE] opamp_basic (L1-L6)
│   ├── Op-amp parameters and ideal models
│   ├── Virtual short, golden rules
│   ├── Multi-pole transfer functions
│   ├── Phase margin and stability
│   └── Noise analysis
│
├── [CORE] opamp_config (L1-L6)
│   ├── Amplifier configurations (inv, non-inv, diff, follower)
│   ├── Feedback factor, noise gain, bandwidth
│   ├── Input/output impedance
│   ├── Instrumentation amplifier design
│   └── DC operating point MNA solver
│
├── [CORE] opamp_filter (L1-L6)
│   ├── Filter specifications and approximation types
│   ├── Butterworth/Chebyshev pole generation
│   ├── Sallen-Key, MFB, state-variable synthesis
│   ├── Cascade biquad design
│   └── Frequency response and group delay
│
├── [CORE] opamp_nonlinear (L1-L6)
│   ├── Comparator with hysteresis (Schmitt trigger)
│   ├── Precision half/full-wave rectifier
│   ├── Peak detector
│   ├── Logarithmic/antilog amplifier
│   └── Triangle/square wave generator
│
├── [CORE] opamp_advanced (L1-L8)
│   ├── Stability margins (PM, GM) via Bode analysis
│   ├── Noise RTI and noise figure computation
│   ├── Miller compensation design
│   ├── Lead compensation design
│   ├── Chopper-stabilized amplifier analysis
│   └── Nested Miller compensation (NMC)
│
└── [CORE] opamp_oscillator (L1-L6)
    ├── Barkhausen criterion verification
    ├── Wien bridge oscillator design
    ├── Phase-shift oscillator design
    ├── Quadrature oscillator design
    └── Oscillator waveform generation and THD estimation
```

## Learning Pathway

1. **Beginner**: opamp_basic → opamp_config (understand golden rules, basic configurations)
2. **Intermediate**: opamp_filter → opamp_nonlinear → opamp_oscillator (design circuits)
3. **Advanced**: opamp_advanced (stability, noise, compensation, chopper)
4. **Expert**: Combine all modules for full system design (IA + filter + stability)
