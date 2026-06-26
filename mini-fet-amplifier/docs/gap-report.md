# Gap Report — FET Amplifier Module

## Identified Gaps and Mitigation

### L8: Advanced Topics

| # | Gap | Priority | Mitigation |
|---|-----|----------|------------|
| 1 | **Large-signal transient simulation** — Time-domain nonlinear analysis (harmonic balance not implemented) | MEDIUM | Existing small-signal + distortion estimation covers first-order needs; transient sim requires SPICE-level engine |
| 2 | **Monte Carlo / mismatch analysis** — Statistical yield analysis for process variation | LOW | Systematic offset analysis exists; full statistical simulation is a separate tool concern |
| 3 | **ESD protection impact** — Input protection diodes affect noise and input capacitance | LOW | Layout/ESD is a physical design concern; covered in `mini-emc-signal-integrity` |

### L9: Research Frontiers

| # | Gap | Priority | Mitigation |
|---|-----|----------|------------|
| 1 | **FinFET / GAAFET device models** — Surface-potential-based models (PSP, BSIM-CMG) for advanced nodes | LOW | Device type enumerations exist; compact models require foundry PDK data |
| 2 | **Cryogenic noise modeling** — Noise at 4K for quantum computing readout | LOW | Temperature model framework exists; cryogenic-specific physics (shot noise dominance, Kondo effect) not modeled |
| 3 | **Subthreshold amplifier design** — Weak inversion optimization for ultra-low power | MEDIUM | `fet_id_subthreshold()` equation exists; full gm/Id table extension needed |
| 4 | **THz amplifier challenges** — Distributed effects, transmission-line modeling, ft/fmax optimization | LOW | S-parameter framework exists; distributed modeling requires EM simulation |

## Current Status

- **L1-L6**: All COMPLETE — no gaps
- **L7**: 6 applications implemented — exceeds minimum
- **L8**: 5/5 advanced topics addressed (includes gm/Id, S-param stability, gate noise, temperature, offset)
- **L9**: 4 frontiers documented with device type support

## Priority Actions (if upgrading)

1. **Medium priority**: Add weak-inversion gm/Id entries to lookup table
2. **Medium priority**: Add transient step response example
3. **Low priority**: Add FinFET SPICE model parameter structure
4. **Low priority**: Add cryogenic noise temperature model

## Self-Assessment

This module is declared **COMPLETE** because:
- L1-L6 are fully implemented with code, tests, and Lean proofs
- L7 has 6 real application designs (exceeding the 2-application minimum)
- L8 has 5 advanced topics with working implementations
- L9 gaps are research-level topics documented but not required for COMPLETE

The remaining L9 gaps are genuine research frontiers that require:
- Foundry-specific compact model data (FinFET)
- Cryogenic measurement data (noise at 4K)
- Full EM simulation (THz effects)

These are documented for awareness but fall outside the scope of a self-contained educational module.
