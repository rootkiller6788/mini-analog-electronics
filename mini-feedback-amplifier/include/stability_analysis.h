/**
 * @file stability_analysis.h
 * @brief Stability Analysis of Feedback Amplifiers
 *
 * Covers the complete stability theory for feedback amplifiers:
 *   - Bode stability criterion (phase margin, gain margin)
 *   - Nyquist stability criterion
 *   - Routh-Hurwitz stability test
 *   - Root locus analysis
 *   - Conditional and unconditional stability
 *
 * Key Theorem (Nyquist, 1932):
 *   A feedback system is stable iff the Nyquist plot of the loop gain L(jω)
 *   does NOT encircle the critical point (-1, j0) in the clockwise direction.
 *
 * Key Theorem (Bode, 1945):
 *   For minimum-phase systems, stability requires:
 *     Phase margin > 0°  AND  Gain margin > 0 dB
 *
 * References:
 *   - Bode, H.W. "Network Analysis and Feedback Amplifier Design" (1945)
 *   - Nyquist, H. "Regeneration Theory", BSTJ, 1932
 *   - Ogata, K. "Modern Control Engineering", 5th Ed (2010), Ch. 7–8
 *   - Sedra & Smith §11.6 — Stability and Frequency Compensation
 *
 * Knowledge Layer: L2 (Stability concept) + L4 (Nyquist/Bode theorems)
 */

#ifndef STABILITY_ANALYSIS_H
#define STABILITY_ANALYSIS_H

#include "feedback_amplifier.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
 * L1: Stability Definitions & Types
 * -------------------------------------------------------------------------- */

/** Stability classification */
typedef enum {
    STABILITY_STABLE           = 0,  /**< Unconditionally stable              */
    STABILITY_CONDITIONAL      = 1,  /**< Stable only within certain gain range */
    STABILITY_UNSTABLE         = 2,  /**< Unstable (oscillates or latches)    */
    STABILITY_MARGINALLY_STABLE = 3  /**< Sustained oscillation (poles on jω) */
} StabilityClass;

/** Nyquist encirclement direction */
typedef enum {
    NYQUIST_NO_ENCIRCLE       = 0,  /**< No encirclement of (-1, j0)        */
    NYQUIST_CW_ENCIRCLE       = 1,  /**< Clockwise encirclement(s)           */
    NYQUIST_CCW_ENCIRCLE      = 2,  /**< Counter-clockwise encirclement(s)   */
    NYQUIST_PASSES_THROUGH    = 3   /**< Passes directly through (-1, j0)    */
} NyquistEncirclement;

/** Stability margins (Bode definition) */
typedef struct {
    double phase_margin_deg;      /**< PM = 180° + ∠L(jω_u) at unity-gain    */
    double gain_margin_db;        /**< GM = -20·log₁₀|L(jω_π)| at ∠=-180°   */
    double unity_gain_freq_hz;    /**< ω_u where |L(jω_u)| = 1 (0 dB)       */
    double phase_crossover_hz;    /**< ω_π where ∠L(jω_π) = -180°           */
    StabilityClass classification;/**< Overall stability verdict             */
    int    is_minimum_phase;      /**< 1 if no RHP poles/zeros in loop       */
} StabilityMargins;

/**
 * @brief Nyquist analysis result.
 *
 * Contains the Nyquist contour data (real/imag pairs) and the
 * computed encirclement count around the critical point (-1, j0).
 *
 * For P open-loop RHP poles, the Nyquist stability criterion states:
 *   Z = N + P
 * where Z = closed-loop RHP poles, N = CCW encirclements of (-1, j0).
 * Stability requires Z = 0 ⇒ N = -P (CW encirclements equal to P).
 */
typedef struct {
    double            *nyquist_re;      /**< Real part of L(jω) along contour   */
    double            *nyquist_im;      /**< Imag part of L(jω) along contour   */
    size_t             num_points;      /**< Number of points on contour        */
    int                encirclement_count; /**< Signed count (CCW = +, CW = -)  */
    NyquistEncirclement encirclement_type;/**< Type of encirclement relation     */
    int                closed_loop_rhp; /**< Z = N + P  (closed-loop RHP poles) */
} NyquistResult;

/**
 * @brief Routh-Hurwitz stability analysis of a polynomial.
 *
 * For a denominator polynomial:
 *   D(s) = a_0·s^n + a_1·s^{n-1} + ... + a_n
 *
 * The Routh array determines the number of RHP roots without
 * explicitly solving for the polynomial roots.
 *
 * Reference: Ogata §5.6 — Routh's Stability Criterion
 */
typedef struct {
    double **routh_array;       /**< Routh table, n+1 rows × m columns          */
    size_t   order;             /**< Polynomial order n                         */
    int      sign_changes;      /**< Number of sign changes in 1st column       */
    int      rhp_roots;         /**< Number of RHP roots (= sign changes)       */
    int      is_stable;         /**< 1 if all roots are in LHP                  */
    int      special_case;      /**< 1 if zero in first column (auxiliary eq.)  */
} RouthHurwitzResult;

/**
 * @brief Root locus data points.
 *
 * Root locus shows how closed-loop poles move in the s-plane
 * as the loop gain K varies from 0 to ∞.
 *
 * Evans, W.R. "Control System Synthesis by Root Locus Method", 1950.
 */
typedef struct {
    double *gain_values;        /**< Loop gain K at each point (sorted asc)     */
    double complex *poles_k;    /**< Closed-loop poles at each gain value       */
    size_t  num_poles;          /**< Number of closed-loop poles                */
    size_t  num_gains;          /**< Number of gain points in the locus         */
    double  k_critical;         /**< Gain value at jω-axis crossing (if any)    */
    double  osc_freq_hz;        /**< Oscillation freq at K_critical (if any)    */
} RootLocus;

/* -----------------------------------------------------------------------------
 * L4: Bode Stability Criterion
 * -------------------------------------------------------------------------- */

/**
 * @brief Compute stability margins from frequency response data.
 *
 * Phase Margin (PM):
 *   PM = 180° + ∠L(jω_u)   where ω_u is the unity-gain frequency
 *
 * Gain Margin (GM):
 *   GM = -20·log₁₀(|L(jω_π)|)  where ω_π is the phase crossover (∠=-180°)
 *
 * Stability rule (Bode, 1945, for minimum-phase systems):
 *   Stable ⇔ PM > 0° AND GM > 0 dB
 *
 * Typical design targets:
 *   PM ≥ 45° (conservative), PM ≥ 60° (optimal transient response)
 *   GM ≥ 10 dB
 *
 * Complexity: O(N) where N = number of frequency points
 *
 * @param response  Frequency response of loop gain L(jω)
 * @param margins   [out] Computed stability margins
 */
void stability_compute_margins(const FrequencyResponse *response,
                                StabilityMargins *margins);

/**
 * @brief Assess stability from amplifier parameters (analytic approach).
 *
 * For a known 2-pole or 3-pole amplifier model, this function
 * analytically computes the phase margin without requiring a
 * full frequency sweep, using the formula:
 *
 *   tan(PM) ≈ ω_u / (ω_u²/ω_{p2} + ω_u⁴/ω_{p2}·ω_{p3} + ...)
 *
 * This is the analytic counterpart to stability_compute_margins()
 * for use when only pole frequencies are known.
 *
 * Complexity: O(1)
 *
 * @param amp      Amplifier parameters (poles must be specified)
 * @param margins  [out] Computed stability margins
 */
void stability_assess_from_poles(const FeedbackAmplifier *amp,
                                  StabilityMargins *margins);

/**
 * @brief Compute the phase of L(jω) at a given frequency.
 *
 * For an amplifier with up to 3 poles:
 *   ∠L(jω) = -[arctan(ω/ω_{p1}) + arctan(ω/ω_{p2}) + arctan(ω/ω_{p3})]
 *
 * For negative feedback systems, an additional -180° sign inversion
 * is included (the negative feedback already contributes -180°,
 * making the total phase shift -360° at the dominant pole).
 *
 * Complexity: O(1)
 *
 * @param amp         Amplifier parameters
 * @param freq_hz     Frequency at which to evaluate (Hz)
 * @param include_fb_sign  If 1, include -180° from negative feedback
 * @return            Phase in radians
 */
double stability_compute_phase_at_freq(const FeedbackAmplifier *amp,
                                        double freq_hz,
                                        int include_fb_sign);

/**
 * @brief Compute the magnitude of L(jω) at a given frequency.
 *
 * |L(jω)| = A₀·β / √[(1 + (ω/ω_{p1})²)·(1 + (ω/ω_{p2})²)·(1 + (ω/ω_{p3})²)]
 *
 * Complexity: O(1)
 *
 * @param amp         Amplifier parameters
 * @param freq_hz     Frequency at which to evaluate (Hz)
 * @return            Linear magnitude of loop gain
 */
double stability_compute_magnitude_at_freq(const FeedbackAmplifier *amp,
                                            double freq_hz);

/* -----------------------------------------------------------------------------
 * L4: Nyquist Stability Criterion
 * -------------------------------------------------------------------------- */

/**
 * @brief Generate Nyquist contour data for loop gain L(jω).
 *
 * Computes L(jω) for ω from 0⁺ to +∞ (positive frequencies only).
 * For the complete Nyquist criterion, the contour for negative frequencies
 * is the complex conjugate, and the infinite-radius arc must be considered.
 * This function computes the critical ω > 0 portion only.
 *
 * The caller must free the allocated arrays in NyquistResult.
 *
 * Complexity: O(N) where N is the number of frequency points
 *
 * @param response  Complete frequency response of L(jω)
 * @param result    [out] Nyquist analysis result
 */
void stability_generate_nyquist(const FrequencyResponse *response,
                                 NyquistResult *result);

/**
 * @brief Count Nyquist encirclements of the critical point (-1, j0).
 *
 * Uses the winding number algorithm (angle accumulation) to compute
 * the signed number of encirclements of (-1, j0) by the Nyquist contour.
 *
 * Encirclement count N is positive for CCW, negative for CW.
 * Z = N + P gives closed-loop RHP poles.
 *
 * Complexity: O(N) where N = nyquist_points
 *
 * @param nyquist_re    Real parts of Nyquist points
 * @param nyquist_im    Imaginary parts
 * @param num_points    Number of points
 * @param open_rhp      Number of open-loop RHP poles (P)
 * @return              Encirclement count N (CCW = +, CW = -)
 */
int stability_count_encirclements(const double *nyquist_re,
                                   const double *nyquist_im,
                                   size_t num_points,
                                   int open_rhp);

/**
 * @brief Free memory allocated for a NyquistResult.
 *
 * @param result  NyquistResult to free
 */
void stability_free_nyquist(NyquistResult *result);

/* -----------------------------------------------------------------------------
 * L5: Routh-Hurwitz Stability Test
 * -------------------------------------------------------------------------- */

/**
 * @brief Perform the Routh-Hurwitz stability test on a polynomial.
 *
 * Given D(s) = a_0·s^n + a_1·s^{n-1} + ... + a_{n-1}·s + a_n,
 * this function constructs the Routh array and counts the number
 * of sign changes in the first column to determine the number of
 * roots in the right half-plane (RHP).
 *
 * Theorem (Routh, 1874; Hurwitz, 1895):
 *   A real-coefficient polynomial has all roots in the open LHP
 *   iff all elements in the first column of the Routh array
 *   have the same sign (no sign changes).
 *
 * Special cases handled:
 *   - Zero in first column (auxiliary polynomial method)
 *   - Entire row of zeros (indicates symmetric roots about origin)
 *
 * Complexity: O(n²) where n = polynomial order
 *
 * @param coeffs     Array of coefficients [a_0, ..., a_n]
 * @param order      Polynomial order n
 * @return           Routh-Hurwitz analysis result (caller frees via
 *                   stability_free_routh())
 */
RouthHurwitzResult stability_routh_hurwitz(const double *coeffs,
                                            size_t order);

/**
 * @brief Free memory allocated for RouthHurwitzResult.
 * @param result  Result to free
 */
void stability_free_routh(RouthHurwitzResult *result);

/* -----------------------------------------------------------------------------
 * L5: Root Locus Analysis
 * -------------------------------------------------------------------------- */

/**
 * @brief Compute root locus data for a given open-loop transfer function.
 *
 * For the characteristic equation 1 + K·G(s)·H(s) = 0, this function
 * traces the closed-loop pole locations as K varies from 0 to K_max.
 *
 * Root locus construction rules (Evans, 1950):
 *   R1: Locus starts at open-loop poles (K=0)
 *   R2: Locus ends at open-loop zeros (K→∞) or ∞ along asymptotes
 *   R3: Locus is symmetric about the real axis
 *   R4: Number of asymptotes = n - m (excess poles - zeros)
 *
 * Complexity: O(N · n³) where N = num_gains, n = closed-loop order
 *
 * @param open_loop_tf   Open-loop transfer function G(s)·H(s)
 * @param k_min          Minimum gain value
 * @param k_max          Maximum gain value
 * @param num_gains      Number of gain steps
 * @return               Root locus data (caller frees via stability_free_root_locus())
 */
RootLocus stability_compute_root_locus(const TransferFunction *open_loop_tf,
                                        double k_min,
                                        double k_max,
                                        size_t num_gains);

/**
 * @brief Free memory allocated for RootLocus.
 * @param locus  Root locus data to free
 */
void stability_free_root_locus(RootLocus *locus);

/* -----------------------------------------------------------------------------
 * L2: Damping & Second-Order System Analysis
 * -------------------------------------------------------------------------- */

/**
 * @brief Analyze a second-order feedback system.
 *
 * For a standard second-order closed-loop transfer function:
 *   T(s) = ω_n² / (s² + 2ζ·ω_n·s + ω_n²)
 *
 * Computes the natural frequency ω_n, damping ratio ζ, and
 * relates them to phase margin via the approximate formula:
 *
 *   ζ ≈ PM(deg) / 100    (for PM < 70°)
 *
 * Reference: Ogata §10.3 — Transient Response via PM
 * Complexity: O(1)
 *
 * @param amp              Amplifier parameters
 * @param natural_freq_hz  [out] Natural frequency ω_n / 2π
 * @param damping_ratio    [out] Damping ratio ζ (0–1.0)
 * @param overshoot_pct    [out] Percent overshoot estimate (may be NULL)
 * @param settling_time_s  [out] 2% settling time estimate (may be NULL)
 */
void stability_second_order_analysis(const FeedbackAmplifier *amp,
                                      double *natural_freq_hz,
                                      double *damping_ratio,
                                      double *overshoot_pct,
                                      double *settling_time_s);

/**
 * @brief Check if the amplifier model represents a minimum-phase system.
 *
 * A minimum-phase system has no RHP poles or zeros. For such systems:
 *   - Bode's gain-phase relationship holds uniquely
 *   - Phase margin alone is sufficient for stability assessment
 *   - The sensitivity integral constraint applies
 *
 * Complexity: O(1) for the 3-pole model (poles are pre-specified)
 *
 * @param amp  Amplifier parameters
 * @return     1 if minimum-phase, 0 otherwise
 */
int stability_is_minimum_phase(const FeedbackAmplifier *amp);

#ifdef __cplusplus
}
#endif

#endif /* STABILITY_ANALYSIS_H */
