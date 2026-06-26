/**
 * @file feedback_amplifier.h
 * @brief Core Feedback Amplifier Theory — Definitions & Structures
 *
 * This module implements the fundamental theory of negative feedback
 * amplifiers as presented in:
 *   - Sedra & Smith, "Microelectronic Circuits", 8th Ed (2020), Ch. 11
 *   - Gray, Hurst, Lewis & Meyer, "Analysis and Design of Analog ICs", 5th Ed
 *   - Razavi, "Fundamentals of Microelectronics", 2nd Ed, Ch. 11
 *
 * Knowledge Layer: L1 (Definitions) + L2 (Core Concepts)
 *
 * Core theorem (Black, 1927):
 *   Closed-loop gain:  A_f = A / (1 + A·β)
 *   where A = open-loop gain, β = feedback factor (return ratio)
 *
 * The Four Classical Feedback Topologies (L1):
 *   1. Series-Shunt   (Series mixing, shunt sampling) → Voltage amplifier
 *   2. Shunt-Series   (Shunt mixing, series sampling) → Current amplifier
 *   3. Series-Series  (Series mixing, series sampling) → Transconductance
 *   4. Shunt-Shunt    (Shunt mixing, shunt sampling)   → Transimpedance
 *
 * @see Sedra & Smith §11.1–§11.5
 */

#ifndef FEEDBACK_AMPLIFIER_H
#define FEEDBACK_AMPLIFIER_H

#include <complex.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
 * L1: Core Definitions — Enumerations & Typedefs
 * -------------------------------------------------------------------------- */

/** Feedback topology classification (Sedra & Smith §11.2, Table 11.1).
 *
 * The topology determines:
 *  - Whether the feedback signal is mixed in series or shunt at the input
 *  - Whether the output is sampled in series or shunt
 *  - How the amplifier A-circuit and β-network are interconnected
 */
typedef enum {
    FB_SERIES_SHUNT   = 0,   /**< Voltage amplifier: series mixing, shunt sampling */
    FB_SHUNT_SERIES   = 1,   /**< Current amplifier: shunt mixing, series sampling  */
    FB_SERIES_SERIES  = 2,   /**< Transconductance: series mixing, series sampling   */
    FB_SHUNT_SHUNT    = 3,   /**< Transimpedance: shunt mixing, shunt sampling       */
    FB_TOPOLOGY_COUNT = 4
} FeedbackTopology;

/** Feedback signal type */
typedef enum {
    FB_NEGATIVE = 0,     /**< Negative feedback: stabilizes, reduces distortion */
    FB_POSITIVE = 1      /**< Positive feedback: used in oscillators, Schmitt triggers */
} FeedbackPolarity;

/** Amplifier classification (a.k.a. controlled-source type) */
typedef enum {
    AMP_VOLTAGE       = FB_SERIES_SHUNT,   /**< Voltage-in, voltage-out, Av = Vo/Vi  */
    AMP_CURRENT       = FB_SHUNT_SERIES,   /**< Current-in, current-out, Ai = Io/Ii  */
    AMP_TRANSCONDUCT  = FB_SERIES_SERIES,  /**< Voltage-in, current-out, Gm = Io/Vi  */
    AMP_TRANSIMPED    = FB_SHUNT_SHUNT     /**< Current-in, voltage-out, Rm = Vo/Ii  */
} AmplifierType;

/* -----------------------------------------------------------------------------
 * L1: Core Data Structures
 * -------------------------------------------------------------------------- */

/**
 * @brief Complex-valued transfer function representation at a single frequency.
 *
 * Represents H(jω) = |H|·e^{j∠H} = Re{H} + j·Im{H}.
 * Used for Bode plot points, Nyquist contours, and frequency-domain analysis.
 *
 * Reference: Ogata, "Modern Control Engineering", Ch. 8 (Frequency Response)
 */
typedef struct {
    double frequency_hz;    /**< Frequency point in Hertz                    */
    double magnitude;       /**< |H(jω)| — linear magnitude (not dB)        */
    double phase_rad;       /**< ∠H(jω) — phase angle in radians [-π, +π]  */
    double re;              /**< Real part: Re{H(jω)}                       */
    double im;              /**< Imag part: Im{H(jω)}                       */
} FrequencyPoint;

/**
 * @brief Frequency response vector (Bode data over frequency sweep).
 *
 * Used to compute and store the complete frequency response of an amplifier
 * (open-loop or closed-loop) for stability analysis and compensation design.
 */
typedef struct {
    FrequencyPoint *points; /**< Array of frequency points, sorted ascending  */
    size_t           count; /**< Number of points in the sweep               */
    double           f_min; /**< Minimum frequency in sweep (Hz)             */
    double           f_max; /**< Maximum frequency in sweep (Hz)             */
} FrequencyResponse;

/**
 * @brief Pole-zero representation of a transfer function.
 *
 * For an amplifier with transfer function:
 *   H(s) = K · Π(s - z_i) / Π(s - p_j)
 * where z_i are zeros, p_j are poles, K is the DC gain constant.
 *
 * This structure supports the pole-zero analysis required for
 * stability assessment (L2) and compensation design (L5).
 */
typedef struct {
    double complex *poles;       /**< Array of pole locations in s-plane       */
    size_t          num_poles;   /**< Number of poles                         */
    double complex *zeros;       /**< Array of zero locations in s-plane       */
    size_t          num_zeros;   /**< Number of zeros                        */
    double          dc_gain;     /**< K = DC gain multiplier                  */
} PoleZeroMap;

/**
 * @brief Parameters defining a negative-feedback amplifier.
 *
 * Encapsulates all parameters needed for the Black feedback model:
 *
 *   Closed-loop gain:  A_f(s) = A(s) / (1 + A(s)·β(s))
 *
 * Core references:
 *   - Black, H.S. "Stabilized Feedback Amplifiers", BSTJ, 1934
 *   - Bode, H.W. "Network Analysis and Feedback Amplifier Design", 1945
 */
typedef struct {
    double open_loop_gain;      /**< A0 = mid-band open-loop gain (linear)    */
    double feedback_factor;     /**< β  = feedback network transfer ratio     */
    FeedbackPolarity polarity;  /**< Negative (stabilizing) or positive       */
    FeedbackTopology  topology; /**< Which of the four feedback topologies    */
    double input_impedance;     /**< Z_i of the basic amplifier (Ω)           */
    double output_impedance;    /**< Z_o of the basic amplifier (Ω)           */
    double dominant_pole_hz;    /**< f_p1 = dominant pole frequency (Hz)      */
    double second_pole_hz;      /**< f_p2 = second pole frequency (Hz)        */
    double third_pole_hz;       /**< f_p3 = third pole frequency (0 = none)   */
    double unity_gain_freq_hz;  /**< f_t = unity-gain frequency (Hz)          */
} FeedbackAmplifier;

/**
 * @brief Performance metrics of a feedback amplifier.
 *
 * Computed metrics after closing the loop, quantifying the benefits
 * of negative feedback: gain desensitization, bandwidth extension,
 * impedance transformation, distortion reduction.
 *
 * Reference: Sedra & Smith §11.4 — Properties of Negative Feedback
 */
typedef struct {
    double closed_loop_gain;        /**< A_f = A / (1 + Aβ) — linear         */
    double loop_gain;               /**< L  = A·β                            */
    double desensitivity;           /**< D  = 1 + Aβ (desensitivity factor)  */
    double bandwidth_hz;            /**< f_3dB after feedback                */
    double closed_input_z;          /**< Z_if — input impedance with feedback */
    double closed_output_z;         /**< Z_of — output impedance with feedbk */
    double gain_sensitivity;        /**< dA_f/A_f per unit dA/A              */
    double distortion_reduction_db; /**< THD reduction due to feedback (dB)   */
} FeedbackMetrics;

/* -----------------------------------------------------------------------------
 * L2: Core Concept — Feedback Loop Transfer Functions
 * -------------------------------------------------------------------------- */

/**
 * @brief represents a rational transfer function T(s) = N(s)/D(s).
 *
 * N(s) = b_0 + b_1·s + b_2·s² + ... + b_m·s^m
 * D(s) = a_0 + a_1·s + a_2·s² + ... + a_n·s^n
 *
 * Standardized for the feedback loop A(s)·β(s) computation.
 */
typedef struct {
    double *num_coeffs;    /**< [b_0, b_1, ..., b_m] numerator coefficients   */
    size_t  num_order;     /**< m = numerator order                          */
    double *den_coeffs;    /**< [a_0, a_1, ..., a_n] denominator coeffs      */
    size_t  den_order;     /**< n = denominator order                        */
} TransferFunction;

/* -----------------------------------------------------------------------------
 * L4: Black's Feedback Formula — Core API
 * -------------------------------------------------------------------------- */

/**
 * @brief Compute closed-loop gain using Black's formula.
 *
 * A_f = A / (1 + A·β)  for negative feedback
 * A_f = A / (1 - A·β)  for positive feedback
 *
 * Theorem (Black, 1927): If |A·β| >> 1, then A_f ≈ 1/β, independent of A.
 * This is the fundamental property exploited in precision amplifier design.
 *
 * Complexity: O(1)
 *
 * @param amp  Pointer to amplifier parameters
 * @return     Closed-loop gain A_f (linear, not dB)
 */
double feedback_compute_closed_loop_gain(const FeedbackAmplifier *amp);

/**
 * @brief Compute the loop gain L = A·β (return ratio).
 *
 * The loop gain is the product of the forward-path gain and the
 * feedback-path attenuation. It is the most important single figure
 * of merit for a feedback system.
 *
 * Bode's return ratio concept (1945): L = -∂x_f/∂x_e (with sign convention)
 *
 * Complexity: O(1)
 *
 * @param amp  Pointer to amplifier parameters
 * @return     Loop gain magnitude |A·β| (linear)
 */
double feedback_compute_loop_gain(const FeedbackAmplifier *amp);

/**
 * @brief Compute the desensitivity factor D = 1 + A·β.
 *
 * The desensitivity factor quantifies how much negative feedback reduces
 * the sensitivity of the closed-loop gain to open-loop gain variations:
 *
 *   dA_f / A_f = (1/D) · (dA/A)
 *
 * D >> 1 ⇒ the closed loop is highly insensitive to A variations.
 *
 * Reference: Sedra & Smith §11.4.1 — Gain Desensitivity
 * Complexity: O(1)
 *
 * @param amp  Pointer to amplifier parameters
 * @return     Desensitivity factor (always ≥1 for negative feedback)
 */
double feedback_compute_desensitivity(const FeedbackAmplifier *amp);

/**
 * @brief Compute the feedback factor β from measured gains.
 *
 * Given open-loop gain A and closed-loop gain A_f (both measured),
 * this function solves Black's formula for β:
 *
 *   β = (1/A_f - 1/A)  for negative feedback
 *
 * This is useful when analyzing existing circuits where β is not
 * directly known but can be inferred from gain measurements.
 *
 * Complexity: O(1)
 *
 * @param open_loop_gain   Measured open-loop gain A (linear)
 * @param closed_loop_gain Measured closed-loop gain A_f (linear)
 * @return                 Extracted feedback factor β
 */
double feedback_compute_beta(double open_loop_gain, double closed_loop_gain);

/* -----------------------------------------------------------------------------
 * L2: Bandwidth Extension & Gain-Bandwidth Product
 * -------------------------------------------------------------------------- */

/**
 * @brief Compute the bandwidth extension due to negative feedback.
 *
 * For a single-pole amplifier:
 *   f_{3dB, closed} = f_{3dB, open} · (1 + A_0·β)
 *
 * The gain-bandwidth product remains constant:
 *   GBW = A_0 · f_p1 = A_f · f_{pf}
 *
 * This is a direct consequence of the dominant-pole approximation.
 * Reference: Sedra & Smith §11.4.2 — Bandwidth Extension
 *
 * Complexity: O(1)
 *
 * @param amp  Pointer to amplifier parameters
 * @return     Closed-loop 3dB bandwidth (Hz)
 */
double feedback_compute_bandwidth_extension(const FeedbackAmplifier *amp);

/**
 * @brief Compute the gain-bandwidth product and verify its constancy.
 *
 * GBW = A_0 · f_p1 (open-loop) ≈ A_f · f_{pf} (closed-loop)
 *
 * This function returns the GBW and, via the output parameter,
 * verifies that the closed-loop GBW matches within numerical tolerance.
 *
 * Reference: Sedra & Smith §11.4.2 — Gain-Bandwidth Product
 * Complexity: O(1)
 *
 * @param amp     Pointer to amplifier parameters
 * @param gbw_cl  [out] Closed-loop GBW for verification (may be NULL)
 * @return        Open-loop gain-bandwidth product (Hz)
 */
double feedback_compute_gbw_product(const FeedbackAmplifier *amp, double *gbw_cl);

/* -----------------------------------------------------------------------------
 * L2: Impedance Transformation due to Feedback
 * -------------------------------------------------------------------------- */

/**
 * @brief Compute input impedance with feedback.
 *
 * The input impedance transformation depends on the mixing topology:
 *   Series mixing (input):    Z_if = Z_i · (1 + Aβ)  — increases
 *   Shunt mixing (input):     Z_if = Z_i / (1 + Aβ)  — decreases
 *
 * Reference: Sedra & Smith §11.4.3–§11.4.4, Table 11.2
 * Complexity: O(1)
 *
 * @param amp  Pointer to amplifier parameters
 * @return     Closed-loop input impedance (Ω)
 */
double feedback_compute_input_impedance(const FeedbackAmplifier *amp);

/**
 * @brief Compute output impedance with feedback.
 *
 * The output impedance transformation depends on the sampling topology:
 *   Shunt sampling (output):  Z_of = Z_o / (1 + Aβ)  — decreases
 *   Series sampling (output): Z_of = Z_o · (1 + Aβ)  — increases
 *
 * Reference: Sedra & Smith §11.4.3–§11.4.4, Table 11.2
 * Complexity: O(1)
 *
 * @param amp  Pointer to amplifier parameters
 * @return     Closed-loop output impedance (Ω)
 */
double feedback_compute_output_impedance(const FeedbackAmplifier *amp);

/* -----------------------------------------------------------------------------
 * L2: Distortion & Nonlinearity Reduction
 * -------------------------------------------------------------------------- */

/**
 * @brief Estimate distortion reduction from negative feedback.
 *
 * For harmonic distortion of order k, the reduction is:
 *   HD_{k,closed} = HD_{k,open} / (1 + Aβ)^k
 *
 * This function computes the reduction for the dominant 2nd and 3rd
 * harmonic components (k = 2, 3), which are the most significant
 * contributors to THD in audio and RF amplifiers.
 *
 * Reference: Gray & Meyer §8.5 — Feedback and Nonlinear Distortion
 * Complexity: O(1)
 *
 * @param amp              Pointer to amplifier parameters
 * @param open_loop_thd_pct Open-loop THD in percent (0–100)
 * @param harmonic_order   Harmonic order (typically 2 or 3)
 * @return                 Estimated closed-loop harmonic distortion (%)
 */
double feedback_compute_distortion_reduction(const FeedbackAmplifier *amp,
                                              double open_loop_thd_pct,
                                              int harmonic_order);
/**
 * @brief Compute the sensitivity of closed-loop gain to open-loop gain variations.
 *
 * S^{A_f}_A = (∂A_f/A_f) / (∂A/A) = 1 / (1 + Aβ)
 *
 * A sensitivity of 0.01 means a 10% change in A produces only a 0.1% change in A_f.
 * Lower is better—this is why negative feedback stabilizes gain.
 *
 * Complexity: O(1)
 *
 * @param amp  Pointer to amplifier parameters
 * @return     Sensitivity ratio (dimensionless, 0 < S ≤ 1)
 */
double feedback_compute_gain_sensitivity(const FeedbackAmplifier *amp);

/* -----------------------------------------------------------------------------
 * L1: Complete Metrics Computation
 * -------------------------------------------------------------------------- */

/**
 * @brief Compute all feedback amplifier metrics in one pass.
 *
 * Populates a FeedbackMetrics structure with all the key performance
 * parameters: closed-loop gain, loop gain, desensitivity, bandwidth,
 * impedance transformations, distortion reduction, and sensitivity.
 *
 * Complexity: O(1)
 *
 * @param amp     Pointer to amplifier parameters (input)
 * @param metrics [out] Pointer to metrics structure (output)
 */
void feedback_compute_metrics(const FeedbackAmplifier *amp,
                               FeedbackMetrics *metrics);

/* -----------------------------------------------------------------------------
 * L2: Transfer Function Evaluation & Pole-Zero Analysis
 * -------------------------------------------------------------------------- */

/**
 * @brief Evaluate a transfer function T(s) at a specific complex frequency s = σ + jω.
 *
 * T(s) = (b_0 + b_1·s + ... + b_m·s^m) / (a_0 + a_1·s + ... + a_n·s^n)
 *
 * Handles the edge case of denominator zero by returning NaN (not a pole-zero
 * cancellation analysis—the caller should check pole locations separately).
 *
 * Complexity: O(n + m) where n, m are numerator/denominator orders
 *
 * @param tf  Transfer function coefficients and orders
 * @param s   Complex frequency point at which to evaluate
 * @return    T(s) as a complex number (or NaN+NaN·i if division by zero)
 */
double complex feedback_eval_transfer_function(const TransferFunction *tf,
                                                double complex s);
/**
 * @brief Find the dominant pole frequency (in Hz) from a PoleZeroMap.
 *
 * The dominant pole is the pole with the smallest magnitude (closest to
 * the origin in the s-plane), which primarily determines the amplifier's
 * bandwidth. For an N-pole system:
 *
 *   ω_{-3dB} ≈ 1 / √(1/ω_{p1}² + ... + 1/ω_{pn}² — 2·Σ 1/(ω_{zi}²) )
 *
 * This function uses the exact formula for up to 3 poles.
 *
 * Complexity: O(n) in number of poles
 *
 * @param pzmap   Pole-zero map of the amplifier
 * @param f_3db   [out] Estimated -3dB frequency (Hz)
 * @param q_factor [out] Quality factor Q (may be NULL for real-pole-only systems)
 * @return         Dominant pole frequency in Hz, or -1.0 on error
 */
double feedback_find_dominant_pole(const PoleZeroMap *pzmap,
                                    double *f_3db,
                                    double *q_factor);

/* -----------------------------------------------------------------------------
 * L4: Bode's Sensitivity Integral (Conservation Law)
 * -------------------------------------------------------------------------- */

/**
 * @brief Compute Bode's sensitivity integral for a feedback amplifier.
 *
 * Bode's Sensitivity Integral Theorem (1945):
 *
 *   ∫_0^∞ ln|S(jω)| dω = π · Σ Re{p_i}  (for RHP poles)
 *
 * For a minimum-phase system (no RHP poles or zeros):
 *   ∫_0^∞ ln|S(jω)| dω = 0
 *
 * This theorem quantifies the fundamental tradeoff in feedback design:
 * sensitivity reduction (|S| < 1) in one frequency band MUST be
 * compensated by sensitivity increase (|S| > 1, "waterbed effect")
 * in another band.
 *
 * Complexity: O(N) where N is the number of frequency points
 *
 * @param response  Frequency response data for loop gain L(jω)
 * @param rhp_poles Array of RHP pole real parts (NULL if none)
 * @param num_rhp   Number of RHP poles
 * @return          Numerical estimate of the Bode integral
 */
double feedback_bode_sensitivity_integral(const FrequencyResponse *response,
                                           const double *rhp_poles,
                                           size_t num_rhp);

/* -----------------------------------------------------------------------------
 * L1: Parameter Validation Utilities
 * -------------------------------------------------------------------------- */

/**
 * @brief Validate amplifier parameters for physical consistency.
 *
 * Checks that gains are positive, impedances are non-negative,
 * pole frequencies are positive and ordered (f_p1 < f_p2 < f_p3),
 * and feedback factor β is within [0, 1].
 *
 * @param amp  Pointer to amplifier parameters
 * @return     0 if valid, negative error code if invalid
 */
int feedback_validate_params(const FeedbackAmplifier *amp);

/**
 * @brief Get a human-readable name for a feedback topology.
 *
 * @param topology  One of the FeedbackTopology enum values
 * @return          String description (e.g., "Series-Shunt (Voltage)")
 */
const char *feedback_topology_name(FeedbackTopology topology);

/**
 * @brief Get unit string for the gain of a given amplifier type.
 *
 * @param type  Amplifier type
 * @return      Unit string ("V/V", "A/A", "A/V (S)", "V/A (Ω)")
 */
const char *feedback_gain_unit(AmplifierType type);

#ifdef __cplusplus
}
#endif

#endif /* FEEDBACK_AMPLIFIER_H */
