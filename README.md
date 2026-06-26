# Mini Analog Electronics

A collection of **from-scratch, zero-dependency C implementations** of university-level analog electronic circuits and devices. Each module maps to MIT and other top-tier university courses, translating textbook circuit theory into runnable C models spanning diode physics, transistor amplifiers, op-amp circuits, active filters, feedback systems, oscillators, and analog IC design.

## Sub-Modules

| Sub-Module | Topics | Key Courses |
|-----------|--------|-------------|
| [mini-active-filter](mini-active-filter/) | Sallen-Key, MFB, switched-capacitor, Gm-C, N-path, cascade design | MIT 6.002, Stanford EE101B, Berkeley EE105 |
| [mini-analog-ic-design](mini-analog-ic-design/) | MOS/BJT device models, current mirrors, diff pairs, bandgap references, noise | Berkeley EE140/EE240, Stanford EE214, MIT 6.775 |
| [mini-bjt-amplifier](mini-bjt-amplifier/) | BJT DC bias, small-signal model, frequency response, cascode, Darlington, multistage | MIT 6.002, Berkeley EE105, Stanford EE114 |
| [mini-diode-rectifier](mini-diode-rectifier/) | PN junction physics, Shockley equation, half/full-wave rectifiers, filter capacitors, snubber | MIT 6.002, Berkeley EE105 |
| [mini-feedback-amplifier](mini-feedback-amplifier/) | Four feedback topologies (S-S, Sh-Sh, S-Sh, Sh-S), stability analysis, frequency compensation | MIT 6.002, Berkeley EE105, Stanford EE114 |
| [mini-fet-amplifier](mini-fet-amplifier/) | FET DC bias, small-signal model, AC frequency analysis, Miller theorem, noise | MIT 6.002, Berkeley EE105, Stanford EE114 |
| [mini-opamp-application](mini-opamp-application/) | Ideal/non-ideal op-amp models, basic circuits, active filters, nonlinear circuits, stability | MIT 6.002, MIT 6.775, Stanford EE214 |
| [mini-oscillator-circuit](mini-oscillator-circuit/) | Barkhausen criterion, LC/RC/relaxation/crystal oscillators, phase noise, PLL | MIT 6.002, Berkeley EE105, Stanford EE114 |

## Design Philosophy

- **Zero external dependencies** — pure C (C99/C11), only `libc` and `libm`
- **Self-contained modules** — each directory has its own `Makefile`, `include/`, `src/`, `examples/`, `demos/`, `tests/`
- **Theory-to-code mapping** — every module includes `docs/` with course-alignment notes and textbook references (Sedra & Smith, Razavi, Gray & Meyer)
- **Practical demos** — filter designers, amplifier simulators, rectifier calculators, oscillator analyzers, and more

## Building

Each module is standalone. Navigate to a module directory and run:

```bash
cd mini-active-filter
make all    # build everything
make test   # run tests
```

Requires **GCC** and **GNU Make**.

## Project Structure

```text
mini-analog-electronics/
├── mini-active-filter/          # Active Filters (Sallen-Key, MFB, SCF, Gm-C)
├── mini-analog-ic-design/       # Analog IC Design (MOS/BJT, mirrors, bandgap)
├── mini-bjt-amplifier/          # BJT Amplifiers (bias, small-signal, cascode)
├── mini-diode-rectifier/        # Diode Physics & Rectifier Circuits
├── mini-feedback-amplifier/     # Feedback Amplifier Topologies & Stability
├── mini-fet-amplifier/          # FET Amplifiers (bias, AC analysis, noise)
├── mini-opamp-application/      # Operational Amplifier Applications
└── mini-oscillator-circuit/     # Oscillator Circuits (LC, RC, crystal, PLL)
```

## License

MIT
