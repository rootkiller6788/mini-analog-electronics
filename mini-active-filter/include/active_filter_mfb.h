/**
 * active_filter_mfb.h — Multiple Feedback (MFB) Topology Design API
 *
 * L2 Core Concepts: Multiple Feedback (Rauch) topology — the second most
 * common active filter topology. Inverting configuration with two feedback
 * paths (capacitive and resistive), providing superior stopband rejection
 * and better high-Q BP performance compared to Sallen-Key.
 *
 * Key characteristics:
 * - Inverting gain (negative), phase inversion
 * - Two feedback paths provide more design degrees of freedom
 * - Superior for high-Q BP designs (Q up to ~25 practical)
 * - Lower sensitivity than Sallen-Key for BP
 * - Better stopband rejection due to capacitive feedforward
 * - Input impedance = R1 (moderate, may need buffer)
 *
 * Courses: MIT 6.002, Stanford EE101B, TU Munich High-Frequency Eng.
 * Reference: J.J. Friend, "A Single Operational Amplifier Biquadratic
 *            Filter Section," IEEE ISCT, 1970
 *            Sedra & Smith (2020) Microelectronic Circuits, §16.4
 */

#ifndef ACTIVE_FILTER_MFB_H
#define ACTIVE_FILTER_MFB_H

#include "active_filter_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================
 * L2: MFB Lowpass Filter
 * ================================================================== */

/** Design MFB lowpass filter — compute component values.
 *
 *  L2: MFB LP transfer function:
 *    H(s) = -(R2/R1) / (s²·R2·R3·C1·C2 + s·R2·R3·C1·(1/R1+1/R2+1/R3) + 1)
 *
 *  Design procedure (simplified method for Q < 20):
 *    1. Choose C2 based on frequency range (100-1000pF for audio)
 *    2. Choose C1 ≥ 4·C2·Q² (for real-valued R's)
 *    3. R2 = 1 / (2·ω0·C2·Q)
 *    4. R1 = R2 / |K| (K = -R2/R1 is DC gain)
 *    5. R3 = 1 / (ω0²·R2·C1·C2)
 *
 *  Alternative equal-R design: R1=R2=R3=R, then C1,C2 set ω0 and Q.
 *  Lower sensitivity, but requires specific C1/C2 ratio.
 *
 *  @param wp      Pole frequency ω₀ (rad/s)
 *  @param qp      Pole Q factor
 *  @param gain    Desired DC gain (negative = inverting, use magnitude here)
 *  @param r_scale Nominal resistance for scaling (ohms)
 *  @param comp    Output component values
 *  @return        ACTIVE_OK or error code
 *  Complexity: O(1) */
int mfb_lp_design(double wp, double qp, double gain,
                   double r_scale, active_component_values_t *comp);

/** MFB LP equal-resistance design variant.
 *  L5: Special case R1=R2=R3=R. Simpler to build, fewer distinct values.
 *  Design constraints: must satisfy C1/C2 ≥ 4Q² for real C values.
 *  If not satisfiable, falls back to standard method.
 *  @param wp      Pole frequency (rad/s)
 *  @param qp      Pole Q
 *  @param gain    DC gain magnitude
 *  @param r_value Common resistance value (ohms)
 *  @param comp    Output component values
 *  Complexity: O(1) */
int mfb_lp_equal_r_design(double wp, double qp, double gain,
                           double r_value, active_component_values_t *comp);

/* ==================================================================
 * L2: MFB Highpass Filter
 * ================================================================== */

/** Design MFB highpass filter.
 *
 *  L2: MFB HP transfer function (C-R swap of LP):
 *    H(s) = -(C1/C3)·s² / (s² + s·(C1+C2+C3)/(R2·C2·C3) +
 *                           1/(R1·R2·C2·C3))
 *
 *  Design: choose equal capacitors C1=C2=C3=C for simplicity.
 *  Then R1 = 1/(ω0·C·Q·(2+1/|K|)), R2 = Q·(2+|K|)/(ω0·C).
 *
 *  @param wp       Pole frequency (rad/s)
 *  @param qp       Pole Q
 *  @param gain     High-frequency gain magnitude
 *  @param c_scale  Capacitance scale (farads)
 *  @param comp     Output component values
 *  Complexity: O(1) */
int mfb_hp_design(double wp, double qp, double gain,
                   double c_scale, active_component_values_t *comp);

/* ==================================================================
 * L2: MFB Bandpass Filter — Canonical High-Q Application
 * ================================================================== */

/** Design MFB bandpass filter — the primary MFB use case.
 *
 *  L2: MFB BP transfer function:
 *    H(s) = -(1/(R1·C1))·s / (s² + s/(R3)·(1/C1+1/C2) +
 *                               (1/(R3·C1·C2))·(1/R1+1/R2))
 *
 *  Design procedure (Daryanani method):
 *    1. Choose C1 = C2 = C for simplicity
 *    2. R1 = Q / (|K|·ω0·C)
 *    3. R2 = Q / ((2·Q²-|K|)·ω0·C)
 *    4. R3 = 2·Q / (ω0·C)
 *
 *  Constraint: 2·Q² > |K| for positive R2.
 *  High-Q BP: MFB is the best 1-op-amp topology.
 *  Q up to ~25 achievable with careful layout.
 *
 *  @param f0      Center frequency (Hz)
 *  @param q       Quality factor (Q > 1)
 *  @param gain_db Center-frequency gain (dB, negative means inverting)
 *  @param c_scale Capacitance scale (farads)
 *  @param comp    Output component values
 *  Complexity: O(1) */
int mfb_bp_design(double f0, double q, double gain_db,
                   double c_scale, active_component_values_t *comp);

/** Estimate maximum achievable Q for MFB BP.
 *  L3: Limited by component tolerances and op-amp GBW.
 *  Q_max_GBW ≈ sqrt(GBW / (f0 · |K|))
 *  Q_max_tolerance ≈ 1 / (tolerance·2)
 *  Returns the tighter constraint. */
double mfb_bp_max_q(double f0, double gain_linear, double gbw_hz,
                     double tolerance_pct);

/* ==================================================================
 * L2: MFB Bandstop (Notch) Filter
 * ================================================================== */

/** Design MFB notch filter.
 *
 *  L2: MFB notch uses a bridged-T RC network in the feedback path.
 *  Also known as the "Bainter notch" or bootstrapped Twin-T.
 *  Deeper notch than passive Twin-T due to active Q enhancement.
 *
 *  Notch frequency: f_n = 1 / (2π·R·C)
 *  Q enhanced by op-amp feedback: Q_active ≈ Q_passive · (1+Rf/Rg)
 *
 *  @param notch_freq  Notch frequency (Hz)
 *  @param q           Q factor (> 1 for narrow notch)
 *  @param depth_db    Desired notch depth (dB, typically 40-60)
 *  @param r_scale     Resistance scale (ohms)
 *  @param comp        Output component values
 *  Complexity: O(1) */
int mfb_notch_design(double notch_freq, double q, double depth_db,
                      double r_scale, active_component_values_t *comp);

/* ==================================================================
 * L3: Transfer Function Analysis for MFB
 * ================================================================== */

/** Compute transfer function coefficients from MFB component values.
 *  L3: Given physical components, derive H(s) = N(s)/D(s).
 *
 *  @param comp     Component values
 *  @param function LP, HP, or BP
 *  @param num      Output numerator [b2, b1, b0]
 *  @param den      Output denominator [1, a1, a0]
 *  @param gain     Output gain
 *  Complexity: O(1) */
int mfb_transfer_function(const active_component_values_t *comp,
                           active_filter_func_t function,
                           double num[3], double den[3], double *gain);

/** Extract pole frequency, Q, and gain from MFB components.
 *  L3: Inverse analysis — from circuit to parameters.
 *  @param comp     Component values
 *  @param function Filter function
 *  @param wp       Output pole frequency (rad/s)
 *  @param qp       Output pole Q
 *  @param gain     Output gain
 *  Complexity: O(1) */
int mfb_extract_params(const active_component_values_t *comp,
                        active_filter_func_t function,
                        double *wp, double *qp, double *gain);

/* ==================================================================
 * L5: MFB Design Optimization
 * ================================================================== */

/** Optimize MFB component values for minimum sensitivity.
 *  L5: Sweeps capacitor ratio C1/C2 to minimize worst-case
 *  sensitivity to component tolerance.
 *  Returns the optimal C1/C2 ratio and corresponding components.
 *
 *  @param wp       Pole frequency (rad/s)
 *  @param qp       Pole Q
 *  @param gain     DC gain (magnitude)
 *  @param ftype    LP or BP
 *  @param comp     Output: optimized component values
 *  @param c_ratio  Output: optimal C1/C2 ratio
 *  Complexity: O(N_sweep) — linear sweep over capacitor ratios */
int mfb_optimize_sensitivity(double wp, double qp, double gain,
                              active_filter_func_t ftype,
                              active_component_values_t *comp,
                              double *c_ratio);

#ifdef __cplusplus
}
#endif

#endif /* ACTIVE_FILTER_MFB_H */
