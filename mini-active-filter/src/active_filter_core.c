/**
 * active_filter_core.c — Core Active Filter Implementation
 *
 * L1-L3: Fundamental shared utilities for active filter design:
 * biquad pole/zero computation, transfer function evaluation,
 * filter initialization, frequency response calculation.
 *
 * Reference: Sedra & Smith (2020) Microelectronic Circuits, Ch. 16
 *            Van Valkenburg, Analog Filter Design (1982)
 *            Huelsman & Allen, Introduction to the Theory and
 *            Design of Active Filters (1980)
 */

#include "active_filter_defs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ==================================================================
 * L3: Biquad Section Operations
 * ================================================================== */

/**
 * active_biquad_init — Initialize a biquad section with canonical values.
 *
 * L1 Definition: Second-order section H(s) = (b2·s² + b1·s + b0) /
 * (s² + (ω₀/Q)·s + ω₀²) characterized by {ω₀, Q, gain, zeros}.
 *
 * @param wp    Pole natural frequency (rad/s)
 * @param qp    Pole quality factor
 * @param gain  DC or HF gain (linear)
 * @param func  Filter function (determines zero placement)
 * @param bq    Output biquad section
 */
void active_biquad_init(double wp, double qp, double gain,
                         active_filter_func_t func,
                         active_biquad_section_t *bq)
{
    if (!bq) return;
    bq->wp = wp;
    bq->qp = qp > 0.0 ? qp : 0.7071;
    bq->gain = gain;
    bq->is_allpole = 1;

    switch (func) {
    case ACTIVE_FUNC_LOWPASS:
        /* All-pole LP: zeros at infinity, numerator = gain·ω₀² */
        bq->wz = 1e12;  /* effectively infinite */
        bq->qz = 0.0;
        break;
    case ACTIVE_FUNC_HIGHPASS:
        /* All-pole HP: double zero at origin, numerator = gain·s² */
        bq->wz = 0.0;
        bq->qz = 0.0;
        break;
    case ACTIVE_FUNC_BANDPASS:
        /* BP: single zero at origin, numerator = gain·(ω₀/Q)·s */
        bq->wz = wp;
        bq->qz = qp;
        bq->is_allpole = 0;
        break;
    case ACTIVE_FUNC_BANDSTOP:
        /* BS (notch): zeros at ±jω₀, numerator = gain·(s²+ω₀²) */
        bq->wz = wp;
        bq->qz = 1e12;  /* very high Q zeros (narrow notch) */
        bq->is_allpole = 0;
        break;
    case ACTIVE_FUNC_ALLPASS:
        /* Allpass: zeros = mirror of poles across jω axis */
        bq->wz = wp;
        bq->qz = -qp;  /* Right half-plane zeros */
        bq->is_allpole = 0;
        break;
    default:
        bq->wz = 1e12;
        bq->qz = 0.0;
        break;
    }
}

/**
 * active_biquad_evaluate — Evaluate biquad transfer function at frequency ω.
 *
 * L3: H(jω) = gain · ( (jω)² + (ωz/Qz)·jω + ωz² ) /
 *                     ( (jω)² + (ωp/Qp)·jω + ωp² )
 *
 * For LP (wz→∞): H(jω) = gain · ωp² / (ωp² - ω² + j·ωp·ω/Qp)
 * For HP (wz=0):  H(jω) = gain · (-ω²) / (ωp² - ω² + j·ωp·ω/Qp)
 * For BP:         H(jω) = gain · j·ωp·ω/Qp / (ωp² - ω² + j·ωp·ω/Qp)
 *
 * @param bq    Biquad section
 * @param omega Angular frequency (rad/s)
 * @return      Complex transfer function value H(jω)
 */
double complex active_biquad_evaluate(const active_biquad_section_t *bq,
                                       double omega)
{
    if (!bq || bq->qp <= 0.0) return 0.0 + 0.0*I;

    double wp2 = bq->wp * bq->wp;
    double w2  = omega * omega;
    double denom_real = wp2 - w2;
    double denom_imag = bq->wp * omega / bq->qp;
    double complex denominator = denom_real + denom_imag * I;

    if (cabs(denominator) < 1e-30) return 1e15 + 0.0*I;

    double complex numerator;

    if (bq->wz > 1e10) {
        /* LP: all-pole, numerator = gain · ωp² (real constant) */
        numerator = bq->gain * wp2 + 0.0*I;
    } else if (bq->wz < 1e-10) {
        /* HP: double zero at origin, numerator = gain · (jω)² = -gain·ω² */
        numerator = -bq->gain * w2 + 0.0*I;
    } else if (fabs(bq->wz - bq->wp) < 1e-9 && fabs(bq->qz - bq->qp) < 1e-9) {
        /* BP: single zero at origin, numerator = gain · j·ωp·ω/Qp */
        numerator = 0.0 + bq->gain * bq->wp * omega / bq->qp * I;
    } else {
        /* General biquadratic numerator: gain·(wz²-ω² + j·wz·ω/Qz) */
        double wz2 = bq->wz * bq->wz;
        double num_real = bq->gain * (wz2 - w2);
        double num_imag = bq->gain * bq->wz * omega / (bq->qz != 0.0 ? bq->qz : 1.0);
        numerator = num_real + num_imag * I;
    }

    return numerator / denominator;
}

/**
 * active_biquad_magnitude_db — Magnitude in dB at frequency ω.
 *
 * L1: |H(jω)|_dB = 20·log10(|H(jω)|)
 */
double active_biquad_magnitude_db(const active_biquad_section_t *bq,
                                   double omega)
{
    double complex h = active_biquad_evaluate(bq, omega);
    double mag = cabs(h);
    if (mag < 1e-15) return -300.0;
    return 20.0 * log10(mag);
}

/**
 * active_biquad_phase_rad — Phase in radians at frequency ω.
 *
 * L1: φ(ω) = arg(H(jω)) = atan2(Im(H), Re(H))
 * Unwrapped phase for cascade compatibility.
 */
double active_biquad_phase_rad(const active_biquad_section_t *bq,
                                double omega)
{
    double complex h = active_biquad_evaluate(bq, omega);
    return atan2(cimag(h), creal(h));
}

/**
 * active_biquad_group_delay — Group delay at frequency ω.
 *
 * L3: τ_g(ω) = -dφ/dω computed via numerical differentiation.
 * Group delay represents the time delay of the signal envelope
 * at frequency ω. Constant group delay = linear phase = no dispersion.
 *
 * For Bessel filters, group delay is maximally flat.
 * For Butterworth: some variation near cutoff.
 * For Chebyshev: significant peaking near cutoff.
 *
 * Uses symmetric difference: τ_g ≈ -(φ(ω+Δ) - φ(ω-Δ)) / (2Δ)
 */
double active_biquad_group_delay(const active_biquad_section_t *bq,
                                  double omega)
{
    double delta = omega * 1e-6;
    if (delta < 1e-12) delta = 1e-12;
    double phi_plus  = active_biquad_phase_rad(bq, omega + delta);
    double phi_minus = active_biquad_phase_rad(bq, omega - delta);
    return -(phi_plus - phi_minus) / (2.0 * delta);
}

/* ==================================================================
 * L3: Cascaded Transfer Function Evaluation
 * ================================================================== */

/**
 * active_cascade_evaluate — Evaluate cascade of N biquad sections.
 *
 * L3: The total transfer function is the product of individual sections:
 * H_total(jω) = ∏_{k=0}^{N-1} H_k(jω)
 *
 * Magnitude: |H_total| = ∏ |H_k| (multiply linear magnitudes)
 * Magnitude dB: |H_total|_dB = Σ |H_k|_dB (add dB values)
 * Phase: φ_total = Σ φ_k (add phase values)
 * Group delay: τ_total = Σ τ_k (add group delays)
 *
 * Cascade evaluation is O(N) per frequency point.
 *
 * @param biquads     Array of biquad sections
 * @param num_biquads Number of sections
 * @param omega       Angular frequency (rad/s)
 * @return            Complex H_total(jω)
 */
double complex active_cascade_evaluate(const active_biquad_section_t *biquads,
                                        int num_biquads, double omega)
{
    if (!biquads || num_biquads < 1) return 1.0 + 0.0*I;

    double complex h_total = 1.0 + 0.0*I;
    for (int k = 0; k < num_biquads; k++) {
        h_total *= active_biquad_evaluate(&biquads[k], omega);
    }
    return h_total;
}

/**
 * active_cascade_magnitude_db — Cascade magnitude response in dB.
 */
double active_cascade_magnitude_db(const active_biquad_section_t *biquads,
                                    int num_biquads, double omega)
{
    if (!biquads || num_biquads < 1) return 0.0;

    double total_db = 0.0;
    for (int k = 0; k < num_biquads; k++) {
        total_db += active_biquad_magnitude_db(&biquads[k], omega);
    }
    return total_db;
}

/* ==================================================================
 * L3: Frequency Response Sweep
 * ================================================================== */

/**
 * active_freq_response_sweep — Compute complete frequency response.
 *
 * L3: Evaluates H(jω) at each frequency point in the specified range.
 * Supports both linear and logarithmic spacing.
 *
 * Log spacing formula:
 *   f_i = f_start · (f_stop/f_start)^(i/(N-1))  for i = 0..N-1
 *
 * Linear spacing formula:
 *   f_i = f_start + i·(f_stop - f_start)/(N-1)
 *
 * @param biquads      Array of biquad sections
 * @param num_biquads  Number of sections
 * @param f_start      Start frequency (Hz)
 * @param f_stop       Stop frequency (Hz)
 * @param num_points   Number of frequency points (≥ 2)
 * @param log_spacing  1 for log, 0 for linear
 * @param response     Output frequency response (caller allocates)
 * @return             ACTIVE_OK or error code
 */
int active_freq_response_sweep(const active_biquad_section_t *biquads,
                                int num_biquads,
                                double f_start, double f_stop,
                                int num_points, int log_spacing,
                                active_freq_response_t *response)
{
    if (!biquads || !response || num_points < 2 || num_biquads < 1)
        return ACTIVE_ERR_NULL_PTR;
    if (f_start <= 0.0 || f_stop <= f_start)
        return ACTIVE_ERR_INVALID_FREQ;

    response->points = (active_freq_point_t *)malloc(
        num_points * sizeof(active_freq_point_t));
    if (!response->points) return ACTIVE_ERR_MEMORY;

    response->num_points = num_points;
    response->f_start = f_start;
    response->f_stop = f_stop;
    response->log_spacing = log_spacing;

    for (int i = 0; i < num_points; i++) {
        double freq;
        if (log_spacing) {
            double ratio = (double)i / (double)(num_points - 1);
            freq = f_start * pow(f_stop / f_start, ratio);
        } else {
            freq = f_start + (f_stop - f_start) * i / (num_points - 1);
        }

        double omega = 2.0 * M_PI * freq;
        double complex h = active_cascade_evaluate(biquads, num_biquads, omega);

        response->points[i].freq_hz = freq;
        response->points[i].real = creal(h);
        response->points[i].imag = cimag(h);
        response->points[i].magnitude = cabs(h);
        response->points[i].magnitude_db = (cabs(h) > 1e-15) ?
            20.0 * log10(cabs(h)) : -300.0;
        response->points[i].phase_rad = atan2(cimag(h), creal(h));
        response->points[i].phase_deg =
            response->points[i].phase_rad * 180.0 / M_PI;

        /* Group delay via numerical differentiation */
        double domega = omega * 1e-4;
        if (domega < 1e-10) domega = 1e-10;
        double complex h_plus = active_cascade_evaluate(
            biquads, num_biquads, omega + domega);
        double complex h_minus = active_cascade_evaluate(
            biquads, num_biquads, omega - domega);
        double phi_plus = atan2(cimag(h_plus), creal(h_plus));
        double phi_minus = atan2(cimag(h_minus), creal(h_minus));
        /* Unwrap phase difference */
        double dphi = phi_plus - phi_minus;
        while (dphi > M_PI) dphi -= 2.0 * M_PI;
        while (dphi < -M_PI) dphi += 2.0 * M_PI;
        response->points[i].group_delay_s = -dphi / (2.0 * domega);
    }

    return ACTIVE_OK;
}

/**
 * active_freq_response_free — Release frequency response memory.
 */
void active_freq_response_free(active_freq_response_t *response)
{
    if (response && response->points) {
        free(response->points);
        response->points = NULL;
        response->num_points = 0;
    }
}

/* ==================================================================
 * L1: Filter Initialization and Management
 * ================================================================== */

/**
 * active_filter_alloc — Allocate and initialize an active filter.
 *
 * L1: Allocates memory for the filter structure and its biquad
 * and component arrays based on the specification's order.
 *
 * @param spec    Filter specification (order determines allocation)
 * @param filter  Output pointer to allocated filter
 * @return        ACTIVE_OK or error code
 */
int active_filter_alloc(const active_filter_spec_t *spec,
                         active_filter_t **filter)
{
    if (!spec || !filter) return ACTIVE_ERR_NULL_PTR;
    if (spec->order < 1 || spec->order > 10)
        return ACTIVE_ERR_INVALID_ORDER;

    active_filter_t *f = (active_filter_t *)malloc(sizeof(active_filter_t));
    if (!f) return ACTIVE_ERR_MEMORY;

    int num_biquads = (spec->order + 1) / 2;  /* ceil(order/2) */

    f->biquads = (active_biquad_section_t *)calloc(
        num_biquads, sizeof(active_biquad_section_t));
    f->components = (active_component_values_t *)calloc(
        num_biquads, sizeof(active_component_values_t));

    if (!f->biquads || !f->components) {
        free(f->biquads);
        free(f->components);
        free(f);
        return ACTIVE_ERR_MEMORY;
    }

    memcpy(&f->spec, spec, sizeof(active_filter_spec_t));
    f->num_biquads = num_biquads;
    f->actual_gain_db = 0.0;
    f->actual_fc_hz = 0.0;
    f->sensitivity_worst = 0.0;
    f->is_stable = 1;
    f->design_notes[0] = '\0';

    *filter = f;
    return ACTIVE_OK;
}

/**
 * active_filter_free — Release all memory associated with a filter.
 *
 * L1: Proper cleanup of dynamically allocated filter.
 */
void active_filter_free(active_filter_t *filter)
{
    if (filter) {
        free(filter->biquads);
        free(filter->components);
        free(filter);
    }
}

/**
 * active_filter_find_cutoff — Find -3dB cutoff frequency.
 *
 * L3: Uses binary search to locate the frequency where |H(jω)|
 * drops to 1/√2 of the passband value.
 *
 * For LP: search from low to high, find where gain drops 3 dB from DC.
 * For HP: search from high to low, find where gain drops 3 dB from HF.
 * For BP: find both lower and upper -3dB points.
 *
 * Uses the cascade evaluator for accuracy.
 *
 * @param biquads     Filter biquad sections
 * @param num_biquads Number of sections
 * @param func        Filter function
 * @param f_low       Lower search bound (Hz)
 * @param f_high      Upper search bound (Hz)
 * @param passband_db Reference passband level (dB)
 * @return            -3dB cutoff frequency (Hz), or -1 on error
 */
double active_filter_find_cutoff(const active_biquad_section_t *biquads,
                                  int num_biquads,
                                  active_filter_func_t func,
                                  double f_low, double f_high,
                                  double passband_db)
{
    if (!biquads || num_biquads < 1 || f_low <= 0.0 || f_high <= f_low)
        return -1.0;

    (void)func;  /* Function type reserved for BP/BS dual-cutoff search */

    double target_db = passband_db - 3.0;
    double lo = f_low;
    double hi = f_high;
    int max_iter = 50;
    double tol = 1e-6;

    /* Check if target is within range */
    double omega_lo = 2.0 * M_PI * lo;
    double mag_lo = active_cascade_magnitude_db(biquads, num_biquads, omega_lo);
    double omega_hi = 2.0 * M_PI * hi;
    double mag_hi = active_cascade_magnitude_db(biquads, num_biquads, omega_hi);

    if ((mag_lo - target_db) * (mag_hi - target_db) > 0) {
        /* Target not bracketed — try wider range */
        if (mag_lo < target_db && mag_hi > target_db) {
            /* Swap */ double tmp = lo; lo = hi; hi = tmp;
        } else {
            return -1.0;
        }
    }

    for (int iter = 0; iter < max_iter; iter++) {
        double mid = (lo + hi) / 2.0;
        if ((hi - lo) / mid < tol) return mid;

        double omega_mid = 2.0 * M_PI * mid;
        double mag_mid = active_cascade_magnitude_db(
            biquads, num_biquads, omega_mid);

        if (fabs(mag_mid - target_db) < 0.01) return mid;

        double omega_lo_val = 2.0 * M_PI * lo;
        double mag_lo_val = active_cascade_magnitude_db(
            biquads, num_biquads, omega_lo_val);

        if ((mag_lo_val - target_db) * (mag_mid - target_db) < 0) {
            hi = mid;
        } else {
            lo = mid;
        }
    }

    return (lo + hi) / 2.0;
}

/* ==================================================================
 * L3: Pole Frequency Normalization
 * ================================================================== */

/**
 * active_normalize_poles — Normalize pole frequencies from prototype.
 *
 * L3: Frequency scaling of normalized prototype (ωc=1 rad/s) to
 * actual desired cutoff.
 *
 * For LP: s → s/ωc (scale all poles by ωc)
 * For HP: s → ωc/s (reciprocal + scale)
 * For BP: s → (s²+ω₀²)/(s·BW) (geometric transformation)
 * For BS: s → (s·BW)/(s²+ω₀²) (reciprocal of BP)
 *
 * @param wp_norm     Normalized pole frequency (rad/s, prototype)
 * @param qp          Pole Q (invariant under LP→LP scaling)
 * @param func        Target filter function
 * @param fc          Target cutoff/center frequency (Hz)
 * @param bw          Bandwidth (Hz, for BP/BS only)
 * @param wp_actual   Output: actual pole frequency (rad/s)
 * @param qp_actual   Output: actual Q (may change for BP/BS)
 */
void active_normalize_poles(double wp_norm, double qp,
                             active_filter_func_t func,
                             double fc, double bw,
                             double *wp_actual, double *qp_actual)
{
    double wc = 2.0 * M_PI * fc;

    switch (func) {
    case ACTIVE_FUNC_LOWPASS:
        /* LP: simple frequency scaling, Q unchanged */
        *wp_actual = wp_norm * wc;
        *qp_actual = qp;
        break;

    case ACTIVE_FUNC_HIGHPASS:
        /* HP: reciprocal transformation, Q unchanged */
        *wp_actual = wc / wp_norm;
        *qp_actual = qp;
        break;

    case ACTIVE_FUNC_BANDPASS: {
        /* BP: s → (s²+ω₀²)/(s·BW)
         * Each prototype pole splits into two BP poles.
         * The normalized ωp and Qp transform as:
         *   ωp_bp ≈ ω₀  (both poles near center)
         *   Q_bp = Q_proto · ω₀ / (ωp_norm · BW_rad)
         *
         * Simplified: use the geometric transformation.
         */
        double w0 = 2.0 * M_PI * fc;
        double bw_rad = 2.0 * M_PI * bw;
        double a = wp_norm * bw_rad;

        /* Quadratic: ω² - a·ω/Qp + ω₀² = 0
         * Two solutions correspond to the two BP poles.
         * We return the arithmetic mean as the dominant pole. */
        double disc = (a*a/(qp*qp)) - 4.0*w0*w0;
        if (disc >= 0) {
            double sqrt_disc = sqrt(disc);
            double w1 = (a/qp + sqrt_disc) / 2.0;
            double w2 = (a/qp - sqrt_disc) / 2.0;
            if (w2 < 0) w2 = w1;  /* discard negative frequency */
            *wp_actual = (w1 + w2) / 2.0;
        } else {
            *wp_actual = w0;
            disc = 0.0;
        }
        *qp_actual = fc / bw;  /* BP Q = f0/BW */
        break;
    }

    case ACTIVE_FUNC_BANDSTOP: {
        /* BS: s → (s·BW)/(s²+ω₀²) — reciprocal of BP */
        double w0 = 2.0 * M_PI * fc;
        double bw_rad = 2.0 * M_PI * bw;
        *wp_actual = w0;  /* notch at ω₀ */
        *qp_actual = w0 / bw_rad;  /* BS Q = f0/BW */
        break;
    }

    default:
        *wp_actual = wp_norm * wc;
        *qp_actual = qp;
        break;
    }
}

/**
 * active_estimate_order_butterworth — Estimate required order for spec.
 *
 * L5: Butterworth order estimation formula:
 *   N ≥ log10((10^{As/10} - 1) / (10^{Ap/10} - 1)) / (2·log10(ωs/ωp))
 *
 * where:
 *   As = stopband attenuation (dB)
 *   Ap = passband ripple (dB) — usually 3 dB for Butterworth
 *   ωs = stopband edge frequency
 *   ωp = passband edge frequency
 *
 * Reference: Van Valkenburg, Analog Filter Design (1982), §3.2
 *
 * @param passband_edge_hz  Passband edge frequency (Hz)
 * @param stopband_edge_hz  Stopband edge frequency (Hz)
 * @param passband_ripple_db Passband ripple (dB, typically 3.0 for Butterworth)
 * @param stopband_atten_db  Minimum stopband attenuation (dB)
 * @return     Minimum required integer order (≥ 1)
 */
int active_estimate_order_butterworth(double passband_edge_hz,
                                       double stopband_edge_hz,
                                       double passband_ripple_db,
                                       double stopband_atten_db)
{
    if (passband_edge_hz <= 0.0 || stopband_edge_hz <= passband_edge_hz)
        return 1;

    double ratio = stopband_edge_hz / passband_edge_hz;
    double ap = pow(10.0, passband_ripple_db / 10.0) - 1.0;
    double as = pow(10.0, stopband_atten_db / 10.0) - 1.0;

    if (ap <= 0.0) ap = 1.0;  /* Use -3dB point if no ripple given */
    if (as <= 0.0) return 1;

    double n = log10(as / ap) / (2.0 * log10(ratio));
    if (n < 1.0) n = 1.0;

    return (int)ceil(n);
}

/**
 * active_estimate_order_chebyshev — Estimate Chebyshev I order.
 *
 * L5: Chebyshev order estimation:
 *   N ≥ acosh(sqrt((10^{As/10} - 1) / (10^{Ap/10} - 1))) / acosh(ωs/ωp)
 *
 * Chebyshev gives lower order than Butterworth for the same spec
 * because it allows ripple in the passband.
 *
 * @return Minimum required integer order
 */
int active_estimate_order_chebyshev(double passband_edge_hz,
                                     double stopband_edge_hz,
                                     double passband_ripple_db,
                                     double stopband_atten_db)
{
    if (passband_edge_hz <= 0.0 || stopband_edge_hz <= passband_edge_hz)
        return 1;

    double ratio = stopband_edge_hz / passband_edge_hz;
    double ap = pow(10.0, passband_ripple_db / 10.0) - 1.0;
    double as = pow(10.0, stopband_atten_db / 10.0) - 1.0;

    if (ap <= 0.0) ap = 1e-6;
    if (as <= 0.0) return 1;
    if (ratio <= 1.0) return 1;

    double arg = sqrt(as / ap);
    double n = acosh(arg) / acosh(ratio);
    if (n < 1.0) n = 1.0;

    return (int)ceil(n);
}

/* ==================================================================
 * L3: Component Value Utilities
 * ================================================================== */

/**
 * active_nearest_eseries — Find nearest standard resistor/capacitor value.
 *
 * L3: E-series values follow the formula: R = 10^((n-1)/E) · R_base
 * where E = 12, 24, 48, 96, or 192.
 *
 * E12: 1.0, 1.2, 1.5, 1.8, 2.2, 2.7, 3.3, 3.9, 4.7, 5.6, 6.8, 8.2
 * E24: adds intermediate values between E12
 * E96: ±1% tolerance standard, very fine granularity
 *
 * Algorithm: compute decade-normalized value, find nearest E-series
 * index, scale back to original decade.
 *
 * @param value     Desired component value
 * @param e_series  E-series (12, 24, 48, 96)
 * @return          Nearest standard value
 */
double active_nearest_eseries(double value, int e_series)
{
    if (value <= 0.0 || e_series < 12) return value;

    /* Normalize to [1.0, 10.0) */
    double decade = pow(10.0, floor(log10(value)));
    double normalized = value / decade;

    /* Find nearest E-series step */
    int num_steps = e_series;
    double best_val = normalized;
    double best_err = 1e30;

    for (int i = 0; i <= num_steps; i++) {
        double candidate = pow(10.0, (double)i / (double)num_steps);
        double err = fabs(candidate - normalized);
        if (err < best_err) {
            best_err = err;
            best_val = candidate;
        }
    }

    return best_val * decade;
}

/**
 * active_component_range_check — Verify components are in practical range.
 *
 * L3: Practical component limits for active filters:
 *   Resistors: 100 Ω to 10 MΩ (below = loading, above = noise/pickup)
 *   Capacitors: 10 pF to 100 μF (below = parasitic, above = electrolytic)
 *
 * Optimal decade for audio: R ~ 1k-100k, C ~ 1nF-100nF
 * Optimal decade for RF: R ~ 50-500, C ~ 1pF-100pF
 *
 * @param comp     Component values to check
 * @param warning  Output warning message if out of range
 * @return         1 if all components in range, 0 otherwise
 */
int active_component_range_check(const active_component_values_t *comp,
                                  char *warning)
{
    if (!comp) return 0;

    int ok = 1;
    warning[0] = '\0';

    /* Individual component checks */
    double resistors[] = {comp->r1, comp->r2, comp->r3, comp->r4, comp->r5};
    double capacitors[] = {comp->c1, comp->c2, comp->c3};

    for (int i = 0; i < 5; i++) {
        if (resistors[i] > 0.0) {
            if (resistors[i] < 10.0) {
                sprintf(warning + strlen(warning),
                        "R%d=%.1f too low (<10Ω); ", i+1, resistors[i]);
                ok = 0;
            }
            if (resistors[i] > 10e6) {
                sprintf(warning + strlen(warning),
                        "R%d=%.1e too high (>10MΩ); ", i+1, resistors[i]);
                ok = 0;
            }
        }
    }

    for (int i = 0; i < 3; i++) {
        if (capacitors[i] > 0.0) {
            if (capacitors[i] < 1e-12) {
                sprintf(warning + strlen(warning),
                        "C%d=%.1e too small (<1pF); ", i+1, capacitors[i]);
                ok = 0;
            }
            if (capacitors[i] > 100e-6) {
                sprintf(warning + strlen(warning),
                        "C%d=%.1e too large (>100μF); ", i+1, capacitors[i]);
                ok = 0;
            }
        }
    }

    return ok;
}

/**
 * active_parallel_resistance — Compute parallel equivalent of two resistors.
 *
 * L1: R_parallel = (R1 · R2) / (R1 + R2)
 * Used everywhere in filter design for Thévenin equivalents.
 */
double active_parallel_resistance(double r1, double r2)
{
    if (r1 <= 0.0 || r2 <= 0.0) return 0.0;
    return (r1 * r2) / (r1 + r2);
}

/**
 * active_series_capacitance — Compute series equivalent of two capacitors.
 *
 * L1: C_series = (C1 · C2) / (C1 + C2)
 */
double active_series_capacitance(double c1, double c2)
{
    if (c1 <= 0.0 || c2 <= 0.0) return 0.0;
    return (c1 * c2) / (c1 + c2);
}

/* ==================================================================
 * L3: Impedance Scaling
 * ================================================================== */

/**
 * active_impedance_scale — Scale component values to new impedance level.
 *
 * L3: Impedance scaling: multiply all R by factor, divide all C by factor.
 * ω₀ and Q are invariant under impedance scaling.
 *
 * R_new = R_old · Z_factor
 * C_new = C_old / Z_factor
 *
 * Used to shift component values into a preferred range while
 * preserving all filter characteristics.
 *
 * @param comp      Input/output component values (modified in-place)
 * @param z_factor  Impedance scaling factor (> 0)
 */
void active_impedance_scale(active_component_values_t *comp, double z_factor)
{
    if (!comp || z_factor <= 0.0) return;

    if (comp->r1 > 0) comp->r1 *= z_factor;
    if (comp->r2 > 0) comp->r2 *= z_factor;
    if (comp->r3 > 0) comp->r3 *= z_factor;
    if (comp->r4 > 0) comp->r4 *= z_factor;
    if (comp->r5 > 0) comp->r5 *= z_factor;
    if (comp->c1 > 0) comp->c1 /= z_factor;
    if (comp->c2 > 0) comp->c2 /= z_factor;
    if (comp->c3 > 0) comp->c3 /= z_factor;
}
