/**
 * fet_small_signal.c — FET Small-Signal Model Extraction and Analysis
 *
 * Implements hybrid-π parameter extraction from DC bias,
 * T-model conversion, two-port parameter computation (Y, S),
 * gain/impedance formulas, and stability analysis.
 *
 * L3 Mathematical Structures: Y-parameters, S-parameters, complex analysis
 * L4 Fundamental Laws: Gain formulas, Miller effect, Rollett stability
 *
 * Reference: Sedra & Smith, "Microelectronic Circuits" 8th Ed., Ch.7
 *            Pozar, "Microwave Engineering" 4th Ed., Ch.4
 *            Gonzalez, "Microwave Transistor Amplifiers" 2nd Ed., Ch.2
 *
 * Stanford EE114 / Berkeley EE105 / Illinois ECE 451
 */

#include "fet_small_signal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <complex.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ─────────────────────────────────────────────────────────
 * Hybrid-π Parameter Extraction
 *
 * Derives small-signal parameters from the DC operating point
 * and device physical parameters.
 * ───────────────────────────────────────────────────────── */

HybridPiModel fet_extract_hybrid_pi(const FetDevice *fet)
{
    HybridPiModel hp;
    memset(&hp, 0, sizeof(hp));

    if (!fet) return hp;

    double gm  = fet->bias.gm;
    double gds = fet->bias.gds;
    double vds = fet->bias.vds;
    double w   = fet->geo.w;
    double l   = fet->geo.l;
    double cox = fet->tech.cox;

    hp.gm  = gm;
    hp.gds = gds;

    /* Body transconductance: gmb = χ * gm
     * χ = γ / (2*sqrt(2*φf + Vsb)) ≈ 0.1-0.3 typically
     */
    double chi = 0.0;
    double vsb = fet->bias.vbs;
    double phi_f = fet->tech.phi_f;
    double gamma_body = fet->tech.gamma_body;
    if (gamma_body > 0.0 && phi_f > 0.0 && (2.0 * phi_f + vsb) > 0.0) {
        chi = gamma_body / (2.0 * sqrt(2.0 * phi_f + vsb));
    }
    hp.gmb = chi * gm;

    /* Gate-source capacitance: Cgs = (2/3)*Cox*W*L + Cgso*W
     * In saturation, channel charge is 2/3 of total (Meyer model).
     */
    double cox_wl = cox * w * l;
    double cgso_w = fet->tech.cgso * w;
    hp.cgs = (2.0 / 3.0) * cox_wl + cgso_w;

    /* Gate-drain capacitance: Cgd = Cgdo*W (overlap dominant in saturation)
     * In saturation, the channel is pinched off at the drain side,
     * so the intrinsic Cgd ≈ 0; only overlap remains.
     */
    double cgdo_w = fet->tech.cgdo * w;
    hp.cgd = cgdo_w;

    /* Gate-bulk capacitance: Cgb ≈ 0 in strong inversion (channel shields)
     * Only significant in accumulation/depletion.
     */
    hp.cgb = 0.0;

    /* Drain-source junction capacitance:
     * Cds = Cj0 * Ad / (1 + Vds/PB)^Mj (for the drain junction)
     */
    double cj0  = fet->tech.cj0;
    double mj   = fet->tech.mj;
    double pb   = fet->tech.pb;
    if (pb <= 0.0) pb = 0.8;  /* Default built-in potential for Si */
    double ad = fet->geo.ad;
    double vds_abs = fabs(vds);
    if (cj0 > 0.0 && ad > 0.0 && (1.0 + vds_abs / pb) > 0.0) {
        hp.cds = cj0 * ad / pow(1.0 + vds_abs / pb, mj);
    } else {
        hp.cds = 0.0;
    }

    /* Source-bulk junction capacitance */
    double as_junc = fet->geo.as;
    double vsb_abs = fabs(vsb);
    if (cj0 > 0.0 && as_junc > 0.0 && (1.0 + vsb_abs / pb) > 0.0) {
        hp.csb = cj0 * as_junc / pow(1.0 + vsb_abs / pb, mj);
    } else {
        hp.csb = 0.0;
    }

    /* Drain-bulk (same as Cds for symmetry) */
    hp.cdb = hp.cds;

    /* Extrinsic parasitics */
    hp.rg   = fet->tech.rsh * w / (12.0 * l);  /* Poly gate resistance */
    hp.rs   = 0.0;  /* Typically from contact/layout; negligible in first order */
    hp.rd   = 0.0;
    hp.rsub = 1000.0;  /* Substrate resistance; layout dependent */

    return hp;
}

/* ─────────────────────────────────────────────────────────
 * T-Model Conversion
 * ───────────────────────────────────────────────────────── */

FetTModel fet_hybrid_pi_to_tmodel(const HybridPiModel *hp)
{
    FetTModel tm;
    memset(&tm, 0, sizeof(tm));

    if (!hp) return tm;

    tm.gm = hp->gm;
    tm.rs = (hp->gm > 0.0) ? 1.0 / hp->gm : INFINITY;
    tm.ro = (hp->gds > 0.0) ? 1.0 / hp->gds : INFINITY;
    tm.r_body = (hp->gmb > 0.0) ? 1.0 / hp->gmb : INFINITY;

    return tm;
}

/* ─────────────────────────────────────────────────────────
 * Ideal Amplifier Gain Formulas
 * ───────────────────────────────────────────────────────── */

double fet_cs_gain_ideal(double gm, double ro, double rd)
{
    if (gm <= 0.0) return 0.0;

    double rout;
    if (ro <= 0.0) {
        rout = rd;
    } else if (rd <= 0.0) {
        rout = ro;
    } else {
        rout = (ro * rd) / (ro + rd);
    }

    return -gm * rout;
}

double fet_cs_gain_degenerated(double gm, double ro, double rd, double rs)
{
    if (gm <= 0.0) return 0.0;

    /* Effective transconductance with source degeneration:
     * Gm_eff = gm / (1 + gm*Rs + (Rs+rd)/(ro||Rd))
     * For large ro: Gm_eff ≈ gm / (1 + gm*Rs)
     */
    double gm_eff;
    if (rs <= 0.0) {
        gm_eff = gm;
    } else {
        gm_eff = gm / (1.0 + gm * rs);
    }

    /* Output resistance looking into drain:
     * Rout ≈ ro * (1 + gm*Rs) for degeneration
     */
    double rout_drain;
    if (ro <= 0.0 || ro == INFINITY) {
        rout_drain = INFINITY;
    } else {
        rout_drain = ro * (1.0 + gm * rs);
    }

    /* Total load at drain: rout_drain || rd */
    double rout_total;
    if (rd <= 0.0) {
        rout_total = rout_drain;
    } else if (rout_drain == INFINITY) {
        rout_total = rd;
    } else {
        rout_total = (rout_drain * rd) / (rout_drain + rd);
    }

    return -gm_eff * rout_total;
}

double fet_cg_gain_ideal(double gm, double ro, double rd, double rsig)
{
    if (gm <= 0.0) return 0.0;

    /* CG: Av = (gm + gmb) * (ro || Rd)
     * Input resistance ≈ 1/gm for large ro
     * Accounting for source resistance: Av = (gm*ro*Rd) / (ro*Rd + rsig*(ro+Rd))
     * Simplified for large ro: Av ≈ gm*Rd / (1 + gm*rsig)
     */

    double rout;
    if (ro <= 0.0) {
        rout = rd;
    } else if (rd <= 0.0) {
        rout = ro;
    } else {
        rout = (ro * rd) / (ro + rd);
    }

    double gain = gm * rout;
    /* Source resistance effect */
    if (rsig > 0.0) {
        gain = gain / (1.0 + gm * rsig);
    }

    return gain;  /* Non-inverting */
}

double fet_cd_gain_ideal(double gm, double gmb, double ro, double rs)
{
    if (gm <= 0.0) return 0.0;

    /* Source follower gain:
     * Av = gm*Rs' / (1 + (gm+gmb)*Rs')
     * where Rs' = ro || Rs
     *
     * For gmb*Rs << 1 (small body effect): Av ≈ gm*Rs / (1 + gm*Rs)
     * For gm*Rs >> 1: Av ≈ 1
     */

    double gm_total = gm + gmb;  /* Account for body effect */
    double rs_prime;

    if (rs <= 0.0) return 0.0;

    if (ro <= 0.0 || ro == INFINITY) {
        rs_prime = rs;
    } else {
        rs_prime = (ro * rs) / (ro + rs);
    }

    return (gm * rs_prime) / (1.0 + gm_total * rs_prime);
}

double fet_cd_output_resistance(double gm, double gmb, double ro, double rs)
{
    /* Rout of source follower = (1/gm) || (1/gmb) || ro || Rs */
    double r1 = (gm > 0.0) ? 1.0 / gm : INFINITY;
    double r2 = (gmb > 0.0) ? 1.0 / gmb : INFINITY;
    double r3 = (ro > 0.0 && ro != INFINITY) ? ro : INFINITY;
    double r4 = (rs > 0.0) ? rs : INFINITY;

    /* Parallel combination */
    double g_total = 0.0;
    if (r1 < INFINITY) g_total += 1.0 / r1;
    if (r2 < INFINITY) g_total += 1.0 / r2;
    if (r3 < INFINITY) g_total += 1.0 / r3;
    if (r4 < INFINITY) g_total += 1.0 / r4;

    return (g_total > 0.0) ? 1.0 / g_total : INFINITY;
}

/* ─────────────────────────────────────────────────────────
 * Y-Parameter Computation
 *
 * Hybrid-π → Y-parameter conversion at frequency f.
 * Reference: Gonzalez §2.2 "Y-Parameter Representation"
 * ───────────────────────────────────────────────────────── */

FetYParams fet_hp_to_yparams(const HybridPiModel *hp, double freq_hz)
{
    FetYParams y;
    memset(&y, 0, sizeof(y));
    y.frequency = freq_hz;

    if (!hp) return y;

    double omega = 2.0 * M_PI * freq_hz;
    double gm  = hp->gm;
    double gds = hp->gds;
    double cgs = hp->cgs;
    double cgd = hp->cgd;
    double cds = hp->cds;

    /* y11 = jω(Cgs + Cgd) [ignoring rg for now] */
    y.y11 = I * omega * (cgs + cgd);

    /* y12 = -jω*Cgd */
    y.y12 = -I * omega * cgd;

    /* y21 = gm - jω*Cgd */
    y.y21 = gm - I * omega * cgd;

    /* y22 = gds + jω*(Cds + Cgd) */
    y.y22 = gds + I * omega * (cds + cgd);

    return y;
}

/* ─────────────────────────────────────────────────────────
 * S-Parameter Conversion
 *
 * Y → S conversion using standard formula:
 * S = (I_n - Z0*Y) * (I_n + Z0*Y)^(-1)  (2-port case)
 * ───────────────────────────────────────────────────────── */

FetSParams fet_yparams_to_sparams(const FetYParams *y, double z0)
{
    FetSParams s;
    memset(&s, 0, sizeof(s));
    s.z0 = z0;
    s.frequency = y->frequency;

    if (!y) return s;

    /* For 2-port: S11 = ((1-Z0*y11)*(1+Z0*y22)+Z0^2*y12*y21) / Δ
     *            S12 = -2*Z0*y12 / Δ
     *            S21 = -2*Z0*y21 / Δ
     *            S22 = ((1+Z0*y11)*(1-Z0*y22)+Z0^2*y12*y21) / Δ
     * where Δ = (1+Z0*y11)*(1+Z0*y22) - Z0^2*y12*y21
     */
    double complex y11 = y->y11;
    double complex y12 = y->y12;
    double complex y21 = y->y21;
    double complex y22 = y->y22;

    double complex d11 = 1.0 + z0 * y11;
    double complex d12 = z0 * y12;
    double complex d21 = z0 * y21;
    double complex d22 = 1.0 + z0 * y22;

    double complex delta = d11 * d22 - d12 * d21;

    if (cabs(delta) < 1e-30) {
        /* Degenerate case */
        s.s11 = 1.0;
        s.s22 = 1.0;
        s.s12 = 0.0;
        s.s21 = 0.0;
        return s;
    }

    s.s11 = ((1.0 - z0 * y11) * (1.0 + z0 * y22) + z0 * z0 * y12 * y21) / delta;
    s.s12 = -2.0 * z0 * y12 / delta;
    s.s21 = -2.0 * z0 * y21 / delta;
    s.s22 = ((1.0 + z0 * y11) * (1.0 - z0 * y22) + z0 * z0 * y12 * y21) / delta;

    return s;
}

/* ─────────────────────────────────────────────────────────
 * S → Y Conversion (inverse)
 * ───────────────────────────────────────────────────────── */

FetYParams fet_sparams_to_yparams(const FetSParams *s)
{
    FetYParams y;
    memset(&y, 0, sizeof(y));
    y.frequency = s->frequency;

    if (!s) return y;

    double z0 = s->z0;
    if (z0 <= 0.0) z0 = 50.0;

    double complex s11 = s->s11;
    double complex s12 = s->s12;
    double complex s21 = s->s21;
    double complex s22 = s->s22;

    double complex denom_y11 = (1.0 + s11) * (1.0 + s22) - s12 * s21;
    if (cabs(denom_y11) < 1e-30) {
        return y;
    }

    y.y11 = ((1.0 - s11) * (1.0 + s22) + s12 * s21) / (denom_y11 * z0);
    y.y12 = -2.0 * s12 / (denom_y11 * z0);
    y.y21 = -2.0 * s21 / (denom_y11 * z0);
    y.y22 = ((1.0 + s11) * (1.0 - s22) + s12 * s21) / (denom_y11 * z0);

    return y;
}

/* ─────────────────────────────────────────────────────────
 * Rollett Stability Factor
 *
 * K > 1 and |Δ| < 1 → unconditionally stable.
 * Reference: Rollett (1962) IRE Trans. CT-9, pp.29-32
 * ───────────────────────────────────────────────────────── */

double fet_rollett_stability_k(const FetSParams *s)
{
    if (!s) return 0.0;

    double s11_mag2 = pow(cabs(s->s11), 2.0);
    double s22_mag2 = pow(cabs(s->s22), 2.0);
    double complex delta = s->s11 * s->s22 - s->s12 * s->s21;
    double delta_mag2 = pow(cabs(delta), 2.0);
    double denom = 2.0 * cabs(s->s12 * s->s21);

    if (denom < 1e-30) {
        return INFINITY;  /* No reverse transmission → absolutely stable */
    }

    return (1.0 - s11_mag2 - s22_mag2 + delta_mag2) / denom;
}

double fet_max_available_gain(const FetSParams *s)
{
    if (!s) return 0.0;

    double k = fet_rollett_stability_k(s);

    if (k < 1.0) {
        /* Potentially unstable — MAG not defined; use MSG instead */
        double msg = cabs(s->s21) / cabs(s->s12);
        return msg;
    }

    /* MAG = |S21/S12| * (K - sqrt(K^2 - 1)) */
    double ratio = cabs(s->s21) / cabs(s->s12);
    double mag = ratio * (k - sqrt(k * k - 1.0));

    return mag;
}
