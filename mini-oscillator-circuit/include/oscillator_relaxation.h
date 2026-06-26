/**
 * @file    oscillator_relaxation.h
 * @brief   Relaxation Oscillators — L1 Defs + L5 Design + L6 Canonical Problems
 *
 * @details Covers 555 timer astable, Schmitt-trigger RC oscillator, op-amp
 *          relaxation oscillator, and ring oscillator. Relaxation oscillators
 *          generate non-sinusoidal waveforms (square, triangle, sawtooth) by
 *          charging/discharging a capacitor between two threshold voltages.
 *
 * Knowledge Mapping:
 *   L1 - Definitions:
 *     - Duty cycle (t_high / T · 100%)
 *     - Threshold voltages (V_TH+, V_TH-)
 *     - Hysteresis width V_HYST = V_TH+ - V_TH-
 *     - RC charging equation: v(t) = V_final + (V_init - V_final)·exp(-t/τ)
 *   L2 - Core Concepts:
 *     - Capacitor charge/discharge cycling
 *     - Schmitt trigger hysteresis
 *     - Comparator-based threshold detection
 *   L3 - Mathematical Structures:
 *     - RC exponential charging: τ = R·C
 *     - Period T = R·C·ln((V_TH+ - V_ss_low)/(V_TH+ - V_ss_high))
 *       + R·C·ln((V_ss_high - V_TH-)/(V_ss_low - V_TH-))
 *   L4 - Fundamental Laws:
 *     - 555 Astable: f = 1.44/((R₁+2R₂)·C), Duty = (R₁+R₂)/(R₁+2R₂)·100%
 *     - Schmitt RC:  f = 1/(2RC·ln(1+2R₂/R₁)) for non-inverting Schmitt
 *   L5 - Algorithms/Methods:
 *     - Component selection for target frequency and duty cycle
 *   L6 - Canonical Problems:
 *     - Design a 1 kHz 555 timer oscillator with 50% duty cycle
 *     - Design a 100 kHz Schmitt trigger RC oscillator
 *
 * Reference: Sedra & Smith (2020), Ch.17.4
 *            Horowitz & Hill, "The Art of Electronics" (2015), Ch.7
 *            Signetics/Philips 555 Timer Datasheet
 */

#ifndef OSCILLATOR_RELAXATION_H
#define OSCILLATOR_RELAXATION_H

#include "oscillator_core.h"

/* ─── L1: 555 Timer Astable Oscillator ────────────────────────────────────── */

/**
 * @brief 555 timer in astable (free-running) configuration.
 *
 *        The 555 timer charges C through R₁+R₂ and discharges through R₂ only.
 *
 *        Charge time (output HIGH):  t_high = ln(2)·(R₁+R₂)·C  ≈ 0.693·(R₁+R₂)·C
 *        Discharge time (output LOW): t_low  = ln(2)·R₂·C       ≈ 0.693·R₂·C
 *
 *        Period:    T = t_high + t_low = ln(2)·(R₁+2R₂)·C
 *        Frequency: f = 1/T = 1.44 / ((R₁+2R₂)·C)
 *        Duty cycle: D = t_high/T = (R₁+R₂)/(R₁+2R₂) · 100%
 *
 *        Note: Duty cycle is always > 50% in this basic configuration.
 *        Using a diode in parallel with R₂ yields D ≈ 50%.
 */
typedef struct {
    double r1_ohms;               /**< R₁: resistor between VCC and DISCH (Ω)          */
    double r2_ohms;               /**< R₂: resistor between DISCH and THRES/TRIG (Ω)   */
    double c_farads;              /**< C: timing capacitor (F)                          */
    double c_bypass_farads;       /**< Bypass capacitor on CONTROL pin (F), usually 10n */
    double supply_voltage_v;      /**< VCC supply voltage (V)                           */
    double v_high_threshold;      /**< Upper threshold = 2/3 VCC (V)                    */
    double v_low_threshold;       /**< Lower threshold = 1/3 VCC (V)                    */
    double t_high_sec;            /**< Time output is HIGH (s)                          */
    double t_low_sec;             /**< Time output is LOW (s)                           */
    double period_sec;            /**< T = t_high + t_low (s)                           */
    double freq_hz;               /**< f = 1/T (Hz)                                    */
    double duty_cycle_percent;    /**< Duty cycle = t_high/T * 100 (%)                  */
    int    has_diode;             /**< 1 if diode across R₂ for 50% duty cycle          */
} relaxation_555_t;

/* ─── L1: Schmitt Trigger RC Relaxation Oscillator ────────────────────────── */

/**
 * @brief Schmitt-trigger inverter (e.g., 74HC14) based relaxation oscillator.
 *
 *        Uses a single resistor R from output to input, and capacitor C from
 *        input to ground. The Schmitt trigger thresholds determine the charge/
 *        discharge endpoints.
 *
 *        For CMOS Schmitt trigger with V_T+ ≈ 2/3 VDD, V_T- ≈ 1/3 VDD:
 *          f ≈ 1 / (2·R·C)
 *
 *        More precisely:
 *          t_high = R·C·ln((VDD - V_T-)/(VDD - V_T+))
 *          t_low  = R·C·ln(V_T+/V_T-)
 *          T = t_high + t_low
 *
 *        For symmetric thresholds (V_T+ = VDD - V_T-): t_high = t_low, 50% duty.
 */
typedef struct {
    double r_ohms;                /**< Timing resistor R (Ω)                            */
    double c_farads;              /**< Timing capacitor C (F)                           */
    double v_supply;              /**< Supply voltage VDD (V)                           */
    double vt_plus;               /**< Positive-going threshold V_T+ (V)                */
    double vt_minus;              /**< Negative-going threshold V_T- (V)                */
    double v_hysteresis;          /**< Hysteresis V_H = V_T+ - V_T- (V)                 */
    double t_high_sec;            /**< Output high time (s)                             */
    double t_low_sec;             /**< Output low time (s)                              */
    double period_sec;            /**< Period T (s)                                     */
    double freq_hz;               /**< Frequency f (Hz)                                 */
    double duty_cycle_percent;    /**< Duty cycle (%)                                   */
} relaxation_schmitt_t;

/* ─── L1: Op-Amp Relaxation Oscillator ────────────────────────────────────── */

/**
 * @brief Op-amp based relaxation oscillator (integrator + Schmitt trigger).
 *
 *        Produces both square wave and triangle wave outputs simultaneously.
 *        One op-amp acts as a Schmitt trigger; the other as an integrator.
 *        The integrator ramps up/down between the Schmitt thresholds.
 *
 *        Square wave at Schmitt output; triangle wave at integrator output.
 *
 *        Period (symmetric):
 *          T = 4·R·C·(R₁/R₂)   where R₁,R₂ set Schmitt thresholds
 *        Frequency:
 *          f = R₂/(4·R·C·R₁)
 *
 *        Triangle amplitude: V_tri_pp = 2·V_sat·(R₁/R₂)
 */
typedef struct {
    double r_integ_ohms;          /**< Integrator input resistor R (Ω)                  */
    double c_integ_farads;        /**< Integrator feedback capacitor C (F)              */
    double r1_schmitt_ohms;       /**< Schmitt feedback resistor R₁ (Ω)                 */
    double r2_schmitt_ohms;       /**< Schmitt input resistor R₂ (Ω)                    */
    double v_sat;                 /**< Op-amp saturation voltage (V)                    */
    double v_th_plus;             /**< Upper Schmitt threshold (V)                      */
    double v_th_minus;            /**< Lower Schmitt threshold (V)                      */
    double period_sec;            /**< Period (s)                                       */
    double freq_hz;               /**< Frequency (Hz)                                   */
    double triangle_amplitude_vpp;/**< Peak-to-peak triangle amplitude (V)              */
    double square_amplitude_vpp;  /**< Peak-to-peak square amplitude (V)                */
} relaxation_opamp_t;

/* ─── L1: Ring Oscillator ─────────────────────────────────────────────────── */

/**
 * @brief CMOS ring oscillator — odd number of inverters in a loop.
 *
 *        Each inverter contributes a propagation delay t_pd. For N inverters
 *        (N odd), the total loop delay is N·t_pd. The signal must propagate
 *        twice around the loop for one full period:
 *
 *          f = 1 / (2 · N · t_pd)
 *
 *        Ring oscillators are used for:
 *          - On-chip clock generation (VCO in PLLs)
 *          - Process monitoring (t_pd measurement)
 *          - Random number generation (jitter source)
 *
 *        Phase noise is typically poor due to low effective Q.
 */
typedef struct {
    int    num_stages;            /**< Number of inverter stages (must be odd, >=3)     */
    double t_pd_per_stage_ps;     /**< Propagation delay per inverter (ps)              */
    double total_loop_delay_ps;   /**< Total loop delay = N·t_pd (ps)                   */
    double freq_hz;               /**< f = 1/(2·N·t_pd) (Hz)                           */
    double supply_voltage_v;      /**< Supply voltage VDD (V)                           */
    double power_per_stage_uw;    /**< Dynamic power per stage (µW)                     */
    double total_power_mw;        /**< Total power (mW)                                 */
    double phase_noise_1mhz_dbc;  /**< Phase noise at 1 MHz offset (dBc/Hz)             */
} ring_osc_t;

/* ─── L5: Relaxation Oscillator Design Functions ──────────────────────────── */

/**
 * @brief Design a 555 timer astable oscillator.
 *
 *        Given target frequency and duty cycle, computes R₁, R₂, and C values.
 *
 * @param freq_hz            Target frequency (Hz)
 * @param duty_cycle_percent Target duty cycle (%) — must be >= 50% without diode
 * @return                   555 astable oscillator parameters
 */
relaxation_555_t  relaxation_555_design(double freq_hz, double duty_cycle_percent);

/**
 * @brief Design a Schmitt trigger RC relaxation oscillator.
 *
 *        Uses typical 74HC14 thresholds (V_T+ ≈ 2/3 VDD, V_T- ≈ 1/3 VDD).
 *
 * @param freq_hz     Target frequency (Hz)
 * @param supply_v    Supply voltage (V)
 * @param r_chosen    Suggested R value (Ω), or 0 for auto-selection
 * @return            Schmitt trigger relaxation oscillator parameters
 */
relaxation_schmitt_t  relaxation_schmitt_design(double freq_hz, double supply_v,
                                                   double r_chosen);

/**
 * @brief Design an op-amp relaxation oscillator (square + triangle output).
 *
 * @param freq_hz           Target frequency (Hz)
 * @param triangle_amp_vpp  Desired triangle amplitude (Vpp)
 * @param supply_v          Supply voltage (±V)
 * @return                  Op-amp relaxation oscillator parameters
 */
relaxation_opamp_t  relaxation_opamp_design(double freq_hz,
                                               double triangle_amp_vpp,
                                               double supply_v);

/**
 * @brief Design a CMOS ring oscillator.
 *
 * @param num_stages    Number of stages (odd, >= 3)
 * @param t_pd_ps       Propagation delay per stage (ps)
 * @param supply_v      Supply voltage (V)
 * @return              Ring oscillator parameters
 */
ring_osc_t  ring_oscillator_design(int num_stages, double t_pd_ps, double supply_v);

/**
 * @brief Validate a 555 oscillator design (frequency, duty cycle verification).
 *
 *        Recomputes actual frequency and duty cycle from component values
 *        and checks against target specifications.
 *
 * @param osc           555 oscillator parameters
 * @param freq_tol_pct  Acceptable frequency tolerance (%)
 * @return              1 if within tolerance, 0 otherwise
 */
int  relaxation_555_validate(const relaxation_555_t *osc, double freq_tol_pct);

#endif /* OSCILLATOR_RELAXATION_H */
