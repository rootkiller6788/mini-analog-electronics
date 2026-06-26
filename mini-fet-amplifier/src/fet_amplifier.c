/**
 * fet_amplifier.c — FET Amplifier Design, Analysis, and Optimization
 *
 * Implements complete design flows for:
 * - Common-Source (CS) amplifier
 * - Common-Drain (source follower) amplifier
 * - Common-Gate amplifier
 * - Cascode amplifier
 * - Differential pair amplifier
 * - Current mirror designs
 * - LNA, PA, and instrumentation amplifier applications
 *
 * L5 Algorithms: CS/CD/CG/Cascode/DiffPair design methodologies
 * L6 Canonical Problems: Complete amplifier design with specs
 * L7 Applications: LNA, PA, instrumentation amplifier
 *
 * Reference: Sedra & Smith §7-10; Razavi §3-5; Sansen §1-5;
 *            Lee "Design of CMOS RF IC" §4-6
 *
 * MIT 6.301 / Berkeley EE140 / Stanford EE214 / Michigan EECS 511
 */

#include "fet_amplifier.h"
#include "fet_noise.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ─────────────────────────────────────────────────────────
 * gm/Id Methodology Implementation
 *
 * The gm/Id design methodology replaces square-law equations
 * for deep submicron transistors. It uses lookup tables from
 * simulation data to relate gm/Id to ft, gm/gds, current density.
 * ───────────────────────────────────────────────────────── */

bool fet_design_gmid(const GmIdTable *table, double target_gm, double target_id,
                     double l, double *w_out, GmIdEntry *result_entry)
{
    if (!table || !w_out || !result_entry || target_gm <= 0.0 || target_id <= 0.0) {
        return false;
    }

    double target_gmid = target_gm / target_id;

    /* Binary search for closest gm/Id entry */
    int lo = 0, hi = (int)(table->n_entries) - 1;
    if (hi < 0) return false;

    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (table->entries[mid].gmid < target_gmid) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }

    /* lo is the first entry with gmid >= target */
    uint32_t idx = (lo < (int)table->n_entries) ? (uint32_t)lo
                                                 : table->n_entries - 1;
    if (idx > 0 && table->entries[idx].gmid > target_gmid) {
        /* Linear interpolation between idx-1 and idx */
        double gmid_lo = table->entries[idx-1].gmid;
        double gmid_hi = table->entries[idx].gmid;
        double frac = (target_gmid - gmid_lo) / (gmid_hi - gmid_lo);

        double idow_lo = table->entries[idx-1].id_over_w;
        double idow_hi = table->entries[idx].id_over_w;
        double idow = idow_lo + frac * (idow_hi - idow_lo);

        *result_entry = table->entries[idx];
        result_entry->gmid = target_gmid;
        result_entry->id_over_w = idow;

        *w_out = target_id / idow * l;
    } else {
        *result_entry = table->entries[idx];
        double idow = table->entries[idx].id_over_w;
        *w_out = (idow > 0.0) ? target_id / idow * l : 1e-6;
    }

    return (*w_out > 0.0);
}

bool fet_gmid_interpolate(const GmIdTable *table, double gmid,
                          GmIdEntry *result)
{
    if (!table || !result || table->n_entries == 0) return false;

    /* Binary search */
    int lo = 0, hi = (int)(table->n_entries) - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (table->entries[mid].gmid < gmid) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }

    uint32_t idx = (lo < (int)table->n_entries) ? (uint32_t)lo
                                                 : table->n_entries - 1;

    if (idx == 0 || fabs(table->entries[idx].gmid - gmid) < 1e-9) {
        *result = table->entries[idx];
        return true;
    }

    /* Interpolate */
    double gmid_lo = table->entries[idx-1].gmid;
    double gmid_hi = table->entries[idx].gmid;
    double frac = (gmid - gmid_lo) / (gmid_hi - gmid_lo);

    result->gmid = gmid;
    result->vgs = table->entries[idx-1].vgs +
                  frac * (table->entries[idx].vgs - table->entries[idx-1].vgs);
    result->id_over_w = table->entries[idx-1].id_over_w +
                  frac * (table->entries[idx].id_over_w - table->entries[idx-1].id_over_w);
    result->ft = table->entries[idx-1].ft +
                  frac * (table->entries[idx].ft - table->entries[idx-1].ft);
    result->gm_gds = table->entries[idx-1].gm_gds +
                  frac * (table->entries[idx].gm_gds - table->entries[idx-1].gm_gds);
    result->vdsat = table->entries[idx-1].vdsat +
                  frac * (table->entries[idx].vdsat - table->entries[idx-1].vdsat);

    return true;
}

/* ─────────────────────────────────────────────────────────
 * Common-Source Amplifier Design
 *
 * Design procedure (Sedra & Smith §7.3):
 * 1. Choose bias Id for power/budget
 * 2. Vds ≈ Vdd/2 for maximum output swing
 * 3. Vov = 0.15-0.3V for good gm efficiency
 * 4. Compute W/L from Id and Vov
 * 5. Design bias network (voltage divider for discrete)
 * 6. Verify gain: Av = -gm*(ro||Rd||RL)
 * 7. Verify bandwidth: OCTC estimate
 * 8. Verify noise: NF calculation
 * ───────────────────────────────────────────────────────── */

AmpDesignResult fet_design_cs_amplifier(const AmpDesignSpec *spec,
                                         const FetTechParams *tech)
{
    AmpDesignResult result;
    memset(&result, 0, sizeof(result));
    result.config = FET_AMP_COMMON_SOURCE;
    result.spec_met = true;

    if (!spec || !tech) {
        result.spec_met = false;
        return result;
    }

    double vdd = spec->vdd;
    if (vdd <= 0.0) vdd = 5.0;

    /* Build device model */
    FetDevice fet;
    memset(&fet, 0, sizeof(fet));
    fet.type = FET_MOSFET_N_ENHANCEMENT;
    fet.tech = *tech;
    fet.temperature = spec->temperature;
    if (fet.temperature <= 0.0) fet.temperature = 300.0;

    /* Device geometry from gm/Id or square-law */
    double target_id = spec->power_target / vdd;
    if (target_id <= 0.0) target_id = 1e-3;  /* 1 mA default */

    double vov = 0.2;  /* 200 mV overdrive */
    double vth = tech->vt_n;
    if (vth <= 0.0) vth = 0.7;

    double cox = tech->cox;
    if (cox <= 0.0) cox = fet_cox_from_tox(tech->tox);

    double kp = fet_kprime(tech->mu_n, cox);

    /* W/L from square-law: Id = (1/2)*kp*(W/L)*Vov^2 */
    double wl = 0.0;
    if (kp > 0.0 && vov > 0.0) {
        wl = (2.0 * target_id) / (kp * vov * vov);
    }
    if (wl <= 0.0) wl = 10.0;

    double l = tech->ld * 2.0 + spec->l_min;
    if (l <= 0.0) l = 1e-6;  /* 1 μm default */
    double w = wl * l;

    fet.geo.w = w;
    fet.geo.l = l;
    fet.geo.w_over_l = wl;

    /* Bias design */
    BiasSpec bias_spec;
    memset(&bias_spec, 0, sizeof(bias_spec));
    bias_spec.target_id  = target_id;
    bias_spec.target_vds = vdd / 2.0;
    bias_spec.target_vov = vov;
    bias_spec.vdd        = vdd;
    bias_spec.rin_min    = spec->rin_target;
    bias_spec.source_degenerated = false;

    BiasResult bias_result;
    fet_bias_design(&fet, &bias_spec, &bias_result);

    result.fet    = fet;
    result.bias   = bias_result.network;
    result.qpoint = bias_result.qpoint;
    result.power_actual = bias_result.power_dc;

    /* Small-signal analysis */
    HybridPiModel hp = fet_extract_hybrid_pi(&fet);
    result.hp_model = hp;

    /* Performance metrics */
    FetAmpMetrics metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.config = FET_AMP_COMMON_SOURCE;
    metrics.qpoint = bias_result.qpoint;

    double rsig = spec->rs_source;
    double rl   = spec->rl_load;
    double cl   = spec->cl_load;
    double rd_val = bias_result.network.rd;
    double gm_val  = bias_result.qpoint.gm;
    double ro_val  = (bias_result.qpoint.gds > 0.0)
                     ? 1.0 / bias_result.qpoint.gds : INFINITY;

    /* Gain */
    metrics.av = fet_cs_gain_ideal(gm_val, ro_val, rd_val);
    metrics.av_db = (fabs(metrics.av) > 0.0)
                    ? 20.0 * log10(fabs(metrics.av)) : -200.0;
    metrics.gm_eff = gm_val;

    /* Input impedance */
    double rg1 = bias_result.network.rg1;
    double rg2 = bias_result.network.rg2;
    if (rg1 > 0.0 && rg2 > 0.0) {
        metrics.rin = (rg1 * rg2) / (rg1 + rg2);
    } else if (rg1 > 0.0) {
        metrics.rin = rg1;
    } else if (rg2 > 0.0) {
        metrics.rin = rg2;
    } else {
        metrics.rin = 1e12;  /* Effectively infinite for MOSFET */
    }

    /* Output impedance */
    double rout_val = ro_val;
    if (rd_val > 0.0 && rd_val < INFINITY) {
        rout_val = (ro_val < INFINITY)
                   ? (ro_val * rd_val) / (ro_val + rd_val) : rd_val;
    }
    metrics.rout = rout_val;

    /* Miller input capacitance */
    MillerDecomposition md = fet_miller_decompose(hp.cgd, metrics.av, 1e6);
    metrics.cin = hp.cgs + md.c_input_miller;
    metrics.rin_w_miller = (rout_val > 0.0) ? rout_val : metrics.rin;

    /* Bandwidth via OCTC */
    OctcResult octc = fet_octc_common_source(&hp, rd_val, rl, rsig, 0.0, cl);
    result.octc = octc;
    metrics.f_low = 0.0;  /* DC-coupled, low-f set by coupling caps */
    metrics.f_high = octc.fh_estimate;
    metrics.bw = metrics.f_high - metrics.f_low;
    metrics.gbw = fet_gain_bandwidth_product(fabs(metrics.av), metrics.bw);

    /* Noise figure */
    FetNoiseParams fnp;
    memset(&fnp, 0, sizeof(fnp));
    fnp.gamma = 0.667;  /* Long-channel */
    fnp.delta = 1.333;
    fnp.kf    = 1e-25;
    fnp.af    = 1.0;
    fnp.ef    = 1.0;
    fnp.rg    = tech->rsh * w / l;
    fnp.n_fingers = fet.geo.nf > 0 ? (double)fet.geo.nf : 1.0;

    FetSmallSignalParams ssp;
    memset(&ssp, 0, sizeof(ssp));
    ssp.gm  = gm_val;
    ssp.cgs = hp.cgs;

    FetNoiseFigure nf_calc = fet_noise_figure_calculate(&fnp, &ssp,
                                 rsig, 0.0, 1e6, spec->temperature);
    result.nf_result = nf_calc;
    metrics.vn_input  = sqrt(nf_calc.vn_sv_total);
    metrics.nf_db     = nf_calc.noise_figure_db;
    metrics.nf_min_db = nf_calc.nf_min_db;

    /* Distortion */
    double gm_val_vv = (vov > 0.0) ? 2.0 * target_id / vov : gm_val;
    double gm2 = (vov > 0.0) ? target_id / (vov * vov) : 0.0;
    double gm3 = 0.0;  /* Pure square-law: no 3rd-order */
    fet_distortion_estimate(gm_val_vv, gm2, gm3, 0.001,
                            &metrics.hd2_1mv, &metrics.hd3_1mv);

    result.metrics = metrics;

    /* Verify specifications */
    if (spec->target_gain_db > 0.0 && metrics.av_db < spec->target_gain_db) {
        result.spec_fail_count++;
        result.spec_met = false;
    }
    if (spec->target_bw_hz > 0.0 && metrics.bw < spec->target_bw_hz) {
        result.spec_fail_count++;
        result.spec_met = false;
    }
    if (spec->nf_max_db > 0.0 && metrics.nf_db > spec->nf_max_db) {
        result.spec_fail_count++;
        result.spec_met = false;
    }
    if (spec->power_max > 0.0 && result.power_actual > spec->power_max) {
        result.spec_fail_count++;
        result.spec_met = false;
    }
    if (spec->rin_target > 0.0 && metrics.rin < spec->rin_target) {
        result.spec_fail_count++;
        result.spec_met = false;
    }

    result.efficiency_pct = 0.0;  /* Class-A: theoretical max 25% */

    return result;
}

/* ─────────────────────────────────────────────────────────
 * Common-Drain (Source Follower) Amplifier Design
 *
 * Key properties:
 * - Av ≈ 1 (voltage buffer)
 * - High Rin, low Rout
 * - Rout ≈ 1/gm (without body effect)
 * - Used for impedance transformation and level shifting
 * ───────────────────────────────────────────────────────── */

AmpDesignResult fet_design_cd_amplifier(const AmpDesignSpec *spec,
                                         const FetTechParams *tech)
{
    AmpDesignResult result;
    memset(&result, 0, sizeof(result));
    result.config = FET_AMP_COMMON_DRAIN;
    result.spec_met = true;

    if (!spec || !tech) {
        result.spec_met = false;
        return result;
    }

    double vdd = spec->vdd;
    if (vdd <= 0.0) vdd = 5.0;

    /* For source follower, drain is at AC ground (Vdd) */
    double target_id = spec->power_target / vdd;
    if (target_id <= 0.0) target_id = 1e-3;

    double vth = tech->vt_n;
    if (vth <= 0.0) vth = 0.7;
    double vov = 0.2;

    double cox = tech->cox;
    if (cox <= 0.0) cox = fet_cox_from_tox(tech->tox);
    double kp = fet_kprime(tech->mu_n, cox);

    /* W/L */
    double wl = (kp > 0.0 && vov > 0.0) ? (2.0 * target_id) / (kp * vov * vov) : 10.0;
    double l = spec->l_min > 0.0 ? spec->l_min : 1e-6;
    double w = wl * l;

    FetDevice fet;
    memset(&fet, 0, sizeof(fet));
    fet.type = FET_MOSFET_N_ENHANCEMENT;
    fet.tech = *tech;
    fet.temperature = spec->temperature > 0.0 ? spec->temperature : 300.0;
    fet.geo.w = w;
    fet.geo.l = l;
    fet.geo.w_over_l = wl;
    fet.tech.cox = cox;

    /* Source follower bias: Rs in source, gate at Vgg */
    double rs_val = (vdd / 2.0) / target_id;  /* Vout_DC ≈ Vdd/2 */
    double vgs_needed = vth + vov;
    double vgg_needed = vgs_needed + vdd / 2.0;  /* Vs = Vdd/2, Vg = Vs + Vgs */

    if (vgg_needed > vdd) vgg_needed = vdd;

    BiasSpec bias_spec;
    memset(&bias_spec, 0, sizeof(bias_spec));
    bias_spec.target_id  = target_id;
    bias_spec.target_vds = vdd / 2.0;
    bias_spec.target_vov = vov;
    bias_spec.vdd        = vdd;
    bias_spec.rin_min    = spec->rin_target > 0.0 ? spec->rin_target : 100e3;
    bias_spec.source_degenerated = false;
    bias_spec.rs_degen   = rs_val;

    BiasResult bias_result;
    fet_bias_design(&fet, &bias_spec, &bias_result);

    /* Adjust: source follower has drain at Vdd, source at Id*Rs */
    bias_result.network.rd = 0.0;  /* Drain directly to Vdd */
    bias_result.network.rs = rs_val;
    bias_result.qpoint.vds = vdd - target_id * rs_val;

    result.fet    = fet;
    result.bias   = bias_result.network;
    result.qpoint = bias_result.qpoint;
    result.power_actual = vdd * target_id;

    /* Metrics */
    HybridPiModel hp = fet_extract_hybrid_pi(&fet);
    result.hp_model = hp;

    double gm_val = bias_result.qpoint.gm;
    double gmb_val = hp.gmb;
    double ro_val = (bias_result.qpoint.gds > 0.0)
                    ? 1.0 / bias_result.qpoint.gds : INFINITY;

    FetAmpMetrics metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.config = FET_AMP_COMMON_DRAIN;
    metrics.qpoint = bias_result.qpoint;
    metrics.av = fet_cd_gain_ideal(gm_val, gmb_val, ro_val, rs_val);
    metrics.av_db = (fabs(metrics.av) > 0.0)
                    ? 20.0 * log10(fabs(metrics.av)) : -200.0;
    metrics.gm_eff = gm_val;

    /* Input resistance very high (gate) */
    double rg1 = bias_result.network.rg1;
    double rg2 = bias_result.network.rg2;
    if (rg1 > 0.0 && rg2 > 0.0 && rg1 < INFINITY && rg2 < INFINITY) {
        metrics.rin = (rg1 * rg2) / (rg1 + rg2);
    } else {
        metrics.rin = 1e12;
    }

    /* Output resistance */
    metrics.rout = fet_cd_output_resistance(gm_val, gmb_val, ro_val, rs_val);

    /* Bandwidth: source follower typically very wide */
    double cl = spec->cl_load;
    if (cl <= 0.0) cl = 1e-12;
    /* Dominant pole from output: fp = 1/(2π * Rout * CL) */
    metrics.f_high = (metrics.rout > 0.0)
                     ? 1.0 / (2.0 * M_PI * metrics.rout * cl) : INFINITY;
    metrics.bw = metrics.f_high;
    metrics.gbw = metrics.bw;  /* Av ≈ 1 */

    result.metrics = metrics;

    if (spec->target_bw_hz > 0.0 && metrics.bw < spec->target_bw_hz) {
        result.spec_fail_count++;
        result.spec_met = false;
    }

    return result;
}

/* ─────────────────────────────────────────────────────────
 * Full Amplifier Performance Analysis
 * ───────────────────────────────────────────────────────── */

FetAmpMetrics fet_analyze_amplifier(const FetDevice *fet,
                                     const BiasNetwork *bias,
                                     FetAmpConfig config,
                                     double rs_source, double rl_load,
                                     double cl_load)
{
    FetAmpMetrics metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.config = config;

    if (!fet || !bias) return metrics;

    HybridPiModel hp = fet_extract_hybrid_pi(fet);
    double gm  = fet->bias.gm;
    double gds = fet->bias.gds;
    double ro  = (gds > 0.0) ? 1.0 / gds : INFINITY;
    double rd  = bias->rd;

    metrics.qpoint = fet->bias;
    metrics.gm_eff = gm;

    switch (config) {
    case FET_AMP_COMMON_SOURCE:
        metrics.av = fet_cs_gain_ideal(gm, ro, rd);
        break;
    case FET_AMP_COMMON_GATE:
        metrics.av = fet_cg_gain_ideal(gm, ro, rd, rs_source);
        break;
    case FET_AMP_COMMON_DRAIN: {
        double rs = bias->rs;
        double gmb = hp.gmb;
        metrics.av = fet_cd_gain_ideal(gm, gmb, ro, rs);
        break;
    }
    case FET_AMP_CASCODE:
        /* Cascode gain ≈ CS gain (output resistance enhanced) */
        metrics.av = fet_cs_gain_ideal(gm, ro * (1.0 + gm * (1.0/gm)), rd);
        break;
    default:
        metrics.av = 0.0;
        break;
    }

    metrics.av_db = (fabs(metrics.av) > 0.0)
                    ? 20.0 * log10(fabs(metrics.av)) : -200.0;

    /* Input resistance */
    double rg1 = bias->rg1;
    double rg2 = bias->rg2;
    if (rg1 > 0.0 && rg2 > 0.0 && rg1 < INFINITY && rg2 < INFINITY) {
        metrics.rin = (rg1 * rg2) / (rg1 + rg2);
    } else if (rg1 > 0.0 && rg1 < INFINITY) {
        metrics.rin = rg1;
    } else if (rg2 > 0.0 && rg2 < INFINITY) {
        metrics.rin = rg2;
    } else {
        metrics.rin = 1e12;
    }

    /* Output resistance */
    if (rd > 0.0 && ro < INFINITY) {
        metrics.rout = (ro * rd) / (ro + rd);
    } else if (rd > 0.0) {
        metrics.rout = rd;
    } else {
        metrics.rout = ro;
    }

    /* Bandwidth via OCTC */
    OctcResult octc;
    if (config == FET_AMP_COMMON_SOURCE) {
        octc = fet_octc_common_source(&hp, rd, rl_load, rs_source, 0.0, cl_load);
    } else if (config == FET_AMP_COMMON_GATE) {
        octc = fet_octc_common_gate(&hp, rd, rl_load, rs_source);
    } else {
        octc = fet_octc_common_source(&hp, rd, rl_load, rs_source, 0.0, cl_load);
    }
    metrics.f_high = octc.fh_estimate;
    metrics.bw     = metrics.f_high;
    metrics.gbw    = fet_gain_bandwidth_product(fabs(metrics.av), metrics.bw);

    return metrics;
}

/* ─────────────────────────────────────────────────────────
 * Cascode Amplifier Design
 *
 * The cascode topology (CS + CG) provides:
 * - Wide bandwidth (Miller effect suppressed)
 * - High output resistance (≈ gm2*ro1*ro2)
 * - Good reverse isolation
 *
 * Design: Both transistors share drain current.
 * CS: W/L chosen for gm/gain
 * CG: W/L chosen for output resistance and bandwidth
 * ───────────────────────────────────────────────────────── */

AmpDesignResult fet_design_cascode_amplifier(const AmpDesignSpec *spec,
                                              const FetTechParams *tech)
{
    AmpDesignResult result;
    memset(&result, 0, sizeof(result));
    result.config = FET_AMP_CASCODE;
    result.spec_met = true;

    if (!spec || !tech) {
        result.spec_met = false;
        return result;
    }

    double vdd = spec->vdd;
    if (vdd <= 0.0) vdd = 5.0;

    /* Both transistors share the same Id */
    double target_id = spec->power_target / vdd;
    if (target_id <= 0.0) target_id = 1e-3;

    double vth = tech->vt_n;
    if (vth <= 0.0) vth = 0.7;
    double vov = 0.15;

    double cox = tech->cox;
    if (cox <= 0.0) cox = fet_cox_from_tox(tech->tox);
    double kp = fet_kprime(tech->mu_n, cox);

    double wl = (kp > 0.0 && vov > 0.0) ? (2.0 * target_id) / (kp * vov * vov) : 10.0;
    double l = spec->l_min > 0.0 ? spec->l_min : 1e-6;
    double w = wl * l;

    /* Both transistors same size (typical) */
    FetDevice fet_cs;
    memset(&fet_cs, 0, sizeof(fet_cs));
    fet_cs.type = FET_MOSFET_N_ENHANCEMENT;
    fet_cs.tech = *tech;
    fet_cs.tech.cox = cox;
    fet_cs.temperature = spec->temperature > 0.0 ? spec->temperature : 300.0;
    fet_cs.geo.w = w;
    fet_cs.geo.l = l;
    fet_cs.geo.w_over_l = wl;

    /* Bias: CS gate at Vgg1, CG gate at Vgg2 (typically Vdd/2+ for cascode) */
    BiasSpec bias_spec;
    memset(&bias_spec, 0, sizeof(bias_spec));
    bias_spec.target_id  = target_id;
    bias_spec.target_vov = vov;
    bias_spec.vdd        = vdd;
    bias_spec.rin_min    = spec->rin_target > 0.0 ? spec->rin_target : 100e3;
    bias_spec.source_degenerated = false;

    BiasResult bias_result_cs;
    fet_bias_design(&fet_cs, &bias_spec, &bias_result_cs);

    result.fet    = fet_cs;
    result.bias   = bias_result_cs.network;
    result.qpoint = bias_result_cs.qpoint;
    result.power_actual = vdd * target_id;

    /* Metrics for cascode */
    HybridPiModel hp = fet_extract_hybrid_pi(&fet_cs);
    result.hp_model = hp;

    double gm_val = bias_result_cs.qpoint.gm;
    double ro_val = (bias_result_cs.qpoint.gds > 0.0)
                    ? 1.0 / bias_result_cs.qpoint.gds : INFINITY;
    double rd_val = bias_result_cs.network.rd;

    /* Cascode output resistance: ro_cascode ≈ gm*cg * ro_cs * ro_cg
     * Approximate: rout ≈ gm * ro * ro ≈ ro * (gm*ro) [intrinsic gain squared]
     */
    double intrinsic_gain = gm_val * ro_val;
    double ro_cascode = ro_val * intrinsic_gain;

    /* Gain: Av ≈ -gm_cs * ro_cascode ≈ -(gm*ro)^2 */
    FetAmpMetrics metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.config = FET_AMP_CASCODE;
    metrics.qpoint = bias_result_cs.qpoint;
    metrics.av = -gm_val * ro_cascode;
    metrics.av_db = (fabs(metrics.av) > 0.0)
                    ? 20.0 * log10(fabs(metrics.av)) : -200.0;
    metrics.gm_eff = gm_val;

    /* Input/output impedance */
    metrics.rin = bias_result_cs.network.rgg > 0.0
                  ? bias_result_cs.network.rgg : 1e12;
    metrics.rout = ro_cascode;

    /* Bandwidth: OCTC for cascode */
    OctcResult octc = fet_octc_cascode(&hp, &hp, rd_val, spec->rl_load,
                                        spec->rs_source);
    result.octc = octc;
    metrics.f_high = octc.fh_estimate;
    metrics.bw = metrics.f_high;
    metrics.gbw = fet_gain_bandwidth_product(fabs(metrics.av), metrics.bw);

    result.metrics = metrics;

    if (spec->target_gain_db > 0.0 && metrics.av_db < spec->target_gain_db) {
        result.spec_fail_count++;
        result.spec_met = false;
    }
    if (spec->target_bw_hz > 0.0 && metrics.bw < spec->target_bw_hz) {
        result.spec_fail_count++;
        result.spec_met = false;
    }

    return result;
}

/* ─────────────────────────────────────────────────────────
 * Common-Gate Amplifier Design
 * ───────────────────────────────────────────────────────── */

AmpDesignResult fet_design_cg_amplifier(const AmpDesignSpec *spec,
                                         const FetTechParams *tech)
{
    AmpDesignResult result;
    memset(&result, 0, sizeof(result));
    result.config = FET_AMP_COMMON_GATE;
    result.spec_met = true;

    if (!spec || !tech) return result;

    double vdd = spec->vdd;
    if (vdd <= 0.0) vdd = 5.0;
    double target_id = spec->power_target / vdd;
    if (target_id <= 0.0) target_id = 1e-3;

    double vth = tech->vt_n;
    if (vth <= 0.0) vth = 0.7;
    double vov = 0.2;

    double cox = tech->cox;
    if (cox <= 0.0) cox = fet_cox_from_tox(tech->tox);
    double kp = fet_kprime(tech->mu_n, cox);
    double wl = (kp > 0.0) ? (2.0 * target_id) / (kp * vov * vov) : 10.0;
    double l = spec->l_min > 0.0 ? spec->l_min : 1e-6;

    FetDevice fet;
    memset(&fet, 0, sizeof(fet));
    fet.type = FET_MOSFET_N_ENHANCEMENT;
    fet.tech = *tech;
    fet.tech.cox = cox;
    fet.temperature = spec->temperature > 0.0 ? spec->temperature : 300.0;
    fet.geo.w = wl * l;
    fet.geo.l = l;
    fet.geo.w_over_l = wl;

    BiasSpec bias_spec;
    memset(&bias_spec, 0, sizeof(bias_spec));
    bias_spec.target_id  = target_id;
    bias_spec.target_vds = vdd / 2.0;
    bias_spec.target_vov = vov;
    bias_spec.vdd        = vdd;
    bias_spec.rin_min    = 50.0;  /* CG has low input Z */

    BiasResult bias_result;
    fet_bias_design(&fet, &bias_spec, &bias_result);

    result.fet    = fet;
    result.bias   = bias_result.network;
    result.qpoint = bias_result.qpoint;
    result.power_actual = vdd * target_id;

    HybridPiModel hp = fet_extract_hybrid_pi(&fet);
    result.hp_model = hp;

    double gm_val = bias_result.qpoint.gm;
    double ro_val = (bias_result.qpoint.gds > 0.0)
                    ? 1.0 / bias_result.qpoint.gds : INFINITY;

    FetAmpMetrics metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.config = FET_AMP_COMMON_GATE;
    metrics.qpoint = bias_result.qpoint;
    metrics.av = fet_cg_gain_ideal(gm_val, ro_val, bias_result.network.rd,
                                    spec->rs_source);
    metrics.av_db = (fabs(metrics.av) > 0.0)
                    ? 20.0 * log10(fabs(metrics.av)) : -200.0;
    metrics.gm_eff = gm_val;

    /* CG input resistance ≈ 1/gm */
    metrics.rin = (gm_val > 0.0) ? 1.0 / gm_val : INFINITY;
    metrics.rout = (ro_val < INFINITY && bias_result.network.rd < INFINITY)
                   ? (ro_val * bias_result.network.rd) /
                     (ro_val + bias_result.network.rd)
                   : bias_result.network.rd;

    OctcResult octc = fet_octc_common_gate(&hp, bias_result.network.rd,
                                            spec->rl_load, spec->rs_source);
    result.octc = octc;
    metrics.f_high = octc.fh_estimate;
    metrics.bw = metrics.f_high;
    metrics.gbw = fet_gain_bandwidth_product(fabs(metrics.av), metrics.bw);

    result.metrics = metrics;
    return result;
}

/* ─────────────────────────────────────────────────────────
 * Differential Pair Amplifier Design
 * ───────────────────────────────────────────────────────── */

AmpDesignResult fet_design_diffpair_amplifier(const AmpDesignSpec *spec,
                                               const FetTechParams *tech)
{
    AmpDesignResult result;
    memset(&result, 0, sizeof(result));
    result.config = FET_AMP_DIFFERENTIAL_PAIR;
    result.spec_met = true;

    if (!spec || !tech) {
        result.spec_met = false;
        return result;
    }

    double vdd = spec->vdd;
    if (vdd <= 0.0) vdd = 5.0;
    double vss = spec->vss;

    /* Tail current: Itail = 2 * Id_per_transistor */
    double id_per = spec->power_target / (vdd - vss);
    if (id_per <= 0.0) id_per = 500e-6;
    double itail = 2.0 * id_per;

    double vth = tech->vt_n;
    if (vth <= 0.0) vth = 0.7;
    double vov = 0.15;

    double cox = tech->cox;
    if (cox <= 0.0) cox = fet_cox_from_tox(tech->tox);
    double kp = fet_kprime(tech->mu_n, cox);
    double wl = (kp > 0.0) ? (2.0 * id_per) / (kp * vov * vov) : 10.0;
    double l = spec->l_min > 0.0 ? spec->l_min : 1e-6;

    FetDevice fet;
    memset(&fet, 0, sizeof(fet));
    fet.type = FET_MOSFET_N_ENHANCEMENT;
    fet.tech = *tech;
    fet.tech.cox = cox;
    fet.temperature = spec->temperature > 0.0 ? spec->temperature : 300.0;
    fet.geo.w = wl * l;
    fet.geo.l = l;
    fet.geo.w_over_l = wl;

    /* Bias: each transistor at Id = itail/2 */
    BiasSpec bias_spec;
    memset(&bias_spec, 0, sizeof(bias_spec));
    bias_spec.target_id  = id_per;
    bias_spec.target_vds = vdd / 2.0 - vss;
    bias_spec.target_vov = vov;
    bias_spec.vdd        = vdd;
    bias_spec.vss        = vss;

    BiasResult bias_result;
    fet_bias_design(&fet, &bias_spec, &bias_result);

    result.fet    = fet;
    result.bias   = bias_result.network;
    result.qpoint = bias_result.qpoint;
    result.power_actual = (vdd - vss) * itail;

    HybridPiModel hp = fet_extract_hybrid_pi(&fet);
    result.hp_model = hp;

    double gm_val = bias_result.qpoint.gm;
    double ro_val = (bias_result.qpoint.gds > 0.0)
                    ? 1.0 / bias_result.qpoint.gds : INFINITY;
    double rd_val = bias_result.network.rd;

    /* Differential gain: Adm = gm * (ro || Rd) */
    double adm = fet_cs_gain_ideal(gm_val, ro_val, rd_val);

    /* CMRR estimate */
    double rss = ro_val;  /* Tail current source output resistance */
    (void)fet_diffpair_cmrr(gm_val, ro_val, rd_val, rss);  /* Available for CMRR check */

    FetAmpMetrics metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.config = FET_AMP_DIFFERENTIAL_PAIR;
    metrics.qpoint = bias_result.qpoint;
    metrics.av = adm;
    metrics.av_db = (fabs(adm) > 0.0) ? 20.0 * log10(fabs(adm)) : -200.0;
    metrics.gm_eff = gm_val;
    metrics.rin = 1e12;   /* Gate input: very high */
    metrics.rout = rd_val;

    OctcResult octc = fet_octc_common_source(&hp, rd_val, spec->rl_load,
                                              spec->rs_source, 0.0, spec->cl_load);
    result.octc = octc;
    metrics.f_high = octc.fh_estimate;
    metrics.bw = metrics.f_high;
    metrics.gbw = fet_gain_bandwidth_product(fabs(metrics.av), metrics.bw);

    result.metrics = metrics;
    return result;
}

/* ─────────────────────────────────────────────────────────
 * Differential Pair Analysis Functions
 * ───────────────────────────────────────────────────────── */

double fet_diffpair_cmrr(double gm, double ro, double rd, double rss)
{
    if (gm <= 0.0 || rss <= 0.0) return 0.0;

    /* Adm = gm * (ro || Rd) */
    double rout;
    if (ro <= 0.0 || ro == INFINITY) {
        rout = rd;
    } else if (rd <= 0.0) {
        rout = ro;
    } else {
        rout = (ro * rd) / (ro + rd);
    }
    double adm = gm * rout;

    /* Acm ≈ Rd / (2*Rss) [simplified] */
    double acm = (rd / (2.0 * rss));

    if (fabs(acm) < 1e-30) return INFINITY;

    double cmrr = fabs(adm / acm);
    return cmrr;
}

void fet_diffpair_icmr(double vdd, double vss, double vov_tail,
                        double vov_input, double vt_input,
                        double vov_load, double vt_load,
                        bool pmos_input, double *icmr_low, double *icmr_high)
{
    if (pmos_input) {
        /* PMOS input pair: ICMR extends toward Vss */
        /* ICMR_low = Vss + V_ov_tail + V_ov_input - |Vt_p| */
        if (icmr_low) {
            *icmr_low = vss + vov_tail + vov_input - fabs(vt_input);
        }
        /* ICMR_high = Vdd - V_ov_load - |Vt_p| */
        if (icmr_high) {
            *icmr_high = vdd - vov_load - fabs(vt_load);
        }
    } else {
        /* NMOS input pair: ICMR extends toward Vdd */
        /* ICMR_low = Vss + V_ov_tail + Vt_n */
        if (icmr_low) {
            *icmr_low = vss + vov_tail + vt_input;
        }
        /* ICMR_high = Vdd - V_ov_load - (V_ov_input + Vt_n) */
        if (icmr_high) {
            *icmr_high = vdd - vov_load - (vov_input + vt_input);
        }
    }
}

double fet_diffpair_systematic_offset(double delta_vth, double vov,
                                       double delta_wl_ratio,
                                       double delta_rd_ratio)
{
    /* Systematic offset voltage:
     * V_os = ΔVth + (Vov/2) * (Δ(W/L)/(W/L) + ΔRd/Rd)
     *
     * Reference: Gray & Meyer §4.5
     */
    double vos = delta_vth + (vov / 2.0) * (delta_wl_ratio + delta_rd_ratio);
    return vos;
}

/* ─────────────────────────────────────────────────────────
 * Current Mirror Functions
 * ───────────────────────────────────────────────────────── */

double fet_current_mirror_ratio(double wl_out, double wl_ref,
                                 double lambda, double vds_out, double vds_ref,
                                 double *actual_ratio, double *rout_mirror)
{
    if (wl_ref <= 0.0) {
        if (actual_ratio) *actual_ratio = 0.0;
        if (rout_mirror) *rout_mirror = INFINITY;
        return 0.0;
    }

    /* Ideal ratio */
    double ratio_ideal = wl_out / wl_ref;

    /* Channel-length modulation mismatch:
     * Iout/Iref = (W/L)_out/(W/L)_ref * (1+λ*Vds_out)/(1+λ*Vds_ref)
     */
    double lambda_corr = (1.0 + lambda * vds_out) / (1.0 + lambda * vds_ref);
    double ratio_actual = ratio_ideal * lambda_corr;

    if (actual_ratio) *actual_ratio = ratio_actual;

    /* Output resistance: ro_out = 1/(λ*Iout) */
    /* Return Rout = ro_out (small-signal output resistance of mirror) */
    if (rout_mirror) {
        /* Output resistance looking into mirror drain */
        *rout_mirror = (lambda > 0.0) ? 1.0 / (lambda * fabs(vds_out)) : INFINITY;
    }

    return ratio_actual;
}

double fet_cascode_mirror_rout(double gm2, double ro1, double ro2)
{
    /* Cascode output resistance: Rout ≈ gm2 * ro1 * ro2 */
    if (gm2 <= 0.0) return (ro2 > 0.0) ? ro2 : INFINITY;

    return gm2 * ro1 * ro2;
}

double fet_wilson_mirror_rout(double gm1, double gm2, double gm3,
                               double ro1, double ro2, double ro3,
                               double *iout_actual)
{
    /* Wilson current mirror: negative feedback increases output resistance.
     * Rout ≈ gm1 * gm3 * ro1 * ro2 * ro3 / (gm2)
     * iout ≈ iref * (W/L)_out / (W/L)_ref  [same as simple mirror]
     */
    if (iout_actual) {
        *iout_actual = 0.0;  /* Depends on reference current */
    }

    double product = gm2;
    if (fabs(product) < 1e-30) {
        return (ro3 > 0.0) ? ro3 : INFINITY;
    }

    double rout = gm1 * gm3 * ro1 * ro2 * ro3 / gm2;
    return rout;
}

void fet_wide_swing_cascode_bias(double vdd, double vov, double vt,
                                  double *vb1, double *vb2,
                                  double *vb3, double *vb4)
{
    (void)vdd;  /* Supply headroom check — reserved for future Vdd-dependent scaling */
    /* Wide-swing cascode bias generator:
     * Generates gate bias voltages for cascode current mirror
     * to maximize output voltage swing.
     *
     * Vb_cascode = Vov + Vt  [just enough to keep mirror devices in saturation]
     * Vb_mirror = Vov + Vt
     *
     * For NMOS cascode:
     *   Vb1 = Vt + 2*Vov     [for cascode device]
     *   Vb2 = Vt + Vov       [for mirror device]
     *   Vb3 = Vt + Vov       [for bias generator]
     *   Vb4 = Vt + Vov       [for bias generator]
     */
    if (vb1) *vb1 = vt + 2.0 * vov;
    if (vb2) *vb2 = vt + vov;
    if (vb3) *vb3 = vt + vov;
    if (vb4) *vb4 = vt + vov;
}

/* ─────────────────────────────────────────────────────────
 * LNA Design (Low-Noise Amplifier)
 *
 * Key design approach for RF LNAs:
 * - Inductive source degeneration for simultaneous noise and power match
 * - Optimize device width for minimum NF at target frequency
 * - Inductive gate degeneration for input matching
 *
 * Reference: Shaeffer & Lee (1997), JSSC Vol.32, pp.745-759
 * ───────────────────────────────────────────────────────── */

AmpDesignResult fet_design_lna(const AmpDesignSpec *spec,
                                const FetTechParams *tech,
                                double target_freq_hz)
{
    AmpDesignResult result;
    memset(&result, 0, sizeof(result));
    result.config = FET_AMP_COMMON_SOURCE;  /* LNA typically CS */
    result.spec_met = true;

    if (!spec || !tech) {
        result.spec_met = false;
        return result;
    }

    /* LNA-specific design: optimize for NF at target frequency */
    double vdd = spec->vdd;
    if (vdd <= 0.0) vdd = 1.8;  /* Typical RF CMOS supply */

    /* For LNA, typically use 5-15 mA */
    double target_id = 10e-3;
    if (spec->power_target > 0.0) {
        target_id = spec->power_target / vdd;
    }

    double vth = tech->vt_n;
    if (vth <= 0.0) vth = 0.5;
    double vov = 0.15;  /* Moderate overdrive for good ft */

    double cox = tech->cox;
    if (cox <= 0.0) cox = fet_cox_from_tox(tech->tox);
    double kp = fet_kprime(tech->mu_n, cox);

    double wl = (kp > 0.0) ? (2.0 * target_id) / (kp * vov * vov) : 50.0;
    double l = spec->l_min > 0.0 ? spec->l_min : 0.18e-6;
    double w = wl * l;

    /* For RF LNA: optimal W exists for minimum NF
     * W_opt ≈ 1 / (3*ω*Cox*Rs*L) [approximate]
     * where Rs = 50Ω source impedance
     */
    double rs_lna = spec->rs_source > 0.0 ? spec->rs_source : 50.0;
    double omega = 2.0 * M_PI * target_freq_hz;
    double w_opt = 1.0 / (3.0 * omega * cox * rs_lna * l);
    if (w_opt > 0.0 && w_opt > w) {
        w = w_opt;  /* Use optimal width for NF */
        wl = w / l;
    }

    FetDevice fet;
    memset(&fet, 0, sizeof(fet));
    fet.type = FET_MOSFET_N_ENHANCEMENT;
    fet.tech = *tech;
    fet.tech.cox = cox;
    fet.temperature = spec->temperature > 0.0 ? spec->temperature : 300.0;
    fet.geo.w = w;
    fet.geo.l = l;
    fet.geo.w_over_l = wl;
    fet.geo.nf = 10;  /* Multi-finger for low Rg */

    BiasSpec bias_spec;
    memset(&bias_spec, 0, sizeof(bias_spec));
    bias_spec.target_id  = target_id;
    bias_spec.target_vds = vdd / 2.0;
    bias_spec.target_vov = vov;
    bias_spec.vdd        = vdd;
    bias_spec.rin_min    = 50.0;

    BiasResult bias_result;
    fet_bias_design(&fet, &bias_spec, &bias_result);

    result.fet    = fet;
    result.bias   = bias_result.network;
    result.qpoint = bias_result.qpoint;
    result.power_actual = vdd * target_id;

    HybridPiModel hp = fet_extract_hybrid_pi(&fet);
    result.hp_model = hp;

    double gm_val = bias_result.qpoint.gm;

    /* S-parameters for RF characterization */
    FetYParams yp = fet_hp_to_yparams(&hp, target_freq_hz);
    FetSParams sp = fet_yparams_to_sparams(&yp, 50.0);

    double s21_db = 20.0 * log10(cabs(sp.s21));

    FetAmpMetrics metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.config = FET_AMP_COMMON_SOURCE;
    metrics.qpoint = bias_result.qpoint;
    metrics.av = cabs(sp.s21);
    metrics.av_db = s21_db;
    metrics.gm_eff = gm_val;
    metrics.rin = 50.0;  /* Matched input */
    metrics.rout = 50.0;  /* Matched output */

    /* NF estimation at RF */
    FetNoiseParams fnp;
    memset(&fnp, 0, sizeof(fnp));
    fnp.gamma = 2.0;   /* Short-channel γ */
    fnp.delta = 4.0;   /* Short-channel δ */
    fnp.kf    = 1e-25;
    fnp.af    = 1.0;
    fnp.ef    = 1.0;
    fnp.rg    = tech->rsh * w / l;
    fnp.n_fingers = 10.0;

    FetSmallSignalParams ssp;
    memset(&ssp, 0, sizeof(ssp));
    ssp.gm  = gm_val;
    ssp.cgs = hp.cgs;

    FetNoiseFigure nf_lna = fet_noise_figure_calculate(&fnp, &ssp,
                              50.0, 0.0, target_freq_hz, spec->temperature);
    result.nf_result = nf_lna;
    metrics.nf_db     = nf_lna.noise_figure_db;
    metrics.nf_min_db = nf_lna.nf_min_db;

    /* ft = gm/(2π*Cgs) */
    double ft_val = (hp.cgs > 0.0) ? gm_val / (2.0 * M_PI * hp.cgs) : 0.0;
    metrics.f_high = ft_val;
    metrics.bw = target_freq_hz * 0.2;  /* ~20% fractional BW typical */
    metrics.gbw = ft_val;

    result.metrics = metrics;
    result.efficiency_pct = 100.0 * 0.0;  /* LNA efficiency not primary metric */
    return result;
}

/* ─────────────────────────────────────────────────────────
 * Power Amplifier (PA) Design
 * ───────────────────────────────────────────────────────── */

AmpDesignResult fet_design_pa(const AmpDesignSpec *spec,
                               const FetTechParams *tech,
                               double target_freq_hz,
                               double *pae_actual)
{
    AmpDesignResult result;
    memset(&result, 0, sizeof(result));
    result.config = FET_AMP_COMMON_SOURCE;
    result.spec_met = true;

    if (!spec || !tech) {
        result.spec_met = false;
        if (pae_actual) *pae_actual = 0.0;
        return result;
    }

    if (pae_actual) *pae_actual = 0.0;

    double vdd = spec->vdd;
    if (vdd <= 0.0) vdd = 3.3;

    /* PA: large device, high current for output power */
    double pout_target = spec->power_target;
    if (pout_target <= 0.0) pout_target = 0.1;  /* 100 mW */

    /* Class-A theoretical max efficiency = 50% (inductive load) */
    double pdc_target = pout_target / 0.4;  /* Target 40% PAE */
    double target_id = pdc_target / vdd;

    double vth = tech->vt_n;
    if (vth <= 0.0) vth = 0.7;
    double vov = 0.3;  /* Higher Vov for PA linearity */

    double cox = tech->cox;
    if (cox <= 0.0) cox = fet_cox_from_tox(tech->tox);
    double kp = fet_kprime(tech->mu_n, cox);
    double wl = (kp > 0.0) ? (2.0 * target_id) / (kp * vov * vov) : 100.0;
    double l = spec->l_min > 0.0 ? spec->l_min : 0.35e-6;
    double w = wl * l;

    FetDevice fet;
    memset(&fet, 0, sizeof(fet));
    fet.type = FET_MOSFET_N_ENHANCEMENT;
    fet.tech = *tech;
    fet.tech.cox = cox;
    fet.temperature = spec->temperature > 0.0 ? spec->temperature : 300.0;
    fet.geo.w = w;
    fet.geo.l = l;
    fet.geo.w_over_l = wl;

    BiasSpec bias_spec;
    memset(&bias_spec, 0, sizeof(bias_spec));
    bias_spec.target_id  = target_id;
    bias_spec.target_vds = vdd / 2.0;
    bias_spec.target_vov = vov;
    bias_spec.vdd        = vdd;

    BiasResult bias_result;
    fet_bias_design(&fet, &bias_spec, &bias_result);

    result.fet    = fet;
    result.bias   = bias_result.network;
    result.qpoint = bias_result.qpoint;
    result.power_actual = vdd * target_id;

    HybridPiModel hp = fet_extract_hybrid_pi(&fet);
    result.hp_model = hp;

    double gm_val = bias_result.qpoint.gm;

    /* Large-signal PA analysis:
     * Pout_max ≈ (Vdd - Vdsat)^2 / (2 * Rload_opt)
     * Ropt ≈ (Vdd - Vdsat) / Imax
     */
    double vdsat = bias_result.qpoint.vdsat;
    double vswing = vdd - vdsat;

    /* Load-pull approximation: Ropt ≈ Vswing^2 / (2*Pout) */
    double rout_opt = (vswing * vswing) / (2.0 * pout_target);

    /* PAE = (Pout - Pin) / Pdc
     * Class-A: PAE_max = 50%
     * Actual depends on gain: Pin = Pout/Gain
     */
    double gain_pa = gm_val * rout_opt;
    double pin_pa  = (gain_pa > 0.0) ? pout_target / gain_pa : 0.0;
    double pae = (pout_target - pin_pa) / result.power_actual * 100.0;
    if (pae > 50.0) pae = 50.0;  /* Can't exceed Class-A physical limit */
    if (pae < 0.0) pae = 0.0;

    if (pae_actual) *pae_actual = pae;
    result.efficiency_pct = pae;

    FetAmpMetrics metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.config = FET_AMP_COMMON_SOURCE;
    metrics.qpoint = bias_result.qpoint;
    metrics.av = gain_pa;
    metrics.av_db = (gain_pa > 0.0) ? 20.0 * log10(gain_pa) : 0.0;
    metrics.gm_eff = gm_val;
    metrics.rout = rout_opt;
    metrics.rin = bias_result.network.rgg > 0.0 ? bias_result.network.rgg : 50.0;
    metrics.f_high = target_freq_hz;
    metrics.bw = target_freq_hz * 0.1;

    result.metrics = metrics;
    return result;
}

/* ─────────────────────────────────────────────────────────
 * Instrumentation Amplifier Design
 * ───────────────────────────────────────────────────────── */

AmpDesignResult fet_design_instrumentation_amp(const AmpDesignSpec *spec,
                                                const FetTechParams *tech,
                                                InAmpMetrics *inamp)
{
    AmpDesignResult result;
    memset(&result, 0, sizeof(result));
    result.config = FET_AMP_DIFFERENTIAL_PAIR;
    result.spec_met = true;

    if (!spec || !tech) {
        result.spec_met = false;
        if (inamp) memset(inamp, 0, sizeof(*inamp));
        return result;
    }
    if (inamp) memset(inamp, 0, sizeof(*inamp));

    /* In-Amp: very high input Z (FET gate), high CMRR
     * Typical two-opamp or three-opamp topology.
     * Here: simple diff pair front-end with FET inputs.
     */
    double vdd = spec->vdd;
    if (vdd <= 0.0) vdd = 5.0;
    double vss = spec->vss;

    double id_per = 100e-6;  /* Low power for instrumentation */
    double itail = 2.0 * id_per;

    double vth = tech->vt_n;
    if (vth <= 0.0) vth = 0.7;
    double vov = 0.15;

    double cox = tech->cox;
    if (cox <= 0.0) cox = fet_cox_from_tox(tech->tox);
    double kp = fet_kprime(tech->mu_n, cox);
    double wl = (kp > 0.0) ? (2.0 * id_per) / (kp * vov * vov) : 20.0;
    double l = spec->l_min > 0.0 ? spec->l_min : 2e-6;  /* Longer L for low noise */
    double w = wl * l;

    FetDevice fet;
    memset(&fet, 0, sizeof(fet));
    fet.type = FET_MOSFET_N_ENHANCEMENT;
    fet.tech = *tech;
    fet.tech.cox = cox;
    fet.temperature = spec->temperature > 0.0 ? spec->temperature : 300.0;
    fet.geo.w = w;
    fet.geo.l = l;
    fet.geo.w_over_l = wl;

    BiasResult bias_result;
    BiasSpec bias_spec;
    memset(&bias_spec, 0, sizeof(bias_spec));
    bias_spec.target_id  = id_per;
    bias_spec.target_vds = vdd / 2.0 - vss;
    bias_spec.target_vov = vov;
    bias_spec.vdd        = vdd;
    bias_spec.rin_min    = 1e9;  /* GΩ input impedance for instrumentation */

    fet_bias_design(&fet, &bias_spec, &bias_result);

    result.fet    = fet;
    result.bias   = bias_result.network;
    result.qpoint = bias_result.qpoint;
    result.power_actual = (vdd - vss) * itail;

    HybridPiModel hp = fet_extract_hybrid_pi(&fet);
    result.hp_model = hp;

    double gm_val = bias_result.qpoint.gm;
    double ro_val = (bias_result.qpoint.gds > 0.0)
                    ? 1.0 / bias_result.qpoint.gds : INFINITY;
    double rd_val = bias_result.network.rd;

    /* In-Amp gain: diff pair gain * second stage (assume ×10) */
    double adm = fet_cs_gain_ideal(gm_val, ro_val, rd_val);
    double gain_total = fabs(adm) * 10.0;  /* Two stages */

    /* CMRR with source degeneration for improved matching */
    double rss = ro_val;
    double cmrr_linear = fet_diffpair_cmrr(gm_val, ro_val, rd_val, rss);

    if (inamp) {
        inamp->gain    = gain_total;
        inamp->cmrr_db = 20.0 * log10(cmrr_linear);
        inamp->rin     = 1e12;  /* FET gate input */
        inamp->bw_hz   = 100e3;  /* Typical instrumentation BW */

        /* Input-referred noise (FET: dominant thermal + flicker) */
        double sv_thermal = 4.0 * FET_K_BOLTZMANN * 300.0 * 0.667 / gm_val;
        /* Integrated over 0.1Hz-100kHz */
        double vn_rms = sqrt(sv_thermal * 100e3 +
                             sv_thermal * 0.667 * log(100e3 / 0.1));
        inamp->vn_rti = vn_rms;
        inamp->offset_drift = 2.0;  /* μV/K typical for FET input */
    }

    FetAmpMetrics metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.config = FET_AMP_DIFFERENTIAL_PAIR;
    metrics.qpoint = bias_result.qpoint;
    metrics.av = gain_total;
    metrics.av_db = 20.0 * log10(gain_total);
    metrics.gm_eff = gm_val;
    metrics.rin = 1e12;
    metrics.rout = rd_val;
    metrics.f_high = 100e3;
    metrics.bw = 100e3;

    result.metrics = metrics;
    return result;
}
