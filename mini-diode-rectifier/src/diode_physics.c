/**
 * diode_physics.c -- PN Junction Diode Physical Model Implementation
 *
 * Implements the Shockley equation, junction capacitance, temperature effects,
 * and all core PN junction diode physics.
 *
 * Reference: Sedra & Smith Ch.1,4; Sze Ch.2
 */

#include "diode_physics.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * L3: Thermal Voltage
 * ============================================================================ */

double diode_thermal_voltage(double T)
{
    /* V_T = k * T / q
     * At T=300K: V_T = 1.380649e-23 * 300 / 1.602176634e-19 = 0.025852 V
     * This is a direct consequence of the equipartition theorem.
     */
    if (T <= 0.0) return 0.0;   /* Invalid temperature; physics requires T > 0 */
    return DIODE_k_BOLTZMANN * T / DIODE_q_ELEMENTARY;
}

/* ============================================================================
 * L3: Shockley Equation — Forward Current
 * ============================================================================ */

double diode_shockley_current(double V_D, double I_S, double n, double V_T)
{
    /* I_D = I_S * (exp(V_D / (n * V_T)) - 1)
     *
     * This is Bill Shockley's 1949 equation describing an ideal PN junction.
     * For V_D >> n*V_T (forward bias), I_D ~= I_S * exp(V_D/(n*V_T)).
     * For V_D << 0 (reverse bias), I_D ~= -I_S (saturation).
     *
     * Guard against overflow: exp(x) overflows IEEE double at x ~= 710.
     * With V_T=25.85mV and n=1, overflow at V_D ~= 18.4V.
     * Clamp argument to 100 to stay safe in most practical cases.
     */
    if (V_T <= 0.0) return 0.0;
    if (I_S <= 0.0) return 0.0;
    if (n <= 0.0) return 0.0;

    double arg = V_D / (n * V_T);
    /* Prevent overflow: clamp argument to [-100, 100] */
    if (arg > 100.0) arg = 100.0;
    if (arg < -100.0) arg = -100.0;

    return I_S * (exp(arg) - 1.0);
}

/* ============================================================================
 * L3: Shockley Inverse — Voltage from Current
 * ============================================================================ */

double diode_shockley_voltage(double I_D, double I_S, double n, double V_T)
{
    /* V_D = n * V_T * ln(I_D / I_S + 1)
     *
     * Inverse of the Shockley equation. Requires I_D > -I_S for
     * the logarithm to be defined (positive argument).
     * For I_D = 0, V_D = 0 (thermal equilibrium).
     */
    if (V_T <= 0.0) return 0.0;
    if (I_S <= 0.0) return 0.0;

    double ratio = I_D / I_S + 1.0;
    if (ratio <= 0.0) return 0.0;  /* Below reverse saturation, clamp to 0 */

    return n * V_T * log(ratio);
}

/* ============================================================================
 * L3: Junction Capacitance (Depletion)
 * ============================================================================ */

double diode_junction_capacitance(double V_D, double C_J0, double V_J, double M)
{
    /* C_j(V_D) = C_J0 / (1 - V_D / V_J)^M
     *
     * Based on the depletion approximation (Schottky 1942).
     * M = 0.5 for abrupt junction, M = 0.33 for linearly graded junction.
     * Valid only for V_D < V_J (reverse bias and moderate forward bias).
     * For V_D >= V_J, the depletion region vanishes and C_j diverges.
     * In practice, SPICE limits V_D < 0.95 * V_J.
     *
     * C_J0: zero-bias junction capacitance (typically pF to nF range)
     */
    if (C_J0 <= 0.0 || V_J <= 0.0) return 0.0;
    if (M <= 0.0) return C_J0;    /* No grading effect */

    /* Limit forward bias to 95% of built-in potential (SPICE convention) */
    double V_eff = V_D;
    if (V_eff > 0.95 * V_J) V_eff = 0.95 * V_J;

    double denominator = 1.0 - V_eff / V_J;
    if (denominator <= 1e-15) denominator = 1e-15;  /* Guard against singularities */

    return C_J0 / pow(denominator, M);
}

/* ============================================================================
 * L3: Diffusion Capacitance
 * ============================================================================ */

double diode_diffusion_capacitance(double I_D, double T_T, double n, double V_T)
{
    /* C_diff = tau_T * I_D / (n * V_T)
     *
     * Arises from minority carrier charge storage in forward bias.
     * tau_T: transit time, the average time a carrier spends traversing
     * the base region. Typical: 1-100 ns for small-signal diodes,
     * 1-10 us for power rectifiers.
     *
     * C_diff is proportional to forward current — dominant at high I_D.
     */
    if (V_T <= 0.0 || n <= 0.0) return 0.0;
    if (I_D <= 0.0) return 0.0;  /* Diffusion capacitance only in forward bias */
    if (T_T <= 0.0) return 0.0;

    return T_T * I_D / (n * V_T);
}

/* ============================================================================
 * L3: Complete Small-Signal Model
 * ============================================================================ */

void diode_small_signal_model(const DiodeModelParams *params, double I_DQ,
                              double V_DQ, double V_T, DiodeSmallSignal *out)
{
    /* Builds the complete small-signal model at a given DC operating point.
     *
     * r_d = n*V_T / I_D  (dynamic resistance, typically ohms to k-ohms)
     * g_d = 1 / r_d      (conductance)
     * C_j = C_J0 / (1 - V_D/V_J)^M  (depletion capacitance)
     * C_diff = T_T * I_D / (n*V_T)  (diffusion capacitance)
     * c_d = C_j + C_diff  (total small-signal capacitance)
     */
    if (!params || !out) return;

    if (I_DQ > 1e-15) {
        out->r_d = params->N * V_T / I_DQ;
        out->g_d = 1.0 / out->r_d;
        out->C_diff = diode_diffusion_capacitance(I_DQ, params->T_T,
                                                   params->N, V_T);
    } else {
        /* At zero/negligible bias, dynamic resistance is very large */
        out->r_d = 1e12;
        out->g_d = 1e-12;
        out->C_diff = 0.0;
    }

    out->C_j = diode_junction_capacitance(V_DQ, params->C_J0, params->V_J,
                                           params->M);
    out->c_d = out->C_j + out->C_diff;
}

/* ============================================================================
 * L3: Temperature-Dependent Saturation Current
 * ============================================================================ */

double diode_I_S_at_temperature(const DiodeModelParams *params, double T)
{
    /* I_S(T) = I_S(T_nom) * (T/T_nom)^(X_TI/N) * exp(q*E_G/(N*k) * (1/T_nom - 1/T))
     *
     * This captures two effects:
     * 1. The T^(X_TI/N) term: increase in n_i^2 with temperature
     *    (n_i^2 ~ T^3 * exp(-E_G/(k*T)) for non-degenerate semiconductors)
     * 2. The exponential term: bandgap narrowing with temperature
     *
     * For silicon at 300 K:
     *   - I_S approximately doubles every 5-10 K temperature rise
     *   - This is why diode V_F decreases ~2 mV/K
     *
     * Ref: Sze Sec.2.4.3
     */
    if (!params || T <= 0.0 || params->T_NOM <= 0.0) return params ? params->I_S : 1e-14;

    double ratio = T / params->T_NOM;
    double exponent_T = params->X_TI / params->N;

    /* q * E_G / (N * k) converts bandgap from eV to temperature scale */
    double E_G_over_NkT = (DIODE_q_ELEMENTARY * params->E_G) /
                          (params->N * DIODE_k_BOLTZMANN);

    double delta_inv_T = 1.0 / params->T_NOM - 1.0 / T;

    return params->I_S * pow(ratio, exponent_T) * exp(E_G_over_NkT * delta_inv_T);
}

/* ============================================================================
 * L3: Temperature Sweep
 * ============================================================================ */

void diode_temperature_sweep(const DiodeModelParams *params, double T,
                             DiodeTempSweep *out)
{
    /* Characterize diode behavior at a given temperature.
     *
     * V_F temperature coefficient is approximately:
     *   dV_F/dT = -(E_G/q - V_F) / T   (for constant I_F)
     * Typically -1.8 to -2.2 mV/K for silicon diodes at 1 mA.
     *
     * This negative TC is why diodes make good temperature sensors
     * (2 mV/K sensitivity, nearly linear over -55 to +150 C).
     */
    if (!params || !out) return;

    out->temperature_K = T;
    out->V_T = diode_thermal_voltage(T);
    out->I_S_at_T = diode_I_S_at_temperature(params, T);

    /* Forward voltage at I_D = 1 mA */
    out->V_F_at_1mA = diode_shockley_voltage(1e-3, out->I_S_at_T,
                                              params->N, out->V_T);

    /* Temperature coefficient: approximate using small delta */
    double T2 = T + 1.0;
    double V_T2 = diode_thermal_voltage(T2);
    double I_S_T2 = diode_I_S_at_temperature(params, T2);
    double V_F_T2 = diode_shockley_voltage(1e-3, I_S_T2, params->N, V_T2);

    /* dV_F/dT in mV/K = (V_F(T+1) - V_F(T)) / 1K * 1000 mV/V */
    out->V_F_tc = (V_F_T2 - out->V_F_at_1mA) * 1000.0;
}

/* ============================================================================
 * L2: Operating Region Classification
 * ============================================================================ */

DiodeRegion diode_classify_region(double V_D, double I_D,
                                  double B_V, double T_J, double T_J_MAX)
{
    /* Classify diode operating region based on voltage, current, and temperature.
     *
     * 1. Thermal runaway check first (safety priority):
     *    If T_J > T_J_MAX, the diode is in danger of destruction.
     *    Positive feedback: T up → I_S up → I_D up → P_D up → T up.
     *
     * 2. Forward bias: V_D > ~0.1 V for meaningful conduction
     *    (At 0.1V with V_T=25.85mV, I/I_S = exp(3.87) ≈ 48x)
     *
     * 3. Reverse bias: V_D < ~-0.1 V
     *    Current saturates at -I_S (plus leakage)
     *
     * 4. Breakdown: V_D < -B_V
     *    Avalanche multiplication causes exponential current increase.
     *    B_V is the magnitude (positive number), so check V_D < -B_V.
     *
     * 5. Zero bias: |V_D| < 0.1 V
     *    No significant current flow.
     */
    if (T_J > T_J_MAX && T_J_MAX > 0.0)
        return DIODE_REGION_THERMAL_RUNAWAY;

    if (V_D < -B_V)
        return DIODE_REGION_BREAKDOWN;

    if (V_D > 0.1 && I_D > 0.0)
        return DIODE_REGION_FORWARD_BIAS;

    if (V_D < -0.1)
        return DIODE_REGION_REVERSE_BIAS;

    return DIODE_REGION_ZERO_BIAS;
}

/* ============================================================================
 * L3: SPICE-Compatible Full I-V (Newton-Raphson iteration)
 * ============================================================================ */

int diode_full_iv(const DiodeModelParams *params, double V_D, double V_T,
                  int max_iter, double tol, double *out_I_D, double *out_P_D)
{
    /* Solves the complete diode I-V including series resistance R_S.
     *
     * The terminal voltage V_D is divided between the junction and R_S:
     *   V_D = V_junction + I_D * R_S
     *   I_D = f(V_junction) where f is the Shockley/breakdown equation
     *
     * We solve: g(I_D) = V_junction(I_D) + I_D*R_S - V_D = 0
     * using Newton-Raphson iteration on I_D.
     *
     * Derivative: g'(I_D) = dV_j/dI_D + R_S
     *   where dV_j/dI_D = 1/(g_D) = r_d (dynamic resistance)
     *   For forward: r_d = n*V_T/I_D, so dV_j/dI_D = n*V_T/I_D
     *
     * Newton update: I_D_new = I_D - g(I_D) / g'(I_D)
     *
     * For negative V_D (reverse/breakdown), we handle separately.
     */

    if (!params || !out_I_D || !out_P_D) return -1;
    if (V_T <= 0.0 || max_iter <= 0) return -1;

    /* Initial guess */
    double I_D;
    if (V_D > 0.1) {
        /* Forward bias: start with exponential approximation */
        I_D = params->I_S * exp(V_D / (params->N * V_T));
        if (I_D > 1e3) I_D = 1e3;  /* Clamp unreasonable guesses */
    } else if (V_D < -params->B_V) {
        /* Breakdown */
        I_D = -params->I_BV * exp((-V_D - params->B_V) / (params->N_BV * V_T));
        if (I_D < -1e3) I_D = -1e3;
    } else {
        /* Reverse bias: small leakage */
        I_D = -params->I_S;
    }

    /* Newton-Raphson iteration */
    int iter;
    for (iter = 0; iter < max_iter; iter++) {
        double V_j = diode_shockley_voltage(I_D, params->I_S, params->N, V_T);

        /* Handle breakdown region */
        if (V_j < -params->B_V * 0.9) {
            /* In breakdown, current is negative and large.
             * Use the breakdown equation directly when V_D is negative enough. */
            if (V_D < -params->B_V) {
                I_D = -params->I_BV * exp((-V_D - params->B_V) /
                                          (params->N_BV * V_T));
                break;
            }
        }

        double g = V_j + I_D * params->R_S - V_D;
        double g_prime;
        if (fabs(I_D) > 1e-15) {
            g_prime = params->N * V_T / fabs(I_D) + params->R_S;
        } else {
            g_prime = 1e12 + params->R_S;
        }

        double delta = g / g_prime;
        I_D = I_D - delta;

        if (fabs(delta) < tol) break;
    }

    *out_I_D = I_D;
    *out_P_D = I_D * V_D;   /* Total power: I * V at terminals */
    if (*out_P_D < 0.0) *out_P_D = 0.0;

    return (iter < max_iter) ? iter + 1 : -1;
}

/* ============================================================================
 * L4: Boltzmann Relation — Built-In Potential
 * ============================================================================ */

double diode_built_in_potential(double N_A, double N_D, double n_i, double V_T)
{
    /* V_bi = V_T * ln(N_A * N_D / n_i^2)
     *
     * This comes from equating the diffusion and drift currents
     * at thermal equilibrium (zero net current). The built-in
     * potential is the barrier that prevents further diffusion.
     *
     * For Si with N_A=N_D=1e16 cm^-3, n_i=1e10 cm^-3:
     *   V_bi = 0.02585 * ln(1e32 / 1e20) = 0.02585 * ln(1e12) = 0.714 V
     *
     * This matches the typical 0.6-0.8 V barrier potential of silicon diodes.
     *
     * Theorem: From Boltzmann transport equation at equilibrium:
     *   J_n = q*mu_n*n*E + q*D_n*dn/dx = 0  at equilibrium
     *   Integrating gives n(x) = N_D * exp(-q*phi(x)/(k*T))
     *   → phi = (kT/q) * ln(N_A*N_D/n_i^2)
     */
    if (V_T <= 0.0 || n_i <= 0.0) return 0.0;
    if (N_A <= 0.0 || N_D <= 0.0) return 0.0;

    double arg = N_A * N_D / (n_i * n_i);
    if (arg <= 0.0) return 0.0;

    return V_T * log(arg);
}

/* ============================================================================
 * L4: Depletion Width (Poisson Solution)
 * ============================================================================ */

double diode_depletion_width(double V_D, double V_bi, double N_A, double N_D,
                             double eps_s)
{
    /* W_dep = sqrt(2 * eps_s * (V_bi - V_D) * (N_A + N_D) / (q * N_A * N_D))
     *
     * Derived from solving Poisson's equation in the depletion approximation:
     *   d^2(phi)/dx^2 = -rho/eps_s = -q*(p - n + N_D+ - N_A-)/eps_s
     *
     * In the depletion region: rho ≈ q*N_D on n-side, -q*N_A on p-side.
     * Integrating twice with boundary conditions gives the width formula.
     *
     * For a one-sided abrupt junction (N_A >> N_D):
     *   W_dep ≈ sqrt(2 * eps_s * (V_bi - V_D) / (q * N_D))
     *
     * Example: Si, N_A=1e18, N_D=1e15, V_bi=0.8V, V_D=0V:
     *   W_dep = sqrt(2*1.04e-12*0.8*1.001e18/(1.6e-19*1e18*1e15))
     *         ≈ 1.02e-4 cm = 1.02 um
     */
    if (eps_s <= 0.0 || V_bi <= V_D) return 0.0;
    if (N_A <= 0.0 || N_D <= 0.0) return 0.0;

    double numerator = 2.0 * eps_s * (V_bi - V_D) * (N_A + N_D);
    double denominator = DIODE_q_ELEMENTARY * N_A * N_D;

    return sqrt(numerator / denominator);
}

/* ============================================================================
 * L2: Breakdown Voltage Estimation
 * ============================================================================ */

double diode_breakdown_voltage_estimate(double N_B, double E_crit, double eps_s)
{
    /* For an abrupt one-sided junction:
     *   BV ≈ eps_s * E_crit^2 / (2 * q * N_B)
     *
     * This comes from solving: E_max = q*N_B*W/eps_s = E_crit at breakdown.
     * And W = sqrt(2*eps_s*BV/(q*N_B)) for one-sided junction.
     *
     * Solving: E_crit = q*N_B/eps_s * sqrt(2*eps_s*BV/(q*N_B))
     *         = sqrt(2*q*N_B*BV/eps_s)
     * → BV = eps_s * E_crit^2 / (2 * q * N_B)
     *
     * For Si with N_B=1e15 cm^-3 and E_crit≈3e5 V/cm:
     *   BV = 1.04e-12 * 9e10 / (2 * 1.6e-19 * 1e15) = 293 V
     *
     * This matches the empirical observation that 1000V diodes need
     * doping below ~3e14 cm^-3.
     *
     * Ref: Sze Sec.2.5, Baliga "Fundamentals of Power Semiconductor Devices"
     */
    if (N_B <= 0.0 || E_crit <= 0.0 || eps_s <= 0.0) return 0.0;

    return eps_s * E_crit * E_crit / (2.0 * DIODE_q_ELEMENTARY * N_B);
}

/* ============================================================================
 * L2: Material Library
 * ============================================================================ */

static const DiodeMaterial material_database[] = {
    { "Si",   1.12, 0.72, 1.0e10,  1350.0, 450.0,  1.5,  3.0e5  },
    { "Ge",   0.67, 0.40, 2.4e13,  3900.0, 1900.0, 0.6,  1.0e5  },
    { "GaAs", 1.42, 1.10, 2.1e6,   8500.0, 400.0,  0.55, 4.0e5  },
    { "GaN",  3.40, 3.20, 1.9e-10, 900.0,  10.0,   1.3,  3.3e6  },
    { "SiC",  3.26, 2.90, 6.7e-11, 900.0,  115.0,  4.9,  2.5e6  },
    { NULL,   0.0,  0.0,  0.0,     0.0,    0.0,    0.0,  0.0    }
};

const DiodeMaterial *diode_material_lookup(const char *material_name)
{
    /* Look up pre-defined semiconductor material parameters.
     *
     * The database includes the five most important power semiconductor
     * materials. GaN and SiC are "wide bandgap" semiconductors that
     * enable higher voltage, higher frequency, and higher temperature
     * operation than silicon.
     *
     * Key advantage of wide bandgap (E_G > 3 eV):
     *   - Critical field is ~10x higher → thinner drift regions
     *   - Higher thermal conductivity → better heat dissipation
     *   - Higher temperature operation (SiC: 600C vs Si: 175C)
     */
    if (!material_name) return NULL;
    for (int i = 0; material_database[i].name != NULL; i++) {
        if (strcmp(material_database[i].name, material_name) == 0)
            return &material_database[i];
    }
    return NULL;
}

/* ============================================================================
 * L3: Load Line Newton-Raphson Solver
 * ============================================================================ */

int diode_load_line_solve(const DiodeModelParams *params, double V_supply,
                          double R_load, double V_T, int max_iter, double tol,
                          DiodeOpPoint *op)
{
    /* Find the diode operating point by intersecting the load line
     * with the diode I-V characteristic.
     *
     * Load line: I_D = (V_supply - V_D) / R_load
     * Diode:     I_D = I_S * (exp(V_D/(n*V_T)) - 1)
     *
     * We solve: f(V_D) = I_S*(exp(V_D/(n*V_T))-1) - (V_supply-V_D)/R_load = 0
     *
     * Newton: V_D_new = V_D - f(V_D)/f'(V_D)
     * where f'(V_D) = I_S*exp(V_D/(n*V_T))/(n*V_T) + 1/R_load
     *
     * Initial guess: V_D = 0.7V for Si (typical forward drop at moderate I).
     * For low V_supply (< 1V), start closer to 0.4V.
     */
    if (!params || !op || R_load <= 0.0 || V_T <= 0.0) return -1;
    if (V_supply <= 0.0) {
        op->V_D = 0.0;
        op->I_D = 0.0;
        op->P_D = 0.0;
        op->T_J = 300.0;
        return 0;
    }

    /* Initial guess based on supply voltage */
    double V_D = 0.7;
    if (V_supply < 1.5) V_D = V_supply * 0.6;
    if (V_supply > 100.0) V_D = 0.8;

    int iter;
    for (iter = 0; iter < max_iter; iter++) {
        double exp_term = exp(V_D / (params->N * V_T));
        if (exp_term > 1e100) exp_term = 1e100;  /* Clamp to prevent overflow */

        double I_diode = params->I_S * (exp_term - 1.0);
        double I_load = (V_supply - V_D) / R_load;
        double f = I_diode - I_load;

        /* Derivative */
        double df = params->I_S * exp_term / (params->N * V_T) + 1.0 / R_load;
        if (df <= 0.0) break;

        double delta = f / df;
        V_D -= delta;

        if (fabs(delta) < tol) break;
    }

    if (V_D < 0.0) V_D = 0.0;
    if (V_D > V_supply) V_D = V_supply;

    op->V_D = V_D;
    op->I_D = (V_supply - V_D) / R_load;
    op->P_D = op->V_D * op->I_D;
    op->T_J = 300.0 + op->P_D * params->RTH_JA;  /* Simplified thermal */

    return (iter < max_iter) ? iter + 1 : -1;
}