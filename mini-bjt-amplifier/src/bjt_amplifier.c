/**
 * bjt_amplifier.c ? Complete BJT Amplifier Design Implementation
 *
 * Integrates DC bias, small-signal analysis, and frequency response
 * into a complete amplifier design framework. Implements cascode,
 * differential pair, Darlington pair, multistage cascade, and
 * distortion analysis.
 *
 * Reference: Sedra & Smith, Ch. 8-9, 13
 *            Gray & Meyer, Ch. 6 (cascode), Ch. 9 (feedback)
 */

#include "bjt_amplifier.h"
#include <math.h>
#include <stdio.h>

/* ================================================================
 * Cascode Amplifier (CE + CB)
 * ================================================================ */

void bjt_cascode_analyze(const bjt_spice_params_t *dev,
                          double IC_target, double VCC,
                          double RC, double RL, double Rs,
                          double R1, double R2, double RE,
                          double temperature_kelvin,
                          bjt_cascode_t *result)
{
    /* Cascode amplifier: CE stage (Q1) driving a CB stage (Q2).
     *
     * Q1 (CE): the input transistor. Its collector connects directly
     *          to Q2's emitter ? no Miller multiplication because Q2's
     *          emitter input impedance is very low (re ? 1/gm).
     *
     * Q2 (CB): the cascode transistor. Its base is at AC ground
     *          (bypassed to ground), providing a low-impedance load
     *          (re2) to Q1's collector. The output is taken from Q2's
     *          collector.
     *
     * Equivalent small-signal model:
     *   Q1: CE with RL_Q1 = re2 ? 1/gm2 ? 26? (for IC=1mA)
     *   Q2: CB with input at emitter, output at collector.
     *
     * Overall voltage gain:
     *   Av_cascode ? -gm1 * RC
     *   (same as CE, but without Miller effect!)
     *
     * Bandwidth improvement: since Q1 sees a very low load (re2 ? 1/gm),
     * the Miller multiplication factor is dramatically reduced:
     *   M = 1 + |Av_Q1| = 1 + gm1 * re2 ? 1 + 1 = 2  (vs. ~101 for CE alone)
     *
     * Output impedance: very high
     *   Rout_cascode ? beta * ro2 (bootstrapping effect)
     *
     * Reference: Sedra & Smith, Sec. 8.6. */

    bjt_bias_network_t net;
    bjt_qpoint_t qp;
    bjt_ss_params_t ss;

    if (!dev || !result) return;

    (void)IC_target;

    /* Bias both transistors at the same IC.
     * For the cascode, Q1 and Q2 share the same DC current. */

    /* Q1: common-emitter transistor */
    net.R1  = R1;
    net.R2  = R2;
    net.RC  = 0.0;  /* Q1's collector goes to Q2's emitter, no RC */
    net.RE  = RE;
    net.RB  = 0.0;
    net.Rs  = Rs;
    net.VCC = VCC;
    net.VEE = 0.0;
    net.type = (R1 > 0.0 && R2 > 0.0) ? BIAS_VOLTAGE_DIVIDER : BIAS_EMITTER_STABILIZED;

    bjt_bias_solve(dev, &net, temperature_kelvin, &qp);
    bjt_compute_ss_params(&qp, dev, temperature_kelvin, &ss);

    result->qp1 = qp;
    result->ss1 = ss;

    /* Q2: common-base transistor (biased through Q1's current) */
    /* In a cascode, Q1's collector current IS Q2's emitter current.
     * The base of Q2 is held at a fixed voltage (typically ~VCC/2). */
    result->qp2.IC = qp.IC;
    result->qp2.IB = qp.IC / dev->BF;
    result->qp2.IE = qp.IC + result->qp2.IB;
    result->qp2.VCE = VCC - qp.IC * RC - qp.IC * RE; /* Approximate */
    result->qp2.VBE = bjt_vbe_from_ic(qp.IC, dev->IS,
                                       bjt_vt(temperature_kelvin), dev->NF);
    result->qp2.VBC = result->qp2.VBE - result->qp2.VCE;
    result->qp2.power = result->qp2.VCE * qp.IC;
    result->qp2.region = BJT_REGION_FORWARD_ACTIVE;

    bjt_compute_ss_params(&result->qp2, dev, temperature_kelvin, &result->ss2);

    /* Overall cascode metrics */
    /* Av_cascode ? -gm1 * (RC || RL)  [since Q2's gain ? 1] */
    {
        double rL = RC;
        if (RL > 0.0) rL = (rL * RL) / (rL + RL);
        if (rL <= 0.0) rL = 1e6;

        result->metrics.Av    = -ss.gm * rL;
        result->metrics.Av_dB = bjt_gain_to_db(result->metrics.Av);
        result->metrics.Rin   = ss.rpi;  /* Same as CE input */
        result->metrics.Rout  = ss.ro * (1.0 + ss.gm * ss.re);  /* Cascode boost */
        if (RC > 0.0) result->metrics.Rout = (result->metrics.Rout * RC) /
                                              (result->metrics.Rout + RC);
        result->metrics.Ai    = ss.beta;  /* Approximate */
        result->metrics.Ai_dB = 20.0 * log10(ss.beta);
        result->metrics.Ap    = fabs(result->metrics.Av * result->metrics.Ai);
        result->metrics.Ap_dB = (result->metrics.Ap > 0.0)
                                ? 10.0 * log10(result->metrics.Ap) : -600.0;

        /* Bandwidth: much better than CE alone because Miller effect reduced */
        result->metrics.f_H  = ss.fT;  /* Approximate: cascode reaches near fT */
        result->metrics.BW   = result->metrics.f_H;
        result->metrics.GBW  = fabs(result->metrics.Av) * result->metrics.f_H;
        result->metrics.f_L  = 0.0;
        result->metrics.swing_max = VCC * 0.8;
        result->metrics.efficiency = 0.25;
        result->metrics.THD  = 0.0;
    }
}

int bjt_cascode_design(double Av_target, double BW_target,
                         double VCC, double RL, double Rs,
                         const bjt_spice_params_t *dev,
                         double temperature_kelvin,
                         bjt_cascode_t *result,
                         bjt_bias_network_t *net)
{
    /* Design a cascode amplifier for specified gain and bandwidth.
     *
     * Design approach:
     * 1. Choose IC to achieve target BW (roughly: IC sets fT, fT ~ IC)
     * 2. Choose RC to achieve target gain (Av ? gm*RC = IC*RC/VT)
     * 3. Design bias network for the chosen IC. */

    double vt, gm_target, RC_needed, IC_needed;

    if (!dev || !result || !net) return -1;
    if (Av_target <= 0.0 || BW_target <= 0.0) return -1;

    vt = bjt_vt(temperature_kelvin);

    /* fT is roughly proportional to IC (through gm).
     * For typical small-signal BJT: fT ? 300 MHz at IC = 1 mA.
     * Scale: fT(IC) ? 300e6 * sqrt(IC/1e-3) (approximate).
     * We need fT >> BW_target for a gain of |Av_target|.
     *
     * GBW requirement: |Av| * BW ? fT for cascode at best case.
     * Target: fT ? 5 * |Av_target| * BW_target (safety margin). */

    double fT_needed = 5.0 * Av_target * BW_target;

    /* Estimate IC needed: fT ? fT_1mA * sqrt(IC/1mA).
     *   IC_needed = 1e-3 * (fT_needed / fT_1mA)^2 */
    IC_needed = 1e-3 * (fT_needed / 300e6) * (fT_needed / 300e6);
    if (IC_needed < 1e-6) IC_needed = 1e-5;
    if (IC_needed > 1e-2) IC_needed = 5e-3;  /* Cap at 5 mA */

    /* RC for gain: |Av| = gm * RC = (IC/VT) * RC.
     *   RC = |Av| * VT / IC */
    gm_target = IC_needed / vt;
    RC_needed = Av_target / gm_target;

    if (RC_needed > VCC / IC_needed) RC_needed = VCC / IC_needed;

    /* Design voltage-divider bias for this IC */
    net->RC = RC_needed;
    net->VCC = VCC;
    net->VEE = 0.0;
    net->Rs  = Rs;
    net->RE  = (VCC * 0.1) / IC_needed;  /* 10% of VCC across RE */

    bjt_bias_design_voltage_divider(IC_needed, VCC * 0.5, VCC,
                                      dev->BF * 0.5, dev,
                                      temperature_kelvin, net);

    /* Now analyze the cascode */
    bjt_cascode_analyze(dev, IC_needed, VCC, RC_needed, RL, Rs,
                         net->R1, net->R2, net->RE,
                         temperature_kelvin, result);

    return 0;
}

/* ================================================================
 * Differential Pair
 * ================================================================ */

void bjt_diff_pair_analyze(const bjt_spice_params_t *dev,
                            double I_EE, double RC,
                            double R_EE, double VCC,
                            double VEE, double temperature_kelvin,
                            bjt_diff_pair_t *result)
{
    /* Differential pair (emitter-coupled pair) analysis.
     *
     * Two matched BJTs with emitters connected to a common current
     * source I_EE (or resistor R_EE to -VEE). The differential input
     * is applied between the two bases, and the output can be taken
     * differentially (between collectors) or single-ended.
     *
     * Differential-mode half-circuit (balanced):
     *   Each transistor carries IC = I_EE/2 at quiescent.
     *   Differential gain (single-ended output):
     *     Av_dm = -gm * RC / 2  [half the differential signal at each collector]
     *   Differential gain (balanced output):
     *     Av_dm_balanced = -gm * RC  [full differential signal]
     *
     * Common-mode half-circuit:
     *   For common-mode input, both bases move together.
     *   The emitter sees 2*R_EE (since emitter current from both transistors
     *   flows through R_EE, or the tail current source impedance).
     *   Common-mode gain:
     *     Av_cm ? -RC / (2 * R_EE)  [for large gm]
     *
     * CMRR = |Av_dm / Av_cm| ? gm * 2 * R_EE = (IC/VT) * 2 * R_EE
     *
     * For current-source biasing, R_EE ? ? ? CMRR ? ? (ideally).
     * In practice, transistor mismatch limits CMRR to ~60-80 dB.
     *
     * Reference: Sedra & Smith, Sec. 9.3; Gray & Meyer, Ch. 5. */

    double IC_half, vt, gm, beta;

    if (!dev || !result) return;

    vt = bjt_vt(temperature_kelvin);

    IC_half = I_EE / 2.0;
    if (IC_half <= 0.0) IC_half = 1e-6;

    gm = bjt_gm(IC_half, vt);
    beta = dev->BF;

    result->I_EE = I_EE;
    result->R_EE = R_EE;
    result->RC1 = RC;
    result->RC2 = RC;

    /* Q-points (identical for matched pair) */
    result->qp1.IC = IC_half;
    result->qp1.IB = IC_half / beta;
    result->qp1.IE = IC_half + result->qp1.IB;
    result->qp1.VBE = bjt_vbe_from_ic(IC_half, dev->IS, vt, dev->NF);
    result->qp1.VCE = VCC - IC_half * RC - result->qp1.IE * R_EE - VEE;
    result->qp1.VBC = result->qp1.VBE - result->qp1.VCE;
    result->qp1.power = result->qp1.VCE * IC_half;
    result->qp1.region = BJT_REGION_FORWARD_ACTIVE;

    result->qp2 = result->qp1;  /* Matched pair */

    /* Small-signal parameters */
    bjt_compute_ss_params(&result->qp1, dev, temperature_kelvin, &result->ss1);
    result->ss2 = result->ss1;

    /* Differential-mode gain (balanced output) */
    result->Av_dm = -gm * RC;

    /* Common-mode gain */
    if (R_EE > 0.0) {
        /* With finite tail resistance */
        result->Av_cm = -RC / (2.0 * R_EE + 1.0/gm);
    } else {
        /* With infinite tail impedance (ideal current source) */
        result->Av_cm = 0.0;  /* Ideally zero */
    }

    /* CMRR */
    if (fabs(result->Av_cm) > 1e-15) {
        result->CMRR = fabs(result->Av_dm / result->Av_cm);
    } else {
        result->CMRR = 1e6;  /* Near-infinite */
    }
    result->CMRR_dB = 20.0 * log10(result->CMRR);

    /* Differential input resistance: looking between the two bases.
     * For a differential signal: each base sees rpi to the common emitter.
     * Total: Rid = 2 * rpi */
    result->Rid = 2.0 * result->ss1.rpi;

    /* Common-mode input resistance: looking into each base to ground.
     * Ric = rpi + (beta+1) * 2 * R_EE (the tail R sees 2*Ie) */
    result->Ric = result->ss1.rpi + (beta + 1.0) * 2.0 * R_EE;

    /* Differential output resistance: between collectors.
     * Rod = 2 * RC */
    result->Rod = 2.0 * RC;
}

double bjt_diff_pair_cmrr_mismatch(double delta_RC_ratio)
{
    /* CMRR limitation due to collector resistor mismatch.
     *
     * If the two RC resistors differ by delta_RC:
     *   CMRR_mismatch ? 1 / (delta_RC / RC)
     *
     * Example: 1% resistor tolerance ? CMRR ? 100 = 40 dB.
     * This is often the dominant CMRR limitation in practical
     * differential amplifiers. */

    if (delta_RC_ratio <= 0.0) return 1e6;
    return 1.0 / delta_RC_ratio;
}

/* ================================================================
 * Darlington Pair
 * ================================================================ */

void bjt_darlington_analyze(const bjt_ss_params_t *ss1,
                             const bjt_ss_params_t *ss2,
                             bjt_darlington_t *result)
{
    /* Darlington pair: two BJTs connected such that the emitter
     * current of Q1 drives the base of Q2. The composite transistor
     * has:
     *
     *   beta_eff = beta1 * beta2 + beta1 + beta2 ? beta1 * beta2
     *   VBE_eff = VBE1 + VBE2 ? 1.4 V (for silicon at 1 mA)
     *
     * Small-signal equivalent (both at same IC for simplicity):
     *   gm_eff ? gm1 * (beta2/(beta2+1)) ? gm/2 (if IC1 = IC2)
     *
     * More accurately, if Q1's IC (which is Q2's IB) = IC2/beta2:
     *   gm_eff ? gm2 / 2
     *   rpi_eff ? rpi1 + beta1 * rpi2 ? 2 * beta^2 / gm
     *
     * The Darlington provides enormous current gain and very high
     * input impedance, but also high VBE and poor high-frequency
     * response (fT_eff << fT of individual transistors).
     *
     * Reference: Sedra & Smith, Sec. 12.6. */

    if (!ss1 || !ss2 || !result) return;

    /* Effective current gain */
    result->beta_eff = ss1->beta * ss2->beta + ss1->beta + ss2->beta;

    /* Effective VBE (sum of two V_BE drops) */
    result->VBE_eff = 1.4;  /* Typical at moderate current */

    /* Effective small-signal parameters */
    result->ss_eq.gm    = ss2->gm;              /* Dominated by Q2 */
    result->ss_eq.rpi   = ss1->rpi + ss1->beta * ss2->rpi;
    result->ss_eq.ro    = ss2->ro / 2.0;        /* Approximate */
    result->ss_eq.Cpi   = ss1->Cpi + ss2->Cpi;
    result->ss_eq.Cmu   = ss1->Cmu + ss2->Cmu;
    result->ss_eq.re    = ss2->re;
    result->ss_eq.alpha = bjt_alpha_from_beta(result->beta_eff);
    result->ss_eq.beta  = result->beta_eff;

    /* fT degradation: Darlington fT ? fT1 * fT2 / (fT1 + fT2) */
    if (ss1->fT > 0.0 && ss2->fT > 0.0) {
        result->ss_eq.fT = (ss1->fT * ss2->fT) / (ss1->fT + ss2->fT);
    } else {
        result->ss_eq.fT = (ss1->fT > 0.0) ? ss1->fT / 2.0 : ss2->fT / 3.0;
    }
    result->ss_eq.f_beta = result->ss_eq.fT / result->beta_eff;
}

/* ================================================================
 * Complete Amplifier Design
 * ================================================================ */

int bjt_amplifier_design(bjt_amp_topology_t topology,
                          double Av_target, double Rin_target,
                          double Rout_target, double VCC,
                          double RL, double Rs,
                          double fL_target, double fH_target,
                          const bjt_spice_params_t *dev,
                          double temperature_kelvin,
                          bjt_amplifier_design_t *design)
{
    /* Complete BJT amplifier design from specifications.
     *
     * This is the top-level design function that automates the
     * design process taught in Sedra & Smith, Ch. 7-10.
     *
     * Design flow:
     * 1. Select bias configuration based on topology
     * 2. Choose IC to meet gain and impedance specs
     * 3. Design bias resistors
     * 4. Compute small-signal performance
     * 5. Size coupling/bypass capacitors for low-freq response
     * 6. Verify bandwidth meets spec
     */

    double vt, gm_needed, IC_needed;
    bjt_bias_network_t *net;
    bjt_qpoint_t *qp;
    bjt_ss_params_t *ss;
    bjt_amp_metrics_t *met;

    (void)Rin_target; (void)Rout_target; (void)fH_target;
    if (!dev || !design) return -1;
    if (VCC <= 0.0) return -1;

    vt = bjt_vt(temperature_kelvin);
    net = &design->bias;
    qp  = &design->qpoint;
    ss  = &design->ss;
    met = &design->metrics;
    design->topology = topology;

    /* Initialize bias network */
    net->VCC = VCC;
    net->VEE = 0.0;
    net->Rs  = Rs;
    net->RB  = 0.0;
    net->type = BIAS_VOLTAGE_DIVIDER;

    /* Step 1: Determine IC needed for gain specification.
     * For CE: |Av| ? gm * RL' ? (IC/VT) * RL'
     *   IC ? |Av_target| * VT / RL' */
    {
        double rL = (RL > 0.0) ? RL : 10000.0;  /* Assume 10k if no RL */
        gm_needed = fabs(Av_target) / rL;
        IC_needed = gm_needed * vt;
        if (IC_needed < 1e-6) IC_needed = 1e-5;  /* Minimum 10 uA */
        if (IC_needed > 2e-2) IC_needed = 1e-2;  /* Maximum 10 mA */
    }

    /* Step 2: Design bias network.
     * For voltage-divider: use standard design procedure. */
    {
        double VCE_target = VCC / 2.0;  /* Center for max swing */
        bjt_bias_design_voltage_divider(IC_needed, VCE_target, VCC,
                                          dev->BF * 0.5, dev,
                                          temperature_kelvin, net);
    }

    /* Step 3: Compute Q-point */
    bjt_bias_solve(dev, net, temperature_kelvin, qp);

    /* Step 4: Small-signal parameters */
    bjt_compute_ss_params(qp, dev, temperature_kelvin, ss);

    /* Step 5: Analyze based on topology */
    switch (topology) {
        case AMP_COMMON_EMITTER:
            bjt_ce_analyze(ss, net->RC, RL, net->RE, Rs, 0, met);
            break;
        case AMP_COMMON_COLLECTOR:
            bjt_cc_analyze(ss, net->RE, RL, Rs, met);
            break;
        case AMP_COMMON_BASE:
            bjt_cb_analyze(ss, net->RC, RL, net->RE, Rs, met);
            break;
        default:
            bjt_ce_analyze(ss, net->RC, RL, net->RE, Rs, 0, met);
            break;
    }

    /* Step 6: Size coupling capacitors.
     * fL_target determines the minimum capacitor values. */
    {
        double Cin, Cout, CE;
        double fL_cin, fL_cout, fL_ce;
    (void)fL_cin; (void)fL_cout; (void)fL_ce;

        /* Target each low-frequency pole at fL_target/3
         * so their combined effect gives fL ? fL_target. */
        double fL_each = fL_target / 3.0;
        if (fL_each < 1.0) fL_each = 1.0;  /* Minimum 1 Hz */

        /* Cin: input coupling with Rs + Rin */
        Cin = 1.0 / (2.0 * M_PI * fL_each * (Rs + met->Rin));
        if (Cin > 1e-3) Cin = 1e-5;  /* Cap at 10 uF */

        /* Cout: output coupling with Rout + RL */
        Cout = 1.0 / (2.0 * M_PI * fL_each * (met->Rout + RL));
        if (Cout > 1e-3) Cout = 1e-5;

        /* CE: emitter bypass with (RE || (re + Rs/(beta+1))) */
        {
            double r_ce = ss->re + Rs / (ss->beta + 1.0);
            if (net->RE > 0.0) r_ce = (net->RE * r_ce) / (net->RE + r_ce);
            CE = 1.0 / (2.0 * M_PI * fL_each * r_ce);
            if (CE > 1e-2) CE = 1e-4;  /* Cap at 100 uF */
        }

        design->coupling_caps[0] = Cin;
        design->coupling_caps[1] = Cout;
        design->coupling_caps[2] = CE;
        design->num_coupling_caps = 3;
    }

    /* Step 7: Build transfer function */
    {
        double fL_in  = bjt_freq_fL_input_coupling(design->coupling_caps[0],
                                                     Rs, met->Rin);
        double fL_out = bjt_freq_fL_output_coupling(design->coupling_caps[1],
                                                      met->Rout, RL);
        double fL_byp = bjt_freq_fL_emitter_bypass(design->coupling_caps[2],
                                                     net->RE, ss->re,
                                                     Rs, ss->beta);
        double fH_mil = bjt_freq_fH_miller_input(ss, Rs, fabs(met->Av));
        double fH_out = bjt_freq_fH_miller_output(ss, net->RC, RL);

        bjt_tf_ce_build(met->Av, fL_in, fL_out, fL_byp, fH_mil, fH_out,
                        &design->tf);

        /* Update bandwidth metrics */
        met->f_L = bjt_tf_find_fL(&design->tf, 0.1, 1e6, 0.1);
        met->f_H = bjt_tf_find_fH(&design->tf, 1e3, 1e9, 1e3);
        met->BW  = met->f_H - met->f_L;
        met->GBW = fabs(met->Av) * met->f_H;
    }

    return 0;
}

double bjt_amplifier_evaluate(const bjt_amplifier_design_t *design,
                               double frequency_hz)
{
    if (!design) return 0.0;
    return bjt_tf_magnitude(&design->tf, frequency_hz);
}

/* ================================================================
 * Multistage Analysis
 * ================================================================ */

int bjt_multistage_add_stage(bjt_multistage_t *amp,
                              const bjt_amplifier_design_t *stage)
{
    if (!amp || !stage) return -1;
    if (amp->n_stages >= BJT_MAX_STAGES) return -1;

    amp->stages[amp->n_stages] = *stage;
    amp->n_stages++;

    return amp->n_stages - 1;
}

void bjt_multistage_analyze(bjt_multistage_t *amp)
{
    /* Analyze a cascade of amplifier stages.
     *
     * Overall:
     *   Av_total = Av_1 * Av_2 * ... * Av_n
     *   Rin_total = Rin_1 (first stage)
     *   Rout_total = Rout_n (last stage)
     *
     * Bandwidth (approximate, assuming dominant-pole per stage):
     *   1/f_H_total^2 ? sum_i (1/f_H_i^2)
     *
     * This is the bandwidth shrinkage effect: cascading n identical
     * stages reduces bandwidth by approximately 1/sqrt(n).
     *
     * Reference: Sedra & Smith, Sec. 9.1. */

    int i;
    double av_total, sum_inv_fH2, sum_inv_fL2;

    if (!amp || amp->n_stages <= 0) return;

    av_total = 1.0;
    sum_inv_fH2 = 0.0;
    sum_inv_fL2 = 0.0;

    for (i = 0; i < amp->n_stages; i++) {
        av_total *= amp->stages[i].metrics.Av;

        if (amp->stages[i].metrics.f_H > 0.0) {
            sum_inv_fH2 += 1.0 / (amp->stages[i].metrics.f_H *
                                  amp->stages[i].metrics.f_H);
        }
        if (amp->stages[i].metrics.f_L > 0.0) {
            sum_inv_fL2 += amp->stages[i].metrics.f_L *
                           amp->stages[i].metrics.f_L;
        }
    }

    amp->overall_metrics.Av    = av_total;
    amp->overall_metrics.Av_dB = bjt_gain_to_db(av_total);
    amp->overall_metrics.Rin   = amp->stages[0].metrics.Rin;
    amp->overall_metrics.Rout  = amp->stages[amp->n_stages - 1].metrics.Rout;

    /* Overall bandwidth */
    if (sum_inv_fH2 > 0.0) {
        amp->overall_metrics.f_H = 1.0 / sqrt(sum_inv_fH2);
    } else {
        amp->overall_metrics.f_H = 1e9;
    }

    amp->overall_metrics.f_L = sqrt(sum_inv_fL2);
    amp->overall_metrics.BW  = amp->overall_metrics.f_H -
                                amp->overall_metrics.f_L;
    amp->overall_metrics.GBW = fabs(av_total) * amp->overall_metrics.f_H;

    /* Current gain: product of individual current gains, adjusted for
     * inter-stage loading. Approximate: */
    amp->overall_metrics.Ai = 1.0;
    for (i = 0; i < amp->n_stages; i++) {
        amp->overall_metrics.Ai *= amp->stages[i].metrics.Ai;
    }
    amp->overall_metrics.Ai_dB = (amp->overall_metrics.Ai > 0.0)
                                  ? 20.0 * log10(amp->overall_metrics.Ai)
                                  : -600.0;

    amp->overall_metrics.Ap = fabs(av_total * amp->overall_metrics.Ai);
    amp->overall_metrics.Ap_dB = (amp->overall_metrics.Ap > 0.0)
                                  ? 10.0 * log10(amp->overall_metrics.Ap)
                                  : -600.0;
}

/* ================================================================
 * Distortion Analysis
 * ================================================================ */

double bjt_harmonic_distortion_hd2(double vin_peak, double vt)
{
    /* Second harmonic distortion for a BJT CE amplifier.
     *
     * The BJT's exponential IC-VBE characteristic creates nonlinear
     * distortion. For a small sinusoidal input vin*sin(wt):
     *
     *   IC = IS * exp((VBE + vin*sin(wt))/VT)
     *      = IC_Q * exp(vin*sin(wt)/VT)
     *
     * Taylor expansion: exp(x) ? 1 + x + x^2/2 + x^3/6 + ...
     *
     * The second harmonic amplitude relative to fundamental:
     *   HD2 ? vin_peak / (4 * VT)
     *
     * Example: vin_peak = 1 mV, VT = 26 mV ? HD2 ? 0.0096 = 0.96%
     *
     * Reference: Sedra & Smith, Sec. 8.2. */

    if (vt <= 0.0) return 0.0;
    return vin_peak / (4.0 * vt);
}

double bjt_harmonic_distortion_hd3(double vin_peak, double vt)
{
    /* Third harmonic distortion:
     *
     *   HD3 ? (vin_peak / VT)^2 / 24
     *
     * HD3 is typically much smaller than HD2 for small signals.
     *
     * Example: vin_peak = 1 mV, VT = 26 mV
     *   ? (1/26)^2 / 24 = 0.00148/24 = 0.000062 = 0.0062% */

    if (vt <= 0.0) return 0.0;
    double x = vin_peak / vt;
    return (x * x) / 24.0;
}

double bjt_thd_from_hd(double hd2, double hd3)
{
    /* Total Harmonic Distortion:
     *
     *   THD = sqrt(HD2^2 + HD3^2 + HD4^2 + ...)
     *
     * For small signals, HD2 dominates. We compute from the first
     * two non-trivial harmonics. */

    return sqrt(hd2 * hd2 + hd3 * hd3);
}

double bjt_max_input_for_thd(double thd_target, double vt)
{
    /* Maximum input amplitude for a given THD specification.
     *
     * Using HD2 as the dominant term:
     *   HD2 ? vin_peak / (4*VT) ? THD_target
     *   vin_peak_max = 4 * VT * THD_target
     *
     * Example: THD < 0.1% (0.001), VT = 26 mV
     *   ? vin_max = 4 * 0.026 * 0.001 = 104 uV
     *
     * This is why BJT amplifiers need either very small signals
     * or negative feedback for linearity. */

    if (vt <= 0.0) return 0.0;
    return 4.0 * vt * thd_target;
}
