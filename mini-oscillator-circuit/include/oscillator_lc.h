/**
 * @file    oscillator_lc.h
 * @brief   LC Oscillator Design — L1 Defs + L3 Math + L4 Laws + L6 Problems
 *
 * @details Covers LC tank oscillator topologies: Colpitts, Hartley, Clapp,
 *          Armstrong, and cross-coupled differential LC oscillators.
 *          LC oscillators dominate RF applications (100 kHz to 10+ GHz)
 *          due to superior phase noise from high-Q inductors/capacitors.
 *
 * Knowledge Mapping:
 *   L1 - Definitions:
 *     - LC tank resonant frequency f₀ = 1/(2π√(LC))
 *     - Colpitts capacitive divider ratio n = C₂/(C₁+C₂)
 *     - Hartley inductive divider ratio n = L₂/(L₁+L₂)
 *     - Clapp series capacitor C₃ for frequency stability
 *     - Negative resistance -R for LC tank loss compensation
 *   L2 - Core Concepts:
 *     - Negative resistance generation (active device)
 *     - Tank Q optimization
 *     - Frequency pulling by load impedance
 *     - Current biasing for amplitude control
 *   L3 - Mathematical Structures:
 *     - Parallel RLC impedance: Z(s) = (s/C) / (s² + s/(RC) + 1/(LC))
 *     - Negative conductance -gm for cross-coupled pair
 *     - Small-signal equivalent circuit analysis
 *   L4 - Fundamental Laws:
 *     - Colpitts frequency: f₀ = 1/(2π√(L·C_eq)), C_eq = C₁C₂/(C₁+C₂)
 *     - Hartley frequency:  f₀ = 1/(2π√(C·L_eq)), L_eq = L₁+L₂+2M
 *     - Clapp frequency:    f₀ ≈ 1/(2π√(L·C₃)), when C₃ << C₁,C₂
 *     - Start-up condition: gm > (C₁C₂ω₀²)/g_mcrit for Colpitts
 *   L5 - Algorithms/Methods:
 *     - LC tank component selection
 *     - Phase noise optimization for given power budget
 *   L6 - Canonical Problems:
 *     - Design a 10 MHz Colpitts oscillator
 *     - Design a 100 MHz Hartley VCO
 *
 * Reference: Sedra & Smith (2020), Ch.17.3
 *            Razavi (2017), Ch.14
 *            Lee, "The Design of CMOS Radio-Frequency ICs" (2004), Ch.17
 */

#ifndef OSCILLATOR_LC_H
#define OSCILLATOR_LC_H

#include "oscillator_core.h"

/* ─── L1: LC Tank Parameters ──────────────────────────────────────────────── */

/**
 * @brief Parallel LC tank resonator parameters.
 *
 *        A lossy parallel LC tank has resonant frequency:
 *          f₀ = 1/(2π√(LC)) · √(1 - 1/Q²) ≈ 1/(2π√(LC)) for high Q
 *
 *        Impedance at resonance: Z₀ = Q·ω₀·L = Q/(ω₀·C) = R_p (parallel resistance)
 *
 *        The tank Q determines phase noise: L(Δf) ∝ 1/Q²
 */
typedef struct {
    double inductance_h;          /**< Inductance L (Henries)                           */
    double capacitance_f;         /**< Total capacitance C (Farads)                     */
    double resonant_freq_hz;      /**< f₀ = 1/(2π√(LC)) (Hz)                          */
    double resonant_omega;        /**< ω₀ = 1/√(LC) (rad/s)                            */
    double characteristic_z;      /**< Z₀ = √(L/C) characteristic impedance (Ω)        */
    double parallel_r_ohms;       /**< Equivalent parallel resistance (Ω)               */
    double series_r_ohms;         /**< Equivalent series resistance (Ω)                 */
    double q_unloaded;            /**< Unloaded Q = R_p/(ω₀·L) = ω₀·R_p·C              */
    double q_loaded;              /**< Loaded Q (including external loading)            */
    double bandwidth_3db_hz;      /**< 3-dB bandwidth = f₀/Q_loaded (Hz)               */
} lc_tank_t;

/* ─── L1: Colpitts Oscillator ─────────────────────────────────────────────── */

/**
 * @brief Colpitts oscillator — capacitive voltage divider feedback.
 *
 *        Topology: Single transistor (BJT/FET) with capacitive voltage divider
 *        (C₁, C₂) across an inductor L. The junction of C₁-C₂ drives the
 *        emitter/source, providing positive feedback.
 *
 *        Equivalent capacitance:  C_eq = C₁·C₂ / (C₁ + C₂)
 *        Oscillation frequency:   f₀ = 1 / (2π · √(L · C_eq))
 *        Feedback ratio:          n = C₂ / (C₁ + C₂)
 *
 *        For BJT Colpitts (common-base):
 *        Start-up condition:  g_m > (C₁ + C₂)² / (R_p · C₁ · C₂)
 *        where R_p is the parallel tank resistance.
 */
typedef struct {
    double inductance_h;          /**< Tank inductor L (H)                              */
    double c1_farads;             /**< Capacitor C₁ (collector/gate to emitter/source)  */
    double c2_farads;             /**< Capacitor C₂ (emitter/source to ground)          */
    double c_eq_farads;           /**< C_eq = C₁·C₂/(C₁+C₂)                            */
    double freq_osc_hz;           /**< f₀ = 1/(2π√(L·C_eq))                           */
    double feedback_ratio;        /**< n = C₂/(C₁+C₂)                                  */
    double gm_min_siemens;        /**< Minimum transconductance for start-up            */
    double bias_current_ma;       /**< Collector/drain bias current (mA)                */
    double amplitude_vpp;         /**< Expected output amplitude (Vpp)                  */
    char   active_device;         /**< 'B' for BJT, 'F' for FET                         */
} colpitts_osc_t;

/* ─── L1: Hartley Oscillator ──────────────────────────────────────────────── */

/**
 * @brief Hartley oscillator — inductive voltage divider feedback.
 *
 *        Topology: Single transistor with tapped inductor (L₁, L₂) and a
 *        capacitor C across the total inductance. The tap point drives
 *        the emitter/source.
 *
 *        Total inductance (no mutual coupling):  L_total = L₁ + L₂
 *        With mutual inductance M:  L_total = L₁ + L₂ + 2M
 *
 *        Oscillation frequency:  f₀ = 1 / (2π · √(L_total · C))
 *        Feedback ratio:         n = L₂ / L_total
 */
typedef struct {
    double l1_henries;            /**< L₁: collector/drain-side inductance (H)          */
    double l2_henries;            /**< L₂: emitter/source-side inductance (H)           */
    double mutual_m_henries;      /**< Mutual inductance M (H) — 0 if uncoupled        */
    double l_total_henries;       /**< L_total = L₁+L₂+2M                              */
    double capacitance_f;         /**< Tank capacitor C (F)                             */
    double freq_osc_hz;           /**< f₀ = 1/(2π√(L_total·C))                        */
    double feedback_ratio;        /**< n = L₂/L_total                                  */
    double gm_min_siemens;        /**< Minimum transconductance for start-up            */
    double coupling_coefficient;  /**< k = M/√(L₁L₂) for coupled inductors             */
} hartley_osc_t;

/* ─── L1: Clapp Oscillator ────────────────────────────────────────────────── */

/**
 * @brief Clapp oscillator — Colpitts variant with series capacitor.
 *
 *        Adds a capacitor C₃ in series with the inductor in a Colpitts
 *        topology. C₃ is chosen much smaller than C₁ and C₂, making the
 *        oscillation frequency primarily determined by L and C₃, with
 *        greatly reduced sensitivity to transistor parasitics.
 *
 *        Equivalent capacitance:  1/C_eq = 1/C₁ + 1/C₂ + 1/C₃
 *        If C₃ << C₁, C₂:  C_eq ≈ C₃
 *
 *        Oscillation frequency:  f₀ ≈ 1 / (2π · √(L · C₃))
 *
 *        The Clapp oscillator offers superior frequency stability compared
 *        to the basic Colpitts because the transistor's parasitic capacitances
 *        (across C₁, C₂) have minimal effect when C₃ dominates C_eq.
 */
typedef struct {
    double inductance_h;          /**< Tank inductor L (H)                              */
    double c1_farads;             /**< Capacitor C₁ (across active device output)       */
    double c2_farads;             /**< Capacitor C₂ (across active device input)        */
    double c3_farads;             /**< Series capacitor C₃ (frequency-setting, C₃<<C₁,C₂) */
    double c_eq_farads;           /**< C_eq = 1/(1/C₁+1/C₂+1/C₃)                       */
    double freq_osc_hz;           /**< f₀ = 1/(2π√(L·C_eq)) ≈ 1/(2π√(L·C₃))           */
    double stability_factor;      /**< (C₁C₂)/C₃² — larger = better stability           */
    double gm_min_siemens;        /**< Minimum transconductance for start-up            */
} clapp_osc_t;

/* ─── L1: Armstrong Oscillator ────────────────────────────────────────────── */

/**
 * @brief Armstrong oscillator — transformer-coupled feedback.
 *
 *        Uses a transformer (or tapped inductor) to couple energy from the
 *        collector/drain circuit back to the base/gate. The "tickler coil"
 *        provides the positive feedback. Popular in early radio receivers.
 *
 *        Oscillation frequency: f₀ = 1/(2π√(L_primary·C))
 *        Feedback: via mutual inductance M from primary to secondary (tickler)
 */
typedef struct {
    double l_primary_h;           /**< Primary (tank) inductance (H)                    */
    double l_secondary_h;         /**< Secondary (tickler) inductance (H)               */
    double mutual_m_h;            /**< Mutual inductance (H)                            */
    double capacitance_f;         /**< Tank capacitor (F)                               */
    double freq_osc_hz;           /**< f₀ = 1/(2π√(L_primary·C))                       */
    double coupling_coefficient;  /**< k = M/√(L_primary·L_secondary)                  */
    double turns_ratio;           /**< N_secondary / N_primary                         */
} armstrong_osc_t;

/* ─── L1: Cross-Coupled LC Oscillator ─────────────────────────────────────── */

/**
 * @brief Differential cross-coupled LC oscillator (negative-gm topology).
 *
 *        Two cross-coupled transistors (BJT or CMOS) provide a negative
 *        resistance -2/gm that compensates the tank parallel loss R_p.
 *        This is the dominant topology in RF CMOS IC design.
 *
 *        Negative resistance:  R_neg = -2/gm  (differential)
 *
 *        Start-up condition:  gm·R_p > 2  (or gm > 2/R_p)
 *
 *        Advantages: differential output, good phase noise, no tapped
 *        inductor/capacitor needed.
 */
typedef struct {
    double inductance_h;          /**< Differential tank inductor (H)                   */
    double capacitance_f;         /**< Total tank capacitance (F)                       */
    double c_parasitic_f;         /**< Parasitic capacitance from transistors (F)       */
    double freq_osc_hz;           /**< f₀ = 1/(2π√(L(C+C_par)))                        */
    double gm_per_device_siemens; /**< Transconductance per transistor (S)              */
    double r_neg_ohms;            /**< Negative resistance = -2/gm (differential)       */
    double tank_rp_ohms;          /**< Tank parallel resistance (Ω)                     */
    double bias_current_ma;       /**< Tail current (mA)                                */
    double amplitude_vpp;         /**< Differential output amplitude (Vpp)              */
    int    startup_margin;        /**< gm·Rp / 2 (should be > 1, typically 2-5)         */
} cross_coupled_lc_osc_t;

/* ─── L5: LC Oscillator Design Functions ─────────────────────────────────── */

/**
 * @brief Compute LC tank parameters from component values.
 *
 * @param l_h         Inductance (H)
 * @param c_f         Capacitance (F)
 * @param series_r    Series resistance (Ω) — 0 for ideal
 * @param parallel_r  Parallel resistance (Ω) — 0 to compute from series_r
 * @return            LC tank parameters
 */
lc_tank_t  lc_tank_analyze(double l_h, double c_f,
                             double series_r, double parallel_r);

/**
 * @brief Design a Colpitts oscillator for a target frequency.
 *
 *        Auto-selects C₁, C₂, and L based on the target frequency and
 *        desired feedback ratio.
 *
 * @param freq_hz          Target frequency (Hz)
 * @param feedback_ratio   Desired n = C₂/(C₁+C₂), typically 0.1–0.5
 * @param l_chosen_h       User-specified L (H), or 0 for auto-selection
 * @return                 Colpitts oscillator parameters
 */
colpitts_osc_t  colpitts_design(double freq_hz, double feedback_ratio,
                                  double l_chosen_h);

/**
 * @brief Design a Hartley oscillator for a target frequency.
 *
 * @param freq_hz          Target frequency (Hz)
 * @param feedback_ratio   Desired n = L₂/(L₁+L₂), typically 0.1–0.5
 * @return                 Hartley oscillator parameters
 */
hartley_osc_t  hartley_design(double freq_hz, double feedback_ratio);

/**
 * @brief Design a Clapp oscillator for a target frequency.
 *
 *        Uses C₃ << C₁, C₂ for improved frequency stability.
 *
 * @param freq_hz          Target frequency (Hz)
 * @param stability_factor Desired (C₁C₂)/C₃² (10-100 typical)
 * @return                 Clapp oscillator parameters
 */
clapp_osc_t  clapp_design(double freq_hz, double stability_factor);

/**
 * @brief Design an Armstrong oscillator.
 *
 * @param freq_hz          Target frequency (Hz)
 * @param coupling_coeff   Coupling coefficient k (0.05-0.5)
 * @return                 Armstrong oscillator parameters
 */
armstrong_osc_t  armstrong_design(double freq_hz, double coupling_coeff);

/**
 * @brief Design a cross-coupled LC oscillator.
 *
 *        Computes required gm for start-up and selects L, C for target
 *        frequency with appropriate tank Q.
 *
 * @param freq_hz          Target frequency (Hz)
 * @param tank_q_target    Desired tank Q (5-20 typical for IC)
 * @param power_mw         Power budget (mW)
 * @return                 Cross-coupled oscillator parameters
 */
cross_coupled_lc_osc_t  cross_coupled_lc_design(double freq_hz,
                                                   double tank_q_target,
                                                   double power_mw);

/**
 * @brief Verify Barkhausen criterion for a Colpitts oscillator.
 *
 * @param osc  Colpitts oscillator parameters
 * @return     Barkhausen criterion evaluation
 */
barkhausen_criterion_t  colpitts_verify_barkhausen(const colpitts_osc_t *osc);

/**
 * @brief Verify Barkhausen criterion for a Hartley oscillator.
 */
barkhausen_criterion_t  hartley_verify_barkhausen(const hartley_osc_t *osc);

/**
 * @brief Verify Barkhausen criterion for a cross-coupled LC oscillator.
 */
barkhausen_criterion_t  cross_coupled_verify_barkhausen(const cross_coupled_lc_osc_t *osc);

/**
 * @brief Compute frequency pulling due to load impedance variation.
 *
 *        f = f₀ / √(1 + j/(2Q_loaded)·(Γ_L))  — simplified pulling model
 *
 * @param nominal_freq_hz  Unloaded oscillation frequency (Hz)
 * @param q_loaded         Loaded Q
 * @param load_vswr        Load VSWR
 * @return                 Frequency deviation (Hz)
 */
double  lc_oscillator_pulling(double nominal_freq_hz, double q_loaded,
                                double load_vswr);

#endif /* OSCILLATOR_LC_H */
