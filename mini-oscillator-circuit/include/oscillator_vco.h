/**
 * @file    oscillator_vco.h
 * @brief   Voltage-Controlled Oscillator — L6 Problems + L7 Applications
 *
 * @details Covers VCO design and analysis: varactor-tuned LC VCOs,
 *          ring VCOs, tuning sensitivity, linearity (KVCO constancy),
 *          and tuning range optimization.
 *
 * Knowledge Mapping:
 *   L1 - Definitions:
 *     - VCO gain K_VCO (Hz/V or rad/s/V)
 *     - Tuning range (f_min to f_max)
 *     - Tuning linearity (deviation from constant K_VCO)
 *     - Varactor capacitance vs voltage C(V) = C_j0/(1+V/φ)^m
 *   L2 - Core Concepts:
 *     - Varactor diode as voltage-controlled capacitor
 *     - Tuning sensitivity trade-off (wide range vs phase noise)
 *     - Band switching for multi-octave VCOs
 *   L3 - Mathematical Structures:
 *     - f(V_tune) = 1/(2π√(L·C_total(V_tune)))
 *     - C_total = C_fixed + C_varactor(V_tune) + C_parasitic
 *   L4 - Fundamental Laws:
 *     - KVCO = df/dV_tune
 *   L5 - Algorithms/Methods:
 *     - Varactor selection for linear tuning
 *     - VCO phase noise optimization
 *   L6 - Canonical Problems:
 *     - Design a 2-3 GHz LC VCO with varactor tuning
 *   L7 - Applications:
 *     - GPS L1 (1.575 GHz) VCO design
 *
 * Reference: Razavi (2017), Ch.14-15
 *            Rohde, "Microwave and Wireless Synthesizers" (1997)
 */

#ifndef OSCILLATOR_VCO_H
#define OSCILLATOR_VCO_H

#include "oscillator_core.h"

/* ─── L1: Varactor Model ──────────────────────────────────────────────────── */

/**
 * @brief Varactor diode (variable capacitance) model.
 *
 *        The junction capacitance of a reverse-biased PN junction depends
 *        on the applied voltage:
 *
 *        C_j(V) = C_j0 / (1 + V/φ)^m
 *
 *        where:
 *          C_j0 = zero-bias junction capacitance (F)
 *          V    = reverse bias voltage (V)
 *          φ   = built-in potential (V), typically 0.6-0.9 V
 *          m   = grading coefficient:
 *                0.5 for abrupt junction
 *                0.33 for linearly graded junction
 *                1.0-2.0 for hyperabrupt (wider tuning)
 *
 *        For MOS varactors (accumulation-mode):
 *        C(V) depends on gate-source voltage and can achieve
 *        C_max/C_min ratios up to 3:1.
 */
typedef struct {
    double cj0_farads;            /**< Zero-bias capacitance (F)                        */
    double phi_v;                 /**< Built-in potential φ (V)                         */
    double m_grading;             /**< Grading coefficient m                            */
    double c_min_farads;          /**< Minimum capacitance at max reverse bias (F)      */
    double c_max_farads;          /**< Maximum capacitance at 0V bias (F)               */
    double tuning_ratio;          /**< C_max / C_min                                   */
    double v_min;                 /**< Minimum tuning voltage (V)                       */
    double v_max;                 /**< Maximum tuning voltage (V)                       */
    double series_r_ohms;         /**< Series resistance (Ω) — affects tank Q           */
    double q_at_1ghz;             /**< Quality factor at 1 GHz                          */
} varactor_t;

/* ─── L1: VCO Parameters ──────────────────────────────────────────────────── */

/**
 * @brief LC VCO with varactor tuning.
 *
 *        Oscillation frequency: f(V_tune) = 1/(2π√(L·C_total(V_tune)))
 *
 *        where C_total = C_fixed + C_varactor(V_tune) + C_parasitic.
 *
 *        K_VCO = df/dV_tune = -f³·L·(dC_total/dV_tune)  (from differentiation)
 */
typedef struct {
    double inductance_h;          /**< Tank inductor L (H)                              */
    double c_fixed_farads;        /**< Fixed capacitance (F)                            */
    double c_parasitic_farads;    /**< Parasitic capacitance from layout (F)             */
    varactor_t varactor;          /**< Varactor parameters                              */
    double freq_min_hz;           /**< Minimum frequency at V_tune_min (Hz)             */
    double freq_max_hz;           /**< Maximum frequency at V_tune_max (Hz)             */
    double freq_center_hz;        /**< Center frequency (Hz)                            */
    double tuning_range_percent;  /**< (f_max-f_min)/f_center * 100 (%)                 */
    double kvco_min_hz_per_v;     /**< Minimum K_VCO over tuning range (Hz/V)           */
    double kvco_max_hz_per_v;     /**< Maximum K_VCO over tuning range (Hz/V)           */
    double kvco_ratio;            /**< K_max/K_min (1 = perfectly linear)               */
    double kvco_avg_hz_per_v;     /**< Average K_VCO (Hz/V)                             */
    double phase_noise_at_1mhz;   /**< Phase noise at 1 MHz offset (dBc/Hz)             */
    double supply_voltage_v;      /**< Supply voltage (V)                               */
    double power_mw;              /**< Power consumption (mW)                           */
    double figure_of_merit;       /**< FOM = PN - 20log(f0/Δf) + 10log(P_dc/1mW)        */
} lc_vco_t;

/* ─── L1: Ring VCO ────────────────────────────────────────────────────────── */

/**
 * @brief Voltage-controlled ring oscillator.
 *
 *        Frequency controlled by varying the delay per stage, typically
 *        through current starving (current-controlled inverter delay).
 *
 *        f(V_tune) ≈ I_bias(V_tune) / (N·C_node·V_swing)
 *
 *        Advantages: small die area, wide tuning range, no inductor
 *        Disadvantages: poor phase noise compared to LC VCO
 */
typedef struct {
    int    num_stages;            /**< Number of delay stages (odd, >=3)                */
    double t_pd_min_ps;           /**< Minimum propagation delay per stage (ps)          */
    double t_pd_max_ps;           /**< Maximum propagation delay per stage (ps)          */
    double freq_max_hz;           /**< f_max = 1/(2·N·t_pd_min) (Hz)                    */
    double freq_min_hz;           /**< f_min = 1/(2·N·t_pd_max) (Hz)                    */
    double kvco_hz_per_v;         /**< Average K_VCO (Hz/V)                              */
    double supply_voltage_v;      /**< Supply voltage (V)                               */
    double power_uw_per_mhz;      /**< Power per MHz (µW/MHz)                            */
    double phase_noise_at_1mhz;   /**< Phase noise at 1 MHz offset (dBc/Hz)             */
} ring_vco_t;

/* ─── Function Declarations — L5-L7 ───────────────────────────────────────── */

/**
 * @brief Compute varactor capacitance at a given bias voltage.
 *
 *        C(V) = C_j0 / (1 + V/φ)^m    for V >= 0 (reverse bias)
 *
 * @param varactor  Varactor parameters
 * @param v_bias    Reverse bias voltage (V)
 * @return          Capacitance (F)
 */
double  varactor_capacitance(const varactor_t *varactor, double v_bias);

/**
 * @brief Initialize a varactor with typical parameters.
 *
 * @param cj0_farads      Zero-bias capacitance (F)
 * @param tuning_ratio    C_max/C_min ratio
 * @param v_tune_max      Maximum tuning voltage (V)
 * @return                Initialized varactor
 */
varactor_t  varactor_init(double cj0_farads, double tuning_ratio, double v_tune_max);

/**
 * @brief Design an LC VCO with varactor tuning.
 *
 * @param freq_min_hz     Minimum frequency (Hz)
 * @param freq_max_hz     Maximum frequency (Hz)
 * @param supply_v        Supply voltage (V)
 * @param power_mw_target Target power (mW)
 * @return                Designed LC VCO
 */
lc_vco_t  lc_vco_design(double freq_min_hz, double freq_max_hz,
                           double supply_v, double power_mw_target);

/**
 * @brief Compute VCO frequency at a given tuning voltage.
 *
 *        f(V) = 1/(2π√(L·(C_fixed + C_varactor(V) + C_parasitic)))
 *
 * @param vco       LC VCO parameters
 * @param v_tune    Tuning voltage (V)
 * @return          Oscillation frequency (Hz)
 */
double  lc_vco_frequency(const lc_vco_t *vco, double v_tune);

/**
 * @brief Compute K_VCO at a given tuning voltage by numerical differentiation.
 *
 * @param vco       LC VCO parameters
 * @param v_tune    Tuning voltage (V)
 * @return          K_VCO (Hz/V) at this bias point
 */
double  lc_vco_kvco(const lc_vco_t *vco, double v_tune);

/**
 * @brief Compute VCO tuning curve over the voltage range.
 *
 * @param vco        LC VCO parameters
 * @param num_points Number of points to compute
 * @param v_tune_arr [out] Tuning voltage array (length num_points)
 * @param freq_arr   [out] Frequency array (length num_points)
 * @param kvco_arr   [out] K_VCO array (length num_points)
 */
void  lc_vco_tuning_curve(const lc_vco_t *vco, size_t num_points,
                            double *v_tune_arr, double *freq_arr, double *kvco_arr);

/**
 * @brief Estimate LC VCO phase noise at a given frequency offset.
 *
 *        Uses a simplified Leeson model adapted for VCO:
 *        L(Δf) ≈ 10·log₁₀(F·k·T/(2·P_sig)·(f₀/(2·Q·Δf))²)
 *
 * @param vco          LC VCO parameters
 * @param offset_hz    Frequency offset (Hz)
 * @param loaded_q     Loaded tank Q (5-20 typical in IC)
 * @param noise_factor Device noise factor
 * @return             Phase noise (dBc/Hz)
 */
double  lc_vco_phase_noise_estimate(const lc_vco_t *vco, double offset_hz,
                                       double loaded_q, double noise_factor);

/**
 * @brief Compute the VCO figure of merit.
 *
 *        FOM = L(Δf) - 20·log₁₀(f₀/Δf) + 10·log₁₀(P_dc / 1mW)
 *
 *        FOM < -180 dBc/Hz is typical for good LC VCO designs.
 *
 * @param vco  LC VCO parameters
 * @return     FOM (dBc/Hz)
 */
double  vco_figure_of_merit(const lc_vco_t *vco);

/**
 * @brief Design a ring VCO.
 *
 * @param freq_center_hz  Center frequency (Hz)
 * @param kvco_target     Target K_VCO (Hz/V)
 * @param supply_v        Supply voltage (V)
 * @return                Designed ring VCO
 */
ring_vco_t  ring_vco_design(double freq_center_hz, double kvco_target,
                               double supply_v);

#endif /* OSCILLATOR_VCO_H */
