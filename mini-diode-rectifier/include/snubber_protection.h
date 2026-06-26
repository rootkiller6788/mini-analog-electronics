/**
 * snubber_protection.h -- Snubber Circuits and Diode Protection
 *
 * Knowledge Coverage: L2 Core Concepts, L5 Algorithms, L6 Canonical Problems, L8 Advanced Topics
 *
 * Reference: Mohan, Undeland & Robbins "Power Electronics" 3rd Ed (2003), Ch. 28
 *            Pressman, Billings & Morey "Switching Power Supply Design" 3rd Ed (2009), Ch. 7
 *            Paul "Introduction to Electromagnetic Compatibility" 2nd Ed (2006), Ch. 12
 *
 * Snubbers are essential protection circuits that:
 *   1. Limit voltage overshoot from parasitic inductance (turn-off snubber)
 *   2. Reduce switching losses (lossless snubber)
 *   3. Suppress EMI (RC snubber across transformer windings)
 *   4. Protect against reverse recovery voltage spikes
 *
 * Key applications: Toyota inverter IGBT protection, Boeing 787 APU rectifier,
 *                   iPhone charger transformer snubber, Tesla motor drive DC link.
 */

#ifndef SNUBBER_PROTECTION_H
#define SNUBBER_PROTECTION_H

#include <stdint.h>

/* ============================================================================
 * L1: Snubber Types
 * ============================================================================ */

/**
 * SnubberType -- Classification of snubber circuits.
 */
typedef enum {
    SNUBBER_RC_TURNOFF,        /**< RC snubber across switch/diode (dv/dt limiter)  */
    SNUBBER_RCD_TURNOFF,       /**< RCD polarized turn-off snubber                   */
    SNUBBER_RL_TURNON,         /**< Series inductor for di/dt limiting              */
    SNUBBER_LOSS_LESS,         /**< Energy recovery snubber (Cuk, Undeland)          */
    SNUBBER_CLAMP_RCD,         /**< RCD clamp for flyback/forward converters         */
    SNUBBER_DAMPER_RC,         /**< RC damper for transformer ringing                */
    SNUBBER_ZENER_CLAMP        /**< Zener/TVS clamp for overvoltage protection       */
} SnubberType;

/* ============================================================================
 * L1: RC Turn-off Snubber Configuration
 * ============================================================================ */

/**
 * RCSnubberConfig -- Design inputs for RC turn-off snubber.
 *
 * The RC snubber limits dv/dt across a diode during reverse recovery.
 *
 * Ref: Mohan Ch.28.2, Pressman Ch.7.
 */
typedef struct {
    double V_bus;              /**< DC bus voltage (V)                                */
    double I_load;             /**< Load current at turn-off (A)                      */
    double L_parasitic;        /**< Parasitic inductance in commutation loop (H)      */
    double C_parasitic;        /**< Parasitic capacitance (F) — could be diode C_j    */
    double f_switching;        /**< Switching frequency (Hz)                          */
    double dv_dt_max;          /**< Maximum allowed dv/dt (V/s)                       */
    double overshoot_fraction; /**< Allowed voltage overshoot fraction (0-1)          */
} RCSnubberConfig;

/* ============================================================================
 * L1: RC Snubber Design Result
 * ============================================================================ */

/**
 * RCSnubberResult -- Optimized RC snubber component values.
 *
 * Design procedure:
 * 1. Choose C_s to absorb parasitic energy: C_s >= L_p * I_load^2 / (V_overshoot^2)
 * 2. Choose R_s for critical damping: R_s = sqrt(L_p / C_s)
 * 3. Verify power in R_s: P_Rs = C_s * V_bus^2 * f_switching
 * 4. Verify dv/dt: dv/dt_max = V_bus / (R_s * C_s), adjust if needed
 */
typedef struct {
    double C_s;                /**< Snubber capacitance (F)                            */
    double R_s;                /**< Snubber resistance (ohm)                           */
    double P_Rs;               /**< Power dissipated in snubber resistor (W)          */
    double V_overshoot_peak;   /**< Peak voltage with snubber (V)                     */
    double dv_dt_actual;       /**< Actual dv/dt with snubber (V/s)                   */
    double t_settle;           /**< Time to settle within 5% (s), ~3*R_s*C_s          */
    int    is_valid;           /**< 1 if design meets constraints                      */
    double damping_factor;     /**< Damping factor zeta (1 = critically damped)       */
} RCSnubberResult;

/* ============================================================================
 * L1: Reverse Recovery Parameters
 * ============================================================================ */

/**
 * ReverseRecoveryParams -- Diode reverse recovery characterization.
 *
 * When a diode switches from forward conduction to reverse blocking,
 * stored minority carriers must be removed, causing a reverse current pulse.
 * This is a major source of switching loss and EMI in rectifier circuits.
 *
 * Key parameters from datasheet:
 *   - t_rr: reverse recovery time
 *   - Q_rr: reverse recovery charge
 *   - I_rrm: peak reverse recovery current
 *   - S_factor: snappiness factor = t_b / t_a
 *
 * Soft recovery (S < 0.8): low EMI, lower efficiency
 * Snappy recovery (S > 1): higher EMI, better efficiency
 * SiC/GaN diodes have essentially zero Q_rr (majority carrier devices).
 *
 * Ref: Sze Sec.2.6, Mohan Ch.28.1, Baliga "Advanced Power MOSFET Concepts" (2010)
 */
typedef struct {
    double t_rr;               /**< Reverse recovery time (s)                          */
    double Q_rr;               /**< Reverse recovery charge (C)                        */
    double I_rrm;              /**< Peak reverse recovery current (A)                  */
    double S_factor;           /**< Snappiness factor t_b/t_a (0.3-1.5 typical)        */
    double I_F;                /**< Forward current before commutation (A)             */
    double di_dt;              /**< Rate of current decrease (A/s), positive value    */
    double V_R;                /**< Reverse voltage applied (V)                        */
    double T_j;                /**< Junction temperature (K)                           */
} ReverseRecoveryParams;

/* ============================================================================
 * L1: Reverse Recovery Analysis
 * ============================================================================ */

/**
 * ReverseRecoveryResult -- Detailed reverse recovery analysis.
 *
 * Energy lost per switching event:
 *   E_rr ~= 0.5 * V_R * Q_rr + 0.25 * V_R * I_rrm * t_b
 * For snappy diodes (S > 1): additional ringing energy
 *
 * Peak reverse voltage overshoot (without snubber):
 *   V_overshoot = L_loop * di_dt + V_R
 *   May cause destructive overvoltage!
 */
typedef struct {
    double E_rr_per_switch;    /**< Energy loss per switching event (J)               */
    double P_rr_total;         /**< Total reverse recovery power loss (W)             */
    double t_a;                /**< Storage time — current from 0 to -I_rrm (s)       */
    double t_b;                /**< Fall time — current from -I_rrm to 0 (s)          */
    double V_overshoot_no_snubber; /**< Voltage spike without snubber (V)             */
    double I_rrm_calculated;   /**< Calculated peak reverse current (A)               */
} ReverseRecoveryResult;

/* ============================================================================
 * L1: Diode Snubber Design Input
 * ============================================================================ */

/**
 * SnubberDesignInput -- Comprehensive snubber design request.
 */
typedef struct {
    double V_bus;              /**< DC bus voltage (V)                                */
    double I_load_max;         /**< Maximum load current (A)                          */
    double L_loop;             /**< Total loop parasitic inductance (H)               */
    double f_op;               /**< Operating/switching frequency (Hz)                */
    double V_overshoot_max;    /**< Maximum allowed voltage overshoot (V)             */
    ReverseRecoveryParams rr;  /**< Diode reverse recovery data                       */
} SnubberDesignInput;

/* ============================================================================
 * L1: Complete Snubber Design
 * ============================================================================ */

/**
 * SnubberDesignOutput -- Complete snubber design with all options.
 */
typedef struct {
    RCSnubberResult rc;        /**< RC turn-off snubber design                         */
    double C_clamp;            /**< RCD clamp capacitor (F)                            */
    double R_clamp;            /**< RCD clamp resistor (ohm)                           */
    double P_total_snubber;    /**< Total snubber power dissipation (W)                */
    double efficiency_impact;  /**< Efficiency reduction due to snubber (0-1)         */
    SnubberType recommended;   /**< Recommended snubber type                          */
    int    requires_tvs;       /**< 1 if additional TVS clamp is needed               */
} SnubberDesignOutput;

/* ============================================================================
 * L1: TVS (Transient Voltage Suppressor) Parameters
 * ============================================================================ */

/**
 * TVSParams -- TVS diode characteristics for overvoltage protection.
 *
 * TVS diodes (also called TransZorb or Transil) are designed to
 * absorb large transient energy pulses while clamping voltage.
 *
 * Used in: Boeing 787 lightning protection, Fukushima radiation-hard
 *          supplies, automotive load dump (ISO 7637-2), USB VBUS protection.
 */
typedef struct {
    double V_RWM;              /**< Reverse stand-off voltage (V), maximum working   */
    double V_BR;               /**< Breakdown voltage at I_T (V)                      */
    double V_C;                /**< Clamping voltage at I_PP (V)                      */
    double I_PP;               /**< Peak pulse current (A)                             */
    double P_PPM;              /**< Peak pulse power (W), at 10/1000 us waveform      */
    double C_j;                /**< Junction capacitance (F), affects signal speed    */
} TVSParams;

/* ============================================================================
 * Function Declarations
 * ============================================================================ */

/* --- L5: RC Snubber Design --- */

/**
 * snubber_rc_design -- Design optimized RC turn-off snubber.
 *
 * Implements the classic RC snubber design procedure:
 * 1. C_s = L_p * I^2 / (V_overshoot^2) — energy absorption
 * 2. R_s = sqrt(L_p / C_s) — critical damping
 * 3. Verify P_Rs = C_s * V^2 * f can be handled
 * 4. Check dv/dt constraint
 *
 * Theorem source: McMurray (1972), "Optimum Snubbers for Power Semiconductors".
 * Complexity: O(1).
 */
RCSnubberResult snubber_rc_design(const RCSnubberConfig *cfg);

/* --- L5: Reverse Recovery Analysis --- */

/**
 * snubber_reverse_recovery_analyze -- Analyze reverse recovery behavior.
 *
 * Using the charge model:
 *   Q_rr = 0.5 * I_rrm * t_rr  (triangular approximation)
 *   t_rr = sqrt(2 * Q_rr / (di/dt)) for S=1
 *
 * Temperature dependence: Q_rr doubles every 40-50 K for silicon diodes.
 *
 * Complexity: O(1).
 */
ReverseRecoveryResult snubber_reverse_recovery_analyze(
    const ReverseRecoveryParams *params);

/* --- L6: Complete Snubber Design --- */

/**
 * snubber_design_complete -- Full snubber design for rectifier diode.
 *
 * Evaluates all snubber options and selects the optimal configuration
 * for the given operating conditions.
 *
 * Complexity: O(1).
 */
SnubberDesignOutput snubber_design_complete(const SnubberDesignInput *input);

/* --- L5: RCD Clamp Design --- */

/**
 * snubber_rcd_clamp_design -- Design RCD clamp for flyback/forward converter.
 *
 * The RCD clamp limits the drain/gate voltage spike when the switch turns off,
 * recycling leakage inductance energy back to the DC bus.
 *
 * V_clamp = V_bus + V_zener (or designed clamp voltage)
 * P_clamp = 0.5 * L_leakage * I_peak^2 * f_switching
 *
 * Complexity: O(1).
 */
void snubber_rcd_clamp_design(double V_bus, double L_leakage, double I_peak,
                              double f_sw, double V_clamp_target,
                              double *R_clamp, double *C_clamp, double *P_R);

/* --- L6: Transformer Ringing Damper --- */

/**
 * snubber_transformer_damper -- Design RC damper for transformer secondary.
 *
 * Transformer leakage inductance and winding capacitance form a resonant
 * tank that rings when diodes switch. An RC damper across the secondary
 * suppresses this ringing, reducing EMI.
 *
 * R_damp = sqrt(L_leakage / C_winding)  (characteristic impedance)
 * C_damp ~= 3 * C_winding  (3x for effective damping)
 *
 * Ref: Paul Ch.12, "EMI Filtering and Shielding".
 * Complexity: O(1).
 */
void snubber_transformer_damper(double L_leakage, double C_winding,
                                double f_ring, double V_secondary_max,
                                double *R_damp, double *C_damp, double *P_R);

/* --- L2: TVS Selection --- */

/**
 * snubber_tvs_select -- Select appropriate TVS diode for protection.
 *
 * Selection criteria:
 * 1. V_RWM >= V_operating_max (stand-off above normal operating)
 * 2. V_C <= V_damage of protected circuit
 * 3. P_PPM >= expected surge power (IEC 61000-4-5 surge levels)
 *
 * Complexity: O(1).
 */
void snubber_tvs_select(double V_operating_max, double V_damage,
                        double I_surge, double t_surge,
                        TVSParams *selected_tvs);

/* --- L8: SiC/GaN Diode Advantage Quantification --- */

/**
 * snubber_wide_bandgap_benefit -- Quantify SiC/GaN over Si in rectifier.
 *
 * SiC Schottky diodes: Q_rr ~= 0 (majority carrier, no stored charge)
 * Si fast recovery: Q_rr ~= 100-1000 nC at 400V/10A
 *
 * Result: SiC/GaN allows smaller/no snubber, higher frequency, higher efficiency.
 *
 * Used in: Tesla Model 3 onboard charger, iPhone GaN charger,
 *          NASA Juno spacecraft power supply, maglev traction converters.
 *
 * Complexity: O(1).
 */
double snubber_wide_bandgap_benefit(double V_bus, double I_load, double f_sw,
                                    double Q_rr_si, double V_f_si,
                                    double R_ds_on_gan, double V_f_sic);

/* --- L6: Parasitic Inductance Estimation --- */

/**
 * snubber_estimate_loop_inductance -- Estimate commutation loop L.
 *
 * PCB trace inductance: ~10 nH/cm for typical power traces.
 * TO-220 package: ~5-10 nH.
 * Total loop = trace + package + capacitor ESL.
 *
 * Complexity: O(1).
 */
double snubber_estimate_loop_inductance(double trace_length_cm,
                                        double trace_width_mm,
                                        double loop_height_mm,
                                        int n_devices);

#endif /* SNUBBER_PROTECTION_H */