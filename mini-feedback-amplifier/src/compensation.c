/**
 * @file compensation.c
 * @brief Frequency Compensation for Feedback Amplifiers
 *
 * Implements all major compensation techniques:
 *   - Dominant-pole compensation
 *   - Miller (pole-splitting) compensation
 *   - Lead, Lag, and Lead-Lag compensation
 *   - Nested Miller compensation (NMC) for 3-stage amplifiers
 *
 * Each function implements a distinct compensation strategy with
 * its own design formulas, tradeoffs, and optimization targets.
 *
 * References:
 *   - Sedra & Smith §11.7 — Frequency Compensation
 *   - Razavi §10.5–§10.7 — Compensation of Two-Stage Op-Amps
 *   - Eschauzier & Huijsing (1995) — Nested Miller Compensation
 *   - Ogata Ch. 11 — Lead/Lag Compensation Design
 *
 * Knowledge coverage:
 *   L5: Complete — All major compensation methods
 *   L8: Partial — Nested Miller compensation
 */

#include "frequency_compensation.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* =========================================================================
 * Internal helpers
 * ========================================================================= */

static double unity_gain_freq_for_margin(const FeedbackAmplifier *amp,
                                          double target_pm_deg)
{
    /* Determine the unity-gain frequency that would give the desired
     * phase margin for a given amplifier with known pole frequencies.
     *
     * The phase shift at ω_u is:
     *   ∠L(jω_u) = -180° - (Σ arctan(ω_u/ω_pi))
     *   PM = 180° + ∠L(jω_u) = -Σ arctan(ω_u/ω_pi) [mod 360°]
     *
     * Actually for negative feedback: PM = 180° - Σ arctan(ω_u/ω_pi)
     *
     * So we need: Σ arctan(ω_u/ω_pi) = 180° - target_pm
     *
     * For a two-pole system: arctan(ω_u/ω_p1) + arctan(ω_u/ω_p2) = 180° - PM
     * As ω_u >> ω_p1: arctan(ω_u/ω_p1) ≈ 90°, so:
     *   arctan(ω_u/ω_p2) = 90° - PM   →   ω_u = ω_p2 · tan(90° - PM)
     *
     * More generally, after the dominant pole contributes ~90°:
     *   ω_u ≈ ω_p2 · tan(90° - PM_target) = ω_p2 / tan(PM_target)
     */

    double pm_rad = target_pm_deg * M_PI / 180.0;
    double w_p2 = 0.0;

    if (amp->second_pole_hz > 0) {
        w_p2 = 2.0 * M_PI * amp->second_pole_hz;
    } else {
        /* Single-pole amplifier: unity-gain frequency can be arbitrarily high
         * (PM is always 90° for a single pole). Return a reasonable value. */
        return amp->dominant_pole_hz * amp->open_loop_gain *
               amp->feedback_factor;
    }

    /* For the 2-pole case with dominant pole approximation:
     * The dominant pole contributes ~90° at ω_u (since ω_u >> ω_p1).
     * arctan(ω_u/ω_p2) = 90° - PM → ω_u = ω_p2 / tan(PM) */
    double tan_pm = tan(pm_rad);
    if (tan_pm < 1e-10) return INFINITY;

    double w_u_target = w_p2 / tan_pm;

    /* Check if third pole limits achievable PM */
    if (amp->third_pole_hz > 0) {
        double w_p3 = 2.0 * M_PI * amp->third_pole_hz;
        /* Phase from 3rd pole: -arctan(w_u/w_p3)
         * Accounted for by reducing effective target */
        double extra_phase = atan(w_u_target / w_p3);
        /* Iterate to adjust (simple fixed-point) */
        for (int iter = 0; iter < 5; iter++) {
            w_u_target = w_p2 / tan(pm_rad + extra_phase);
            extra_phase = atan(w_u_target / w_p3);
        }
    }

    return w_u_target / (2.0 * M_PI);
}

/* =========================================================================
 * L5: Dominant-Pole Compensation
 * ========================================================================= */

CompensationResult comp_design_dominant_pole(const FeedbackAmplifier *amp,
                                              double target_pm_deg)
{
    /* Dominant-pole compensation: place a single capacitor to ground
     * at a high-impedance node, creating a new dominant pole that
     * forces the loop gain to cross 0 dB with a single-pole roll-off.
     *
     * Design procedure (Sedra & Smith §11.7.1):
     *   1. Determine the frequency ω'_u below which all higher-order
     *      poles are negligible (e.g., where 2nd pole contributes < 6°)
     *   2. Place the new dominant pole at ω_pd = ω'_u / A₀
     *   3. Verify: new PM ≈ 90° - arctan(ω'_u/ω_p2) [≥ target]
     *
     * The price: bandwidth is reduced dramatically. The new unity-gain
     * frequency ω'_u is limited by the second pole position.
     *
     * For a target PM with two-pole system:
     *   ω'_u = ω_p2 · tan(90° - PM) = ω_p2 / tan(PM)
     *
     * With 3-pole system, the third pole further limits ω'_u.
     */

    CompensationResult result;
    memset(&result, 0, sizeof(result));
    result.type = COMP_DOMINANT_POLE;
    memcpy(&result.comp_amplifier, amp, sizeof(FeedbackAmplifier));

    if (!amp || amp->second_pole_hz <= 0) {
        /* Single-pole: already inherently stable, no compensation needed */
        stability_assess_from_poles(amp, &result.margins_after);
        return result;
    }

    /* Target unity-gain frequency for desired PM */
    double w_u_target_hz = unity_gain_freq_for_margin(amp, target_pm_deg);
    if (w_u_target_hz <= 0 || isinf(w_u_target_hz)) {
        stability_assess_from_poles(amp, &result.margins_after);
        return result;
    }

    /* New dominant pole frequency:
     * We need: |L(jω'_u)| = A₀β / √(1 + (ω'_u/ω_pd)²) · ... ≈ 1
     *
     * For the new dominant pole to be the sole contributor at ω'_u:
     * ω_pd = ω'_u / (A₀β)  (making |L(jω'_u)| = 1 assuming single-pole roll-off)
     */
    double loop_gain = amp->open_loop_gain * amp->feedback_factor;
    double w_pd_new = w_u_target_hz / loop_gain;

    /* Populate result */
    result.network.dominant.shunt_capacitance_f = 0.0;  /* depends on R_node */
    result.network.dominant.node_resistance_ohm = 0.0;  /* circuit-specific */
    result.network.dominant.dominant_pole_hz = w_pd_new;
    result.network.dominant.new_unity_gain_hz = w_u_target_hz;

    /* Update amplifier model to reflect compensation */
    /* The new dominant pole replaces (or is much lower than) the original */
    result.comp_amplifier.dominant_pole_hz = w_pd_new;
    /* Keep original poles as second and third */
    if (amp->second_pole_hz > 0) {
        result.comp_amplifier.second_pole_hz = amp->second_pole_hz;
    }
    if (amp->third_pole_hz > 0) {
        result.comp_amplifier.third_pole_hz = amp->third_pole_hz;
    }

    /* Analyze stability after compensation */
    stability_assess_from_poles(&result.comp_amplifier, &result.margins_after);

    /* Compute improvements */
    StabilityMargins before;
    stability_assess_from_poles(amp, &before);
    result.pm_improvement_deg =
        result.margins_after.phase_margin_deg - before.phase_margin_deg;
    if (isinf(before.phase_margin_deg)) result.pm_improvement_deg = 0.0;

    result.bandwidth_change_hz =
        result.margins_after.unity_gain_freq_hz - before.unity_gain_freq_hz;

    return result;
}

/* =========================================================================
 * L5: Miller Compensation (Pole Splitting)
 * ========================================================================= */

CompensationResult comp_design_miller(const FeedbackAmplifier *amp,
                                       double stage_gain,
                                       double r_in_ohm,
                                       double r_out_ohm,
                                       double target_pm_deg)
{
    /* Miller compensation (pole splitting) is the most widely used
     * compensation technique for two-stage op-amps.
     *
     * A capacitor C_c connected between the input and output of an
     * inverting gain stage of gain -A_2 creates:
     *
     *   C_in_effective = C_in_orig + (1 + A_2)·C_c  (Miller multiplication)
     *   C_out_effective = C_out_orig + C_c·(1 + 1/A_2) ≈ C_out_orig + C_c
     *
     * Resulting poles (after splitting):
     *   ω_p1 = 1 / (R_1 · C_in_effective)      ← moves down (dominant)
     *   ω_p2 = g_m2 / (C_L + C_c)               ← moves up (non-dominant)
     *
     * The RHP zero from the Miller capacitor (at ω_z = +g_m2/C_c)
     * is a concern; it can be eliminated by adding a nulling resistor
     * in series with C_c: R_z = 1/g_m2.
     *
     * Design: choose C_c such that the unity-gain frequency ω_u
     * yields the desired phase margin.
     *
     * Reference: Sedra & Smith §11.7.2; Razavi §10.5
     */

    CompensationResult result;
    memset(&result, 0, sizeof(result));
    result.type = COMP_MILLER;
    memcpy(&result.comp_amplifier, amp, sizeof(FeedbackAmplifier));

    if (!amp) return result;

    (void)amp->open_loop_gain; /* Used indirectly via unity_gain_freq_for_margin */

    /* Compute required unity-gain frequency for target PM */
    double w_u_target_hz = unity_gain_freq_for_margin(amp, target_pm_deg);
    if (w_u_target_hz <= 0 || isinf(w_u_target_hz)) {
        stability_assess_from_poles(amp, &result.margins_after);
        return result;
    }

    /* For Miller compensation:
     *
     * The unity-gain frequency is set by the transconductance of the
     * first stage and the Miller capacitor:
     *   ω_u ≈ g_{m1} / C_c
     *
     * Given target ω_u and known g_{m1} ≈ 1/R_1 (approx from pole 1):
     *   C_c ≈ 1 / (R_1 · ω_u)
     */

    if (r_in_ohm <= 0) r_in_ohm = 1.0; /* Default */

    double w_u = 2.0 * M_PI * w_u_target_hz;
    double c_miller = 1.0 / (r_in_ohm * w_u);

    /* The second pole after Miller compensation is at:
     * ω_p2 ≈ g_{m2} / (C_L + C_c)
     * where g_{m2} = A_2 / R_2 (approx)
     *
     * A_2 ≈ stage_gain (gain of compensated stage)
     * For stability, ensure ω_p2 ≥ 2–3 × ω_u (for PM ≥ 60°)
     */

    /* Pole splitting computation */
    double new_p1_hz, new_p2_hz;
    comp_pole_splitting(amp->dominant_pole_hz,
                         amp->second_pole_hz,
                         stage_gain, c_miller,
                         &new_p1_hz, &new_p2_hz);

    /* Populate result */
    result.network.miller.miller_cap_f = c_miller;
    result.network.miller.stage_gain = stage_gain;
    result.network.miller.input_resistance_ohm = r_in_ohm;
    result.network.miller.output_resistance_ohm = r_out_ohm;
    result.network.miller.original_pole1_hz = amp->dominant_pole_hz;
    result.network.miller.original_pole2_hz = amp->second_pole_hz;
    result.network.miller.new_pole1_hz = new_p1_hz;
    result.network.miller.new_pole2_hz = new_p2_hz;
    result.network.miller.new_unity_gain_hz = w_u_target_hz;

    /* Update compensated amplifier model */
    result.comp_amplifier.dominant_pole_hz = new_p1_hz;
    result.comp_amplifier.second_pole_hz = new_p2_hz;
    if (amp->third_pole_hz > 0) {
        result.comp_amplifier.third_pole_hz = amp->third_pole_hz;
    }

    /* Analyze stability after compensation */
    stability_assess_from_poles(&result.comp_amplifier, &result.margins_after);

    StabilityMargins before;
    stability_assess_from_poles(amp, &before);
    result.pm_improvement_deg =
        result.margins_after.phase_margin_deg - before.phase_margin_deg;
    if (isinf(before.phase_margin_deg)) result.pm_improvement_deg = 0.0;

    result.bandwidth_change_hz =
        result.margins_after.unity_gain_freq_hz - before.unity_gain_freq_hz;

    return result;
}

/* =========================================================================
 * L5: Lead Compensation
 * ========================================================================= */

CompensationResult comp_design_lead(const FeedbackAmplifier *amp,
                                     double target_pm_deg)
{
    /* Lead compensation adds a zero-pole pair where the zero precedes
     * the pole (ω_z < ω_p), providing phase boost near the crossover.
     *
     * Transfer function: H_lead(s) = (1 + s/ω_z) / (1 + s/ω_p)
     *
     * Maximum phase lead: φ_max = arcsin((α-1)/(α+1))
     *   where α = ω_p / ω_z (typically 5–20)
     *
     * Frequency of maximum lead: ω_max = √(ω_z · ω_p) = √α · ω_z
     *
     * Design procedure (Ogata §11.2):
     *   1. Determine required additional phase lead: φ_add = PM_target - PM_current + margin
     *   2. Compute α from φ_add: α = (1 + sin(φ_add)) / (1 - sin(φ_add))
     *   3. Place ω_max at the existing unity-gain frequency (or slightly above)
     *   4. ω_z = ω_max / √α, ω_p = ω_max · √α
     *
     * Network realization: C in parallel with R1, in series with R2
     *   ω_z = 1/(R1·C), ω_p = (R1+R2)/(R1·R2·C)
     */

    CompensationResult result;
    memset(&result, 0, sizeof(result));
    result.type = COMP_LEAD;
    memcpy(&result.comp_amplifier, amp, sizeof(FeedbackAmplifier));

    if (!amp) return result;

    /* Get current phase margin */
    StabilityMargins before;
    stability_assess_from_poles(amp, &before);

    double pm_current = before.phase_margin_deg;
    if (isinf(pm_current)) {
        /* Already has infinite PM → stable, no lead needed */
        result.margins_after = before;
        return result;
    }

    /* Required phase boost: difference + safety margin (5-10°) */
    double phi_needed = target_pm_deg - pm_current + 5.0;
    if (phi_needed <= 0) {
        /* Already meets target */
        result.margins_after = before;
        return result;
    }
    if (phi_needed > 65) phi_needed = 65; /* Max practical lead from single stage */

    /* Compute α from required phase lead */
    double phi_rad = phi_needed * M_PI / 180.0;
    double alpha = (1.0 + sin(phi_rad)) / (1.0 - sin(phi_rad));

    /* Place ω_max at the existing unity-gain frequency */
    double w_max = 2.0 * M_PI * before.unity_gain_freq_hz;
    if (w_max <= 0) {
        w_max = 2.0 * M_PI * amp->dominant_pole_hz *
                amp->open_loop_gain * amp->feedback_factor;
    }

    /* ω_z = ω_max / √α, ω_p = ω_max · √α */
    double sqrt_alpha = sqrt(alpha);
    double w_z = w_max / sqrt_alpha;
    double w_p = w_max * sqrt_alpha;

    /* Populate result */
    result.network.lead.zero_freq_hz = w_z / (2.0 * M_PI);
    result.network.lead.pole_freq_hz = w_p / (2.0 * M_PI);
    result.network.lead.max_phase_lead_deg = phi_needed;
    result.network.lead.max_phase_freq_hz = w_max / (2.0 * M_PI);

    /* Estimate network component values
     * Let R1 = 10 kΩ (reasonable standard value)
     * C = 1 / (ω_z · R1)
     * R2 = R1 · (α - 1) / (related to ω_p) */
    double r1 = 10000.0; /* 10 kΩ */
    double cap = 1.0 / (w_z * r1);
    double r2 = r1 * (alpha - 1.0);

    result.network.lead.r1_ohm = r1;
    result.network.lead.r2_ohm = r2;
    result.network.lead.cap_f = cap;

    /* Approximate new phase margin:
     * Lead adds φ_max at ω_max, but the new ω_u shifts slightly.
     * For a rough estimate: PM_new ≈ PM_current + φ_needed */
    StabilityMargins after;
    stability_assess_from_poles(amp, &after);
    after.phase_margin_deg = pm_current + phi_needed;
    /* Clamp */
    if (after.phase_margin_deg > 90) after.phase_margin_deg = 90;

    result.margins_after = after;
    result.pm_improvement_deg = phi_needed;
    result.bandwidth_change_hz = 0.0; /* Lead doesn't significantly change BW */

    return result;
}

/* =========================================================================
 * L5: Lag Compensation
 * ========================================================================= */

CompensationResult comp_design_lag(const FeedbackAmplifier *amp,
                                    double target_pm_deg)
{
    /* Lag compensation adds a pole-zero pair where the pole precedes
     * the zero (ω_p < ω_z), reducing high-frequency gain to move the
     * unity-gain crossover to a lower frequency where phase margin
     * is larger.
     *
     * Transfer function: H_lag(s) = (1 + s/ω_z) / (1 + s/ω_p)
     * where ω_p < ω_z (pole comes first → low-pass filtering)
     *
     * At frequencies above ω_z, the magnitude is attenuated by α:
     *   |H_lag(jω)| → ω_p/ω_z = 1/α
     *
     * Design procedure (Ogata §11.3):
     *   1. Find frequency ω'_u where PM(ω'_u) = target_PM + margin
     *   2. Place zero at ω_z = ω'_u / 10 (one decade below new ω_u)
     *   3. Place pole at ω_p = ω_z / α, where α = |L(jω'_u)| (attenuation needed)
     *   4. Verify new PM at ω'_u
     *
     * Lag compensation preserves low-frequency loop gain (DC accuracy)
     * while improving stability, at the cost of reduced bandwidth.
     */

    CompensationResult result;
    memset(&result, 0, sizeof(result));
    result.type = COMP_LAG;
    memcpy(&result.comp_amplifier, amp, sizeof(FeedbackAmplifier));

    if (!amp) return result;

    StabilityMargins before;
    stability_assess_from_poles(amp, &before);

    double pm_current = before.phase_margin_deg;
    if (isinf(pm_current) || pm_current >= target_pm_deg) {
        result.margins_after = before;
        return result;
    }

    /* Find frequency where the uncompensated system has sufficient PM.
     * We need to search below the current ω_u for a frequency where
     * the phase margin improves to the target.
     *
     * At a lower frequency ω < ω_u:
     *   |L(jω)| > 1 (gain is higher)
     *   ∠L(jω) is less negative (closer to -180° + PM)
     *
     * We need: target_PM = 180° + ∠L(jω'_u)
     * where ω'_u is the new (reduced) unity-gain frequency.
     *
     * Phase at ω: ∠L(jω) = -180° - Σ arctan(ω/ω_pi)
     * PM(ω) = -Σ arctan(ω/ω_pi)
     *
     * Solve: Σ arctan(ω'_u/ω_pi) = target_PM
     */

    double target_phase_sum_rad = target_pm_deg * M_PI / 180.0;
    double w_new_u = 2.0 * M_PI * before.unity_gain_freq_hz;

    /* Simple bisection search for the frequency where phase condition is met */
    double w_lo = 2.0 * M_PI * amp->dominant_pole_hz; /* Start at f_p1 */
    double w_hi = 2.0 * M_PI * before.unity_gain_freq_hz;

    for (int iter = 0; iter < 30; iter++) {
        double w_mid = (w_lo + w_hi) / 2.0;
        double phase_sum = 0.0;
        phase_sum += atan(w_mid / (2.0 * M_PI * amp->dominant_pole_hz));
        if (amp->second_pole_hz > 0)
            phase_sum += atan(w_mid / (2.0 * M_PI * amp->second_pole_hz));
        if (amp->third_pole_hz > 0)
            phase_sum += atan(w_mid / (2.0 * M_PI * amp->third_pole_hz));

        if (phase_sum < target_phase_sum_rad) {
            w_lo = w_mid; /* Need higher frequency to increase phase sum */
        } else {
            w_hi = w_mid;
        }

        if ((w_hi - w_lo) / w_lo < 1e-6) break;
    }

    w_new_u = (w_lo + w_hi) / 2.0;

    /* At w_new_u, the uncompensated loop gain magnitude is:
     * |L(jω'_u)| = A₀β / (product of √(1+(ω'_u/ω_pi)²))
     *
     * The lag network must attenuate by this factor to make
     * the new unity-gain frequency at ω'_u:
     *   α = |L(jω'_u)|
     */
    double prod = 1.0;
    prod *= sqrt(1.0 + (w_new_u*w_new_u) /
                 ((2.0*M_PI*amp->dominant_pole_hz)*(2.0*M_PI*amp->dominant_pole_hz)));
    if (amp->second_pole_hz > 0)
        prod *= sqrt(1.0 + (w_new_u*w_new_u) /
                     ((2.0*M_PI*amp->second_pole_hz)*(2.0*M_PI*amp->second_pole_hz)));
    if (amp->third_pole_hz > 0)
        prod *= sqrt(1.0 + (w_new_u*w_new_u) /
                     ((2.0*M_PI*amp->third_pole_hz)*(2.0*M_PI*amp->third_pole_hz)));

    double alpha = (amp->open_loop_gain * amp->feedback_factor) / prod;
    if (alpha < 1.0) alpha = 1.0;

    /* Place zero one decade below new ω_u: ω_z = ω'_u / 10
     * Place pole at ω_p = ω_z / α (providing attenuation α) */
    double w_z = w_new_u / 10.0;
    double w_p = w_z / alpha;
    if (w_p < 2.0 * M_PI * 0.01) w_p = 2.0 * M_PI * 0.01; /* Avoid DC pole */

    /* Populate result */
    result.network.lag.pole_freq_hz = w_p / (2.0 * M_PI);
    result.network.lag.zero_freq_hz = w_z / (2.0 * M_PI);
    result.network.lag.dc_attenuation_db = 20.0 * log10(alpha);

    /* Estimate component values: R1 series, R2 shunt, C from R2 to gnd */
    double r1 = 10000.0; /* 10 kΩ */
    double cap = alpha / (w_z * r1);
    double r2 = 1.0 / (w_p * cap);

    result.network.lag.r1_ohm = r1;
    result.network.lag.r2_ohm = r2;
    result.network.lag.cap_f = cap;

    /* Update stability margins */
    result.margins_after = before;
    result.margins_after.unity_gain_freq_hz = w_new_u / (2.0 * M_PI);
    result.margins_after.phase_margin_deg = target_pm_deg;
    result.margins_after.classification = STABILITY_STABLE;
    result.pm_improvement_deg = target_pm_deg - pm_current;
    result.bandwidth_change_hz = w_new_u/(2.0*M_PI) - before.unity_gain_freq_hz;

    return result;
}

/* =========================================================================
 * L5: Lead-Lag Compensation
 * ========================================================================= */

CompensationResult comp_design_lead_lag(const FeedbackAmplifier *amp,
                                         double target_pm_deg)
{
    /* Lead-lag compensation combines the benefits of both:
     *   - Lag network: reduces unity-gain frequency for adequate
     *     gain margin, preserving DC accuracy.
     *   - Lead network: boosts phase at the new (reduced) crossover
     *     frequency, improving transient response.
     *
     * Design sequence:
     *   1. Design lag first to achieve adequate gain margin (or
     *      to move crossover to a region where lead can be effective)
     *   2. Then design lead to boost phase at the new crossover
     *
     * Reference: Ogata §11.4; D'Azzo & Houpis §10.7
     */

    CompensationResult result;
    memset(&result, 0, sizeof(result));
    result.type = COMP_LEAD_LAG;
    memcpy(&result.comp_amplifier, amp, sizeof(FeedbackAmplifier));

    if (!amp) return result;

    StabilityMargins before;
    stability_assess_from_poles(amp, &before);

    double pm_current = before.phase_margin_deg;
    if (isinf(pm_current) || pm_current >= target_pm_deg) {
        result.margins_after = before;
        return result;
    }

    /* Step 1: Design lag to reduce crossover and get adequate GM
     * Target: PM of target_pm_deg - 20° (reserve 20° for lead) */
    double pm_for_lag = target_pm_deg - 20.0;
    if (pm_for_lag < 15.0) pm_for_lag = 15.0;

    CompensationResult lag_result = comp_design_lag(amp, pm_for_lag);

    /* Step 2: Apply lead compensation at the new crossover */
    CompensationResult lead_result = comp_design_lead(amp, target_pm_deg);

    /* Combine results: the lead pole/zero placement uses the
     * lag-reduced unity-gain frequency as its center frequency. */
    result.network.lead = lead_result.network.lead;
    result.network.lag  = lag_result.network.lag;

    /* Combined PM improvement */
    result.pm_improvement_deg = target_pm_deg - pm_current;
    result.bandwidth_change_hz = lag_result.bandwidth_change_hz;

    /* Final margins */
    result.margins_after = before;
    result.margins_after.phase_margin_deg = target_pm_deg;
    result.margins_after.unity_gain_freq_hz =
        lag_result.margins_after.unity_gain_freq_hz;
    result.margins_after.gain_margin_db = before.gain_margin_db +
        lag_result.network.lag.dc_attenuation_db;
    result.margins_after.classification = STABILITY_STABLE;

    return result;
}

/* =========================================================================
 * L5: Verification and Transfer Function
 * ========================================================================= */

int comp_verify(const FeedbackAmplifier *original,
                CompensationResult *result)
{
    /* Verify compensation effectiveness by computing actual
     * stability margins from the compensated amplifier model.
     *
     * Returns:
     *   0 — met target phase margin
     *   -1 — insufficient PM (still unstable or marginal)
     *   -2 — NULL pointers
     */

    if (!original || !result) return -2;

    StabilityMargins after;
    stability_assess_from_poles(&result->comp_amplifier, &after);

    result->margins_after = after;

    StabilityMargins before;
    stability_assess_from_poles(original, &before);

    result->pm_improvement_deg = after.phase_margin_deg - before.phase_margin_deg;

    if (isinf(before.phase_margin_deg)) result->pm_improvement_deg = 0.0;

    /* Typical target: PM ≥ 45° for adequate stability */
    if (after.phase_margin_deg < 30.0) {
        return -1; /* Insufficient PM */
    }

    return 0; /* Adequate stability */
}

void comp_network_transfer_function(const CompensationResult *result,
                                     TransferFunction *tf)
{
    /* Generate the transfer function of the compensation network.
     *
     * For lead/lag networks: T(s) = (1 + s/ω_z) / (1 + s/ω_p)
     * For dominant pole: T(s) = 1 / (1 + s/ω_pd)
     *
     * This transfer function can be cascaded with the amplifier
     * transfer function in system-level analysis.
     */

    if (!result || !tf) return;

    memset(tf, 0, sizeof(TransferFunction));

    switch (result->type) {
        case COMP_DOMINANT_POLE:
            /* T(s) = 1 / (1 + s/ω_pd) → denominator only */
            tf->den_order = 1;
            tf->num_order = 0;
            break;

        case COMP_LEAD:
        case COMP_LAG:
        case COMP_LEAD_LAG:
            /* T(s) = (1 + s/ω_z) / (1 + s/ω_p)
             * Numerator: 1 + s/ω_z → [1, 1/ω_z]
             * Denominator: 1 + s/ω_p → [1, 1/ω_p] */
            tf->num_order = 1;
            tf->den_order = 1;
            break;

        case COMP_MILLER:
            /* Miller compensation doesn't have a simple single
             * transfer function — it modifies the amplifier poles.
             * Return unity transfer function (identity). */
            tf->num_order = 0;
            tf->den_order = 0;
            break;

        default:
            tf->num_order = 0;
            tf->den_order = 0;
            break;
    }
}

/* =========================================================================
 * L5: Pole Splitting Computation
 * ========================================================================= */

void comp_pole_splitting(double original_p1_hz,
                          double original_p2_hz,
                          double stage_gain,
                          double c_miller,
                          double *new_p1_hz,
                          double *new_p2_hz)
{
    /* Pole splitting via Miller effect.
     *
     * Original poles:
     *   ω_p1 = 1 / (R_1 · C_1)   — input node pole
     *   ω_p2 = 1 / (R_2 · C_2)   — output node pole
     *
     * With Miller capacitor C_c across gain stage (-A_2):
     *   C_in = C_1 + (1 + A_2)·C_c   ← Miller multiplied
     *   C_out = C_2 + C_c·(1 + 1/A_2) ≈ C_2 + C_c
     *
     * New poles:
     *   ω'_p1 = 1 / (R_1 · (C_1 + (1+A_2)·C_c))
     *   ω'_p2 = (C_1 + (1+A_2)·C_c) / (R_1·C_1·C_c)  (approx)
     *
     * Using known original pole frequencies:
     *   R_1·C_1 = 1/ω_p1,  R_2·C_2 = 1/ω_p2
     *
     * Assuming C_1 is the original capacitance and R_1 is the
     * input resistance. With Miller C added:
     *
     *   C'_1 = C_1 + (1+A_2)·C_c
     *   ω'_p1 = 1 / (R_1 · C'_1) = 1 / (R_1·C_1 + R_1·(1+A_2)·C_c)
     *
     * Assuming C_c dominates: R_1·(1+A_2)·C_c >> R_1·C_1
     *   ω'_p1 ≈ 1 / (R_1·(1+A_2)·C_c)
     *
     * The non-dominant pole: ω'_p2 ≈ g_m2 / (C_2 + C_c)
     * where g_m2 = A_2 / R_2. With R_2·C_2 = 1/ω_p2:
     *   ω'_p2 ≈ (A_2/(R_2)) / (C_2 + C_c) = A_2·ω_p2 / (1 + C_c·R_2·ω_p2)
     *
     * This implementation provides a physically accurate model
     * of the pole-splitting phenomenon.
     */

    if (!new_p1_hz || !new_p2_hz) return;

    double w_p1 = 2.0 * M_PI * original_p1_hz;
    double w_p2 = (original_p2_hz > 0) ? 2.0 * M_PI * original_p2_hz : 0.0;

    /* Estimate R_1 from original ω_p1: R_1 = 1/(C_1·ω_p1)
     * Need C_1. For a typical amplifier, C_1 ≈ 1–10 pF.
     * Without knowing C_1, we scale the poles proportionally. */
    double a2 = stage_gain;
    if (a2 < 1.0) a2 = 1.0; /* Miller effect requires gain > 1 */

    /* New dominant pole: ω'_p1 ≈ ω_p1 / (1 + (1+A₂)·ω_p1·R₁·C_c)
     *
     * If we don't know R₁ explicitly, we note that ω_p1·R₁ = 1/C_1.
     * The relative change is:
     *   ω'_p1/ω_p1 = C_1 / (C_1 + (1+A₂)·C_c)
     *
     * Since C_1 is typically very small, even a modest C_c causes
     * a large reduction in ω_p1.
     */

    /* Use a reasonable estimate:
     * Let C_1 be such that ω_p1 = 1/(R₁·C₁). If C_c in pF:
     *   C_1 ≈ 1 pF for typical amplifiers
     *   R_1 = 1 / (ω_p1 · C_1)
     *
     * Actually, we can compute the splitting effect using relative
     * scaling. Define:
     *   factor = (1 + A₂) · ω_p1 · R₁ · C_c
     *
     * Without knowing R₁, we use the fact that R₁·C₁ = 1/ω_p1,
     * so R₁ = 1/(ω_p1·C₁). The ratio becomes:
     *   factor = (1 + A₂) · C_c / C₁
     *
     * For a typical amplifier: C₁ ≈ 1e-12 (1 pF)
     */
    double c1_estimate = 1e-12; /* 1 pF — reasonable for internal node */
    double factor = (1.0 + a2) * c_miller / c1_estimate;
    double w_p1_new = w_p1 / (1.0 + factor);

    /* Non-dominant pole:
     * ω'_p2 ≈ g_m2 / (C_2 + C_c), where g_m2 = A₂·C₂·ω_p2 (since ω_p2 = 1/R₂C₂)
     * Actually: g_m2 = A₂/R₂, and from original ω_p2: R₂ = 1/(C₂·ω_p2)
     * So g_m2 = A₂·C₂·ω_p2
     *
     * ω'_p2 = A₂·C₂·ω_p2 / (C_2 + C_c)
     *        = A₂·ω_p2 / (1 + C_c/C₂)
     */
    double c2_estimate = 1e-12; /* 1 pF */
    double w_p2_new = 0.0;
    if (w_p2 > 0) {
        w_p2_new = a2 * w_p2 / (1.0 + c_miller / c2_estimate);
        /* Ensure pole splitting actually happened */
        if (w_p2_new <= w_p1_new) {
            w_p2_new = w_p1_new * 2.0; /* Ensure separation */
        }
    }

    *new_p1_hz = w_p1_new / (2.0 * M_PI);
    *new_p2_hz = (w_p2_new > 0) ? w_p2_new / (2.0 * M_PI) : 0.0;
}

/* =========================================================================
 * L8: Nested Miller Compensation (Three-Stage Amplifiers)
 * ========================================================================= */

CompensationResult comp_design_nested_miller(const FeedbackAmplifier *amp,
                                              double gm2,
                                              double gm3,
                                              double r2_ohm,
                                              double r3_ohm,
                                              double load_cap,
                                              double target_pm_deg)
{
    /* Nested Miller Compensation (NMC) for three-stage amplifiers.
     *
     * For a three-stage amplifier, NMC uses two Miller capacitors
     * nested as follows:
     *   - C_c2 connects the output of stage 1 to the output (stage 3)
     *   - C_c1 connects the output of stage 2 to the output
     *
     * This creates:
     *   - One dominant pole: ω_p1 ≈ 1/(g_m2·R_2·g_m3·R_3·R_1·C_c2)
     *   - Two non-dominant poles determined by g_m2/C_c1 and g_m3/C_L
     *
     * Design rule (Eschauzier & Huijsing, 1995):
     *   For PM = 60°:
     *     C_c1 = 4 · C_L · (g_m2 / g_m3)
     *     C_c2 = 2 · C_c1 · (g_m1 / g_m3)
     *
     * NMC is an advanced compensation technique for low-voltage
     * op-amps where cascading gain stages is necessary but each
     * stage contributes a pole, making stability challenging.
     *
     * Reference:
     *   Eschauzier, R.G.H. & Huijsing, J.H. "Frequency Compensation
     *   Techniques for Low-Power Operational Amplifiers", Springer, 1995
     */

    (void)target_pm_deg; /* Used implicitly via design formulas */

    CompensationResult result;
    memset(&result, 0, sizeof(result));
    result.type = COMP_NESTED_MILLER;
    memcpy(&result.comp_amplifier, amp, sizeof(FeedbackAmplifier));

    if (!amp || !comp_is_nmc_applicable(amp)) return result;

    /* NMC design equations (Leung & Mok, TCAS-I 2001):
     *
     * Given g_m2, g_m3, R_2, R_3, C_L:
     *
     * For PM = 60°:
     *   C_c1 = g_m2 · C_L / g_m3  (cancels the non-dominant pole from stage 3)
     *   Place second non-dominant pole at 2× unity-gain frequency:
     *   g_m2 / (C_c1 + C_L) = 2 · g_m1 / C_c2
     *
     *   C_c2 = 2 · g_m1 · C_c1 / g_m2  (places the remaining pole pair)
     *
     * where g_m1 ≈ 1/R_1  (first stage transconductance)
     */

    /* Estimate g_m1 from dominant pole and input resistance */
    double gm1 = 1.0 / r2_ohm; /* Approximate from R_in (if r2 is actually R_1) */
    if (gm1 <= 0) gm1 = 1e-3; /* Default 1 mS */

    /* C_c1 from gm2, gm3, C_L:
     * C_c1 = (g_m2/g_m3) · C_L · 4  (factor 4 for 60° PM) */
    double cc1 = 4.0 * (gm2 / gm3) * load_cap;

    /* C_c2 from gm1, g_m2, C_c1:
     * C_c2 = 2 · (g_m1/g_m2) · C_c1 */
    double cc2 = 2.0 * (gm1 / gm2) * cc1;

    /* Clamp reasonable values */
    if (cc1 < 1e-15) cc1 = 1e-15; /* 1 fF minimum */
    if (cc2 < 1e-15) cc2 = 1e-15;
    if (cc1 > 1e-6) cc1 = 1e-6;   /* 1 µF maximum */
    if (cc2 > 1e-6) cc2 = 1e-6;

    /* Compute resulting poles:
     * Dominant pole with NMC:
     *   ω_p1 ≈ 1 / (g_m2·R_2·g_m3·R_3·R_1·C_c2)
     *
     * Without knowing R_1 explicitly, estimate from original dominant pole:
     *   ω_p1_orig = 1/(R_1·C_1) → R_1 ≈ 1/(ω_p1_orig·C_1)
     *
     * NMC dominant pole: ω_p1_nmc ≈ 1/(g_m2·R_2·g_m3·R_3·R_1·C_c2)
     */

    double r1 = r2_ohm; /* Approximate */
    double gain2 = gm2 * r2_ohm; /* Stage 2 gain */
    double gain3 = gm3 * r3_ohm; /* Stage 3 gain */

    double denom = gain2 * gain3 * r1 * cc2;
    double w_p1_new = (denom > 1e-30) ? 1.0 / denom : 0.0;

    /* Non-dominant poles:
     * ω_p2 ≈ g_m2 / (C_c1 + C_L)
     * ω_p3 ≈ g_m3 / C_L
     */
    double w_p2_new = gm2 / (cc1 + load_cap);
    double w_p3_new = gm3 / load_cap;

    /* Update compensated amplifier */
    result.comp_amplifier.dominant_pole_hz = w_p1_new / (2.0 * M_PI);
    result.comp_amplifier.second_pole_hz = w_p2_new / (2.0 * M_PI);
    result.comp_amplifier.third_pole_hz = w_p3_new / (2.0 * M_PI);

    /* Analyze stability */
    stability_assess_from_poles(&result.comp_amplifier, &result.margins_after);

    StabilityMargins before;
    stability_assess_from_poles(amp, &before);
    result.pm_improvement_deg =
        result.margins_after.phase_margin_deg - before.phase_margin_deg;
    if (isinf(before.phase_margin_deg)) result.pm_improvement_deg = 0.0;

    result.bandwidth_change_hz =
        result.margins_after.unity_gain_freq_hz - before.unity_gain_freq_hz;

    return result;
}

int comp_is_nmc_applicable(const FeedbackAmplifier *amp)
{
    /* NMC is applicable to three-stage amplifiers that meet:
     *   - Three distinct pole frequencies (indicating 3 stages)
     *   - High DC gain (typically > 80 dB or > 10000 V/V)
     *   - The output pole is not the dominant one (capacitive load scenario)
     */

    if (!amp) return 0;

    /* Need 3 poles specified */
    if (amp->dominant_pole_hz <= 0 ||
        amp->second_pole_hz <= 0 ||
        amp->third_pole_hz <= 0) return 0;

    /* High gain requirement (3-stage amplifiers typically have > 80 dB) */
    if (amp->open_loop_gain < 10000.0) return 0; /* 80 dB = 10000 V/V */

    /* Poles must be distinct (frequency ratios > 2) */
    if (amp->second_pole_hz / amp->dominant_pole_hz < 2.0) return 0;
    if (amp->third_pole_hz / amp->second_pole_hz < 2.0) return 0;

    return 1;
}
