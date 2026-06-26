/**
 * rectifier_topology.h -- Rectifier Topology Types and Analysis
 *
 * Knowledge Coverage: L1 Definitions, L2 Core Concepts, L5 Algorithms, L6 Canonical Problems
 *
 * Reference: Sedra & Smith "Microelectronic Circuits" 8th Ed (2020), Ch. 4
 *            Mohan, Undeland & Robbins "Power Electronics" 3rd Ed (2003), Ch. 5
 *            Rashid "Power Electronics Handbook" 4th Ed (2018), Ch. 3
 *
 * Key Metrics:
 *   PIV (Peak Inverse Voltage) -- Maximum reverse voltage a diode must block
 *   TUF (Transformer Utilization Factor) -- DC output power / transformer VA rating
 *   Form Factor -- V_rms / V_dc
 *   Ripple Factor -- V_rms_ripple / V_dc
 *   Rectification Efficiency -- P_dc / P_ac_total
 */

#ifndef RECTIFIER_TOPOLOGY_H
#define RECTIFIER_TOPOLOGY_H

#include <stdint.h>

/* ============================================================================
 * L1: Rectifier Types Enum
 * ============================================================================ */

/**
 * RectifierType -- Enumerates all fundamental rectifier topologies.
 */
typedef enum {
    RECTIFIER_HALF_WAVE,           /**< Single diode, conducts half cycle          */
    RECTIFIER_FULL_WAVE_CT,        /**< Center-tapped transformer, 2 diodes         */
    RECTIFIER_BRIDGE,              /**< Full-wave bridge, 4 diodes                  */
    RECTIFIER_VOLTAGE_DOUBLER,     /**< Delon/Greinacher voltage doubler            */
    RECTIFIER_VOLTAGE_TRIPLER,     /**< Cascaded voltage tripler                    */
    RECTIFIER_COCKROFT_WALTON,     /**< N-stage Cockroft-Walton multiplier          */
    RECTIFIER_THREE_PHASE_HALF,    /**< Three-phase half-wave (3 diodes)            */
    RECTIFIER_THREE_PHASE_BRIDGE,  /**< Three-phase full bridge (6 diodes)          */
    RECTIFIER_SCR_CONTROLLED,      /**< Thyristor-based phase-controlled            */
    RECTIFIER_SYNCHRONOUS          /**< MOSFET-based synchronous rectification      */
} RectifierType;

/* ============================================================================
 * L1: Rectifier Configuration
 * ============================================================================ */

/**
 * RectifierConfig -- Input specification for rectifier analysis.
 */
typedef struct {
    RectifierType type;      /**< Topology type                                    */
    double V_in_rms;         /**< Input RMS voltage (V)                            */
    double V_in_peak;        /**< Input peak voltage (V), = V_in_rms * sqrt(2)     */
    double freq_Hz;          /**< Input frequency (Hz), typically 50 or 60         */
    double R_load;           /**< Load resistance (ohm), for resistive load        */
    double V_f;              /**< Diode forward voltage drop (V), default 0.7      */
    double transformer_ratio;/**< Transformer turns ratio, 1.0 = direct            */
} RectifierConfig;

/* ============================================================================
 * L1: Half-Wave Rectifier Result
 * ============================================================================ */

/**
 * HalfWaveResult -- Complete analysis of a half-wave rectifier.
 *
 * Key Formulas (purely resistive load, ideal diode):
 *   V_dc = V_m / pi                (average DC voltage)
 *   V_rms_output = V_m / 2         (RMS output voltage)
 *   PIV = V_m                      (peak inverse voltage)
 *   Ripple factor = 1.21           (121% ripple for HW resistive)
 *   Efficiency = 40.5%             (max theoretical, resistive load)
 *   TUF = 0.287                    (transformer utilization factor)
 *   Form factor = V_rms / V_dc = 1.57
 *
 * Ref: Sedra & Smith Sec.4.5, Rashid Ch.3.2
 */
typedef struct {
    double V_dc;              /**< Average (DC) output voltage (V)                 */
    double V_rms_output;      /**< RMS output voltage (V)                          */
    double I_dc;              /**< Average load current (A)                        */
    double I_rms;             /**< RMS load current (A)                            */
    double P_dc;              /**< DC output power (W)                             */
    double P_ac;              /**< AC input power (W)                              */
    double PIV;               /**< Peak inverse voltage (V)                        */
    double ripple_factor;     /**< Ripple factor (unitless), V_rms_ripple/V_dc     */
    double efficiency;        /**< Rectification efficiency (0-1)                  */
    double TUF;               /**< Transformer utilization factor                  */
    double form_factor;       /**< Form factor = V_rms / V_dc                      */
    double peak_repetitive_current; /**< Maximum repetitive diode current (A)      */
    double conduction_angle_rad;    /**< Diode conduction angle per cycle (rad)     */
} HalfWaveResult;

/* ============================================================================
 * L1: Full-Wave Center-Tapped Result
 * ============================================================================ */

/**
 * FullWaveCTResult -- Complete analysis of a full-wave center-tapped rectifier.
 *
 * Key Formulas:
 *   V_dc = 2 * V_m / pi            (twice the HW average)
 *   V_rms_output = V_m / sqrt(2)   (same as input RMS)
 *   PIV = 2 * V_m                  (important: twice the peak!)
 *   Ripple factor = 0.482          (48.2% without filter)
 *   Efficiency = 81.2%             (theoretical max, resistive load)
 *   TUF = 0.693                    (center-tapped transformer)
 *   Ripple frequency = 2 * f_line  (100 Hz for 50 Hz line)
 *
 * Ref: Sedra & Smith Sec.4.5, Rashid Ch.3.3
 */
typedef struct {
    double V_dc;
    double V_rms_output;
    double I_dc;
    double I_rms;
    double P_dc;
    double P_ac;
    double PIV;                    /**< = 2 * V_m for CT configuration              */
    double ripple_factor;
    double efficiency;
    double TUF;
    double form_factor;
    double ripple_freq_Hz;         /**< Ripple fundamental frequency (Hz)           */
    double peak_repetitive_current;
    double diode_avg_current;      /**< Average current per diode                   */
} FullWaveCTResult;

/* ============================================================================
 * L1: Bridge Rectifier Result
 * ============================================================================ */

/**
 * BridgeResult -- Complete analysis of a full-wave bridge rectifier.
 *
 * Key Formulas:
 *   V_dc = 2 * V_m / pi            (same as FW CT, minus 2*V_f in practice)
 *   PIV = V_m                      (half of CT configuration!)
 *   TUF = 0.812                    (better transformer utilization than CT)
 *   Ripple frequency = 2 * f_line
 *
 * Ref: Sedra & Smith Sec.4.5, Rashid Ch.3.4
 */
typedef struct {
    double V_dc;
    double V_rms_output;
    double I_dc;
    double I_rms;
    double P_dc;
    double P_ac;
    double PIV;                    /**< = V_m (key advantage over CT)               */
    double ripple_factor;
    double efficiency;
    double TUF;
    double form_factor;
    double ripple_freq_Hz;
    double peak_repetitive_current;
    double diode_avg_current;
    double total_diode_drop;       /**< = 2 * V_f (two diodes conduct at a time)    */
} BridgeResult;

/* ============================================================================
 * L1: Voltage Multiplier Configuration
 * ============================================================================ */

/**
 * VoltageMultiplierConfig -- Parameters for voltage multiplier analysis.
 */
typedef struct {
    int n_stages;            /**< Number of multiplier stages (>= 1)               */
    double V_in_peak;        /**< Input AC peak voltage (V)                        */
    double freq_Hz;          /**< Input frequency (Hz)                             */
    double C_stage;          /**< Capacitance per stage (F)                        */
    double I_load;           /**< Load current (A), 0 for no-load analysis          */
    double V_f;              /**< Diode forward voltage drop (V)                   */
} VoltageMultiplierConfig;

/* ============================================================================
 * L1: Voltage Multiplier Result
 * ============================================================================ */

/**
 * VoltageMultiplierResult -- Output of a voltage multiplier analysis.
 */
typedef struct {
    double V_out_no_load;     /**< Theoretical no-load output voltage (V)           */
    double V_out_load;        /**< Output voltage under load (V)                    */
    double V_ripple_pp;       /**< Peak-to-peak ripple voltage (V)                  */
    double V_drop;            /**< Voltage drop due to load current (V)             */
    double output_impedance;  /**< Effective output impedance (ohm)                  */
    double PIV_per_stage;     /**< Peak inverse voltage per diode stage (V)         */
    int    effective_stages;  /**< Effective number of stages under load            */
} VoltageMultiplierResult;

/* ============================================================================
 * L1: SCR Controlled Rectifier
 * ============================================================================ */

/**
 * SCRRectifierConfig -- Phase-controlled rectifier configuration.
 *
 * Used in: DC motor drives, Tesla battery charging, industrial DC supplies.
 */
typedef struct {
    RectifierType base_type;  /**< Must be RECTIFIER_SCR_CONTROLLED                 */
    double V_in_rms;          /**< Input AC RMS voltage (V)                         */
    double freq_Hz;           /**< Line frequency (Hz)                              */
    double R_load;            /**< Load resistance (ohm)                             */
    double alpha_rad;         /**< Firing angle (rad), 0 = full conduction           */
    double V_f;               /**< Diode/SCR forward drop (V)                       */
} SCRRectifierConfig;

/* ============================================================================
 * L1: SCR Rectifier Result
 * ============================================================================ */

/**
 * SCRRectifierResult -- DC output as function of firing angle.
 *
 * Half-wave: V_dc = V_m/(2*pi) * (1 + cos(alpha))
 * Bridge:    V_dc = V_m/pi * (1 + cos(alpha))
 *
 * Ref: Rashid Ch.4, Mohan Ch.5
 */
typedef struct {
    double V_dc;              /**< Average DC output voltage (V)                    */
    double V_rms_output;      /**< RMS output voltage (V)                          */
    double I_dc;              /**< Average DC load current (A)                     */
    double P_dc;              /**< DC output power (W)                             */
    double displacement_factor; /**< cos(alpha), fundamental displacement factor    */
    double power_factor;      /**< True power factor                               */
    double firing_angle_deg;  /**< Firing angle in degrees                         */
} SCRRectifierResult;

/* ============================================================================
 * Function Declarations
 * ============================================================================ */

/* --- L5: Half-Wave Analysis --- */

/**
 * rectifier_half_wave_analyze -- Analyze half-wave rectifier (resistive load).
 *
 * Computes all key metrics: V_dc, V_rms, PIV, ripple factor, efficiency, TUF.
 * Includes diode forward drop effects.
 *
 * Theorem source: Fourier series of half-wave rectified sine, Sedra & Smith Sec.4.5.
 * Complexity: O(1).
 */
HalfWaveResult rectifier_half_wave_analyze(const RectifierConfig *cfg);

/* --- L5: Full-Wave Center-Tapped Analysis --- */

/**
 * rectifier_fullwave_ct_analyze -- Analyze full-wave CT rectifier.
 *
 * Computes V_dc = 2*V_m/pi - V_f, PIV = 2*V_m, TUF = 0.693.
 * Complexity: O(1).
 */
FullWaveCTResult rectifier_fullwave_ct_analyze(const RectifierConfig *cfg);

/* --- L5: Bridge Rectifier Analysis --- */

/**
 * rectifier_bridge_analyze -- Analyze full-wave bridge rectifier.
 *
 * Computes V_dc = (2*V_m/pi) - 2*V_f, PIV = V_m - V_f.
 * The bridge is the most common topology in DC power supplies.
 * Complexity: O(1).
 */
BridgeResult rectifier_bridge_analyze(const RectifierConfig *cfg);

/* --- L5: Voltage Multiplier Analysis --- */

/**
 * rectifier_voltage_multiplier_analyze -- Analyze N-stage voltage multiplier.
 *
 * For N-stage Cockroft-Walton multiplier:
 *   V_out_no_load = 2 * N * V_m
 *   V_ripple = I_load / (f * C) * N * (N+1) / 2
 *   V_drop = I_load / (f * C) * (2/3 * N^3 + N^2/2 - N/6)
 *
 * Theorem source: Cockroft & Walton (1932), "Experiments with High Velocity Ions".
 * Complexity: O(1).
 */
VoltageMultiplierResult rectifier_voltage_multiplier_analyze(
    const VoltageMultiplierConfig *cfg);

/* --- L5: SCR Phase-Controlled Rectifier Analysis --- */

/**
 * rectifier_scr_analyze -- Analyze phase-controlled rectifier.
 *
 * V_dc(alpha) for bridge: V_m/pi * (1 + cos(alpha))
 * Output controlled by adjusting firing angle alpha.
 *
 * Applied in: Toyota hybrid vehicle motor drives, Tesla Supercharger front-end.
 * Complexity: O(1).
 */
SCRRectifierResult rectifier_scr_analyze(const SCRRectifierConfig *cfg);

/* --- L2: Optimal Topology Selection --- */

/**
 * rectifier_select_topology -- Recommend optimal rectifier topology.
 *
 * Decision criteria: V_in, I_load, efficiency target, cost constraints.
 * Considers PIV stress, diode count, transformer requirements.
 *
 * Complexity: O(1).
 */
RectifierType rectifier_select_topology(double V_in_rms, double I_load,
                                        int need_isolation, int cost_sensitive);

/* --- L2: Rectifier Comparison --- */

/**
 * rectifier_compare_topologies -- Compare two topologies side-by-side.
 *
 * Prints comparison table to stdout. Used for design education.
 * Complexity: O(1).
 */
void rectifier_compare_topologies(RectifierType t1, RectifierType t2,
                                  const RectifierConfig *cfg);

/* --- L6: DC Power Supply Voltage Estimation --- */

/**
 * rectifier_estimate_dc_voltage -- Quick estimate of DC output.
 *
 * For sine input: half-wave ~ 0.318*V_m, full-wave ~ 0.637*V_m.
 * With capacitor filter: approach V_m at light load.
 *
 * Complexity: O(1).
 */
double rectifier_estimate_dc_voltage(RectifierType type, double V_in_peak,
                                     double V_f);

/* --- L6: PIV Calculation --- */

/**
 * rectifier_calculate_piv -- Compute PIV for given type.
 *
 * Half-wave: PIV = V_m
 * Full-wave CT: PIV = 2*V_m
 * Bridge: PIV = V_m
 *
 * This is a critical safety parameter — exceeding PIV destroys diodes.
 * Complexity: O(1).
 */
double rectifier_calculate_piv(RectifierType type, double V_in_peak, double V_f);

/* --- L2: Ripple Frequency --- */

/**
 * rectifier_ripple_frequency -- Get fundamental ripple frequency.
 *
 * Half-wave: f_ripple = f_line
 * Full-wave/bridge: f_ripple = 2 * f_line
 * Three-phase bridge: f_ripple = 6 * f_line
 *
 * Complexity: O(1).
 */
double rectifier_ripple_frequency(RectifierType type, double line_freq_Hz);

#endif /* RECTIFIER_TOPOLOGY_H */