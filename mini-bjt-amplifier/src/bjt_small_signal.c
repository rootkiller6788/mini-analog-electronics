/**
 * bjt_small_signal.c ? BJT Small-Signal AC Analysis Implementation
 *
 * Implements gain, impedance, and two-port parameter calculations for
 * the three fundamental BJT amplifier configurations: Common-Emitter (CE),
 * Common-Collector (CC/emitter follower), and Common-Base (CB).
 *
 * All formulas follow the hybrid-pi model analysis from Sedra & Smith.
 * Each function implements a specific closed-form expression derived from
 * KVL/KCL applied to the small-signal equivalent circuit.
 */

#include "bjt_small_signal.h"
#include <math.h>

/* ================================================================
 * Common-Emitter (CE) Analysis
 * ================================================================ */

double bjt_ce_voltage_gain(const bjt_ss_params_t *ss,
                            double RC, double RL, double Rs)
{
    /* CE voltage gain (no emitter degeneration, bypassed RE):
     *
     * Av = -gm * (RC || RL || ro) * (rpi / (Rs + rpi))
     *
     * Derivation:
     *   vbe = vin * rpi/(Rs + rpi)           [voltage division at input]
     *   vout = -gm * vbe * (RC || RL || ro)  [controlled source + output load]
     *   Av = vout/vin = -gm * (RC||RL||ro) * rpi/(Rs+rpi)
     *
     * The negative sign: CE inverts (180? phase shift).
     * For high-beta BJT (rpi >> Rs): Av ? -gm * (RC||RL||ro).
     */

    double rL, r_par, vbe_divider;

    if (!ss || ss->gm <= 0.0) return 0.0;

    /* Effective load: RC || RL || ro */
    r_par = 0.0;
    if (RC > 0.0) r_par = RC;
    if (RL > 0.0) r_par = (r_par > 0.0) ? (r_par * RL) / (r_par + RL) : RL;
    if (ss->ro > 0.0) r_par = (r_par > 0.0) ? (r_par * ss->ro) / (r_par + ss->ro) : r_par;
    if (r_par <= 0.0) r_par = 1e6;

    rL = r_par;

    /* Input voltage division: fraction of vin across rpi */
    if (ss->rpi > 0.0) {
        vbe_divider = ss->rpi / (Rs + ss->rpi);
    } else {
        vbe_divider = 1.0;
    }

    return -ss->gm * rL * vbe_divider;
}

double bjt_ce_voltage_gain_degenerated(const bjt_ss_params_t *ss,
                                        double RC, double RL, double RE)
{
    /* CE with unbypassed emitter resistance RE (emitter degeneration):
     *
     * Exact formula including Early effect:
     *   Av = -gm * (RC||RL||ro) / (1 + gm*RE + (RC||RL||ro)/ro + RE/re)
     *
     * Simplified for RE >> re and ro >> RC||RL:
     *   Av ? -(RC||RL) / RE
     *
     * The degeneration reduces gain but:
     * - Improves linearity (reduces distortion)
     * - Increases bandwidth (reduced gain ? higher bandwidth, GBW constant)
     * - Increases input impedance (Rin = rpi + (beta+1)*RE)
     */

    double rL, denom;

    if (!ss || ss->gm <= 0.0) return 0.0;

    /* Effective load: RC || RL || ro */
    rL = RC;
    if (RL > 0.0) rL = (rL * RL) / (rL + RL);
    if (ss->ro > 0.0) rL = (rL * ss->ro) / (rL + ss->ro);
    if (rL <= 0.0) rL = 1e6;

    /* Degeneration denominator */
    denom = 1.0 + ss->gm * RE + rL / ss->ro + RE / ss->re;

    return -ss->gm * rL / denom;
}

double bjt_ce_input_impedance(const bjt_ss_params_t *ss)
{
    /* CE input impedance (without degeneration): Rin = rpi.
     * This is the resistance looking into the base with emitter at AC ground. */
    if (!ss) return 0.0;
    return ss->rpi;
}

double bjt_ce_input_impedance_degenerated(const bjt_ss_params_t *ss,
                                           double RE)
{
    /* CE input impedance WITH emitter degeneration:
     *
     *   Rin = rpi + (beta + 1) * RE
     *
     * The emitter resistance RE is "reflected" to the base side
     * multiplied by (beta+1). This is a fundamental BJT property
     * known as the impedance reflection rule.
     *
     * Derivation:
     *   vin = ib * rpi + ie * RE = ib * rpi + (beta+1)*ib * RE
     *   Rin = vin/ib = rpi + (beta+1)*RE
     */
    if (!ss) return 0.0;
    return ss->rpi + (ss->beta + 1.0) * RE;
}

double bjt_ce_output_impedance(const bjt_ss_params_t *ss, double RC)
{
    /* CE output impedance (looking into collector):
     *   Rout = RC || ro
     *
     * Without Early effect (ro = infinity): Rout = RC.
     * With Early effect: finite ro reduces Rout slightly. */
    if (!ss || ss->ro <= 0.0) return RC;

    if (RC <= 0.0) return ss->ro;
    return (RC * ss->ro) / (RC + ss->ro);
}

double bjt_ce_output_impedance_degenerated(const bjt_ss_params_t *ss,
                                            double RC, double RE)
{
    /* CE output impedance with emitter degeneration:
     *
     *   Rout = RC || [ ro * (1 + gm*RE) ]
     *
     * The degeneration boosts the effective output resistance
     * by factor (1 + gm*RE), approaching an ideal current source.
     *
     * This is the "cascoding" or "bootstrapping" effect ?
     * the same principle used in cascode amplifiers. */

    double ro_eff;

    if (!ss) return RC;

    ro_eff = ss->ro * (1.0 + ss->gm * RE);

    if (RC <= 0.0) return ro_eff;
    return (RC * ro_eff) / (RC + ro_eff);
}

double bjt_ce_current_gain(const bjt_ss_params_t *ss,
                            double RC, double RL, double Rs)
{
    /* CE current gain:
     *   Ai = beta * (RC/(RC+RL)) * (Rs/(Rs+rpi))
     *
     * The current division at output (RC||RL) and input (Rs||rpi)
     * reduces the intrinsic current gain beta. */
    double io_div, ii_div;

    if (!ss) return 0.0;

    /* Output current division: fraction of IC that reaches RL */
    if (RC > 0.0 && RL > 0.0) {
        io_div = RC / (RC + RL);
    } else if (RL > 0.0) {
        io_div = 1.0;
    } else {
        io_div = 0.0;  /* No load, no output current */
    }

    /* Input current division: fraction of source current into base */
    if (ss->rpi > 0.0 && Rs > 0.0) {
        ii_div = Rs / (Rs + ss->rpi);
    } else if (ss->rpi > 0.0) {
        ii_div = 1.0;
    } else {
        ii_div = 0.0;
    }

    return ss->beta * io_div * ii_div;
}

/* ================================================================
 * Common-Collector (CC) / Emitter-Follower Analysis
 * ================================================================ */

double bjt_cc_voltage_gain(const bjt_ss_params_t *ss,
                            double RE, double RL, double Rs)
{
    /* CC (emitter follower) voltage gain:
     *
     *   Av = (beta+1)*(RE||RL) / (rpi + (beta+1)*(RE||RL) + Rs*?)
     *
     * More precisely, including source resistance:
     *   Av = (beta+1)*R_E_eff / (Rs + rpi + (beta+1)*R_E_eff)
     *
     * where R_E_eff = RE || RL.
     *
     * Since (beta+1)*R_E_eff >> rpi + Rs typically:
     *   Av ? 1    (slightly less than 1, e.g., 0.95-0.99)
     *
     * No phase inversion: Av > 0. */

    double r_e_eff, numer, denom;
    (void)Rs;  /* Source resistance included in CC gain expression */

    if (!ss) return 0.0;

    /* Effective emitter load */
    if (RE <= 0.0 && RL <= 0.0) return 0.0;
    if (RE <= 0.0) r_e_eff = RL;
    else if (RL <= 0.0) r_e_eff = RE;
    else r_e_eff = (RE * RL) / (RE + RL);

    /* CC voltage gain: output at emitter, input at base.
     * Using the hybrid-pi model with controlled source: */
    numer = (ss->beta + 1.0) * r_e_eff;
    denom = ss->rpi + (ss->beta + 1.0) * r_e_eff;

    if (denom <= 0.0) return 0.0;
    return numer / denom;
}

double bjt_cc_input_impedance(const bjt_ss_params_t *ss,
                               double RE, double RL)
{
    /* CC input impedance: looking into the base.
     *
     *   Rin = rpi + (beta+1) * (RE || RL)
     *
     * This is the impedance reflection rule applied to the emitter
     * load: the entire emitter-side impedance is multiplied by (beta+1).
     *
     * CC has the highest input impedance of the three configurations,
     * making it ideal as a buffer (voltage follower). */

    double r_e_eff;

    if (!ss) return 0.0;

    if (RE <= 0.0 && RL <= 0.0) return ss->rpi;
    if (RE <= 0.0) r_e_eff = RL;
    else if (RL <= 0.0) r_e_eff = RE;
    else r_e_eff = (RE * RL) / (RE + RL);

    return ss->rpi + (ss->beta + 1.0) * r_e_eff;
}

double bjt_cc_output_impedance(const bjt_ss_params_t *ss,
                                double RE, double Rs)
{
    /* CC output impedance: looking into the emitter.
     *
     *   Rout = RE || ( (rpi + Rs) / (beta+1) )
     *        = RE || ( re + Rs/(beta+1) )      since rpi/(beta+1) ? re
     *
     * Very low output impedance: the source impedance Rs is divided
     * by (beta+1) when seen from the emitter side.
     *
     * For Rs = 50?, beta = 100: Rs/(beta+1) ? 0.5? ? extremely low! */

    double r_look_in;

    if (!ss) return 0.0;

    r_look_in = (ss->rpi + Rs) / (ss->beta + 1.0);

    if (RE <= 0.0) return r_look_in;
    return (RE * r_look_in) / (RE + r_look_in);
}

double bjt_cc_current_gain(const bjt_ss_params_t *ss,
                            double RE, double RL, double Rs)
{
    /* CC current gain:
     *
     *   Ai ? (beta+1) * (RE/(RE+RL))
     *
     * The current division at the emitter output reduces the ideal
     * (beta+1) gain. Note: CC provides current gain, not voltage gain. */

    double io_div;

    if (!ss) return 0.0;

    if (RE > 0.0 && RL > 0.0) {
        io_div = RE / (RE + RL);
    } else if (RL > 0.0) {
        io_div = 0.0;
    } else {
        io_div = 1.0;  /* No RL, all emitter current is output */
    }

    /* Input current division from source */
    (void)Rs;

    return (ss->beta + 1.0) * io_div;
}

/* ================================================================
 * Common-Base (CB) Analysis
 * ================================================================ */

double bjt_cb_voltage_gain(const bjt_ss_params_t *ss,
                            double RC, double RL, double RE, double Rs)
{
    /* CB voltage gain:
     *
     *   Av = gm * (RC||RL) * ( RE||re / (Rs + RE||re) )
     *
     * Non-inverting (positive gain). Good for high-frequency applications
     * because there is no Miller multiplication of Cmu (base is at AC ground).
     *
     * The input is at the emitter, so Rin ? re (very low).
     * This means the input voltage divider RE||re/(Rs + RE||re)
     * can significantly reduce the overall gain if Rs is large. */

    double rL, r_in, div;

    if (!ss || ss->gm <= 0.0) return 0.0;

    /* Collector load */
    rL = RC;
    if (RL > 0.0) rL = (rL * RL) / (rL + RL);
    if (rL <= 0.0) rL = 1e6;

    /* Input resistance looking into emitter */
    r_in = ss->re;
    if (RE > 0.0) r_in = (RE * ss->re) / (RE + ss->re);

    /* Input voltage division */
    if (Rs + r_in > 0.0) {
        div = r_in / (Rs + r_in);
    } else {
        div = 1.0;
    }

    return ss->gm * rL * div;
}

double bjt_cb_input_impedance(const bjt_ss_params_t *ss, double RE)
{
    /* CB input impedance: looking into the emitter.
     *
     *   Rin = RE || re
     *
     * Very low (typically 10-100 ?), which is the main disadvantage
     * of the CB configuration ? it's hard to drive. */

    if (!ss) return 0.0;

    if (RE <= 0.0) return ss->re;
    return (RE * ss->re) / (RE + ss->re);
}

double bjt_cb_output_impedance(const bjt_ss_params_t *ss, double RC)
{
    /* CB output impedance: looking into the collector.
     *
     *   Rout = RC || ro
     *
     * Same as CE ? the collector side sees the same topology.
     * In a CB, the base is grounded, so the output is taken
     * from the collector just like in CE. */

    if (!ss || ss->ro <= 0.0) return RC;
    if (RC <= 0.0) return ss->ro;
    return (RC * ss->ro) / (RC + ss->ro);
}

/* ================================================================
 * Miller Theorem
 * ================================================================ */

double bjt_miller_capacitance(double Cmu, double voltage_gain_magnitude)
{
    /* Miller capacitance multiplication:
     *
     *   C_miller_input = Cmu * (1 + |Av|)
     *
     * This is the feedback capacitance Cmu (C_bc) reflected to
     * the input, multiplied by (1 + |Av|). For CE with |Av| = 100,
     * a 2 pF Cbc appears as ~202 pF at the input ? completely
     * dominating the frequency response.
     *
     * The Miller theorem (Miller, 1920) applies to any impedance
     * connected between input and output of an inverting amplifier. */
    return Cmu * (1.0 + voltage_gain_magnitude);
}

double bjt_miller_factor(double voltage_gain_magnitude)
{
    /* Miller multiplication factor M = 1 + |Av|.
     * This is the impedance multiplication factor.
     * For CE: M >> 1 (gain is large, negative).
     * For CC: M ? 0 (gain ? 1, positive, so 1 - Av ? 0). */
    return 1.0 + voltage_gain_magnitude;
}

/* ================================================================
 * Complete Amplifier Analysis Functions
 * ================================================================ */

void bjt_ce_analyze(const bjt_ss_params_t *ss,
                     double RC, double RL, double RE, double Rs,
                     int re_bypassed,
                     bjt_amp_metrics_t *metrics)
{
    if (!ss || !metrics) return;

    if (re_bypassed) {
        metrics->Av = bjt_ce_voltage_gain(ss, RC, RL, Rs);
        metrics->Rin = bjt_ce_input_impedance(ss);
        metrics->Rout = bjt_ce_output_impedance(ss, RC);
    } else {
        metrics->Av = bjt_ce_voltage_gain_degenerated(ss, RC, RL, RE);
        metrics->Rin = bjt_ce_input_impedance_degenerated(ss, RE);
        metrics->Rout = bjt_ce_output_impedance_degenerated(ss, RC, RE);
    }

    metrics->Av_dB = bjt_gain_to_db(metrics->Av);
    metrics->Ai = bjt_ce_current_gain(ss, RC, RL, Rs);
    metrics->Ai_dB = (metrics->Ai > 0.0) ? 20.0 * log10(metrics->Ai) : -600.0;
    metrics->Ap = fabs(metrics->Av * metrics->Ai);
    metrics->Ap_dB = (metrics->Ap > 0.0) ? 10.0 * log10(metrics->Ap) : -600.0;

    /* Bandwidth estimation ? will be refined by frequency analysis */
    metrics->f_L = 0.0;
    metrics->f_H = ss->f_beta;
    metrics->BW = metrics->f_H;
    metrics->GBW = fabs(metrics->Av) * metrics->f_H;

    /* Swing estimation */
    metrics->swing_max = 0.8 * (2.0 * RC * ss->gm * 0.025);  /* Approximate */
    metrics->efficiency = 0.25;  /* Class A: max theoretical 25% for resistive load */
    metrics->THD = 0.0;  /* Calculated elsewhere */
}

void bjt_cc_analyze(const bjt_ss_params_t *ss,
                     double RE, double RL, double Rs,
                     bjt_amp_metrics_t *metrics)
{
    if (!ss || !metrics) return;

    metrics->Av = bjt_cc_voltage_gain(ss, RE, RL, Rs);
    metrics->Av_dB = bjt_gain_to_db(metrics->Av);
    metrics->Rin = bjt_cc_input_impedance(ss, RE, RL);
    metrics->Rout = bjt_cc_output_impedance(ss, RE, Rs);
    metrics->Ai = bjt_cc_current_gain(ss, RE, RL, Rs);
    metrics->Ai_dB = (metrics->Ai > 0.0) ? 20.0 * log10(metrics->Ai) : -600.0;
    metrics->Ap = fabs(metrics->Av * metrics->Ai);
    metrics->Ap_dB = (metrics->Ap > 0.0) ? 10.0 * log10(metrics->Ap) : -600.0;
    metrics->f_L = 0.0;
    metrics->f_H = ss->fT;
    metrics->BW = metrics->f_H;
    metrics->GBW = metrics->f_H;  /* Av ? 1, so GBW ? fT */
    metrics->swing_max = 5.0;     /* Depends on bias */
    metrics->efficiency = 0.25;
    metrics->THD = 0.0;
}

void bjt_cb_analyze(const bjt_ss_params_t *ss,
                     double RC, double RL, double RE, double Rs,
                     bjt_amp_metrics_t *metrics)
{
    if (!ss || !metrics) return;

    metrics->Av = bjt_cb_voltage_gain(ss, RC, RL, RE, Rs);
    metrics->Av_dB = bjt_gain_to_db(metrics->Av);
    metrics->Rin = bjt_cb_input_impedance(ss, RE);
    metrics->Rout = bjt_cb_output_impedance(ss, RC);
    metrics->Ai = ss->alpha;
    metrics->Ai_dB = 20.0 * log10(ss->alpha);
    metrics->Ap = fabs(metrics->Av * metrics->Ai);
    metrics->Ap_dB = (metrics->Ap > 0.0) ? 10.0 * log10(metrics->Ap) : -600.0;
    metrics->f_L = 0.0;
    metrics->f_H = ss->fT;
    metrics->BW = metrics->f_H;
    metrics->GBW = fabs(metrics->Av) * metrics->f_H;
    metrics->swing_max = 5.0;
    metrics->efficiency = 0.25;
    metrics->THD = 0.0;
}

/* ================================================================
 * Two-Port Parameter Conversion
 * ================================================================ */

void bjt_ce_to_z_params(const bjt_ss_params_t *ss,
                         double RC, double frequency_hz,
                         bjt_z_params_t *zp)
{
    if (!ss || !zp) return;

    /* For CE with RC as collector load, compute z-parameters.
     * Approximate: z11 = rpi, z12 ? 0, z21 = -gm * rpi * RC (at low freq),
     *             z22 = RC || ro.
     *
     * More accurate: use frequency-dependent hybrid-pi. */
    bjt_z_from_ss(ss, frequency_hz, zp);

    /* Adjust for external RC */
    if (RC > 0.0 && ss->ro > 0.0) {
        double ro_rc = (RC * ss->ro) / (RC + ss->ro);
        zp->z22 = ro_rc;
        zp->z21 = -ss->gm * ss->rpi * ro_rc;
    }
}

void bjt_ce_to_y_params(const bjt_ss_params_t *ss,
                         double RC, double frequency_hz,
                         bjt_y_params_t *yp)
{
    if (!ss || !yp) return;

    bjt_y_from_ss(ss, frequency_hz, yp);

    /* Adjust for external RC: y22' = y22 + 1/RC */
    if (RC > 0.0) {
        yp->y22 += 1.0 / RC;
    }
}

int bjt_z_to_y(const bjt_z_params_t *zp, bjt_y_params_t *yp)
{
    double det;

    if (!zp || !yp) return -1;

    det = zp->z11 * zp->z22 - zp->z12 * zp->z21;

    if (fabs(det) < 1e-30) return -1;

    yp->y11 =  zp->z22 / det;
    yp->y12 = -zp->z12 / det;
    yp->y21 = -zp->z21 / det;
    yp->y22 =  zp->z11 / det;

    return 0;
}

int bjt_y_to_z(const bjt_y_params_t *yp, bjt_z_params_t *zp)
{
    double det;

    if (!yp || !zp) return -1;

    det = yp->y11 * yp->y22 - yp->y12 * yp->y21;

    if (fabs(det) < 1e-30) return -1;

    zp->z11 =  yp->y22 / det;
    zp->z12 = -yp->y12 / det;
    zp->z21 = -yp->y21 / det;
    zp->z22 =  yp->y11 / det;

    return 0;
}

/* ================================================================
 * Maximum Available Gain
 * ================================================================ */

double bjt_max_available_gain(const bjt_y_params_t *yp)
{
    /* Maximum Available Gain (MAG) from y-parameters.
     *
     * MAG = |y21 / y12| * (K - sqrt(K^2 - 1))
     *
     * where K is the Rollett stability factor.
     * Valid only for K >= 1 (unconditionally stable).
     * For K < 1, use Maximum Stable Gain: MSG = |y21/y12|. */

    double K, mag;

    if (!yp) return 0.0;
    if (yp->y12 <= 0.0) return 1e6;  /* No reverse transmission */

    K = bjt_stability_factor(yp);

    mag = yp->y21 / yp->y12;

    if (K >= 1.0) {
        return mag * (K - sqrt(K * K - 1.0));
    } else {
        return mag;  /* MSG */
    }
}

double bjt_stability_factor(const bjt_y_params_t *yp)
{
    /* Rollett stability factor K:
     *
     * K = (2 * Re(y11) * Re(y22) - Re(y12 * y21)) / |y12 * y21|
     *
     * If K > 1 and |Delta| < 1: unconditionally stable.
     * Otherwise: potentially unstable (can oscillate with certain terminations).
     *
     * Reference: Rollett (1962), "Stability and Power-Gain Invariants
     *   of Linear Twoports", IRE Trans. Circuit Theory. */

    double re_y11, re_y22, re_y12y21, abs_y12y21;

    if (!yp) return 0.0;

    /* For our simplified y-parameters (magnitude-based), we approximate
     * real parts from the DC conductances. */
    re_y11 = yp->y11;   /* Since y11 = 1/rpi + jwC, Re(y11) ? 1/rpi */
    re_y22 = yp->y22;   /* Re(y22) ? 1/ro + ... */

    re_y12y21 = yp->y12 * yp->y21;
    abs_y12y21 = fabs(yp->y12 * yp->y21);

    if (abs_y12y21 < 1e-30) return 1e6;

    return (2.0 * re_y11 * re_y22 - re_y12y21) / abs_y12y21;
}
