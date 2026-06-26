# Course Tree — FET Amplifier Module

## Prerequisite Knowledge Graph

This module depends on concepts from:

```
                    ┌─────────────────────┐
                    │  Basic Circuit Laws │
                    │  (Ohm, KVL, KCL)    │
                    └──────────┬──────────┘
                               │
              ┌────────────────┼────────────────┐
              ▼                ▼                ▼
    ┌─────────────────┐ ┌───────────┐ ┌──────────────────┐
    │ Semiconductor   │ │ Complex   │ │  Signal Theory   │
    │ Physics (PN, MOS)│ │ Numbers   │ │  (Fourier/Laplace)│
    └────────┬────────┘ └─────┬─────┘ └────────┬─────────┘
             │                │                │
             └────────────────┼────────────────┘
                              │
              ┌───────────────┼───────────────┐
              ▼               ▼               ▼
    ┌─────────────────┐ ┌───────────┐ ┌──────────────┐
    │ FET Device      │ │ Small-    │ │ Bode /       │
    │ Physics (this)  │ │ Signal    │ │ Frequency    │
    │ L1: Types, I-V  │ │ Model     │ │ Response     │
    └────────┬────────┘ │ L3: hy-π  │ │ L3-L4: TF    │
             │          └─────┬─────┘ └──────┬───────┘
             │                │              │
             └────────────────┼──────────────┘
                              │
                              ▼
    ┌─────────────────────────────────────────────┐
    │           FET Amplifier Design              │
    │  L2: DC Bias, Load Line                     │
    │  L5: CS/CD/CG/Cascode/DiffPair Design       │
    │  L6: Canonical Amplifier Problems           │
    │  L4: Miller Theorem, OCTC, Noise            │
    └──────────────────────┬──────────────────────┘
                           │
         ┌─────────────────┼─────────────────┐
         ▼                 ▼                 ▼
    ┌──────────┐   ┌──────────────┐   ┌──────────────┐
    │ Feedback │   │ Operational  │   │ RF/Microwave │
    │ Amplifier│   │ Amplifiers   │   │ Amplifiers   │
    │ (stability│  │ (two-stage,  │   │ (LNA, PA,    │
    │  analysis)│  │  compensation)│  │  matching)   │
    └──────────┘   └──────────────┘   └──────────────┘
```

## Module Internal Dependency Tree

```
fet_types.h (L1: definitions, I-V equations)
    │
    ├── fet_dc_bias.h / fet_dc_bias.c (L2, L5: bias analysis & design)
    │   └── Depends on: fet_types.h
    │
    ├── fet_small_signal.h / fet_small_signal.c (L3-L4: hybrid-π, Y/S params)
    │   └── Depends on: fet_types.h
    │
    ├── fet_ac_analysis.h / fet_ac_analysis.c (L3-L5: TF, Bode, OCTC, Miller)
    │   └── Depends on: fet_types.h, fet_small_signal.h
    │
    ├── fet_noise.h / fet_noise.c (L1, L3-L4: noise models, NF, Friis)
    │   └── Depends on: fet_types.h, fet_small_signal.h
    │
    └── fet_amplifier.h / fet_amplifier.c (L5-L7: design flows)
        └── Depends on: all above + fet_dc_bias.h + fet_noise.h

tests/test_fet_amplifier.c
    └── Depends on: all headers

examples/example_cs_amplifier.c
examples/example_source_follower.c
examples/example_cascode.c
    └── Depends on: fet_amplifier.h, fet_noise.h

src/fet_modeling.lean
    └── Independent: formal verification of key theorems
```

## Upstream Modules (in this project)

| Module | Relevance | Dependency |
|--------|-----------|------------|
| `0. mini-signal-system-theory` | Fourier/Laplace transforms for frequency analysis | Understanding of `TransferFunction`, Bode plots |
| `1. mini-circuit-analysis` | KVL/KCL, Thevenin/Norton equivalents for bias networks | DC load line, Thevenin gate equivalent |
| `2. mini-analog-electronics/mini-bjt-amplifier` | BJT amplifier comparison and dual concepts | BJT vs FET: gm = Ic/Vt vs gm = 2Id/Vov |

## Downstream Modules (in this project)

| Module | Relevance | Dependency |
|--------|-----------|------------|
| `2. mini-analog-electronics/mini-feedback-amplifier` | Feedback topologies using FET gain stages | Uses CS/CD gain blocks |
| `2. mini-analog-electronics/mini-opamp-application` | CMOS op-amp = differential pair + CS gain stage | Uses diff pair + current mirror |
| `2. mini-analog-electronics/mini-oscillator-circuit` | FET-based oscillators (Colpitts, Pierce) | Uses CS amplifier with feedback |
| `5. mini-communication-principle` | LNA in receiver front-ends | Uses LNA design |
| `11. mini-wireless-mobile-comm` | RF power amplifiers for handsets | Uses PA design |
| `17. mini-audio-video-eng` | Audio preamplifiers and buffers | Uses CS/CD design |

## Learning Path

1. **Start**: `fet_types.h` — understand device types and I-V equations
2. **Branch A — Discrete Design**: `fet_dc_bias.h/.c` → `fet_amplifier.h/.c` (CS design)
3. **Branch B — IC Design**: `fet_small_signal.h/.c` → `fet_ac_analysis.h/.c` → noise
4. **Branch C — RF Design**: S-parameters → stability → LNA → PA
5. **Integration**: Examples tie all branches together
6. **Verification**: Lean formal proofs + C test suite

## L9: Research Frontiers (Future)

```
Current Module
    │
    ├──→ FinFET/GAAFET models (not yet implemented)
    │       └── Requires: foundry PDK data, BSIM-CMG equations
    │
    ├──→ Cryogenic amplifiers (<4K, quantum readout)
    │       └── Requires: shot noise dominance, superconducting models
    │
    ├──→ Subthreshold amplifier optimization
    │       └── Requires: extended gm/Id tables for weak inversion
    │
    └──→ THz amplifier design (ft/fmax limits)
            └── Requires: distributed modeling, EM simulation
```
