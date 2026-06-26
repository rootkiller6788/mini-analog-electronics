/**
 * active_filter_state_variable.h — State-Variable & Biquad Topologies
 *
 * L2 Core Concepts: State-variable (KHN — Kerwin-Huelsman-Newcomb) and
 * Tow-Thomas biquad filters — multi-op-amp topologies offering
 * simultaneous LP/BP/HP outputs, independent ω₀ and Q tuning,
 * and lowest component sensitivity.
 *
 * Key characteristics of State-Variable (KHN):
 * - Three op-amps (two integrators + one summing amp)
 * - Simultaneous LP, BP, HP outputs (BS via summing)
 * - ω₀ tuned by integrator R/C, Q tuned by feedback ratio
 * - Extremely low sensitivity: S_R^ω0 = -0.5, S_R^Q < 1 for Q < 10
 * - Ideal for voltage-controlled filters (VCF) using MDACs or OTAs
 *
 * Key characteristics of Tow-Thomas Biquad:
 * - Three op-amps (lossy integrator + integrator + inverter)
 * - Simultaneous LP and BP outputs
 * - ω₀ = 1/(R·C), Q = Rq/R
 * - Preferred for cascaded high-order filters
 * - All capacitors equal (unlike KHN)
 *
 * Courses: Stanford EE101B, Berkeley EE105, ETH 227-0455
 * Reference: Kerwin, Huelsman & Newcomb, "State-Variable Synthesis
 *            for Insensitive Integrated Circuit Transfer Functions,"
 *            IEEE JSSC, vol. SC-2, 1967
 *            Tow, "A Step-by-Step Active Filter Design,"
 *            IEEE Spectrum, vol. 6, 1969
 */

#ifndef ACTIVE_FILTER_STATE_VARIABLE_H
#define ACTIVE_FILTER_STATE_VARIABLE_H

#include "active_filter_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================
 * L2: KHN State-Variable Filter Design
 * ================================================================== */

/** Design KHN state-variable filter — second-order section.
 *
 *  L2: Three-op-amp topology with two integrators (lossless) and
 *  one summing amplifier. The state variables are the outputs of
 *  the two integrators, representing the filter's dynamic state.
 *
 *  Architecture:
 *    Summer: produces HP output (fed to 1st integrator)
 *    Integrator 1: HP → BP (lossless integration)
 *    Integrator 2: BP → LP (lossless integration)
 *
 *  Design equations for equal-R, equal-C variant:
 *    ω₀ = 1/(R·C)  where R = integrator R, C = integrator C
 *    Q = (1 + R4/R5) / (2 + R2/R3)  (for standard KHN)
 *    HP gain at ∞: R2/R1
 *    BP gain at ω₀: Q·(HP gain)
 *    LP gain at DC: HP gain
 *
 *  @param f0      Center/cutoff frequency (Hz)
 *  @param q       Quality factor
 *  @param gain_db Passband gain (dB)
 *  @param r_scale Base resistance (ohms, typical 10k-100k)
 *  @param comp    Output: seven-component values (R1-R5, C1-C2)
 *  @return        ACTIVE_OK or error code
 *  Complexity: O(1) */
int khn_state_variable_design(double f0, double q, double gain_db,
                               double r_scale, active_component_values_t *comp);

/** Design KHN filter with independent Q and ω₀ trimming.
 *  L5: The KHN topology uniquely allows independent adjustment of
 *  ω₀ (via integrator R) and Q (via feedback resistor ratio)
 *  without interaction. This is the key advantage over Sallen-Key.
 *
 *  Trim ranges:
 *    ω₀: ±20% via R_trim (potentiometer in series with integrator R)
 *    Q:   ±50% via Rq_trim (potentiometer in feedback divider)
 *
 *  @param f0          Nominal center frequency (Hz)
 *  @param q           Nominal Q
 *  @param trim_range  Fractional trim range (e.g., 0.2 for ±20%)
 *  @param r_scale     Base resistance (ohms)
 *  @param comp        Output component values with trim resistors
 *  Complexity: O(1) */
int khn_state_variable_trimmable(double f0, double q, double trim_range,
                                  double r_scale,
                                  active_component_values_t *comp);

/** Compute all three KHN outputs (HP, BP, LP) given component values.
 *  L3: The state-variable filter provides simultaneous multi-output.
 *  For each output, computes the complete second-order transfer function.
 *
 *  @param comp     Component values
 *  @param hp_num   HP numerator [b2_hp, b1_hp, b0_hp]
 *  @param bp_num   BP numerator [b2_bp, b1_bp, b0_bp]
 *  @param lp_num   LP numerator [b2_lp, b1_lp, b0_lp]
 *  @param den      Common denominator [1, a1, a0]
 *  @param hp_gain  HP high-frequency gain
 *  @param bp_gain  BP center-frequency gain
 *  @param lp_gain  LP DC gain
 *  Complexity: O(1) */
int khn_compute_outputs(const active_component_values_t *comp,
                         double hp_num[3], double bp_num[3],
                         double lp_num[3], double den[3],
                         double *hp_gain, double *bp_gain, double *lp_gain);

/** Create bandstop output by summing HP + LP from KHN filter.
 *  L2: BS = HP + LP (from state-variable simultaneous outputs).
 *  This produces a notch filter without additional op-amps.
 *  Notch frequency = ω₀, notch Q = Q of the filter.
 *  Additional op-amp in summer configuration required.
 *
 *  @param comp         KHN component values
 *  @param bs_num       BS numerator coefficients [b2, b1, b0]
 *  @param den          Common denominator [1, a1, a0]
 *  @param notch_depth  Output: notch depth estimation (dB)
 *  Complexity: O(1) */
int khn_bandstop_from_sums(const active_component_values_t *comp,
                            double bs_num[3], double den[3],
                            double *notch_depth);

/* ==================================================================
 * L2: Tow-Thomas Biquad Filter Design
 * ================================================================== */

/** Design Tow-Thomas biquad — second-order section.
 *
 *  L2: The Tow-Thomas biquad is the most widely used biquad
 *  for cascaded high-order filters. It uses three op-amps:
 *  a damped integrator, a lossless integrator, and an inverter.
 *
 *  Architecture:
 *    Stage 1: Damped integrator (lossy) — produces BP
 *             H1(s) = -(1/(R1·C1))·s / (s² + s/(Rq·C1) + 1/(R2·R·C1·C2))
 *    Stage 2: Lossless integrator — BP → LP
 *             H2(s) = -1/(s·R·C2)
 *    Stage 3: Inverter — restores sign (optional)
 *
 *  Design (equal-C, equal-R variant):
 *    Choose C1 = C2 = C
 *    Choose R (integrator R) for ω₀: R = 1/(ω₀·C)
 *    R1 = R / K_bp   (sets BP gain)
 *    Rq = Q·R        (sets Q)
 *    R2 = R          (feedback R in first stage)
 *
 *  @param f0      Center/cutoff frequency (Hz)
 *  @param q       Quality factor
 *  @param gain_db Passband gain (dB)
 *  @param c_scale Capacitance scale (farads, e.g., 1nF for audio)
 *  @param comp    Output component values (R1, R2, Rq, C1, C2; R3 not used)
 *  @return        ACTIVE_OK or error code
 *  Complexity: O(1) */
int tow_thomas_biquad_design(double f0, double q, double gain_db,
                              double c_scale, active_component_values_t *comp);

/** Design Tow-Thomas with independent Q tuning.
 *  L5: Q is set by Rq/R ratio. By making Rq a potentiometer,
 *  Q can be adjusted without affecting ω₀.
 *  This is a key advantage for tunable filters.
 *
 *  @param f0        Center frequency (Hz)
 *  @param q_min     Minimum Q
 *  @param q_max     Maximum Q
 *  @param gain_db   Passband gain (dB)
 *  @param c_scale   Capacitance scale (farads)
 *  @param comp      Output component values (Rq = potentiometer)
 *  Complexity: O(1) */
int tow_thomas_tunable_q(double f0, double q_min, double q_max,
                          double gain_db, double c_scale,
                          active_component_values_t *comp);

/** Compute Tow-Thomas biquad transfer functions (LP and BP).
 *  L3: Given components, calculate H_LP(s) and H_BP(s).
 *
 *  @param comp     Component values
 *  @param lp_num   LP numerator [b2, b1, b0] (b1=b2=0 for LP)
 *  @param bp_num   BP numerator [b2, b1, b0] (b2=b0=0 for BP)
 *  @param den      Common denominator [1, a1, a0]
 *  @param lp_gain  LP DC gain
 *  @param bp_gain  BP center gain
 *  Complexity: O(1) */
int tow_thomas_compute_outputs(const active_component_values_t *comp,
                                double lp_num[3], double bp_num[3],
                                double den[3], double *lp_gain,
                                double *bp_gain);

/* ==================================================================
 * L5: Akerberg-Mossberg Biquad — Single-Amplifier Variant
 * ================================================================== */

/** Design Akerberg-Mossberg biquad — single op-amp biquad.
 *
 *  L5: The Akerberg-Mossberg topology realizes a biquadratic
 *  transfer function using a single op-amp with an RC network
 *  in the feedback path. Combines the component economy of
 *  Sallen-Key with the flexibility of multi-amp biquads.
 *
 *  Transfer function:
 *    H(s) = (V_out/V_in) = -(Y1·Y3) / (Y5·(Y1+Y2+Y3+Y4) + Y3·Y4)
 *    where Y1-Y5 are RC admittances.
 *
 *  Can realize LP, BP, HP with appropriate Y choices.
 *  Sensitivity is higher than 3-amp biquad but better than Sallen-Key.
 *
 *  Reference: Akerberg & Mossberg, "A Versatile Active RC Building
 *            Block with Inherent Compensation for the Finite
 *            Bandwidth of the Amplifier," IEEE Trans. CAS, 1974
 *
 *  @param function Filter function (LP/HP/BP)
 *  @param f0       Characteristic frequency (Hz)
 *  @param q        Quality factor
 *  @param gain_db  Passband gain (dB)
 *  @param r_scale  Resistance scale (ohms)
 *  @param comp     Output component values
 *  Complexity: O(1) */
int akerberg_mossberg_design(active_filter_func_t function,
                              double f0, double q, double gain_db,
                              double r_scale, active_component_values_t *comp);

#ifdef __cplusplus
}
#endif

#endif /* ACTIVE_FILTER_STATE_VARIABLE_H */
