/**
 * active_filter_state_variable.c — State-Variable & Biquad Implementation
 *
 * L2 Core Concepts: KHN state-variable filter, Tow-Thomas biquad,
 * and Akerberg-Mossberg single-op-amp biquad.
 *
 * The KHN and Tow-Thomas topologies are the "gold standard" for
 * precision active filters: they offer simultaneous multiple outputs
 * (KHN), independent ω₀ and Q tuning, and the lowest sensitivity
 * among all active topologies.
 *
 * Reference:
 *   Kerwin, Huelsman & Newcomb, "State-Variable Synthesis for
 *   Insensitive Integrated Circuit Transfer Functions,"
 *   IEEE JSSC, vol. SC-2, no. 3, pp. 87-92, 1967.
 *   Tow, "A Step-by-Step Active Filter Design," IEEE Spectrum,
 *   vol. 6, no. 12, pp. 64-68, 1969.
 *
 * Courses: Stanford EE101B, Berkeley EE105, ETH 227-0455
 */

#include "active_filter_state_variable.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ==================================================================
 * L2: KHN State-Variable Filter
 * ================================================================== */

/**
 * khn_state_variable_design — Design KHN state-variable filter.
 *
 * L2: The KHN filter uses 3 op-amps:
 *   Amp1: Summing amplifier — produces HP output
 *   Amp2: Integrator 1 (lossless) — HP→BP
 *   Amp3: Integrator 2 (lossless) — BP→LP
 *
 * Architecture signal flow:
 *   V_in ──→ [Sum Amp] ──(HP)──→ [Integ1] ──(BP)──→ [Integ2] ──(LP)
 *              ↑   ↑                         |           |
 *              │   └───── R_q feedback ──────┘           │
 *              └─────────── R_f feedback ────────────────┘
 *
 * The summer computes:
 *   V_HP = -(Rf/R_in)·V_in - (R_q/R_bp)·V_BP - (R_q/R_lp)·V_LP
 *
 * For the standard second-order section:
 *   V_HP / V_in = s²·K_HP / (s² + s·ω₀/Q + ω₀²)
 *   V_BP / V_in = -s·ω₀·K_BP / (s² + s·ω₀/Q + ω₀²)
 *   V_LP / V_in = ω₀²·K_LP / (s² + s·ω₀/Q + ω₀²)
 *
 * Design (equal-R, equal-C, unity-gain variant):
 *   Choose R (integrator resistor) and C (integrator capacitor).
 *   ω₀ = 1/(R·C)
 *
 *   For Q, the Q-setting resistor network:
 *   R_q connects BP output to summer input.
 *   For the standard inverting KHN: Q = R_q / R
 *   (This gives independent Q control by adjusting only R_q!)
 *
 *   Gain: K_HP = R_f/R_in, K_BP = Q·K_HP, K_LP = K_HP
 *
 * This is the cleanest and most tunable active filter topology.
 * ω₀ can be tuned via R (dual-gang pot), Q via R_q alone.
 */
int khn_state_variable_design(double f0, double q, double gain_db,
                               double r_scale, active_component_values_t *comp)
{
    if (!comp || f0 <= 0.0 || q <= 0.0 || r_scale <= 0.0)
        return ACTIVE_ERR_NULL_PTR;

    double w0 = 2.0 * M_PI * f0;
    double gain_lin = pow(10.0, gain_db / 20.0);

    /* Design procedure (equal-R/C, standard KHN):
     * 1. Choose C (integrator capacitor) based on frequency
     *    For audio: 1nF to 100nF
     *    f0 = 1kHz → C = 1nF, R = 1/(w0·C) = 159k (choose 160k)
     * 2. Set ω₀: choose R_int = 1/(w0·C)
     * 3. Set Q: R_q = Q · R_int (the Q-setting resistor from BP to summer)
     * 4. Set gain: R_in = R_f / K  where R_f sets feedback level
     */

    /* Choose practical C based on frequency */
    double c;
    if (f0 < 100.0)      c = 100e-9;  /* 100nF for sub-audio */
    else if (f0 < 1000.0) c = 10e-9;  /* 10nF for audio low */
    else if (f0 < 10000.0) c = 1e-9;  /* 1nF for audio mid */
    else if (f0 < 100000.0) c = 100e-12; /* 100pF for audio high */
    else                   c = 10e-12; /* 10pF for RF */

    double r_int = 1.0 / (w0 * c);

    /* Scale to r_scale if within reasonable range */
    if (r_int < r_scale / 100.0) {
        c *= r_int / r_scale * 100.0;
        r_int = r_scale;
    } else if (r_int > r_scale * 100.0) {
        c *= r_int / r_scale / 100.0;
        r_int = r_scale;
    }

    /* R_q sets Q: Q = R_q / R_int (for standard KHN) */
    double r_q = q * r_int;

    /* R_f and R_in set the HP gain:
     * K_HP = -R_f/R_in → choose R_f = r_int, R_in = R_f/K */
    double r_f = r_int;
    double r_in = r_f / (gain_lin > 0.01 ? gain_lin : 0.01);

    comp->r1 = r_int;   /* Integrator R (both channels same) */
    comp->r2 = r_q;     /* Q-setting resistor (BP to summer) */
    comp->r3 = r_in;    /* Input resistor (to summer) */
    comp->r4 = r_f;     /* Feedback resistor (HP to summer) */
    comp->r5 = r_int;   /* Second integrator R (same value) */
    comp->c1 = c;       /* Integrator 1 capacitor */
    comp->c2 = c;       /* Integrator 2 capacitor */
    comp->c3 = 0.0;

    return ACTIVE_OK;
}

/**
 * khn_state_variable_trimmable — KHN with independent ω₀ and Q trim.
 *
 * L5: The KHN uniquely allows independent tuning of ω₀ and Q.
 *
 * ω₀ tuning: A dual-gang potentiometer in series with the
 * integrator resistors varies ω₀ without affecting Q.
 *   R_eff = R_fixed + R_trim_pot
 *   ω₀ = 1/(R_eff·C) → trim_range yields ω₀ range.
 *
 * Q tuning: A potentiometer in the Q-setting feedback path
 * adjusts Q independently of ω₀.
 *   Q = R_q_total / R_int where R_q_total = R_fixed + R_q_pot
 */
int khn_state_variable_trimmable(double f0, double q, double trim_range,
                                  double r_scale,
                                  active_component_values_t *comp)
{
    if (!comp || f0 <= 0.0 || q <= 0.0 || trim_range <= 0.0)
        return ACTIVE_ERR_NULL_PTR;

    double w0 = 2.0 * M_PI * f0;
    double c = (f0 < 10000.0) ? 1e-9 : 100e-12;
    double r_nominal = 1.0 / (w0 * c);

    /* Fixed R = nominal · (1 - trim_range) */
    double r_fixed = r_nominal * (1.0 - trim_range);
    /* Potentiometer = nominal · 2·trim_range (center = nominal) */
    double r_pot = r_nominal * 2.0 * trim_range;
    (void)r_pot;  /* Potentiometer value stored for reference */

    /* Q resistor: fixed + pot in series */
    double rq_nominal = q * r_nominal;
    double rq_fixed = rq_nominal * (1.0 - trim_range);
    double rq_pot = rq_nominal * 2.0 * trim_range;

    comp->r1 = r_fixed;      /* Fixed integrator R */
    comp->r2 = rq_fixed;     /* Fixed Q-setting R */
    comp->r3 = r_nominal;    /* Input R (unity gain) */
    comp->r4 = r_nominal;    /* Feedback R */
    comp->r5 = rq_pot;       /* Q trim pot (use R5 to store pot value) */
    (void)r_scale;           /* Used only through r_nominal derivation */
    comp->c1 = c;
    comp->c2 = c;
    comp->c3 = 0.0;

    return ACTIVE_OK;
}

/**
 * khn_compute_outputs — Compute all three KHN transfer functions.
 *
 * L3: Derives HP, BP, LP transfer functions from component values.
 * Each output shares the same denominator but has a unique numerator.
 */
int khn_compute_outputs(const active_component_values_t *comp,
                         double hp_num[3], double bp_num[3],
                         double lp_num[3], double den[3],
                         double *hp_gain, double *bp_gain, double *lp_gain)
{
    if (!comp || !hp_num || !bp_num || !lp_num || !den)
        return ACTIVE_ERR_NULL_PTR;

    double r_int = comp->r1;   /* Integrator R */
    double r_q = comp->r2;     /* Q-setting R */
    double r_in = comp->r3;    /* Input R */
    double r_f = comp->r4;     /* Feedback R */
    double c = comp->c1;       /* Integrator C (assume equal) */

    if (r_int <= 0 || c <= 0) return ACTIVE_ERR_COMPONENT_RANGE;

    double w0 = 1.0 / (r_int * c);
    double q = r_q / r_int;
    double k_hp = r_f / r_in;  /* HP gain magnitude */

    /* Common denominator: s² + (ω₀/Q)·s + ω₀² */
    den[0] = 1.0;
    den[1] = w0 / q;
    den[2] = w0 * w0;

    /* HP numerator: K_HP · s² */
    hp_num[0] = k_hp;
    hp_num[1] = 0.0;
    hp_num[2] = 0.0;

    /* BP numerator: -K_BP · (ω₀/Q) · s  (K_BP = Q·K_HP) */
    bp_num[0] = 0.0;
    bp_num[1] = -k_hp * q * w0 / q;  /* = -k_hp * w0 */
    bp_num[2] = 0.0;

    /* LP numerator: K_LP · ω₀²  (K_LP = K_HP) */
    lp_num[0] = 0.0;
    lp_num[1] = 0.0;
    lp_num[2] = k_hp * w0 * w0;

    *hp_gain = k_hp;
    *bp_gain = k_hp * q;
    *lp_gain = k_hp;

    return ACTIVE_OK;
}

/**
 * khn_bandstop_from_sums — Create BS output from HP + LP summing.
 *
 * L2: The KHN inherently provides BS by summing HP and LP outputs
 * through an additional op-amp summer.
 *
 * V_BS = -(R_sum/R_hp)·V_HP - (R_sum/R_lp)·V_LP
 *
 * When R_hp = R_lp = R_sum (equal-weight sum):
 * V_BS/V_in = -K·(s² + ω₀²) / (s² + s·ω₀/Q + ω₀²)
 *
 * This is a notch at ω₀ with notch Q = filter Q.
 * Perfect cancellation requires matched summing resistors
 * (or a trim pot for notch depth optimization).
 */
int khn_bandstop_from_sums(const active_component_values_t *comp,
                            double bs_num[3], double den[3],
                            double *notch_depth)
{
    if (!comp || !bs_num || !den || !notch_depth)
        return ACTIVE_ERR_NULL_PTR;

    double hp_num[3], bp_num[3], lp_num[3];
    double hp_gain, bp_gain, lp_gain;

    int ret = khn_compute_outputs(comp, hp_num, bp_num, lp_num,
                                   den, &hp_gain, &bp_gain, &lp_gain);
    if (ret != ACTIVE_OK) return ret;

    /* BS numerator = HP numerator + LP numerator
     * = K·s² + K·ω₀² = K·(s² + ω₀²) */
    bs_num[0] = hp_num[0];         /* K */
    bs_num[1] = 0.0;               /* No s term (notch at ω₀) */
    bs_num[2] = lp_num[2];         /* K·ω₀² */

    /* Notch depth depends on matching.
     * With perfect matching: infinite notch depth (zero at ±jω₀).
     * With 1% mismatch: notch depth ≈ 40 dB.
     * With 0.1% matching: notch depth ≈ 60 dB. */
    *notch_depth = 60.0;  /* Theoretical for well-trimmed circuit */

    return ACTIVE_OK;
}

/* ==================================================================
 * L2: Tow-Thomas Biquad
 * ================================================================== */

/**
 * tow_thomas_biquad_design — Design Tow-Thomas biquad.
 *
 * L2: The Tow-Thomas biquad uses 3 op-amps:
 *   Amp1: Damped integrator — produces BP output
 *         H1(s) = -(1/(R1·C1))·s / (s² + s/(Rq·C1) + 1/(R·R2·C1·C2))
 *   Amp2: Lossless integrator — BP→LP
 *         H2(s) = -1/(s·R·C2)
 *   Amp3: Inverter (optional) — phase correction
 *
 * Transfer functions:
 *   LP:  V_LP/V_in = -(R2/R1) · ω₀²/(s² + s·ω₀/Q + ω₀²)
 *   BP:  V_BP/V_in = -(Rq/R1) · (ω₀/Q)·s/(s² + s·ω₀/Q + ω₀²)
 *
 * where: ω₀ = 1/√(R·R2·C1·C2)
 *        Q = Rq/√(R·R2) · √(C1/C2)
 *        (simplified: with R=R2, C1=C2=C: ω₀=1/(R·C), Q=Rq/R)
 *
 * The Tow-Thomas design is particularly elegant with equal-R, equal-C:
 *   R = R2 = integrator R
 *   C1 = C2 = C
 *   ω₀ = 1/(R·C)
 *   Q = Rq / R
 *   LP gain = -R2/R1 = -1 (for R1=R)
 *   BP gain = -Rq/R1 = -Q (for R1=R)
 *
 * Reference: L.C. Thomas, "The Biquad: Part I — Some Practical Design
 *            Considerations," IEEE Trans. CT, vol. CT-18, 1971.
 */
int tow_thomas_biquad_design(double f0, double q, double gain_db,
                              double c_scale, active_component_values_t *comp)
{
    if (!comp || f0 <= 0.0 || q <= 0.0 || c_scale <= 0.0)
        return ACTIVE_ERR_NULL_PTR;

    double w0 = 2.0 * M_PI * f0;
    double gain_lin = pow(10.0, gain_db / 20.0);

    /* Equal-C, equal-R variant */
    double c = c_scale;
    double r = 1.0 / (w0 * c);
    double rq = q * r;        /* Q-setting resistor */
    double r1 = r / gain_lin; /* Input R sets gain: K = R2/R1 = R/R1 */

    comp->r1 = r1;   /* Input resistor */
    comp->r2 = r;    /* Feedback R in damped integrator */
    comp->r3 = rq;   /* Q-setting resistor */
    comp->r4 = r;    /* Second integrator R */
    comp->r5 = 0.0;
    comp->c1 = c;    /* Damped integrator C */
    comp->c2 = c;    /* Lossless integrator C */
    comp->c3 = 0.0;

    return ACTIVE_OK;
}

/**
 * tow_thomas_tunable_q — Tow-Thomas with potentiometer Q adjustment.
 *
 * L5: Q = Rq/R can be adjusted by making Rq a potentiometer.
 * Since ω₀ depends on R and C, not Rq, Q tuning is independent
 * of ω₀ — a key advantage.
 *
 * Rq = Rq_min (fixed) + Rq_pot (potentiometer)
 * Q_min = Rq_min / R
 * Q_max = (Rq_min + Rq_pot) / R
 */
int tow_thomas_tunable_q(double f0, double q_min, double q_max,
                          double gain_db, double c_scale,
                          active_component_values_t *comp)
{
    if (!comp || f0 <= 0.0 || q_min <= 0.0 || q_max <= q_min)
        return ACTIVE_ERR_NULL_PTR;

    double w0 = 2.0 * M_PI * f0;
    double c = c_scale;
    double r = 1.0 / (w0 * c);

    double rq_fixed = q_min * r;
    double rq_pot = (q_max - q_min) * r;

    double gain_lin = pow(10.0, gain_db / 20.0);
    double r1 = r / gain_lin;

    comp->r1 = r1;
    comp->r2 = r;
    comp->r3 = rq_fixed;  /* Fixed portion of Rq */
    comp->r4 = r;
    comp->r5 = rq_pot;    /* Potentiometer portion (stored in R5) */
    comp->c1 = c;
    comp->c2 = c;
    comp->c3 = 0.0;

    return ACTIVE_OK;
}

/**
 * tow_thomas_compute_outputs — Compute Tow-Thomas LP and BP outputs.
 *
 * L3: LP output from second integrator, BP from first (damped) integrator.
 * AP output requires summing LP, BP, and HP (from amplifier input).
 */
int tow_thomas_compute_outputs(const active_component_values_t *comp,
                                double lp_num[3], double bp_num[3],
                                double den[3], double *lp_gain,
                                double *bp_gain)
{
    if (!comp || !lp_num || !bp_num || !den)
        return ACTIVE_ERR_NULL_PTR;

    double r1 = comp->r1, r = comp->r2, rq = comp->r3;
    double c = comp->c1;

    if (r1 <= 0 || r <= 0 || c <= 0) return ACTIVE_ERR_COMPONENT_RANGE;

    double w0 = 1.0 / (r * c);
    double q = rq / r;

    /* Common denominator */
    den[0] = 1.0;
    den[1] = w0 / q;
    den[2] = w0 * w0;

    /* LP numerator: -(R/R1)·ω₀² */
    lp_num[0] = 0.0;
    lp_num[1] = 0.0;
    lp_num[2] = -(r / r1) * w0 * w0;

    /* BP numerator: -(Rq/R1)·(ω₀/Q)·s */
    bp_num[0] = 0.0;
    bp_num[1] = -(rq / r1) * w0 / q;
    bp_num[2] = 0.0;

    *lp_gain = r / r1;
    *bp_gain = rq / r1;

    return ACTIVE_OK;
}

/* ==================================================================
 * L5: Akerberg-Mossberg Single-Op-Amp Biquad
 * ================================================================== */

/**
 * akerberg_mossberg_design — Single-op-amp biquadratic filter.
 *
 * L5: The Akerberg-Mossberg topology realizes a complete biquad
 * using one op-amp and five admittances (RC networks). It's more
 * component-efficient than the 3-amp biquads but has higher
 * sensitivity.
 *
 * General transfer function (admittance form):
 *   H(s) = -Y1·Y3 / [Y5·(Y1+Y2+Y3+Y4) + Y3·Y4]
 *
 * By choosing appropriate RC combinations for Y1-Y5, all filter
 * types (LP, HP, BP, BS, AP) can be realized.
 *
 * For LP: Y1=R1, Y2=sC1, Y3=R2, Y4=sC2, Y5=R3
 *   H_LP(s) = -(R2/R1) / (s²·R2·R3·C1·C2 + s·R3·(C1+C2) + 1)
 *   This is actually the MFB LP in disguise!
 *
 * The Akerberg-Mossberg general form encompasses MFB (LP, HP, BP)
 * as special cases, plus additional configurations.
 *
 * Reference: Akerberg & Mossberg, "A Versatile Active RC Building
 *            Block with Inherent Compensation for the Finite
 *            Bandwidth of the Amplifier," IEEE Trans. CAS,
 *            vol. CAS-21, pp. 75-78, 1974.
 */
int akerberg_mossberg_design(active_filter_func_t function,
                              double f0, double q, double gain_db,
                              double r_scale, active_component_values_t *comp)
{
    if (!comp || f0 <= 0.0 || q <= 0.0 || r_scale <= 0.0)
        return ACTIVE_ERR_NULL_PTR;

    double w0 = 2.0 * M_PI * f0;
    double gain_lin = pow(10.0, gain_db / 20.0);

    (void)function;  /* Function type selection for non-LP preserved for extension */

    /* For LP: implement as MFB (Y1=R1, Y2=C1, Y3=R2, Y4=C2, Y5=R3)
     * This is the canonical AM LP realization. */
    double c2 = 1.0 / (w0 * r_scale);
    double c1 = 4.0 * q * q * c2;

    double r2 = 1.0 / (2.0 * w0 * q * c2);
    double r1 = r2 / gain_lin;
    double r3 = 1.0 / (w0 * w0 * r2 * c1 * c2);

    comp->r1 = r1;  /* Y1 = 1/R1 */
    comp->r2 = r2;  /* Y3 = 1/R2 */
    comp->r3 = r3;  /* Y5 = 1/R3 */
    comp->r4 = 0.0;
    comp->r5 = 0.0;
    comp->c1 = c1;  /* Y2 = sC1 */
    comp->c2 = c2;  /* Y4 = sC2 */
    comp->c3 = 0.0;

    return ACTIVE_OK;
}
