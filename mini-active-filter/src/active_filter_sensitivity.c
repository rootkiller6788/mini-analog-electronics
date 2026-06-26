/**
 * active_filter_sensitivity.c — Sensitivity Analysis Implementation
 *
 * L3-L4: Component sensitivity computation, Monte Carlo tolerance
 * analysis, and op-amp non-ideality modeling for active filters.
 *
 * Sensitivity analysis is critical for active filter design because:
 * - Active filters use positive feedback to enhance Q
 * - Q sensitivity grows with Q in Sallen-Key topologies
 * - Passive LC filters have inherently low sensitivity
 * - KHN and Tow-Thomas match LC ladder sensitivity
 *
 * Key theorems:
 * - Sensitivity sum theorem: Σ S_Ri^ω₀ + Σ S_Ci^ω₀ = -N_C
 * - Q sensitivity sum: Σ S_xi^Q = 0
 * - Fialkow-Gerst dimensional homogeneity constraint
 *
 * Reference:
 *   Snelgrove, "Synthesis and Analysis of Active RC Filters,"
 *   in Schaumann & Van Valkenburg (2001), Ch. 15
 *   Sedra & Brackett, Filter Theory and Design: Active and
 *   Passive (1978), Ch. 8
 */

#include "active_filter_sensitivity.h"
#include "active_filter_sallen_key.h"
#include "active_filter_mfb.h"
#include "active_filter_state_variable.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ==================================================================
 * L3: Analytical Sensitivity Computation
 * ================================================================== */

/**
 * sensitivity_sallen_key — Sallen-Key component sensitivities.
 *
 * L3: Derived analytically from the transfer function.
 *
 * For Sallen-Key LP with unity gain (K=1, R1=R2=R, C1=C2=C):
 *   ω₀ = 1/(R·C)
 *   Q = 1/(2 + (1-K)) = 0.5 (limited!)
 *
 * For K > 1 (general Sallen-Key):
 *   ω₀ = 1/√(R1·R2·C1·C2)
 *   Q = √(C1·C2·R1·R2) / (R1·C1 + R2·C2 + R1·C2·(1-K))
 *
 * Sensitivities (derived via partial differentiation):
 *   S_R1^ω₀ = S_R2^ω₀ = S_C1^ω₀ = S_C2^ω₀ = -0.5
 *   S_R1^Q = -0.5 + Q·(1 + R2/R1 + C1·(1-K)/C2)
 *   S_R2^Q = -0.5 + Q·(1 + R1/R2)
 *   S_C1^Q =  0.5 - Q·(1 + R2·C2/(R1·C1))
 *   S_C2^Q =  0.5 - Q·(1 + R1·C1/(R2·C2) + R1·(1-K)/R2)
 *
 * For equal-R, equal-C, K > 1:
 *   S_R^Q = Q·(3-K) - 0.5
 *   S_C^Q = 0.5 - Q·(3-K)
 *
 * With K = 3 - 1/Q (Butterworth condition):
 *   S_R^Q = Q·(1/Q) - 0.5 = 0.5
 *   S_C^Q = 0.5 - Q·(1/Q) = -0.5
 *
 * This shows that properly designed Sallen-Key Butterworth filters
 * have S_Q = ±0.5 — excellent! But the K must be exactly right.
 */
int sensitivity_sallen_key(const active_component_values_t *comp,
                            active_filter_func_t function,
                            active_sensitivity_t *sens)
{
    if (!comp || !sens) return ACTIVE_ERR_NULL_PTR;

    double r1 = comp->r1, r2 = comp->r2;
    double c1 = comp->c1, c2 = comp->c2;
    double k = 1.0;

    if (comp->r4 > 0 && comp->r3 > 0)
        k = 1.0 + comp->r4 / comp->r3;
    else if (comp->r4 > 0)
        k = 2.0;  /* estimate */

    if (r1 <= 0 || r2 <= 0 || c1 <= 0 || c2 <= 0) {
        memset(sens, 0, sizeof(active_sensitivity_t));
        sens->worst_case = 1e10;
        return ACTIVE_ERR_COMPONENT_RANGE;
    }

    /* ω₀ sensitivities — universal for any RC topology */
    sens->s_r1_f0 = -0.5;
    sens->s_r2_f0 = -0.5;
    sens->s_c1_f0 = -0.5;
    sens->s_c2_f0 = -0.5;

    if (function == ACTIVE_FUNC_LOWPASS) {
        /* Denominator coefficient a1 = R1·C1 + R2·C2 + R1·C2·(1-K) */
        double a1 = r1*c1 + r2*c2 + r1*c2*(1.0 - k);

        /* Q = ω₀·denom²/a1 = 1/(ω₀·a1)... let's use numerical approach */
        /* Use the extracted parameters for exact Q */
        double wp, qp, gain_val;
        int ret = sallen_key_extract_params(comp, function, &wp, &qp, &gain_val);
        if (ret != ACTIVE_OK) return ret;

        double q = qp;
        double w0 = wp;

        /* Q sensitivities (from transfer function partial derivatives) */
        sens->s_r1_q = -0.5 + q * w0 * (r1*c1 + r1*c2*(1.0-k)) / a1;
        sens->s_r2_q = -0.5 + q * w0 * r2 * c2 / a1;
        sens->s_c1_q =  0.5 - q * w0 * r1 * c1 / a1;
        sens->s_c2_q =  0.5 - q * w0 * (r2*c2 + r1*(1.0-k)) * c2 / (a1*c2);
        /* Simplified approximations for equal-R, equal-C case: */
        if (fabs(r1-r2)/r1 < 0.01 && fabs(c1-c2)/c1 < 0.01) {
            sens->s_r1_q =  0.5 - q * (k - 2.0);
            sens->s_r2_q = -0.5 - q * (k - 2.0);
            sens->s_c1_q =  0.5;
            sens->s_c2_q = -0.5;
        }

        sens->s_r3_q = (comp->r3 > 0) ? -q * (k - 1.0) / k : 0.0;
        sens->s_r4_q = (comp->r4 > 0) ?  q * (k - 1.0) / k : 0.0;

    } else if (function == ACTIVE_FUNC_HIGHPASS) {
        /* HP: dual of LP, swap R↔C */
        double wp, qp, gain_val;
        int ret = sallen_key_extract_params(comp, function, &wp, &qp, &gain_val);
        if (ret != ACTIVE_OK) return ret;

        double q = qp;
        sens->s_r1_q = -0.5;
        sens->s_r2_q =  0.5;
        sens->s_c1_q = -0.5 + q;
        sens->s_c2_q =  0.5;
        sens->s_r3_q = 0.0;
        sens->s_r4_q = 0.0;
    } else {
        /* BP: approximated */
        sens->s_r1_q = -0.5;
        sens->s_r2_q = -0.5;
        sens->s_c1_q =  0.5;
        sens->s_c2_q =  0.5;
        sens->s_r3_q = 0.0;
        sens->s_r4_q = 0.0;
    }

    /* Find worst-case sensitivity */
    double worst = 0.0;
    double abs_vals[] = {
        fabs(sens->s_r1_q), fabs(sens->s_r2_q), fabs(sens->s_r3_q),
        fabs(sens->s_r4_q), fabs(sens->s_c1_q), fabs(sens->s_c2_q),
        fabs(sens->s_r1_f0), fabs(sens->s_r2_f0),
        fabs(sens->s_c1_f0), fabs(sens->s_c2_f0)
    };
    for (int i = 0; i < 10; i++) {
        if (abs_vals[i] > worst) worst = abs_vals[i];
    }
    sens->worst_case = worst;

    return ACTIVE_OK;
}

/**
 * sensitivity_mfb — MFB topology component sensitivities.
 *
 * L3: MFB generally has lower Q sensitivity than Sallen-Key.
 *
 * For MFB LP with C1/C2 >> 1 (which is typical for Q > 1):
 *   S_C1^Q ≈ 0.5
 *   S_C2^Q ≈ -0.5
 *   S_R3^Q ≈ 0.5
 *
 * The capacitor ratio C1/C2 is the critical parameter for Q.
 * Unlike Sallen-Key, MFB Q sensitivity does NOT grow linearly with Q.
 */
int sensitivity_mfb(const active_component_values_t *comp,
                     active_filter_func_t function,
                     active_sensitivity_t *sens)
{
    if (!comp || !sens) return ACTIVE_ERR_NULL_PTR;

    double r1 = comp->r1, r2 = comp->r2, r3 = comp->r3;
    double c1 = comp->c1, c2 = comp->c2;

    if (r1 <= 0 || r2 <= 0 || c1 <= 0 || c2 <= 0) {
        memset(sens, 0, sizeof(active_sensitivity_t));
        sens->worst_case = 1e10;
        return ACTIVE_ERR_COMPONENT_RANGE;
    }

    /* ω₀ sensitivities (universal for RC) */
    sens->s_r1_f0 = 0.0;  /* R1 doesn't affect ω₀ in MFB */
    sens->s_r2_f0 = -0.5;
    sens->s_r3_f0 = (r3 > 0) ? -0.5 : 0.0;
    sens->s_c1_f0 = -0.5;
    sens->s_c2_f0 = -0.5;

    /* Q sensitivities for MFB LP */
    if (function == ACTIVE_FUNC_LOWPASS) {
        double sqrt_ratio = sqrt(c1 / c2);
        double g_sum = 1.0/r1 + 1.0/r2 + (r3 > 0 ? 1.0/r3 : 0.0);

        sens->s_r1_q = (r1 > 0) ? -0.5 + 1.0/(r1 * g_sum) : 0.0;
        sens->s_r2_q = -0.5 + 1.0/(r2 * g_sum);
        sens->s_r3_q = (r3 > 0) ? -0.5 + 1.0/(r3 * g_sum) : 0.0;
        sens->s_c1_q =  0.5 - sqrt_ratio;
        sens->s_c2_q = -0.5 + sqrt_ratio;
        sens->s_r4_q = 0.0;

    } else if (function == ACTIVE_FUNC_BANDPASS) {
        /* MFB BP sensitivities */
        sens->s_r1_q = -0.5;
        sens->s_r2_q = -0.5;
        sens->s_r3_q =  0.5;
        sens->s_c1_q =  0.5 - 1.0;
        sens->s_c2_q = -0.5 + 1.0;
        sens->s_r4_q = 0.0;

    } else {
        memset(sens, 0, sizeof(active_sensitivity_t));
    }

    /* Worst case */
    double worst = 0.0;
    double vals[] = {
        fabs(sens->s_r1_q), fabs(sens->s_r2_q), fabs(sens->s_r3_q),
        fabs(sens->s_c1_q), fabs(sens->s_c2_q),
        fabs(sens->s_r1_f0), fabs(sens->s_r2_f0), fabs(sens->s_r3_f0),
        fabs(sens->s_c1_f0), fabs(sens->s_c2_f0)
    };
    for (int i = 0; i < 10; i++) {
        if (vals[i] > worst) worst = vals[i];
    }
    sens->worst_case = worst;

    return ACTIVE_OK;
}

/**
 * sensitivity_khn — KHN state-variable topology sensitivities.
 *
 * L3: The KHN has the BEST sensitivity properties of all
 * multi-op-amp topologies.
 *
 * Key result: S_Rq^Q = 1 (only one component affects Q!)
 * All other S_xi^Q ≤ 0.5 (bounded, independent of Q value!)
 *
 * This means KHN Q sensitivity does NOT increase with Q —
 * a unique and highly desirable property.
 */
int sensitivity_khn(const active_component_values_t *comp,
                     active_sensitivity_t *sens)
{
    if (!comp || !sens) return ACTIVE_ERR_NULL_PTR;

    /* ω₀ sensitivities: depends only on integrator R and C (verified via theory) */
    sens->s_r1_f0 = -0.5;  /* Integrator R */
    sens->s_r2_f0 =  0.0;  /* Q-setting R doesn't affect ω₀ */
    sens->s_r3_f0 =  0.0;  /* Input R doesn't affect ω₀ */
    sens->s_r4_f0 =  0.0;  /* Feedback R doesn't affect ω₀ */
    sens->s_c1_f0 = -0.5;  /* Integrator C */
    sens->s_c2_f0 = -0.5;  /* Second integrator C */

    /* Q sensitivities — the key advantage of KHN */
    /* Q = Rq/R_int, so S_Rq^Q = 1, S_Rint^Q = -1 */
    /* But from the full transfer function:
     *   S_Rq^Q ≈ 1 (Q-setting resistor)
     *   S_R^Q ≈ -0.5 (integrator resistor)
     *   S_C^Q ≈ 0.5 (integrator capacitor)
     *
     * All Q sensitivities are BOUNDED — they do NOT grow with Q! */
    sens->s_r1_q = -0.5;   /* Integrator R affects Q slightly */
    sens->s_r2_q =  1.0;   /* Q-setting R directly controls Q */
    sens->s_r3_q =  0.0;   /* Input R doesn't affect Q */
    sens->s_r4_q =  0.0;   /* Feedback R doesn't affect Q */
    sens->s_c1_q =  0.5;   /* Integrator C */
    sens->s_c2_q =  0.5;   /* Second integrator C */

    sens->worst_case = 1.0;  /* Max |S| = 1 for Q-setting resistor */

    return ACTIVE_OK;
}

/**
 * sensitivity_tow_thomas — Tow-Thomas biquad sensitivities.
 *
 * L3: Similar to KHN — Q sensitivity is bounded.
 * S_Rq^Q = 1, all other S ≤ 0.5.
 */
int sensitivity_tow_thomas(const active_component_values_t *comp,
                            active_sensitivity_t *sens)
{
    if (!comp || !sens) return ACTIVE_ERR_NULL_PTR;

    /* Using theoretical values for Tow-Thomas topology */
    sens->s_r1_f0 =  0.0;   /* Input R */
    sens->s_r2_f0 = -0.5;   /* Feedback R */
    sens->s_r3_f0 =  0.0;   /* Q-setting R */
    sens->s_r4_f0 = -0.5;   /* Integrator R */
    sens->s_c1_f0 = -0.5;
    sens->s_c2_f0 = -0.5;

    sens->s_r1_q =  0.0;
    sens->s_r2_q = -0.5;
    sens->s_r3_q =  1.0;   /* Q = Rq/R */
    sens->s_r4_q = -0.5;
    sens->s_c1_q =  0.5;
    sens->s_c2_q =  0.5;

    sens->worst_case = 1.0;

    return ACTIVE_OK;
}

/**
 * sensitivity_generic — Numerical sensitivity via perturbation.
 *
 * L3: When analytical formulas aren't available, compute
 * sensitivity by perturbing each component and measuring the
 * effect on ω₀ and Q.
 */
int sensitivity_generic(const active_component_values_t *comp,
                         active_topology_t topology,
                         active_filter_func_t function,
                         active_sensitivity_t *sens, double delta)
{
    if (!comp || !sens) return ACTIVE_ERR_NULL_PTR;

    /* Get nominal parameters */
    double wp_nom, qp_nom, gain_nom;

    /* Select the right extraction function based on topology */
    int ret;
    switch (topology) {
    case ACTIVE_TOPO_SALLEN_KEY:
        ret = sallen_key_extract_params(comp, function, &wp_nom, &qp_nom, &gain_nom);
        break;
    case ACTIVE_TOPO_MFB:
        ret = mfb_extract_params(comp, function, &wp_nom, &qp_nom, &gain_nom);
        break;
    default:
        return ACTIVE_ERR_NOT_IMPL;
    }
    if (ret != ACTIVE_OK) return ret;

    /* Perturb each resistor and compute sensitivity */
    double r1_orig = comp->r1, r2_orig = comp->r2;
    double r3_orig = comp->r3, r4_orig = comp->r4;
    double c1_orig = comp->c1, c2_orig = comp->c2;

    active_component_values_t pert = *comp;

    /* S_R1 */
    if (r1_orig > 0) {
        pert.r1 = r1_orig * (1.0 + delta);
        double wp_p, qp_p, g_p;
        if (topology == ACTIVE_TOPO_SALLEN_KEY)
            sallen_key_extract_params(&pert, function, &wp_p, &qp_p, &g_p);
        else
            mfb_extract_params(&pert, function, &wp_p, &qp_p, &g_p);
        sens->s_r1_f0 = (wp_p - wp_nom) / (wp_nom * delta);
        sens->s_r1_q  = (qp_p - qp_nom) / (qp_nom * delta);
        pert.r1 = r1_orig;
    } else {
        sens->s_r1_f0 = sens->s_r1_q = 0.0;
    }

    /* S_R2 */
    if (r2_orig > 0) {
        pert.r2 = r2_orig * (1.0 + delta);
        double wp_p, qp_p, g_p;
        if (topology == ACTIVE_TOPO_SALLEN_KEY)
            sallen_key_extract_params(&pert, function, &wp_p, &qp_p, &g_p);
        else
            mfb_extract_params(&pert, function, &wp_p, &qp_p, &g_p);
        sens->s_r2_f0 = (wp_p - wp_nom) / (wp_nom * delta);
        sens->s_r2_q  = (qp_p - qp_nom) / (qp_nom * delta);
        pert.r2 = r2_orig;
    } else { sens->s_r2_f0 = sens->s_r2_q = 0.0; }

    /* S_R3 */
    if (r3_orig > 0) {
        pert.r3 = r3_orig * (1.0 + delta);
        double wp_p, qp_p, g_p;
        if (topology == ACTIVE_TOPO_SALLEN_KEY)
            sallen_key_extract_params(&pert, function, &wp_p, &qp_p, &g_p);
        else
            mfb_extract_params(&pert, function, &wp_p, &qp_p, &g_p);
        sens->s_r3_f0 = (wp_p - wp_nom) / (wp_nom * delta);
        sens->s_r3_q  = (qp_p - qp_nom) / (qp_nom * delta);
        pert.r3 = r3_orig;
    } else { sens->s_r3_f0 = sens->s_r3_q = 0.0; }

    /* S_R4 */
    if (r4_orig > 0) {
        pert.r4 = r4_orig * (1.0 + delta);
        double wp_p, qp_p, g_p;
        if (topology == ACTIVE_TOPO_SALLEN_KEY)
            sallen_key_extract_params(&pert, function, &wp_p, &qp_p, &g_p);
        else
            mfb_extract_params(&pert, function, &wp_p, &qp_p, &g_p);
        sens->s_r4_f0 = (wp_p - wp_nom) / (wp_nom * delta);
        sens->s_r4_q  = (qp_p - qp_nom) / (qp_nom * delta);
        pert.r4 = r4_orig;
    } else { sens->s_r4_f0 = sens->s_r4_q = 0.0; }

    /* S_C1 */
    if (c1_orig > 0) {
        pert.c1 = c1_orig * (1.0 + delta);
        double wp_p, qp_p, g_p;
        if (topology == ACTIVE_TOPO_SALLEN_KEY)
            sallen_key_extract_params(&pert, function, &wp_p, &qp_p, &g_p);
        else
            mfb_extract_params(&pert, function, &wp_p, &qp_p, &g_p);
        sens->s_c1_f0 = (wp_p - wp_nom) / (wp_nom * delta);
        sens->s_c1_q  = (qp_p - qp_nom) / (qp_nom * delta);
        pert.c1 = c1_orig;
    } else { sens->s_c1_f0 = sens->s_c1_q = 0.0; }

    /* S_C2 */
    if (c2_orig > 0) {
        pert.c2 = c2_orig * (1.0 + delta);
        double wp_p, qp_p, g_p;
        if (topology == ACTIVE_TOPO_SALLEN_KEY)
            sallen_key_extract_params(&pert, function, &wp_p, &qp_p, &g_p);
        else
            mfb_extract_params(&pert, function, &wp_p, &qp_p, &g_p);
        sens->s_c2_f0 = (wp_p - wp_nom) / (wp_nom * delta);
        sens->s_c2_q  = (qp_p - qp_nom) / (qp_nom * delta);
    } else { sens->s_c2_f0 = sens->s_c2_q = 0.0; }

    /* Worst case */
    double worst = 0.0;
    double vals[] = {
        fabs(sens->s_r1_q), fabs(sens->s_r2_q), fabs(sens->s_r3_q),
        fabs(sens->s_r4_q), fabs(sens->s_c1_q), fabs(sens->s_c2_q),
        fabs(sens->s_r1_f0), fabs(sens->s_r2_f0), fabs(sens->s_r3_f0),
        fabs(sens->s_r4_f0), fabs(sens->s_c1_f0), fabs(sens->s_c2_f0)
    };
    for (int i = 0; i < 12; i++) {
        if (vals[i] > worst) worst = vals[i];
    }
    sens->worst_case = worst;
    sens->s_r3_q = sens->s_r3_q;  /* ensure stored */

    return ACTIVE_OK;
}

/* ==================================================================
 * L4: Sensitivity Sum Theorems
 * ================================================================== */

/**
 * sensitivity_sum_theorem — Verify ω₀ sensitivity sum invariance.
 *
 * L4 Theorem: For any active RC filter, Σ S_{R,C}^ω₀ = -N_C
 * where N_C is the number of capacitors.
 *
 * This follows from dimensional analysis: ω₀ ∝ 1/(RC) for each
 * RC product. Each factor of R contributes -0.5, each factor of
 * C contributes -0.5. Since a second-order section has ω₀ ∝ 1/√(RC),
 * the sum is Σ(-0.5) for all R,C = -2 for a biquad with 2R+2C.
 */
int sensitivity_sum_theorem(const active_component_values_t *comp,
                             active_topology_t topology,
                             active_filter_func_t function,
                             double *sum, double *expected)
{
    if (!comp || !sum || !expected) return ACTIVE_ERR_NULL_PTR;

    active_sensitivity_t sens;
    int ret;

    switch (topology) {
    case ACTIVE_TOPO_SALLEN_KEY:
        ret = sensitivity_sallen_key(comp, function, &sens);
        break;
    case ACTIVE_TOPO_MFB:
        ret = sensitivity_mfb(comp, function, &sens);
        break;
    case ACTIVE_TOPO_STATE_VARIABLE:
        ret = sensitivity_khn(comp, &sens);
        break;
    case ACTIVE_TOPO_BIQUAD:
        ret = sensitivity_tow_thomas(comp, &sens);
        break;
    default:
        ret = sensitivity_generic(comp, topology, function, &sens, 0.001);
        break;
    }

    if (ret != ACTIVE_OK) return ret;

    /* Sum of all ω₀ sensitivities */
    *sum = sens.s_r1_f0 + sens.s_r2_f0 + sens.s_r3_f0 + sens.s_r4_f0 +
           sens.s_c1_f0 + sens.s_c2_f0;

    /* Expected sum = -N_C where N_C = number of capacitors used */
    int num_caps = 0;
    if (comp->c1 > 0) num_caps++;
    if (comp->c2 > 0) num_caps++;
    *expected = -(double)num_caps;

    return ACTIVE_OK;
}

/**
 * sensitivity_q_sum_theorem — Verify Q sensitivity sum.
 *
 * L4 Theorem: Σ S_xi^Q = 0 for any biquadratic active filter.
 *
 * This follows from the bilinear relationship between component
 * values and the quality factor. The proof involves showing that
 * Q is a homogeneous function of degree 0 in the component values.
 */
int sensitivity_q_sum_theorem(const active_component_values_t *comp,
                               active_topology_t topology,
                               active_filter_func_t function,
                               double *sum)
{
    if (!comp || !sum) return ACTIVE_ERR_NULL_PTR;

    active_sensitivity_t sens;
    int ret;

    switch (topology) {
    case ACTIVE_TOPO_SALLEN_KEY:
        ret = sensitivity_sallen_key(comp, function, &sens);
        break;
    case ACTIVE_TOPO_MFB:
        ret = sensitivity_mfb(comp, function, &sens);
        break;
    case ACTIVE_TOPO_STATE_VARIABLE:
        ret = sensitivity_khn(comp, &sens);
        break;
    case ACTIVE_TOPO_BIQUAD:
        ret = sensitivity_tow_thomas(comp, &sens);
        break;
    default:
        ret = sensitivity_generic(comp, topology, function, &sens, 0.001);
        break;
    }

    if (ret != ACTIVE_OK) return ret;

    *sum = sens.s_r1_q + sens.s_r2_q + sens.s_r3_q + sens.s_r4_q +
           sens.s_c1_q + sens.s_c2_q;

    return ACTIVE_OK;
}

/* ==================================================================
 * L5: Monte Carlo Tolerance Analysis
 * ================================================================== */

/**
 * monte_carlo_tolerance — Statistical yield analysis.
 *
 * L5: Monte Carlo simulation of component tolerances.
 * Each component is perturbed by Gaussian noise with σ = tolerance/3
 * (matches ±3σ = tolerance range for ±3σ manufacturing).
 *
 * Returns mean and std of ω₀ and Q, plus estimated yield.
 */
int monte_carlo_tolerance(const active_component_values_t *comp,
                           active_topology_t topology,
                           active_filter_func_t function,
                           double tolerance_pct, int num_trials,
                           double *wp_mean, double *wp_std,
                           double *q_mean, double *q_std,
                           double *yield_pct)
{
    if (!comp || !wp_mean || !wp_std || !q_mean || !q_std || !yield_pct)
        return ACTIVE_ERR_NULL_PTR;

    double sigma = tolerance_pct / 3.0 / 100.0;  /* Fractional σ */

    /* Get nominal parameters */
    double wp_nom, qp_nom, gain_nom;
    int ret;
    if (topology == ACTIVE_TOPO_SALLEN_KEY)
        ret = sallen_key_extract_params(comp, function, &wp_nom, &qp_nom, &gain_nom);
    else if (topology == ACTIVE_TOPO_MFB)
        ret = mfb_extract_params(comp, function, &wp_nom, &qp_nom, &gain_nom);
    else
        return ACTIVE_ERR_NOT_IMPL;
    if (ret != ACTIVE_OK) return ret;

    /* Accumulators */
    double sum_wp = 0.0, sum_wp2 = 0.0;
    double sum_q = 0.0, sum_q2 = 0.0;
    int in_spec = 0;

    /* Simple pseudo-random Gaussian via Box-Muller */
    /* Seed with constant for reproducibility */
    unsigned int seed = 12345;

    for (int trial = 0; trial < num_trials; trial++) {
        active_component_values_t pert = *comp;

        /* Gaussian perturbation for each component */
        double g1, g2;
        /* Box-Muller on each pair of components */
        /* R1 */
        seed = seed * 1103515245 + 12345;
        double u1 = (double)(seed & 0x7FFFFFFF) / 0x7FFFFFFF;
        seed = seed * 1103515245 + 12345;
        double u2 = (double)(seed & 0x7FFFFFFF) / 0x7FFFFFFF;
        if (u1 > 1e-10) {
            g1 = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
            g2 = sqrt(-2.0 * log(u1)) * sin(2.0 * M_PI * u2);
        } else { g1 = g2 = 0.0; }

        if (comp->r1 > 0) pert.r1 *= (1.0 + g1 * sigma);
        if (comp->r2 > 0) pert.r2 *= (1.0 + g2 * sigma);

        /* Generate more Gaussian pairs */
        seed = seed * 1103515245 + 12345;
        u1 = (double)(seed & 0x7FFFFFFF) / 0x7FFFFFFF;
        seed = seed * 1103515245 + 12345;
        u2 = (double)(seed & 0x7FFFFFFF) / 0x7FFFFFFF;
        if (u1 > 1e-10) {
            g1 = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
            g2 = sqrt(-2.0 * log(u1)) * sin(2.0 * M_PI * u2);
        } else { g1 = g2 = 0.0; }

        if (comp->r3 > 0) pert.r3 *= (1.0 + g1 * sigma);
        if (comp->r4 > 0) pert.r4 *= (1.0 + g2 * sigma);

        seed = seed * 1103515245 + 12345;
        u1 = (double)(seed & 0x7FFFFFFF) / 0x7FFFFFFF;
        seed = seed * 1103515245 + 12345;
        u2 = (double)(seed & 0x7FFFFFFF) / 0x7FFFFFFF;
        if (u1 > 1e-10) {
            g1 = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
            g2 = sqrt(-2.0 * log(u1)) * sin(2.0 * M_PI * u2);
        } else { g1 = g2 = 0.0; }

        if (comp->c1 > 0) pert.c1 *= (1.0 + g1 * sigma);
        if (comp->c2 > 0) pert.c2 *= (1.0 + g2 * sigma);

        /* Extract parameters from perturbed components */
        double wp_p, qp_p, gain_p;
        if (topology == ACTIVE_TOPO_SALLEN_KEY)
            sallen_key_extract_params(&pert, function, &wp_p, &qp_p, &gain_p);
        else
            mfb_extract_params(&pert, function, &wp_p, &qp_p, &gain_p);

        sum_wp += wp_p;
        sum_wp2 += wp_p * wp_p;
        sum_q += qp_p;
        sum_q2 += qp_p * qp_p;

        /* Check if within spec (±tolerance on ω₀ and Q) */
        double wp_err = fabs(wp_p - wp_nom) / wp_nom;
        double qp_err = fabs(qp_p - qp_nom) / qp_nom;
        if (wp_err < tolerance_pct / 100.0 && qp_err < tolerance_pct / 100.0)
            in_spec++;
    }

    *wp_mean = sum_wp / num_trials;
    *wp_std = sqrt(sum_wp2 / num_trials - (*wp_mean) * (*wp_mean));
    *q_mean = sum_q / num_trials;
    *q_std = sqrt(sum_q2 / num_trials - (*q_mean) * (*q_mean));
    *yield_pct = 100.0 * in_spec / num_trials;

    return ACTIVE_OK;
}

/**
 * worst_case_analysis — Extreme value analysis.
 *
 * L5: Evaluates corner cases (each component at ±tolerance limit)
 * to find absolute worst-case ω₀, Q extremes.
 *
 * For N components: 2^N corners. With 6 components: 64 corners.
 */
int worst_case_analysis(const active_component_values_t *comp,
                         active_topology_t topology,
                         active_filter_func_t function,
                         double tolerance_pct,
                         double *wp_min, double *wp_max,
                         double *q_min, double *q_max)
{
    if (!comp || !wp_min || !wp_max || !q_min || !q_max)
        return ACTIVE_ERR_NULL_PTR;

    double tol = tolerance_pct / 100.0;

    /* Get nominal */
    double wp_nom, qp_nom, gain_nom;
    int ret;
    if (topology == ACTIVE_TOPO_SALLEN_KEY)
        ret = sallen_key_extract_params(comp, function, &wp_nom, &qp_nom, &gain_nom);
    else
        ret = mfb_extract_params(comp, function, &wp_nom, &qp_nom, &gain_nom);
    if (ret != ACTIVE_OK) return ret;

    *wp_min = *wp_max = wp_nom;
    *q_min = *q_max = qp_nom;

    /* Evaluate all 2^6 = 64 corner combinations */
    for (int corner = 0; corner < 64; corner++) {
        active_component_values_t pert = *comp;

        if (comp->r1 > 0) pert.r1 *= (1.0 + ((corner >> 0) & 1) ? tol : -tol);
        if (comp->r2 > 0) pert.r2 *= (1.0 + ((corner >> 1) & 1) ? tol : -tol);
        if (comp->r3 > 0) pert.r3 *= (1.0 + ((corner >> 2) & 1) ? tol : -tol);
        if (comp->r4 > 0) pert.r4 *= (1.0 + ((corner >> 3) & 1) ? tol : -tol);
        if (comp->c1 > 0) pert.c1 *= (1.0 + ((corner >> 4) & 1) ? tol : -tol);
        if (comp->c2 > 0) pert.c2 *= (1.0 + ((corner >> 5) & 1) ? tol : -tol);

        double wp_p, qp_p, gain_p;
        if (topology == ACTIVE_TOPO_SALLEN_KEY)
            sallen_key_extract_params(&pert, function, &wp_p, &qp_p, &gain_p);
        else
            mfb_extract_params(&pert, function, &wp_p, &qp_p, &gain_p);

        if (wp_p < *wp_min) *wp_min = wp_p;
        if (wp_p > *wp_max) *wp_max = wp_p;
        if (qp_p < *q_min) *q_min = qp_p;
        if (qp_p > *q_max) *q_max = qp_p;
    }

    return ACTIVE_OK;
}

/* ==================================================================
 * L4: Op-Amp Non-Ideality Effects
 * ================================================================== */

/**
 * gbw_effect_on_poles — Model finite GBW on filter poles.
 *
 * L4: For a single-pole op-amp model A(s) = GBW/s:
 *
 * First-order perturbation analysis (Budak & Petrela, 1972):
 *   ω₀_actual = ω₀_ideal · (1 - ω₀/(GBW·β))
 *   Q_actual  = Q_ideal · (1 + 2·Q·ω₀/GBW)
 *
 * where β is the feedback factor.
 *
 * For Sallen-Key: β ≈ 1/K (the non-inverting amplifier feedback)
 * For MFB: β = R1/(R1+R2) (the inverting amplifier feedback)
 * For KHN: β ≈ 0.5 (integrator feedback)
 *
 * Q-enhancement: finite GBW ALWAYS increases Q. This can cause
 * instability (oscillation) if Q_ideal · ω₀_ideal is close to GBW.
 *
 * Compensated by "predistortion": design for lower Q than desired.
 */
int gbw_effect_on_poles(double wp_ideal, double q_ideal, double gbw,
                          active_topology_t topology,
                          double *wp_actual, double *q_actual)
{
    if (!wp_actual || !q_actual || wp_ideal <= 0.0 || q_ideal <= 0.0)
        return ACTIVE_ERR_NULL_PTR;

    double gbw_rad = 2.0 * M_PI * gbw;

    /* Feedback factor β depends on topology */
    double beta;
    switch (topology) {
    case ACTIVE_TOPO_SALLEN_KEY:  beta = 0.5;  break;  /* Approximate */
    case ACTIVE_TOPO_MFB:          beta = 0.3;  break;
    case ACTIVE_TOPO_STATE_VARIABLE: beta = 0.5; break;
    case ACTIVE_TOPO_BIQUAD:       beta = 0.5; break;
    default:                       beta = 0.5; break;
    }

    /* Q-enhancement: ΔQ/Q ≈ 2·Q·ω₀/GBW_rad */
    double q_enhance = 2.0 * q_ideal * wp_ideal / gbw_rad;

    /* ω₀ shift: Δω₀/ω₀ ≈ -ω₀/(GBW_rad·β) */
    double wp_shift = wp_ideal / (gbw_rad * beta);

    *wp_actual = wp_ideal * (1.0 - wp_shift);
    *q_actual  = q_ideal * (1.0 + q_enhance);

    /* Check for excessive Q-enhancement */
    if (*q_actual > 100.0 || *q_actual < 0.0) {
        return ACTIVE_ERR_GBW_LIMIT;
    }

    return ACTIVE_OK;
}

/**
 * gbw_max_frequency — Maximum usable frequency given GBW constraint.
 *
 * L4: For a given Q tolerance (e.g., 10% Q error acceptable),
 * the maximum pole frequency is:
 *
 *   f_max = GBW / (2·Q·(Q_error_fraction))  (simplified)
 *
 * More precise: f_max = GBW · (Q_actual/Q_ideal - 1) / (2·Q_ideal)
 */
double gbw_max_frequency(double q_max_ratio, double gbw,
                          active_topology_t topology)
{
    (void)topology;  /* Topology scaling factor could be added */
    if (q_max_ratio <= 1.0 || gbw <= 0.0) return 0.0;

    return gbw * (q_max_ratio - 1.0) / (2.0 * q_max_ratio * q_max_ratio);
}

/**
 * slew_rate_max_frequency — Maximum frequency before slew limiting.
 *
 * L4: SR = 2π·f·Vpk → f_max = SR/(2π·Vpk)
 *
 * For active filters, the highest-swing output is typically the
 * BP output (which has Q·K times the input at center frequency).
 * Ensure SR is adequate at the maximum expected output amplitude.
 */
double slew_rate_max_frequency(double sr_v_us, double vpk)
{
    if (vpk <= 0.0 || sr_v_us <= 0.0) return 0.0;
    return (sr_v_us * 1e6) / (2.0 * M_PI * vpk);
}

/**
 * dc_offset_propagation — Accumulated DC offset through cascade.
 *
 * L4: Each op-amp contributes:
 * - Input offset voltage Vos (multiplied by DC gain of that stage)
 * - Input bias current Ib flowing through equivalent resistance Req
 *
 * Total output offset:
 *   Vos_total = Σ Vos_k · A_DC_k · (product of gains of all later stages)
 *
 * For unity-gain filters, Vos_total ≈ Σ Vos_k (roughly additive).
 * For filters with gain, offset is amplified.
 */
double dc_offset_propagation(const double *dc_gains, const double *vos_uv,
                              const double *ib_na, const double *req_ohm,
                              int num_stages)
{
    if (!dc_gains || !vos_uv || num_stages <= 0) return 0.0;

    double total_offset = 0.0;
    double cumulative_gain = 1.0;

    /* Work backwards from output */
    for (int i = num_stages - 1; i >= 0; i--) {
        double stage_offset_v = vos_uv[i] * 1e-6;  /* μV to V */
        if (ib_na && req_ohm) {
            stage_offset_v += ib_na[i] * 1e-9 * req_ohm[i];  /* nA·Ω = V */
        }
        total_offset += stage_offset_v * cumulative_gain;
        cumulative_gain *= dc_gains[i];
    }

    return total_offset;
}

/* ==================================================================
 * L8: Sensitivity Jacobian
 * ================================================================== */

/**
 * sensitivity_jacobian — Multi-parameter sensitivity matrix.
 *
 * L8: Computes the 2×N Jacobian matrix:
 *   J[0][i] = ∂ω₀/∂x_i · x_i/ω₀ = S_xi^ω₀
 *   J[1][i] = ∂Q/∂x_i · x_i/Q = S_xi^Q
 *
 * This matrix is used for:
 * - Optimal tuning: which component to trim for maximum effect
 * - Yield optimization: identify worst offenders
 * - Fault diagnosis: isolate faulty component from measured shifts
 */
int sensitivity_jacobian(const active_component_values_t *comp,
                          active_topology_t topology,
                          active_filter_func_t function,
                          double matrix[2][6])
{
    if (!comp || !matrix) return ACTIVE_ERR_NULL_PTR;

    active_sensitivity_t sens;
    int ret;

    switch (topology) {
    case ACTIVE_TOPO_SALLEN_KEY:
        ret = sensitivity_sallen_key(comp, function, &sens);
        break;
    case ACTIVE_TOPO_MFB:
        ret = sensitivity_mfb(comp, function, &sens);
        break;
    case ACTIVE_TOPO_STATE_VARIABLE:
        ret = sensitivity_khn(comp, &sens);
        break;
    case ACTIVE_TOPO_BIQUAD:
        ret = sensitivity_tow_thomas(comp, &sens);
        break;
    default:
        ret = sensitivity_generic(comp, topology, function, &sens, 0.001);
        break;
    }

    if (ret != ACTIVE_OK) return ret;

    /* Row 0: ω₀ sensitivities */
    matrix[0][0] = sens.s_r1_f0;
    matrix[0][1] = sens.s_r2_f0;
    matrix[0][2] = sens.s_r3_f0;
    matrix[0][3] = sens.s_r4_f0;
    matrix[0][4] = sens.s_c1_f0;
    matrix[0][5] = sens.s_c2_f0;

    /* Row 1: Q sensitivities */
    matrix[1][0] = sens.s_r1_q;
    matrix[1][1] = sens.s_r2_q;
    matrix[1][2] = sens.s_r3_q;
    matrix[1][3] = sens.s_r4_q;
    matrix[1][4] = sens.s_c1_q;
    matrix[1][5] = sens.s_c2_q;

    return ACTIVE_OK;
}
