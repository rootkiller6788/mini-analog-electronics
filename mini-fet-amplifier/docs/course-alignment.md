# Course Alignment — FET Amplifier Module

## Nine-School Curriculum Mapping

### MIT
| Course | Topic | Module Coverage |
|--------|-------|----------------|
| **6.002** Circuits & Electronics | MOSFET amplifier basics, biasing, small-signal model | All L1-L3, CS/CD/CG configurations |
| **6.301** Solid-State Circuits | Differential pairs, current mirrors, op-amp design | `fet_design_diffpair_amplifier()`, current mirror functions |
| **6.776** High-Speed Communication Circuits | RF LNA design, noise figure, S-parameters | `fet_design_lna()`, `fet_noise_figure_calculate()` |

### Stanford
| Course | Topic | Module Coverage |
|--------|-------|----------------|
| **EE114** Fundamentals of Analog IC Design | MOS device physics, gm/Id, cascode, frequency response | All L1-L5, gm/Id tables, OCTC method |
| **EE214** Advanced Analog IC Design | Noise, distortion, fully-differential amplifiers | Noise analysis, distortion estimation, diff pair |
| **EE314** RF IC Design | LNAs, PAs, S-parameter design | `fet_design_lna()`, `fet_design_pa()`, Y/S conversion |

### UC Berkeley
| Course | Topic | Module Coverage |
|--------|-------|----------------|
| **EE105** Microelectronic Devices & Circuits | MOSFET DC/AC analysis, amplifier configurations | All bias topologies, gain formulas |
| **EE140** Linear Integrated Circuits | Differential pairs, current mirrors, op-amps | `fet_design_diffpair_amplifier()`, current mirror analysis |
| **EE240** Advanced Analog IC | gm/Id methodology, noise optimization | `fet_design_gmid()`, noise figure matching |

### Illinois
| Course | Topic | Module Coverage |
|--------|-------|----------------|
| **ECE 310** Digital Signal Processing | *(not directly applicable)* | — |
| **ECE 451** EM & Transmission Lines | S-parameters, impedance matching | S-param conversion, stability analysis |
| **ECE 453** Wireless Communication Circuits | LNA, mixer, PA design | `fet_design_lna()`, `fet_design_pa()` |

### Michigan
| Course | Topic | Module Coverage |
|--------|-------|----------------|
| **EECS 411** Microwave Circuits | RF amplifier design, S-parameters, stability | `fet_rollett_stability_k()`, `fet_yparams_to_sparams()` |
| **EECS 511** Integrated Analog Circuits | Advanced amplifier topologies | Cascode, differential pair, current mirror |
| **EECS 522** Analog IC Design | Noise, distortion, gm/Id | Full noise analysis suite |

### Georgia Tech
| Course | Topic | Module Coverage |
|--------|-------|----------------|
| **ECE 4350** Microelectronic Circuits | FET amplifiers, frequency response | All CS/CD/CG, OCTC, Bode plots |
| **ECE 6350** Applied EM | S-parameters, network analysis | Two-port parameter conversion |

### TU Munich
| Course | Topic | Module Coverage |
|--------|-------|----------------|
| **EI7001** Halbleiterbauelemente | MOSFET physics, small-signal model | Device types and I-V equations |
| **EI7006** Analoge Schaltungen | Amplifier design, biasing, stability | All bias and amplifier design functions |

### ETH Zürich
| Course | Topic | Module Coverage |
|--------|-------|----------------|
| **227-0455** Semiconductor Devices | MOS device physics and modeling | `FetTechParams`, `FetGeometry` |
| **227-0465** Analog IC Design | Amplifier topologies, noise, matching | Complete amplifier design suite |
| **227-0470** RF IC Design | S-parameters, stability, LNAs | RF amplifier functions |

### Tsinghua (清华)
| Course | Topic | Module Coverage |
|--------|-------|----------------|
| **模拟电子技术基础** | FET amplifier fundamentals, CS/CD/CG | All basic configurations |
| **模拟集成电路设计** | Differential pairs, op-amps, current mirrors | Diff pair, current mirror, cascode |
| **射频集成电路** | RF amplifiers, noise, S-parameters | LNA, PA, S-parameter analysis |

## Textbook Alignment

| Textbook | Chapters | Module Coverage |
|----------|----------|----------------|
| **Sedra & Smith**, Microelectronic Circuits 8e | Ch.5-10 | Primary reference: all functions |
| **Razavi**, Design of Analog CMOS IC 2e | Ch.2-7 | gm/Id, cascode, noise |
| **Gray & Meyer**, Analysis and Design of Analog IC 5e | Ch.3-7, 11 | OCTC, noise figure, diff pair |
| **Lee**, Design of CMOS RF IC 2e | Ch.4-6, 11 | LNA, PA, S-params, noise |
| **Sansen**, Analog Design Essentials | Ch.1-5 | gm/Id, distortion, noise |
| **Tsividis**, Operation and Modeling of MOS Transistor 3e | Ch.4, 7 | Small-signal model extraction |

## Course Common Core (Intersection of All Nine Schools)

1. **MOSFET I-V equations** (all schools) → `fet_id_saturation()`, `fet_id_triode()`
2. **CS/CD/CG amplifier topologies** (all schools) → `fet_cs_gain_ideal()`, `fet_cd_gain_ideal()`, `fet_cg_gain_ideal()`
3. **DC biasing** (all schools) → `fet_bias_voltage_divider()`, `fet_bias_design()`
4. **Small-signal hybrid-π model** (all schools) → `fet_extract_hybrid_pi()`
5. **Frequency response / Miller effect** (all schools) → `fet_miller_decompose()`, `fet_octc_common_source()`
6. **Differential pair** (8/9 schools) → `fet_design_diffpair_amplifier()`
7. **Current mirrors** (8/9 schools) → `fet_current_mirror_ratio()`
8. **Noise in FETs** (7/9 schools) → `fet_noise_figure_calculate()`
9. **S-parameters for RF** (6/9 schools) → `fet_yparams_to_sparams()`
