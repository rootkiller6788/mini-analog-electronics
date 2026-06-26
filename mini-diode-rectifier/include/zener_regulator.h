/**
 * zener_regulator.h -- Zener Diode Voltage Regulation
 *
 * Knowledge Coverage: L1 Definitions, L2 Core Concepts, L4 Fundamental Laws, L5 Algorithms, L6 Canonical Problems
 *
 * Reference: Sedra & Smith "Microelectronic Circuits" 8th Ed (2020), Ch. 4, Sec.4.4
 *            Horowitz & Hill "The Art of Electronics" 3rd Ed (2015), Ch. 1, Ch. 9
 *
 * Zener regulation is the simplest form of voltage regulation.
 * The Zener diode operates in reverse breakdown, maintaining a nearly
 * constant voltage across a wide range of reverse currents.
 *
 * Key Relationships:
 *   V_out = V_Z + I_Z * Z_Z     (Zener terminal voltage)
 *   I_Z = (V_in - V_out) / R_S - I_load
 *   Line regulation = delta_V_out / delta_V_in  (ideally << 1)
 *   Load regulation = delta_V_out / delta_I_load (ideally << 1)
 */

#ifndef ZENER_REGULATOR_H
#define ZENER_REGULATOR_H

#include <stdint.h>

/* ============================================================================
 * L1: Zener Diode Parameters
 * ============================================================================ */

/**
 * ZenerParams -- Electrical characteristics of a Zener diode.
 *
 * The Zener diode is characterized by:
 *   - V_Z: nominal Zener breakdown voltage
 *   - Z_Z: dynamic impedance in breakdown region (small = better regulation)
 *   - I_ZK: knee current (minimum current for stable regulation)
 *   - I_ZM: maximum rated current
 *   - P_max: maximum power dissipation
 *   - TC: temperature coefficient (mV/K), positive for V_Z > ~5.6V,
 *         negative for V_Z < ~5.6V (Zener vs. avalanche dominance)
 */
typedef struct {
    double V_Z_nom;          /**< Nominal Zener voltage (V) at I_ZT                 */
    double I_ZT;             /**< Test current for V_Z_nom (A)                      */
    double Z_Z;              /**< Dynamic impedance at I_ZT (ohm)                    */
    double I_ZK;             /**< Knee current (A), minimum for regulation           */
    double I_ZM;             /**< Maximum rated Zener current (A)                    */
    double P_max;            /**< Maximum power dissipation (W)                      */
    double T_C;              /**< Temperature coefficient (mV/K or %/K)             */
    double T_J_max;          /**< Maximum junction temperature (K)                   */
    double RTH_JA;           /**< Thermal resistance junction-to-ambient (K/W)       */
} ZenerParams;

/* ============================================================================
 * L1: Zener Regulator Design Input
 * ============================================================================ */

/**
 * ZenerRegulatorDesign -- User requirements for a Zener shunt regulator.
 *
 * Design task: given an unregulated DC input and a load current range,
 * determine the optimal series resistor R_S and confirm that the chosen
 * Zener diode can maintain regulation under all conditions.
 *
 * This maps to the classic textbook Zener regulator design problem:
 * "Design a Zener regulator for V_out = X, I_load = 0..Y mA, V_in = Z..W V"
 *
 * Ref: Sedra & Smith Example 4.7
 */
typedef struct {
    double V_in_min;         /**< Minimum unregulated DC input voltage (V)          */
    double V_in_max;         /**< Maximum unregulated DC input voltage (V)          */
    double V_out_target;     /**< Desired regulated output voltage (V)              */
    double I_load_min;       /**< Minimum load current (A), often 0                 */
    double I_load_max;       /**< Maximum load current (A)                          */
    double V_ripple_in_pp;   /**< Peak-to-peak input ripple voltage (V)             */
} ZenerRegulatorDesign;

/* ============================================================================
 * L1: Zener Regulator Design Result
 * ============================================================================ */

/**
 * ZenerRegulatorResult -- Complete design output for a Zener shunt regulator.
 */
typedef struct {
    double R_S;              /**< Series resistor value (ohm)                        */
    double P_RS_max;         /**< Maximum power dissipated in R_S (W)                */
    double I_Z_min;          /**< Minimum Zener current (A), at V_in_min, I_load_max */
    double I_Z_max;          /**< Maximum Zener current (A), at V_in_max, I_load_min */
    double P_Z_max;          /**< Maximum Zener power dissipation (W)                */
    double V_out_min;        /**< Worst-case minimum output voltage (V)              */
    double V_out_max;        /**< Worst-case maximum output voltage (V)              */
    double line_regulation;  /**< delta_V_out / delta_V_in (unitless)               */
    double load_regulation;  /**< delta_V_out / delta_I_load (ohm)                  */
    double efficiency_min;   /**< Minimum efficiency (at V_in_max, I_load_min)       */
    double efficiency_max;   /**< Maximum efficiency (at V_in_min, I_load_max)       */
    int    is_feasible;      /**< 1 if design constraints are satisfiable             */
    double T_J_max_op;       /**< Maximum junction temperature during operation (K)   */
    double margin_I_ZK;      /**< Safety margin above knee current (A)               */
} ZenerRegulatorResult;

/* ============================================================================
 * L1: Zener Operating Point
 * ============================================================================ */

/**
 * ZenerOpPoint -- Instantaneous operating point of a Zener shunt regulator.
 */
typedef struct {
    double V_in;             /**< Input voltage at this instant (V)                  */
    double V_out;            /**< Regulated output voltage (V)                       */
    double I_Z;              /**< Zener current (A)                                  */
    double I_load;           /**< Load current (A)                                   */
    double I_RS;             /**< Current through series resistor (A)                */
    double P_Z;              /**< Zener power dissipation (W)                        */
    double P_RS;             /**< Series resistor power dissipation (W)              */
} ZenerOpPoint;

/* ============================================================================
 * L1: Line Regulation Sweep
 * ============================================================================ */

/**
 * LineRegulationSweep -- Output voltage variation with input voltage.
 *
 * For R_S and Z_Z known:
 *   line_regulation = Z_Z / (R_S + Z_Z) ~= Z_Z / R_S   (when R_S >> Z_Z)
 */
typedef struct {
    int    n_points;         /**< Number of sweep points                             */
    double *V_in_array;      /**< Input voltage sweep values (V)                     */
    double *V_out_array;     /**< Corresponding output voltages (V)                  */
    double line_reg_slope;   /**< Fitted line regulation slope (dV_out/dV_in)       */
    double worst_case_delta; /**< Maximum V_out deviation across sweep (V)           */
} LineRegulationSweep;

/* ============================================================================
 * L1: Load Regulation Sweep
 * ============================================================================ */

/**
 * LoadRegulationSweep -- Output voltage variation with load current.
 *
 * load_regulation = Z_Z || R_S ~= Z_Z   (when Z_Z << R_S)
 */
typedef struct {
    int    n_points;
    double *I_load_array;    /**< Load current sweep values (A)                      */
    double *V_out_array;     /**< Corresponding output voltages (V)                  */
    double load_reg_slope;   /**< dV_out/dI_load, typically negative (ohm)           */
    double worst_case_delta;
} LoadRegulationSweep;

/* ============================================================================
 * L1: Zener Temperature Compensation
 * ============================================================================ */

/**
 * ZenerTempCompensation -- Temperature-compensated Zener reference.
 *
 * For precision voltage references, a forward-biased diode (negative TC)
 * can be placed in series with a Zener (positive TC for V_Z > 5.6V)
 * to achieve near-zero temperature coefficient.
 *
 * Ref: Horowitz & Hill Sec.9.7, "Precision Voltage References"
 */
typedef struct {
    double V_Z_alone;        /**< Zener voltage alone (V)                            */
    double V_f_comp;         /**< Forward voltage of compensation diode (V)          */
    double V_ref_total;      /**< Total compensated reference voltage (V)            */
    double TC_Z_alone;       /**< Zener temperature coefficient (mV/K)              */
    double TC_f_comp;        /**< Compensation diode TC (mV/K), typically -2 mV/K    */
    double TC_net;           /**< Net temperature coefficient (mV/K)                 */
    double optimal_I_Z;      /**< Zener current for optimal TC cancellation (A)      */
} ZenerTempCompensation;

/* ============================================================================
 * Function Declarations
 * ============================================================================ */

/* --- L5: Zener Regulator Design --- */

/**
 * zener_regulator_design -- Design a Zener shunt voltage regulator.
 *
 * Implements the complete textbook design procedure (Sedra & Smith Sec.4.4):
 * 1. Select Zener diode with V_Z ~= V_out_target
 * 2. Calculate R_S range: R_S_min = (V_in_max - V_out) / (I_Z_max + I_load_min)
 *    R_S_max = (V_in_min - V_out) / (I_Z_min + I_load_max)
 * 3. Choose R_S between min and max, check power ratings
 * 4. Verify thermal constraints
 * 5. Calculate line and load regulation
 *
 * Complexity: O(1).
 *
 * @param zener    Zener diode parameters
 * @param design   Design requirements
 * @param result   Output: complete design result (caller-allocated)
 */
void zener_regulator_design(const ZenerParams *zener,
                            const ZenerRegulatorDesign *design,
                            ZenerRegulatorResult *result);

/* --- L3: Zener Operating Point Calculation --- */

/**
 * zener_op_point -- Calculate instantaneous operating point.
 *
 * V_out = V_Z + I_Z * Z_Z  (linearized Zener model)
 * I_Z = (V_in - V_out) / R_S - I_load
 *
 * Solves the linear system for V_out.
 * Complexity: O(1).
 */
void zener_op_point(double V_in, double R_S, double I_load,
                    const ZenerParams *zener, ZenerOpPoint *op);

/* --- L3: Linearized Zener Output Voltage --- */

/**
 * zener_output_voltage -- Compute Zener terminal voltage under load.
 *
 * V_out = (V_in * Z_Z + V_Z * R_S) / (R_S + Z_Z)  -  I_load * (R_S || Z_Z)
 *
 * This is the exact linearized solution for the Zener shunt regulator.
 * Complexity: O(1).
 */
double zener_output_voltage(double V_in, double R_S, double I_load,
                            const ZenerParams *zener);

/* --- L6: Line Regulation Analysis --- */

/**
 * zener_line_regulation_sweep -- Sweep input voltage and compute output.
 *
 * Complexity: O(n_points).
 * Caller must free out->V_in_array and out->V_out_array.
 */
void zener_line_regulation_sweep(double V_in_start, double V_in_stop, int n_points,
                                 double R_S, double I_load, const ZenerParams *zener,
                                 LineRegulationSweep *out);

/* --- L6: Load Regulation Analysis --- */

/**
 * zener_load_regulation_sweep -- Sweep load current and compute output.
 *
 * Complexity: O(n_points).
 * Caller must free out->I_load_array and out->V_out_array.
 */
void zener_load_regulation_sweep(double I_load_start, double I_load_stop,
                                 int n_points, double V_in, double R_S,
                                 const ZenerParams *zener, LoadRegulationSweep *out);

/* --- L2: Zener Knee Current Check --- */

/**
 * zener_check_knee -- Verify Zener current stays above knee current.
 *
 * Returns 1 if I_Z_min >= I_ZK (safe), 0 otherwise.
 * Below I_ZK, the Zener exits the breakdown region and regulation fails.
 *
 * Complexity: O(1).
 */
int zener_check_knee(const ZenerParams *zener, double I_Z_min);

/* --- L2: Zener Power Dissipation Check --- */

/**
 * zener_check_power -- Verify Zener power stays within ratings.
 *
 * Returns 1 if P_Z_max <= P_max and T_J <= T_J_max, 0 otherwise.
 * Includes thermal derating: T_J = T_ambient + P_Z * RTH_JA.
 *
 * Complexity: O(1).
 */
int zener_check_power(const ZenerParams *zener, double P_Z_actual,
                      double T_ambient);

/* --- L3: Zener Dynamic Impedance Model --- */

/**
 * zener_dynamic_impedance -- Estimate dynamic impedance at given current.
 *
 * Z_Z(I_Z) ~= Z_ZT * sqrt(I_ZT / I_Z) for I_Z >> I_ZK (simplified model).
 * More accurate models incorporate the knee region roll-off.
 *
 * Complexity: O(1).
 */
double zener_dynamic_impedance(const ZenerParams *zener, double I_Z);

/* --- L4: Zener Breakdown Physics --- */

/**
 * zener_breakdown_type -- Classify breakdown mechanism based on voltage.
 *
 * V_Z < ~5V: Zener breakdown (tunneling), negative TC
 * V_Z ~ 5-6V: Mixed mechanism, near-zero TC
 * V_Z > ~6V: Avalanche breakdown, positive TC
 *
 * Ref: Sze Sec.2.5, "Breakdown Voltage"
 * Complexity: O(1).
 * @return: -1 for Zener-dominant, 0 for mixed, +1 for avalanche-dominant
 */
int zener_breakdown_type(double V_Z);

/* --- L5: Temperature-Compensated Reference Design --- */

/**
 * zener_temp_compensated_design -- Design temperature-compensated reference.
 *
 * Combines a Zener diode with a forward-biased compensation diode
 * to achieve near-zero temperature coefficient.
 *
 * Ref: Horowitz & Hill Sec.9.7.1
 * Complexity: O(1).
 */
void zener_temp_compensated_design(const ZenerParams *zener, double T_ambient,
                                   ZenerTempCompensation *out);

/* --- L6: Series Pass Transistor Boost --- */

/**
 * zener_series_pass_boost -- Compute improved regulation with pass transistor.
 *
 * Adding an NPN emitter follower after the Zener:
 *   V_out = V_Z - V_BE
 *   I_out_max = beta * I_Z_max (greatly increased)
 *   load_regulation improved by factor of beta
 *
 * Used in: iPhone charger reference design, Toyota ECU power supplies.
 *
 * Complexity: O(1).
 * @param beta  Transistor current gain (h_FE)
 * @param V_BE  Base-emitter voltage (V), typically 0.7
 */
void zener_series_pass_boost(const ZenerRegulatorResult *zener_only,
                             double beta, double V_BE,
                             double *V_out, double *I_out_max,
                             double *load_reg);

#endif /* ZENER_REGULATOR_H */