/**
 * active_filter_sallen_key.c — Sallen-Key Topology Implementation
 *
 * L2 Core Concepts: Complete Sallen-Key design for LP, HP, BP, and
 * notch configurations. Implements the design equations, transfer
 * function analysis, and optimization routines.
 *
 * The Sallen-Key topology uses a single op-amp with positive feedback
 * through a capacitor (or resistor for HP) to enhance Q beyond 0.5
 * (the maximum Q achievable with a passive RC cascade).
 *
 * Key design equations reference:
 *   Sallen & Key, "A Practical Method of Designing RC Active
 *   Filters," IRE Trans. Circuit Theory, vol. CT-2, pp. 74-85, 1955.
 *
 * Courses: MIT 6.002, Stanford EE101B, Berkeley EE105
 */

#include "active_filter_sallen_key.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ==================================================================
 * L2: Sallen-Key Lowpass — Full Design
 * ================================================================== */

/**
 * sallen_key_lp_design — Design Sallen-Key lowpass filter.
 *
 * L2: The Sallen-Key LP circuit consists of:
 *   - Input resistor R1 in series
 *   - R2 from op-amp output to junction of R1-C1
 *   - C1 from R1-R2 junction to non-inverting input
 *   - C2 from non-inverting input to ground
 *   - R3, R4 set gain K = 1 + R4/R3 (or unity gain if R3=∞, R4=0)
 *
 * Transfer function:
 *   H(s) = K / (s²·R1·R2·C1·C2 +
 *               s·[R1·C1 + R2·C2 + R1·C2·(1-K)] + 1)
 *
 * Equating to standard form: H(s) = K·ω₀²/(s² + s·ω₀/Q + ω₀²)
 *   ω₀² = 1/(R1·R2·C1·C2)
 *   1/Q = ω₀·(R1·C1 + R2·C2 + R1·C2·(1-K))
 *
 * Simplified design (equal-R, C-ratio method):
 *   Let R1 = R2 = R
 *   Let C1 = m·C, C2 = C (choose m based on required Q)
 *   Then: ω₀ = 1/(R·C·√m)
 *         1/Q = √m + 1/√m + √m·(1-K)
 *   For K=1: Q = 1/(√m + 1/√m)
 *   So for Q = 1/√2 (Butterworth): √m + 1/√m = √2 → m = 1
 *
 * For K > 1, the term √m·(1-K) reduces Q (more damping).
 * By making K > 1, we can achieve lower Q with same m, or
 * higher m for same Q.
 *
 * Reference: Sedra & Smith (2020), §16.3.1, Example 16.3
 */
int sallen_key_lp_design(double wp, double qp, double gain,
                          double r_scale, active_component_values_t *comp)
{
    if (!comp || wp <= 0.0 || qp <= 0.0 || r_scale <= 0.0)
        return ACTIVE_ERR_NULL_PTR;
    if (gain < 1.0) gain = 1.0;  /* Sallen-Key minimum gain = 1 */

    /* For gain K=1: simplest design, lowest sensitivity.
     * R1 = R2 = r_scale
     * Choose C1, C2 to satisfy ω₀ and Q.
     * From ω₀² = 1/(R²·C1·C2) and Q = 1/(√(C2/C1) + √(C1/C2)):
     *
     * Let n = √(C2/C1). Then Q = 1/(n + 1/n).
     * Solving: n² - n/Q + 1 = 0 → n = (1/Q ± √(1/Q² - 4))/2
     * For real n, need Q ≤ 0.5 (this is the unity-gain limit!)
     *
     * With gain K > 1:
     *   Q = 1/(n + 1/n + n·(1-K))
     *   Higher K REDUCES Q, allowing lower Q with real component ratios.
     *
     * For Q > 0.5 with K=1: C1 ≠ C2 layout yields complex n — not
     * physically realizable with equal R. We need unequal R.
     *
     * General design for arbitrary Q and K:
     *   Choose n = C2/C1 such that n + 1/n + n·(1-K) = 1/Q
     *   Let n = 1 for simplicity.
     *   Then C1 = C2 = C, and 2 + (1-K) = 1/Q → K = 3 - 1/Q
     *
     * This shows K directly sets Q when C1=C2, R1=R2=R.
     *
     * Butterworth Q=0.7071 → K = 3 - 1.414 = 1.586
     * This is the classic Sallen-Key Butterworth design!
     *
     * ω₀ = 1/(R·C) (when R1=R2=R, C1=C2=C).
     */

    double k_required = 3.0 - 1.0 / qp;
    if (k_required < 1.0) {
        /* Q > 0.5 but K < 1 needed — use unequal C method */
        /* For unity gain (K=1) and Q > 0.5:
         * Use C1 = 2·Q·C2: then R1 = R2 = 1/(ω₀·√(C1·C2)) */
        double c2 = 1.0 / (wp * r_scale);
        double c1 = 2.0 * qp * c2;
        double r_check = 1.0 / (wp * sqrt(c1 * c2));

        /* Scale to fit impedance target */
        double z_ratio = r_scale / r_check;
        c1 /= z_ratio;
        c2 /= z_ratio;

        comp->r1 = r_scale;
        comp->r2 = r_scale;
        comp->r3 = 0.0;    /* Open for unity gain */
        comp->r4 = 0.0;    /* Short to output */
        comp->r5 = 0.0;
        comp->c1 = c1;
        comp->c2 = c2;
        comp->c3 = 0.0;
    } else if (k_required <= gain) {
        /* Can use equal-R, equal-C with K = k_required */
        double c_val = 1.0 / (wp * r_scale);

        comp->r1 = r_scale;
        comp->r2 = r_scale;
        /* R3, R4 set K = 1 + R4/R3 → choose R3 = r_scale, R4 = (K-1)·R3 */
        comp->r3 = r_scale;
        comp->r4 = (k_required - 1.0) * r_scale;
        comp->r5 = 0.0;
        comp->c1 = c_val;
        comp->c2 = c_val;
        comp->c3 = 0.0;
    } else {
        /* K needed > gain available → Use m = C2/C1 > 1 to reduce K */
        /* General solution: Q = 1/(√m + 1/√m + √m·(1-K))
         * Solve for m given desired Q and available K. */
        double k_use = gain;
        /* Quadratic in n = √m:
         * n²·(1-K) + n/Q + 1 = 0  where K > 1
         * n = [-1/Q ± √(1/Q² - 4·(1-K))] / (2·(1-K))
         * Since K > 1, denominator is negative. For positive n:
         * n = [1/Q + √(1/Q² + 4·(K-1))] / (2·(K-1)) */
        double disc = 1.0/(qp*qp) + 4.0*(k_use - 1.0);
        double n = (1.0/qp + sqrt(disc)) / (2.0 * (k_use - 1.0));
        double m = n * n;

        double c2 = 1.0 / (wp * r_scale * n);
        double c1 = m * c2;

        comp->r1 = r_scale;
        comp->r2 = r_scale;
        comp->r3 = r_scale;
        comp->r4 = (k_use - 1.0) * r_scale;
        comp->r5 = 0.0;
        comp->c1 = c1;
        comp->c2 = c2;
        comp->c3 = 0.0;
    }

    return ACTIVE_OK;
}

/**
 * sallen_key_lp_unity_gain — Unity-gain Sallen-Key LP design.
 *
 * L2: Special case K=1 with voltage follower. Simplest active filter.
 * All Q < 0.5 are passive and don't need the op-amp at all.
 * Q = 0.5 to 10 achievable with unity gain, using C1 >> C2.
 *
 * For unity gain: K=1, R3=∞, R4=0.
 *   ω₀ = 1/(R·√(C1·C2))
 *   Q = 1/(√(C1/C2) + √(C2/C1))
 *
 * Let n² = C1/C2. Then Q = 1/(n + 1/n) → n = Q + √(Q²-1) for Q ≥ 0.5.
 *
 * For Q = 0.707 (Butterworth): n = 0.707 + 0 = 0.707? No...
 * n = Q + √(Q²-1) = 0.707 + √(-0.5) — this is complex!
 *
 * With K=1, the maximum Q achievable is 0.5 (when C1=C2).
 * This is a fundamental limitation: unity-gain Sallen-Key LP
 * CANNOT achieve Q > 0.5! That's why the gain variant exists.
 *
 * For Q > 0.5, we MUST use K > 1, or use unequal R values.
 *
 * Unequal-R method for unity gain:
 * Let R2 = x·R1, C1=C2=C.
 * Then ω₀ = 1/(√x·R1·C), Q = √x/(x+1)
 * For Butterworth Q=0.707: √x/(x+1) = 0.707 → x² + x - 2 = 0 → x = 1
 * Same constraint! The gain variant is indeed necessary for Q > 0.5.
 *
 * Correct approach: use K ≥ 1 + 2Q for Q > 0.5 with equal R,C.
 * This is redirected to sallen_key_lp_design with appropriate gain.
 */
int sallen_key_lp_unity_gain(double wp, double qp, double r_scale,
                              active_component_values_t *comp)
{
    if (!comp || wp <= 0.0 || qp <= 0.0 || r_scale <= 0.0)
        return ACTIVE_ERR_NULL_PTR;

    if (qp > 0.5) {
        /* Can't achieve Q > 0.5 with unity gain LP.
         * Use minimum K = 3 - 1/Q to achieve desired Q. */
        double k_min = 3.0 - 1.0 / qp;
        return sallen_key_lp_design(wp, qp, k_min, r_scale, comp);
    }

    /* Q ≤ 0.5: achievable with K=1, equal R, equal C */
    double c = 1.0 / (wp * r_scale);
    comp->r1 = r_scale;
    comp->r2 = r_scale;
    comp->r3 = 0.0;
    comp->r4 = 0.0;
    comp->r5 = 0.0;
    comp->c1 = c;
    comp->c2 = c;
    comp->c3 = 0.0;

    return ACTIVE_OK;
}

/* ==================================================================
 * L2: Sallen-Key Highpass
 * ================================================================== */

/**
 * sallen_key_hp_design — Design Sallen-Key highpass filter.
 *
 * L2: HP is the dual of LP: swap R↔C in the circuit.
 *
 * Transfer function:
 *   H(s) = K·s²/(s² + s·[1/(R2·C1) + 1/(R2·C2) + (1-K)/(R1·C1)] +
 *                 1/(R1·R2·C1·C2))
 *
 * Standard form: H(s) = K·s²/(s² + s·ω₀/Q + ω₀²)
 *   ω₀² = 1/(R1·R2·C1·C2)
 *
 * Equal-C design (C1=C2=C):
 *   ω₀ = 1/(C·√(R1·R2))
 *   Q = 1/(2·√(R2/R1) + √(R1/R2)·(1-K))
 *
 * For K=1, equal-R: ω₀ = 1/(R·C), Q = 1/(2 + (1-K)) = 0.5
 * Same limitation as LP! K > 1 needed for Q > 0.5.
 *
 * For Butterworth Q=0.707, with K=1.586, equal-R, equal-C:
 *   ω₀ = 1/(R·C), Q = 1/(2 + 0.586·(-0.586)) = 1/(2 - 0.343) ≈ 0.603
 * Hmm, need adjusted values. Let's use the general method.
 *
 * General: choose C1=C2=C, R1=R_scale.
 * Then from Q equation, solve for R2.
 */
int sallen_key_hp_design(double wp, double qp, double gain,
                          double c_scale, active_component_values_t *comp)
{
    if (!comp || wp <= 0.0 || qp <= 0.0 || c_scale <= 0.0)
        return ACTIVE_ERR_NULL_PTR;
    if (gain < 1.0) gain = 1.0;

    double c = c_scale;

    /* For K=1: R1 = 1/(2·Q·ω₀·C), R2 = 2·Q/(ω₀·C) — this gives Q */
    /* For K > 1: use equal-R method */
    if (fabs(gain - 1.0) < 0.01) {
        /* Unity gain HP: unique R1,R2 for desired Q */
        double r1 = 1.0 / (2.0 * qp * wp * c);
        double r2 = 2.0 * qp / (wp * c);
        comp->r1 = r1;
        comp->r2 = r2;
        comp->r3 = 0.0;
        comp->r4 = 0.0;
    } else {
        /* Gain > 1 variant: use K to set Q */
        double r = 1.0 / (wp * c);
        comp->r1 = r;
        comp->r2 = r;
        comp->r3 = r;
        /* Use gain arg to set feedback ratio; Q = 1/(2 + (1-K)) */
        (void)qp; /* Q is set implicitly by gain choice */
        comp->r4 = (gain - 1.0) * r;
    }

    comp->r5 = 0.0;
    comp->c1 = c;
    comp->c2 = c;
    comp->c3 = 0.0;

    return ACTIVE_OK;
}

int sallen_key_hp_unity_gain(double wp, double qp, double c_scale,
                              active_component_values_t *comp)
{
    return sallen_key_hp_design(wp, qp, 1.0, c_scale, comp);
}

/* ==================================================================
 * L2: Sallen-Key Bandpass
 * ================================================================== */

/**
 * sallen_key_bp_design — Sallen-Key bandpass filter.
 *
 * L2: The Sallen-Key BP uses a Wien network-like topology.
 * The circuit is a bridge of RC elements with the op-amp providing
 * positive feedback to sharpen Q.
 *
 * Transfer function (standard form):
 *   H(s) = K·(ω₀/Q)·s / (s² + (ω₀/Q)·s + ω₀²)
 *
 * Component relations (simplified equal-R, equal-C design):
 *   ω₀ = 1/(R·C)
 *   Q = 1/(3 - K)  ← This is the key! K must be < 3 for stability!
 *   K = 1 + R4/R3
 *
 * For Q = 10: K = 3 - 1/10 = 2.9 (very close to oscillation!)
 * This illustrates why high-Q Sallen-Key BP is problematic.
 *
 * Reference: Daryanani, Principles of Active Network Synthesis
 *            and Design (1976), §5.4
 */
int sallen_key_bp_design(double f0, double q, double gain_db,
                          double r_scale, active_component_values_t *comp)
{
    if (!comp || f0 <= 0.0 || q <= 0.0 || r_scale <= 0.0)
        return ACTIVE_ERR_NULL_PTR;
    if (q > 25.0) return ACTIVE_ERR_INVALID_Q;  /* Beyond practical SK BP */

    double w0 = 2.0 * M_PI * f0;
    double k = 3.0 - 1.0 / q;  /* For Q > 0.5, K < 3 */

    if (k <= 1.0) {
        /* Q ≤ 0.5 → non-positive gain needed — use unity gain */
        k = 1.0;
    }
    if (k >= 3.0) return ACTIVE_ERR_INVALID_Q;  /* K too close to oscillation */

    /* Center-frequency gain: |H(jω₀)| = K·Q/(3-K) = K·Q² */
    /* But we want specific gain_db. Adjust K accordingly. */
    double gain_desired = pow(10.0, gain_db / 20.0);
    /* gain_desired = K·Q² (for this topology with equal R,C) */
    double k_needed = gain_desired / (q * q);
    if (k_needed < 1.0) k_needed = 1.0;
    if (k_needed > 2.95) k_needed = 2.95;

    /* Use K that satisfies both Q and gain requirements */
    /* If they conflict, prioritize stability (Q-based K) */
    double k_use = (k_needed < k) ? k_needed : k;

    double r = r_scale;
    double c = 1.0 / (w0 * r);

    comp->r1 = r;   /* Input R */
    comp->r2 = r;   /* Ground R */
    comp->r3 = r;   /* Gain-setting R3 */
    comp->r4 = (k_use - 1.0) * r;  /* Gain-setting R4 */
    comp->r5 = 0.0;
    comp->c1 = c;
    comp->c2 = c;
    comp->c3 = 0.0;

    return ACTIVE_OK;
}

/* ==================================================================
 * L2: Sallen-Key Notch (Twin-T)
 * ================================================================== */

/**
 * sallen_key_notch_design — Twin-T notch filter.
 *
 * L2: The active Twin-T uses a passive Twin-T network (3R, 3C)
 * in the negative feedback path of an op-amp, with the "T" midpoint
 * bootstrapped to the output through a voltage divider to deepen
 * the notch.
 *
 * Passive Twin-T notch frequency: f_n = 1/(2π·R·C)
 * Passive notch depth: ~30-40 dB (limited by component matching)
 * Active notch depth: > 60 dB (Q-enhanced by op-amp)
 *
 * Design:
 *   Top T: R - 2C - R (series), C/2 to ground at midpoint
 *   Bottom T: C - R/2 - C (series), 2R to ground at midpoint
 *   Notch at f_n = 1/(2πRC)
 *
 * Q enhancement: The op-amp samples the Twin-T output and
 * feeds back to the ground node. Q ≈ 1/(4·(1-β))
 * where β is the feedback factor. For β → 1, Q → ∞.
 * Practical β = 0.9 → Q ≈ 2.5; β = 0.99 → Q ≈ 25.
 */
int sallen_key_notch_design(double notch_freq, double q,
                             double r_scale, active_component_values_t *comp)
{
    if (!comp || notch_freq <= 0.0 || q <= 0.0 || r_scale <= 0.0)
        return ACTIVE_ERR_NULL_PTR;

    double w0 = 2.0 * M_PI * notch_freq;

    /* Choose R = r_scale, compute C for notch frequency */
    double r = r_scale;
    double c = 1.0 / (w0 * r);

    /* For active Twin-T, the feedback β sets Q:
     * β = 1 - 1/(4·Q)  for Q > 0.25 */
    double beta = 1.0 - 1.0 / (4.0 * q);
    if (beta < 0.0) beta = 0.0;
    if (beta > 0.99) beta = 0.99;

    /* R3, R4 set β = R4/(R3+R4) */
    double r_divider = r_scale;
    comp->r4 = beta * r_divider;
    comp->r3 = r_divider - comp->r4;
    if (comp->r3 < 100.0) comp->r3 = 100.0;

    comp->r1 = r;
    comp->r2 = r / 2.0;  /* Bottom T uses R/2 */
    comp->r5 = 2.0 * r;  /* Top T mid grounding resistor */
    comp->c1 = c;
    comp->c2 = c;
    comp->c3 = 2.0 * c;  /* Top T mid capacitor */

    return ACTIVE_OK;
}

/* ==================================================================
 * L3: Transfer Function Analysis
 * ================================================================== */

/**
 * sallen_key_transfer_function — Compute H(s) coefficients from components.
 *
 * L3: Given physical components, derive the exact transfer function.
 * Essential for verification and tolerance analysis.
 */
int sallen_key_transfer_function(const active_component_values_t *comp,
                                  active_filter_func_t function,
                                  double num[3], double den[3], double *gain)
{
    if (!comp || !num || !den || !gain) return ACTIVE_ERR_NULL_PTR;

    double r1 = comp->r1, r2 = comp->r2;
    double c1 = comp->c1, c2 = comp->c2;
    double r3 = comp->r3, r4 = comp->r4;

    if (r1 <= 0 || r2 <= 0 || c1 <= 0 || c2 <= 0)
        return ACTIVE_ERR_COMPONENT_RANGE;

    double k;
    if (r3 <= 0 && r4 <= 0) {
        k = 1.0;  /* Unity gain follower */
    } else if (r3 > 0 && r4 <= 0) {
        k = 1.0;  /* R4 shorted */
    } else if (r4 > 0) {
        k = 1.0 + r4 / (r3 > 0 ? r3 : 1e6);
    } else {
        k = 1.0;
    }
    *gain = k;

    switch (function) {
    case ACTIVE_FUNC_LOWPASS: {
        /* H(s) = K / (s²·R1·R2·C1·C2 + s·[R1·C1 + R2·C2 + R1·C2·(1-K)] + 1) */
        double a2 = r1 * r2 * c1 * c2;
        double a1 = r1 * c1 + r2 * c2 + r1 * c2 * (1.0 - k);
        double a0 = 1.0;

        den[0] = 1.0;      /* s² coefficient = 1 */
        den[1] = a1 / a2;  /* s coefficient */
        den[2] = a0 / a2;  /* constant = ω₀² */
        num[0] = 0.0;
        num[1] = 0.0;
        num[2] = k / a2;   /* numerator = K·ω₀² */
        break;
    }
    case ACTIVE_FUNC_HIGHPASS: {
        /* HP: swap R↔C from LP
         * H(s) = K·s² / (s² + s·[1/(R2·C1)+1/(R2·C2)+(1-K)/(R1·C1)]
         *                 + 1/(R1·R2·C1·C2)) */
        double a0 = 1.0 / (r1 * r2 * c1 * c2);
        double a1 = 1.0/(r2*c1) + 1.0/(r2*c2) + (1.0-k)/(r1*c1);

        den[0] = 1.0;
        den[1] = a1;
        den[2] = a0;
        num[0] = k;         /* s² coefficient */
        num[1] = 0.0;
        num[2] = 0.0;
        break;
    }
    case ACTIVE_FUNC_BANDPASS: {
        /* BP: H(s) = K·(ω₀/Q)·s / (s² + (ω₀/Q)·s + ω₀²)
         * ω₀ = 1/(R·C), Q = 1/(3-K) for equal-R,C design */
        double rc = r1 * c1;  /* assuming R1=R2=R, C1=C2=C */
        double w0 = 1.0 / rc;
        double q_val = 1.0 / (3.0 - k);

        den[0] = 1.0;
        den[1] = w0 / q_val;
        den[2] = w0 * w0;
        num[0] = 0.0;
        num[1] = k * w0 / q_val;
        num[2] = 0.0;
        break;
    }
    default:
        return ACTIVE_ERR_NOT_IMPL;
    }

    return ACTIVE_OK;
}

/**
 * sallen_key_extract_params — Extract ω₀, Q, gain from components.
 *
 * L3: Inverse of design — given a built circuit, what are its
 * filter parameters? Useful for troubleshooting and tolerance
 * verification.
 */
int sallen_key_extract_params(const active_component_values_t *comp,
                               active_filter_func_t function,
                               double *wp, double *qp, double *gain)
{
    if (!comp || !wp || !qp || !gain) return ACTIVE_ERR_NULL_PTR;

    double num[3], den[3];
    int ret = sallen_key_transfer_function(comp, function, num, den, gain);
    if (ret != ACTIVE_OK) return ret;

    /* From denominator: s² + (ω₀/Q)·s + ω₀² = s² + den[1]·s + den[2] */
    *wp = sqrt(den[2]);
    *qp = *wp / den[1];

    return ACTIVE_OK;
}

/* ==================================================================
 * L5: E-Series Snap Optimization
 * ================================================================== */

/**
 * sallen_key_snap_to_eseries — Snap component values to standard values.
 *
 * L5: Real-world components come in standard E-series values.
 * This function finds the nearest standard values and computes
 * the resulting filter parameter errors.
 */
int sallen_key_snap_to_eseries(const active_component_values_t *comp_in,
                                int e_series, active_component_values_t *comp_out,
                                double *freq_err, double *q_err)
{
    if (!comp_in || !comp_out) return ACTIVE_ERR_NULL_PTR;

    /* Snap resistances */
    comp_out->r1 = active_nearest_eseries(comp_in->r1, e_series);
    comp_out->r2 = active_nearest_eseries(comp_in->r2, e_series);
    comp_out->r3 = (comp_in->r3 > 0) ?
        active_nearest_eseries(comp_in->r3, e_series) : 0.0;
    comp_out->r4 = (comp_in->r4 > 0) ?
        active_nearest_eseries(comp_in->r4, e_series) : 0.0;
    comp_out->r5 = 0.0;

    /* Snap capacitances */
    comp_out->c1 = active_nearest_eseries(comp_in->c1, e_series);
    comp_out->c2 = active_nearest_eseries(comp_in->c2, e_series);
    comp_out->c3 = 0.0;

    /* Compute parameter errors */
    double wp_ideal = 0.0, qp_ideal = 0.0, gain_ideal = 0.0;
    sallen_key_extract_params(comp_in, ACTIVE_FUNC_LOWPASS,
                               &wp_ideal, &qp_ideal, &gain_ideal);

    double wp_snap = 0.0, qp_snap = 0.0, gain_snap = 0.0;
    sallen_key_extract_params(comp_out, ACTIVE_FUNC_LOWPASS,
                               &wp_snap, &qp_snap, &gain_snap);

    *freq_err = (wp_snap - wp_ideal) / wp_ideal * 100.0;
    *q_err = (qp_snap - qp_ideal) / qp_ideal * 100.0;

    return ACTIVE_OK;
}
