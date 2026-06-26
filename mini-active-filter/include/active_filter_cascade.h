/**
 * active_filter_cascade.h — Cascade Design & High-Order Filter Realization
 *
 * L2 Core Concepts: High-order active filters realized as a cascade
 * of second-order (biquad) sections plus one optional first-order
 * section for odd-order filters.
 *
 * Key principles:
 * - Pole-zero pairing: optimal assignment of pole pairs to biquad stages
 *   to minimize dynamic range problems and noise
 * - Section ordering: sequence of biquad stages to avoid internal clipping
 *   (lowest-Q first for LP, highest-Q first for BP)
 * - Gain distribution: distribute total gain across stages to maximize
 *   dynamic range (equal output swing per stage)
 * - Frequency scaling: each biquad is independently tunable
 *
 * Courses: MIT 6.002, Berkeley EE105, Stanford EE101B
 * Reference: Laker & Sansen, Design of Analog Integrated Circuits
 *            and Systems (1994), Ch. 5
 *            Sedra & Smith (2020), §16.6-16.7
 */

#ifndef ACTIVE_FILTER_CASCADE_H
#define ACTIVE_FILTER_CASCADE_H

#include "active_filter_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================
 * L2: Pole-Zero Pairing and Section Ordering
 * ================================================================== */

/** Compute Butterworth poles for order N (for cascade decomposition).
 *  L3: Butterworth poles lie on a circle in the left half-plane.
 *  For even N: N/2 complex-conjugate pairs.
 *  For odd N: (N-1)/2 complex pairs + 1 real pole.
 *
 *  @param order  Filter order (1..10)
 *  @param wp     Array of pole frequencies ωₚ (normalized, rad/s)
 *  @param qp     Array of pole Q factors (normalized)
 *  @param max_pairs Max array size (>= ceil(order/2))
 *  @return       Number of biquad sections (ceil(order/2))
 *  Complexity: O(N) */
int cascade_butterworth_poles(int order, double *wp, double *qp, int max_pairs);

/** Compute Chebyshev I poles for cascade decomposition.
 *  L3: Chebyshev poles lie on an ellipse.
 *  All pole pairs have different Q values — the pairing matters.
 *
 *  @param order      Filter order (1..10)
 *  @param ripple_db  Passband ripple (dB)
 *  @param wp         Pole frequencies (rad/s, normalized)
 *  @param qp         Pole Q factors
 *  @param max_pairs  Max array size
 *  @return           Number of biquad sections
 *  Complexity: O(N) */
int cascade_chebyshev_poles(int order, double ripple_db,
                             double *wp, double *qp, int max_pairs);

/** Compute Bessel poles for cascade decomposition.
 *  L3: Bessel poles are the roots of reverse Bessel polynomials.
 *  Unlike Butterworth, pole frequencies are not all at ωc.
 *  Q values are generally lower than Butterworth (better phase).
 *
 *  @param order      Filter order (1..10)
 *  @param wp         Pole frequencies (rad/s, normalized)
 *  @param qp         Pole Q factors
 *  @param max_pairs  Max array size
 *  @return           Number of biquad sections
 *  Complexity: O(N²) due to polynomial root-finding */
int cascade_bessel_poles(int order, double *wp, double *qp, int max_pairs);

/** Optimal pole-zero pairing for cascade design.
 *  L5: This is the critical step in cascaded filter design.
 *
 *  Strategy (minimizes internal dynamic range problems):
 *  1. Pair the highest-Q pole with the nearest transmission zero
 *  2. Continue in descending Q order
 *  3. This minimizes the peak gain of each biquad stage
 *
 *  For all-pole filters (Butterworth, Chebyshev), no zeros to pair —
 *  we just order sections appropriately.
 *
 *  @param wp         Pole frequencies array
 *  @param qp         Pole Q factors array
 *  @param num_pairs  Number of biquad pairs
 *  @param pairing    Output: pair assignment [pair_idx][2] = {wp_idx, qp_idx}
 *  Complexity: O(N log N) — sorting by Q */
int cascade_optimal_pairing(const double *wp, const double *qp,
                             int num_pairs, int *pairing);

/** Determine optimal section ordering for cascade.
 *  L5: Section ordering affects dynamic range and noise performance.
 *
 *  Rules of thumb:
 *    LP filter: ascending Q (lowest Q first) → minimizes noise
 *    BP filter: descending Q (highest Q first) → minimizes clipping
 *    General:   lowest peak gain per section first
 *
 *  Also applies to gain distribution: sections with highest Q
 *  should have lower gain to prevent peaking-induced clipping.
 *
 *  @param qp          Pole Q factors
 *  @param num_pairs   Number of sections
 *  @param ordering    Output: section order indices
 *  Complexity: O(N log N) */
int cascade_optimal_ordering(const double *qp, int num_pairs,
                              int *ordering);

/* ==================================================================
 * L5: Gain Distribution Across Cascade
 * ================================================================== */

/** Distribute total gain across cascaded sections.
 *  L5: Optimal gain distribution maximizes dynamic range by
 *  equalizing the peak output swing of each stage.
 *
 *  For LP: flat passband → equal gain per stage works.
 *  For BP/notch: sections near ω₀ have higher peak response
 *  → should receive lower gain to prevent internal clipping.
 *
 *  Algorithm: Assign gain proportional to 1/(peak_response)
 *  Peak response ≈ Q for BP sections, ≈ 1 for LP.
 *
 *  @param total_gain   Total cascaded gain (linear)
 *  @param qp           Q factors of each section
 *  @param num_pairs    Number of sections
 *  @param per_section  Output: gain per section (linear)
 *  Complexity: O(N) */
int cascade_distribute_gain(double total_gain, const double *qp,
                             int num_pairs, double *per_section);

/* ==================================================================
 * L6: Complete Cascade Design
 * ================================================================== */

/** Complete cascade filter design — high-order active filter.
 *  L6: Top-level function that designs a complete high-order
 *  active filter from specification.
 *
 *  Workflow:
 *  1. Compute normalized poles for specified approximation
 *  2. Decompose into biquad sections
 *  3. Assign topology to each biquad (user-specified or auto)
 *  4. Pair poles optimally
 *  5. Order sections for best dynamic range
 *  6. Distribute gain across sections
 *  7. Compute component values for each biquad
 *  8. Analyze overall transfer function
 *
 *  @param spec    Complete filter specification
 *  @param filter  Output: fully designed filter with components
 *  @return        ACTIVE_OK or error code
 *  Complexity: O(N²) for N-order filter */
int cascade_filter_design(const active_filter_spec_t *spec,
                           active_filter_t *filter);

/** Evaluate overall cascade transfer function at a frequency.
 *  L3: Cascaded TF = product of individual biquad TFs.
 *  H_total(jω) = ∏_k H_k(jω) = ∏_k K_k·N_k(jω)/D_k(jω)
 *
 *  @param filter   Designed cascade filter
 *  @param freq_hz  Evaluation frequency (Hz)
 *  @param point    Output frequency response point
 *  Complexity: O(N_sections) */
int cascade_evaluate_response(const active_filter_t *filter,
                               double freq_hz, active_freq_point_t *point);

/** Compute complete frequency response sweep for a cascade filter.
 *  L3: Evaluates magnitude and phase across a frequency range.
 *  Useful for generating Bode plot data.
 *
 *  @param filter      Designed cascade filter
 *  @param f_start     Start frequency (Hz)
 *  @param f_stop      Stop frequency (Hz)
 *  @param num_points  Number of points (≥ 10)
 *  @param log_spacing 1 for logarithmic spacing, 0 for linear
 *  @param response    Output: frequency response vector
 *  Complexity: O(N_sections · N_points) */
int cascade_frequency_response(const active_filter_t *filter,
                                double f_start, double f_stop,
                                int num_points, int log_spacing,
                                active_freq_response_t *response);

/** Verify cascade filter against specification.
 *  L6: Checks that the designed filter meets all spec requirements:
 *  - Cutoff frequency within tolerance
 *  - Passband ripple within spec
 *  - Stopband attenuation meets minimum
 *  - All stages stable
 *
 *  @param filter    Designed filter
 *  @param tolerance Fractional tolerance (e.g., 0.05 for 5%)
 *  @param report    Output: verification report (character buffer, 512 bytes)
 *  @return          1 if passes, 0 if fails
 *  Complexity: O(N_points · N_sections) */
int cascade_verify(const active_filter_t *filter, double tolerance,
                    char *report);

/* ==================================================================
 * L8: Leapfrog (LC Ladder Simulation) Design
 * ================================================================== */

/** Design leapfrog active filter by simulating LC ladder prototype.
 *
 *  L8: Leapfrog filters simulate doubly-terminated LC ladder networks
 *  using integrators and summers in a leapfrog feedback structure.
 *  They inherit the low sensitivity of LC ladders — the gold standard
 *  for high-order filters.
 *
 *  The leapfrog structure implements the state equations of the LC
 *  ladder: each inductor current and capacitor voltage becomes an
 *  integrator output, with summer connections following the ladder
 *  topology (Kirchhoff's current/voltage laws).
 *
 *  Key advantage: Lowest sensitivity among all active topologies,
 *  approaching the insensitivity of passive LC filters.
 *
 *  Reference: Girling & Good, "The Leapfrog or Active Ladder Synthesis,"
 *            Wireless World, vol. 76, 1970
 *
 *  @param spec   Filter specification (order 3-9 supported)
 *  @param filter Output: leapfrog filter design with many op-amps
 *  @return       ACTIVE_OK or error code
 *  Complexity: O(N²) where N is filter order */
int cascade_leapfrog_design(const active_filter_spec_t *spec,
                             active_filter_t *filter);

/** Design LC ladder prototype values for a given filter specification.
 *  L3: Computes normalized L and C values of the doubly-terminated
 *  ladder, from which the leapfrog active equivalent is derived.
 *
 *  References: Williams & Taylor, Electronic Filter Design Handbook
 *              Zverev, Handbook of Filter Synthesis (1967)
 *
 *  @param spec       Filter specification
 *  @param l_values   Output: inductor values (henries, array size >= order)
 *  @param c_values   Output: capacitor values (farads, array size >= order)
 *  @param num_elem   Output: number of elements
 *  Complexity: O(N) using tabulated prototype values */
int cascade_ladder_prototype(const active_filter_spec_t *spec,
                              double *l_values, double *c_values,
                              int *num_elem);

/* ==================================================================
 * L7: First-Order Section (for odd-order filters)
 * ================================================================== */

/** Design first-order active section (LP or HP).
 *  L7: Used as the final stage in odd-order cascade filters.
 *  Simple RC + op-amp buffer or inverting amplifier.
 *
 *  LP: H(s) = -K / (1 + s·R·C)  (inverting integrator with DC feedback)
 *      or H(s) = K / (1 + s·R·C)  (non-inverting buffer with RC)
 *
 *  HP: H(s) = -K·s·R·C / (1 + s·R·C)
 *
 *  @param function  ACTIVE_FUNC_LOWPASS or ACTIVE_FUNC_HIGHPASS
 *  @param fc        -3dB cutoff frequency (Hz)
 *  @param gain_db   DC gain (LP) or HF gain (HP) in dB
 *  @param r_scale   Resistance scale (ohms)
 *  @param comp      Output component values
 *  Complexity: O(1) */
int cascade_first_order_section(active_filter_func_t function,
                                 double fc, double gain_db,
                                 double r_scale,
                                 active_component_values_t *comp);

#ifdef __cplusplus
}
#endif

#endif /* ACTIVE_FILTER_CASCADE_H */
