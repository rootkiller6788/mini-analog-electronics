/**
 * active_filter_sallen_key.h — Sallen-Key Topology Design API
 *
 * L2 Core Concepts: Sallen-Key (VCVS) filter topology — the most widely
 * used active filter topology. Single op-amp, non-inverting, positive
 * feedback through a capacitor for Q enhancement.
 *
 * Key characteristics:
 * - Single op-amp, low power
 * - Non-inverting gain (≥ 1)
 * - Low output impedance, high input impedance
 * - Q set by gain K = 1 + Rf/Rg
 * - Component sensitivity proportional to Q — not suitable for Q > 10
 * - Unity-gain variant: K=1, Q set purely by R/C ratios, lowest sensitivity
 *
 * Courses: MIT 6.002, Stanford EE101B, Berkeley EE105
 * Reference: Sallen & Key, "A Practical Method of Designing RC Active
 *            Filters," IRE Trans. Circuit Theory, vol. CT-2, 1955
 *            Sedra & Smith (2020) Microelectronic Circuits, §16.3
 */

#ifndef ACTIVE_FILTER_SALLEN_KEY_H
#define ACTIVE_FILTER_SALLEN_KEY_H

#include "active_filter_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================
 * L2: Sallen-Key Lowpass Filter
 * ================================================================== */

/** Compute component values for Sallen-Key lowpass filter.
 *
 *  L2: The Sallen-Key LP transfer function:
 *    H(s) = K / (s²·R1·R2·C1·C2 + s·[R1·C1·(1-K) + R1·C2 + R2·C2] + 1)
 *
 *  Design equations (unity gain K=1):
 *    R1 = R2 = R (choose based on impedance level)
 *    C1 = 2·Q / (ω0·R)
 *    C2 = 1 / (2·Q·ω0·R)
 *
 *  For K > 1 (gain variant):
 *    Choose m = C2/C1 ratio (typically m ≥ 4·Q² for real R values)
 *    R2 = (1/ω0)·sqrt(1/(m))·...
 *    Component sensitivity grows with K, limiting usable Q.
 *
 *  @param wp      Pole natural frequency ω₀ (rad/s)
 *  @param qp      Pole quality factor Qp
 *  @param gain    DC gain K (≥ 1.0, use 1.0 for minimal sensitivity)
 *  @param r_scale Resistance scale factor (ohms, e.g. 10k)
 *  @param comp    Output: computed component values
 *  @return        ACTIVE_OK or error code
 *  Complexity: O(1) — direct algebraic computation */
int sallen_key_lp_design(double wp, double qp, double gain,
                          double r_scale, active_component_values_t *comp);

/** Compute component values for unity-gain Sallen-Key lowpass.
 *  L2: Special case — K=1, R4 connected as voltage follower.
 *  Simplest design with lowest component sensitivity.
 *  Choose C1 first, then: R1 = 2·Q/(ω0·C1) (if C1=C2)
 *  General: choose C2 ≥ 4·C1·Q², then R = 1/(ω0·√(C1·C2))
 *
 *  @param wp      Pole frequency ω₀ (rad/s)
 *  @param qp      Pole Q
 *  @param r_scale Nominal resistance for scaling (ohms)
 *  @param comp    Output component values
 *  Complexity: O(1) */
int sallen_key_lp_unity_gain(double wp, double qp, double r_scale,
                              active_component_values_t *comp);

/* ==================================================================
 * L2: Sallen-Key Highpass Filter
 * ================================================================== */

/** Compute component values for Sallen-Key highpass filter.
 *
 *  L2: The Sallen-Key HP transfer function (interchanged R↔C from LP):
 *    H(s) = K·s² / (s² + s·[1/(R1·C1) + 1/(R2·C1) + (1-K)/(R2·C2)] +
 *                   1/(R1·R2·C1·C2))
 *
 *  Design equations (unity gain):
 *    C1 = C2 = C (choose based on frequency range)
 *    R1 = 1 / (2·Q·ω0·C)
 *    R2 = 2·Q / (ω0·C)
 *
 *  @param wp      Pole frequency ω₀ (rad/s)
 *  @param qp      Pole Q factor
 *  @param gain    High-frequency gain K (≥ 1.0)
 *  @param c_scale Capacitance scale factor (farads, e.g. 10nF)
 *  @param comp    Output component values
 *  Complexity: O(1) */
int sallen_key_hp_design(double wp, double qp, double gain,
                          double c_scale, active_component_values_t *comp);

/** Unity-gain Sallen-Key highpass design.
 *  L2: K=1, feedback resistor R4→0 (short), R3→open.
 *  Lowest component sensitivity. C1=C2 simplifies design.
 *  @param wp      Pole frequency (rad/s)
 *  @param qp      Pole Q factor
 *  @param c_scale Capacitance scale (farads)
 *  @param comp    Output component values
 *  Complexity: O(1) */
int sallen_key_hp_unity_gain(double wp, double qp, double c_scale,
                              active_component_values_t *comp);

/* ==================================================================
 * L2: Sallen-Key Bandpass Filter
 * ================================================================== */

/** Sallen-Key bandpass filter design.
 *
 *  L2: The Sallen-Key BP uses a Wien-bridge-like positive feedback.
 *  Transfer function:
 *    H(s) = K·(ω0/Q)·s / (s² + (ω0/Q)·s + ω0²)
 *
 *  Design: R3, C1 set the bandpass; R1,R2,C2 determine Q.
 *  This topology has high component sensitivity — Q > 5 not recommended.
 *  For high-Q BP, prefer MFB or state-variable topologies.
 *
 *  @param f0      Center frequency (Hz)
 *  @param q       Quality factor (1 < Q < 10 recommended)
 *  @param gain_db Passband gain (dB)
 *  @param r_scale Resistance scale (ohms)
 *  @param comp    Output component values
 *  Complexity: O(1) */
int sallen_key_bp_design(double f0, double q, double gain_db,
                          double r_scale, active_component_values_t *comp);

/* ==================================================================
 * L2: Sallen-Key Bandstop (Notch) Filter
 * ================================================================== */

/** Sallen-Key notch filter via Twin-T configuration.
 *
 *  L2: Twin-T notch is the most common Sallen-Key bandstop.
 *  Three resistors and three capacitors form the notch network.
 *  The op-amp bootstraps the ground, deepening the notch.
 *  Notch frequency: f_n = 1 / (2π·R·C)
 *  Notch depth depends on component matching and op-amp CMRR.
 *
 *  @param notch_freq  Notch frequency (Hz)
 *  @param q           Quality factor (width of notch)
 *  @param r_scale     Resistance scale (ohms)
 *  @param comp        Output component values
 *  Complexity: O(1) */
int sallen_key_notch_design(double notch_freq, double q,
                             double r_scale, active_component_values_t *comp);

/* ==================================================================
 * L3: Transfer Function Analysis for Sallen-Key
 * ================================================================== */

/** Compute the actual transfer function of a Sallen-Key circuit.
 *  L3: Given component values, compute H(s) coefficients.
 *  Verifies that the designed circuit meets specification.
 *
 *  @param comp      Component values
 *  @param function  LP, HP, or BP
 *  @param num       Output numerator coefficients [b2, b1, b0] (b2·s²+b1·s+b0)
 *  @param den       Output denominator coefficients [1, a1, a0] (s²+a1·s+a0)
 *  @param gain      Output: computed DC/HF gain
 *  Complexity: O(1) */
int sallen_key_transfer_function(const active_component_values_t *comp,
                                  active_filter_func_t function,
                                  double num[3], double den[3], double *gain);

/** Compute pole frequency and Q from Sallen-Key components.
 *  L3: Reverse design — given a circuit, extract its dynamic parameters.
 *  Useful for tolerance analysis and troubleshooting. */
int sallen_key_extract_params(const active_component_values_t *comp,
                               active_filter_func_t function,
                               double *wp, double *qp, double *gain);

/* ==================================================================
 * L5: Design Optimization
 * ================================================================== */

/** Optimize Sallen-Key component values to preferred E-series.
 *  L5: Snaps computed component values to nearest E12/E24/E48/E96 values.
 *  Then recalculates filter response with real-world components.
 *  Reports frequency error and Q error from ideal.
 *
 *  @param comp_in   Ideal component values
 *  @param e_series  Target E-series (12, 24, 48, 96)
 *  @param comp_out  Snapped component values
 *  @param freq_err  Output: frequency error (%)
 *  @param q_err     Output: Q error (%)
 *  Complexity: O(1) */
int sallen_key_snap_to_eseries(const active_component_values_t *comp_in,
                                int e_series, active_component_values_t *comp_out,
                                double *freq_err, double *q_err);

#ifdef __cplusplus
}
#endif

#endif /* ACTIVE_FILTER_SALLEN_KEY_H */
