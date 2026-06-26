/**
 * active_filter_sensitivity.h — Sensitivity Analysis & Non-Ideal Effects
 *
 * L3 Mathematical Structures: Sensitivity functions, Monte Carlo
 * tolerance analysis, op-amp non-ideality modeling.
 *
 * L4 Fundamental Laws: Sensitivity invariance theorems,
 * gain-bandwidth product effects on filter poles.
 *
 * Sensitivity: S_x^y = (∂y/∂x)·(x/y)
 * Measures fractional change in output characteristic y per
 * fractional change in component x. |S| < 1 is desirable.
 *
 * Key sensitivity principles for active filters:
 * - All topologies have S_R^ω0 = S_C^ω0 = -0.5 for ω₀
 * - Sallen-Key Q sensitivity grows with Q and gain K
 *   S^Q_K ≈ (2Q-1)·K for K > 1
 * - MFB Q sensitivity: S^Q_{C1/C2} ≈ Q (lower than SK for BP)
 * - KHN Q sensitivity: S^Q_R ≈ 1 (best — independent of Q!)
 * - LC ladder (leapfrog): near-zero sensitivity (inherited property)
 *
 * Courses: MIT 6.002, Berkeley EE105, Stanford EE101B
 * Reference: Snelgrove, "Synthesis and Analysis of Active RC
 *            Filters," in Schaumann & Van Valkenburg, Design of
 *            Analog Filters (2001), Ch. 15
 */

#ifndef ACTIVE_FILTER_SENSITIVITY_H
#define ACTIVE_FILTER_SENSITIVITY_H

#include "active_filter_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================
 * L3: Sensitivity Computation
 * ================================================================== */

/** Compute component sensitivities for Sallen-Key topology.
 *
 *  L3: Analytical sensitivity formulas (derived from transfer function).
 *  For Sallen-Key LP (unity gain, K=1, R1=R2=R, C1=C2=C):
 *    S_R^ω0 = S_C^ω0 = -0.5 (always, for any topology)
 *    S_R1^Q = -0.5 + Q
 *    S_R2^Q = -0.5
 *    S_C1^Q =  0.5 - Q
 *    S_C2^Q =  0.5
 *
 *  Notice: Q sensitivities grow linearly with Q for Sallen-Key.
 *  For Q=10, |S|_max ≈ 10 (10% component error → 100% Q error!).
 *  This is why Sallen-Key is limited to Q < 10.
 *
 *  @param comp     Component values
 *  @param function LP or HP
 *  @param sens     Output: sensitivity analysis results
 *  @return         ACTIVE_OK or error code
 *  Complexity: O(1) */
int sensitivity_sallen_key(const active_component_values_t *comp,
                            active_filter_func_t function,
                            active_sensitivity_t *sens);

/** Compute component sensitivities for MFB topology.
 *
 *  L3: MFB sensitivity is generally lower than Sallen-Key for BP.
 *  For MFB LP:
 *    S_R1^ω0 = S_R3^ω0 = S_C1^ω0 = S_C2^ω0 = -0.5
 *    S_R1^Q = -0.5 + 1/(1 + R1/R2 + R1/R3)
 *    S_C1^Q =  0.5 - Q·(C2/C1)
 *    S_C2^Q = -0.5 + Q·(C2/C1)
 *
 *  For MFB BP: Q sensitivity to C1/C2 ratio can be reduced by
 *  choosing C1 ≈ C2 when possible.
 *
 *  @param comp     Component values
 *  @param function LP, HP, or BP
 *  @param sens     Output sensitivity results
 *  Complexity: O(1) */
int sensitivity_mfb(const active_component_values_t *comp,
                     active_filter_func_t function,
                     active_sensitivity_t *sens);

/** Compute component sensitivities for KHN state-variable topology.
 *
 *  L3: KHN has the lowest sensitivity of any 3-op-amp topology.
 *    S_R^ω0 = S_C^ω0 = -0.5 (integrator R and C)
 *    S_Rq^Q ≈ 1 (Q-setting resistor only)
 *    All other S^Q ≈ 0.5
 *
 *  The Q sensitivity is independent of Q value — this is the
 *  key advantage of KHN over Sallen-Key for high-Q designs.
 *
 *  @param comp    Component values
 *  @param sens    Output sensitivity results
 *  Complexity: O(1) */
int sensitivity_khn(const active_component_values_t *comp,
                     active_sensitivity_t *sens);

/** Compute sensitivity for Tow-Thomas biquad.
 *  L3: Tow-Thomas has very similar sensitivity to KHN.
 *  S_Rq^Q = 1 (Q-setting resistor), all other S < 1.
 *
 *  @param comp    Component values
 *  @param sens    Output sensitivity results
 *  Complexity: O(1) */
int sensitivity_tow_thomas(const active_component_values_t *comp,
                            active_sensitivity_t *sens);

/** Generic sensitivity computation for any topology.
 *  L3: Uses numerical differentiation when analytical formulas
 *  aren't available. Perturbs each component by ±δ, evaluates
 *  resulting ω₀ and Q change, computes sensitivity.
 *
 *  @param comp      Component values
 *  @param topology  Which topology
 *  @param function  LP, HP, or BP
 *  @param sens      Output sensitivity
 *  @param delta     Perturbation fraction (e.g., 0.001 for 0.1%)
 *  Complexity: O(N_components · 2) — two evaluations per component */
int sensitivity_generic(const active_component_values_t *comp,
                         active_topology_t topology,
                         active_filter_func_t function,
                         active_sensitivity_t *sens, double delta);

/* ==================================================================
 * L4: Sensitivity Sum Invariance Theorem
 * ================================================================== */

/** Verify the sensitivity sum theorem for ω₀.
 *
 *  L4: Theorem (Sensitivity Sum Invariance):
 *  For any active RC filter, the sum of all passive component
 *  sensitivities with respect to ω₀ equals -N, where N is the
 *  number of reactive elements (capacitors).
 *
 *  Σ S_R^ω0 + Σ S_C^ω0 = -(number of capacitors)
 *
 *  This is a fundamental invariant — a consequence of dimensional
 *  homogeneity of the transfer function. It means you can never
 *  make ALL components have zero sensitivity simultaneously.
 *
 *  Reference: Fialkow & Gerst, "The Transfer Function of an
 *            RC Network," Quarterly Appl. Math., vol. 12, 1952
 *
 *  @param comp      Component values
 *  @param topology  Filter topology
 *  @param function  Filter function
 *  @param sum       Output: computed sensitivity sum
 *  @param expected  Output: expected sum (-num_capacitors)
 *  @return          ACTIVE_OK, or error if components null
 *  Complexity: O(1) */
int sensitivity_sum_theorem(const active_component_values_t *comp,
                             active_topology_t topology,
                             active_filter_func_t function,
                             double *sum, double *expected);

/** Verify the Q sensitivity envelope theorem.
 *
 *  L4: The sum of all Q sensitivities satisfies:
 *  Σ S_xi^Q = 0 for any biquadratic active filter.
 *
 *  This follows from the bilinear relationship between component
 *  values and the filter's quality factor. Combined with the ω₀
 *  sum theorem, this provides a complete sensitivity budget.
 *
 *  Reference: Snelgrove, IEEE Trans. CAS, vol. CAS-29, 1982
 *
 *  @param comp     Component values
 *  @param topology Filter topology
 *  @param function Filter function
 *  @param sum      Output: Q sensitivity sum (should ≈ 0)
 *  @return         ACTIVE_OK */
int sensitivity_q_sum_theorem(const active_component_values_t *comp,
                               active_topology_t topology,
                               active_filter_func_t function,
                               double *sum);

/* ==================================================================
 * L5: Monte Carlo Tolerance Analysis
 * ================================================================== */

/** Monte Carlo analysis of filter response under component tolerances.
 *
 *  L5: Statistical analysis using Monte Carlo method:
 *  1. For each trial, perturb each component by a Gaussian random
 *     amount (σ = tolerance/3 for ±3σ = tolerance range)
 *  2. Compute resulting ω₀, Q, and gain
 *  3. Accumulate statistics: mean, standard deviation, min, max
 *  4. Estimate yield = fraction of trials meeting spec
 *
 *  This directly models manufacturing variability. Unlike worst-case
 *  analysis, it gives realistic yield estimates.
 *
 *  @param comp         Nominal component values
 *  @param topology     Filter topology
 *  @param function     Filter function
 *  @param tolerance_pct Component tolerance (±%, e.g., 1.0 for ±1%)
 *  @param num_trials   Number of Monte Carlo trials (≥ 1000)
 *  @param wp_mean      Output: mean pole frequency
 *  @param wp_std       Output: std dev of pole frequency
 *  @param q_mean       Output: mean Q
 *  @param q_std        Output: std dev of Q
 *  @param yield_pct    Output: estimated yield (%)
 *  @return             ACTIVE_OK or error code
 *  Complexity: O(N_trials) */
int monte_carlo_tolerance(const active_component_values_t *comp,
                           active_topology_t topology,
                           active_filter_func_t function,
                           double tolerance_pct, int num_trials,
                           double *wp_mean, double *wp_std,
                           double *q_mean, double *q_std,
                           double *yield_pct);

/** Worst-case analysis (EVA — Extreme Value Analysis).
 *  L5: Evaluates all corner combinations of component tolerances
 *  to find the absolute worst-case response deviation.
 *  Reports min/max ω₀, Q, and gain across all corners.
 *
 *  @param comp          Nominal component values
 *  @param topology      Filter topology
 *  @param function      Filter function
 *  @param tolerance_pct Tolerance (%)
 *  @param wp_min        Output: minimum ω₀
 *  @param wp_max        Output: maximum ω₀
 *  @param q_min         Output: minimum Q
 *  @param q_max         Output: maximum Q
 *  @return              ACTIVE_OK
 *  Complexity: O(2^N_components) — 2 corners per component */
int worst_case_analysis(const active_component_values_t *comp,
                         active_topology_t topology,
                         active_filter_func_t function,
                         double tolerance_pct,
                         double *wp_min, double *wp_max,
                         double *q_min, double *q_max);

/* ==================================================================
 * L4: Op-Amp Non-Ideality Effects
 * ================================================================== */

/** Model the effect of finite GBW on filter poles.
 *
 *  L4: For a single-pole op-amp model: A(s) = GBW/s
 *  The actual filter poles shift from the ideal values.
 *
 *  First-order approximation (for Sallen-Key and MFB):
 *    ω₀_actual ≈ ω₀_ideal / (1 + 2·ω₀/(GBW·β))
 *    Q_actual ≈ Q_ideal · (1 + 2·Q·ω₀/(GBW))
 *
 *  where β is the feedback factor at the op-amp input.
 *  This is known as "Q-enhancement" — finite GBW always
 *  INCREASES Q, potentially causing instability for high-Q designs.
 *
 *  Q-enhancement can be compensated by pre-distorting the design
 *  (designing for lower Q than actually desired).
 *
 *  Reference: Budak & Petrela, "Frequency Limitations of
 *            Active Filters Using Operational Amplifiers,"
 *            IEEE Trans. CT, vol. CT-19, 1972
 *
 *  @param wp_ideal     Ideal pole frequency (rad/s)
 *  @param q_ideal      Ideal Q
 *  @param gbw          Op-amp gain-bandwidth product (Hz)
 *  @param topology     Filter topology
 *  @param wp_actual    Output: shifted pole frequency
 *  @param q_actual     Output: shifted Q (Q-enhancement)
 *  @return             ACTIVE_OK or ACTIVE_ERR_GBW_LIMIT
 *  Complexity: O(1) */
int gbw_effect_on_poles(double wp_ideal, double q_ideal, double gbw,
                          active_topology_t topology,
                          double *wp_actual, double *q_actual);

/** Compute maximum usable frequency for a given Q and GBW.
 *  L4: Inverse of GBW effect — given desired Q and op-amp GBW,
 *  what is the maximum pole frequency before Q-enhancement
 *  exceeds a specified limit?
 *
 *  f_max ≈ GBW / (2·α·Q·(Q_actual/Q_ideal - 1))
 *
 *  where α is a topology-dependent factor:
 *    Sallen-Key: α ≈ 3·Q (worst)
 *    MFB: α ≈ 2
 *    KHN: α ≈ 1 (best GBW immunity)
 *
 *  @param q_max      Maximum acceptable Q error (ratio, e.g., 1.1)
 *  @param gbw        Op-amp GBW (Hz)
 *  @param topology   Filter topology
 *  @return           Maximum usable pole frequency (Hz)
 *  Complexity: O(1) */
double gbw_max_frequency(double q_max, double gbw,
                          active_topology_t topology);

/** Compute slew rate limit for large-signal operation.
 *  L4: Slew rate SR limits the maximum frequency for a given
 *  output amplitude: f_max_SR = SR / (2π·Vpk)
 *
 *  For filters, the output of each stage must not exceed the
 *  slew rate limit at the highest frequency of interest.
 *  BP filters are most affected due to high Q peaking.
 *
 *  @param sr_v_us     Slew rate (V/μs)
 *  @param vpk         Peak output voltage (V)
 *  @return            Maximum frequency before slew limiting (Hz)
 *  Complexity: O(1) */
double slew_rate_max_frequency(double sr_v_us, double vpk);

/** Model DC offset propagation through cascaded filters.
 *  L4: Each op-amp contributes input offset voltage (Vos) and
 *  bias current (Ib) induced errors. In a cascade, DC offsets
 *  accumulate and are multiplied by DC gains of subsequent stages.
 *
 *  Vout_offset = Σ_k (Vos_k · G_DC_k + Ib_k · R_eq_k · G_DC_k)
 *
 *  @param stages        Array of DC gain per stage
 *  @param vos_uv        Array of offset voltages per stage (μV)
 *  @param ib_na         Array of bias currents per stage (nA)
 *  @param req_ohm       Array of equivalent resistances per stage (Ω)
 *  @param num_stages    Number of stages
 *  @return              Total output offset voltage (V)
 *  Complexity: O(N) */
double dc_offset_propagation(const double *stages, const double *vos_uv,
                              const double *ib_na, const double *req_ohm,
                              int num_stages);

/* ==================================================================
 * L8: Advanced Sensitivity — Multi-Parameter Optimization
 * ================================================================== */

/** Compute the multiparameter sensitivity for tuning optimization.
 *  L8: Uses the Fisher information-like sensitivity matrix to
 *  determine optimal tuning strategy — which components to trim
 *  for maximum ω₀/Q correction with minimum interaction.
 *
 *  @param comp      Nominal component values
 *  @param topology  Filter topology
 *  @param function  Filter function
 *  @param matrix    Output: 2×6 sensitivity Jacobian [ω₀-row, Q-row]
 *  @return          ACTIVE_OK
 *  Complexity: O(N_components) */
int sensitivity_jacobian(const active_component_values_t *comp,
                          active_topology_t topology,
                          active_filter_func_t function,
                          double matrix[2][6]);

#ifdef __cplusplus
}
#endif

#endif /* ACTIVE_FILTER_SENSITIVITY_H */
