/**
 * diode_physics.h -- PN Junction Diode Physical Model
 *
 * Knowledge Coverage: L1 Definitions, L2 Core Concepts, L3 Math Structures, L4 Fundamental Laws
 *
 * Reference: Sedra & Smith "Microelectronic Circuits" 8th Ed (2020), Ch. 1, 4
 *            S.M. Sze "Physics of Semiconductor Devices" 3rd Ed (2007), Ch. 2
 *
 * Key Equations:
 *   Shockley:   I_D = I_S * (exp(V_D / (n * V_T)) - 1)
 *   Thermal Voltage: V_T = k * T / q  (~25.85 mV at 300 K)
 *   Dynamic Resistance: r_d = n * V_T / I_D
 *   Junction Capacitance: C_j = C_j0 / (1 - V_D / V_bi)^m
 *   Diffusion Capacitance: C_diff = tau_T * I_D / (n * V_T)
 */

#ifndef DIODE_PHYSICS_H
#define DIODE_PHYSICS_H

#include <stdint.h>

/* ============================================================================
 * L1: Core Physical Constants
 * ============================================================================ */

/** Boltzmann constant (J/K) */
#define DIODE_k_BOLTZMANN  1.380649e-23

/** Elementary charge (C) */
#define DIODE_q_ELEMENTARY 1.602176634e-19

/** Thermal voltage at 300 K (V) */
#define DIODE_VT_300K      0.025851991

/** Silicon permittivity (F/cm) */
#define DIODE_EPS_SI       1.04e-12

/** Silicon intrinsic carrier concentration at 300 K (cm^-3) */
#define DIODE_NI_SI_300K   1.0e10

/* ============================================================================
 * L1: Diode Physical Parameters (struct)
 * ============================================================================ */

/**
 * DiodeModelParams -- Full physical parameter set for a PN junction diode.
 * Maps to SPICE .MODEL D card parameters.
 *
 * Ref: Sedra & Smith Sec.4.3, Sze Sec.2.4
 */
typedef struct {
    double I_S;           /**< Saturation current (A), typical 1e-12..1e-6    */
    double N;             /**< Ideality factor (1.0 diffusion, 2.0 SRH)       */
    double R_S;           /**< Series resistance (ohm)                         */
    double C_J0;          /**< Zero-bias junction capacitance (F)              */
    double V_J;           /**< Built-in junction potential (V), ~0.6-0.9 Si    */
    double M;             /**< Grading coefficient (0.33 step, 0.5 linear)     */
    double T_T;           /**< Transit time (s)                                */
    double B_V;           /**< Reverse breakdown voltage (V, positive)         */
    double I_BV;          /**< Current at breakdown voltage (A)                */
    double N_BV;          /**< Breakdown ideality factor                       */
    double E_G;           /**< Bandgap energy (eV), Si=1.12                    */
    double X_TI;          /**< Sat current temperature exponent (~3 for Si)    */
    double P_MAX;         /**< Maximum power dissipation (W)                   */
    double T_NOM;         /**< Nominal temperature for params (K), usually 300 */
    double RTH_JA;        /**< Junction-to-ambient thermal resistance (K/W)    */
} DiodeModelParams;

/* ============================================================================
 * L1: Diode Operating Point
 * ============================================================================ */

/**
 * DiodeOpPoint -- DC operating point of a diode.
 */
typedef struct {
    double V_D;           /**< Forward bias voltage (V)                        */
    double I_D;           /**< Diode current (A), positive = forward           */
    double P_D;           /**< Power dissipation (W)                           */
    double T_J;           /**< Junction temperature (K)                        */
} DiodeOpPoint;

/* ============================================================================
 * L1: Diode Small-Signal AC Parameters
 * ============================================================================ */

/**
 * DiodeSmallSignal -- AC small-signal equivalent circuit parameters.
 *
 * At a given DC operating point (I_D, V_D), the diode linearizes to:
 *   r_d = n*V_T / I_D   (small-signal resistance)
 *   g_d = 1 / r_d       (small-signal conductance)
 *   c_d = C_j + C_diff  (total small-signal capacitance)
 *
 * Ref: Sedra & Smith Sec.4.2.6
 */
typedef struct {
    double r_d;           /**< Dynamic / small-signal resistance (ohm)          */
    double g_d;           /**< Small-signal conductance (S)                    */
    double c_d;           /**< Total small-signal capacitance (F)              */
    double C_j;           /**< Depletion capacitance at operating point (F)    */
    double C_diff;        /**< Diffusion capacitance at operating point (F)    */
} DiodeSmallSignal;

/* ============================================================================
 * L1: Temperature Sweep Point
 * ============================================================================ */

/**
 * DiodeTempSweep -- I-V characteristic at a specific temperature.
 */
typedef struct {
    double temperature_K; /**< Temperature in Kelvin                            */
    double V_T;           /**< Thermal voltage at this temperature (V)          */
    double I_S_at_T;      /**< Saturation current at this temperature (A)       */
    double V_F_at_1mA;    /**< Forward voltage at I_D = 1 mA (V)                */
    double V_F_tc;        /**< Forward voltage temperature coefficient (mV/K)   */
} DiodeTempSweep;

/* ============================================================================
 * L2: Diode Operating Region Enum
 * ============================================================================ */

/**
 * DiodeRegion -- Classifies the operating region of a PN junction diode.
 */
typedef enum {
    DIODE_REGION_FORWARD_BIAS,
    DIODE_REGION_REVERSE_BIAS,
    DIODE_REGION_BREAKDOWN,
    DIODE_REGION_ZERO_BIAS,
    DIODE_REGION_THERMAL_RUNAWAY
} DiodeRegion;

/* ============================================================================
 * L2: Semiconductor Material Constants
 * ============================================================================ */

/**
 * DiodeMaterial -- Pre-defined material parameters for common semiconductors.
 */
typedef struct {
    const char *name;
    double E_G;
    double V_bi_typical;
    double ni_300K;
    double mu_n;
    double mu_p;
    double k_thermal;
    double BV_critical_field;
} DiodeMaterial;

/* ============================================================================
 * Function Declarations
 * ============================================================================ */

/* L3: Thermal Voltage */
double diode_thermal_voltage(double T);

/* L3: Shockley Equation */
double diode_shockley_current(double V_D, double I_S, double n, double V_T);

/* L3: Shockley Inverse */
double diode_shockley_voltage(double I_D, double I_S, double n, double V_T);

/* L3: Junction Capacitance */
double diode_junction_capacitance(double V_D, double C_J0, double V_J, double M);

/* L3: Diffusion Capacitance */
double diode_diffusion_capacitance(double I_D, double T_T, double n, double V_T);

/* L3: Small-Signal Model */
void diode_small_signal_model(const DiodeModelParams *params, double I_DQ,
                              double V_DQ, double V_T, DiodeSmallSignal *out);

/* L3: Temperature-Dependent Saturation Current */
double diode_I_S_at_temperature(const DiodeModelParams *params, double T);

/* L3: Temperature Sweep */
void diode_temperature_sweep(const DiodeModelParams *params, double T,
                             DiodeTempSweep *out);

/* L2: Operating Region Classification */
DiodeRegion diode_classify_region(double V_D, double I_D,
                                  double B_V, double T_J, double T_J_MAX);

/* L3: SPICE-Compatible Full I-V */
int diode_full_iv(const DiodeModelParams *params, double V_D, double V_T,
                  int max_iter, double tol, double *out_I_D, double *out_P_D);

/* L4: Boltzmann Relation */
double diode_built_in_potential(double N_A, double N_D, double n_i, double V_T);

/* L4: Depletion Width */
double diode_depletion_width(double V_D, double V_bi, double N_A, double N_D,
                             double eps_s);

/* L2: Breakdown Voltage Estimation */
double diode_breakdown_voltage_estimate(double N_B, double E_crit, double eps_s);

/* L2: Material Library */
const DiodeMaterial *diode_material_lookup(const char *material_name);

/* L3: Load Line Intersection (Newton-Raphson) */
int diode_load_line_solve(const DiodeModelParams *params, double V_supply,
                          double R_load, double V_T, int max_iter, double tol,
                          DiodeOpPoint *op);

#endif /* DIODE_PHYSICS_H */