/**
 * @file    oscillator_rc.h
 * @brief   RC Oscillator Design — L1 Defs + L5 Design Methods + L6 Canonical Problems
 *
 * @details Covers RC phase-shift oscillator (3-stage), Wien bridge oscillator,
 *          and Twin-T notch oscillator. Each topology uses RC networks for
 *          frequency selection with op-amp or transistor gain stages.
 *
 * Knowledge Mapping:
 *   L1 - Definitions:
 *     - RC time constant τ = R·C
 *     - Three-stage RC ladder transfer function
 *     - Wien bridge lead-lag network
 *     - Twin-T notch frequency
 *   L2 - Core Concepts:
 *     - RC network phase shift accumulation
 *     - Gain requirements for oscillation
 *     - Amplitude stabilization (diode, JFET, thermistor)
 *   L3 - Mathematical Structures:
 *     - s-domain transfer functions of RC networks
 *     - Barkhausen criterion applied to RC topologies
 *   L4 - Fundamental Laws:
 *     - RC phase-shift frequency: f = 1/(2π·R·C·√6)  (3-stage, equal R,C)
 *     - Wien bridge frequency:   f = 1/(2π·R·C)
 *     - Minimum gain for Wien:   A_v >= 3
 *   L5 - Algorithms/Methods:
 *     - Component selection for target frequency
 *     - Amplitude limiting design
 *     - THD minimization
 *   L6 - Canonical Problems:
 *     - Design a 1 kHz Wien bridge oscillator
 *     - Design a 10 kHz RC phase-shift oscillator
 *
 * Reference: Sedra & Smith (2020), Ch.17.2
 */

#ifndef OSCILLATOR_RC_H
#define OSCILLATOR_RC_H

#include "oscillator_core.h"

/* ─── L1: RC Phase-Shift Oscillator ───────────────────────────────────────── */

/**
 * @brief 3-stage RC phase-shift oscillator.
 *
 *        Topology: Inverting amplifier (gain -Rf/R1) + 3 cascaded RC high-pass
 *        filter stages. Each stage contributes 60° phase shift at f_osc,
 *        totaling 180° from the RC network. Combined with the 180° from the
 *        inverting amplifier, the total loop phase shift is 360° (0°).
 *
 *        Oscillation frequency (equal R, equal C):
 *          f_osc = 1 / (2π · R · C · √6)
 *
 *        Minimum amplifier gain:
 *          |A| >= 29  (for identical R, C stages)
 *
 *        For non-identical stages: solve the characteristic equation.
 */
typedef struct {
    double r_ohms;               /**< Resistance per stage (Ω), equal-R assumption    */
    double c_farads;              /**< Capacitance per stage (F), equal-C assumption   */
    double r_gain_ohms;           /**< Feedback resistor from output to inverting input */
    double r_input_ohms;          /**< Input resistor to inverting terminal             */
    double amp_gain_magnitude;    /**< |A| = Rf / Rin (magnitude of inverting gain)    */
    double freq_osc_hz;           /**< f_osc = 1/(2π·R·C·√6)                          */
    double freq_osc_rads;         /**< ω_osc = 1/(R·C·√6)                             */
    double phase_per_stage_deg;   /**< Phase shift per RC stage at f_osc (60°)         */
    int    num_stages;            /**< Number of RC stages (typically 3)               */
    double attenuation;           /**< |β| at f_osc = 1/29 for equal-RC 3-stage       */
    double startup_gain_min;      /**< Minimum gain for start-up (>= 29)               */
} rc_phase_shift_osc_t;

/* ─── L1: Wien Bridge Oscillator ──────────────────────────────────────────── */

/**
 * @brief Wien bridge oscillator using a non-inverting amplifier.
 *
 *        Topology: Non-inverting amplifier (gain 1 + Rf/Rg) with a Wien bridge
 *        network in the positive feedback path. The bridge consists of a series
 *        RC and parallel RC forming a lead-lag network.
 *
 *        Transfer function of the lead-lag network:
 *          β(s) = (sRC) / (s²R²C² + 3sRC + 1)
 *
 *        At ω₀ = 1/(RC):
 *          |β| = 1/3
 *          ∠β = 0°
 *
 *        Therefore, amplifier gain must be exactly 3:
 *          A_v = 1 + Rf/Rg = 3  →  Rf = 2·Rg
 *
 *        Oscillation frequency:
 *          f_osc = 1 / (2π · R · C)
 *
 *        Amplitude stabilization methods:
 *          1. Diode limiter (back-to-back diodes in feedback)
 *          2. JFET voltage-controlled resistor
 *          3. Thermistor / incandescent lamp (classic HP 200A)
 */
typedef struct {
    double r_series_ohms;         /**< Series resistor in lead-lag (Ω)                 */
    double c_series_farads;       /**< Series capacitor in lead-lag (F)                */
    double r_parallel_ohms;       /**< Parallel resistor in lead-lag (Ω)               */
    double c_parallel_farads;     /**< Parallel capacitor in lead-lag (F)              */
    double r_feedback_ohms;       /**< Rf: feedback resistor (Ω)                       */
    double r_ground_ohms;         /**< Rg: resistor to ground (Ω)                      */
    double amp_gain;              /**< Non-inverting gain = 1 + Rf/Rg                  */
    double freq_osc_hz;           /**< f_osc = 1/(2π·R·C) for equal components         */
    double freq_osc_rads;         /**< ω_osc = 1/(R·C)                                */
    double beta_attenuation;      /**< |β| at ω_osc = 1/3                             */
    int    equal_rc;              /**< 1 if R_series=R_parallel and C_series=C_parallel */
} wien_bridge_osc_t;

/* ─── L1: Twin-T Notch Oscillator ─────────────────────────────────────────── */

/**
 * @brief Twin-T oscillator using a notch filter in negative feedback.
 *
 *        The Twin-T network provides a sharp notch at f_notch = 1/(2π·R·C).
 *        When placed in the negative feedback path of an op-amp, the amplifier
 *        oscillates at the notch frequency because feedback is minimal there,
 *        allowing positive feedback (via a separate path) to dominate.
 *
 *        Notch frequency:
 *          f_notch = 1 / (2π · R · C)
 *
 *        Q of the notch can be adjusted by the resistor ratio.
 */
typedef struct {
    double r_ohms;                /**< Base resistance value (Ω)                       */
    double c_farads;              /**< Base capacitance value (F)                      */
    double r_divider_ohms;        /**< Voltage divider resistor for Q control (Ω)      */
    double freq_notch_hz;         /**< Notch frequency f_n = 1/(2π·R·C)               */
    double freq_osc_hz;           /**< Oscillation frequency (= notch frequency)       */
    double q_notch;               /**< Notch filter quality factor                     */
    double notch_depth_db;        /**< Attenuation at notch frequency (dB)             */
    double amp_gain_required;     /**< Required op-amp gain for oscillation            */
} twin_t_osc_t;

/* ─── L5: RC Oscillator Design Functions ──────────────────────────────────── */

/**
 * @brief Design a 3-stage RC phase-shift oscillator for a target frequency.
 *
 *        Selects R and C values to achieve f_target. Uses the equal-R, equal-C
 *        simplification. Verifies that required gain is achievable.
 *
 *        f_osc = 1 / (2π·R·C·√6) → choose R then solve for C, or vice versa.
 *
 * @param freq_hz      Target oscillation frequency (Hz)
 * @param r_chosen_ohms User-specified R value (Ω), or 0 for auto-selection
 * @param supply_v     Supply voltage (V)
 * @return             Designed RC phase-shift oscillator parameters
 */
rc_phase_shift_osc_t  rc_phase_shift_design(double freq_hz,
                                              double r_chosen_ohms,
                                              double supply_v);

/**
 * @brief Design a Wien bridge oscillator for a target frequency.
 *
 *        Component selection: choose C first (available standard values),
 *        then compute R = 1/(2π·f·C). Set amplifier gain to 3 (Rf = 2·Rg).
 *
 * @param freq_hz      Target oscillation frequency (Hz)
 * @param c_chosen_f   User-specified C value (F), or 0 for auto-selection
 * @return             Designed Wien bridge oscillator parameters
 */
wien_bridge_osc_t  wien_bridge_design(double freq_hz, double c_chosen_f);

/**
 * @brief Compute the transfer function magnitude and phase of a 3-stage
 *        RC ladder at a given frequency.
 *
 *        H(s) = 1 / (1 + 6sRC + 5s²R²C² + s³R³C³)  for equal R, C stages.
 *
 * @param r_ohms   Resistance per stage (Ω)
 * @param c_farads Capacitance per stage (F)
 * @param freq_hz  Evaluation frequency (Hz)
 * @param mag      [out] |H(jω)| at freq_hz
 * @param phase_deg [out] ∠H(jω) in degrees at freq_hz
 */
void  rc_ladder_transfer(double r_ohms, double c_farads, double freq_hz,
                          double *mag, double *phase_deg);

/**
 * @brief Compute the Wien bridge lead-lag transfer function.
 *
 *        β(jω) = (jωRC) / (1 - ω²R²C² + 3jωRC)
 *
 * @param r_ohms       R (Ω), assumes series=parallel R
 * @param c_farads     C (F), assumes series=parallel C
 * @param freq_hz      Evaluation frequency (Hz)
 * @param mag          [out] |β(jω)|
 * @param phase_deg    [out] ∠β(jω) in degrees
 */
void  wien_bridge_transfer(double r_ohms, double c_farads, double freq_hz,
                            double *mag, double *phase_deg);

/**
 * @brief Verify Barkhausen criterion for an RC phase-shift oscillator.
 *
 *        Computes loop gain at the design frequency and checks conditions.
 *
 * @param osc  RC phase-shift oscillator parameters
 * @return     Barkhausen criterion evaluation
 */
barkhausen_criterion_t  rc_phase_shift_verify_barkhausen(const rc_phase_shift_osc_t *osc);

/**
 * @brief Verify Barkhausen criterion for a Wien bridge oscillator.
 *
 * @param osc  Wien bridge oscillator parameters
 * @return     Barkhausen criterion evaluation
 */
barkhausen_criterion_t  wien_bridge_verify_barkhausen(const wien_bridge_osc_t *osc);

/**
 * @brief Design a Twin-T notch oscillator.
 *
 * @param freq_hz     Target frequency (Hz)
 * @param q_desired   Desired notch Q (5-50 typical)
 * @return            Twin-T oscillator parameters
 */
twin_t_osc_t  twin_t_design(double freq_hz, double q_desired);

/**
 * @brief Compute THD estimate for a Wien bridge oscillator with diode limiting.
 *
 *        Based on the amplitude control mechanism and loop gain excess.
 *
 * @param osc             Wien bridge parameters
 * @param excess_gain_db  How much the small-signal gain exceeds 3 (dB)
 * @return                Estimated THD in percent
 */
double  wien_bridge_estimate_thd(const wien_bridge_osc_t *osc, double excess_gain_db);

#endif /* OSCILLATOR_RC_H */
