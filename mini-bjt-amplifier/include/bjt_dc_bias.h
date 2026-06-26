/**
 * bjt_dc_bias.h ? BJT DC Biasing Analysis
 *
 * DC biasing establishes the quiescent operating point (Q-point) for the BJT
 * amplifier. A stable Q-point is critical: it determines small-signal
 * parameters, ensures operation in forward-active region, and resists
 * variations due to temperature and beta spread.
 *
 * Covers 6 bias configurations taught across MIT 6.002, Berkeley EE105,
 * and Stanford EE101B:
 *
 *   1. Fixed Base Bias ? simplest, thermally unstable
 *   2. Emitter-Stabilized Bias ? RE provides DC feedback
 *   3. Voltage-Divider Bias ? most common, excellent stability
 *   4. Collector-Feedback Bias ? feedback from C to B
 *   5. Current-Source Bias ? IC design favorite
 *   6. Dual-Supply Bias ? symmetric +VCC/-VEE
 *
 * Reference: Sedra & Smith, Ch. 7; Gray & Meyer, Ch. 4
 *
 * Knowledge Levels:
 *   L2: DC operating point, bias stability, load-line analysis
 *   L3: KVL/KCL equation systems for bias networks
 *   L4: Thevenin equivalent for voltage-divider bias
 *   L5: Iterative and analytic Q-point solution methods
 *   L8: Temperature compensation, beta-sensitivity analysis
 */

#ifndef BJT_DC_BIAS_H
#define BJT_DC_BIAS_H

#include "bjt_model.h"

/* ---- L2: Bias Configuration Solver Declarations ---- */

/* Compute Thevenin equivalent for voltage-divider bias:
 *   Vth = VCC * R2 / (R1 + R2)
 *   Rth = R1 * R2 / (R1 + R2) = R1 || R2
 * Output parameters returned via pointers.
 * Complexity: O(1). */
void bjt_bias_thevenin(double R1, double R2, double VCC,
                       double *Vth, double *Rth);

/* Fixed Base Bias: RB to VCC, RE = 0, RC present.
 * Solves for Q-point assuming forward-active region.
 *   IB = (VCC - VBE) / RB
 *   IC = beta * IB
 *   VCE = VCC - IC * RC
 * Returns 0 on success, -1 if cutoff, -2 if saturation. */
int bjt_bias_fixed(const bjt_spice_params_t *dev,
                    const bjt_bias_network_t *net,
                    double temperature_kelvin,
                    bjt_qpoint_t *qp);

/* Emitter-Stabilized Bias: RB to VCC + RE.
 * KVL: VCC = IB*RB + VBE + (beta+1)*IB*RE
 *   IB = (VCC - VBE) / (RB + (beta+1)*RE)
 * The RE resistor provides negative feedback, stabilizing IC against beta.
 * Returns 0 on success, <0 on error. */
int bjt_bias_emitter_stabilized(const bjt_spice_params_t *dev,
                                 const bjt_bias_network_t *net,
                                 double temperature_kelvin,
                                 bjt_qpoint_t *qp);

/* Voltage-Divider Bias (most common discrete design).
 * Step 1: Compute Thevenin equivalent (Vth, Rth) of R1/R2.
 * Step 2: KVL ? Vth = IB*Rth + VBE + (beta+1)*IB*RE
 * Step 3: IB = (Vth - VBE) / (Rth + (beta+1)*RE)
 * Step 4: IC, VCE as usual.
 * This configuration is stable when Rth << (beta+1)*RE. */
int bjt_bias_voltage_divider(const bjt_spice_params_t *dev,
                              const bjt_bias_network_t *net,
                              double temperature_kelvin,
                              bjt_qpoint_t *qp);

/* Collector-Feedback Bias: RB connected from collector to base.
 * KVL: VCC = (IC+IB)*RC + IB*RB + VBE
 *   IB = (VCC - VBE) / (RB + (beta+1)*RC)
 * This provides negative feedback: increasing IC lowers VC,
 * which reduces IB, counteracting the increase. */
int bjt_bias_collector_feedback(const bjt_spice_params_t *dev,
                                 const bjt_bias_network_t *net,
                                 double temperature_kelvin,
                                 bjt_qpoint_t *qp);

/* Current-Source Bias: emitter current forced by external current source I_EE.
 * Simplifies to: IE = I_EE, VBE computed from IS/NF/VT.
 * Common in IC designs (current mirrors). */
int bjt_bias_current_source(const bjt_spice_params_t *dev,
                             double I_EE, double VCC, double RC,
                             double temperature_kelvin,
                             bjt_qpoint_t *qp);

/* Dual-Supply Bias: +/-VCC, emitter to GND via RE, base via RB to GND.
 * KVL for base loop: 0 = IB*RB + VBE + IE*RE - VEE
 *   (with VEE negative for NPN, making VEE < 0 so -VEE > 0)
 * Actually: GND - IB*RB - VBE - IE*RE + VEE = 0 ? IE = (VEE - VBE)/(RB/(beta+1) + RE)
 * For typical: VEE is negative supply, emitter sees -VEE through RE. */
int bjt_bias_dual_supply(const bjt_spice_params_t *dev,
                          const bjt_bias_network_t *net,
                          double temperature_kelvin,
                          bjt_qpoint_t *qp);

/* Generic bias solver: dispatches to the correct function based on net->type.
 * Complexity: O(1). */
int bjt_bias_solve(const bjt_spice_params_t *dev,
                    const bjt_bias_network_t *net,
                    double temperature_kelvin,
                    bjt_qpoint_t *qp);

/* ---- L4: Load Line Analysis ---- */

/* Compute DC load line for a CE amplifier.
 * DC load line: VCE = VCC - IC*(RC + RE)
 * Saturation point: IC_sat = VCC / (RC + RE) at VCE = 0
 * Cutoff point: VCE_cutoff = VCC at IC = 0
 * The load_line_slope = -1/(RC+RE).
 * Complexity: O(1). */
void bjt_load_line_dc(const bjt_bias_network_t *net,
                      double *ic_sat, double *vce_cutoff,
                      double *slope);

/* Compute AC load line for a CE amplifier with load RL.
 * AC load: ic_ac = -vce_ac / (RC || RL)
 * Different from DC load line because coupling caps remove RC+RE from
 * the AC path (RE may be bypassed). The Q-point must remain within
 * the AC load line for linear operation.
 * Complexity: O(1). */
void bjt_load_line_ac(double RC, double RL, double RE_bypassed,
                      const bjt_qpoint_t *qp,
                      double *ac_slope, double *vce_intercept,
                      double *ic_intercept);

/* ---- L5: Stability Analysis Methods ---- */

/* Beta sensitivity: how much does IC change per unit change in beta?
 * S_beta = dIC/d(beta) ? (delta_IC / IC) / (delta_beta / beta)
 * For voltage-divider bias:
 *   S_beta ? (1 + RB/((beta+1)*RE))^(-1) * (1/beta)
 * Lower S_beta = better stability. Ideal: S_beta -> 0.
 * Reference: Millman & Halkias, "Integrated Electronics", Ch. 10. */
double bjt_bias_beta_sensitivity(const bjt_bias_network_t *net,
                                  double beta, double ic_nominal);

/* Temperature sensitivity of VBE: dVBE/dT ? -2 mV/K for silicon BJT.
 * This means IC increases with temperature (VBE drops, same bias -> more IC).
 * Compensation requires making Vth >> VBE or using diode compensation.
 * Complexity: O(1). */
double bjt_bias_vbe_tempco(double temperature_kelvin, double ic,
                            double IS, double NF);

/* Design a stable voltage-divider bias network.
 * Given target IC, VCE, VCC, and device params, computes R1, R2, RC, RE.
 * Design rules (rule-of-thumb):
 *   VRE = VCC/10 (good compromise between swing and stability)
 *   Rth = 0.1 * (beta_min + 1) * RE (stable bias rule)
 *   VCE ? VCC/2 (maximum symmetric swing)
 * Returns 0 on success, -1 if impossible (e.g., VCE_target > VCC). */
int bjt_bias_design_voltage_divider(double IC_target, double VCE_target,
                                      double VCC, double beta_min,
                                      const bjt_spice_params_t *dev,
                                      double temperature_kelvin,
                                      bjt_bias_network_t *net);

/* Compute power dissipation in the bias network.
 * P_total = VCC * (IC + I_R1R2_divider)
 * Individual: P_RC = IC^2 * RC, P_RE = IE^2 * RE,
 *             P_R1R2 = VCC^2 / (R1+R2) for divider.
 * Complexity: O(1). */
void bjt_bias_power_dissipation(const bjt_bias_network_t *net,
                                 const bjt_qpoint_t *qp,
                                 double *p_total, double *p_RC,
                                 double *p_RE, double *p_R1R2);

/* ---- L8: Temperature-Compensated Biasing ---- */

/* Compute temperature-compensated Vth for a diode-compensated divider.
 * Using a diode with matching IS/VT characteristics:
 *   The diode current sets Vth that tracks VBE tempco.
 * Complexity: O(1). */
double bjt_bias_temp_compensated_vth(double VCC, double R1, double R2,
                                      double diode_IS, double temperature_kelvin);

/* Compute Q-point shift due to temperature change (delta_T).
 * Uses the tempco models for IS(T), beta(T), and VBE(T).
 * IS(T) = IS(Tnom) * (T/Tnom)^XTI * exp(EG/VT * (T/Tnom - 1))
 * beta(T) ? beta(Tnom) * (T/Tnom)^1.5
 * Returns delta_IC as a fraction of IC_nominal. */
double bjt_bias_temp_shift(const bjt_spice_params_t *dev,
                            const bjt_bias_network_t *net,
                            double temperature_kelvin_new,
                            double ic_nominal, double beta_nominal);

#endif /* BJT_DC_BIAS_H */
