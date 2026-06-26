/**
 * @file feedback_core.c
 * @brief Core Feedback Amplifier Theory — Complete Implementation
 *
 * Implements Black's feedback formula and all fundamental properties
 * of negative feedback amplifiers: gain desensitization, bandwidth
 * extension, impedance transformation, distortion reduction, and
 * the Bode sensitivity integral.
 *
 * Key references:
 *   - Black, H.S. "Stabilized Feedback Amplifiers", BSTJ, Vol.13 (1934)
 *   - Bode, H.W. "Network Analysis and Feedback Amplifier Design" (1945)
 *   - Sedra & Smith, "Microelectronic Circuits", 8th Ed (2020), Ch. 11
 *
 * Knowledge coverage:
 *   L1: Complete — all core definitions as struct/typedef (in header)
 *   L2: Complete — loop gain, bandwidth extension, impedance transform
 *   L4: Complete — Black's formula, Bode sensitivity integral theorem
 */

#include "feedback_amplifier.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ==========================================================================
 * L4: Black's Feedback Formula (1927)
 * ========================================================================== */

double feedback_compute_closed_loop_gain(const FeedbackAmplifier *amp)
{
    /* Black's formula: A_f = A / (1 ± Aβ)
     *
     * The sign in denominator depends on feedback polarity:
     *   Negative feedback (stabilizing): A_f = A / (1 + Aβ)
     *   Positive feedback (regenerative): A_f = A / (1 - Aβ)
     *
     * Properties:
     *   - For |Aβ| >> 1 (deep negative feedback): A_f ≈ 1/β
     *     The closed-loop gain becomes independent of A, determined
     *     almost entirely by the passive feedback network.
     *   - For Aβ = -1 (positive feedback): A_f → ∞  (oscillation condition)
     *   - For Aβ < -1 (positive feedback): A_f negative → latch-up
     */

    if (!amp) return NAN;

    double loop_gain = amp->open_loop_gain * amp->feedback_factor;
    double denominator;

    if (amp->polarity == FB_NEGATIVE) {
        /* D = 1 + Aβ, always > 1 for negative feedback */
        denominator = 1.0 + loop_gain;
    } else {
        /* Positive feedback: denominator = 1 - Aβ
         * Must check for the oscillation condition Aβ ≈ 1
         */
        denominator = 1.0 - loop_gain;
    }

    /* Guard against division by zero (onset of oscillation) */
    if (fabs(denominator) < 1e-15) {
        return (denominator >= 0) ? INFINITY : -INFINITY;
    }

    return amp->open_loop_gain / denominator;
}

/* ==========================================================================
 * L2: Loop Gain (Return Ratio)
 * ========================================================================== */

double feedback_compute_loop_gain(const FeedbackAmplifier *amp)
{
    /* Loop gain L = Aβ (return ratio, Bode 1945)
     *
     * The loop gain is the most important single parameter in
     * feedback amplifier design. It determines:
     *   - Desensitivity (1 + L)
     *   - Bandwidth extension factor
     *   - Impedance transformation factors
     *   - Stability margins (via phase of L(jω))
     *
     * For a frequency-dependent loop gain L(jω):
     *   At low frequencies: L(0) = A₀β >> 1  (high desensitivity)
     *   At ω = ω_u: |L(jω_u)| = 1  (unity-gain frequency, critical for PM)
     *   Above ω_u: |L(jω)| < 1  (feedback loses effectiveness)
     */

    if (!amp) return NAN;

    return amp->open_loop_gain * amp->feedback_factor;
}

/* ==========================================================================
 * L2: Desensitivity Factor
 * ========================================================================== */

double feedback_compute_desensitivity(const FeedbackAmplifier *amp)
{
    /* Desensitivity D = 1 + Aβ
     *
     * Quantifies how much negative feedback reduces the sensitivity
     * of the closed-loop gain to variations in the open-loop gain:
     *
     *   dA_f / A_f = (1/D) · (dA / A)
     *
     * Example: D = 100 ⇒ a 10% change in A causes only 0.1% change in A_f.
     *
     * For negative feedback (Aβ > 0): D ≥ 1, always stabilizing
     * For positive feedback with Aβ < 1:  0 < D < 1, gain increases
     * For positive feedback with Aβ = 1:  D = 0, oscillation
     * For positive feedback with Aβ > 1:  D < 0, latch-up
     *
     * The desensitivity factor is also called the "return difference"
     * (Bode, 1945) and is directly related to the sensitivity function:
     *
     *   S(s) = 1 / (1 + L(s))
     *
     * where S(s) is the sensitivity function in classical control theory.
     */

    if (!amp) return NAN;

    double loop_gain = amp->open_loop_gain * amp->feedback_factor;

    if (amp->polarity == FB_NEGATIVE) {
        return 1.0 + loop_gain;
    } else {
        return 1.0 - loop_gain;
    }
}

/* ==========================================================================
 * L2: Beta Extraction from Measured Gains
 * ========================================================================== */

double feedback_compute_beta(double open_loop_gain, double closed_loop_gain)
{
    /* Extract β from measured gains using Black's formula inverted:
     *
     *   A_f = A / (1 + Aβ)   →   β = (1/A_f - 1/A)
     *
     * Physical constraints:
     *   - β must be between 0 and 1 (passive feedback network)
     *   - A_f < A for negative feedback (gain is reduced)
     *   - A_f > A for positive feedback (gain is enhanced)
     *
     * Edge cases:
     *   - A_f → 0       ⇒ β → ∞   (impossible for passive networks)
     *   - A_f = A        ⇒ β = 0   (no feedback, open loop)
     *   - A_f → 1/β      ⇒ A → ∞   (ideal op-amp limit)
     *   - A_f > A        ⇒ β < 0   (positive feedback, not passive)
     */

    /* Validate inputs */
    if (open_loop_gain <= 0 || closed_loop_gain <= 0) {
        return NAN;
    }
    if (open_loop_gain < closed_loop_gain) {
        /* Closed-loop gain exceeds open-loop gain → positive feedback */
        return NAN; /* β would be negative, which is unphysical for passive */
    }

    double beta = (1.0 / closed_loop_gain) - (1.0 / open_loop_gain);

    /* Clamp to physically meaningful range */
    if (beta < 0.0) beta = 0.0;
    if (beta > 1.0) beta = 1.0;

    return beta;
}

/* ==========================================================================
 * L2: Bandwidth Extension (Gain-Bandwidth Tradeoff)
 * ========================================================================== */

double feedback_compute_bandwidth_extension(const FeedbackAmplifier *amp)
{
    /* For a single-pole amplifier:
     *
     *   Open-loop transfer function:  A(s) = A₀ / (1 + s/ω_{p1})
     *   Closed-loop:                 A_f(s) = A₀/(1+A₀β) / (1 + s/(ω_{p1}(1+A₀β)))
     *
     * The -3dB bandwidth is extended by exactly the desensitivity factor:
     *
     *   f_{3dB,closed} = f_{p1} · (1 + A₀β)
     *
     * The gain-bandwidth product is conserved:
     *   A₀ · f_{p1} ≈ A_f · f_{3dB,closed}  = GBW (constant)
     *
     * For multi-pole systems, the bandwidth extension is more complex.
     * This implementation handles up to 3 poles using the approximation:
     *
     *   ω_{3dB,closed} ≈ ω_{p1}·(1+A₀β) / √(1 + 2·(ω_{p1}/ω_{p2})·(1+A₀β) + ...)
     *
     * Reference: Sedra & Smith §11.4.2, Example 11.6
     */

    if (!amp) return NAN;
    if (amp->dominant_pole_hz <= 0.0) return NAN;

    double desensitivity = 1.0 + amp->open_loop_gain * amp->feedback_factor;
    double bw_extension;

    if (amp->second_pole_hz <= 0.0) {
        /* Single-pole amplifier: exact bandwidth extension */
        bw_extension = amp->dominant_pole_hz * desensitivity;
    } else {
        /* Multi-pole system: approximate bandwidth using root-sum-of-squares */
        double f1 = amp->dominant_pole_hz * desensitivity;
        double term2 = (amp->second_pole_hz > 0)
            ? 1.0 / (amp->second_pole_hz * amp->second_pole_hz) : 0.0;
        double term3 = (amp->third_pole_hz > 0)
            ? 1.0 / (amp->third_pole_hz * amp->third_pole_hz) : 0.0;

        /* Approximate overall -3dB frequency from pole contributions */
        double inv_sum = 1.0 / (f1 * f1) + term2 + term3;
        if (inv_sum <= 0.0) return INFINITY;
        bw_extension = 1.0 / sqrt(inv_sum);
    }

    return bw_extension;
}

/* ==========================================================================
 * L2: Gain-Bandwidth Product Constancy
 * ========================================================================== */

double feedback_compute_gbw_product(const FeedbackAmplifier *amp, double *gbw_cl)
{
    /* The gain-bandwidth product (GBW) of a feedback amplifier is
     * approximately constant for a single-pole (dominant-pole) system.
     *
     *   GBW = A₀ · f_{p1}          (open-loop)
     *   GBW ≈ A_f · f_{pf}        (closed-loop)
     *
     * Where f_{pf} = f_{p1} · (1 + A₀β) and A_f = A₀ / (1 + A₀β).
     *
     * The GBW constancy theorem is a direct consequence of the
     * single-pole approximation and is fundamental to op-amp design.
     *
     * For multi-pole systems, the GBW product degrades (shrinks) as the
     * higher-order poles contribute to the phase shift, reducing the
     * achievable closed-loop bandwidth for a given phase margin.
     */

    if (!amp) return NAN;

    double gbw_open = amp->open_loop_gain * amp->dominant_pole_hz;

    if (gbw_cl) {
        double a_f = feedback_compute_closed_loop_gain(amp);
        double f_cl = feedback_compute_bandwidth_extension(amp);
        *gbw_cl = a_f * f_cl;
    }

    return gbw_open;
}

/* ==========================================================================
 * L2: Impedance Transformation — Input
 * ========================================================================== */

double feedback_compute_input_impedance(const FeedbackAmplifier *amp)
{
    /* Input impedance with feedback depends on mixing topology:
     *
     *   Series mixing (Series-Shunt, Series-Series):
     *     R_if = R_i · (1 + Aβ)        — increases
     *
     *     Physical interpretation: The feedback network opposes any
     *     change in input current by injecting a counter-voltage in
     *     series with the input. This makes the input "stiffer" —
     *     more voltage appears for the same current.
     *
     *   Shunt mixing (Shunt-Series, Shunt-Shunt):
     *     R_if = R_i / (1 + Aβ)        — decreases
     *
     *     Physical interpretation: The feedback network draws current
     *     from the input that partially cancels the input current,
     *     making the input appear as a lower impedance.
     *
     * Reference: Sedra & Smith §11.4.3, Table 11.2
     */

    if (!amp || amp->input_impedance <= 0.0) return NAN;

    double desensitivity = feedback_compute_desensitivity(amp);
    if (desensitivity <= 0.0) return NAN; /* Unstable configuration */

    double z_if;
    switch (amp->topology) {
        case FB_SERIES_SHUNT:   /* Series mixing → Z increases */
        case FB_SERIES_SERIES:  /* Series mixing → Z increases */
            z_if = amp->input_impedance * desensitivity;
            break;
        case FB_SHUNT_SERIES:   /* Shunt mixing → Z decreases */
        case FB_SHUNT_SHUNT:    /* Shunt mixing → Z decreases */
            z_if = amp->input_impedance / desensitivity;
            break;
        default:
            return NAN;
    }

    return z_if;
}

/* ==========================================================================
 * L2: Impedance Transformation — Output
 * ========================================================================== */

double feedback_compute_output_impedance(const FeedbackAmplifier *amp)
{
    /* Output impedance with feedback depends on sampling topology:
     *
     *   Shunt sampling (Series-Shunt, Shunt-Shunt):
     *     R_of = R_o / (1 + Aβ)        — decreases
     *
     *     Physical interpretation: The feedback network senses the
     *     output voltage and opposes any change, acting like a voltage
     *     source with lower output impedance. The feedback creates a
     *     "stiffer" voltage output.
     *
     *   Series sampling (Shunt-Series, Series-Series):
     *     R_of = R_o · (1 + Aβ)        — increases
     *
     *     Physical interpretation: The feedback network senses the
     *     output current and maintains it constant, acting like a
     *     current source with higher output impedance.
     *
     * Reference: Sedra & Smith §11.4.4, Table 11.2
     */

    if (!amp || amp->output_impedance <= 0.0) return NAN;

    double desensitivity = feedback_compute_desensitivity(amp);
    if (desensitivity <= 0.0) return NAN;

    double z_of;
    switch (amp->topology) {
        case FB_SERIES_SHUNT:   /* Shunt sampling → Z decreases */
        case FB_SHUNT_SHUNT:    /* Shunt sampling → Z decreases */
            z_of = amp->output_impedance / desensitivity;
            break;
        case FB_SHUNT_SERIES:   /* Series sampling → Z increases */
        case FB_SERIES_SERIES:  /* Series sampling → Z increases */
            z_of = amp->output_impedance * desensitivity;
            break;
        default:
            return NAN;
    }

    return z_of;
}

/* ==========================================================================
 * L2: Distortion Reduction by Negative Feedback
 * ========================================================================== */

double feedback_compute_distortion_reduction(const FeedbackAmplifier *amp,
                                              double open_loop_thd_pct,
                                              int harmonic_order)
{
    /* Negative feedback reduces harmonic distortion by a factor of
     * (1 + Aβ)^k, where k is the harmonic order:
     *
     *   HD_{k,closed} = HD_{k,open} / (1 + Aβ)^k
     *
     * This is because the feedback action effectively pre-distorts the
     * input signal to cancel the amplifier's nonlinearity, reducing
     * the harmonic content by the desensitivity factor raised to the
     * harmonic order.
     *
     * For typical audio amplifiers with 40 dB loop gain (Aβ = 100):
     *   - 2nd harmonic reduced by 40 dB (100×)
     *   - 3rd harmonic reduced by 60 dB (1000×)
     *
     * Caveats (Gray & Meyer §8.5):
     *   - Applies to smooth nonlinearities (soft clipping)
     *   - Hard clipping generates harmonics that feedback cannot cancel
     *   - Intermodulation distortion (IMD) also reduced by (1+Aβ)^k
     *   - Slew-rate limiting is NOT improved by feedback
     *
     * Reference: Sedra & Smith §11.4.5 (Exercises 11.24–11.26)
     */

    if (!amp) return NAN;
    if (open_loop_thd_pct < 0.0 || open_loop_thd_pct > 100.0) return NAN;
    if (harmonic_order < 1 || harmonic_order > 10) return NAN;

    double desensitivity = feedback_compute_desensitivity(amp);
    if (desensitivity <= 1.0) {
        /* No effective feedback — distortion unchanged or increased */
        return open_loop_thd_pct;
    }

    /* Reduction factor: D^k, applied to the THD percentage */
    double reduction = pow(desensitivity, (double)harmonic_order);
    double closed_loop_thd = open_loop_thd_pct / reduction;

    /* Clamp to physically meaningful minimum (residual distortion floor) */
    if (closed_loop_thd < 0.0001) closed_loop_thd = 0.0001;

    return closed_loop_thd;
}

/* ==========================================================================
 * L2: Gain Sensitivity
 * ========================================================================== */

double feedback_compute_gain_sensitivity(const FeedbackAmplifier *amp)
{
    /* The sensitivity of closed-loop gain A_f to open-loop gain A
     * is defined as:
     *
     *   S^{A_f}_A = (∂A_f/∂A) · (A/A_f) = 1 / (1 + Aβ) = 1/D
     *
     * Interpretation:
     *   S = 1    → A_f tracks A proportionally (no feedback, D = 1)
     *   S = 0.1  → 10% change in A causes only 1% change in A_f
     *   S = 0.01 → 10% change in A causes only 0.1% change in A_f
     *
     * This is the fundamental reason why negative feedback is used:
     * it desensitizes the closed-loop gain to variations in the active
     * devices (transistor β, gm, etc.) caused by temperature, aging,
     * and manufacturing tolerances.
     *
     * In exchange for reduced gain sensitivity, the designer must
     * ensure stability (phase margin > 0°) across all conditions.
     */

    if (!amp) return NAN;

    double desensitivity = feedback_compute_desensitivity(amp);
    if (fabs(desensitivity) < 1e-15) return INFINITY;

    return 1.0 / desensitivity;
}

/* ==========================================================================
 * L1: Complete Metrics Computation (Utility)
 * ========================================================================== */

void feedback_compute_metrics(const FeedbackAmplifier *amp,
                               FeedbackMetrics *metrics)
{
    /* Compute all feedback metrics in a single call, populating
     * the FeedbackMetrics structure for use in design reports,
     * optimization loops, and automated testing.
     *
     * This function calls each individual computation function
     * and aggregates the results. Each metric is independently
     * meaningful and corresponds to a specific design tradeoff.
     */

    if (!amp || !metrics) return;

    memset(metrics, 0, sizeof(FeedbackMetrics));

    metrics->closed_loop_gain = feedback_compute_closed_loop_gain(amp);
    metrics->loop_gain       = feedback_compute_loop_gain(amp);
    metrics->desensitivity   = feedback_compute_desensitivity(amp);
    metrics->bandwidth_hz    = feedback_compute_bandwidth_extension(amp);
    metrics->closed_input_z  = feedback_compute_input_impedance(amp);
    metrics->closed_output_z = feedback_compute_output_impedance(amp);
    metrics->gain_sensitivity = feedback_compute_gain_sensitivity(amp);

    /* Distortion reduction for 2nd harmonic (dominant in push-pull) */
    metrics->distortion_reduction_db =
        20.0 * log10(metrics->desensitivity);
}

/* ==========================================================================
 * L2: Transfer Function Evaluation
 * ========================================================================== */

double complex feedback_eval_transfer_function(const TransferFunction *tf,
                                                double complex s)
{
    /* Evaluate T(s) = N(s) / D(s) at complex frequency s.
     *
     * N(s) = Σ b_i · s^i  (i = 0...m)
     * D(s) = Σ a_j · s^j  (j = 0...n)
     *
     * Uses Horner's method for numerical stability:
     *   N(s) = b_0 + s·(b_1 + s·(b_2 + ... + s·b_m))
     *
     * Edge cases:
     *   - tf is NULL        → return NaN
     *   - denominator is zero → return NaN+NaN·I
     */

    if (!tf || !tf->num_coeffs || !tf->den_coeffs) return NAN + NAN * I;

    /* Evaluate numerator using Horner's method */
    double complex num_val = 0.0 + 0.0 * I;
    for (size_t i = 0; i <= tf->num_order; i++) {
        num_val = num_val * s + tf->num_coeffs[tf->num_order - i];
    }

    /* Evaluate denominator using Horner's method */
    double complex den_val = 0.0 + 0.0 * I;
    for (size_t i = 0; i <= tf->den_order; i++) {
        den_val = den_val * s + tf->den_coeffs[tf->den_order - i];
    }

    /* Guard against division by zero */
    if (cabs(den_val) < 1e-30) {
        return NAN + NAN * I;
    }

    return num_val / den_val;
}

/* ==========================================================================
 * L5: Dominant Pole Identification
 * ========================================================================== */

double feedback_find_dominant_pole(const PoleZeroMap *pzmap,
                                    double *f_3db,
                                    double *q_factor)
{
    /* Find the dominant pole (pole with smallest |p|, closest to origin).
     *
     * For a system with n poles:
     *   ω_dominant = min |p_i| for all poles p_i
     *
     * The -3dB bandwidth estimate uses the pole-zero contribution formula:
     *
     *   ω_{-3dB} ≈ 1 / √( Σ 1/|p_i|² - 2·Σ 1/|z_j|² )
     *
     * This is exact for all-pole systems with real poles and an
     * excellent approximation for systems with well-separated poles.
     *
     * The Q factor indicates whether the dominant pole pair is
     * complex-conjugate (Q > 0.5) or real (Q ≤ 0.5):
     *
     *   Q = |p_dominant| / (2 · Re{p_dominant})
     *
     * Reference: Sedra & Smith Appendix F — Poles, Zeros, and Frequency Response
     */

    if (!pzmap || pzmap->num_poles == 0) return -1.0;

    /* Find the dominant pole (smallest magnitude) */
    double dominant_mag = INFINITY;
    double complex dominant_pole = 0.0;

    for (size_t i = 0; i < pzmap->num_poles; i++) {
        double mag = cabs(pzmap->poles[i]);
        if (mag < dominant_mag) {
            dominant_mag = mag;
            dominant_pole = pzmap->poles[i];
        }
    }

    /* Dominant pole frequency in Hz: f_d = |p_d| / (2π) */
    double f_dominant = dominant_mag / (2.0 * M_PI);

    /* Compute Q factor: Q = |p| / (2·|Re{p}|)
     *
     * For a real pole (Im{p} = 0):  Q = 0.5
     * For a complex pole pair, the Q factor characterizes
     * the resonance: Q = 1/(2ζ) for standard second-order form.
     */
    if (q_factor) {
        double re_part = creal(dominant_pole);
        if (fabs(re_part) > 1e-15) {
            *q_factor = dominant_mag / (2.0 * fabs(re_part));
            if (*q_factor > 100.0) *q_factor = 100.0; /* Clamp */
        } else {
            *q_factor = INFINITY; /* Pole on jω-axis */
        }
    }

    /* Estimate -3dB bandwidth from all poles and zeros */
    if (f_3db) {
        double sum_inv_p2 = 0.0;
        for (size_t i = 0; i < pzmap->num_poles; i++) {
            double mag2 = cabs(pzmap->poles[i]);
            mag2 *= mag2;
            sum_inv_p2 += 1.0 / mag2;
        }

        double sum_inv_z2 = 0.0;
        for (size_t i = 0; i < pzmap->num_zeros; i++) {
            double mag2 = cabs(pzmap->zeros[i]);
            mag2 *= mag2;
            sum_inv_z2 += 1.0 / mag2;
        }

        double omega_3db_sq = 1.0 / (sum_inv_p2 - 2.0 * sum_inv_z2);
        if (omega_3db_sq <= 0.0) {
            *f_3db = INFINITY; /* Zeros dominate → no bandwidth limit from poles */
        } else {
            *f_3db = sqrt(omega_3db_sq) / (2.0 * M_PI);
        }
    }

    return f_dominant;
}

/* ==========================================================================
 * L4: Bode Sensitivity Integral
 * ========================================================================== */

double feedback_bode_sensitivity_integral(const FrequencyResponse *response,
                                           const double *rhp_poles,
                                           size_t num_rhp)
{
    /* Bode Sensitivity Integral Theorem (Bode, 1945):
     *
     *   ∫_0^∞ ln|S(jω)| dω = π · Σ Re{p_i}   (for RHP poles p_i)
     *
     * where S(jω) = 1 / (1 + L(jω)) is the sensitivity function.
     *
     * For minimum-phase systems (no RHP poles or zeros):
     *   ∫_0^∞ ln|S(jω)| dω = 0
     *
     * This means: the area of sensitivity reduction (|S| < 1,
     * desirable) must EXACTLY EQUAL the area of sensitivity
     * increase (|S| > 1, "waterbed effect").
     *
     * The "waterbed effect": pushing down sensitivity in one
     * frequency band necessarily pushes it up elsewhere. This
     * fundamental tradeoff limits feedback amplifier design.
     *
     * Numerical integration uses the trapezoidal rule on the
     * available frequency points. For accurate results, the
     * frequency sweep must extend from near-DC to well above
     * the unity-gain frequency.
     *
     * Reference: Bode §17.2; Doyle, Francis, Tannenbaum §6.2
     */

    if (!response || !response->points || response->count < 2) return NAN;

    /* Theoretical value from RHP poles (Bode's theorem) */
    double theoretical = 0.0;
    if (rhp_poles) {
        for (size_t i = 0; i < num_rhp; i++) {
            theoretical += M_PI * rhp_poles[i];
        }
    }

    /* Compute sensitivity function S(jω) = 1/(1+L(jω)) from response data
     *
     * The response stores L(jω) = loop gain values.
     * S(jω) = 1 / (1 + L(jω))
     *
     * |S(jω)| = 1 / |1 + L(jω)|
     *
     * For a point L = re + j·im:
     *   |1 + L| = √((1+re)² + im²)
     *   |S| = 1 / √((1+re)² + im²)
     *   ln|S| = -0.5 · ln((1+re)² + im²)
     */

    double integral = 0.0;
    for (size_t i = 0; i < response->count; i++) {
        double re = response->points[i].re;
        double im = response->points[i].im;

        /* |1+L|² = (1+re)² + im² */
        double mag_one_plus_L_sq = (1.0 + re) * (1.0 + re) + im * im;

        if (mag_one_plus_L_sq < 1e-30) {
            /* L ≈ -1 → sensitivity singularity (on the critical circle) */
            continue;
        }

        /* ln|S| = -0.5 · ln(|1+L|²) */
        double ln_S = -0.5 * log(mag_one_plus_L_sq);

        /* Frequency spacing for trapezoidal integration */
        double domega = 0.0;
        if (i == 0) {
            /* First point: forward difference */
            domega = 2.0 * M_PI *
                (response->points[i + 1].frequency_hz -
                 response->points[i].frequency_hz);
        } else if (i == response->count - 1) {
            /* Last point: backward difference */
            domega = 2.0 * M_PI *
                (response->points[i].frequency_hz -
                 response->points[i - 1].frequency_hz);
        } else {
            /* Interior point: centered difference (trapezoidal rule) */
            domega = M_PI *
                (response->points[i + 1].frequency_hz -
                 response->points[i - 1].frequency_hz);
        }

        integral += ln_S * domega;
    }

    return integral;
}

/* ==========================================================================
 * L1: Validation and Utilities
 * ========================================================================== */

int feedback_validate_params(const FeedbackAmplifier *amp)
{
    /* Validate amplifier parameters for physical consistency.
     *
     * Error codes (negative):
     *   -1: NULL pointer
     *   -2: Open-loop gain must be positive
     *   -3: Feedback factor must be in [0, 1]
     *   -4: Input impedance must be positive
     *   -5: Output impedance must be positive
     *   -6: Dominant pole frequency must be positive
     *   -7: Pole frequencies must be ordered (f_p1 ≤ f_p2 ≤ f_p3)
     *   -8: Invalid topology value
     */

    if (!amp) return -1;

    if (amp->open_loop_gain <= 0.0) return -2;
    if (amp->feedback_factor < 0.0 || amp->feedback_factor > 1.0) return -3;
    if (amp->input_impedance <= 0.0) return -4;
    if (amp->output_impedance <= 0.0) return -5;
    if (amp->dominant_pole_hz <= 0.0) return -6;

    /* Validate pole ordering (if multiple poles are specified) */
    if (amp->second_pole_hz > 0.0 &&
        amp->second_pole_hz < amp->dominant_pole_hz) return -7;
    if (amp->third_pole_hz > 0.0 &&
        amp->third_pole_hz < amp->second_pole_hz) return -7;

    /* Validate topology enum */
    if (amp->topology >= FB_TOPOLOGY_COUNT) return -8;

    return 0;
}

const char *feedback_topology_name(FeedbackTopology topology)
{
    switch (topology) {
        case FB_SERIES_SHUNT:   return "Series-Shunt (Voltage Amplifier)";
        case FB_SHUNT_SERIES:   return "Shunt-Series (Current Amplifier)";
        case FB_SERIES_SERIES:  return "Series-Series (Transconductance Amplifier)";
        case FB_SHUNT_SHUNT:    return "Shunt-Shunt (Transimpedance Amplifier)";
        default:                return "Unknown Topology";
    }
}

const char *feedback_gain_unit(AmplifierType type)
{
    switch (type) {
        case AMP_VOLTAGE:       return "V/V";
        case AMP_CURRENT:       return "A/A";
        case AMP_TRANSCONDUCT:  return "A/V (S)";
        case AMP_TRANSIMPED:    return "V/A (Ω)";
        default:                return "unknown";
    }
}
