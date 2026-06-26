/**
 * power_rectifier.h -- High-Power and Three-Phase Rectifier Systems
 *
 * Knowledge Coverage: L1 Definitions, L6 Canonical Problems, L7 Applications, L8 Advanced Topics
 *
 * Reference: Mohan, Undeland & Robbins "Power Electronics" 3rd Ed (2003), Ch. 5-6
 *            Rashid "Power Electronics Handbook" 4th Ed (2018), Ch. 3-4
 *            Erickson & Maksimovic "Fundamentals of Power Electronics" 2nd Ed (2001)
 *
 * Applications: Tesla Supercharger front-end, Toyota Prius inverter DC link,
 *               Boeing 787 power distribution, Detroit auto plant DC drives,
 *               NASA spacecraft power processing, iPhone USB charger input stage,
 *               Fukushima backup power systems, maglev train traction supplies.
 */

#ifndef POWER_RECTIFIER_H
#define POWER_RECTIFIER_H

#include <stdint.h>

/* ============================================================================
 * L1: Three-Phase Connection Types
 * ============================================================================ */

/**
 * ThreePhaseConnection -- Wye (star) or delta input configuration.
 */
typedef enum {
    THREEPHASE_WYE,            /**< Star/wye connection, neutral available          */
    THREEPHASE_DELTA           /**< Delta connection, line-to-line only             */
} ThreePhaseConnection;

/* ============================================================================
 * L1: Three-Phase Rectifier Configuration
 * ============================================================================ */

/**
 * ThreePhaseConfig -- Parameters for three-phase rectifier analysis.
 */
typedef struct {
    double V_ll_rms;           /**< Line-to-line RMS voltage (V), e.g. 480, 208      */
    double freq_Hz;            /**< Line frequency (Hz), typically 50 or 60          */
    double R_load;             /**< Load resistance (ohm)                             */
    double L_source;           /**< Source/commutation inductance per phase (H)      */
    double V_f;                /**< Diode forward voltage drop (V)                   */
    ThreePhaseConnection connection; /**< Wye or delta input                        */
    int    use_half_wave;      /**< 0=full-bridge (6-pulse), 1=half-wave (3-pulse)   */
} ThreePhaseConfig;

/* ============================================================================
 * L1: Three-Phase Rectifier Result
 * ============================================================================ */

/**
 * ThreePhaseResult -- Complete analysis of three-phase rectifier.
 *
 * For 6-pulse bridge (ideal, no commutation overlap):
 *   V_dc = (3 * sqrt(3) / pi) * V_m_ll ~= 1.654 * V_m_ll = 2.34 * V_ll_rms
 *   V_r_pp = V_dc * (2 - sqrt(3)) / 2 ~= 0.068 * V_dc  (4.2% ripple!)
 *   Ripple frequency = 6 * f_line (300 Hz for 50 Hz, 360 Hz for 60 Hz)
 *   PIV per diode = V_m_ll = sqrt(2) * V_ll_rms
 *
 * For 3-pulse half-wave:
 *   V_dc = (3 * sqrt(3) / (2 * pi)) * V_m_ln ~= 0.827 * V_m_ln
 *   Ripple frequency = 3 * f_line
 *
 * Thomson (Tesla) noted this in 1888 — the 6-pulse bridge was the
 * foundation of early Niagara Falls DC transmission.
 *
 * Ref: Mohan Ch.5, Rashid Ch.3.6
 */
typedef struct {
    double V_dc;               /**< Average DC output voltage (V)                    */
    double V_rms_output;       /**< RMS output voltage (V)                           */
    double V_r_pp;             /**< Peak-to-peak ripple voltage (V)                  */
    double ripple_factor;      /**< Ripple factor                                    */
    double ripple_freq_Hz;     /**< Fundamental ripple frequency (Hz)                */
    double I_dc;               /**< Average DC load current (A)                      */
    double I_diode_avg;        /**< Average current per diode (A)                    */
    double I_diode_rms;        /**< RMS current per diode (A)                        */
    double I_diode_peak;       /**< Peak repetitive diode current (A)                */
    double PIV;                /**< Peak inverse voltage per diode (V)               */
    double P_dc;               /**< DC output power (W)                              */
    double efficiency;         /**< Rectification efficiency (0-1)                   */
    double commutation_angle_rad; /**< Overlap angle due to source inductance (rad)  */
    double V_drop_commutation; /**< Voltage drop due to commutation overlap (V)      */
    int    pulse_number;       /**< Pulse number (3 for half-wave, 6 for full)       */
} ThreePhaseResult;

/* ============================================================================
 * L1: Multi-Pulse Rectifier
 * ============================================================================ */

/**
 * MultiPulseConfig -- 12/18/24-pulse rectifier configuration.
 *
 * Higher pulse numbers reduce line current harmonics.
 * Used in aircraft (Airbus A380, Boeing 787 Dreamliner) and
 * industrial drives to meet IEEE 519 harmonic standards.
 *
 * 12-pulse: Two 6-pulse bridges with 30-degree phase shift transformer
 * 18-pulse: Three 6-pulse bridges with 20-degree phase shift
 * 24-pulse: Four 6-pulse bridges with 15-degree phase shift
 */
typedef struct {
    int    pulse_count;        /**< 12, 18, or 24                                     */
    double V_ll_rms;           /**< Primary line-to-line RMS voltage (V)              */
    double freq_Hz;            /**< Line frequency (Hz)                               */
    double R_load;             /**< Load resistance (ohm)                              */
    double V_f;                /**< Diode forward drop (V)                            */
} MultiPulseConfig;

/* ============================================================================
 * L1: Multi-Pulse Result
 * ============================================================================ */

/**
 * MultiPulseResult -- Output analysis for multi-pulse rectifier.
 */
typedef struct {
    double V_dc;
    double V_r_pp;
    double ripple_factor;
    double ripple_freq_Hz;
    double THD_input_current;  /**< Total harmonic distortion of input line current  */
    double fundamental_I_rms;  /**< Fundamental component of line current (A)         */
    int    dominant_harmonic;  /**< Dominant harmonic order (e.g., 11th for 12-pulse) */
    double efficiency;
} MultiPulseResult;

/* ============================================================================
 * L1: Thermal Management
 * ============================================================================ */

/**
 * ThermalConfig -- Thermal analysis configuration.
 *
 * Junction temperature estimation for reliability prediction.
 * Used extensively in automotive (Toyota, Detroit), aerospace (Boeing, NASA),
 * and industrial (Fukushima backup systems) applications.
 */
typedef struct {
    double Ta;                 /**< Ambient temperature (K)                           */
    double Tj_max;             /**< Maximum allowed junction temperature (K)          */
    double Rth_jc;             /**< Junction-to-case thermal resistance (K/W)         */
    double Rth_cs;             /**< Case-to-sink thermal resistance (K/W)             */
    double Rth_sa;             /**< Sink-to-ambient thermal resistance (K/W)          */
    double P_dissipated;       /**< Total power dissipated (W)                        */
} ThermalConfig;

/* ============================================================================
 * L1: Thermal Result
 * ============================================================================ */

/**
 * ThermalResult -- Junction temperature and safety margins.
 */
typedef struct {
    double Tj;                 /**< Estimated junction temperature (K)                */
    double Tc;                 /**< Estimated case temperature (K)                    */
    double Th;                 /**< Estimated heatsink temperature (K)                */
    double margin_K;           /**< Thermal margin (K), Tj_max - Tj                  */
    double Rth_sa_required;    /**< Required sink-to-ambient resistance (K/W)         */
    int    needs_heatsink;     /**< 1 if heatsink is required                         */
    double MTBF_factor;        /**< Relative MTBF vs. nominal (Arrhenius law)         */
} ThermalResult;

/* ============================================================================
 * L1: Power Loss Breakdown
 * ============================================================================ */

/**
 * PowerLossBreakdown -- Detailed power loss analysis.
 *
 * Separates losses into:
 *   - Conduction loss: V_f * I_avg per diode
 *   - Reverse recovery loss: f * E_rr per switching event
 *   - Leakage loss: V_r * I_r (typically negligible)
 */
typedef struct {
    double P_conduction;       /**< Total conduction loss (W)                          */
    double P_reverse_recovery; /**< Total reverse recovery loss (W)                    */
    double P_leakage;          /**< Total leakage loss (W)                             */
    double P_total;            /**< Total power loss (W)                               */
    double efficiency;         /**< Power efficiency P_out / (P_out + P_total)        */
} PowerLossBreakdown;

/* ============================================================================
 * L1: Inrush Current Analysis
 * ============================================================================ */

/**
 * InrushAnalysis -- Capacitor charging inrush current.
 *
 * When a rectifier first connects to AC mains with a discharged filter capacitor,
 * the inrush current can be enormous (limited only by source impedance).
 * This is a critical design consideration for:
 *   - iPhone USB charger (NTC thermistor limits inrush)
 *   - Tesla Supercharger (pre-charge circuit with contactor bypass)
 *   - Maglev train station converters (soft-start sequence)
 */
typedef struct {
    double I_peak_inrush;      /**< Peak inrush current (A)                            */
    double I2t;                /**< I^2 * t energy let-through (A^2.s)                */
    double t_charge_to_63pct;  /**< Time to charge to 63% of final voltage (s)        */
    double t_charge_to_90pct;  /**< Time to charge to 90% of final voltage (s)        */
    double NTC_resistance_cold;/**< Recommended NTC cold resistance (ohm)              */
    int    needs_precharge;    /**< 1 if pre-charge circuit recommended               */
} InrushAnalysis;

/* ============================================================================
 * Function Declarations
 * ============================================================================ */

/* --- L6: Three-Phase Rectifier Analysis --- */

/**
 * power_three_phase_analyze -- Full three-phase rectifier analysis.
 *
 * Includes commutation overlap effects from source inductance.
 * Commutation angle mu satisfies:
 *   cos(mu) = 1 - (2 * omega * L_s * I_dc) / (sqrt(3) * V_m_ll)
 *
 * V_drop_commutation = (3 * omega * L_s * I_dc) / pi for 6-pulse bridge.
 *
 * Complexity: O(1).
 */
ThreePhaseResult power_three_phase_analyze(const ThreePhaseConfig *cfg);

/* --- L8: Multi-Pulse Rectifier Analysis --- */

/**
 * power_multipulse_analyze -- Analyze 12/18/24-pulse rectifiers.
 *
 * Input current THD for ideal multi-pulse:
 *   6-pulse: THD ~= 30%
 *   12-pulse: THD ~= 15%
 *   18-pulse: THD ~= 10%
 *   24-pulse: THD ~= 7.5%
 *
 * Dominant harmonic = pulse_count +/- 1 (e.g., 11th and 13th for 12-pulse).
 *
 * Ref: IEEE 519-2014 harmonic limits.
 * Complexity: O(1).
 */
MultiPulseResult power_multipulse_analyze(const MultiPulseConfig *cfg);

/* --- L1: Thermal Analysis --- */

/**
 * power_thermal_analyze -- Compute junction temperature and thermal margin.
 *
 * Tj = Ta + P_dissipated * (Rth_jc + Rth_cs + Rth_sa)
 *
 * Arrhenius-based MTBF factor: exp(E_a/k * (1/T_ref - 1/Tj))
 * where E_a ~= 0.7 eV for semiconductor failure mechanisms.
 *
 * Complexity: O(1).
 */
ThermalResult power_thermal_analyze(const ThermalConfig *cfg);

/* --- L7: Power Loss Breakdown --- */

/**
 * power_loss_breakdown -- Detailed power loss analysis for rectifier.
 *
 * Conduction: P_cond = n_diodes * V_f * I_avg_per_diode
 * Reverse recovery: P_rr = n_diodes * f_line * E_rr_per_switch
 *   where E_rr ~= 0.5 * V_r * I_rr * t_rr (triangular approximation)
 *
 * Complexity: O(1).
 */
PowerLossBreakdown power_loss_breakdown(double V_f, double I_avg_total,
                                        int n_diodes_conducting,
                                        double f_line, double V_reverse,
                                        double I_rr, double t_rr);

/* --- L7: Inrush Current Analysis --- */

/**
 * power_inrush_analyze -- Analyze capacitor charging inrush.
 *
 * I_peak_inrush = V_m / R_source  (at t=0+, capacitor is a short)
 * I2t = integral of I^2 dt during charging: (V_m^2 * C) / (2 * R_source)
 *
 * Ref: Pressman "Switching Power Supply Design" 3rd Ed, Ch. 3.
 * Complexity: O(1).
 */
InrushAnalysis power_inrush_analyze(double V_m, double C_filter,
                                    double R_source, double L_source,
                                    double NTC_R25);

/* --- L7: Efficiency Comparison --- */

/**
 * power_efficiency_compare -- Compare single-phase vs three-phase efficiency.
 *
 * Three-phase has inherently higher efficiency due to:
 *   - Lower ripple (less filtering loss)
 *   - Higher TUF (better transformer utilization)
 *   - Balanced loading (no neutral current)
 *
 * Complexity: O(1).
 */
void power_efficiency_compare(double V_dc_target, double I_load,
                              double *eff_1ph_bridge, double *eff_3ph_bridge);

/* --- L8: Synchronous Rectifier Efficiency --- */

/**
 * power_synchronous_rectifier_efficiency -- Compare MOSFET vs diode rectification.
 *
 * Synchronous rectification replaces diodes with actively-driven MOSFETs.
 * R_DS(on) of modern MOSFETs can achieve V_drop << 100 mV vs 700 mV for Si diodes.
 *
 * Key for: Tesla vehicle DC-DC converters, iPhone fast chargers,
 *          data center power supplies (12V bus to PoL).
 *
 * Complexity: O(1).
 */
double power_synchronous_rectifier_efficiency(double I_load, double R_ds_on,
                                              double V_f_diode, double V_out);

/* --- L7: Line Current Harmonic Analysis --- */

/**
 * power_line_harmonics -- Compute line current harmonics for single-phase bridge.
 *
 * A single-phase bridge with capacitor filter draws highly non-sinusoidal
 * current. The harmonics are primarily odd-order (3rd, 5th, 7th, ...).
 *
 * IEC 61000-3-2 limits harmonics for equipment >75W.
 * This is why PFC (Power Factor Correction) is mandatory above certain power levels.
 *
 * Complexity: O(n_harmonics).
 *
 * @param n_harmonics  Number of harmonics to compute (max 20)
 * @param harmonics    Output array (caller-allocated, size n_harmonics)
 */
void power_line_harmonics(double I_fundamental, double conduction_angle_rad,
                          int n_harmonics, double *harmonics);

#endif /* POWER_RECTIFIER_H */