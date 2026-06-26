/**
 * fet_ac_analysis.c — FET Amplifier Frequency Response Analysis
 *
 * Implements transfer function construction, Bode plot generation,
 * Miller decomposition, OCTC method for bandwidth estimation,
 * pole-zero analysis, and harmonic distortion estimation.
 *
 * L3 Math Structures: Transfer functions, complex analysis, Laplace domain
 * L4 Fundamental Laws: Miller theorem, OCTC method
 * L5 Algorithms: Bandwidth estimation, distortion analysis
 *
 * Reference: Sedra & Smith, "Microelectronic Circuits" 8th Ed., Ch.9-10
 *            Gray & Meyer, "Analysis and Design of Analog IC" 5th Ed., Ch.7
 *            Sansen, "Analog Design Essentials" Ch.4
 *
 * MIT 6.002 / Berkeley EE105 / Stanford EE114 / Michigan EECS 411
 */

#include "fet_ac_analysis.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ─────────────────────────────────────────────────────────
 * Transfer Function Construction
 * ───────────────────────────────────────────────────────── */

TransferFunction fet_tf_from_pzmap(const double complex poles[], uint32_t np,
                                    const double complex zeros[], uint32_t nz,
                                    double k)
{
    TransferFunction tf;
    memset(&tf, 0, sizeof(tf));

    tf.num_order = nz;
    tf.den_order = np;
    tf.k = k;

    /* Copy poles */
    uint32_t np_copy = (np <= FET_MAX_ORDER) ? np : FET_MAX_ORDER;
    for (uint32_t i = 0; i < np_copy; i++) {
        tf.poles[i] = poles[i];
    }

    /* Copy zeros */
    uint32_t nz_copy = (nz <= FET_MAX_ORDER) ? nz : FET_MAX_ORDER;
    for (uint32_t i = 0; i < nz_copy; i++) {
        tf.zeros[i] = zeros[i];
    }

    /* Compute DC gain: H(0) = k * Π(-zi) / Π(-pj) */
    double complex num = k;
    for (uint32_t i = 0; i < nz_copy; i++) {
        num *= (-zeros[i]);
    }
    double complex den = 1.0;
    for (uint32_t i = 0; i < np_copy; i++) {
        den *= (-poles[i]);
    }
    tf.dc_gain = creal(num / den);

    return tf;
}

double complex fet_tf_evaluate(const TransferFunction *tf, double complex s)
{
    if (!tf) return 0.0;

    /* H(s) = k * Π(s - zi) / Π(s - pj) */
    double complex num = tf->k;
    for (uint32_t i = 0; i < tf->num_order; i++) {
        num *= (s - tf->zeros[i]);
    }

    double complex den = 1.0;
    for (uint32_t i = 0; i < tf->den_order; i++) {
        den *= (s - tf->poles[i]);
    }

    if (cabs(den) < 1e-30) {
        return INFINITY;  /* Pole at s */
    }

    return num / den;
}

double fet_tf_magnitude(const TransferFunction *tf, double freq_hz)
{
    if (!tf) return 0.0;

    double omega = 2.0 * M_PI * freq_hz;
    double complex s = I * omega;
    double complex h = fet_tf_evaluate(tf, s);
    return cabs(h);
}

double fet_tf_phase(const TransferFunction *tf, double freq_hz)
{
    if (!tf) return 0.0;

    double omega = 2.0 * M_PI * freq_hz;
    double complex s = I * omega;
    double complex h = fet_tf_evaluate(tf, s);
    return carg(h) * 180.0 / M_PI;  /* Phase in degrees */
}

/* ─────────────────────────────────────────────────────────
 * Bode Plot Generation
 * ───────────────────────────────────────────────────────── */

void fet_bode_generate(const TransferFunction *tf, double f_start,
                       double f_end, uint32_t n_pts_per_decade, BodeData *bode)
{
    if (!tf || !bode) return;

    /* Compute number of decades and total points */
    if (f_start <= 0.0) f_start = 1.0;
    if (f_end <= f_start) f_end = f_start * 1000.0;

    double decades = log10(f_end / f_start);
    if (n_pts_per_decade < 10) n_pts_per_decade = 10;
    uint32_t n_points = (uint32_t)(decades * n_pts_per_decade) + 1;
    if (n_points < 2) n_points = 2;
    if (n_points > 10000) n_points = 10000;

    bode->n_points = n_points;
    bode->tf = (TransferFunction *)tf;  /* non-const for caller flexibility */

    bode->frequencies  = (double *)malloc(n_points * sizeof(double));
    bode->magnitude_db = (double *)malloc(n_points * sizeof(double));
    bode->phase_deg    = (double *)malloc(n_points * sizeof(double));

    if (!bode->frequencies || !bode->magnitude_db || !bode->phase_deg) {
        fet_bode_free(bode);
        return;
    }

    double log_f_start = log10(f_start);
    double log_f_end   = log10(f_end);
    double log_step    = (log_f_end - log_f_start) / (n_points - 1);

    for (uint32_t i = 0; i < n_points; i++) {
        double freq = pow(10.0, log_f_start + i * log_step);
        bode->frequencies[i] = freq;

        double mag = fet_tf_magnitude(tf, freq);
        if (mag > 0.0) {
            bode->magnitude_db[i] = 20.0 * log10(mag);
        } else {
            bode->magnitude_db[i] = -200.0;  /* Effectively -∞ */
        }

        bode->phase_deg[i] = fet_tf_phase(tf, freq);
    }
}

void fet_bode_free(BodeData *bode)
{
    if (!bode) return;
    free(bode->frequencies);
    free(bode->magnitude_db);
    free(bode->phase_deg);
    bode->frequencies  = NULL;
    bode->magnitude_db = NULL;
    bode->phase_deg    = NULL;
    bode->n_points     = 0;
    bode->tf           = NULL;
}

double fet_bandwidth_from_bode(const BodeData *bode)
{
    if (!bode || !bode->magnitude_db || bode->n_points < 2) return 0.0;

    /* Find max gain */
    double max_db = bode->magnitude_db[0];
    for (uint32_t i = 1; i < bode->n_points; i++) {
        if (bode->magnitude_db[i] > max_db) {
            max_db = bode->magnitude_db[i];
        }
    }

    double target_db = max_db - 3.0;

    /* Find lower -3dB frequency */
    double f_low = 0.0;
    for (uint32_t i = 1; i < bode->n_points; i++) {
        if (bode->magnitude_db[i] >= target_db &&
            bode->magnitude_db[i-1] < target_db) {
            /* Linear interpolation */
            double frac = (target_db - bode->magnitude_db[i-1]) /
                          (bode->magnitude_db[i] - bode->magnitude_db[i-1]);
            f_low = bode->frequencies[i-1] +
                    frac * (bode->frequencies[i] - bode->frequencies[i-1]);
            break;
        }
        if (bode->magnitude_db[i] >= target_db && i == 1) {
            f_low = bode->frequencies[0];
        }
    }

    /* Find upper -3dB frequency */
    double f_high = bode->frequencies[bode->n_points - 1];
    bool found_high = false;
    for (uint32_t i = bode->n_points - 1; i > 0; i--) {
        if (bode->magnitude_db[i-1] >= target_db &&
            bode->magnitude_db[i] < target_db) {
            double frac = (bode->magnitude_db[i-1] - target_db) /
                          (bode->magnitude_db[i-1] - bode->magnitude_db[i]);
            f_high = bode->frequencies[i-1] +
                     frac * (bode->frequencies[i] - bode->frequencies[i-1]);
            found_high = true;
            break;
        }
    }

    if (!found_high && f_low > 0.0) {
        return f_high - f_low;
    }

    return f_high - f_low;
}

double fet_gain_bandwidth_product(double av_mid, double bw_hz)
{
    return fabs(av_mid) * bw_hz;
}

/* ─────────────────────────────────────────────────────────
 * Miller Decomposition
 *
 * For a bridging impedance Z between input and output with gain Av:
 *   Z1 = Z / (1 - Av)  (input equivalent)
 *   Z2 = Z * Av / (Av - 1)  (output equivalent)
 *
 * For capacitors: Cin_miller = C * (1 - Av), Cout_miller ≈ C
 * ───────────────────────────────────────────────────────── */

MillerDecomposition fet_miller_decompose(double cgd, double av, double freq)
{
    MillerDecomposition md;
    memset(&md, 0, sizeof(md));

    md.c_original = cgd;
    md.av_node    = av;
    md.z_original = (freq > 0.0 && cgd > 0.0)
                    ? 1.0 / (2.0 * M_PI * freq * cgd) : INFINITY;

    /* If Av = -100 (CS amplifier), Cin_miller ≈ 101*Cgd → very significant! */
    double one_minus_av = 1.0 - av;

    if (fabs(one_minus_av) > 1e-30) {
        md.c_input_miller  = cgd * one_minus_av;
        md.z_input_equiv   = 1.0 / (2.0 * M_PI * freq * md.c_input_miller);
    } else {
        md.c_input_miller  = INFINITY;
        md.z_input_equiv   = 0.0;
    }

    if (fabs(av) > 1e-30) {
        md.c_output_miller = cgd * (av - 1.0) / av;
        md.z_output_equiv  = 1.0 / (2.0 * M_PI * freq * md.c_output_miller);
    } else {
        md.c_output_miller = cgd;
        md.z_output_equiv  = md.z_original;
    }

    return md;
}

/* ─────────────────────────────────────────────────────────
 * Open-Circuit Time Constants (OCTC) Method
 *
 * ωh ≈ 1 / Σ(Ri_open * Ci)
 *
 * This is a hand-analysis method that estimates the upper -3dB
 * frequency of an amplifier by considering each capacitor's
 * time constant individually.
 * ───────────────────────────────────────────────────────── */

/** Add a time constant contribution to the OCTC result */
static void octc_add(OctcResult *octc, const char *name,
                     double cap, double res)
{
    if (!octc || octc->n_caps >= FET_OCTC_MAX_CAPS) return;
    if (cap <= 0.0 || res <= 0.0) return;

    uint32_t i = octc->n_caps;
    octc->tc[i].name        = name;
    octc->tc[i].capacitance = cap;
    octc->tc[i].resistance_seen = res;
    octc->tc[i].time_constant   = cap * res;
    octc->tc[i].frequency_corner = 1.0 / (2.0 * M_PI * cap * res);
    octc->sum_tau += octc->tc[i].time_constant;
    octc->n_caps++;
}

OctcResult fet_octc_common_source(const HybridPiModel *hp,
                                   double rd, double rl, double rsig,
                                   double cs, double cl)
{
    OctcResult octc;
    memset(&octc, 0, sizeof(octc));

    if (!hp) return octc;

    double gm  = hp->gm;
    double gds = hp->gds;
    double ro  = (gds > 0.0) ? 1.0 / gds : INFINITY;

    /* Total load at drain: ro || Rd || RL */
    double rload = INFINITY;
    if (ro < INFINITY) rload = ro;
    if (rd > 0.0 && rd < INFINITY) {
        rload = (rload < INFINITY) ? (rload * rd) / (rload + rd) : rd;
    }
    if (rl > 0.0 && rl < INFINITY) {
        rload = (rload < INFINITY) ? (rload * rl) / (rload + rl) : rl;
    }

    /* Mid-band gain for Miller effect */
    double av_mid = -gm * rload;

    /* 1. Cgs: R_seen = Rsig (looking from gate through source)
     *    Actually R_seen_Cgs = Rsig (in series with gate)
     */
    octc_add(&octc, "Cgs", hp->cgs, rsig);

    /* 2. Cgd Miller input: R_seen = Rsig||Rin_effective
     *    More precisely: R_miller_in = (Rsig + rload) / (1 + gm*rload)
     *    Simplified: for large gm*rload, R ≈ Rsig
     */
    double r_miller_in;
    if (gm > 0.0 && rload < INFINITY) {
        r_miller_in = rsig * (1.0 + gm * rload) + rload;
    } else {
        r_miller_in = rsig;
    }
    double c_miller_in = hp->cgd * (1.0 - av_mid);  /* Cgd*(1+|Av|) */
    if (c_miller_in < 0.0) c_miller_in = -c_miller_in;
    octc_add(&octc, "Cgd_miller_in", c_miller_in, r_miller_in);

    /* 3. Cds: R_seen = rload (drain to ground) */
    octc_add(&octc, "Cds", hp->cds, rload);

    /* 4. Cgd Miller output: C ≈ Cgd, R = rload */
    octc_add(&octc, "Cgd_miller_out", hp->cgd, rload);

    /* 5. CL: load capacitance */
    if (cl > 0.0) {
        octc_add(&octc, "CL", cl, rload);
    }

    /* 6. Cs: source bypass capacitor (if unbypassed) */
    if (cs > 0.0 && cs < 1e-3) {
        /* R_seen_Cs = (1/gm) || Rs for CS with source degeneration */
        double r_seen_cs = (gm > 0.0) ? 1.0 / gm : INFINITY;
        octc_add(&octc, "Cs_bypass", cs, r_seen_cs);
    }

    /* Estimate fh */
    if (octc.sum_tau > 0.0) {
        octc.fh_estimate = 1.0 / (2.0 * M_PI * octc.sum_tau);
    }

    return octc;
}

OctcResult fet_octc_common_gate(const HybridPiModel *hp,
                                 double rd, double rl, double rsig)
{
    OctcResult octc;
    memset(&octc, 0, sizeof(octc));

    if (!hp) return octc;

    double gm  = hp->gm;
    double gds = hp->gds;
    double ro  = (gds > 0.0) ? 1.0 / gds : INFINITY;

    /* Total load at drain */
    double rload = INFINITY;
    if (ro < INFINITY) rload = ro;
    if (rd > 0.0 && rd < INFINITY) {
        rload = (rload < INFINITY) ? (rload * rd) / (rload + rd) : rd;
    }
    if (rl > 0.0 && rl < INFINITY) {
        rload = (rload < INFINITY) ? (rload * rl) / (rload + rl) : rl;
    }

    /* CG input resistance looking into source: Rin ≈ 1/gm */
    double rin_cg = (gm > 0.0) ? 1.0 / gm : INFINITY;
    double r_source_eff = (rsig < INFINITY && rin_cg < INFINITY)
                           ? (rsig * rin_cg) / (rsig + rin_cg) : rsig;

    /* 1. Cgs: R_seen = Rsig || (1/gm) (looking from gate to source) */
    octc_add(&octc, "Cgs", hp->cgs, r_source_eff);

    /* 2. Cgd: R_seen = rload (no Miller effect in CG — big advantage!) */
    octc_add(&octc, "Cgd", hp->cgd, rload);

    /* 3. Cds: R_seen = rload */
    octc_add(&octc, "Cds", hp->cds, rload);

    if (octc.sum_tau > 0.0) {
        octc.fh_estimate = 1.0 / (2.0 * M_PI * octc.sum_tau);
    }

    return octc;
}

OctcResult fet_octc_cascode(const HybridPiModel *hp_cs,
                              const HybridPiModel *hp_cg,
                              double rd, double rl, double rsig)
{
    OctcResult octc;
    memset(&octc, 0, sizeof(octc));

    if (!hp_cs || !hp_cg) return octc;

    double gm_cs  = hp_cs->gm;
    double gm_cg  = hp_cg->gm;
    double gds_cg = hp_cg->gds;
    double ro_cg  = (gds_cg > 0.0) ? 1.0 / gds_cg : INFINITY;

    /* In cascode, the CS drain sees the CG source (~1/gm_cg)
     * so the CS gain is approximately -gm_cs * (1/gm_cg) ≈ -1.
     * This nearly eliminates Miller multiplication of Cgd_cs!
     */
    double av_cs = (gm_cs > 0.0 && gm_cg > 0.0) ? -gm_cs / gm_cg : -1.0;

    /* CS Miller: very small because Av ≈ -1 */
    double c_miller_in = hp_cs->cgd * (1.0 - av_cs);
    if (c_miller_in < 0.0) c_miller_in = -c_miller_in;

    /* R seen by Cgs_cs = Rsig */
    octc_add(&octc, "Cgs_CS", hp_cs->cgs, rsig);

    /* Miller Cgd_CS input: R = Rsig (Av≈-1 means multiplication ≈ 2) */
    double r_miller_cs = rsig;
    octc_add(&octc, "Cgd_CS_miller", c_miller_in, r_miller_cs);

    /* Cds_CS: node impedance is ~1/gm_cg */
    double r_node_cs = (gm_cg > 0.0) ? 1.0 / gm_cg : INFINITY;
    octc_add(&octc, "Cds_CS", hp_cs->cds, r_node_cs);

    /* CG transistor capacitances */
    double rload = INFINITY;
    if (ro_cg < INFINITY) rload = ro_cg;
    if (rd > 0.0 && rd < INFINITY) {
        rload = (rload < INFINITY) ? (rload * rd) / (rload + rd) : rd;
    }
    if (rl > 0.0 && rl < INFINITY) {
        rload = (rload < INFINITY) ? (rload * rl) / (rload + rl) : rl;
    }

    /* Cgs_CG: R = 1/gm_cg (looking from source) */
    double r_source_cg = (gm_cg > 0.0) ? 1.0 / gm_cg : INFINITY;
    octc_add(&octc, "Cgs_CG", hp_cg->cgs, r_source_cg);

    /* Cgd_CG: R = rload (no Miller in CG) */
    octc_add(&octc, "Cgd_CG", hp_cg->cgd, rload);

    /* Cds_CG: R = rload */
    octc_add(&octc, "Cds_CG", hp_cg->cds, rload);

    if (octc.sum_tau > 0.0) {
        octc.fh_estimate = 1.0 / (2.0 * M_PI * octc.sum_tau);
    }

    return octc;
}

double fet_dominant_pole_from_octc(const OctcResult *octc)
{
    if (!octc || octc->sum_tau <= 0.0) return 0.0;
    return 1.0 / (2.0 * M_PI * octc->sum_tau);
}

/* ─────────────────────────────────────────────────────────
 * Harmonic Distortion Estimation
 *
 * Uses Taylor series expansion of FET I-V characteristic:
 * id(vgs) = Id_Q + gm*vgs + (gm2/2)*vgs^2 + (gm3/6)*vgs^3 + ...
 *
 * For a sinusoidal input vgs = Vm*cos(ωt):
 * HD2 = |H2|/|H1| ≈ (1/4)*(gm2/gm)*Vm
 * HD3 = |H3|/|H1| ≈ (1/24)*(gm3/gm)*Vm^2
 *
 * In square-law FET (long channel):
 * gm2 = β (constant → gm' = ∂²Id/∂Vgs²)
 * gm3 = 0 (pure square-law has no 3rd-order!)
 *
 * For short-channel devices, higher-order terms appear.
 * ───────────────────────────────────────────────────────── */

void fet_distortion_estimate(double gm, double gm2, double gm3,
                              double vm_amplitude, double *hd2_db, double *hd3_db)
{
    double hd2 = 0.0;
    double hd3 = 0.0;

    if (gm > 0.0 && vm_amplitude > 0.0) {
        /* HD2 = (1/4)*(gm2/gm)*Vm */
        hd2 = 0.25 * (gm2 / gm) * vm_amplitude;

        /* HD3 = (1/24)*(gm3/gm)*Vm^2 */
        if (fabs(gm3) > 1e-30) {
            hd3 = (1.0 / 24.0) * (gm3 / gm) * vm_amplitude * vm_amplitude;
        }
    }

    /* Convert to dBc */
    if (hd2_db) {
        *hd2_db = (hd2 > 1e-12) ? 20.0 * log10(hd2) : -200.0;
    }
    if (hd3_db) {
        *hd3_db = (hd3 > 1e-12) ? 20.0 * log10(hd3) : -200.0;
    }
}
