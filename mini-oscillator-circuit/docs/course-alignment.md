# Course Alignment — mini-oscillator-circuit

## Nine-School Curriculum Mapping

| School | Course | Oscillator Topics Covered | Our Implementation |
|--------|--------|--------------------------|-------------------|
| **MIT** | 6.002 Circuits & Electronics | RC/LC/RLC circuits, Barkhausen criterion | All RC/LC/crystal topologies |
| **MIT** | 6.301 Solid-State Circuits | Transistor oscillators, negative resistance | Colpitts, Hartley, cross-coupled |
| **Stanford** | EE101B Circuits II | Oscillator design, Wien bridge, phase-shift | wien_bridge_design(), rc_phase_shift_design() |
| **Stanford** | EE314 RF Integrated Circuits | LC VCO, PLL, phase noise | lc_vco_design(), charge_pump_pll_design() |
| **Berkeley** | EE105 Analog Circuits | BJT/FET oscillators, amplitude control | Colpitts with BJT model |
| **Berkeley** | EE142 IC for Communications | VCO, PLL frequency synthesis | Full PLL + VCO design suite |
| **Illinois** | ECE 453 Wireless IC | Cross-coupled LC VCO, phase noise analysis | cross_coupled_lc_design(), Leeson/Hajimiri |
| **Michigan** | EECS 411 Microwave | High-frequency oscillators, dielectric resonators | LC tank analysis, frequency pulling |
| **Georgia Tech** | ECE 6445 RF Transceivers | PLL synthesizer, LO generation | charge_pump_pll_design() with phase noise |
| **TU Munich** | High-Frequency Engineering | RF oscillator design, phase noise | leeson_phase_noise_compute() |
| **ETH Zurich** | 227-0455 EM & RF Design | Microwave oscillators, VCO optimization | lc_vco_design() with tuning analysis |
| **Tsinghua** | 通信电子线路 | Oscillator circuits, crystal oscillators | pierce_design(), colpitts_crystal_design() |

## Textbook References

| Textbook | Chapters | Topics |
|----------|----------|--------|
| Sedra & Smith, Microelectronic Circuits (2020) | Ch.17 | RC/LC/crystal oscillators, Barkhausen |
| Razavi, Design of Analog CMOS ICs (2017) | Ch.14-15 | LC VCO, PLL, phase noise |
| Razavi, RF Microelectronics (2012) | Ch.7-8 | RF oscillators, frequency synthesizers |
| Lee, CMOS RF IC Design (2004) | Ch.17 | Cross-coupled LC VCO, phase noise |
| Gardner, Phaselock Techniques (2005) | Ch.2-5 | PLL theory, loop filter design |
| Best, Phase-Locked Loops (2007) | Ch.3-4 | Charge-pump PLL, stability |
| Vittoz, High-Performance Crystal Oscillator Circuits (1988) | - | Pierce oscillator, crystal model |
| Rohde, Microwave and Wireless Synthesizers (1997) | Ch.5-6 | VCO design, phase noise optimization |
| Horowitz & Hill, The Art of Electronics (2015) | Ch.7 | Relaxation oscillators, 555 timer |
