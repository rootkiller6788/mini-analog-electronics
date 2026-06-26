/**
 * @file    oscillator_crystal.h
 * @brief   Crystal Oscillator Design — L1 Defs + L4 Laws + L6 Problems
 *
 * @details Covers quartz crystal resonator model, Pierce and Colpitts crystal
 *          oscillator topologies. Crystal oscillators provide the highest
 *          frequency stability (Q up to 2,000,000) and are the standard
 *          frequency reference in virtually all digital systems.
 *
 * Knowledge Mapping:
 *   L1 - Definitions:
 *     - Crystal equivalent circuit (L₁, C₁, R₁ motional arm + C₀ shunt)
 *     - Series resonance f_s = 1/(2π√(L₁C₁))
 *     - Parallel resonance f_p = f_s·√(1 + C₁/C₀)
 *     - Load capacitance C_L for frequency pulling
 *     - Pullability (frequency range vs load capacitance)
 *     - Drive level (max power dissipation in crystal)
 *   L2 - Core Concepts:
 *     - Piezoelectric effect
 *     - AT-cut vs BT-cut crystal temperature characteristics
 *     - Overtone operation (3rd, 5th overtone)
 *     - Negative resistance of inverter stage
 *   L3 - Mathematical Structures:
 *     - Crystal impedance: Z(s) = (1/sC₀) || (sL₁ + R₁ + 1/sC₁)
 *     - Reactance vs frequency curve
 *   L4 - Fundamental Laws:
 *     - Series resonant frequency: f_s = 1/(2π√(L₁C₁))
 *     - Parallel resonant frequency: f_p = f_s·√(1 + C₁/C₀)
 *     - Pullability: Δf/f_s ≈ C₁/(2(C₀+C_L))
 *     - Pierce gain requirement: gm > ω²·C₁·C₂·R₁·(1+C₀/C_L)²
 *   L5 - Algorithms/Methods:
 *     - Crystal parameter extraction from measurements
 *     - Load capacitance selection for frequency accuracy
 *     - Drive level compliance verification
 *   L6 - Canonical Problems:
 *     - Design a 16 MHz Pierce oscillator for MCU
 *     - Design a 32.768 kHz watch crystal oscillator
 *
 * Reference: Vittoz et al., "High-Performance Crystal Oscillator Circuits" (1988)
 *            Sedra & Smith (2020), Ch.17.5
 */

#ifndef OSCILLATOR_CRYSTAL_H
#define OSCILLATOR_CRYSTAL_H

#include "oscillator_core.h"

/* ─── L1: Crystal Equivalent Circuit Model ────────────────────────────────── */

/**
 * @brief Quartz crystal resonator equivalent circuit (Butterworth-Van Dyke model).
 *
 *        The crystal behaves electrically as:
 *
 *             ┌─── L₁ ── C₁ ── R₁ ──┐
 *             │                       │
 *             └─────── C₀ ────────────┘
 *
 *        L₁ = motional inductance (very large: mH to kH)
 *        C₁ = motional capacitance (very small: fF)
 *        R₁ = motional resistance (tens of Ω)
 *        C₀ = shunt / holder capacitance (pF)
 *
 *        Typical values for a 10 MHz AT-cut crystal:
 *          L₁ ≈ 9 mH,  C₁ ≈ 0.027 pF,  R₁ ≈ 10 Ω,  C₀ ≈ 5 pF
 *        This gives: Q = ω₀·L₁/R₁ ≈ 56,000
 */
typedef struct {
    double l1_henries;            /**< Motional inductance L₁ (H)                      */
    double c1_farads;             /**< Motional capacitance C₁ (F)                     */
    double r1_ohms;               /**< Motional resistance R₁ (Ω)                      */
    double c0_farads;             /**< Shunt capacitance C₀ (F)                        */
    double fs_hz;                 /**< Series resonant frequency f_s = 1/(2π√(L₁C₁))  */
    double fp_hz;                 /**< Parallel resonant frequency f_p (Hz)            */
    double q_factor;              /**< Quality factor Q = ωL₁/R₁                       */
    double delta_f_hz;            /**< f_p - f_s (typically 10-200 ppm of f_s)         */
    double nominal_freq_hz;       /**< Nominal frequency (typically f_s for series)    */
} crystal_model_t;

/* ─── L1: Pierce Crystal Oscillator ───────────────────────────────────────── */

/**
 * @brief Pierce oscillator — the most common crystal oscillator topology.
 *
 *        Uses a CMOS inverter (or single transistor) with the crystal in
 *        a π-network: feedback resistor R_f (biases inverter in linear
 *        region), crystal between output and input, and load capacitors
 *        C₁ (output to ground), C₂ (input to ground).
 *
 *        The inverter provides 180° phase shift; the crystal + load caps
 *        provide the remaining 180° at series resonance.
 *
 *        Oscillation frequency: near the crystal's parallel resonance
 *        with load capacitance C_L = C₁·C₂/(C₁+C₂) + C_stray.
 *
 *        Negative resistance of inverter:
 *          R_neg = -gm / (ω² · C₁ · C₂)
 *
 *        Start-up condition: |R_neg| > R₁(max) · (1 + C₀/C_L)²
 *
 *        Drive level: P_xtal = (I_xtal_rms)² · R₁
 */
typedef struct {
    crystal_model_t crystal;      /**< Crystal equivalent circuit parameters           */
    double c1_farads;             /**< Load capacitor C₁ (output to GND) (F)           */
    double c2_farads;             /**< Load capacitor C₂ (input to GND) (F)            */
    double c_stray_farads;        /**< PCB stray capacitance (F), typically 2-5 pF     */
    double c_load_farads;         /**< C_L = C₁C₂/(C₁+C₂) + C_stray                   */
    double rf_ohms;               /**< Feedback resistor R_f (Ω), typically 1 MΩ      */
    double rd_ohms;               /**< Current-limiting resistor R_d (Ω), typically 0  */
    double gm_min_siemens;        /**< Minimum gm for reliable start-up                */
    double r_neg_ohms;            /**< Negative resistance provided by inverter        */
    double freq_osc_hz;           /**< Actual oscillation frequency (Hz)               */
    double freq_pull_ppm;         /**< Frequency offset from nominal due to C_L (ppm)  */
    double drive_level_uw;        /**< Crystal drive power (µW), max typically 100 µW  */
    int    drive_level_ok;        /**< 1 if drive level within spec                    */
} pierce_osc_t;

/* ─── L1: Colpitts Crystal Oscillator ─────────────────────────────────────── */

/**
 * @brief Colpitts crystal oscillator — crystal replaces inductor.
 *
 *        The crystal operates in its inductive region (between f_s and f_p)
 *        and together with C₁, C₂ forms a Colpitts oscillator. This topology
 *        is common in discrete RF designs.
 *
 *        Oscillation frequency: between f_s and f_p, determined by C₁ and C₂.
 */
typedef struct {
    crystal_model_t crystal;      /**< Crystal equivalent circuit parameters           */
    double c1_farads;             /**< Capacitor C₁ (F)                                */
    double c2_farads;             /**< Capacitor C₂ (F)                                */
    double c_eq_farads;           /**< C_eq = C₁·C₂/(C₁+C₂)                           */
    double freq_osc_hz;           /**< Oscillation frequency (Hz)                      */
    double effective_inductance_h;/**< Equivalent inductance of crystal at f_osc (H)   */
    double gm_min_siemens;        /**< Minimum gm for start-up                         */
} colpitts_crystal_osc_t;

/* ─── L5: Crystal Oscillator Design Functions ─────────────────────────────── */

/**
 * @brief Create a crystal model from typical parameters.
 *
 * @param nominal_freq_hz  Nominal frequency (e.g., 16e6 for 16 MHz)
 * @param c0_farads        Shunt capacitance (F), if 0 uses typical value
 * @param q_typical        Quality factor, if 0 uses typical value
 * @return                 Crystal equivalent circuit model
 */
crystal_model_t  crystal_model_create(double nominal_freq_hz,
                                        double c0_farads, double q_typical);

/**
 * @brief Compute crystal impedance at a given frequency.
 *
 *        Z_crystal(jω) = R₁ + jωL₁ + 1/(jωC₁) || 1/(jωC₀)
 *
 * @param crystal  Crystal model
 * @param freq_hz  Frequency (Hz)
 * @param re       [out] Real part of impedance (Ω)
 * @param im       [out] Imaginary part of impedance (Ω)
 */
void  crystal_impedance(const crystal_model_t *crystal, double freq_hz,
                         double *re, double *im);

/**
 * @brief Design a Pierce crystal oscillator.
 *
 *        Selects load capacitors C₁, C₂ for target load capacitance,
 *        computes required gm for start-up, and verifies drive level.
 *
 * @param crystal_freq_hz  Crystal nominal frequency (Hz)
 * @param target_cl_farads Target load capacitance (F), or 0 for default
 * @param supply_v         Supply voltage (V)
 * @return                 Designed Pierce oscillator parameters
 */
pierce_osc_t  pierce_design(double crystal_freq_hz, double target_cl_farads,
                              double supply_v);

/**
 * @brief Design a Colpitts crystal oscillator.
 *
 * @param crystal_freq_hz Crystal nominal frequency (Hz)
 * @param feedback_ratio  Desired C₂/(C₁+C₂) ratio
 * @return                Designed Colpitts crystal oscillator
 */
colpitts_crystal_osc_t  colpitts_crystal_design(double crystal_freq_hz,
                                                   double feedback_ratio);

/**
 * @brief Compute frequency pulling in ppm for a given load capacitance.
 *
 *        Δf/f_s (ppm) ≈ C₁/(2(C₀+C_L)) · 10⁶
 *
 * @param crystal      Crystal model
 * @param c_load_farads Load capacitance (F)
 * @return             Frequency offset from f_s in ppm
 */
double  crystal_pullability_ppm(const crystal_model_t *crystal, double c_load_farads);

/**
 * @brief Verify Barkhausen criterion for a Pierce oscillator.
 *
 *        Checks that the inverter provides sufficient negative resistance
 *        to overcome crystal motional loss.
 *
 * @param osc  Pierce oscillator parameters
 * @return     Barkhausen criterion evaluation
 */
barkhausen_criterion_t  pierce_verify_barkhausen(const pierce_osc_t *osc);

/**
 * @brief Check crystal drive level limits.
 *
 *        Crystal manufacturers specify maximum drive level (typically
 *        100 µW for standard crystals, 1 µW for watch crystals).
 *
 * @param osc            Pierce oscillator
 * @param max_drive_uw   Maximum rated drive level (µW)
 * @return               1 if drive <= max_drive, 0 if overdriven
 */
int  crystal_drive_level_check(const pierce_osc_t *osc, double max_drive_uw);

/**
 * @brief Compute oscillator start-up time for a crystal oscillator.
 *
 *        τ_start ≈ Q/(π·f) · ln(Vout/4kTBR₁)
 *
 * @param pierce   Pierce oscillator parameters
 * @return         Start-up time estimate (seconds)
 */
double  crystal_startup_time(const pierce_osc_t *pierce);

#endif /* OSCILLATOR_CRYSTAL_H */
