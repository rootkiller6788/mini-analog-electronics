/**
 * zener_regulator.c -- Zener Diode Voltage Regulation Implementation
 *
 * Implements Zener shunt regulator design, line/load regulation analysis,
 * and temperature-compensated reference design.
 *
 * Reference: Sedra & Smith Sec.4.4; Horowitz & Hill Ch.9
 */

#include "zener_regulator.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * L5: Zener Regulator Design
 * ============================================================================ */

void zener_regulator_design(const ZenerParams *zener,
                            const ZenerRegulatorDesign *design,
                            ZenerRegulatorResult *result)
{
    /* Complete Zener shunt regulator design procedure.
     *
     * Design steps (Sedra & Smith Example 4.7):
     *
     * 1. Zener voltage selection: V_Z should equal V_out_target.
     *    In practice, use the nearest standard value.
     *
     * 2. Series resistor R_S must satisfy two constraints:
     *    (a) At V_in_min and I_load_max, I_Z must stay above I_ZK:
     *        R_S < (V_in_min - V_out) / (I_ZK + I_load_max)
     *    (b) At V_in_max and I_load_min, I_Z must stay below I_ZM:
     *        R_S > (V_in_max - V_out) / (I_ZM + I_load_min)
     *
     * 3. If R_S_max < R_S_min, the design is infeasible.
     *    Choose R_S in the middle of the feasible range.
     *
     * 4. Verify power dissipation:
     *    P_Z_max = I_Z_max * (V_Z + I_Z_max * Z_Z)
     *    Must be < P_max with thermal derating.
     *
     * 5. Calculate line regulation:
     *    line_reg = Z_Z / (R_S + Z_Z) ≈ Z_Z / R_S
     *
     * 6. Calculate load regulation:
     *    load_reg = Z_Z || R_S ≈ Z_Z (when R_S >> Z_Z)
     *
     * The Zener regulator is simple but inefficient for large V_in/V_out
     * ratios because the excess voltage is dropped across R_S.
     *
     * Ref: Sedra & Smith Sec.4.4.3, Design Example 4.7
     */
    if (!zener || !design || !result) return;

    /* Initialize result to zero */
    *result = (ZenerRegulatorResult){0};

    double V_out = design->V_out_target;
    double V_in_min = design->V_in_min;
    double V_in_max = design->V_in_max;
    double I_L_min = design->I_load_min;
    double I_L_max = design->I_load_max;

    /* Step 1: Verify Zener voltage is close to target */
    double V_Z_diff = fabs(zener->V_Z_nom - V_out);
    if (V_Z_diff > V_out * 0.3) {
        /* Zener voltage too far from target — poor regulation expected */
        result->is_feasible = 0;
        return;
    }

    /* Step 2: Calculate R_S bounds */
    double R_S_max = (V_in_min - V_out) / (zener->I_ZK + I_L_max);
    double R_S_min = (V_in_max - V_out) / (zener->I_ZM + I_L_min);

    if (R_S_max < R_S_min) {
        /* No feasible R_S exists */
        result->is_feasible = 0;
        return;
    }

    /* Choose middle value */
    result->R_S = (R_S_min + R_S_max) / 2.0;

    /* Step 3: Calculate extreme operating conditions */
    result->I_Z_min = (V_in_min - V_out) / result->R_S - I_L_max;
    result->I_Z_max = (V_in_max - V_out) / result->R_S - I_L_min;

    /* Step 4: Power calculations */
    result->P_Z_max = result->I_Z_max * (zener->V_Z_nom + result->I_Z_max * zener->Z_Z);
    result->P_RS_max = (V_in_max - V_out) * (V_in_max - V_out) / result->R_S;

    /* Step 5: Worst-case output voltages */
    result->V_out_min = zener_output_voltage(V_in_min, result->R_S, I_L_max, zener);
    result->V_out_max = zener_output_voltage(V_in_max, result->R_S, I_L_min, zener);

    /* Step 6: Regulation metrics */
    result->line_regulation = zener->Z_Z / (result->R_S + zener->Z_Z);
    result->load_regulation = (zener->Z_Z * result->R_S) / (zener->Z_Z + result->R_S);

    /* Step 7: Efficiency */
    double P_out_max = V_out * I_L_max;
    double P_in_max = V_in_max * (I_L_max + result->I_Z_min);
    result->efficiency_max = (P_in_max > 0.0) ? P_out_max / P_in_max : 0.0;

    double P_out_min = V_out * I_L_min;
    double P_in_min = V_in_min * (I_L_min + result->I_Z_max);
    result->efficiency_min = (P_in_min > 0.0) ? P_out_min / P_in_min : 0.0;

    /* Step 8: Thermal and knee margin */
    result->margin_I_ZK = result->I_Z_min - zener->I_ZK;
    result->T_J_max_op = 300.0 + result->P_Z_max * zener->RTH_JA;

    /* Feasibility check */
    result->is_feasible = 1;
    if (result->margin_I_ZK < 0.0) {
        result->is_feasible = 0;  /* Below knee — regulation fails */
    }
    if (result->P_Z_max > zener->P_max) {
        result->is_feasible = 0;  /* Exceeds power rating */
    }
    if (result->T_J_max_op > zener->T_J_max) {
        result->is_feasible = 0;  /* Thermal violation */
    }
}

/* ============================================================================
 * L3: Zener Operating Point
 * ============================================================================ */

void zener_op_point(double V_in, double R_S, double I_load,
                    const ZenerParams *zener, ZenerOpPoint *op)
{
    /* Instantaneous operating point of a Zener shunt regulator.
     *
     * The circuit is a simple voltage divider plus nonlinear Zener:
     *   I_RS = (V_in - V_out) / R_S
     *   I_Z = I_RS - I_load
     *   V_out = V_Z + I_Z * Z_Z  (linearized model)
     *
     * Solving: V_out = V_Z + Z_Z * ((V_in - V_out)/R_S - I_load)
     *   V_out * (1 + Z_Z/R_S) = V_Z + Z_Z * V_in / R_S - Z_Z * I_load
     *   V_out = (V_Z * R_S + Z_Z * V_in - Z_Z * R_S * I_load) / (R_S + Z_Z)
     */
    if (!zener || !op) return;

    op->V_in = V_in;
    op->I_load = I_load;
    op->V_out = zener_output_voltage(V_in, R_S, I_load, zener);

    op->I_RS = (V_in - op->V_out) / R_S;
    if (op->I_RS < 0.0) op->I_RS = 0.0;

    op->I_Z = op->I_RS - I_load;
    if (op->I_Z < 0.0) op->I_Z = 0.0;

    op->P_Z = op->I_Z * op->V_out;
    op->P_RS = op->I_RS * op->I_RS * R_S;
}

/* ============================================================================
 * L3: Linearized Zener Output Voltage
 * ============================================================================ */

double zener_output_voltage(double V_in, double R_S, double I_load,
                            const ZenerParams *zener)
{
    /* V_out = (V_Z*R_S + Z_Z*V_in - Z_Z*R_S*I_load) / (R_S + Z_Z)
     *
     * This is the exact solution of the linearized Zener regulator.
     * The denominator R_S + Z_Z sets the regulation quality:
     * smaller Z_Z relative to R_S → better regulation.
     *
     * For small Z_Z << R_S:
     *   V_out ≈ V_Z + Z_Z*(V_in/R_S - I_load)  → V_out ≈ V_Z
     *
     * The Zener current is: I_Z = (V_in - V_out)/R_S - I_load
     */
    if (!zener || R_S <= 0.0) return 0.0;

    double V_Z = zener->V_Z_nom;
    double Z_Z = zener->Z_Z;
    double denom = R_S + Z_Z;

    return (V_Z * R_S + Z_Z * V_in - Z_Z * R_S * I_load) / denom;
}

/* ============================================================================
 * L6: Line Regulation Sweep
 * ============================================================================ */

void zener_line_regulation_sweep(double V_in_start, double V_in_stop, int n_points,
                                 double R_S, double I_load, const ZenerParams *zener,
                                 LineRegulationSweep *out)
{
    /* Sweep input voltage and record output voltage.
     *
     * Line regulation is defined as dV_out/dV_in at constant I_load.
     * For an ideal regulator, this would be 0 (perfect rejection).
     * For a Zener: line_reg ≈ Z_Z / R_S (typically 0.1-5%).
     *
     * This function allocates memory for the sweep arrays.
     * Caller must free out->V_in_array and out->V_out_array.
     */
    if (!zener || !out || n_points < 2) return;

    out->n_points = n_points;
    out->V_in_array = (double *)malloc((size_t)n_points * sizeof(double));
    out->V_out_array = (double *)malloc((size_t)n_points * sizeof(double));

    if (!out->V_in_array || !out->V_out_array) {
        free(out->V_in_array);
        free(out->V_out_array);
        out->V_in_array = NULL;
        out->V_out_array = NULL;
        out->n_points = 0;
        return;
    }

    double step = (V_in_stop - V_in_start) / (double)(n_points - 1);
    double sum_dV_out = 0.0, sum_dV_in = 0.0;
    double prev_V_out = 0.0;
    out->worst_case_delta = 0.0;

    for (int i = 0; i < n_points; i++) {
        out->V_in_array[i] = V_in_start + step * (double)i;
        out->V_out_array[i] = zener_output_voltage(out->V_in_array[i], R_S,
                                                    I_load, zener);
        if (i > 0) {
            double dV = out->V_out_array[i] - prev_V_out;
            if (fabs(dV) > out->worst_case_delta)
                out->worst_case_delta = fabs(dV);
            sum_dV_out += dV;
            sum_dV_in += step;
        }
        prev_V_out = out->V_out_array[i];
    }

    if (sum_dV_in > 0.0)
        out->line_reg_slope = sum_dV_out / sum_dV_in;
    else
        out->line_reg_slope = zener->Z_Z / (R_S + zener->Z_Z);
}

/* ============================================================================
 * L6: Load Regulation Sweep
 * ============================================================================ */

void zener_load_regulation_sweep(double I_load_start, double I_load_stop,
                                 int n_points, double V_in, double R_S,
                                 const ZenerParams *zener, LoadRegulationSweep *out)
{
    /* Sweep load current and record output voltage.
     *
     * Load regulation = dV_out/dI_load, typically reported in mV/mA or %.
     * For Zener: load_reg ≈ Z_Z (when Z_Z << R_S).
     *
     * The output voltage drops as load current increases because
     * more current flows through the load (less through Zener),
     * and the Zener's internal impedance causes a slight voltage change.
     */
    if (!zener || !out || n_points < 2) return;

    out->n_points = n_points;
    out->I_load_array = (double *)malloc((size_t)n_points * sizeof(double));
    out->V_out_array = (double *)malloc((size_t)n_points * sizeof(double));

    if (!out->I_load_array || !out->V_out_array) {
        free(out->I_load_array);
        free(out->V_out_array);
        out->I_load_array = NULL;
        out->V_out_array = NULL;
        out->n_points = 0;
        return;
    }

    double step = (I_load_stop - I_load_start) / (double)(n_points - 1);
    double sum_dV = 0.0, sum_dI = 0.0;
    double prev_V_out = 0.0;
    out->worst_case_delta = 0.0;

    for (int i = 0; i < n_points; i++) {
        out->I_load_array[i] = I_load_start + step * (double)i;
        out->V_out_array[i] = zener_output_voltage(V_in, R_S,
                                                    out->I_load_array[i], zener);
        if (i > 0) {
            double dV = out->V_out_array[i] - prev_V_out;
            if (fabs(dV) > out->worst_case_delta)
                out->worst_case_delta = fabs(dV);
            sum_dV += dV;
            sum_dI += step;
        }
        prev_V_out = out->V_out_array[i];
    }

    if (fabs(sum_dI) > 1e-15)
        out->load_reg_slope = sum_dV / sum_dI;
    else
        out->load_reg_slope = -(zener->Z_Z * R_S) / (zener->Z_Z + R_S);
}

/* ============================================================================
 * L2: Zener Knee Current Check
 * ============================================================================ */

int zener_check_knee(const ZenerParams *zener, double I_Z_min)
{
    /* Verify the Zener stays in the breakdown region.
     *
     * Below I_ZK (knee current), the Zener exits breakdown and
     * the voltage collapses, losing all regulation.
     *
     * The knee is where the I-V curve transitions from the sharp
     * breakdown slope to the softer leakage region. Operating in
     * the knee region causes increased noise and poor regulation.
     *
     * Design rule: I_Z_min should be at least 2-5x I_ZK for
     * reliable operation over temperature and manufacturing spread.
     */
    if (!zener) return 0;
    return (I_Z_min >= zener->I_ZK) ? 1 : 0;
}

/* ============================================================================
 * L2: Zener Power Dissipation Check
 * ============================================================================ */

int zener_check_power(const ZenerParams *zener, double P_Z_actual,
                      double T_ambient)
{
    /* Verify Zener power and thermal limits.
     *
     * Power derating: P_max(T) = P_max(25C) * (T_J_max - T_ambient) / (T_J_max - 25)
     *   where T_J_max typically 150-200C for silicon.
     *
     * With heatsink: T_J = T_ambient + P_Z * (RTH_JC + RTH_CS + RTH_SA)
     * Without heatsink: T_J = T_ambient + P_Z * RTH_JA
     */
    if (!zener) return 0;

    /* Power check */
    if (P_Z_actual > zener->P_max) return 0;

    /* Thermal check */
    double T_J = T_ambient + P_Z_actual * zener->RTH_JA;
    if (T_J > zener->T_J_max) return 0;

    return 1;
}

/* ============================================================================
 * L3: Zener Dynamic Impedance Model
 * ============================================================================ */

double zener_dynamic_impedance(const ZenerParams *zener, double I_Z)
{
    /* Dynamic impedance varies with operating current.
     *
     * Simplified model: Z_Z(I_Z) ≈ Z_ZT * (I_ZT / I_Z)^k
     * where k ≈ 0.3-0.5 for most Zener diodes.
     *
     * At high currents: Z_Z decreases (better regulation)
     * At low currents (near I_ZK): Z_Z increases sharply
     *
     * The trade-off: higher I_Z improves regulation but wastes power.
     * Typical design compromise: I_Z ≈ 20% of I_ZM.
     */
    if (!zener || I_Z <= 0.0) return zener ? zener->Z_Z : 10.0;

    if (I_Z < zener->I_ZK) {
        /* Below knee: impedance rises dramatically */
        return zener->Z_Z * pow(zener->I_ZK / I_Z, 0.7);
    }

    /* Above knee: impedance decreases with current */
    return zener->Z_Z * pow(zener->I_ZT / I_Z, 0.4);
}

/* ============================================================================
 * L4: Zener Breakdown Physics Classification
 * ============================================================================ */

int zener_breakdown_type(double V_Z)
{
    /* Classify breakdown mechanism based on Zener voltage.
     *
     * Zener breakdown (quantum tunneling):
     *   - Dominant for V_Z < ~5.6V
     *   - Negative temperature coefficient (TC < 0)
     *   - Electrons tunnel through the narrow depletion region
     *   - Heavily doped junctions (thin depletion region)
     *
     * Avalanche breakdown (impact ionization):
     *   - Dominant for V_Z > ~5.6V
     *   - Positive temperature coefficient (TC > 0)
     *   - Carriers gain enough energy in the depletion field to
     *     create electron-hole pairs via collision
     *   - Lightly doped junctions (wide depletion region)
     *
     * At V_Z ≈ 5.6V: both mechanisms contribute equally,
     * giving near-zero temperature coefficient. This is why
     * precision 5.6V Zeners are used as voltage references.
     *
     * Ref: Sze Sec.2.5.2, "Breakdown Voltage"
     */
    if (V_Z < 4.5) return -1;  /* Zener-dominated */
    if (V_Z > 6.5) return 1;   /* Avalanche-dominated */
    return 0;                   /* Mixed / near-zero TC */
}

/* ============================================================================
 * L5: Temperature-Compensated Reference Design
 * ============================================================================ */

void zener_temp_compensated_design(const ZenerParams *zener, double T_ambient,
                                   ZenerTempCompensation *out)
{
    /* Temperature-compensated Zener reference.
     *
     * A forward-biased silicon diode has TC ≈ -2 mV/K.
     * A Zener above ~5.6V has TC ≈ +2 mV/K.
     *
     * By placing them in series, the TCs cancel:
     *   V_ref = V_Z + V_f
     *   TC_net = TC_Z + TC_f ≈ 0
     *
     * The optimal Zener current for cancellation depends on
     * the exact TC of the particular device, but typically:
     *   - For V_Z = 6.2V, optimal I_Z ≈ 5-10 mA
     *   - Net TC < 5 ppm/K achievable with careful selection
     *
     * This is the principle behind the 1N829 temperature-compensated
     * reference diode and the LM399/LTZ1000 precision references
     * used in high-precision instruments (6.5+ digit multimeters).
     *
     * Ref: Horowitz & Hill Sec.9.7.1
     */
    if (!zener || !out) return;

    out->V_Z_alone = zener->V_Z_nom;
    out->V_f_comp = 0.65;  /* Typical silicon forward voltage at 5mA */
    out->V_ref_total = out->V_Z_alone + out->V_f_comp;

    /* TC estimation from breakdown type */
    int breakdown = zener_breakdown_type(zener->V_Z_nom);
    if (breakdown > 0) {
        /* Avalanche: positive TC, roughly +1 to +3 mV/K */
        out->TC_Z_alone = 2.0;  /* mV/K */
    } else if (breakdown < 0) {
        /* Zener: negative TC, roughly -1 to -3 mV/K */
        out->TC_Z_alone = -2.0;  /* mV/K */
    } else {
        /* Mixed: near-zero already */
        out->TC_Z_alone = 0.3;
    }

    out->TC_f_comp = -2.0;  /* mV/K, standard silicon diode TC */
    out->TC_net = out->TC_Z_alone + out->TC_f_comp;
    out->optimal_I_Z = 0.0075;  /* 7.5 mA, typical optimum */
}

/* ============================================================================
 * L6: Series Pass Transistor Boost
 * ============================================================================ */

void zener_series_pass_boost(const ZenerRegulatorResult *zener_only,
                             double beta, double V_BE,
                             double *V_out, double *I_out_max,
                             double *load_reg)
{
    /* NPN emitter follower boost for Zener regulator.
     *
     * The transistor acts as a current amplifier:
     *   I_out = beta * I_Z  (in active region)
     *   V_out = V_Z - V_BE
     *
     * Benefits:
     *   - Output current multiplied by beta (50-300x)
     *   - Load regulation improved by beta
     *   - Zener power dissipation greatly reduced
     *
     * This configuration is the ancestor of the modern
     * linear voltage regulator (LM7805, LM317).
     *
     * Used in: Toyota ECU 5V supplies, iPhone accessory power,
     *          NASA Voyager power conditioning (discrete design).
     */
    if (!zener_only || !V_out || !I_out_max || !load_reg) return;

    *V_out = zener_only->V_out_min - V_BE;
    if (*V_out < 0.0) *V_out = 0.0;

    *I_out_max = beta * zener_only->I_Z_max;

    /* Load regulation improved by beta */
    if (beta > 1.0)
        *load_reg = zener_only->load_regulation / beta;
    else
        *load_reg = zener_only->load_regulation;
}