/**
 * @file    oscillator_pll.h
 * @brief   Phase-Locked Loop Oscillators — L6 Canonical Problems + L7 Applications
 *
 * @details Covers PLL fundamentals for oscillator applications: phase detector,
 *          loop filter, VCO, and frequency divider. Covers Type-I, Type-II,
 *          and charge-pump PLL architectures.
 *
 * Knowledge Mapping:
 *   L1 - Definitions:
 *     - Phase detector gain K_d (V/rad)
 *     - VCO gain K_vco (Hz/V or rad/s/V)
 *     - Loop bandwidth ω_c (rad/s)
 *     - Lock range, capture range, hold range
 *     - Phase margin, damping factor
 *   L2 - Core Concepts:
 *     - Negative feedback in phase domain
 *     - Loop filter design (lead-lag, PI)
 *     - Charge pump + PFD operation
 *     - Cycle slipping during acquisition
 *   L3 - Mathematical Structures:
 *     - PLL transfer function H(s) = θ_out/θ_ref
 *     - Type-I:   H(s) = K/(s+K)
 *     - Type-II:  H(s) = (2ζω_n s + ω²_n)/(s² + 2ζω_n s + ω²_n)
 *   L4 - Fundamental Laws:
 *     - PLL lock condition: phase error constant for Type-I, zero for Type-II
 *     - Loop stability: phase margin > 45°
 *   L5 - Algorithms/Methods:
 *     - Loop filter component selection
 *     - Phase margin optimization
 *   L6 - Canonical Problems:
 *     - Design a 1 GHz PLL frequency synthesizer
 *   L7 - Applications:
 *     - Frequency synthesis for GPS receiver LO
 *
 * Reference: Gardner, "Phaselock Techniques" (2005)
 *            Best, "Phase-Locked Loops" (2007)
 *            Razavi, "Design of Analog CMOS Integrated Circuits" (2017), Ch.15
 */

#ifndef OSCILLATOR_PLL_H
#define OSCILLATOR_PLL_H

#include "oscillator_core.h"

/* ─── L1: PLL Architecture Types ──────────────────────────────────────────── */

/**
 * @brief PLL type classification by loop filter order.
 */
typedef enum {
    PLL_TYPE_I = 1,         /**< Single integrator (VCO only), 1st order loop    */
    PLL_TYPE_II,            /**< Two integrators (loop filter + VCO), 2nd order   */
    PLL_TYPE_II_CHARGE_PUMP /**< Charge-pump PLL with PFD, effectively Type-II    */
} pll_type_t;

/* ─── L1: Phase Detector Types ────────────────────────────────────────────── */

/**
 * @brief Phase detector types.
 */
typedef enum {
    PD_MULTIPLIER = 0,      /**< Analog multiplier / mixer                      */
    PD_XOR,                 /**< XOR gate (digital)                              */
    PD_JK_FLIPFLOP,         /**< JK flip-flop phase detector                    */
    PD_PFD                  /**< Phase-Frequency Detector (charge pump PLL)      */
} phase_detector_type_t;

/* ─── L1: PLL Phase Detector ──────────────────────────────────────────────── */

/**
 * @brief Phase detector parameters.
 */
typedef struct {
    phase_detector_type_t type;   /**< Phase detector type                            */
    double kd_v_per_rad;          /**< Phase detector gain (V/rad)                     */
    double max_phase_error_rad;   /**< Linear range (±rad)                             */
    double output_voltage_min;    /**< Minimum output voltage (V)                      */
    double output_voltage_max;    /**< Maximum output voltage (V)                      */
    double ripple_freq_hz;        /**< Output ripple frequency (2·f_ref for XOR)       */
} phase_detector_t;

/* ─── L1: Loop Filter ─────────────────────────────────────────────────────── */

/**
 * @brief Loop filter for Type-I and Type-II PLL.
 *
 *        Passive lead-lag filter (Type-II PLL):
 *                 ┌── R₂ ─┬───┐
 *          Vin ───┤        │   ├─── Vout
 *                 └── C ──┘   │
 *                             │
 *                            GND
 *
 *        Transfer function: F(s) = (1 + s·R₂·C) / (1 + s·(R₁+R₂)·C)
 *
 *        For active PI filter (op-amp based):
 *        F(s) = (1 + s·R₂·C) / (s·R₁·C)
 */
typedef struct {
    double r1_ohms;               /**< R₁ (Ω) — input resistor or series resistor       */
    double r2_ohms;               /**< R₂ (Ω) — resistor in series with C              */
    double c_farads;              /**< C (F) — loop filter capacitor                    */
    double c2_farads;             /**< C₂ (F) — secondary capacitor for ripple reduction*/
    int    is_active;             /**< 1 if active (op-amp) filter, 0 if passive        */
    double tau1_sec;              /**< τ₁ = R₁·C (time constant 1)                     */
    double tau2_sec;              /**< τ₂ = R₂·C (time constant 2)                     */
    double zero_freq_hz;          /**< Zero frequency f_z = 1/(2π·R₂·C) (Hz)           */
    double pole_freq_hz;          /**< Pole frequency f_p (Hz)                          */
} pll_loop_filter_t;

/* ─── L1: PLL System Parameters ───────────────────────────────────────────── */

/**
 * @brief Complete PLL system parameters for oscillator applications.
 *
 *        Open-loop transfer function:
 *          G(s) = K_d · F(s) · K_vco / s  (with 1/N divider: / N)
 *
 *        Closed-loop transfer function:
 *          H(s) = G(s) / (1 + G(s)/N)
 */
typedef struct {
    pll_type_t pll_type;          /**< PLL architecture type                             */
    double ref_freq_hz;           /**< Reference frequency f_ref (Hz)                    */
    double output_freq_hz;        /**< Output frequency f_out = N·f_ref (Hz)             */
    double vco_free_run_hz;       /**< VCO free-running frequency (Hz)                   */
    int    n_divider;             /**< Feedback divider ratio N                          */
    int    r_divider;             /**< Reference divider ratio R (before PD)             */
    double kd_v_per_rad;          /**< Phase detector gain (V/rad)                       */
    double kvco_hz_per_v;         /**< VCO gain (Hz/V) → K_vco = 2π·K_vco (rad/s/V)     */
    double loop_bandwidth_hz;     /**< Closed-loop bandwidth f_c = ω_c/(2π) (Hz)        */
    double damping_factor;        /**< Damping factor ζ (0.5-2.0 typical, 0.707 optimal) */
    double natural_freq_hz;       /**< Natural frequency f_n = ω_n/(2π) (Hz)             */
    double phase_margin_deg;      /**< Phase margin at ω_c (degrees, >45° recommended)   */
    double lock_time_us;          /**< Lock time estimate (µs)                            */
    double freq_step_hz;          /**< Frequency step for lock time calc (Hz)             */
    double lock_range_hz;         /**< Lock range (Hz)                                    */
    double capture_range_hz;      /**< Capture range (Hz) without cycle slipping          */
    double spur_level_dbc;        /**< Reference spur level (dBc)                         */
} pll_system_t;

/* ─── L1: Charge Pump PLL ─────────────────────────────────────────────────── */

/**
 * @brief Charge-pump PLL specific parameters.
 *
 *        Uses a phase-frequency detector (PFD) driving a charge pump that
 *        sources/sinks current I_cp into the loop filter.
 *
 *        The charge pump + PFD combination has infinite pull-in range
 *        (frequency detection as well as phase detection), making it the
 *        dominant PLL architecture in modern ICs.
 *
 *        Loop filter transimpedance:
 *          Z_f(s) = (1 + s·R₂·C₁) / (s·(C₁+C₂) + s²·R₂·C₁·C₂)
 *          ≈ (1 + s·R₂·C₁) / (s·C₁)  for C₂ << C₁
 */
typedef struct {
    pll_system_t system;          /**< Base PLL system parameters                         */
    double charge_pump_current_a; /**< Charge pump current I_cp (A)                       */
    pll_loop_filter_t filter;     /**< Loop filter components                             */
    double pfd_dead_zone_ps;      /**< PFD dead zone (ps), important for in-band noise    */
    double in_band_pn_dbc_hz;     /**< In-band phase noise (dBc/Hz)                       */
    double vco_pn_at_1mhz_dbc;    /**< VCO phase noise at 1 MHz offset (dBc/Hz)           */
    double total_rms_jitter_ps;   /**< Estimated total RMS jitter (ps)                    */
} charge_pump_pll_t;

/* ─── Function Declarations — L5-L7 ───────────────────────────────────────── */

/**
 * @brief Design a Type-II charge-pump PLL frequency synthesizer.
 *
 *        Given reference frequency, output frequency, and design constraints,
 *        selects loop filter components and estimates performance.
 *
 * @param ref_freq_hz        Reference frequency (Hz), e.g., 10e6 for 10 MHz
 * @param output_freq_hz     Output frequency (Hz), e.g., 1e9 for 1 GHz
 * @param loop_bw_hz         Desired loop bandwidth (Hz), typically f_ref/10 to f_ref/100
 * @param phase_margin_deg   Desired phase margin (degrees), typically 45°-60°
 * @param k_vco_hz_per_v     VCO tuning gain (Hz/V)
 * @param i_cp_a             Charge pump current (A), typically 100e-6 to 1e-3
 * @return                   Designed charge pump PLL
 */
charge_pump_pll_t  charge_pump_pll_design(double ref_freq_hz,
                                             double output_freq_hz,
                                             double loop_bw_hz,
                                             double phase_margin_deg,
                                             double k_vco_hz_per_v,
                                             double i_cp_a);

/**
 * @brief Design a basic Type-I PLL (multiplier PD + passive filter).
 *
 * @param ref_freq_hz        Reference frequency (Hz)
 * @param output_freq_hz     Output frequency (Hz)
 * @param kd_v_per_rad       Phase detector gain (V/rad)
 * @param kvco_hz_per_v      VCO gain (Hz/V)
 * @return                   PLL system parameters
 */
pll_system_t  pll_type1_design(double ref_freq_hz, double output_freq_hz,
                                 double kd_v_per_rad, double kvco_hz_per_v);

/**
 * @brief Compute PLL closed-loop transfer function magnitude at a frequency.
 *
 *        |H(jω)| = |G(jω)| / |1 + G(jω)/N|
 *
 * @param pll      PLL system parameters
 * @param freq_hz  Frequency (Hz)
 * @return         |H(j2πf)|
 */
double  pll_transfer_magnitude(const pll_system_t *pll, double freq_hz);

/**
 * @brief Compute PLL phase noise at output (combining reference, PFD/CP, VCO).
 *
 *        The PLL acts as a low-pass filter on reference/PFD noise and
 *        a high-pass filter on VCO noise.
 *
 * @param pll          Charge pump PLL parameters
 * @param offset_hz    Frequency offset from carrier (Hz)
 * @return             Output phase noise at offset (dBc/Hz)
 */
double  charge_pump_pll_phase_noise(const charge_pump_pll_t *pll, double offset_hz);

/**
 * @brief Estimate PLL lock time for a frequency step.
 *
 *        t_lock ≈ 4/(ζ·ω_n)  for step to within ~1% of final value
 *
 * @param pll          PLL system parameters
 * @param freq_step_hz Frequency jump (Hz)
 * @return             Lock time (seconds)
 */
double  pll_lock_time_estimate(const pll_system_t *pll, double freq_step_hz);

/**
 * @brief Design loop filter components for a charge-pump PLL.
 *
 *        For a given loop bandwidth ω_c and phase margin φ_m:
 *          τ₁ = (sec(φ_m) - tan(φ_m))/ω_c
 *          τ₂ = 1/(ω²_c·τ₁)
 *          C₁ = (K_d·K_vco·τ₁)/(N·ω²_c·τ₂)·√((1+ω²_c·τ²₂)/(1+ω²_c·τ²₁))
 *          R₂ = τ₂/C₁
 *
 * @param pll  [in/out] PLL (uses bandwidth, PM, Kd, Kvco, N to compute filter)
 */
void  pll_loop_filter_design(charge_pump_pll_t *pll);

/**
 * @brief Validate PLL loop stability.
 *
 * @param pll  PLL system parameters
 * @return     1 if stable (PM > 0), 0 otherwise
 */
int  pll_stability_check(const pll_system_t *pll);

/**
 * @brief Compute lock and capture ranges for a Type-I PLL.
 *
 *        Lock range:     Δω_L = K_d·K_vco·|F(0)|  (Type-I only)
 *        Capture range:  Δω_c ≈ √(2ζω_n·Δω_L)     (approximate)
 *
 * @param pll  [in/out] PLL — lock_range_hz and capture_range_hz filled
 */
void  pll_range_compute(pll_system_t *pll);

#endif /* OSCILLATOR_PLL_H */
