# Course Tree — Mini Active Filter

## Prerequisite Dependency Graph

```
mini-circuit-analysis (1.)
├── Laplace transform (s-domain analysis)
├── Transfer functions (poles, zeros, Bode plots)
└── Network analysis (KVL, KCL, admittance)

mini-opamp-application (2.)
├── Op-amp basics (ideal, single-pole models)
├── Inverting/non-inverting amplifiers
└── Integrator/differentiator circuits

mini-filter-theory (0.)
├── Butterworth, Chebyshev, Bessel approximations
├── Pole-zero placement theory
└── Frequency transformations (LP→HP/BP/BS)

    └── mini-active-filter (HERE) ★
        ├── Sallen-Key topology (1955)
        ├── Multiple Feedback (MFB/Rauch) topology (1970)
        ├── KHN State-Variable (1967)
        ├── Tow-Thomas Biquad (1969)
        ├── Cascade design methodology
        ├── Sensitivity analysis (Fialkow-Gerst, Snelgrove)
        ├── Op-amp non-ideality (GBW, slew rate, offset)
        ├── Monte Carlo tolerance analysis
        ├── Switched-Capacitor Filters (1977)
        ├── Gm-C / OTA-C Filters (1985)
        ├── N-Path Filters (1960/2011)
        └── MEMS Resonator Filters

    └── mini-analog-ic-design (2.)
        ├── CMOS op-amp design
        ├── On-chip filter implementation
        └── PVT variation compensation
```

## Knowledge Flow

1. **Filter Types** → Which filter function (LP, HP, BP, BS)?
2. **Approximation** → What pole-zero pattern (Butterworth, Chebyshev, etc.)?
3. **Order** → How many poles? (determined by stopband requirement)
4. **Topology** → Which circuit (SK, MFB, KHN, TT)?
5. **Components** → What R and C values?
6. **Analysis** → Sensitivity, non-idealities, yield
7. **Implementation** → PCB layout, component selection, testing

## Research Frontiers Path

```
mini-active-filter (L8 Advanced)
├── Switched-capacitor filters → mini-analog-ic-design
├── Gm-C filters → mini-analog-ic-design, mini-wireless-mobile-comm
├── N-path filters → mini-wireless-mobile-comm
└── MEMS filters → mini-sensor-measurement

L9 Research:
├── AI-optimized filter synthesis
├── Quantum-limited noise analysis
├── 6G tunable N-path filter arrays
└── Molecular/atomic-scale filters (graphene, CNT)
```
