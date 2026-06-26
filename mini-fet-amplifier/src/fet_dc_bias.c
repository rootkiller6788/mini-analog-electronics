/**
 * fet_dc_bias.c — FET DC Biasing: Q-Point Calculation and Analysis
 *
 * Implements all bias network topologies: fixed gate, self-bias,
 * voltage divider, current source. Uses Newton-Raphson iteration
 * for solving the nonlinear FET I-V equations at DC.
 *
 * L2 Core Concepts: Bias point establishment, stability
 * L5 Algorithms: Newton-Raphson solver, sensitivity analysis
 *
 * Reference: Sedra & Smith, "Microelectronic Circuits" 8th Ed., Ch.6
 *            Gray & Meyer, "Analysis and Design of Analog IC" 5th Ed., Ch.3
 *
 * MIT 6.002 / Berkeley EE105 / Stanford EE114
 */

#include "fet_dc_bias.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ─────────────────────────────────────────────────────────
 * Internal: Newton-Raphson iteration parameters
 * ───────────────────────────────────────────────────────── */

#define BIAS_MAX_ITER   100
#define BIAS_TOL_ID     1e-12   /* 1 pA tolerance */
#define BIAS_TOL_VDS    1e-9    /* 1 nV tolerance */
#define BIAS_DAMPING    0.5     /* Damping factor for convergence aid */

/* ─────────────────────────────────────────────────────────
 * DC Load Line
 * ───────────────────────────────────────────────────────── */

DcLoadLine fet_compute_load_line(double vdd, double vss, double rd, double rs,
                                  double rg1, double rg2)
{
    DcLoadLine ll;
    memset(&ll, 0, sizeof(ll));

    ll.vdd = vdd;
    ll.rd  = rd;
    ll.rs  = rs;
    ll.rg1 = rg1;
    ll.rg2 = rg2;

    /* Thevenin equivalent for gate */
    if (rg1 > 0.0 && rg2 > 0.0) {
        ll.rgg = (rg1 * rg2) / (rg1 + rg2);
        ll.vgg = vdd * rg2 / (rg1 + rg2);
    } else if (rg2 > 0.0 && rg1 <= 0.0) {
        ll.rgg = rg2;
        ll.vgg = vdd;
    } else {
        ll.rgg = 0.0;
        ll.vgg = 0.0;
    }

    /* Load line endpoints */
    double r_total = rd + rs;
    if (r_total > 0.0) {
        ll.id_max  = (vdd - vss) / r_total;
        ll.slope   = -1.0 / r_total;
    } else {
        ll.id_max  = INFINITY;
        ll.slope   = 0.0;
    }
    ll.vds_max = vdd - vss;

    return ll;
}

void fet_gate_equivalent(double vdd, double rg1, double rg2,
                         double *vgg, double *rgg)
{
    if (rg1 <= 0.0 && rg2 <= 0.0) {
        *vgg = 0.0;
        *rgg = 0.0;
        return;
    }
    if (rg1 <= 0.0) {
        *rgg = rg2;
        *vgg = vdd;
        return;
    }
    if (rg2 <= 0.0) {
        *rgg = rg1;
        *vgg = vdd;
        return;
    }
    *rgg = (rg1 * rg2) / (rg1 + rg2);
    *vgg = vdd * rg2 / (rg1 + rg2);
}

/* ─────────────────────────────────────────────────────────
 * Newton-Raphson: Solve Id for fixed Vgs
 *
 * Id = (1/2)*beta*(Vgs - Vth)^2*(1 + λ*Vds)
 * With Vds = Vdd - Id*(Rd + Rs), Vgs = Vgg - Id*Rs
 *
 * f(Id) = Id - 0.5*beta*(Vgg - Id*Rs - Vth)^2*(1 + λ*(Vdd - Id*(Rd+Rs))) = 0
 * f'(Id) computed analytically.
 * ───────────────────────────────────────────────────────── */

static double bias_f_id(double id, double beta, double vgg, double vth,
                         double lambda, double vdd, double rd, double rs)
{
    double vgs = vgg - id * rs;
    double vov = vgs - vth;
    if (vov <= 0.0) return id;  /* Cutoff: Id = 0 */
    double vds = vdd - id * (rd + rs);
    if (vds < 0.0) vds = 0.0;
    double term = 0.5 * beta * vov * vov * (1.0 + lambda * vds);
    return id - term;
}

static double bias_df_did(double id, double beta, double vgg, double vth,
                           double lambda, double vdd, double rd, double rs)
{
    double vgs = vgg - id * rs;
    double vov = vgs - vth;
    if (vov <= 0.0) return 1.0;
    double vds = vdd - id * (rd + rs);
    double r_total = rd + rs;

    /* d/dId of Id - 0.5*beta*(vgg - Id*Rs - Vth)^2*(1 + λ*(Vdd - Id*(Rd+Rs)))
     * = 1 - 0.5*beta*[ 2*(vov)*(-Rs)*(1+λ*vds) + vov^2*(-λ*r_total) ]
     */
    double term1 = 2.0 * vov * (-rs) * (1.0 + lambda * vds);
    double term2 = vov * vov * (-lambda * r_total);
    double derivative = 1.0 - 0.5 * beta * (term1 + term2);
    return derivative;
}

/**
 * Universal Newton-Raphson solver for FET bias point.
 * Solves the nonlinear system involving:
 *   Id = f(Vgs, Vds)  [FET I-V equation]
 *   Vgs = Vgg - Id*Rs  [gate-source loop]
 *   Vds = Vdd - Id*(Rd+Rs)  [drain-source loop]
 *
 * The single-variable formulation solves f(Id) = 0.
 *
 * Complexity: O(N) where N ≤ BIAS_MAX_ITER, typically converges in 5-10 steps
 * Convergence: quadratic near root (Newton's method property)
 */
static bool bias_newton_solve(double beta, double vth, double lambda,
                               double vgg, double vdd, double rd, double rs,
                               double *id_solution, uint32_t *iterations)
{
    /* Initial guess from linear approximation: Id = Vdd / (2*(Rd+Rs)) */
    double r_total = rd + rs;
    double id = (r_total > 0.0) ? vdd / (2.0 * r_total) : 1e-3;

    /* Check if Vgg is below Vth → cutoff */
    if (vgg <= vth && rs <= 0.0) {
        *id_solution = 0.0;
        *iterations = 1;
        return true;
    }

    for (uint32_t i = 0; i < BIAS_MAX_ITER; i++) {
        double f_val  = bias_f_id(id, beta, vgg, vth, lambda, vdd, rd, rs);
        double df_val = bias_df_did(id, beta, vgg, vth, lambda, vdd, rd, rs);

        if (fabs(df_val) < 1e-30) {
            /* Degenerate case: reset and perturb */
            id *= 1.1;
            continue;
        }

        double delta = f_val / df_val;
        /* Apply damping if step is large */
        if (fabs(delta) > 0.5 * id) {
            delta = (delta > 0 ? 1.0 : -1.0) * 0.5 * id;
        }
        id -= delta;

        if (id < 0.0) id = 0.0;  /* Physical: Id cannot be negative */

        if (fabs(delta) < BIAS_TOL_ID || (fabs(f_val) < BIAS_TOL_ID && i > 3)) {
            *id_solution = id;
            *iterations = i + 1;
            return true;
        }
    }

    /* Did not converge — return best estimate */
    *id_solution = (id >= 0.0) ? id : 0.0;
    *iterations = BIAS_MAX_ITER;
    return false;
}

/* ─────────────────────────────────────────────────────────
 * Fixed-Gate Bias
 * Vgs = Vgg (fixed), no source resistor feedback.
 * Simplest but worst stability.
 * ───────────────────────────────────────────────────────── */

BiasResult fet_bias_fixed_gate(const FetDevice *fet, const BiasSpec *spec)
{
    BiasResult result;
    memset(&result, 0, sizeof(result));
    result.converged = false;

    if (!fet) return result;

    double beta  = fet_beta(fet_kprime(fet->tech.mu_n, fet->tech.cox),
                            fet->geo.w_over_l);
    double vth   = fet->tech.vt_n;
    double lambda = fet->tech.lambda;
    double vgg   = spec->target_vgs;  /* Vgs directly set by gate voltage */
    double vdd   = spec->vdd;

    /* Solve: Id = 0.5*beta*(Vgg - Vth)^2*(1 + λ*(Vdd - Id*Rd)) */
    double rd = 0.0;  /* For fixed gate, typically no source resistor */
    double rs = 0.0;
    double id, vds;

    if (vgg <= vth) {
        id  = 0.0;
        vds = vdd;
    } else {
        /* Assume Rd set for target Vds: Rd = (Vdd - Vds)/Id */
        rd = (vdd - spec->target_vds) / spec->target_id;
        if (rd < 0.0) rd = 0.0;
        bool ok = bias_newton_solve(beta, vth, lambda, vgg, vdd, rd, rs,
                                     &id, &result.iterations);
        result.converged = ok;
        vds = vdd - id * rd;
    }

    /* Populate Q-point */
    result.qpoint.vgs = vgg;
    result.qpoint.vds = vds;
    result.qpoint.vbs = 0.0;
    result.qpoint.id  = id;
    result.qpoint.vov = vgg - vth;
    result.qpoint.gm  = (result.qpoint.vov > 0)
        ? beta * result.qpoint.vov * (1.0 + lambda * vds) : 0.0;
    result.qpoint.gds = (id > 0) ? lambda * id / (1.0 + lambda * vds) : 0.0;
    result.qpoint.vth_effective = vth;
    result.qpoint.vdsat = result.qpoint.vov;
    result.qpoint.region = fet_classify_region(vgg, vds, vth);
    result.qpoint.valid = true;

    /* Network */
    result.network.topology = BIAS_FIXED_GATE;
    result.network.vdd = vdd;
    result.network.rd  = rd;
    result.network.rs  = rs;

    /* Load line */
    result.load_line = fet_compute_load_line(vdd, 0.0, rd, rs, 0, 0);
    result.power_dc   = vdd * id;
    result.vout_swing = 2.0 * fmin(vds, vdd - vds);

    /* Sensitivity: ∂Id/∂Vth */
    double gm_local = result.qpoint.gm;
    if (gm_local > 0.0 && id > 0.0) {
        result.sensitivity_id = -(2.0 * id) / (result.qpoint.vov);
    } else {
        result.sensitivity_id = 0.0;
    }

    return result;
}

/* ─────────────────────────────────────────────────────────
 * Self-Bias (Source Degeneration)
 *
 * Gate at ground through Rg. Source resistor provides
 * negative DC feedback: Vgs = -Id*Rs
 *
 * Applicable to JFETs and depletion-mode MOSFETs where
 * Vgs must be negative (for N-channel).
 * ───────────────────────────────────────────────────────── */

BiasResult fet_bias_self_bias(const FetDevice *fet, const BiasSpec *spec)
{
    BiasResult result;
    memset(&result, 0, sizeof(result));
    result.converged = false;

    if (!fet) return result;

    double beta   = fet_beta(fet_kprime(fet->tech.mu_n, fet->tech.cox),
                             fet->geo.w_over_l);
    double vth    = fet->tech.vt_n;
    double lambda = fet->tech.lambda;
    double vdd    = spec->vdd;

    /* Self-bias: Vgg = 0, Rs used to set Vgs = -Id*Rs */
    /* For depletion-mode or JFET, Vth < 0 (N-ch) */
    /* Solve: Id = 0.5*beta*(0 - Id*Rs - Vth)^2*(1 + λ*(Vdd - Id*(Rd+Rs))) */
    double rs = spec->rs_degen;
    if (rs <= 0.0) {
        rs = (vth < 0) ? fabs(vth) / spec->target_id : 1e3;
    }
    double rd = (vdd - spec->target_vds) / spec->target_id - rs;
    if (rd < 0.0) rd = 0.0;

    double id;
    bool ok = bias_newton_solve(beta, vth, lambda, 0.0, vdd, rd, rs,
                                 &id, &result.iterations);
    result.converged = ok;

    double vgs = -id * rs;
    double vds = vdd - id * (rd + rs);

    result.qpoint.vgs = vgs;
    result.qpoint.vds = vds;
    result.qpoint.vbs = 0.0;
    result.qpoint.id  = id;
    result.qpoint.vov = vgs - vth;
    result.qpoint.gm  = (result.qpoint.vov > 0)
        ? beta * result.qpoint.vov * (1.0 + lambda * vds) : 0.0;
    result.qpoint.gds = (id > 0) ? lambda * id / (1.0 + lambda * vds) : 0.0;
    result.qpoint.vth_effective = vth;
    result.qpoint.vdsat = result.qpoint.vov;
    result.qpoint.region = fet_classify_region(vgs, vds, vth);
    result.qpoint.valid = true;

    result.network.topology = BIAS_SELF_BIAS;
    result.network.vdd = vdd;
    result.network.rd  = rd;
    result.network.rs  = rs;

    result.load_line = fet_compute_load_line(vdd, 0.0, rd, rs, 0, 0);
    result.power_dc   = vdd * id;
    result.vout_swing = 2.0 * fmin(vds - result.qpoint.vdsat,
                                    vdd - vds);

    double gm_local = result.qpoint.gm;
    if (gm_local > 0.0) {
        /* With degeneration: Id stabilized, ∂Id/∂Vth = -gm/(1+gm*Rs) */
        result.sensitivity_id = -gm_local / (1.0 + gm_local * rs);
    }

    return result;
}

/* ─────────────────────────────────────────────────────────
 * Voltage Divider Bias
 *
 * Gate voltage set by R1-R2 divider: Vgg = Vdd * Rg2/(Rg1+Rg2)
 * Source resistor provides negative feedback for stability.
 * This is the preferred discrete FET biasing method.
 *
 * Design equations:
 *   Vgs = Vgg - Id*Rs
 *   Vds = Vdd - Id*(Rd + Rs)
 *
 * Design procedure (from Sedra & Smith §6.3.3):
 *   1. Select Id and Vds (typically Vdd/2 for max swing)
 *   2. Vov chosen for gain/headroom trade-off
 *   3. Compute Rs = Vov/Id for Vgs ≈ Vth + Vov
 *   4. Vgg = Vgs + Id*Rs = Vth + Vov + Id*Rs
 *   5. Choose Rg1, Rg2 for Vgg and Rin_target
 * ───────────────────────────────────────────────────────── */

BiasResult fet_bias_voltage_divider(const FetDevice *fet, const BiasSpec *spec)
{
    BiasResult result;
    memset(&result, 0, sizeof(result));
    result.converged = false;

    if (!fet) return result;

    double beta   = fet_beta(fet_kprime(fet->tech.mu_n, fet->tech.cox),
                             fet->geo.w_over_l);
    double vth    = fet->tech.vt_n;
    double lambda = fet->tech.lambda;
    double vdd    = spec->vdd;
    double target_id = spec->target_id;

    /* Design procedure */
    double vov  = spec->target_vov;
    double vgs  = vth + vov;
    double rs   = spec->rs_degen;
    double r_total = (vdd - spec->target_vds) / target_id;
    double rd   = r_total - rs;
    if (rd < 0.0) rd = 0.0;

    /* Gate voltage needed: Vgg = Vgs + Id*Rs */
    double vgg_needed = vgs + target_id * rs;

    /* Choose Rg1, Rg2 for Vgg */
    double rg1, rg2;
    if (vgg_needed >= vdd) {
        /* Vgg cannot exceed Vdd; reduce Rs */
        rs = (vdd - vgs) / target_id;
        if (rs < 0.0) rs = 0.0;
        rd = r_total - rs;
        if (rd < 0.0) rd = 0.0;
        vgg_needed = vdd;
    }

    /* Rin_min constraint: Rg1||Rg2 >= rin_min */
    double rin_min = spec->rin_min;
    if (rin_min <= 0.0) rin_min = 10e3;

    /* Rg2 = Rgg * Vdd / Vgg, Rg1 = Rgg * Vdd / (Vdd - Vgg) */
    /* Choose Rgg = rin_min (exact) */
    double rgg = rin_min;
    if (vgg_needed > 0.0 && vgg_needed < vdd) {
        rg2 = rgg * vdd / vgg_needed;
        rg1 = rgg * vdd / (vdd - vgg_needed);
    } else if (vgg_needed >= vdd) {
        rg1 = 0.0;
        rg2 = rgg;
    } else {
        rg1 = rgg;
        rg2 = 1e12;  /* Effectively open */
    }

    double vgg_actual;
    fet_gate_equivalent(vdd, rg1, rg2, &vgg_actual, &rgg);

    /* Solve for actual Id with the designed network */
    double id;
    bool ok = bias_newton_solve(beta, vth, lambda, vgg_actual, vdd, rd, rs,
                                 &id, &result.iterations);
    result.converged = ok;

    double vgs_actual = vgg_actual - id * rs;
    double vds_actual = vdd - id * (rd + rs);

    result.qpoint.vgs = vgs_actual;
    result.qpoint.vds = vds_actual;
    result.qpoint.vbs = 0.0;
    result.qpoint.id  = id;
    result.qpoint.vov = vgs_actual - vth;
    result.qpoint.gm  = (result.qpoint.vov > 0)
        ? beta * result.qpoint.vov * (1.0 + lambda * vds_actual) : 0.0;
    result.qpoint.gds = (id > 0)
        ? lambda * id / (1.0 + lambda * vds_actual) : 0.0;
    result.qpoint.vth_effective = vth;
    result.qpoint.vdsat = result.qpoint.vov;
    result.qpoint.region = fet_classify_region(vgs_actual, vds_actual, vth);
    result.qpoint.valid = true;

    result.network.topology = BIAS_VOLTAGE_DIVIDER;
    result.network.vdd = vdd;
    result.network.rg1 = rg1;
    result.network.rg2 = rg2;
    result.network.rd  = rd;
    result.network.rs  = rs;
    result.network.rgg = rgg;
    result.network.vgg = vgg_actual;

    result.load_line = fet_compute_load_line(vdd, 0.0, rd, rs, rg1, rg2);
    result.power_dc   = vdd * id;
    result.vout_swing = 2.0 * fmin(vds_actual - result.qpoint.vdsat,
                                    vdd - vds_actual);

    double gm_local = result.qpoint.gm;
    if (gm_local > 0.0) {
        /* Id stability against Vth variation: S = -gm/(1 + gm*Rs + gm*Rg/β...) */
        /* Simplified for typical discrete: */
        double denom = 1.0 + gm_local * rs;
        result.sensitivity_id = (denom > 0) ? -gm_local / denom : 0.0;
    }

    return result;
}

/* ─────────────────────────────────────────────────────────
 * Current Source Bias (Active Load)
 *
 * Id forced by current source → independent of Vth, Vdd.
 * Best bias stability. Preferred for IC design.
 * ───────────────────────────────────────────────────────── */

BiasResult fet_bias_current_source(const FetDevice *fet, double iref,
                                    double vdd, double vss)
{
    BiasResult result;
    memset(&result, 0, sizeof(result));
    result.converged = true;  /* Current is forced */

    if (!fet) {
        result.converged = false;
        return result;
    }

    double beta   = fet_beta(fet_kprime(fet->tech.mu_n, fet->tech.cox),
                             fet->geo.w_over_l);
    double vth    = fet->tech.vt_n;
    double lambda = fet->tech.lambda;

    /* With Id = iref known, back-calculate Vgs:
     * iref = 0.5*beta*(Vgs-Vth)^2*(1+λ*Vds)
     * Approximate: Vgs ≈ Vth + sqrt(2*iref/beta)
     * Vds is a free parameter; for active load, Vds adjusts to circuit.
     */
    double vov = sqrt(2.0 * iref / beta);
    double vgs = vth + vov;
    /* Vds depends on the drain node voltage; assume Vdd/2 for typical */
    double vds = (vdd - vss) / 2.0;

    result.qpoint.vgs = vgs;
    result.qpoint.vds = vds;
    result.qpoint.vbs = 0.0;
    result.qpoint.id  = iref;
    result.qpoint.vov = vov;
    result.qpoint.gm  = beta * vov * (1.0 + lambda * vds);
    /* Alternatively: gm = 2*Id/Vov (more accurate for long channel) */
    result.qpoint.gm  = (vov > 0) ? 2.0 * iref / vov : 0.0;
    result.qpoint.gds = (iref > 0) ? lambda * iref / (1.0 + lambda * vds) : 0.0;
    result.qpoint.vth_effective = vth;
    result.qpoint.vdsat = vov;
    result.qpoint.region = FET_REGION_SATURATION;
    result.qpoint.valid = true;

    result.network.topology = BIAS_CURRENT_SOURCE;
    result.network.vdd = vdd;
    result.network.vss = vss;
    result.network.iref = iref;
    result.network.rd  = INFINITY;  /* Active load — high impedance */
    result.network.rs  = 0.0;

    result.load_line = fet_compute_load_line(vdd, vss, INFINITY, 0.0, 0, 0);
    result.load_line.id_max = iref;
    result.load_line.slope  = 0.0;
    result.power_dc = vdd * iref;

    /* With current-source bias, Id is independent of Vth → zero sensitivity */
    result.sensitivity_id = 0.0;
    result.iterations     = 1;

    return result;
}

/* ─────────────────────────────────────────────────────────
 * General Q-Point Solver
 *
 * Given an arbitrary bias network, solve for Id, Vgs, Vds
 * satisfying the FET equations and KVL constraints.
 * ───────────────────────────────────────────────────────── */

bool fet_solve_qpoint(const FetDevice *fet, const BiasNetwork *nw,
                      FetDcOpPoint *qpoint)
{
    if (!fet || !nw || !qpoint) return false;

    double beta   = fet_beta(fet_kprime(fet->tech.mu_n, fet->tech.cox),
                             fet->geo.w_over_l);
    double vth    = fet->tech.vt_n;
    double lambda = fet->tech.lambda;

    double rs = nw->rs;
    if (nw->topology == BIAS_CURRENT_SOURCE) {
        /* Id is forced */
        double id = nw->iref;
        double vov = sqrt(2.0 * id / beta);
        qpoint->id  = id;
        qpoint->vgs = vth + vov;
        qpoint->vds = nw->vdd / 2.0;  /* Approximate; depends on drain load */
        qpoint->vov = vov;
        qpoint->gm  = 2.0 * id / vov;
        qpoint->gds = lambda * id;
        qpoint->region = FET_REGION_SATURATION;
        qpoint->valid  = true;
        return true;
    }

    double vgg = nw->vgg;
    /* If vgg not computed, derive from divider */
    if (vgg <= 0.0 && nw->rg1 > 0.0 && nw->rg2 > 0.0) {
        vgg = nw->vdd * nw->rg2 / (nw->rg1 + nw->rg2);
    }

    double rd = nw->rd;
    double vdd = nw->vdd;

    double id;
    uint32_t iters;
    bool ok = bias_newton_solve(beta, vth, lambda, vgg, vdd, rd, rs,
                                 &id, &iters);

    double vgs = vgg - id * rs;
    double vds = vdd - id * (rd + rs);
    double vov = vgs - vth;

    qpoint->id  = id;
    qpoint->vgs = vgs;
    qpoint->vds = vds;
    qpoint->vbs = 0.0;
    qpoint->vov = vov;
    qpoint->gm  = (vov > 0) ? 2.0 * id / vov : 0.0;
    qpoint->gds = (id > 0) ? lambda * id : 0.0;
    qpoint->vth_effective = vth;
    qpoint->vdsat = vov;
    qpoint->region = fet_classify_region(vgs, vds, vth);
    qpoint->valid  = ok;

    return ok;
}

/* ─────────────────────────────────────────────────────────
 * Bias Sensitivity and Temperature Analysis
 * ───────────────────────────────────────────────────────── */

double fet_bias_sensitivity_vth(const FetDevice *fet, const BiasNetwork *nw,
                                 const FetDcOpPoint *qp)
{
    (void)fet;  /* Reserved for technology-dependent sensitivity */
    if (!qp || qp->gm <= 0.0) return 0.0;

    double gm = qp->gm;
    double rs = (nw) ? nw->rs : 0.0;

    /* Standard sensitivity formula for source-degenerated FET:
     * S_{Id}^{Vth} = ∂Id/∂Vth * Vth/Id = -gm*Vth / (Id*(1 + gm*Rs))
     * For small signals: ∂Id/∂Vth ≈ -gm/(1+gm*Rs)
     * Sensitivity: S = (∂Id/∂Vth) * (Vth/Id) ≈ -gm*Vth/(Id*(1+gm*Rs))
     */
    double denom = qp->id * (1.0 + gm * rs);
    if (fabs(denom) < 1e-30) return 0.0;

    return -gm * qp->vth_effective / denom;
}

double fet_bias_temperature_drift(const FetDevice *fet,
                                   const FetTempModel *tmodel,
                                   const FetDcOpPoint *qp)
{
    if (!fet || !tmodel || !qp) return 0.0;

    /* Id(T) varies through Vth(T) and μ(T):
     * dId/dT = (∂Id/∂Vth)*(dVth/dT) + (∂Id/∂μ)*(dμ/dT)
     *
     * ∂Id/∂Vth = -gm
     * dVth/dT = -α_Vth (Vth decreases with temperature)
     * ∂Id/∂μ = Id/μ
     * dμ/dT = -(α_μ/T0) * μ(T0)
     *
     * dId/dT = gm*α_Vth + Id * (-α_μ/T) at T=T0
     */
    double dId_dVth = -(qp->gm);  /* ∂Id/∂Vth = -gm */
    double dVth_dT  = tmodel->alpha_vth;  /* Typically ~ -2mV/K → negative */
    double term1 = dId_dVth * dVth_dT;

    /* μ dependence: Id ∝ μ, so ∂Id/∂μ = Id/μ_nominal */
    double mu = tmodel->mu_nominal;
    double dmu_dT;
    if (fabs(mu) > 1e-12) {
        dmu_dT = -tmodel->alpha_mu * mu / tmodel->t_nominal;
        double dId_dmu = qp->id / mu;
        double term2 = dId_dmu * dmu_dT;
        return term1 + term2;
    }

    return term1;
}

bool fet_bias_validate(const FetDcOpPoint *qp, double vdd, double pmax,
                       double bv_dss, double id_max)
{
    (void)vdd;  /* Supply voltage, used for SOA upper bound */
    if (!qp || !qp->valid) return false;

    /* Check saturation */
    if (qp->region != FET_REGION_SATURATION) {
        return false;
    }

    /* Check voltage rating */
    if (qp->vds > bv_dss) {
        return false;
    }

    /* Check current rating */
    if (qp->id > id_max) {
        return false;
    }

    /* Check power */
    double p_dc = qp->vds * qp->id;
    if (p_dc > pmax) {
        return false;
    }

    return true;
}

/* ─────────────────────────────────────────────────────────
 * Bias Design: Compute resistor values to achieve target Q-point
 * ───────────────────────────────────────────────────────── */

bool fet_bias_design(const FetDevice *fet, const BiasSpec *spec,
                     BiasResult *result)
{
    if (!fet || !spec || !result) return false;

    memset(result, 0, sizeof(*result));

    double beta   = fet_beta(fet_kprime(fet->tech.mu_n, fet->tech.cox),
                             fet->geo.w_over_l);
    double vth    = fet->tech.vt_n;
    double lambda = fet->tech.lambda;

    /* Determine topology from overdrive and target */
    BiasTopology topology = BIAS_VOLTAGE_DIVIDER;
    if (spec->vdd <= 0.0) {
        result->converged = false;
        return false;
    }

    /* Step 1: Choose Rd for target Vds */
    double vdd = spec->vdd;
    double id_target = spec->target_id;
    double vds_target = spec->target_vds;
    double vov_target = spec->target_vov;

    if (id_target <= 0.0) id_target = 1e-3;  /* Default 1 mA */
    if (vds_target <= 0.0) vds_target = vdd / 2.0;
    if (vov_target <= 0.0) vov_target = 0.2;  /* Default 200 mV overdrive */

    /* Step 2: Compute source resistor */
    double vgs_target = vth + vov_target;
    double rs = 0.0;
    if (spec->source_degenerated) {
        rs = spec->rs_degen;
        if (rs <= 0.0) {
            /* Typically 10% of (Vdd/Vov)*ro for good linearity */
            rs = 0.1 * vdd / id_target;
        }
    }

    /* Gate voltage needed: Vgg = Vgs + Id*Rs */
    double vgg_needed = vgs_target + id_target * rs;
    if (vgg_needed > vdd) vgg_needed = vdd;

    /* Rd from supply constraint: Rd = (Vdd - Vds - Id*Rs)/Id */
    double rd = (vdd - vds_target) / id_target - rs;
    if (rd < 0.0) {
        /* Target Vds too high; adjust */
        rd = 0.0;
        vds_target = vdd - id_target * rs;
    }

    /* Rin target → choose Rg1, Rg2 */
    double rin_target = spec->rin_min;
    if (rin_target <= 0.0) rin_target = 100e3;

    double rgg = rin_target;  /* Rg1||Rg2 = Rin */
    double rg1, rg2;
    if (vgg_needed > 0.0 && vgg_needed < vdd) {
        rg2 = rgg * vdd / vgg_needed;
        rg1 = rgg * vdd / (vdd - vgg_needed);
    } else {
        /* Degenerate case */
        rg1 = 1e12;
        rg2 = rgg;
    }

    double vgg_actual;
    fet_gate_equivalent(vdd, rg1, rg2, &vgg_actual, &rgg);

    /* Solve for actual operating point */
    double id_actual;
    uint32_t iters;
    bias_newton_solve(beta, vth, lambda, vgg_actual, vdd, rd, rs,
                      &id_actual, &iters);

    double vgs_actual = vgg_actual - id_actual * rs;
    double vds_actual = vdd - id_actual * (rd + rs);
    double vov_actual = vgs_actual - vth;

    result->qpoint.vgs = vgs_actual;
    result->qpoint.vds = vds_actual;
    result->qpoint.vbs = 0.0;
    result->qpoint.id  = id_actual;
    result->qpoint.vov = vov_actual;
    result->qpoint.gm  = (vov_actual > 0)
        ? 2.0 * id_actual / vov_actual : 0.0;
    result->qpoint.gds = (id_actual > 0)
        ? lambda * id_actual : 0.0;
    result->qpoint.vth_effective = vth;
    result->qpoint.vdsat = vov_actual;
    result->qpoint.region = fet_classify_region(vgs_actual, vds_actual, vth);
    result->qpoint.valid = true;

    result->network.topology = topology;
    result->network.vdd = vdd;
    result->network.rg1 = rg1;
    result->network.rg2 = rg2;
    result->network.rd  = rd;
    result->network.rs  = rs;
    result->network.rgg = rgg;
    result->network.vgg = vgg_actual;

    result->load_line = fet_compute_load_line(vdd, 0.0, rd, rs, rg1, rg2);
    result->power_dc   = vdd * id_actual;
    result->vout_swing = 2.0 * fmin(vds_actual - vov_actual, vdd - vds_actual);
    result->converged  = true;
    result->iterations = iters;

    double gm = result->qpoint.gm;
    if (gm > 0.0) {
        result->sensitivity_id = -gm / (1.0 + gm * rs);
    }

    return true;
}
