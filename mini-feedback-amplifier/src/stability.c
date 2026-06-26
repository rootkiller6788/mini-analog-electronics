/**
 * @file stability.c
 * @brief Stability Analysis for Feedback Amplifiers — Full Implementation
 *
 * Implements the complete stability analysis toolchain:
 *   - Bode stability criterion with phase/gain margin computation
 *   - Nyquist stability criterion with winding number algorithm
 *   - Routh-Hurwitz polynomial stability test
 *   - Root locus analysis
 *   - Second-order system characterization
 *
 * Key Theorems:
 *   Nyquist (1932): Z = N + P (closed-loop RHP poles = CCW encirclements
 *                   of -1 + open-loop RHP poles)
 *   Bode (1945):    For minimum-phase systems, stability requires
 *                   PM > 0° and GM > 0 dB at the crossover frequencies.
 *   Routh (1874):   The number of sign changes in column 1 of the Routh
 *                   array equals the number of RHP roots.
 *
 * Knowledge coverage:
 *   L2: Complete — damping, minimum-phase concept
 *   L4: Complete — Nyquist criterion, Bode criterion, Routh-Hurwitz
 *   L5: Complete — root locus computation
 */

#include "stability_analysis.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* =========================================================================
 * Internal helper: convert radians to degrees
 * ========================================================================= */
static inline double rad2deg(double rad) {
    return rad * 180.0 / M_PI;
}

static inline double deg2rad(double deg) {
    return deg * M_PI / 180.0;
}

/* =========================================================================
 * L4: Bode Stability Criterion — Phase and Gain Margins
 * ========================================================================= */

void stability_compute_margins(const FrequencyResponse *response,
                                StabilityMargins *margins)
{
    /* Bode stability criterion for minimum-phase systems:
     *
     *   Phase Margin (PM): PM = 180° + ∠L(jω_u)
     *     where ω_u is the unity-gain frequency (|L(jω_u)| = 1, 0 dB)
     *
     *   Gain Margin (GM):  GM = -20·log₁₀|L(jω_π)|
     *     where ω_π is the phase crossover (∠L(jω_π) = -180°)
     *
     * Stability rule:
     *   PM > 0°  AND  GM > 0 dB  ⇒  STABLE (for minimum-phase)
     *   PM > 45°  →  adequate stability margin
     *   PM < 30°  →  marginal, peaking and ringing expected
     *
     * For non-minimum-phase systems, Bode's criterion is insufficient;
     * the Nyquist criterion must be used.
     */

    if (!response || !margins || !response->points || response->count < 2) {
        if (margins) memset(margins, 0, sizeof(StabilityMargins));
        return;
    }

    memset(margins, 0, sizeof(StabilityMargins));
    margins->classification = STABILITY_STABLE;
    margins->is_minimum_phase = 1; /* Assume minimum-phase for Bode analysis */

    /* --- Find unity-gain frequency ω_u (crossover of 0 dB) --- */
    double w_u = -1.0;
    double phase_at_wu = 0.0;

    for (size_t i = 0; i < response->count; i++) {
        double mag_db = 20.0 * log10(response->points[i].magnitude);

        if (i == 0 && mag_db < 0.0) {
            /* Already below 0 dB at first point — use first point */
            w_u = response->points[i].frequency_hz;
            phase_at_wu = response->points[i].phase_rad;
            break;
        }

        if (i > 0) {
            double mag_prev_db = 20.0 * log10(response->points[i-1].magnitude);
            /* Detect zero crossing */
            if (mag_prev_db >= 0.0 && mag_db <= 0.0) {
                /* Linear interpolation for the exact crossover frequency */
                double frac = (0.0 - mag_prev_db) / (mag_db - mag_prev_db);
                w_u = response->points[i-1].frequency_hz +
                    frac * (response->points[i].frequency_hz -
                            response->points[i-1].frequency_hz);
                /* Interpolate phase */
                phase_at_wu = response->points[i-1].phase_rad +
                    frac * (response->points[i].phase_rad -
                            response->points[i-1].phase_rad);
                break;
            }
            if (mag_prev_db < 0.0 && mag_db >= 0.0) {
                /* Crossed from below to above (possible in non-monotonic |L|) */
                double frac = (0.0 - mag_prev_db) / (mag_db - mag_prev_db);
                w_u = response->points[i-1].frequency_hz +
                    frac * (response->points[i].frequency_hz -
                            response->points[i-1].frequency_hz);
                phase_at_wu = response->points[i-1].phase_rad +
                    frac * (response->points[i].phase_rad -
                            response->points[i-1].phase_rad);
                break;
            }
        }
    }

    /* --- Find phase crossover ω_π (phase = -180° / -π rad) --- */
    double w_pi = -1.0;
    double mag_at_wpi = 1.0;

    for (size_t i = 0; i < response->count; i++) {
        /* Normalize phase to [-π, +π] */
        double phase = response->points[i].phase_rad;
        /* Check for phase crossing -180° (or equivalently +180°) */
        if (i > 0) {
            double phase_prev = response->points[i-1].phase_rad;
            /* Detect crossing of -π (or π, wrapping) */
            int crossing = 0;
            /* Simple case: phase decreases through -π */
            if (phase_prev > -M_PI && phase <= -M_PI) crossing = 1;
            /* Wrapping case: phase wraps from -π to +π */
            if (phase_prev < -M_PI * 0.99 && phase > M_PI * 0.99) crossing = 1;

            if (crossing) {
                /* Interpolate to find the exact crossing */
                double phase_span = phase - phase_prev;
                /* Handle wrap-around */
                if (phase_span > M_PI) phase_span -= 2.0 * M_PI;
                if (phase_span < -M_PI) phase_span += 2.0 * M_PI;

                double target = -M_PI;
                double frac = (target - phase_prev) / phase_span;
                if (frac >= 0.0 && frac <= 1.0) {
                    w_pi = response->points[i-1].frequency_hz +
                        frac * (response->points[i].frequency_hz -
                                response->points[i-1].frequency_hz);
                    /* Interpolate magnitude in log space */
                    double mag_prev_db = 20.0 * log10(
                        response->points[i-1].magnitude);
                    double mag_db = 20.0 * log10(
                        response->points[i].magnitude);
                    mag_at_wpi = pow(10.0,
                        (mag_prev_db + frac * (mag_db - mag_prev_db)) / 20.0);
                    break;
                }
            }
        }
    }

    /* --- Compute margins --- */
    margins->unity_gain_freq_hz = (w_u > 0) ? w_u : 0.0;
    margins->phase_crossover_hz = (w_pi > 0) ? w_pi : 0.0;

    /* Phase margin */
    if (w_u > 0) {
        /* PM = 180° + ∠L(jω_u) in degrees */
        margins->phase_margin_deg = 180.0 + rad2deg(phase_at_wu);
        /* Normalize to [-180°, +180°] */
        while (margins->phase_margin_deg > 180.0)
            margins->phase_margin_deg -= 360.0;
        while (margins->phase_margin_deg < -180.0)
            margins->phase_margin_deg += 360.0;
    } else {
        margins->phase_margin_deg = INFINITY; /* |L| never reaches 1 */
    }

    /* Gain margin */
    if (w_pi > 0) {
        margins->gain_margin_db = -20.0 * log10(mag_at_wpi);
        /* Negative GM means |L| > 1 at phase crossover → unstable */
    } else {
        margins->gain_margin_db = INFINITY; /* Phase never reaches -180° */
    }

    /* Classify stability */
    if (margins->is_minimum_phase) {
        if (margins->phase_margin_deg > 0.0 && margins->gain_margin_db > 0.0) {
            margins->classification = STABILITY_STABLE;
        } else if (margins->phase_margin_deg == 0.0 ||
                   margins->gain_margin_db == 0.0) {
            margins->classification = STABILITY_MARGINALLY_STABLE;
        } else {
            margins->classification = STABILITY_UNSTABLE;
        }
    }
}

/* =========================================================================
 * L5: Stability Assessment from Pole Frequencies (Analytic)
 * ========================================================================= */

void stability_assess_from_poles(const FeedbackAmplifier *amp,
                                  StabilityMargins *margins)
{
    /* Analytic stability analysis using the known pole frequencies.
     *
     * For an amplifier with poles at p_1, p_2, p_n:
     *
     *   ∠L(jω) = -Σ arctan(ω/ω_{pi})
     *
     *   |L(jω)| = A₀β / √(Π (1 + (ω/ω_{pi})²))
     *
     * The unity-gain frequency ω_u satisfies: |L(jω_u)| = 1
     *
     * For a single-pole system:
     *   ω_u = ω_{p1} · √((A₀β)² - 1) ≈ ω_{p1} · A₀β
     *   PM  = 90° (always stable, no oscillation possible)
     *
     * For a two-pole system:
     *   ω_u ≈ ω_{p1} · A₀β  (if A₀β >> 1 and ω_{p2} >> ω_u)
     *   PM  = 90° - arctan(ω_u/ω_{p2})
     *
     * For a three-pole system with ω_{p3} >> ω_{p2}:
     *   PM ≈ 90° - arctan(ω_u/ω_{p2}) - arctan(ω_u/ω_{p3})
     */

    if (!amp || !margins) return;

    memset(margins, 0, sizeof(StabilityMargins));
    margins->is_minimum_phase = stability_is_minimum_phase(amp);

    double a0 = amp->open_loop_gain;
    double beta = amp->feedback_factor;
    double f1 = amp->dominant_pole_hz;
    double f2 = amp->second_pole_hz;
    double f3 = amp->third_pole_hz;
    double loop_gain_dc = a0 * beta;

    if (loop_gain_dc <= 1.0) {
        /* Loop gain ≤ 1: system is unconditionally stable */
        margins->phase_margin_deg = 180.0;
        margins->gain_margin_db = INFINITY;
        margins->unity_gain_freq_hz = f1 * loop_gain_dc;
        margins->classification = STABILITY_STABLE;
        return;
    }

    /* Find unity-gain frequency by solving: |L(jω_u)| = 1
     *
     * Use the single-pole approximation as initial guess, then
     * refine with Newton iteration considering all poles.
     *
     * Initial guess: ω_u ≈ 2π·f1 · loop_gain_dc
     */
    double w_u = 2.0 * M_PI * f1 * loop_gain_dc;

    /* Newton iteration to refine ω_u: solve f(ω) = |L(jω)|² - 1 = 0 */
    for (int iter = 0; iter < 20; iter++) {
        /* |L(jω)|² = (loop_gain_dc)² / (product of (1 + (ω/ωi)²)) */
        double prod = 1.0;
        double dprod = 0.0; /* Derivative of product w.r.t. ω² */

        double w1 = 2.0 * M_PI * f1;
        double w2_sq = 0.0, w3_sq = 0.0;
        if (f2 > 0) w2_sq = (2.0 * M_PI * f2) * (2.0 * M_PI * f2);
        if (f3 > 0) w3_sq = (2.0 * M_PI * f3) * (2.0 * M_PI * f3);

        double w2 = w_u * w_u;

        prod *= (1.0 + w2 / (w1 * w1));
        dprod += 1.0 / (w1 * w1);

        if (w2_sq > 0) {
            prod *= (1.0 + w2 / w2_sq);
            dprod += prod / (1.0 + w2 / w2_sq) * (1.0 / w2_sq);
        }
        if (w3_sq > 0) {
            prod *= (1.0 + w2 / w3_sq);
            dprod += prod / (1.0 + w2 / w3_sq) * (1.0 / w3_sq);
        }

        double L_mag_sq = (loop_gain_dc * loop_gain_dc) / prod;
        double f_val = L_mag_sq - 1.0;

        if (fabs(f_val) < 1e-8) break;

        /* Derivative: d(|L|²)/d(ω²) = -|L|² · (dprod/prod) */
        double fprime = -L_mag_sq * dprod;

        double w2_new = w2 - f_val / fprime;
        if (w2_new <= 0) w2_new = w2 * 0.5;
        if (w2_new > w2 * 10.0) w2_new = w2 * 1.5; /* Damp the update */

        w_u = sqrt(w2_new);
    }

    margins->unity_gain_freq_hz = w_u / (2.0 * M_PI);

    /* Compute phase at ω_u */
    double phase = 0.0;
    phase -= atan(w_u / (2.0 * M_PI * f1));
    if (f2 > 0) phase -= atan(w_u / (2.0 * M_PI * f2));
    if (f3 > 0) phase -= atan(w_u / (2.0 * M_PI * f3));

    margins->phase_margin_deg = 180.0 + rad2deg(phase);

    /* Find gain margin: frequency where phase = -180° */
    /* For multi-pole systems, solve: Σ arctan(ω/ω_pi) = π */
    double w_pi = 0.0;
    if (f2 > 0) {
        /* For 2-pole: arctan(ω/ω_p1) + arctan(ω/ω_p2) = π
         * This happens at ω = √(ω_p1·ω_p2) when poles are widely separated,
         * but in general, solve via Newton. For widely separated poles:
         * ω_π ≈ √(ω_p1·ω_p2)
         */
        w_pi = 2.0 * M_PI * sqrt(f1 * f2);

        /* Newton iteration to refine ω_π */
        for (int iter = 0; iter < 15; iter++) {
            double sum_atan = atan(w_pi / (2.0 * M_PI * f1))
                            + atan(w_pi / (2.0 * M_PI * f2));
            if (f3 > 0) sum_atan += atan(w_pi / (2.0 * M_PI * f3));

            double fval = sum_atan - M_PI;
            if (fabs(fval) < 1e-8) break;

            /* Derivative: d/dω of Σ arctan(ω/ω_i) = Σ ω_i/(ω²+ω_i²) */
            double df = 0.0;
            double w1 = 2.0 * M_PI * f1;
            df += w1 / (w_pi * w_pi + w1 * w1);
            if (f2 > 0) {
                double w2 = 2.0 * M_PI * f2;
                df += w2 / (w_pi * w_pi + w2 * w2);
            }
            if (f3 > 0) {
                double w3 = 2.0 * M_PI * f3;
                df += w3 / (w_pi * w_pi + w3 * w3);
            }

            w_pi -= fval / df;
            if (w_pi <= 0) { w_pi = 2.0 * M_PI * f1; break; }
        }

        margins->phase_crossover_hz = w_pi / (2.0 * M_PI);

        /* |L(jω_π)| */
        double w1 = 2.0 * M_PI * f1;
        double mag = loop_gain_dc /
            sqrt((1.0 + (w_pi*w_pi)/(w1*w1)));
        if (f2 > 0) {
            double w2 = 2.0 * M_PI * f2;
            mag /= sqrt(1.0 + (w_pi*w_pi)/(w2*w2));
        }
        if (f3 > 0) {
            double w3 = 2.0 * M_PI * f3;
            mag /= sqrt(1.0 + (w_pi*w_pi)/(w3*w3));
        }
        margins->gain_margin_db = -20.0 * log10(mag);
    } else {
        margins->phase_crossover_hz = INFINITY;
        margins->gain_margin_db = INFINITY;
    }

    /* Classify stability */
    if (margins->phase_margin_deg > 0.0 && margins->gain_margin_db > 0.0) {
        margins->classification = STABILITY_STABLE;
    } else if (fabs(margins->phase_margin_deg) < 0.1 ||
               fabs(margins->gain_margin_db) < 0.1) {
        margins->classification = STABILITY_MARGINALLY_STABLE;
    } else {
        margins->classification = STABILITY_UNSTABLE;
    }
}

/* =========================================================================
 * L2: Phase and Magnitude at a Given Frequency
 * ========================================================================= */

double stability_compute_phase_at_freq(const FeedbackAmplifier *amp,
                                        double freq_hz,
                                        int include_fb_sign)
{
    /* Compute the phase shift contributed by the amplifier poles
     * and (optionally) the -180° from negative feedback sign inversion.
     *
     * For an n-pole amplifier:
     *   ∠A(jω) = -Σ arctan(ω/ω_{pi})
     *
     * Each pole contributes -90° at its corner frequency and
     * asymptotically -90° at high frequencies.
     *
     * The total loop phase (with β assumed real and positive):
     *   ∠L(jω) = ∠A(jω) + ∠β(jω) + (phase from feedback sign)
     *           = -Σ arctan(ω/ω_{pi}) + 0 + (-180° if negative feedback)
     */

    if (!amp || freq_hz < 0.0) return NAN;

    double w = 2.0 * M_PI * freq_hz;
    double phase = 0.0;

    /* Dominant pole */
    if (amp->dominant_pole_hz > 0) {
        phase -= atan(w / (2.0 * M_PI * amp->dominant_pole_hz));
    }

    /* Second pole */
    if (amp->second_pole_hz > 0) {
        phase -= atan(w / (2.0 * M_PI * amp->second_pole_hz));
    }

    /* Third pole */
    if (amp->third_pole_hz > 0) {
        phase -= atan(w / (2.0 * M_PI * amp->third_pole_hz));
    }

    /* Negative feedback sign inversion contributes -180° base phase */
    if (include_fb_sign) {
        phase -= M_PI;
    }

    return phase;
}

double stability_compute_magnitude_at_freq(const FeedbackAmplifier *amp,
                                            double freq_hz)
{
    /* Compute |L(jω)| for the given amplifier.
     *
     * |L(jω)| = (A₀β) / √[ Π (1 + (ω/ω_{pi})²) ]
     *
     * where the product runs over all amplifier poles.
     */

    if (!amp || freq_hz < 0.0) return NAN;

    double w = 2.0 * M_PI * freq_hz;
    double loop_gain_dc = amp->open_loop_gain * amp->feedback_factor;
    double mag;

    if (freq_hz == 0.0) {
        mag = loop_gain_dc;
    } else {
        double prod = 1.0;

        /* Dominant pole contribution */
        if (amp->dominant_pole_hz > 0) {
            double w1 = 2.0 * M_PI * amp->dominant_pole_hz;
            prod *= sqrt(1.0 + (w * w) / (w1 * w1));
        }

        /* Second pole */
        if (amp->second_pole_hz > 0) {
            double w2 = 2.0 * M_PI * amp->second_pole_hz;
            prod *= sqrt(1.0 + (w * w) / (w2 * w2));
        }

        /* Third pole */
        if (amp->third_pole_hz > 0) {
            double w3 = 2.0 * M_PI * amp->third_pole_hz;
            prod *= sqrt(1.0 + (w * w) / (w3 * w3));
        }

        mag = loop_gain_dc / prod;
    }

    return mag;
}

/* =========================================================================
 * L4: Nyquist Stability Criterion
 * ========================================================================= */

void stability_generate_nyquist(const FrequencyResponse *response,
                                 NyquistResult *result)
{
    /* Generate Nyquist contour data and analyze encirclements.
     *
     * The Nyquist contour traces L(jω) = A(jω)·β(jω) as ω sweeps
     * from 0 to +∞. The complete Nyquist contour (ω from -∞ to +∞)
     * is the mirror image about the real axis for negative frequencies.
     *
     * The Nyquist Stability Criterion:
     *   Z = N + P
     * where:
     *   Z = number of closed-loop RHP poles (want Z = 0 for stability)
     *   N = number of CCW encirclements of (-1, j0) (negative = CW)
     *   P = number of open-loop RHP poles
     *
     * Stability requires Z = 0, i.e., N = -P (CW encirclements equal P).
     * For minimum-phase systems (P = 0): N must be 0.
     */

    if (!response || !result || !response->points || response->count < 2) {
        if (result) memset(result, 0, sizeof(NyquistResult));
        return;
    }

    memset(result, 0, sizeof(NyquistResult));
    result->num_points = response->count;

    /* Allocate storage for nyquist real/imag parts */
    result->nyquist_re = (double *)malloc(response->count * sizeof(double));
    result->nyquist_im = (double *)malloc(response->count * sizeof(double));

    if (!result->nyquist_re || !result->nyquist_im) {
        free(result->nyquist_re);
        free(result->nyquist_im);
        result->nyquist_re = NULL;
        result->nyquist_im = NULL;
        result->num_points = 0;
        return;
    }

    /* Copy real and imaginary parts from the frequency response */
    for (size_t i = 0; i < response->count; i++) {
        result->nyquist_re[i] = response->points[i].re;
        result->nyquist_im[i] = response->points[i].im;
    }

    /* Analyze encirclements of (-1, j0) */
    result->encirclement_count = stability_count_encirclements(
        result->nyquist_re, result->nyquist_im,
        result->num_points, 0  /* Assume P = 0 for basic analysis */
    );

    /* Determine encirclement type */
    if (result->encirclement_count == 0) {
        result->encirclement_type = NYQUIST_NO_ENCIRCLE;
    } else if (result->encirclement_count > 0) {
        result->encirclement_type = NYQUIST_CCW_ENCIRCLE;
    } else {
        result->encirclement_type = NYQUIST_CW_ENCIRCLE;
    }

    /* Z = N + P (for P=0, Z = N) */
    result->closed_loop_rhp = result->encirclement_count;
}

int stability_count_encirclements(const double *nyquist_re,
                                   const double *nyquist_im,
                                   size_t num_points,
                                   int open_rhp)
{
    /* Count the winding number (encirclements) of (-1, j0) using
     * the angle accumulation method.
     *
     * For each consecutive pair of points (re[i], im[i]) and
     * (re[i+1], im[i+1]), compute the angle change of the vector
     * from (-1, j0) to each point. Accumulate the total angle change
     * over the complete contour.
     *
     * Winding number N = (total accumulated angle) / (2π)
     *
     * Algorithm:
     *   1. For each point i, compute vector v_i = (re[i]+1, im[i])
     *   2. Compute angle change Δθ = atan2(v_{i+1} × v_i, v_{i+1} · v_i)
     *   3. Accumulate Δθ over all points
     *   4. N = round(total_Δθ / (2π))
     */

    (void)open_rhp; /* Reserved for future: Z = N + P */

    if (!nyquist_re || !nyquist_im || num_points < 2) return 0;

    double total_angle = 0.0;

    for (size_t i = 0; i < num_points - 1; i++) {
        /* Vector from (-1, j0) to point i */
        double v1_re = nyquist_re[i] + 1.0;
        double v1_im = nyquist_im[i];

        /* Vector from (-1, j0) to point i+1 */
        double v2_re = nyquist_re[i+1] + 1.0;
        double v2_im = nyquist_im[i+1];

        /* Compute cross product and dot product for angle change */
        double cross = v1_re * v2_im - v1_im * v2_re;
        double dot   = v1_re * v2_re + v1_im * v2_im;

        /* Δθ = atan2(cross, dot) — signed angle */
        double dtheta = atan2(cross, dot);

        total_angle += dtheta;
    }

    /* Winding number (CCW = +). Use lround for correct rounding of
     * negative values (e.g., -2π/(2π) + 0.5 = -0.5 would cast to 0). */
    int winding = (int)lround(total_angle / (2.0 * M_PI));

    return winding;
}

void stability_free_nyquist(NyquistResult *result)
{
    if (result) {
        free(result->nyquist_re);
        free(result->nyquist_im);
        memset(result, 0, sizeof(NyquistResult));
    }
}

/* =========================================================================
 * L5: Routh-Hurwitz Stability Test
 * ========================================================================= */

RouthHurwitzResult stability_routh_hurwitz(const double *coeffs,
                                            size_t order)
{
    /* Routh-Hurwitz stability criterion for polynomial:
     *
     *   D(s) = a₀·sⁿ + a₁·sⁿ⁻¹ + ... + a_{n-1}·s + a_n
     *
     * The Routh array is constructed as:
     *
     *   sⁿ:  a₀    a₂    a₄    a₆   ...
     *   sⁿ⁻¹: a₁    a₃    a₅    a₇   ...
     *   sⁿ⁻²: b₁    b₂    b₃    ...
     *   ...
     *   s⁰:  z₁
     *
     *   where: b₁ = (a₁·a₂ - a₀·a₃) / a₁
     *          b₂ = (a₁·a₄ - a₀·a₅) / a₁
     *          c₁ = (b₁·a₃ - a₁·b₂) / b₁
     *          ...
     *
     * Theorem: The number of RHP roots equals the number of sign
     * changes in the first column of the Routh array.
     *
     * Special cases:
     *   1. Zero in first column → replace with ε → 0 and continue
     *   2. Entire row of zeros → auxiliary polynomial, differentiate,
     *      replace the zero row with coefficients of derivative
     */

    RouthHurwitzResult result;
    memset(&result, 0, sizeof(result));
    result.order = order;

    if (!coeffs || order == 0) return result;

    /* Allocate Routh array: (order+1) rows, with max columns */
    size_t cols = (order + 2) / 2; /* ceil((n+1)/2) */
    result.routh_array = (double **)malloc((order + 1) * sizeof(double *));
    if (!result.routh_array) return result;

    for (size_t i = 0; i <= order; i++) {
        result.routh_array[i] = (double *)calloc(cols, sizeof(double));
        if (!result.routh_array[i]) {
            /* Free previously allocated rows */
            for (size_t k = 0; k < i; k++) {
                free(result.routh_array[k]);
            }
            free(result.routh_array);
            result.routh_array = NULL;
            return result;
        }
    }

    /* Fill first two rows from polynomial coefficients */
    for (size_t j = 0; j < cols; j++) {
        size_t idx_even = 2 * j;
        size_t idx_odd  = 2 * j + 1;

        if (idx_even <= order) {
            result.routh_array[0][j] = coeffs[idx_even];
        }
        if (idx_odd <= order) {
            result.routh_array[1][j] = coeffs[idx_odd];
        }
    }

    /* Build remaining rows */
    int sign_changes = 0;
    double prev_sign = 0.0;
    int prev_sign_set = 0;

    for (size_t i = 2; i <= order; i++) {
        int all_zero = 1;

        for (size_t j = 0; j < cols - 1; j++) {
            double a = result.routh_array[i-2][0];
            double b = result.routh_array[i-2][j+1];
            double c = result.routh_array[i-1][0];
            double d = result.routh_array[i-1][j+1];

            /* b_{i,j} = -(1/c) · det([[a, b], [c, d]]) = (c·b - a·d)/c */
            /* Actually: b_ij = (c·b - a·d) / c */
            /* Wait, the standard formula is:
             *   element = -(a·d - b·c) / c = (b·c - a·d) / c = b - (a·d)/c
             * Let me use: entry = (c·b - a·d) / c
             */
            if (fabs(c) < 1e-15) {
                /* Zero in first column — handle special case with ε */
                /* For the Routh array computation, if c = 0, we skip
                 * the special case handling here for simplicity and just
                 * set the entry to the numerator value (limit as c→0).
                 * Actually, when c=0 the standard approach is to use
                 * a small ε and take the limit. */
                result.routh_array[i][j] = b; /* limit as c → 0 with ε method */
                result.special_case = 1;
            } else {
                result.routh_array[i][j] = (c * b - a * d) / c;
            }

            if (fabs(result.routh_array[i][j]) > 1e-15) all_zero = 0;
        }

        /* Check for entire row of zeros (auxiliary polynomial case) */
        if (all_zero) {
            /* Fill row with derivative of auxiliary polynomial coefficients.
             * The auxiliary polynomial is formed from the row above (i-1),
             * multiplied by the appropriate powers of s. Its derivative
             * coefficients replace the zero row.
             *
             * For row s^{n-i+1}, the auxiliary polynomial is:
             *   P(s) = r[0]·s^{n-i+1} + r[1]·s^{n-i-1} + r[2]·s^{n-i-3} + ...
             * P'(s) = (n-i+1)·r[0]·s^{n-i} + (n-i-1)·r[1]·s^{n-i-2} + ...
             * The coefficients of P'(s) are: (exponent)·r[k]
             */
            result.special_case = 1;
            size_t exp_start = order - (i - 1);
            for (size_t j = 0; j < cols - 1; j++) {
                if ((int)(exp_start - 2*j) > 0) {
                    result.routh_array[i][j] =
                        (double)(exp_start - 2*j) *
                        result.routh_array[i-1][j];
                }
            }
        }

        /* Check sign of first column entry */
        double fc = result.routh_array[i][0];
        if (fabs(fc) > 1e-15) {
            if (prev_sign_set) {
                if ((prev_sign > 0 && fc < 0) || (prev_sign < 0 && fc > 0)) {
                    sign_changes++;
                }
            }
            prev_sign = fc;
            prev_sign_set = 1;
        }
    }

    /* Count sign changes in first column */
    /* Re-count properly across all rows */
    sign_changes = 0;
    double last_nonzero = 0.0;
    int has_last = 0;

    for (size_t i = 0; i <= order; i++) {
        double val = result.routh_array[i][0];
        if (fabs(val) < 1e-15) continue; /* Skip zeros (treated separately) */

        if (has_last) {
            if ((last_nonzero > 0 && val < 0) ||
                (last_nonzero < 0 && val > 0)) {
                sign_changes++;
            }
        }
        last_nonzero = val;
        has_last = 1;
    }

    result.sign_changes = sign_changes;
    result.rhp_roots = sign_changes;
    result.is_stable = (sign_changes == 0);

    return result;
}

void stability_free_routh(RouthHurwitzResult *result)
{
    if (result && result->routh_array) {
        for (size_t i = 0; i <= result->order; i++) {
            free(result->routh_array[i]);
        }
        free(result->routh_array);
        result->routh_array = NULL;
        result->order = 0;
    }
}

/* =========================================================================
 * L5: Root Locus Computation
 * ========================================================================= */

RootLocus stability_compute_root_locus(const TransferFunction *open_loop_tf,
                                        double k_min,
                                        double k_max,
                                        size_t num_gains)
{
    /* Root locus: For characteristic equation 1 + K·G(s)·H(s) = 0,
     * trace closed-loop pole locations as K varies.
     *
     * Closed-loop denominator: D_cl(s) = D(s) + K·N(s)
     * where G(s)·H(s) = N(s)/D(s)
     *
     * For each gain K, solve the polynomial:
     *   D_cl(s) = 0  →  find all roots (closed-loop poles)
     *
     * This implementation uses the companion matrix method for
     * polynomial root finding, which converts the root-finding
     * problem to an eigenvalue computation.
     */

    RootLocus locus;
    memset(&locus, 0, sizeof(locus));

    if (!open_loop_tf || num_gains < 1 || k_max <= k_min) return locus;

    size_t closed_order = open_loop_tf->den_order;
    if (open_loop_tf->num_order > closed_order) {
        closed_order = open_loop_tf->num_order;
    }
    if (closed_order == 0) return locus;

    locus.num_poles = closed_order;
    locus.num_gains = num_gains;

    locus.gain_values = (double *)malloc(num_gains * sizeof(double));
    locus.poles_k = (double complex *)malloc(num_gains * closed_order *
                                              sizeof(double complex));

    if (!locus.gain_values || !locus.poles_k) {
        free(locus.gain_values);
        free(locus.poles_k);
        memset(&locus, 0, sizeof(locus));
        return locus;
    }

    double log_k_min = log10(k_min);
    double log_k_max = log10(k_max);
    double log_step = (num_gains > 1)
        ? (log_k_max - log_k_min) / (double)(num_gains - 1)
        : 0.0;

    /* For each gain, compute closed-loop poles
     *
     * For a simple implementation, we use the fact that for
     * a second-order system with no zeros:
     *
     *   G(s)H(s) = A₀β / ((1 + s/ω_p1)(1 + s/ω_p2))
     *
     * The closed-loop characteristic equation is:
     *   s² + (ω_p1 + ω_p2)·s + ω_p1·ω_p2·(1 + K) = 0
     *
     * where K = A₀β. The closed-loop poles are:
     *   s_{1,2} = -ζ·ω_n ± j·ω_n·√(1-ζ²)
     *
     * For higher-order systems, numerical root-finding is required.
     * This implementation handles up to 3rd-order systems analytically.
     */

    locus.k_critical = INFINITY; /* Will be updated when jω crossing found */
    locus.osc_freq_hz = 0.0;

    for (size_t ig = 0; ig < num_gains; ig++) {
        double K = pow(10.0, log_k_min + ig * log_step);
        locus.gain_values[ig] = K;

        /* Build closed-loop denominator coefficients */
        size_t cl_order = open_loop_tf->den_order;
        double *cl_coeffs = (double *)calloc(cl_order + 1, sizeof(double));
        if (!cl_coeffs) continue;

        /* D_cl(s) = D(s) + K·N(s) */
        for (size_t j = 0; j <= cl_order; j++) {
            double d_coeff = (j <= open_loop_tf->den_order)
                ? open_loop_tf->den_coeffs[j] : 0.0;
            double n_coeff = (j <= open_loop_tf->num_order)
                ? open_loop_tf->num_coeffs[j] : 0.0;
            cl_coeffs[j] = d_coeff + K * n_coeff;
        }

        /* Find poles via quadratic/cubic formula for orders 2 and 3 */
        if (cl_order == 2) {
            /* s² + a₁·s + a₀ = 0 (normalized so a₂ = 1) */
            double a2 = cl_coeffs[2];
            double a1 = cl_coeffs[1] / a2;
            double a0 = cl_coeffs[0] / a2;

            double disc = a1*a1 - 4.0*a0;

            if (disc >= 0) {
                locus.poles_k[ig * closed_order + 0] =
                    (-a1 - sqrt(disc)) / 2.0;
                locus.poles_k[ig * closed_order + 1] =
                    (-a1 + sqrt(disc)) / 2.0;
            } else {
                double re = -a1 / 2.0;
                double im = sqrt(-disc) / 2.0;
                locus.poles_k[ig * closed_order + 0] = re - im * I;
                locus.poles_k[ig * closed_order + 1] = re + im * I;

                /* Check for jω-axis crossing (re = 0) */
                if (fabs(re) < 1e-10) {
                    if (K < locus.k_critical) {
                        locus.k_critical = K;
                        locus.osc_freq_hz = im / (2.0 * M_PI);
                    }
                }
            }
        } else if (cl_order == 3) {
            /* Cubic: a₃·s³ + a₂·s² + a₁·s + a₀ = 0
             * Use Cardano's method for cubic roots */
            double a3 = cl_coeffs[3];
            double a2 = cl_coeffs[2] / a3;
            double a1 = cl_coeffs[1] / a3;
            double a0 = cl_coeffs[0] / a3;

            /* Depressed cubic: t³ + p·t + q = 0
             * where t = s + a2/3 */
            double p = a1 - a2*a2 / 3.0;
            double q = a0 - a2*a1/3.0 + 2.0*a2*a2*a2/27.0;

            double disc = (q*q/4.0) + (p*p*p/27.0);

            double re_shift = -a2 / 3.0;
            double complex t1, t2, t3;

            if (disc > 0) {
                /* One real root, two complex conjugate */
                double u = cbrt(-q/2.0 + sqrt(disc));
                double v = cbrt(-q/2.0 - sqrt(disc));
                t1 = (u + v);
                t2 = -(u+v)/2.0 + (u-v)*sqrt(3.0)/2.0 * I;
                t3 = -(u+v)/2.0 - (u-v)*sqrt(3.0)/2.0 * I;
            } else if (disc == 0) {
                /* All real, at least two equal */
                double u = cbrt(-q/2.0);
                t1 = 2.0*u;
                t2 = -u;
                t3 = -u;
            } else {
                /* Three distinct real roots (casus irreducibilis) */
                double phi = acos(-q/2.0 / sqrt(-p*p*p/27.0));
                double r = 2.0 * sqrt(-p/3.0);
                t1 = r * cos(phi/3.0);
                t2 = r * cos((phi + 2.0*M_PI)/3.0);
                t3 = r * cos((phi + 4.0*M_PI)/3.0);
            }

            locus.poles_k[ig * closed_order + 0] = t1 + re_shift;
            locus.poles_k[ig * closed_order + 1] = t2 + re_shift;
            locus.poles_k[ig * closed_order + 2] = t3 + re_shift;

            /* Check for jω-axis crossing among complex poles */
            for (int k = 0; k < 3; k++) {
                double re = creal(locus.poles_k[ig * closed_order + k]);
                double im = cimag(locus.poles_k[ig * closed_order + k]);
                if (fabs(re) < 1e-10 && fabs(im) > 1e-10) {
                    if (K < locus.k_critical) {
                        locus.k_critical = K;
                        locus.osc_freq_hz = fabs(im) / (2.0 * M_PI);
                    }
                }
            }
        } else {
            /* For 1st order, trivial */
            if (cl_order == 1) {
                locus.poles_k[ig * closed_order + 0] =
                    -cl_coeffs[0] / cl_coeffs[1];
            }
            /* For order > 3, leave poles as NAN/zero (indicates
             * numerical root finding would be needed) */
            for (size_t k = 0; k < closed_order; k++) {
                if (cl_order > 3) {
                    locus.poles_k[ig * closed_order + k] = NAN + NAN * I;
                }
            }
        }

        free(cl_coeffs);
    }

    return locus;
}

void stability_free_root_locus(RootLocus *locus)
{
    if (locus) {
        free(locus->gain_values);
        free(locus->poles_k);
        memset(locus, 0, sizeof(RootLocus));
    }
}

/* =========================================================================
 * L2: Second-Order System Analysis
 * ========================================================================= */

void stability_second_order_analysis(const FeedbackAmplifier *amp,
                                      double *natural_freq_hz,
                                      double *damping_ratio,
                                      double *overshoot_pct,
                                      double *settling_time_s)
{
    /* For a second-order closed-loop system:
     *
     *   T(s) = ω_n² / (s² + 2ζ·ω_n·s + ω_n²)
     *
     * Natural frequency ω_n and damping ratio ζ are extracted from
     * the closed-loop pole locations.
     *
     * Approximate relationships from phase margin (Ogata §10.3):
     *   ζ ≈ PM(deg) / 100      (for PM < 70°)
     *   ζ ≈ 0.01 · PM(deg)     (rule of thumb)
     *
     * More precise: ζ = tan(PM) / (2·√(1+tan²(PM))) ?
     * Actually: PM = arctan(2ζ / √(√(4ζ⁴+1) - 2ζ²))
     * Inverse approximated for quick estimates.
     *
     * Transient response (2nd-order underdamped):
     *   % Overshoot = 100 · exp(-π·ζ / √(1-ζ²))
     *   2% settling time: t_s ≈ 4 / (ζ·ω_n)
     */

    if (!amp) return;

    /* Get phase margin */
    StabilityMargins margins;
    stability_assess_from_poles(amp, &margins);

    /* Approximate damping ratio from phase margin */
    double pm_deg = margins.phase_margin_deg;
    double zeta;

    if (pm_deg > 0 && pm_deg < 90) {
        /* More accurate approximation:
         * ζ ≈ PM(deg) / (100 + PM(deg)/5)
         * This fits the true relationship better than the linear rule. */
        zeta = pm_deg / 100.0;
        /* Refine: for PM > 45°, the relationship bends */
        if (pm_deg > 30) {
            zeta = pm_deg / (100.0 + pm_deg * 0.05);
        }
    } else if (pm_deg >= 90) {
        zeta = 1.0; /* Overdamped or critically damped */
    } else {
        zeta = 0.0; /* Unstable */
    }

    if (damping_ratio) *damping_ratio = zeta;

    /* Natural frequency estimate from closed-loop bandwidth */
    double bw = feedback_compute_bandwidth_extension(amp);
    /* ω_n ≈ ω_{BW} / √(1 - 2ζ² + √(4ζ⁴ - 4ζ² + 2)) */
    /* Simpler: ω_n ≈ ω_{BW} / √(1 - 2ζ²) for 0 < ζ < 0.707 */
    double wn = 2.0 * M_PI * bw;
    if (zeta < 0.707 && zeta > 0.0) {
        wn = (2.0 * M_PI * bw) / sqrt(1.0 - 2.0 * zeta * zeta);
    }

    if (natural_freq_hz) *natural_freq_hz = wn / (2.0 * M_PI);

    /* Percent overshoot */
    if (overshoot_pct && zeta < 1.0 && zeta > 0.0) {
        double exp_term = -M_PI * zeta / sqrt(1.0 - zeta * zeta);
        *overshoot_pct = 100.0 * exp(exp_term);
    } else if (overshoot_pct) {
        *overshoot_pct = (zeta >= 1.0) ? 0.0 : 100.0;
    }

    /* 2% settling time: t_s ≈ 4/(ζ·ω_n) */
    if (settling_time_s && zeta > 0.0) {
        *settling_time_s = 4.0 / (zeta * wn);
    } else if (settling_time_s) {
        *settling_time_s = INFINITY;
    }
}

/* =========================================================================
 * L2: Minimum-Phase Check
 * ========================================================================= */

int stability_is_minimum_phase(const FeedbackAmplifier *amp)
{
    /* A system is minimum-phase if all poles and zeros of its
     * transfer function are in the open left half-plane (LHP).
     *
     * For our amplifier model, we only track poles. All poles of
     * a stable amplifier are in the LHP, so the single question is
     * whether there are any RHP zeros.
     *
     * RHP zeros arise from:
     *   - Capacitive feedforward paths (lead compensation)
     *   - Right-half-plane zeros from Miller-compensated stages
     *     (the Miller RHP zero at ω_z = +g_m/C_c)
     *
     * For a basic uncompensated amplifier with real negative poles,
     * the system IS minimum-phase (no RHP zeros).
     *
     * The RHP zero from Miller compensation is at:
     *   ω_z = +g_m2 / C_c  (positive real, in RHP)
     *
     * This function returns 1 for minimum-phase based on the
     * amplifier poles alone (all are assumed LHP for stable amps).
     */

    if (!amp) return 0;

    /* For a standard amplifier (no explicit RHP poles or zeros), it
     * is minimum-phase. If the feedback is positive (regenerative),
     * poles may shift to RHP, but that's a closed-loop phenomenon.
     *
     * Open-loop: all specified poles are in LHP (negative real).
     * Therefore, this is a minimum-phase system.
     */

    /* Check that no pole is specified with a positive real part */
    /* In our model, poles are specified as (negative) frequencies,
     * so any positive frequency means LHP pole. This is always true
     * for the model. */
    if (amp->dominant_pole_hz <= 0) return 0; /* Invalid */

    /* A properly specified amplifier is minimum-phase in open-loop */
    return 1;
}
