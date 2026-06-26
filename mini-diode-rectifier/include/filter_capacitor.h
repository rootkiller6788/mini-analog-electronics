/**
 * filter_capacitor.h -- Rectifier Output Filtering and Smoothing
 *
 * Knowledge Coverage: L1 Definitions, L3 Math Structures, L5 Algorithms, L6 Canonical Problems
 *
 * Reference: Sedra & Smith "Microelectronic Circuits" 8th Ed (2020), Ch. 4, App. E
 *            Erickson & Maksimovic "Fundamentals of Power Electronics" 2nd Ed (2001), Ch. 15
 *            Pressman, Billings & Morey "Switching Power Supply Design" 3rd Ed (2009)
 *
 * Key Equations:
 *   Capacitor-input filter ripple (full-wave):
 *     V_r_pp ~= I_load / (2 * f_ripple * C)     (linear approximation)
 *     V_r_pp ~= V_m * (1 - exp(-T / (R*C)))     (exponential decay, exact)
 *   Conduction angle: cos(theta) ~= 1 - 2*V_r_pp/V_m
 *   Peak repetitive current: I_peak ~= I_load * pi * sqrt(2*V_m/V_r_pp)
 */

#ifndef FILTER_CAPACITOR_H
#define FILTER_CAPACITOR_H

#include <stdint.h>

/* ============================================================================
 * L1: Filter Types
 * ============================================================================ */

/**
 * FilterType -- Output filter topologies for rectifier DC smoothing.
 */
typedef enum {
    FILTER_NONE,               /**< No filter, raw rectified output               */
    FILTER_CAPACITOR_INPUT,    /**< Single capacitor across output                 */
    FILTER_RC_LOWPASS,         /**< Series R + shunt C low-pass                    */
    FILTER_LC_LOWPASS,         /**< Series L + shunt C (choke-input)              */
    FILTER_PI_CAPACITOR,       /**< C-L-C pi filter                                */
    FILTER_RC_SECTION,         /**< Cascaded RC sections                           */
    FILTER_LC_RESONANT,        /**< LC tuned to ripple frequency                   */
    FILTER_ACTIVE_REGULATOR    /**< Linear post-regulator (LDO style)              */
} FilterType;

/* ============================================================================
 * L1: Capacitor Filter Parameters
 * ============================================================================ */

/**
 * CapacitorFilterParams -- Configuration for capacitor-input filter analysis.
 *
 * This is the most common rectifier filter: a large electrolytic capacitor
 * connected directly across the rectifier output.
 */
typedef struct {
    double C;                /**< Filter capacitance (F)                            */
    double R_load;           /**< Load resistance (ohm)                              */
    double V_m;              /**< Peak input voltage from rectifier (V)             */
    double f_ripple;         /**< Ripple fundamental frequency (Hz)                 */
    double ESR;              /**< Equivalent series resistance of capacitor (ohm)   */
    double V_f;              /**< Diode forward drop (V)                            */
} CapacitorFilterParams;

/* ============================================================================
 * L1: Ripple Analysis Result
 * ============================================================================ */

/**
 * RippleAnalysis -- Complete ripple analysis for capacitor-input filter.
 *
 * Key metrics determine the quality of the DC output:
 *   - V_r_pp: peak-to-peak ripple (smaller = better)
 *   - V_dc_avg: average DC level (includes sag from V_m)
 *   - Ripple factor: V_r_rms / V_dc (should be << 1 for good supply)
 *   - Conduction angle: how much of the cycle the diodes conduct
 *   - I_peak: maximum repetitive diode current (critical for diode rating)
 *
 * Ref: Sedra & Smith Sec.4.5, Erickson & Maksimovic Ch.15
 */
typedef struct {
    double V_r_pp;           /**< Peak-to-peak ripple voltage (V)                   */
    double V_r_rms;          /**< RMS ripple voltage (V)                            */
    double V_dc_avg;         /**< Average DC output voltage (V)                     */
    double V_dc_min;         /**< Minimum (valley) voltage (V)                      */
    double V_dc_max;         /**< Maximum (peak) voltage (V)                        */
    double ripple_factor;    /**< Ripple factor = V_r_rms / V_dc_avg               */
    double conduction_angle_rad; /**< Diode conduction angle (rad)                  */
    double conduction_duty;  /**< Conduction duty cycle (0-1)                       */
    double I_peak;           /**< Peak repetitive diode current (A)                 */
    double I_rms_diode;      /**< RMS diode current (A)                             */
    double I_avg_diode;      /**< Average diode current (A)                         */
    double C_min_required;   /**< Minimum C needed for target ripple (F)            */
    double output_impedance; /**< Dynamic output impedance at ripple freq (ohm)      */
} RippleAnalysis;

/* ============================================================================
 * L1: LC Choke-Input Filter
 * ============================================================================ */

/**
 * LCFilterParams -- Parameters for choke-input (LC) filter.
 *
 * The inductor maintains continuous current, reducing peak currents
 * and improving transformer utilization. Common in high-power supplies.
 */
typedef struct {
    double L;                /**< Filter inductance (H)                              */
    double C;                /**< Filter capacitance (F)                             */
    double R_load;           /**< Load resistance (ohm)                              */
    double V_m;              /**< Peak input voltage (V)                             */
    double f_ripple;         /**< Ripple frequency (Hz)                              */
    double L_critical;       /**< Critical inductance for continuous conduction (H)  */
} LCFilterParams;

/* ============================================================================
 * L1: LC Filter Result
 * ============================================================================ */

/**
 * LCFilterResult -- Output analysis for LC choke-input filter.
 */
typedef struct {
    double V_dc_avg;         /**< Average DC output voltage (V)                     */
    double V_r_pp;           /**< Peak-to-peak ripple (V)                           */
    double ripple_factor;    /**< Ripple factor                                     */
    double I_dc;             /**< Average load current (A)                          */
    double I_min;            /**< Minimum inductor current (A)                      */
    double I_peak;           /**< Peak inductor current (A)                         */
    int    is_continuous;    /**< 1 = continuous conduction, 0 = discontinuous       */
    double L_critical_actual;/**< Actual critical inductance for given load (H)      */
    double f_cutoff;         /**< LC filter cutoff frequency (Hz), = 1/(2*pi*sqrt(LC)) */
    double attenuation_db;   /**< Ripple attenuation at f_ripple (dB)               */
} LCFilterResult;

/* ============================================================================
 * L1: Pi Filter (C-L-C)
 * ============================================================================ */

/**
 * PiFilterParams -- Parameters for C-L-C pi-section filter.
 */
typedef struct {
    double C1;               /**< Input capacitor (F)                                */
    double L;                /**< Series inductor (H)                                */
    double C2;               /**< Output capacitor (F)                               */
    double R_load;           /**< Load resistance (ohm)                              */
    double f_ripple;         /**< Ripple fundamental frequency (Hz)                  */
} PiFilterParams;

/* ============================================================================
 * L1: Pi Filter Result
 * ============================================================================ */

/**
 * PiFilterResult -- Analysis result for pi-section filter.
 */
typedef struct {
    double V_dc_avg;
    double V_r_pp_output;    /**< Output ripple after full filter (V)               */
    double V_r_pp_intermediate; /**< Ripple at C1-L junction (V)                    */
    double ripple_factor;
    double attenuation_total_db; /**< Total ripple attenuation (dB)                */
    double f_resonance;      /**< LC resonance frequency (Hz)                       */
    double Q_factor;         /**< Quality factor                                     */
} PiFilterResult;

/* ============================================================================
 * L1: Filter Design Target
 * ============================================================================ */

/**
 * FilterDesignTarget -- User-specified filter design objectives.
 *
 * Used by the automated filter design functions.
 */
typedef struct {
    double V_in_peak;        /**< Peak input voltage from rectifier (V)             */
    double V_out_min;        /**< Minimum acceptable output voltage (V)             */
    double I_load;           /**< Maximum load current (A)                          */
    double V_ripple_max_pp;  /**< Maximum allowed peak-to-peak ripple (V)           */
    double f_ripple;         /**< Ripple frequency (Hz)                             */
    double I_peak_max;       /**< Maximum allowed peak diode current (A)            */
    FilterType preferred_type; /**< Preferred filter type or FILTER_NONE for auto   */
} FilterDesignTarget;

/* ============================================================================
 * L1: Filter Design Result
 * ============================================================================ */

/**
 * FilterDesignResult -- Automated filter design output.
 */
typedef struct {
    FilterType chosen_type;
    double C;                /**< Capacitance value (F)                              */
    double L;                /**< Inductance value (H), 0 if not applicable           */
    double R;                /**< Series resistance (ohm), 0 if not applicable        */
    double estimated_V_dc;   /**< Estimated average DC output (V)                    */
    double estimated_V_r_pp; /**< Estimated ripple (V)                               */
    double cost_factor;      /**< Relative cost estimate (lower = cheaper)           */
    int    component_count;  /**< Number of passive components                       */
} FilterDesignResult;

/* ============================================================================
 * Function Declarations
 * ============================================================================ */

/* --- L5: Capacitor-Input Filter Ripple Analysis --- */

/**
 * filter_capacitor_ripple_analyze -- Full ripple analysis for capacitor-input filter.
 *
 * Uses the exponential decay model:
 *   V_dc_min = V_m * exp(-T_discharge / (R_load * C))
 * where T_discharge ~= 1/f_ripple for half-wave, 1/(2*f_ripple) for full-wave.
 *
 * Conduction angle approximation:
 *   cos(theta_c) ~= 1 - 2*V_r_pp / V_m
 *
 * Peak repetitive current (Schade 1943):
 *   I_peak ~= I_load * pi * sqrt(2*V_m / V_r_pp)
 *
 * Theorem source: O.H. Schade (1943), "Analysis of Rectifier Operation", Proc. IRE.
 * Complexity: O(1).
 */
RippleAnalysis filter_capacitor_ripple_analyze(const CapacitorFilterParams *params);

/* --- L5: Minimum Capacitance Calculation --- */

/**
 * filter_minimum_capacitance -- Compute minimum C for target ripple.
 *
 * C_min = I_load / (f_ripple * V_r_pp_max)   (linear approximation)
 *
 * This is the classic "rule of thumb" formula for electrolytic capacitor sizing.
 * For more accuracy, use filter_capacitor_ripple_analyze() iteratively.
 *
 * Complexity: O(1).
 */
double filter_minimum_capacitance(double I_load, double f_ripple, double V_r_pp_max);

/* --- L5: LC Choke-Input Filter Analysis --- */

/**
 * filter_lc_choke_analyze -- Analyze LC choke-input filter.
 *
 * Critical inductance for continuous conduction:
 *   L_crit = R_load / (3 * pi * f_ripple)
 *
 * Ripple attenuation (for f_ripple >> f_cutoff):
 *   V_r_pp_out / V_r_pp_in ~= 1 / ((2*pi*f_ripple)^2 * L*C)
 *   Attenuation_dB = -40 * log10(f_ripple / f_cutoff) for ideal LC
 *
 * Complexity: O(1).
 */
LCFilterResult filter_lc_choke_analyze(const LCFilterParams *params);

/* --- L5: Pi Filter Analysis --- */

/**
 * filter_pi_analyze -- Analyze C-L-C pi-section filter.
 *
 * Two-stage attenuation: C1 smooths the raw rectifier output,
 * L-C2 provides additional ripple reduction.
 * Total attenuation ~= -60 dB/decade above resonance.
 *
 * Complexity: O(1).
 */
PiFilterResult filter_pi_analyze(const PiFilterParams *params);

/* --- L5: Automated Filter Design --- */

/**
 * filter_design_auto -- Automated filter component selection.
 *
 * Given design targets (V_in_peak, I_load, V_ripple_max), selects optimal
 * filter topology and component values. Considers:
 *   - Component count vs. performance
 *   - Voltage drop in resistive elements
 *   - Inductor cost at high currents
 *   - Electrolytic capacitor voltage derating
 *
 * Complexity: O(1).
 */
FilterDesignResult filter_design_auto(const FilterDesignTarget *target);

/* --- L3: RC Time Constant for Filter Discharge --- */

/**
 * filter_rc_discharge -- Exponential decay model for capacitor discharge.
 *
 * V(t) = V_initial * exp(-t / (R*C))
 *
 * Used to model the discharge phase between rectifier conduction pulses.
 * Complexity: O(1).
 */
double filter_rc_discharge(double V_initial, double t, double R, double C);

/* --- L3: Charging Phase Model --- */

/**
 * filter_charging_voltage -- Model capacitor charging through diode.
 *
 * V_c(t) = V_m - (V_m - V_c0) * exp(-t / (R_effective * C))
 * where R_effective includes transformer winding resistance + diode R_S.
 *
 * Complexity: O(1).
 */
double filter_charging_voltage(double V_m, double V_c0, double t,
                               double R_effective, double C);

/* --- L6: Output Impedance vs Frequency --- */

/**
 * filter_output_impedance -- Compute filter output impedance at given frequency.
 *
 * Z_out(f) depends on filter type and capacitor ESR.
 * Important for predicting load transient response.
 *
 * Complexity: O(1).
 */
double filter_output_impedance(FilterType type, double C, double L, double ESR,
                               double frequency_Hz);

/* --- L6: Transient Load Regulation --- */

/**
 * filter_load_step_response -- Estimate voltage dip from load step.
 *
 * delta_V ~= delta_I * delta_t / C    (charge removed before loop responds)
 *
 * Complexity: O(1).
 */
double filter_load_step_response(double C, double delta_I, double response_time_s);

/* --- L2: Filter Type Recommendation --- */

/**
 * filter_recommend -- Recommend filter type based on requirements.
 *
 * Low current, cost-sensitive: capacitor-input
 * Medium power, better ripple: LC choke-input
 * Low noise: pi filter + active post-regulator
 *
 * Complexity: O(1).
 */
FilterType filter_recommend(double I_load, double V_ripple_required,
                            int noise_sensitive, int cost_sensitive);

#endif /* FILTER_CAPACITOR_H */