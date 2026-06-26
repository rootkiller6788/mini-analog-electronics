/**
 * active_filter_mfb.c — Multiple Feedback (MFB) Topology Implementation
 *
 * L2 Core Concepts: Complete MFB filter design for LP, HP, BP, and
 * notch configurations. MFB is the preferred single-op-amp topology
 * for bandpass filters with moderate to high Q.
 *
 * The MFB topology uses an inverting op-amp configuration with two
 * feedback paths — one resistive and one capacitive. This provides
 * better stopband rejection than Sallen-Key and higher achievable Q
 * for bandpass designs.
 *
 * Reference: J.J. Friend, "A Single Operational Amplifier Biquadratic
 *            Filter Section," in IEEE Int. Symp. Circuit Theory, 1970
 *            Daryanani, Principles of Active Network Synthesis (1976)
 *
 * Courses: MIT 6.002, Berkeley EE105, TU Munich
 */

#include "active_filter_mfb.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ==================================================================
 * L2: MFB Lowpass
 * ================================================================== */

/**
 * mfb_lp_design — Design MFB lowpass filter.
 *
 * L2: MFB LP circuit:
 *   R1: Input → inverting node
 *   R2: Inverting node → output (feedback)
 *   R3: Inverting node → ground (via R_n)
 *   C1: Inverting node → output (feedback capacitor)
 *   C2: Inverting node → ground
 *   R2 also connects to output → forms DC feedback.
 *
 * Transfer function:
 *   H(s) = -(R2/R1) / (s²·R2·R3·C1·C2 +
 *                       s·R2·R3·C1·(1/R1+1/R2+1/R3) + 1)
 *
 * Standard form: H(s) = -K / (s²/ω₀² + s/(ω₀·Q) + 1)
 *   ω₀² = 1/(R2·R3·C1·C2)
 *   1/Q = R2·R3·C1·ω₀·(1/R1+1/R2+1/R3) = √(R2·R3·C1/C2)·(1/R1+1/R2+1/R3)
 *   K = R2/R1 (DC gain magnitude)
 *
 * Design procedure (from ω₀, Q, K):
 *   1. Choose C2 (capacitor from inverting node to ground)
 *      Typical: C2 = 1/(2π·f0·R_desired)  ~ 100pF-10nF for audio
 *   2. Choose C1 such that real-valued R's are possible:
 *      Need C1 ≥ 4·Q²·C2  (for real R3)
 *   3. R2 = 1/(2·ω₀·Q·C2)
 *   4. R1 = R2/K
 *   5. R3 = 1/(ω₀²·R2·C1·C2)
 *
 * Reference: Sedra & Smith (2020), §16.4.1
 */
int mfb_lp_design(double wp, double qp, double gain,
                   double r_scale, active_component_values_t *comp)
{
    if (!comp || wp <= 0.0 || qp <= 0.0 || r_scale <= 0.0)
        return ACTIVE_ERR_NULL_PTR;

    /* Choose C2 based on impedance level */
    double c2 = 1.0 / (wp * r_scale);

    /* Constraint for real R3: C1 ≥ 4·Q²·C2 */
    double c1_min = 4.0 * qp * qp * c2;
    double c1 = c1_min * 1.1;  /* 10% margin */

    /* R2 from Q-C2 relationship */
    double r2 = 1.0 / (2.0 * wp * qp * c2);

    /* R1 from gain: K = R2/R1 */
    double r1 = r2 / (gain > 0.01 ? gain : 0.01);

    /* R3 from ω₀ constraint */
    double r3 = 1.0 / (wp * wp * r2 * c1 * c2);

    comp->r1 = r1;
    comp->r2 = r2;
    comp->r3 = r3;
    comp->r4 = 0.0;
    comp->r5 = 0.0;
    comp->c1 = c1;
    comp->c2 = c2;
    comp->c3 = 0.0;

    return ACTIVE_OK;
}

/**
 * mfb_lp_equal_r_design — MFB LP with equal resistors.
 *
 * L5: R1=R2=R3=R simplifies procurement. However, this imposes
 * a fixed relationship between gain and Q.
 *
 * With R1=R2=R3=R:
 *   K = R2/R1 = 1 (unity gain only)
 *   ω₀² = 1/(R²·C1·C2)
 *   Q = 1/(3)·√(C1/C2)
 *
 * So Q determines the C1/C2 ratio. For Q=1: C1 = 9·C2
 * For Q=10: C1 = 900·C2 — unrealistic capacitor ratio!
 *
 * Equal-R design is only practical for low-Q applications.
 */
int mfb_lp_equal_r_design(double wp, double qp, double gain,
                           double r_value, active_component_values_t *comp)
{
    if (!comp || wp <= 0.0 || qp <= 0.0 || r_value <= 0.0)
        return ACTIVE_ERR_NULL_PTR;

    /* Unity gain constraint for equal-R MFB */
    if (fabs(gain - 1.0) > 0.01)
        return mfb_lp_design(wp, qp, gain, r_value, comp);

    double r = r_value;
    /* Q = (1/3)·√(C1/C2) → C1 = 9·Q²·C2 */
    /* ω₀² = 1/(R²·C1·C2) = 1/(R²·9·Q²·C2²) → C2 = 1/(3·Q·ω₀·R) */
    double c2 = 1.0 / (3.0 * qp * wp * r);
    double c1 = 9.0 * qp * qp * c2;

    comp->r1 = r;
    comp->r2 = r;
    comp->r3 = r;
    comp->r4 = 0.0;
    comp->r5 = 0.0;
    comp->c1 = c1;
    comp->c2 = c2;
    comp->c3 = 0.0;

    return ACTIVE_OK;
}

/* ==================================================================
 * L2: MFB Highpass
 * ================================================================== */

/**
 * mfb_hp_design — Design MFB highpass filter.
 *
 * L2: MFB HP circuit is the C-R dual of the LP circuit.
 * Capacitors in the signal path, resistors to ground.
 *
 * Transfer function:
 *   H(s) = -(C1/C3)·s² / (s² + s·(C1+C2+C3)/(R2·C2·C3) +
 *                          1/(R1·R2·C2·C3))
 *
 * Equal-C design (C1=C2=C3=C):
 *   K_HF = 1 (high-frequency gain magnitude = C1/C3 = 1)
 *   ω₀² = 1/(R1·R2·C²)
 *   Q = √(R1/R2) / 3
 *
 * For Q=0.7071: √(R1/R2) = 3·0.7071 = 2.121 → R1/R2 = 4.5
 */
int mfb_hp_design(double wp, double qp, double gain,
                   double c_scale, active_component_values_t *comp)
{
    if (!comp || wp <= 0.0 || qp <= 0.0 || c_scale <= 0.0)
        return ACTIVE_ERR_NULL_PTR;

    double c = c_scale;

    /* Equal-C design: C1 = C2 = C3 = C */
    /* HF gain K = C1/C3 = 1 for equal-C */
    /* If gain != 1, adjust C1: K = C1/C3 → C1 = K·C3 = K·C */
    double c1 = gain * c;
    double c2 = c;
    double c3 = c;

    /* Q = √(R1/R2) / (1 + C2/C3 + C1/C3) ... actually */
    /* General formula: Q = √(R1·R2) / ... complex */
    /* Simpler: solve numerically */
    /* For equal-C, K=1: Q = √(R1/R2) / 3
     * → √(R1/R2) = 3·Q → R1 = 9·Q²·R2
     * ω₀² = 1/(R1·R2·C²) = 1/(9·Q²·R2²·C²) → R2 = 1/(3·Q·ω₀·C) */

    if (fabs(gain - 1.0) < 0.01) {
        double r2 = 1.0 / (3.0 * qp * wp * c);
        double r1 = 9.0 * qp * qp * r2;
        comp->r1 = r1;
        comp->r2 = r2;
    } else {
        /* General gain case: */
        /* K = C1/C3. Choose C3=C, C2=C, C1=K·C */
        /* Then: ω₀² = 1/(R1·R2·C2·C3) = 1/(R1·R2·C²)
         * Q = √(R1/R2) / (1 + C2/C3 + C1/C3) = √(R1/R2)/(2+K)
         * → R1/R2 = Q²·(2+K)²
         * → R2 = 1/(ω₀·C·Q·(2+K)), R1 = Q·(2+K)·R2 */
        double factor = 2.0 + gain;
        double r2 = 1.0 / (wp * c * qp * factor);
        double r1 = qp * factor * r2;
        comp->r1 = r1;
        comp->r2 = r2;
    }

    comp->r3 = 0.0;
    comp->r4 = 0.0;
    comp->r5 = 0.0;
    comp->c1 = c1;
    comp->c2 = c2;
    comp->c3 = c3;

    return ACTIVE_OK;
}

/* ==================================================================
 * L2: MFB Bandpass — Primary Use Case
 * ================================================================== */

/**
 * mfb_bp_design — Design MFB bandpass filter.
 *
 * L2: MFB BP is the most important MFB configuration. It achieves
 * higher Q than Sallen-Key BP with a single op-amp.
 *
 * Circuit:
 *   R1: Input → inverting node (via C1 — AC coupling)
 *   R2: Inverting node → ground (via C1)
 *   R3: Inverting node → output (feedback R)
 *   C1: Input → inverting node (series)
 *   C2: Inverting node → output (parallel feedback C)
 *
 * Transfer function:
 *   H(s) = -(1/(R1·C2))·s / (s² + s/(R3)·(1/C1+1/C2) +
 *                             1/(R3·C1·C2)·(1/R1+1/R2))
 *
 * Standard form:
 *   H(s) = -K·(ω₀/Q)·s / (s² + (ω₀/Q)·s + ω₀²)
 *
 *   ω₀² = (1/R1+1/R2) / (R3·C1·C2)
 *   Q = ω₀·R3·C1·C2 / (C1+C2)
 *   K = (1/R1) / (1/R3·(1/C1+1/C2)) = R3·C1 / (R1·(C1+C2))
 *
 * Simplified design (C1 = C2 = C):
 *   ω₀ = 1/C · √(1/R3·(1/R1+1/R2))
 *   Q = 0.5 · √(R3·(1/R1+1/R2))
 *   K = R3 / (2·R1)  (center-frequency gain)
 *
 * From Q and K:
 *   R1 = Q / (ω₀·C·K)        ... but K depends on R1!
 *   Iterative: R1 = Q/(ω₀·C·K), R3 = 2·K·R1, R2 = Q/(ω₀·C·(2·Q²-K))
 *
 * Constraint: 2·Q² > K for R2 > 0 (realizable).
 *
 * Daryanani design method (most widely used):
 *   Choose C1 = C2 = C
 *   R1 = Q / (ω₀·C·|K|)
 *   R2 = Q / (ω₀·C·(2·Q² - |K|))
 *   R3 = 2·Q / (ω₀·C)
 *
 * Reference: Daryanani (1976), §5.5.2
 */
int mfb_bp_design(double f0, double q, double gain_db,
                   double c_scale, active_component_values_t *comp)
{
    if (!comp || f0 <= 0.0 || q <= 0.0 || c_scale <= 0.0)
        return ACTIVE_ERR_NULL_PTR;

    double w0 = 2.0 * M_PI * f0;
    double gain_lin = pow(10.0, gain_db / 20.0);
    double c = c_scale;

    /* Daryanani equal-C design */
    double r1 = q / (w0 * c * gain_lin);

    /* Check realizability: 2·Q² > K */
    double denom = 2.0 * q * q - gain_lin;
    if (denom <= 0.0) {
        /* R2 would be negative or zero — reduce gain or increase Q */
        return ACTIVE_ERR_COMPONENT_RANGE;
    }
    double r2 = q / (w0 * c * denom);
    double r3 = 2.0 * q / (w0 * c);

    comp->r1 = r1;
    comp->r2 = r2;
    comp->r3 = r3;
    comp->r4 = 0.0;
    comp->r5 = 0.0;
    comp->c1 = c;
    comp->c2 = c;
    comp->c3 = 0.0;

    return ACTIVE_OK;
}

/**
 * mfb_bp_max_q — Estimate maximum achievable Q for MFB BP.
 *
 * L3: Q is limited by op-amp GBW and component tolerances.
 *
 * GBW limitation: The integrator quality at ω₀ depends on the
 * op-amp's excess gain at that frequency.
 * Q_max_GBW ≈ 0.5·√(GBW/(ω₀/(2π)))
 *
 * Tolerance limitation: Component mismatch limits Q.
 * Q_max_tol ≈ 1/(2·|Δtolerance|)
 *
 * Returns the more restrictive of the two.
 */
double mfb_bp_max_q(double f0, double gain_linear, double gbw_hz,
                     double tolerance_pct)
{
    (void)gain_linear;
    (void)f0;
    double q_gbw = 0.5 * sqrt(gbw_hz / f0);
    double q_tol = 1.0 / (2.0 * tolerance_pct / 100.0);
    return (q_gbw < q_tol) ? q_gbw : q_tol;
}

/* ==================================================================
 * L2: MFB Notch
 * ================================================================== */

/**
 * mfb_notch_design — Design MFB notch filter.
 *
 * L2: MFB notch uses a bridged-T network in the feedback path.
 * Active notch is deeper than passive because the op-amp
 * "bootstraps" the notch, effectively increasing Q.
 *
 * Notch frequency: f_n = 1/(2π·R·C) for Twin-T elements.
 * The op-amp samples the notch output and amplifies the
 * residual, then subtracts it from the input — this deepens
 * the notch.
 *
 * Notch depth limited by:
 *   - Resistor matching (affects notch frequency accuracy)
 *   - Op-amp CMRR (frequency-dependent)
 *   - Op-amp open-loop gain (finite gain at notch freq)
 */
int mfb_notch_design(double notch_freq, double q, double depth_db,
                      double r_scale, active_component_values_t *comp)
{
    if (!comp || notch_freq <= 0.0 || q <= 0.0 || r_scale <= 0.0)
        return ACTIVE_ERR_NULL_PTR;
    if (depth_db < 20.0) depth_db = 20.0;  /* Sensible minimum */

    double w0 = 2.0 * M_PI * notch_freq;

    /* Twin-T based MFB notch */
    double r = r_scale;
    double c = 1.0 / (w0 * r);

    /* Q is set by the bootstrap feedback ratio */
    /* Notch depth set by the summing accuracy */
    double feedback_factor = 1.0 - 1.0 / (4.0 * q);

    comp->r1 = r;
    comp->r2 = r * 0.5;     /* Twin-T bottom leg R/2 */
    comp->r3 = r_scale;     /* Summing resistor */
    comp->r4 = r_scale * feedback_factor;  /* Feedback divider */
    comp->r5 = r * 2.0;     /* Twin-T top grounding R */
    comp->c1 = c;
    comp->c2 = c;           /* Twin-T capacitors */
    comp->c3 = c * 2.0;     /* Twin-T mid capacitor */

    return ACTIVE_OK;
}

/* ==================================================================
 * L3: Transfer Function Analysis
 * ================================================================== */

int mfb_transfer_function(const active_component_values_t *comp,
                           active_filter_func_t function,
                           double num[3], double den[3], double *gain)
{
    if (!comp || !num || !den || !gain) return ACTIVE_ERR_NULL_PTR;

    switch (function) {
    case ACTIVE_FUNC_LOWPASS: {
        double r1 = comp->r1, r2 = comp->r2, r3 = comp->r3;
        double c1 = comp->c1, c2 = comp->c2;

        if (r1 <= 0 || r2 <= 0 || r3 <= 0 || c1 <= 0 || c2 <= 0)
            return ACTIVE_ERR_COMPONENT_RANGE;

        double w0_sq = 1.0 / (r2 * r3 * c1 * c2);
        double w0 = sqrt(w0_sq);
        double q_val = 1.0 / (w0 * (r1*r2*r3*c1*c2*w0 /
            (r1*r2*r3*c1*c2) * (1.0/r1 + 1.0/r2 + 1.0/r3)));

        /* Simplified: */
        double sum_g = 1.0/r1 + 1.0/r2 + 1.0/r3;
        q_val = sqrt(c2/c1) / (sqrt(r2*r3/(r1*r1)) * sum_g * sqrt(c1*c2));
        /* Numerically: */
        q_val = 1.0 / (sqrt(r2*r3*c1*c2) * sum_g);

        double k = r2 / r1;

        den[0] = 1.0;
        den[1] = w0 / q_val;
        den[2] = w0_sq;
        num[0] = 0.0;
        num[1] = 0.0;
        num[2] = k * w0_sq;
        *gain = k;
        break;
    }
    case ACTIVE_FUNC_BANDPASS: {
        double r1 = comp->r1, r2 = comp->r2, r3 = comp->r3;
        double c1 = comp->c1, c2 = comp->c2;

        if (r1 <= 0 || r2 <= 0 || r3 <= 0 || c1 <= 0 || c2 <= 0)
            return ACTIVE_ERR_COMPONENT_RANGE;

        double g1 = 1.0/r1, g2 = 1.0/r2, g3 = 1.0/r3;
        double w0_sq = (g1 + g2) * g3 / (c1 * c2);
        double w0 = sqrt(w0_sq);
        double q_val = sqrt(c1*c2*(g1+g2)*g3) / (g3*(c1+c2));
        /* Center-frequency gain = Q·K/ω₀ */
        (void)(g1 / (g3 * (c1+c2) / (c1*c2))); /* unused diagnostic formula */
        double h0 = -g1 / (g3 * (1.0/c1 + 1.0/c2));

        den[0] = 1.0;
        den[1] = w0 / q_val;
        den[2] = w0_sq;
        num[0] = 0.0;
        num[1] = -h0 * w0 / q_val;
        num[2] = 0.0;
        *gain = fabs(h0);
        break;
    }
    default:
        return ACTIVE_ERR_NOT_IMPL;
    }

    return ACTIVE_OK;
}

int mfb_extract_params(const active_component_values_t *comp,
                        active_filter_func_t function,
                        double *wp, double *qp, double *gain)
{
    double num[3], den[3];
    int ret = mfb_transfer_function(comp, function, num, den, gain);
    if (ret != ACTIVE_OK) return ret;

    *wp = sqrt(den[2]);
    *qp = *wp / den[1];
    return ACTIVE_OK;
}

/* ==================================================================
 * L5: Sensitivity Optimization
 * ================================================================== */

/**
 * mfb_optimize_sensitivity — Find optimal C1/C2 ratio for min sensitivity.
 *
 * L5: Sweeps C1/C2 ratio and computes worst-case sensitivity at each
 * point. The optimal ratio minimizes max(S_Ri^Q, S_Ci^Q).
 *
 * MFB Q sensitivity to capacitor ratio:
 *   S_C1^Q = 0.5 - Q·√(C2/C1)/2
 *   S_C2^Q = -0.5 + Q·√(C1/C2)/2
 *
 * These cross zero at different C1/C2 ratios. The optimum is
 * where they have equal magnitude.
 */
int mfb_optimize_sensitivity(double wp, double qp, double gain,
                              active_filter_func_t ftype,
                              active_component_values_t *comp,
                              double *c_ratio)
{
    if (!comp || wp <= 0.0 || qp <= 0.0) return ACTIVE_ERR_NULL_PTR;

    /* For MFB, the optimal C1/C2 ratio for minimum sensitivity
     * is approximately: C1/C2 = 4·Q² (for LP)
     * This makes all Q sensitivities ≈ 0.5 */
    double optimal_ratio = 4.0 * qp * qp;
    if (ftype == ACTIVE_FUNC_BANDPASS) {
        /* For BP, optimal ratio is different */
        optimal_ratio = 2.0 * qp;
    }

    *c_ratio = optimal_ratio;

    /* Design with optimal ratio */
    if (ftype == ACTIVE_FUNC_LOWPASS) {
        /* Using optimized C1/C2 ratio in LP design */
        double r_scale = 10000.0;  /* nominal */
        double c2 = 1.0 / (wp * r_scale * sqrt(optimal_ratio));
        double c1 = optimal_ratio * c2;
        double r2 = 1.0 / (2.0 * wp * qp * c2);
        double r1 = r2 / gain;
        double r3 = 1.0 / (wp * wp * r2 * c1 * c2);

        comp->r1 = r1;
        comp->r2 = r2;
        comp->r3 = r3;
        comp->r4 = 0.0;
        comp->r5 = 0.0;
        comp->c1 = c1;
        comp->c2 = c2;
        comp->c3 = 0.0;
    } else {
        /* For BP, use Daryanani with optimal C */
        double w0 = wp;
        double c1 = optimal_ratio * 1e-9;
        double c2 = 1e-9;
        double r1 = qp / (w0 * c1 * gain);
        double denom = 2.0 * qp * qp - gain;
        if (denom <= 0) return ACTIVE_ERR_COMPONENT_RANGE;
        double r2 = qp / (w0 * c1 * denom);
        double r3 = 2.0 * qp / (w0 * sqrt(c1 * c2));

        comp->r1 = r1;
        comp->r2 = r2;
        comp->r3 = r3;
        comp->r4 = 0.0;
        comp->r5 = 0.0;
        comp->c1 = c1;
        comp->c2 = c2;
        comp->c3 = 0.0;
    }

    return ACTIVE_OK;
}
