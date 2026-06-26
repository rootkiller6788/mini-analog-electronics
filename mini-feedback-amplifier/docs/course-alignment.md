# Course Alignment — mini-feedback-amplifier

## Nine-School Curriculum Mapping

### MIT — 6.002 Circuits & Electronics / 6.301 Solid-State Circuits
| Topic | Section | Implementation |
|-------|---------|---------------|
| Negative feedback principles | Lec 20-21 | `feedback_core.c` |
| Op-amp non-inverting amplifier | Lec 21 | `example1_noninverting_amp.c` |
| Stability of feedback systems | Lec 22 | `stability.c` |

### Stanford — EE101B Circuits II / EE214B Analog IC Design
| Topic | Section | Implementation |
|-------|---------|---------------|
| Two-port feedback analysis | Ch. 8 | `topology_analysis.c` |
| Miller compensation | Ch. 9 | `comp_design_miller()` |
| Nested Miller compensation | Ch. 9 | `comp_design_nested_miller()` |

### Berkeley — EE105 Microelectronic Devices / EE140 Analog IC
| Topic | Section | Implementation |
|-------|---------|---------------|
| Series-Shunt feedback | Ch. 18 | `topology_analyze_series_shunt()` |
| Feedback amplifier frequency response | Ch. 19 | `feedback_compute_bandwidth_extension()` |
| Phase margin and stability | Ch. 19 | `stability_assess_from_poles()` |

### Illinois — ECE 342 Electronic Circuits
| Topic | Section | Implementation |
|-------|---------|---------------|
| Feedback topologies classification | Module 10 | `amplifier_topologies.h` |
| Bode plot stability analysis | Module 11 | `stability_compute_margins()` |

### Michigan — EECS 411 Microwave Circuits / EECS 413 Monolithic Amplifiers
| Topic | Section | Implementation |
|-------|---------|---------------|
| Broadband feedback amplifiers | Ch. 5 | `feedback_core.c` |
| Stability circles (RF-specific) | Ch. 5 | `stability_analysis.h` (Nyquist) |

### Georgia Tech — ECE 3042 Microelectronic Circuits
| Topic | Section | Implementation |
|-------|---------|---------------|
| Feedback and operational amplifiers | Ch. 8-10 | All topology analysis functions |
| Frequency compensation techniques | Ch. 11 | `compensation.c` |

### TU Munich — EI0406 Analog Circuit Design
| Topic | Section | Implementation |
|-------|---------|---------------|
| Rückkopplungsverstärker (Feedback Amps) | Kap. 5 | All core files |
| Stabilitätsanalyse (Stability) | Kap. 6 | `stability.c` |

### ETH Zürich — 227-0166 Analog Integrated Circuits
| Topic | Section | Implementation |
|-------|---------|---------------|
| Feedback theory fundamentals | Ch. 6 | `feedback_core.c` |
| Compensation of multi-stage amplifiers | Ch. 7 | `compensation.c` |
| Nested Miller compensation | Ch. 7 | `comp_design_nested_miller()` |

### 清华大学 (Tsinghua) — 模拟电子技术基础 (Analog Electronics)
| Topic | Section | Implementation |
|-------|---------|---------------|
| 反馈的基本概念 (Feedback Basics) | §6.1-6.2 | `feedback_amplifier.h` |
| 负反馈的四种组态 (Four Topologies) | §6.3 | `topology_analysis.c` |
| 反馈放大器的稳定性 (Stability) | §6.5 | `stability.c` |
| 频率补偿 (Frequency Compensation) | §6.6 | `compensation.c` |

## Textbook Alignment

| Textbook | Author | Chapters Covered |
|----------|--------|-----------------|
| Microelectronic Circuits, 8th Ed | Sedra & Smith | §11.1–§11.7 (full chapter) |
| Analysis and Design of Analog ICs, 5th Ed | Gray, Hurst, Lewis, Meyer | §8.1–§8.6, §9.1–§9.5 |
| Design of Analog CMOS ICs, 2nd Ed | Razavi | §10.1–§10.7 |
| Modern Control Engineering, 5th Ed | Ogata | §5.6 (Routh), §7 (Root Locus), §8 (Bode/Nyquist) |
| Frequency Compensation Techniques... | Eschauzier & Huijsing | Full monograph (NMC) |
