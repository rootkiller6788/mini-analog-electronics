# mini-oscillator-circuit

Oscillator Circuit submodule — complete implementation of Barkhausen criterion, 16 oscillator topologies (RC, LC, crystal, relaxation, ring, VCO), phase noise analysis (Leeson + Hajimiri), PLL frequency synthesis, and Van der Pol limit cycle simulation.

## Module Status: COMPLETE ✅

- **L1 Definitions**: Complete (18 core definitions with C types)
- **L2 Core Concepts**: Complete (10 concepts: positive feedback, frequency-selective networks, amplitude stabilization, etc.)
- **L3 Mathematical Structures**: Complete (Van der Pol equation, transfer functions, LC tank impedance, crystal impedance, RC charging)
- **L4 Fundamental Laws**: Complete (14 laws verified: Barkhausen, oscillation frequencies, Leeson PN, Hajimiri LTV, pullability, Nyquist stability)
- **L5 Algorithms**: Complete (12 algorithms: RK4 solver, PN computation, Bode analysis, THD, Allan variance, jitter, design methods for all topologies)
- **L6 Canonical Problems**: Complete (8 problems: 1 kHz Wien, 10 MHz Colpitts, 16 MHz Pierce, 1 kHz 555, 1 GHz PLL, RC phase-shift, LC VCO, cross-coupled LC)
- **L7 Applications**: Complete (5 applications: GPS L1 VCO, MCU clock, PLL synthesizer, audio tone, RF carrier)
- **L8 Advanced Topics**: Partial+ (Hajimiri LTV model, charge-pump PLL optimization, Van der Pol limit cycles, VCO linearity, injection locking)
- **L9 Research Frontiers**: Partial (documented: MEMS/NEMS oscillators, quantum oscillators, THz VCO, AI-designed oscillators)

## Code Metrics

| Metric | Value |
|--------|-------|
| Header files (.h) | 8 files, 2,302 lines |
| Source files (.c) | 8 files, 3,972 lines |
| Lean 4 formalization | 1 file, 175 lines |
| **include/ + src/ total** | **6,274 lines** ✅ |
| Test suite | 57 tests, all passing |
| Examples | 5 end-to-end examples |

## Core Definitions (L1)

| Definition | C Type | Description |
|-----------|--------|-------------|
| Barkhausen criterion | `barkhausen_criterion_t` | |AB|=1, ∠AB=0 condition for oscillation |
| Oscillator parameters | `oscillator_params_t` | Unified parameters for all 16 topologies |
| Quality factor Q | `quality_factor_t` | Q = 2π·E_stored/E_lost_per_cycle |
| Phase noise | `phase_noise_t` | L(Δf) dBc/Hz per Leeson (1966) |
| Frequency stability | `frequency_stability_t` | Temperature, supply, aging, Allan deviation |
| Crystal model | `crystal_model_t` | Butterworth-Van Dyke equivalent circuit |
| Varactor model | `varactor_t` | C(V) = C_j0/(1+V/φ)^m junction model |
| LC tank | `lc_tank_t` | Resonant frequency, Q, characteristic impedance |

## Core Theorems (L4)

| Theorem | Formula | Verification |
|---------|---------|-------------|
| **Barkhausen Criterion** | |AB| = 1, ∠AB = 0 (2πn) | `barkhausen_evaluate()` — tested at 1 kHz |
| **RC Phase-Shift Frequency** | f = 1/(2π·R·C·√6) | Tested for 1 kHz design |
| **Wien Bridge Frequency** | f = 1/(2π·R·C) | Verified via transfer function |
| **Colpitts Frequency** | f = 1/(2π√(L·C_eq)), C_eq=C₁C₂/(C₁+C₂) | Tested at 10 MHz |
| **Hartley Frequency** | f = 1/(2π√((L₁+L₂+2M)·C)) | Tested at 1 MHz |
| **555 Astable Frequency** | f = 1.44/((R₁+2R₂)·C) | Tested at 1 kHz |
| **Crystal Series Resonance** | f_s = 1/(2π√(L₁C₁)) | Model validated |
| **Crystal Pullability** | Δf/f_s = C₁/(2(C₀+C_L)) | Computed for 18 pF load |
| **Leeson Phase Noise** | L(Δf) = 10log[(FkT/2P_s)(1+(f₀/2QΔf)²)(1+f_c/Δf)] | Computed for 100 MHz/10 kHz |
| **Hajimiri LTV Model** | L(Δω) ∝ Γ²_rms/(2q²_maxΔω²) | LTV PN computation |

## Core Algorithms (L5)

| Algorithm | Implementation | Complexity |
|-----------|---------------|-----------|
| Runge-Kutta 4 (Van der Pol) | `van_der_pol_simulate()` | O(N) |
| Leeson phase noise | `leeson_phase_noise_compute()` | O(1) |
| Hajimiri LTV phase noise | `hajimiri_phase_noise_compute()` | O(1) |
| Bode sweep + crossover detection | `bode_analyze()`, `bode_find_crossings()` | O(N) |
| THD computation | `thd_compute()` | O(N) |
| Allan variance | `allan_variance_compute()` | O(M) |
| PN to RMS jitter integration | `phase_noise_to_rms_jitter()` | O(N) |
| Charge-pump PLL design | `charge_pump_pll_design()` | O(1) |
| LC VCO tuning curve | `lc_vco_tuning_curve()` | O(N) |

## Oscillator Topologies (L5-L6)

| Family | Topologies | Key Design Function |
|--------|-----------|-------------------|
| **RC** (3) | Phase-shift, Wien bridge, Twin-T | `rc_phase_shift_design()`, `wien_bridge_design()` |
| **LC** (5) | Colpitts, Hartley, Clapp, Armstrong, Cross-coupled | `colpitts_design()`, `hartley_design()`, `clapp_design()` |
| **Crystal** (2) | Pierce, Colpitts-crystal | `pierce_design()`, `colpitts_crystal_design()` |
| **Relaxation** (3) | 555 astable, Schmitt RC, Op-amp | `relaxation_555_design()`, `relaxation_schmitt_design()` |
| **VCO** (2) | LC VCO, Ring VCO | `lc_vco_design()`, `ring_vco_design()` |
| **Other** (1) | Ring oscillator | `ring_oscillator_design()` |

## Nine-School Curriculum Mapping

| School | Course | Topics Covered |
|--------|--------|---------------|
| **MIT** | 6.002/6.301 | Barkhausen, transistor oscillators |
| **Stanford** | EE101B/EE314 | Wien bridge, LC VCO, PLL, phase noise |
| **Berkeley** | EE105/EE142 | BJT/FET oscillators, frequency synthesis |
| **Illinois** | ECE 453 | Cross-coupled LC VCO, phase noise |
| **Michigan** | EECS 411 | High-frequency oscillators, frequency pulling |
| **Georgia Tech** | ECE 6445 | PLL synthesizer, LO generation |
| **TU Munich** | HF Engineering | RF oscillator design, Leeson model |
| **ETH Zurich** | 227-0455 | Microwave oscillators, VCO optimization |
| **Tsinghua** | 通信电子线路 | Crystal oscillators, Pierce, Colpitts |

## Build & Run

```bash
make          # Build tests + all 5 examples
make test     # Run 57-test suite
make examples # Build examples only
make clean    # Clean build artifacts

# Run examples
./examples/example_wien_bridge      # 1 kHz Wien bridge design
./examples/example_colpitts         # 10 MHz Colpitts oscillator
./examples/example_crystal          # 16 MHz Pierce crystal oscillator
./examples/example_555_relaxation   # 1 kHz 555 timer astable
./examples/example_pll_vco          # 1 GHz PLL synthesizer + VCO
```

## File Structure

```
mini-oscillator-circuit/
├── README.md                     # This file
├── Makefile                      # Build system
├── include/
│   ├── oscillator_core.h         # Core types, Barkhausen criterion
│   ├── oscillator_rc.h           # RC phase-shift, Wien bridge, Twin-T
│   ├── oscillator_lc.h           # Colpitts, Hartley, Clapp, Armstrong, X-coupled
│   ├── oscillator_crystal.h      # Crystal model, Pierce, Colpitts-crystal
│   ├── oscillator_relaxation.h   # 555 astable, Schmitt RC, op-amp, ring
│   ├── oscillator_analysis.h     # Van der Pol, Leeson, Hajimiri, Bode, THD, jitter
│   ├── oscillator_pll.h          # Charge-pump PLL, Type-I, loop filter
│   └── oscillator_vco.h          # Varactor model, LC VCO, ring VCO
├── src/
│   ├── oscillator_core.c         # Barkhausen, Q factor, startup, tolerance
│   ├── oscillator_rc.c           # RC oscillator design (3 topologies)
│   ├── oscillator_lc.c           # LC oscillator design (5 topologies)
│   ├── oscillator_crystal.c      # Crystal oscillator design + analysis
│   ├── oscillator_relaxation.c   # Relaxation oscillator design (3+1 topologies)
│   ├── oscillator_analysis.c     # Van der Pol, PN models, Bode, THD, jitter
│   ├── oscillator_pll.c          # PLL design + analysis
│   ├── oscillator_vco.c          # VCO design + tuning analysis
│   └── oscillator.lean           # Lean 4 formalization
├── tests/
│   └── test_oscillator.c         # 57-test suite
├── examples/
│   ├── example_wien_bridge.c     # 1 kHz Wien bridge oscillator
│   ├── example_colpitts.c        # 10 MHz Colpitts oscillator
│   ├── example_crystal.c         # 16 MHz Pierce crystal oscillator
│   ├── example_555_relaxation.c  # 1 kHz 555 timer astable
│   └── example_pll_vco.c         # 1 GHz PLL synthesizer + LC VCO
├── demos/
├── benches/
└── docs/
    ├── knowledge-graph.md        # L1-L9 knowledge coverage table
    ├── coverage-report.md        # Per-level coverage assessment
    ├── gap-report.md             # Missing items + priorities
    ├── course-alignment.md       # Nine-school curriculum mapping
    └── course-tree.md            # Prerequisite dependency tree
```

## References

- Sedra, A.S. & Smith, K.C., *Microelectronic Circuits*, 8th ed., Oxford, 2020.
- Razavi, B., *Design of Analog CMOS Integrated Circuits*, 2nd ed., McGraw-Hill, 2017.
- Razavi, B., *RF Microelectronics*, 2nd ed., Prentice Hall, 2012.
- Lee, T.H., *The Design of CMOS Radio-Frequency Integrated Circuits*, 2nd ed., Cambridge, 2004.
- Gardner, F.M., *Phaselock Techniques*, 3rd ed., Wiley, 2005.
- Best, R.E., *Phase-Locked Loops: Design, Simulation, and Applications*, 6th ed., McGraw-Hill, 2007.
- Vittoz, E.A. et al., "High-Performance Crystal Oscillator Circuits: Theory and Application", *IEEE JSSC*, 1988.
- Leeson, D.B., "A Simple Model of Feedback Oscillator Noise Spectrum", *Proc. IEEE*, 1966.
- Hajimiri, A. & Lee, T.H., "A General Theory of Phase Noise in Electrical Oscillators", *IEEE JSSC*, 1998.
- Demir, A., Mehrotra, A., & Roychowdhury, J., "Phase Noise in Oscillators: A Unifying Theory", *IEEE TCAD*, 2000.
- Rohde, U.L., *Microwave and Wireless Synthesizers: Theory and Design*, Wiley, 1997.
- Horowitz, P. & Hill, W., *The Art of Electronics*, 3rd ed., Cambridge, 2015.

---

*Built to SKILL.md standard — knowledge-first, code-as-carrier. All 57 tests passing, 5 examples compiling, 6,274 lines of source code.*
