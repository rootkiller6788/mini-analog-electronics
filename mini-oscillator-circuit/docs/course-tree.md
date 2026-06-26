# Course Tree — mini-oscillator-circuit

## Prerequisite Dependencies

### Required Prerequisites (from earlier modules)

```
mini-circuit-analysis/
  └── mini-circuit-topology/
       ├── Kirchhoff's Laws (KVL, KCL)
       ├── Impedance (R, L, C in frequency domain)
       ├── Transfer functions H(s) = Vout/Vin
       ├── Bode plots (magnitude & phase)
       └── Two-port network parameters

mini-signal-system-theory/
  └── mini-signal-basis/
       ├── Complex numbers (e^{jomega t} representation)
       ├── Sinusoidal steady-state analysis
       └── RMS / amplitude / peak-to-peak
  └── mini-system-analysis/
       ├── Stability (poles in s-plane)
       ├── Feedback systems
       └── Nyquist stability criterion

mini-analog-electronics/
  └── mini-bjt-amplifier/
       ├── Small-signal model (gm, r_pi, r_o)
       └── Gain stages (CE, CB, CC)
  └── mini-fet-amplifier/
       ├── MOSFET small-signal model
       └── CMOS inverter transfer characteristic
  └── mini-opamp-application/
       ├── Inverting / non-inverting amplifier
       ├── Integrator / differentiator
       └── Comparator / Schmitt trigger
```

### Internal Dependency Graph

```
oscillator_core.h ─────────────────────────────────────────────────────┐
  │ (Barkhausen criterion, Q factor, startup, tolerance)               │
  ├── oscillator_rc.h ─────────────────────────────────────────────────┤
  │     (RC phase-shift, Wien bridge, Twin-T)                          │
  ├── oscillator_lc.h ─────────────────────────────────────────────────┤
  │     (Colpitts, Hartley, Clapp, Armstrong, cross-coupled)           │
  ├── oscillator_crystal.h ────────────────────────────────────────────┤
  │     (Crystal model, Pierce, Colpitts-crystal)                      │
  ├── oscillator_relaxation.h ─────────────────────────────────────────┤
  │     (555 astable, Schmitt RC, op-amp relax, ring)                  │
  ├── oscillator_analysis.h ───────────────────────────────────────────┤
  │     (Van der Pol, Leeson PN, Hajimiri, Bode, THD, jitter)          │
  ├── oscillator_pll.h ────────────────────────────────────────────────┤
  │     (Charge-pump PLL, Type-I PLL, loop filter, stability)          │
  └── oscillator_vco.h ────────────────────────────────────────────────┘
        (Varactor model, LC VCO, ring VCO, tuning curve, FOM)
```

### Knowledge Flow

```
Circuit Theory (R, L, C, impedance)
  ↓
Frequency-Selective Networks (RC ladder, LC tank, crystal)
  ↓
Active Devices (BJT, FET, op-amp, CMOS inverter)
  ↓
Positive Feedback Loop (Barkhausen criterion)
  ↓
Self-Sustained Oscillation (start-up → steady-state)
  ↓
Oscillator Topologies (16 types across 4 families)
  ↓
Performance Analysis (phase noise, THD, stability, jitter)
  ↓
Applications (clocks, RF carriers, frequency synthesis, GPS)
  ↓
Advanced Topics (PLL, injection locking, MEMS, THz)
```

## Postrequisite Modules

This module is prerequisite for:
- `mini-wireless-mobile-comm/` — RF LO design
- `mini-radar-remote-sensing/` — Radar oscillator stability
- `mini-navigation-positioning/` — GPS receiver frequency synthesis
- `mini-rf-circuit-debugging/` — Oscillator troubleshooting
