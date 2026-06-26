/**
 * bjt_dc_bias.c ? BJT DC Biasing Implementation
 *
 * Implements all six standard BJT bias configurations, load-line analysis,
 * stability metrics, and temperature compensation. This is the foundation
 * for all amplifier designs ? without a stable Q-point, no linear amplifier
 * can function reliably.
 *
 * Reference: Sedra & Smith, "Microelectronic Circuits", 8th ed., Ch. 7
 *            Millman & Halkias, "Integrated Electronics", Ch. 10
 */

#include "bjt_dc_bias.h"
#include <math.h>
#include <stdio.h>

/* ================================================================
 * Thevenin Equivalent for Voltage Divider
 * ================================================================ */

void bjt_bias_thevenin(double R1, double R2, double VCC,
                       double *Vth, double *Rth)
{
    /* The Thevenin equivalent transforms a voltage divider (R1, R2, VCC)
     * into an equivalent voltage source Vth in series with Rth:
     *
     *   Vth = VCC * R2 / (R1 + R2)
     *   Rth = R1 * R2 / (R1 + R2)  = R1 || R2
     *
     * This is the single most important transformation in BJT bias
     * analysis, as it reduces a 4-component network to 2 components. */
    double sum;

    if (!Vth || !Rth) return;

    if (R1 <= 0.0 || R2 <= 0.0) {
        *Vth = VCC;
        *Rth = (R1 > 0.0) ? R1 : ((R2 > 0.0) ? R2 : 1e6);
        return;
    }

    sum = R1 + R2;
    *Vth = VCC * R2 / sum;
    *Rth = R1 * R2 / sum;
}

/* ================================================================
 * Fixed Base Bias
 * ================================================================ */

int bjt_bias_fixed(const bjt_spice_params_t *dev,
                    const bjt_bias_network_t *net,
                    double temperature_kelvin,
                    bjt_qpoint_t *qp)
{
    /* Fixed base bias: single RB from VCC to base.
     *
     * Circuit: VCC -- RB -- B (base)
     *          VCC -- RC -- C (collector)
     *          E -- GND
     *
     * Analysis (assuming forward-active):
     *   VCC = IB * RB + VBE
     *   IB = (VCC - VBE) / RB
     *   IC = beta * IB
     *   VCE = VCC - IC * RC
     *
     * Problem: extremely beta-dependent. If beta varies 2:1 (typical
     * for 2N3904: 100-300), IC varies 2:1. Not suitable for production.
     */

    double vt, vbe, ib, ic, vce;

    if (!dev || !net || !qp) return -1;
    if (net->RB <= 0.0) return -1;
    vt = bjt_vt(temperature_kelvin);

    /* Estimate VBE. For first-pass, assume VBE ? 0.7V.
     * More accurate: iterate to solve VBE = f(IC/IS). */
    vbe = 0.7;

    /* First iteration: compute IB using assumed VBE */
    ib = (net->VCC - vbe) / net->RB;
    if (ib <= 0.0) {
        /* Cutoff: RB too large or VCC too small */
        qp->IC = qp->IB = qp->IE = 0.0;
        qp->VCE = net->VCC;
        qp->VBE = 0.0;
        qp->VBC = -net->VCC;
        qp->power = 0.0;
        qp->region = BJT_REGION_CUTOFF;
        return 0;
    }

    ic = dev->BF * ib;

    /* Second iteration: compute VBE from IC */
    vbe = bjt_vbe_from_ic(ic, dev->IS, vt, dev->NF);
    if (vbe >= net->VCC) vbe = net->VCC - 0.1;

    /* Recompute IB with updated VBE */
    ib = (net->VCC - vbe) / net->RB;
    if (ib <= 0.0) ib = 1e-12;
    ic = dev->BF * ib;

    vce = net->VCC - ic * (net->RC);
    if (vce < 0.2) {
        /* Saturation: VCE too low */
        qp->region = BJT_REGION_SATURATION;
    } else {
        qp->region = BJT_REGION_FORWARD_ACTIVE;
    }

    qp->IC    = ic;
    qp->IB    = ib;
    qp->IE    = ic + ib;
    qp->VCE   = vce;
    qp->VBE   = vbe;
    qp->VBC   = vbe - vce;
    qp->power = net->VCC * (ic + ib);
    return 0;
}

/* ================================================================
 * Emitter-Stabilized Bias
 * ================================================================ */

int bjt_bias_emitter_stabilized(const bjt_spice_params_t *dev,
                                 const bjt_bias_network_t *net,
                                 double temperature_kelvin,
                                 bjt_qpoint_t *qp)
{
    /* Emitter-stabilized: RB to VCC, RE to GND.
     *
     * KVL base-emitter loop:
     *   VCC = IB*RB + VBE + IE*RE
     *   IE = (beta+1)*IB
     *   IB = (VCC - VBE) / (RB + (beta+1)*RE)
     *
     * The (beta+1)*RE term provides negative feedback:
     * If IC increases (e.g., temperature), IE increases,
     * V_RE increases, reducing VBE, reducing IC. Self-stabilizing.
     *
     * Stability requirement: (beta+1)*RE >> RB.
     */

    double vt, vbe, ib, ic, ie, vce, vrc;
    double beta_eff;
    int iter;

    if (!dev || !net || !qp) return -1;
    if (net->RB <= 0.0) return -1;
    vt = bjt_vt(temperature_kelvin);
    /* RE may be zero, handled in KVL denominator */

    beta_eff = dev->BF;

    /* Initial guess */
    vbe = 0.7;
    ib = 1e-6;
    ic = beta_eff * ib;

    /* Iterative solution: VBE depends on IC, IC depends on VBE.
     * Use fixed-point iteration (2-3 iterations sufficient). */
    for (iter = 0; iter < 5; iter++) {
        vbe = bjt_vbe_from_ic(ic, dev->IS, vt, dev->NF);
        if (vbe >= net->VCC) vbe = net->VCC * 0.8;
        if (vbe < 0.3) vbe = 0.3;

        ib = (net->VCC - vbe) / (net->RB + (beta_eff + 1.0) * net->RE);
        if (ib <= 0.0) {
            ib = 1e-12;
            break;
        }

        ic = beta_eff * ib;
        vce = net->VCC - ic * (net->RC) - (ic + ib) * net->RE;

        /* Check saturation */
        if (vce < 0.2) {
            ic = (net->VCC - 0.2) / (net->RC + net->RE + net->RE / beta_eff);
            ib = ic / beta_eff;
            break;
        }
    }

    ie = ic + ib;
    vrc = ic * net->RC;
    vce = net->VCC - vrc - ie * net->RE;

    qp->IC    = ic;
    qp->IB    = ib;
    qp->IE    = ie;
    qp->VCE   = vce;
    qp->VBE   = vbe;
    qp->VBC   = vbe - vce;
    qp->power = net->VCC * (ic + ib);
    qp->region = bjt_determine_region(vbe, vbe - vce, vt, BJT_NPN);

    return 0;
}

/* ================================================================
 * Voltage-Divider Bias (Most Common Discrete Design)
 * ================================================================ */

int bjt_bias_voltage_divider(const bjt_spice_params_t *dev,
                              const bjt_bias_network_t *net,
                              double temperature_kelvin,
                              bjt_qpoint_t *qp)
{
    /* Voltage-divider bias: R1 to VCC, R2 to GND, base at divider node.
     *
     * Step 1: Thevenin equivalent of R1/R2 divider.
     *   Vth = VCC * R2 / (R1 + R2)
     *   Rth = R1 || R2
     *
     * Step 2: KVL of base-emitter loop with Thevenin.
     *   Vth = IB * Rth + VBE + IE * RE
     *   Vth = IB * Rth + VBE + (beta+1) * IB * RE
     *   IB = (Vth - VBE) / (Rth + (beta+1)*RE)
     *
     * Stability rule: Choose Rth <= 0.1 * (beta_min + 1) * RE.
     * This ensures <10% IC variation for 3:1 beta range.
     *
     * Step 3: IC = beta * IB, VCE = VCC - IC*RC - IE*RE.
     */

    double vt, vth, rth, vbe, ib, ic, ie, vce;
    double beta_eff;
    int iter;

    if (!dev || !net || !qp) return -1;
    if (net->R1 <= 0.0 || net->R2 <= 0.0) {
        /* Fall back to fixed bias if no divider */
        return bjt_bias_fixed(dev, net, temperature_kelvin, qp);
    }

    vt = bjt_vt(temperature_kelvin);
    beta_eff = dev->BF;

    /* Thevenin equivalent */
    bjt_bias_thevenin(net->R1, net->R2, net->VCC, &vth, &rth);

    if (vth <= 0.5) {
        /* Vth below VBE threshold ? transistor off */
        qp->IC = qp->IB = qp->IE = 0.0;
        qp->VCE = net->VCC;
        qp->VBE = vth;
        qp->VBC = vth - net->VCC;
        qp->power = 0.0;
        qp->region = BJT_REGION_CUTOFF;
        return 0;
    }

    /* Iterative solution */
    vbe = 0.7;
    ic = 1e-3;

    for (iter = 0; iter < 5; iter++) {
        vbe = bjt_vbe_from_ic(ic, dev->IS, vt, dev->NF);
        if (vbe >= vth) vbe = vth * 0.9;
        if (vbe < 0.3) vbe = 0.3;

        ib = (vth - vbe) / (rth + (beta_eff + 1.0) * net->RE);
        if (ib <= 0.0) {
            ib = 1e-12;
            break;
        }

        ic = beta_eff * ib;
        ie = ic + ib;
        vce = net->VCC - ic * net->RC - ie * net->RE;

        if (vce < 0.2) {
            /* Saturated ? adjust */
            ic = (net->VCC - 0.2 - vbe * (net->RE / (rth + net->RE)))
                 / (net->RC + net->RE);
            if (ic < 0.0) ic = 0.0;
            ib = ic / beta_eff;
            break;
        }
    }

    ie = ic + ib;
    vce = net->VCC - ic * net->RC - ie * net->RE;

    qp->IC    = ic;
    qp->IB    = ib;
    qp->IE    = ie;
    qp->VCE   = vce;
    qp->VBE   = vbe;
    qp->VBC   = vbe - vce;
    qp->power = net->VCC * (ic + (ic / beta_eff));
    qp->region = bjt_determine_region(vbe, vbe - vce, vt, BJT_NPN);

    return 0;
}

/* ================================================================
 * Collector-Feedback Bias
 * ================================================================ */

int bjt_bias_collector_feedback(const bjt_spice_params_t *dev,
                                 const bjt_bias_network_t *net,
                                 double temperature_kelvin,
                                 bjt_qpoint_t *qp)
{
    /* Collector-feedback bias: RB from collector to base.
     *
     * Circuit:
     *   VCC -- RC -- C (collector)
     *   C -- RB -- B (base)
     *   E -- GND
     *
     * KVL through RB:
     *   VCC = (IC + IB)*RC + IB*RB + VBE
     *   IB = (VCC - VBE) / (RB + (beta+1)*RC)
     *
     * Negative feedback mechanism:
     * IC up ? VC down ? IB down ? IC down. Self-stabilizing.
     * Simpler than voltage-divider (fewer resistors) but lower input
     * impedance (RB is between input and output).
     */

    double vt, vbe, ib, ic, ie, vce, vc;
    double beta_eff;
    int iter;

    if (!dev || !net || !qp) return -1;
    if (net->RB <= 0.0) return -1;
    vt = bjt_vt(temperature_kelvin);
    beta_eff = dev->BF;

    vbe = 0.7;
    ic = 1e-3;

    for (iter = 0; iter < 5; iter++) {
        vbe = bjt_vbe_from_ic(ic, dev->IS, vt, dev->NF);
        if (vbe >= net->VCC) vbe = net->VCC * 0.7;

        /* IB = (VCC - VBE) / (RB + (beta+1)*RC) */
        ib = (net->VCC - vbe) / (net->RB + (beta_eff + 1.0) * net->RC);
        if (ib <= 0.0) {
            ib = 1e-12;
            break;
        }

        ic = beta_eff * ib;
        ie = ic + ib;
        vc = net->VCC - ie * net->RC;
        vce = vc;
    }

    ie = ic + ib;
    vc = net->VCC - ie * net->RC;
    vce = vc;

    qp->IC    = ic;
    qp->IB    = ib;
    qp->IE    = ie;
    qp->VCE   = vce;
    qp->VBE   = vbe;
    qp->VBC   = vbe - vce;
    qp->power = net->VCC * ie;
    qp->region = bjt_determine_region(vbe, vbe - vce, vt, BJT_NPN);

    return 0;
}

/* ================================================================
 * Current-Source Bias
 * ================================================================ */

int bjt_bias_current_source(const bjt_spice_params_t *dev,
                             double I_EE, double VCC, double RC,
                             double temperature_kelvin,
                             bjt_qpoint_t *qp)
{
    /* Current-source bias: IE forced to I_EE by external current source.
     * This is the standard IC biasing technique using current mirrors.
     *
     * IE = I_EE (given)
     * IC = alpha * IE = beta/(beta+1) * I_EE ? I_EE
     * IB = IE/(beta+1)
     * VCE = VCC - IC*RC
     * VBE = NF * VT * ln(IC/IS + 1)
     *
     * Advantages: IC independent of beta (to first order),
     *             excellent stability, no large resistors.
     */

    double vt, alpha, ic, ib, vbe, vce;

    if (!dev || !qp) return -1;
    if (I_EE <= 0.0) {
        qp->IC = qp->IB = qp->IE = 0.0;
        qp->VCE = VCC;
        qp->VBE = 0.0;
        qp->power = 0.0;
        qp->region = BJT_REGION_CUTOFF;
        return 0;
    }

    vt = bjt_vt(temperature_kelvin);
    alpha = bjt_alpha_from_beta(dev->BF);
    ic = alpha * I_EE;
    ib = I_EE - ic;

    if (ib <= 0.0) ib = ic / dev->BF;

    vbe = bjt_vbe_from_ic(ic, dev->IS, vt, dev->NF);
    vce = VCC - ic * RC;

    qp->IC    = ic;
    qp->IB    = ib;
    qp->IE    = I_EE;
    qp->VCE   = vce;
    qp->VBE   = vbe;
    qp->VBC   = vbe - vce;
    qp->power = VCC * I_EE;
    qp->region = (vce > 0.3) ? BJT_REGION_FORWARD_ACTIVE : BJT_REGION_SATURATION;

    return 0;
}

/* ================================================================
 * Dual-Supply Bias
 * ================================================================ */

int bjt_bias_dual_supply(const bjt_spice_params_t *dev,
                          const bjt_bias_network_t *net,
                          double temperature_kelvin,
                          bjt_qpoint_t *qp)
{
    /* Dual-supply bias: +VCC and -VEE, emitter to -VEE through RE.
     *
     * Base loop KVL (going from GND through RB, VBE, RE to -VEE):
     *   0 = -IB*RB - VBE - IE*RE + VEE   [where VEE is the magnitude of negative supply]
     *   IE = (VEE - VBE) / (RE + RB/(beta+1))
     *
     * Actually, with VEE being negative: VEE_positive = |VEE|.
     *   0 = -IB*RB - VBE - IE*RE - VEE    [VEE < 0, say -5V]
     *   IE = (-VEE - VBE) / (RE + RB/(beta+1))
     *   IE = (|VEE| - VBE) / (RE + RB/(beta+1)) = (VEE_mag - VBE) / (...)
     */

    double vt, vbe, ib, ic, ie, vce;
    double vee_mag, beta_eff;
    int iter;

    if (!dev || !net || !qp) return -1;

    /* net->VEE is negative for NPN dual-supply (e.g., -5V).
     * vee_mag is the magnitude = -VEE if VEE < 0, else VEE. */
    vee_mag = (net->VEE < 0.0) ? -net->VEE : net->VEE;
    vt = bjt_vt(temperature_kelvin);
    beta_eff = dev->BF;
    vbe = 0.7;
    ic = 1e-3;

    for (iter = 0; iter < 5; iter++) {
        vbe = bjt_vbe_from_ic(ic, dev->IS, vt, dev->NF);

        /* IE = (VEE_mag - VBE) / (RE + RB/(beta+1)) */
        ie = (vee_mag - vbe) / (net->RE + net->RB / (beta_eff + 1.0));
        if (ie <= 0.0) {
            ie = 1e-12;
            break;
        }

        ic = beta_eff / (beta_eff + 1.0) * ie;
        ib = ie - ic;
        vce = net->VCC - ic * net->RC - ie * net->RE + vee_mag;
    }

    qp->IC    = ic;
    qp->IB    = ib;
    qp->IE    = ie;
    qp->VCE   = vce;
    qp->VBE   = vbe;
    qp->VBC   = vbe - vce;
    qp->power = (net->VCC + vee_mag) * ie;
    qp->region = bjt_determine_region(vbe, vbe - vce, vt, BJT_NPN);

    return 0;
}

/* ================================================================
 * Generic Bias Solver (Dispatcher)
 * ================================================================ */

int bjt_bias_solve(const bjt_spice_params_t *dev,
                    const bjt_bias_network_t *net,
                    double temperature_kelvin,
                    bjt_qpoint_t *qp)
{
    if (!dev || !net || !qp) return -1;

    switch (net->type) {
        case BIAS_FIXED_BASE:
            return bjt_bias_fixed(dev, net, temperature_kelvin, qp);
        case BIAS_EMITTER_STABILIZED:
            return bjt_bias_emitter_stabilized(dev, net, temperature_kelvin, qp);
        case BIAS_VOLTAGE_DIVIDER:
            return bjt_bias_voltage_divider(dev, net, temperature_kelvin, qp);
        case BIAS_COLLECTOR_FEEDBACK:
            return bjt_bias_collector_feedback(dev, net, temperature_kelvin, qp);
        case BIAS_DUAL_SUPPLY:
            return bjt_bias_dual_supply(dev, net, temperature_kelvin, qp);
        default:
            /* Unknown bias type ? try voltage divider as safe default */
            /* default type, already set */
            return bjt_bias_voltage_divider(dev, net, temperature_kelvin, qp);
    }
}

/* ================================================================
 * Load Line Analysis
 * ================================================================ */

void bjt_load_line_dc(const bjt_bias_network_t *net,
                      double *ic_sat, double *vce_cutoff,
                      double *slope)
{
    /* DC load line equation: VCE = VCC - IC * (RC + RE)
     *
     * Saturation point (VCE = 0): IC_sat = VCC / (RC + RE)
     * Cutoff point (IC = 0): VCE_cutoff = VCC
     * Slope = d(VCE)/d(IC) = -(RC + RE)
     *
     * The Q-point must lie on this line. The load line
     * graphically shows all possible (VCE, IC) operating points. */

    double r_total;
    if (!net) return;

    r_total = net->RC + net->RE;
    if (r_total <= 0.0) r_total = 1.0;  /* Avoid divide-by-zero */

    if (ic_sat) *ic_sat = net->VCC / r_total;
    if (vce_cutoff) *vce_cutoff = net->VCC;
    if (slope) *slope = -r_total;
}

void bjt_load_line_ac(double RC, double RL, double RE_bypassed,
                      const bjt_qpoint_t *qp,
                      double *ac_slope, double *vce_intercept,
                      double *ic_intercept)
{
    (void)RE_bypassed;
    /* AC load line: during AC operation, the effective load is
     * RC || RL (since coupling cap isolates DC, RE may be bypassed).
     *
     * The AC load line passes through the Q-point with slope = -1/(RC||RL).
     * The Q-point must be centered on the AC load line for maximum
     * symmetric swing without clipping. */

    double r_ac;
    if (!qp) return;

    if (RC <= 0.0 && RL <= 0.0) {
        r_ac = 1e6;  /* No load */
    } else if (RC <= 0.0) {
        r_ac = RL;
    } else if (RL <= 0.0) {
        r_ac = RC;
    } else {
        r_ac = (RC * RL) / (RC + RL);  /* RC || RL */
    }

    if (ac_slope) *ac_slope = -1.0 / r_ac;

    /* The AC load line equation: ic_ac = -vce_ac / (RC||RL)
     * The line passes through the Q-point (VCE, IC):
     *   IC + i_c = -(VCE + v_ce) / r_ac  (incorrect sign convention)
     *
     * Correct: The total instantaneous quantities lie on the AC load line.
     *   i_C = I_CQ - v_ce / r_ac
     *   v_CE = V_CEQ + v_ce
     *   i_C = I_CQ - (v_CE - V_CEQ) / r_ac
     *   i_C = (V_CEQ + I_CQ * r_ac - v_CE) / r_ac
     *
     * Intercepts:
     *   v_CE intercept (i_C = 0): V_CEQ + I_CQ * r_ac
     *   i_C intercept (v_CE = 0): I_CQ + V_CEQ / r_ac
     */

    if (vce_intercept) *vce_intercept = qp->VCE + qp->IC * r_ac;
    if (ic_intercept)   *ic_intercept   = qp->IC + qp->VCE / r_ac;
}

/* ================================================================
 * Beta Sensitivity
 * ================================================================ */

double bjt_bias_beta_sensitivity(const bjt_bias_network_t *net,
                                  double beta, double ic_nominal)
{
    (void)ic_nominal;
    /* Beta sensitivity S_beta = (dIC/IC) / (dbeta/beta).
     *
     * For voltage-divider bias:
     *   S_beta ? (1 + (Rth/((beta+1)*RE))) / (beta * (1 + RE/re))
     *
     * Simplified (Sedra & Smith, 8th ed., Eq. 7.155):
     *   S_beta = (IC/beta) * dIC/d(beta)
     *          ? (RE + re) / ((beta+1)*RE + rpi) for emitter-stabilized
     *
     * Lower is better. For a well-designed voltage-divider bias
     * with Rth << (beta+1)*RE: S_beta ? 0.1 or less.
     *
     * Returns: fractional change in IC per fractional change in beta. */

    double rth, vth_dummy;

    if (!net || beta <= 0.0) return 1.0;

    if (net->type == BIAS_VOLTAGE_DIVIDER && net->R1 > 0.0 && net->R2 > 0.0) {
        bjt_bias_thevenin(net->R1, net->R2, net->VCC, &vth_dummy, &rth);
    } else {
        rth = net->RB;
    }

    if (net->RE <= 0.0) {
        /* No emitter degeneration ? high beta sensitivity */
        return 1.0;
    }

    return rth / ((beta + 1.0) * net->RE + rth);
}

/* ================================================================
 * VBE Temperature Coefficient
 * ================================================================ */

double bjt_bias_vbe_tempco(double temperature_kelvin, double ic,
                            double IS, double NF)
{
    (void)ic; (void)IS; (void)NF;
    /* dVBE/dT ? -(VGO - VBE) / T - 3*VT/T
     *
     * Simplified rule of thumb for silicon at 300K:
     *   dVBE/dT ? -2 mV/K
     *
     * More accurate formula (derived from VBE = NF*VT*ln(IC/IS)):
     *   dVBE/dT = VBE/T - NF*VT/T * (3 + EG/VT)
     *           = (VBE - (NF*k*T/q)*(3 + EG/VT)) / T
     *
     * This negative tempco is fundamental ? it means IC rises
     * with temperature for a fixed bias voltage. */

    /* vt not needed for rule-of-thumb estimate */

    if (temperature_kelvin <= 0.0 || ic <= 0.0) return -0.002;

    /* Rule-of-thumb: ~ -2 mV/K for silicon BJT */
    return -0.002;
}

/* ================================================================
 * Design Voltage-Divider Bias Network
 * ================================================================ */

int bjt_bias_design_voltage_divider(double IC_target, double VCE_target,
                                      double VCC, double beta_min,
                                      const bjt_spice_params_t *dev,
                                      double temperature_kelvin,
                                      bjt_bias_network_t *net)
{
    /* Design a stable voltage-divider bias network for target IC, VCE.
     *
     * Design procedure (Sedra & Smith, Sec. 7.4):
     * 1. Choose V_RE = VCC/10 (rule of thumb: 10% of supply for stability)
     * 2. RE = V_RE / IE ? V_RE / IC_target
     * 3. RC = (VCC - VCE_target - V_RE) / IC_target
     * 4. Rth = 0.1 * beta_min * RE  (stability criterion)
     * 5. Vth = VBE + V_RE + IB*Rth ? VBE + V_RE
     * 6. R1 = Rth * VCC / Vth, R2 = Rth / (1 - Vth/VCC)
     */

    double V_RE, VBE, IE, IB, Vth, Rth;

    if (!dev || !net || IC_target <= 0.0 || VCC <= 0.0) return -1;
    if (VCE_target >= VCC) return -1;
    if (VCE_target <= 0.5) VCE_target = VCC / 2.0;

    /* Step 1: Emitter voltage */
    V_RE = VCC / 10.0;

    /* Step 2: Emitter resistor */
    IE = IC_target;  /* Approximation: IE ? IC */
    net->RE = V_RE / IE;

    /* Step 3: Collector resistor */
    net->RC = (VCC - VCE_target - V_RE) / IC_target;
    if (net->RC <= 0.0) {
        net->RC = (VCC - VCE_target - 0.5) / IC_target;
        if (net->RC <= 0.0) return -1;
    }

    /* Step 4: Thevenin resistance (stability criterion) */
    Rth = 0.1 * beta_min * net->RE;
    if (Rth < 100.0) Rth = 1000.0;  /* Minimum to avoid excessive divider current */

    /* Step 5: Thevenin voltage */
    VBE = bjt_vbe_from_ic(IC_target, dev->IS,
                           bjt_vt(temperature_kelvin), dev->NF);
    IB = IC_target / beta_min;
    Vth = VBE + V_RE + IB * Rth;

    if (Vth >= VCC) return -1;

    /* Step 6: R1 and R2 from Thevenin equivalent */
    net->R1 = Rth * VCC / Vth;
    net->R2 = Rth / (1.0 - Vth / VCC);

    if (net->R1 <= 0.0 || net->R2 <= 0.0) return -1;

    net->VCC = VCC;
    net->VEE = 0.0;
    net->RB  = 0.0;
    net->Rs  = 50.0;  /* Default source impedance */
    net->type = BIAS_VOLTAGE_DIVIDER;

    return 0;
}

/* ================================================================
 * Power Dissipation
 * ================================================================ */

void bjt_bias_power_dissipation(const bjt_bias_network_t *net,
                                 const bjt_qpoint_t *qp,
                                 double *p_total, double *p_RC,
                                 double *p_RE, double *p_R1R2)
{
    double ic, ie, i_divider;

    if (!net || !qp) return;

    ic = qp->IC;
    ie = qp->IE;

    if (p_RC)    *p_RC    = ic * ic * net->RC;
    if (p_RE)    *p_RE    = ie * ie * net->RE;

    if (p_R1R2) {
        if (net->R1 > 0.0 && net->R2 > 0.0) {
            i_divider = net->VCC / (net->R1 + net->R2);
            *p_R1R2 = net->VCC * i_divider;
        } else {
            *p_R1R2 = 0.0;
        }
    }

    if (p_total) {
        *p_total = net->VCC * (ic + qp->IB);
        if (p_R1R2) *p_total += *p_R1R2;
    }
}

/* ================================================================
 * Temperature-Compensated Biasing
 * ================================================================ */

double bjt_bias_temp_compensated_vth(double VCC, double R1, double R2,
                                      double diode_IS, double temperature_kelvin)
{
    (void)VCC; (void)R1; (void)R2;
    /* Diode-compensated bias: a diode with matching IS provides
     * temperature tracking. The diode forward voltage tracks VBE,
     * so the voltage across RE remains approximately constant.
     *
     * This function returns the compensated Vth that maintains
     * approximately constant IE across temperature.
     *
     * Without compensation: IE ? (Vth - VBE(T)) / RE.
     * With diode compensation: Vth also varies with T, tracking VBE.
     *   Vth(T) = VCC * R2/(R1+R2) + V_diode(T) * R1/(R1+R2)
     *
     * Simplified: we return the target Vth = VBE(T) + IE_target * RE.
     */

    double vt = bjt_vt(temperature_kelvin);
    double vbe;

    /* Assume nominal IC of 1 mA for diode current estimation */
    vbe = bjt_vbe_from_ic(1e-3, diode_IS, vt, 1.0);
    return vbe;
}

/* ================================================================
 * Temperature Shift Estimation
 * ================================================================ */

double bjt_bias_temp_shift(const bjt_spice_params_t *dev,
                            const bjt_bias_network_t *net,
                            double temperature_kelvin_new,
                            double ic_nominal, double beta_nominal)
{
    (void)beta_nominal;
    /* Estimate fractional change in IC due to temperature shift.
     *
     * Three temperature effects:
     * 1. IS increases with T: IS ? T^3 * exp(-EG/VT)
     * 2. beta increases with T: beta ? T^1.5 (approx)
     * 3. VBE decreases with T: dVBE/dT ? -2 mV/K
     *
     * For a well-designed voltage-divider bias with good stability,
     * the net effect is < 10% IC shift over 0-70?C commercial range. */

    double t_nom, t_new, delta_vbe, frac_change;

    if (!dev || !net) return 0.0;
    if (ic_nominal <= 0.0) return 0.0;

    t_nom = dev->TNOM;
    t_new = temperature_kelvin_new;

    if (t_nom <= 0.0) t_nom = 300.0;

    /* VBE shift: -2 mV/K * delta_T */
    delta_vbe = -0.002 * (t_new - t_nom);

    /* IC fraction change depends on bias type.
     * For voltage divider: frac_change ? delta_vbe / V_RE.
     * For fixed bias: full delta_vbe effect. */
    if (net->type == BIAS_VOLTAGE_DIVIDER && net->RE > 0.0) {
        double v_re_nom = ic_nominal * net->RE;
        if (v_re_nom > 0.1) {
            frac_change = -delta_vbe / v_re_nom;
        } else {
            frac_change = -delta_vbe / 0.1;
        }
    } else {
        /* Fixed or emitter-stabilized: larger sensitivity */
        frac_change = -delta_vbe / 0.025;  /* ~ VT */
    }

    return frac_change;
}
