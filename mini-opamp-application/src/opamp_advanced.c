/**
 * opamp_advanced.c - Advanced Op-Amp Topics Implementation
 *
 * Implements stability analysis, frequency compensation, noise optimization,
 * fully differential amplifiers, chopper stabilization, and nested Miller
 * compensation for three-stage amplifiers.
 *
 * Coverage: L1-L8 (Definitions through Advanced Topics)
 */

#include "opamp_advanced.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*============================================================================
 * L2: STABILITY ANALYSIS
 *============================================================================*/

/**
 * analyze_stability_margins - Full Bode-based stability analysis.
 *
 * Computes all stability metrics from the loop gain frequency response.
 *
 * Phase Margin (PM): Additional phase lag needed to reach -180 deg at f_t.
 *   PM = 180 + angle(L(j*2*pi*f_t))
 *
 * Gain Margin (GM): Factor by which the loop gain can increase before
 * instability at the phase crossover frequency.
 *   GM = -|L(j*2*pi*f_180)|  in dB
 *
 * Interpretation:
 *   PM > 60 deg: excellent transient response, no overshoot
 *   PM = 45 deg: good response, ~20% overshoot
 *   PM = 30 deg: significant ringing, ~40% overshoot
 *   PM =  0 deg: oscillatory (marginally stable)
 *   PM <  0 deg: unstable (oscillation grows)
 *
 * @param model   Op-amp pole-zero model
 * @param beta    Feedback factor (0 < beta <= 1)
 * @param margins  Output: stability metrics
 *
 * Reference: Bode (1945), Sedra & Smith S10.10
 */
void analyze_stability_margins(const OpAmpPoleZeroModel *model,
                                double beta, StabilityMargins *margins) {
    if (!model || !margins) return;
    memset(margins, 0, sizeof(StabilityMargins));

    if (beta <= 0.0 || beta > 1.0) return;

    /* Find unity-gain frequency f_t (where |L(jw)| = 1) */
    double f_t = find_unity_gain_frequency(model, beta, 1.0, 1e9, 1e-3);
    margins->unity_gain_freq = f_t;

    /* Phase margin: PM = 180 + phase at f_t */
    double phase_at_ft = opamp_multi_pole_gain_phase(model, f_t);
    /* beta is resistive, adds no phase */
    margins->phase_margin = 180.0 + phase_at_ft * 180.0 / M_PI;
    if (margins->phase_margin > 180.0) margins->phase_margin = 180.0;
    if (margins->phase_margin < -180.0) margins->phase_margin = -180.0;

    /* Find phase crossover frequency f_180 (where phase = -180 deg) */
    /* Search for the frequency where phase crosses -180 deg */
    double f_180 = 0.0;
    double prev_phase = 0.0;
    for (double f = 1.0; f < 1e9; f *= 1.01) {
        double phase = opamp_multi_pole_gain_phase(model, f) * 180.0 / M_PI;
        if (phase <= -180.0 && prev_phase > -180.0) {
            f_180 = f;
            break;
        }
        prev_phase = phase;
    }
    margins->phase_cross_freq = f_180;

    /* Gain margin: GM = -20*log10(|L(jw_180)|) */
    if (f_180 > 0.0) {
        double mag = opamp_multi_pole_gain_mag(model, f_180) * beta;
        margins->gain_margin = (mag > 0) ? -20.0 * log10(mag) : 100.0;
    } else {
        margins->gain_margin = 100.0;  /* No phase crossover = infinite GM */
    }

    /* Stability checks */
    margins->is_stable = (margins->phase_margin > 0.0 && margins->gain_margin > 0.0);
    margins->is_robust_stable = (margins->phase_margin > 45.0 && margins->gain_margin > 10.0);
}

/**
 * loop_gain_magnitude_phase - Compute loop gain Bode plot point.
 *
 * L(jw) = A_ol(jw) * beta
 * |L(jw)|_dB = |A_ol(jw)|_dB + 20*log10(beta)
 * phase(L) = phase(A_ol)  (beta is real for resistive feedback)
 *
 * @param model      Op-amp model
 * @param beta       Feedback factor
 * @param f          Frequency (Hz)
 * @param mag_db     Output: |L| (dB)
 * @param phase_deg  Output: phase (deg)
 */
void loop_gain_magnitude_phase(const OpAmpPoleZeroModel *model,
                                double beta, double f,
                                double *mag_db, double *phase_deg) {
    if (!model || !mag_db || !phase_deg) return;

    double A_mag = opamp_multi_pole_gain_mag(model, f);
    double A_phase = opamp_multi_pole_gain_phase(model, f);

    *mag_db = 20.0 * log10(A_mag * beta);
    *phase_deg = A_phase * 180.0 / M_PI;
}

/**
 * find_unity_gain_frequency - Find frequency where |L(jw)| = 1 (0 dB).
 *
 * Binary search between f_start and f_end.
 * The loop gain decreases monotonically with frequency for minimum-phase
 * systems with more poles than zeros (true for op-amps).
 *
 * @param model    Op-amp model
 * @param beta     Feedback factor
 * @param f_start  Search range start (Hz)
 * @param f_end    Search range end (Hz)
 * @param tol      Tolerance (fractional)
 * @return         Unity-gain frequency (Hz)
 */
double find_unity_gain_frequency(const OpAmpPoleZeroModel *model,
                                  double beta, double f_start,
                                  double f_end, double tol) {
    if (!model) return 1.0;
    if (f_start <= 0.0) f_start = 0.001 * model->f_poles[0];
    if (f_end <= f_start) f_end = model->A_ol * model->f_poles[0] * beta * 10.0;

    double f_low = f_start;
    double f_high = f_end;
    double f_mid = (f_low + f_high) / 2.0;

    for (int iter = 0; iter < 60; iter++) {
        f_mid = (f_low + f_high) / 2.0;
        double mag = opamp_multi_pole_gain_mag(model, f_mid) * beta;

        if (fabs(mag - 1.0) < tol) break;
        if (mag > 1.0) {
            f_low = f_mid;
        } else {
            f_high = f_mid;
        }

        if (f_high - f_low < f_mid * tol * 0.01) break;
    }

    return f_mid;
}

/*============================================================================
 * L3: NOISE ANALYSIS
 *============================================================================*/

/**
 * compute_noise_RTI - Total input-referred noise density.
 *
 * Complete noise model including:
 *   1. Op-amp voltage noise: e_n (V/sqrt(Hz))
 *      - White noise: constant above f_ce
 *      - 1/f (flicker) noise: e_n_flicker^2 * f_ce / f
 *   2. Op-amp current noise: i_n (A/sqrt(Hz))
 *      - Flows through equivalent resistance at input, creating voltage noise
 *      - e_ni_current = i_n * R_eq
 *   3. Johnson (thermal) noise: e_R = sqrt(4kTR) (V/sqrt(Hz))
 *      - 4kTR at T=300K: R=1k -> 4.07 nV/sqrt(Hz)
 *        R=10k -> 12.9 nV/sqrt(Hz), R=100k -> 40.7 nV/sqrt(Hz)
 *
 * All sources are uncorrelated and add in quadrature (RMS):
 *   e_ni_total = sqrt(e_n^2 + (i_n*R_eq)^2 + 4kT*R_eq)
 *
 * EQUIVALENT RESISTANCE R_eq seen from input:
 *   For non-inverting: R_eq = R_source + (R_f || R_g)
 *   For inverting:     R_eq = R_source || R_i + (R_f || (R_i+R_source))
 *   Simplified: R_eq = R_source + R_f||R_g (approximation)
 *
 * EXAMPLE (OPA627, ultra-low noise):
 *   e_n = 5.2 nV/sqrt(Hz), i_n = 1.6 fA/sqrt(Hz)
 *   R_source = 100 Ohm:
 *     Resistor noise: sqrt(4kT*100) = 1.28 nV/sqrt(Hz)
 *     Current noise: 1.6e-15 * 100 = 0.16 pV/sqrt(Hz) (negligible)
 *     Total: sqrt(5.2^2 + 1.28^2) = 5.35 nV/sqrt(Hz)
 *   => Op-amp voltage noise dominates for low R_source.
 *
 * EXAMPLE (LM741):
 *   e_n = 20 nV/sqrt(Hz), i_n = 0.5 pA/sqrt(Hz)
 *   R_source = 100k Ohm:
 *     Resistor noise: sqrt(4kT*100k) = 40.7 nV/sqrt(Hz)
 *     Current noise: 0.5e-12 * 100k = 50 nV/sqrt(Hz)
 *     Total: sqrt(20^2 + 40.7^2 + 50^2) = 67 nV/sqrt(Hz)
 *   => For high R_source, resistor and current noise dominate.
 *
 * @param noise     Op-amp noise model
 * @param R_source  Source resistance (Ohm)
 * @param R_f       Feedback resistor (Ohm)
 * @param R_g       Gain resistor (Ohm)
 * @param f         Frequency (Hz)
 * @return          Total input-referred noise (V/sqrt(Hz))
 *
 * Reference: TI SLVA043, Motchenbacher & Connelly Ch.5
 */
double compute_noise_RTI(const OpAmpNoiseModel *noise,
                          double R_source, double R_f, double R_g, double f) {
    if (!noise || f <= 0.0) return 0.0;

    const double k = 1.380649e-23;
    const double T = 300.0;

    /* Equivalent resistance at input */
    double R_par = (R_f * R_g) / (R_f + R_g);
    if (R_f <= 0.0 || R_g <= 0.0) R_par = 0.0;
    double R_eq = R_source + R_par;

    /* 1. Op-amp voltage noise (white + 1/f) */
    double e_n_sq = noise->e_n_white * noise->e_n_white;
    /* Add 1/f flicker noise */
    if (noise->f_ce > 0.0 && f < noise->f_ce * 10.0) {
        double e_flicker_sq = noise->e_n_flicker_1Hz * noise->e_n_flicker_1Hz
                            * noise->f_ce / f;
        e_n_sq += e_flicker_sq;
    }

    /* 2. Current noise contribution */
    double i_n_sq = noise->i_n_white * noise->i_n_white;
    /* 1/f for current noise */
    if (noise->f_ci > 0.0 && f < noise->f_ci * 10.0) {
        /* Simplified: flicker current noise */
        i_n_sq *= (1.0 + noise->f_ci / f);
    }

    /* 3. Johnson noise from R_eq */
    double e_R_sq = 4.0 * k * T * R_eq;

    /* Total input-referred noise density */
    double e_total_sq = e_n_sq + i_n_sq * R_eq * R_eq + e_R_sq;

    return sqrt(e_total_sq);
}

/**
 * compute_noise_figure - Noise figure of op-amp circuit.
 *
 * NF quantifies SNR degradation:
 *   NF = 10*log10(1 + (e_ni_amp^2 + e_ni_current^2) / (4kTR_s))
 *
 * where e_ni_amp = op-amp voltage noise referred to input,
 * and the denominator is the source's Johnson noise power.
 *
 * For a perfect (noiseless) amplifier: NF = 0 dB.
 * Typical op-amp circuits: NF = 3-20 dB depending on R_source.
 *
 * LOW NOISE DESIGN RULES:
 *   1. Match R_source to e_n/i_n for minimum NF
 *   2. Use low-value resistors (< 10k) to minimize Johnson noise
 *   3. Place gain early in the signal chain to overcome later noise
 *   4. For photodiode amplifiers: use FET-input op-amp (low i_n)
 *   5. For low-Z sensors: use BJT-input op-amp (low e_n)
 *
 * @param noise     Op-amp noise model
 * @param R_source  Source resistance (Ohm)
 * @param f         Frequency (Hz)
 * @return          Noise figure (dB)
 */
double compute_noise_figure(const OpAmpNoiseModel *noise,
                             double R_source, double f) {
    if (!noise || R_source <= 0.0) return 0.0;

    const double k = 1.380649e-23;
    const double T = 300.0;

    double e_n_sq = noise->e_n_white * noise->e_n_white;
    if (noise->f_ce > 0.0 && f < noise->f_ce) {
        e_n_sq += noise->e_n_flicker_1Hz * noise->e_n_flicker_1Hz
                * noise->f_ce / f;
    }

    double i_n_sq = noise->i_n_white * noise->i_n_white;
    double e_amp_sq = e_n_sq + i_n_sq * R_source * R_source;
    double e_source_sq = 4.0 * k * T * R_source;

    return 10.0 * log10(1.0 + e_amp_sq / e_source_sq);
}

/*============================================================================
 * L4: NYQUIST STABILITY CRITERION
 *============================================================================*/

/**
 * nyquist_stability_check - Check stability via Nyquist criterion.
 *
 * Nyquist criterion (simplified for minimum-phase systems):
 *   The closed-loop system is stable if and only if the Nyquist plot
 *   of L(jw) does not encircle the point (-1, j0).
 *
 * For well-behaved op-amps (minimum phase, gain decreases monotonically):
 *   The system is stable if |L(jw)| < 1 (0 dB) when phase(L) = -180 deg.
 *
 * Implementation: search for the phase crossover frequency and check
 * the loop gain magnitude there.
 *
 * @param model    Op-amp model
 * @param beta     Feedback factor
 * @return         1 if stable, 0 if unstable
 *
 * Reference: Nyquist "Regeneration Theory" BSTJ (1932)
 */
int nyquist_stability_check(const OpAmpPoleZeroModel *model, double beta) {
    if (!model || beta <= 0.0) return 0;

    StabilityMargins margins;
    analyze_stability_margins(model, beta, &margins);
    return margins.is_stable;
}

/*============================================================================
 * L5: COMPENSATION DESIGN
 *============================================================================*/

/**
 * design_miller_compensation - Design Miller compensation for two-stage amp.
 *
 * Miller compensation places a capacitor C_c between the input and output
 * of the high-gain second stage. The Miller effect multiplies the effective
 * capacitance by (1 + A2), allowing a physically small capacitor to create
 * a low-frequency dominant pole.
 *
 * POLE SPLITTING:
 *   Without compensation: p1 = 1/(R1*C1), p2 = 1/(R2*C2)
 *   With Miller C_c:
 *     p1_new = 1/(R1*(C1 + C_c*(1+A2)))  -> moves DOWN (dominant)
 *     p2_new = g_m2/(C_L + C_c)           -> moves UP (non-dominant)
 *
 * This separation (pole splitting) improves stability.
 *
 * DESIGN: Choose C_c such that f_t = g_m1/(2*pi*C_c)
 *   and the non-dominant pole p2 > 3*f_t for PM > 70 deg.
 *
 * RHP ZERO: C_c also creates a right-half-plane zero at f_z = g_m2/(2*pi*C_c).
 *   This zero degrades phase margin. Solution: add R_z = 1/g_m2 in series
 *   with C_c to push the zero to infinity (nulling resistor).
 *
 * @param pm_uncompensated  Phase margin before compensation (deg)
 * @param target_PM         Desired phase margin (deg)
 * @param GBW               Gain-bandwidth product (Hz)
 * @param g_m2              Second-stage transconductance (A/V)
 * @param comp              Output: compensation network
 * @return                  1 on success
 *
 * Reference: Gray & Meyer S9.4.2, Razavi S10.5
 */
int design_miller_compensation(double pm_uncompensated, double target_PM,
                                double GBW, double g_m2,
                                CompensationNetwork *comp) {
    if (!comp) return 0;
    if (pm_uncompensated >= target_PM) {
        /* Already stable, no compensation needed */
        comp->C_c = 0.0;
        comp->R_z = 0.0;
        comp->comp_type = 1;  /* Miller */
        comp->new_PM = pm_uncompensated;
        return 1;
    }

    /* Required phase boost at unity gain */
    double phi_boost = target_PM - pm_uncompensated;
    if (phi_boost < 0.0) phi_boost = 0.0;

    /* Choose C_c to set dominant pole */
    /* f_t = g_m1/(2*pi*C_c) => C_c = g_m1/(2*pi*f_t) */
    /* Using GBW = f_t (approx), g_m1 = 2*pi*GBW*C_c */
    /* Assume g_m1 such that GBW/g_m1 = 2*pi*C_c */
    /* Simplified: C_c sets the new unity-gain frequency */
    double C_c = g_m2 / (2.0 * M_PI * GBW * 10.0);  /* Heuristic */
    if (C_c < 1.0e-15) C_c = 1.0e-12;  /* 1 pF min */
    if (C_c > 1.0e-6) C_c = 1.0e-6;    /* 1 uF max */

    comp->C_c = C_c;
    comp->comp_type = 1;  /* Miller */
    comp->R_c = 0.0;

    /* Nulling resistor to cancel RHP zero */
    comp->R_z = 1.0 / g_m2;
    comp->new_f_p1 = 1.0 / (2.0 * M_PI * C_c * (1.0 / g_m2));
    comp->new_PM = pm_uncompensated + 30.0;  /* Estimate */

    return 1;
}

/**
 * design_lead_compensation - Design lead network for improved PM.
 *
 * Lead compensation adds phase lead at the unity-gain frequency
 * by introducing a zero below f_t and a pole above f_t.
 *
 * Transfer function: D(s) = (1 + s/w_z) / (1 + s/w_p)
 *   where w_z < w_t < w_p
 *
 * Maximum phase lead at frequency w_m = sqrt(w_z * w_p):
 *   phi_max = arcsin((alpha-1)/(alpha+1)) where alpha = w_p/w_z
 *
 * Design steps:
 *   1. Find required phase boost phi_m
 *   2. Compute alpha = (1+sin(phi_m))/(1-sin(phi_m))
 *   3. Place w_m at current unity-gain frequency
 *   4. w_z = w_m/sqrt(alpha), w_p = w_m*sqrt(alpha)
 *   5. Determine R and C values from w_z and w_p
 *
 * @param f_unity     Unity-gain frequency (Hz)
 * @param PM_before   Phase margin before compensation (deg)
 * @param PM_target   Target phase margin (deg)
 * @param comp        Output: compensation network
 * @return            1 on success
 */
int design_lead_compensation(double f_unity, double PM_before,
                              double PM_target, CompensationNetwork *comp) {
    if (!comp) return 0;

    double phi_required = PM_target - PM_before + 5.0;  /* +5 deg margin */
    if (phi_required <= 0.0) {
        comp->C_c = 0.0;
        comp->R_c = 0.0;
        return 1;
    }
    if (phi_required > 65.0) phi_required = 65.0;  /* Max single-stage lead */

    double phi_rad = phi_required * M_PI / 180.0;
    double alpha = (1.0 + sin(phi_rad)) / (1.0 - sin(phi_rad));
    double w_m = 2.0 * M_PI * f_unity;
    double w_z = w_m / sqrt(alpha);
    double w_p = w_m * sqrt(alpha);

    /* Choose C = 10 nF, compute R1, R2 */
    double C = 10.0e-9;
    comp->C_c = C;
    comp->R_c = 1.0 / (w_z * C);   /* R1 */
    comp->R_z = 1.0 / (w_p * C);   /* R2 */
    comp->comp_type = 2;  /* Lead */
    comp->new_PM = PM_before + phi_required;

    return 1;
}

/*============================================================================
 * L6: COMPENSATED AMPLIFIER RESPONSE
 *============================================================================*/

/**
 * compensated_amplifier_closed_loop_response - Response after compensation.
 *
 * Evaluates the closed-loop frequency response of the compensated
 * amplifier, accounting for the compensation network.
 *
 * @param cfg        Amplifier config
 * @param opamp      Op-amp parameters
 * @param comp       Compensation network
 * @param f          Frequency (Hz)
 * @param mag_db     Output: closed-loop gain (dB)
 * @param phase_deg  Output: closed-loop phase (deg)
 */
void compensated_amplifier_closed_loop_response(const AmplifierConfig *cfg,
                                                  const OpAmpParams *opamp,
                                                  const CompensationNetwork *comp,
                                                  double f,
                                                  double *mag_db,
                                                  double *phase_deg) {
    if (!cfg || !opamp || !comp || !mag_db || !phase_deg) return;

    /* Simplified: base closed-loop BW on compensated parameters */
    double A_cl = opamp_cfg_closed_loop_gain(cfg, opamp);
    double f_p1_comp = comp->new_f_p1;
    if (f_p1_comp <= 0.0) f_p1_comp = opamp->f_p1;  /* No comp = use original */

    /* Single-pole closed-loop model */
    double A_cl_mag = fabs(A_cl);
    double f_3dB = opamp->GBW / opamp_cfg_noise_gain(cfg);

    if (f <= 0.0) {
        *mag_db = 20.0 * log10(A_cl_mag);
        *phase_deg = 0.0;
    } else {
        double mag = A_cl_mag / sqrt(1.0 + (f / f_3dB) * (f / f_3dB));
        *mag_db = 20.0 * log10(mag);
        *phase_deg = -atan(f / f_3dB) * 180.0 / M_PI;
    }
}

/**
 * fully_diff_amp_common_mode_analysis - Analyze CMFB stability.
 *
 * In a fully differential amplifier, the common-mode feedback (CMFB)
 * loop must be stable independently of the differential-mode loop.
 *
 * The CMFB loop typically has lower bandwidth than the differential
 * loop because it senses common-mode through a resistive divider
 * and has additional phase shift.
 *
 * CM gain: A_cm(s) = A_ol_cm(s) * beta_cm
 * where beta_cm is the CMFB feedback factor.
 *
 * @param fda        Fully differential amplifier config
 * @param model      Op-amp pole-zero model
 * @param margins    Output: CM loop stability margins
 * @return           1 if CMFB stable, 0 if unstable
 */
int fully_diff_amp_common_mode_analysis(const FullyDiffAmp *fda,
                                         const OpAmpPoleZeroModel *model,
                                         StabilityMargins *margins) {
    if (!fda || !model || !margins) return 0;

    /* CMFB feedback factor from resistive divider */
    double beta_cm = 0.0;
    if (fda->R_cm > 0.0) {
        /* CM sense uses R_cm from each output to V_cm_ref */
        beta_cm = 0.5;  /* Typical: two equal R_cm to a summing node */
    }

    if (beta_cm <= 0.0) {
        /* No CMFB: not stable by definition */
        margins->is_stable = 0;
        return 0;
    }

    analyze_stability_margins(model, beta_cm, margins);
    return margins->is_stable;
}

/*============================================================================
 * L7: APPLICATIONS - Low-Noise Preamplifier
 *============================================================================*/

/**
 * low_noise_preamplifier_design - Design a low-noise preamplifier stage.
 *
 * The goal is to minimize the noise figure for a given source impedance.
 *
 * DESIGN METHODOLOGY:
 *   1. Characterize the source: what is R_source? What is the signal level?
 *   2. Choose op-amp based on R_source:
 *      - R_s < 1k: use BJT-input op-amp (low e_n, moderate i_n)
 *        Example: AD797 (e_n=0.9 nV/sqrt(Hz), i_n=2 pA/sqrt(Hz))
 *      - 1k < R_s < 100k: use either
 *        Example: OPA627 (e_n=5.2 nV/sqrt(Hz), i_n=1.6 fA/sqrt(Hz))
 *      - R_s > 100k: use FET-input op-amp (low i_n)
 *        Example: LMC6001 (i_n=0.13 fA/sqrt(Hz))
 *   3. Set gain to overcome later stage noise
 *   4. Choose feedback resistors to minimize their noise contribution
 *      - R_f < R_source * 10 to keep their noise below source noise
 *   5. Optimize R_s for minimum NF: R_s_opt = e_n / i_n
 *
 * APPLICATION EXAMPLES:
 *   - ECG preamplifier: R_s~50k (electrode), BW=0.05-150 Hz, gain=1000
 *     -> Use INA128 instrumentation amplifier (FET input)
 *   - Condenser microphone preamp: R_s~2k, BW=20-20kHz, gain=100
 *     -> Use NE5534 (BJT, low e_n, moderate R_s)
 *   - Photodiode TIA: R_s~infinity (current source), BW=100kHz, gain=1M
 *     -> Use OPA128 (FET, ultra-low i_n)
 *
 * @param noise             Op-amp noise model
 * @param target_gain       Desired voltage gain (V/V)
 * @param bandwidth         Signal bandwidth (Hz)
 * @param R_s_opt           Output: optimal source resistance (Ohm)
 * @param NF_min            Output: minimum achievable NF (dB)
 * @param recommended_R_f   Output: recommended feedback resistor (Ohm)
 * @return                  1 on success
 *
 * Reference: Motchenbacher & Connelly (1993), Ott "Noise Reduction Techniques"
 */
int low_noise_preamplifier_design(const OpAmpNoiseModel *noise,
                                   double target_gain, double bandwidth,
                                   double *R_s_opt, double *NF_min,
                                   double *recommended_R_f) {
    (void)bandwidth;  /* Reserved for future BW-dependent optimization */
    if (!noise || !R_s_opt || !NF_min || !recommended_R_f) return 0;

    /* Optimal source resistance for minimum noise figure */
    if (noise->i_n_white > 0.0) {
        *R_s_opt = noise->e_n_white / noise->i_n_white;
    } else {
        *R_s_opt = 10000.0;  /* Default */
    }

    /* Compute minimum noise figure at R_s_opt */
    const double k = 1.380649e-23;
    const double T = 300.0;
    double e_n_times_i_n = noise->e_n_white * noise->i_n_white;
    *NF_min = 10.0 * log10(1.0 + e_n_times_i_n / (2.0 * k * T));

    /* Recommended feedback resistor */
    /* R_f should be large enough to not load output but small enough
       to keep its Johnson noise below the op-amp noise */
    *recommended_R_f = *R_s_opt * target_gain;
    if (*recommended_R_f < 1000.0) *recommended_R_f = 1000.0;
    if (*recommended_R_f > 1.0e6) *recommended_R_f = 1.0e6;

    return 1;
}

/*============================================================================
 * L8: ADVANCED TOPICS - Chopper and NMC
 *============================================================================*/

/**
 * chopper_stabilized_amp_analysis - Analyze chopper amplifier performance.
 *
 * Chopper stabilization is a modulation technique that eliminates
 * DC offset and 1/f noise by translating the signal to a higher
 * frequency where these errors are negligible.
 *
 * OPERATING PRINCIPLE:
 *   1. Input signal is modulated (chopped) at f_chop to a carrier
 *   2. Signal + offset + 1/f noise are amplified together
 *   3. Output is demodulated (chopped again) to baseband
 *   4. Offset and 1/f noise are modulated to f_chop and filtered out
 *
 * LIMITATIONS:
 *   - Signal bandwidth must be << f_chop to avoid aliasing
 *   - Charge injection from chopper switches creates residual offset
 *   - Clock feedthrough can corrupt the output
 *
 * APPLICATIONS:
 *   - Precision DC measurements (sub-uV offset)
 *   - Strain gauge amplifiers
 *   - Thermocouple amplifiers
 *
 * EXAMPLE CHOPPER OP-AMPS:
 *   - ICL7650 (Intersil, 1980s): V_os < 5 uV, drift 0.05 uV/C
 *   - ADA4528 (ADI): V_os < 2.5 uV, 97 nV p-p noise (0.1-10 Hz)
 *   - MCP6V01 (Microchip): auto-zero, V_os < 2 uV
 *
 * @param noise       Op-amp noise model
 * @param f_chop      Chopping frequency (Hz)
 * @param f_sig       Maximum signal frequency (Hz)
 * @param V_os        Original offset voltage (V)
 * @param eff_offset  Output: effective residual offset (V)
 * @param eff_noise   Output: effective input noise density (V/sqrt(Hz))
 *
 * Reference: Enz & Temes "Circuit Techniques for Reducing Op-Amp
 *            Imperfections" Proc. IEEE 84(11), 1996
 */
void chopper_stabilized_amp_analysis(const OpAmpNoiseModel *noise,
                                      double f_chop, double f_sig, double V_os,
                                      double *eff_offset, double *eff_noise) {
    if (!noise || !eff_offset || !eff_noise) return;

    /* Residual offset: reduced by open-loop gain */
    if (f_chop > 0.0 && f_sig < f_chop / 10.0) {
        /* Chopping reduces offset dramatically */
        *eff_offset = V_os / 1.0e5;  /* Reduced by gain factor */
    } else {
        *eff_offset = V_os;
    }

    /* Residual noise: 1/f noise is eliminated, only white noise remains */
    if (f_chop > noise->f_ce) {
        *eff_noise = noise->e_n_white;  /* 1/f eliminated by chopping */
    } else {
        /* 1/f noise still present at signal band */
        *eff_noise = noise->e_n_white * (1.0 + noise->f_ce / f_chop);
    }

    if (*eff_offset < 0.0) *eff_offset = 0.0;
}

/**
 * nested_miller_compensation - Design NMC for three-stage amplifier.
 *
 * Nested Miller Compensation (NMC) is used for three-stage op-amps,
 * particularly in low-voltage designs where cascoding is difficult.
 *
 * THREE-STAGE AMPLIFIER:
 *   Stage 1: Differential pair (g_m1, high gain)
 *   Stage 2: Common-source amplifier (g_m2, moderate gain)
 *   Stage 3: Output buffer (g_m3, low output impedance)
 *
 * NMC CONFIGURATION:
 *   C_c1: from output (stage 3) to stage 1 output (nested)
 *   C_c2: from output to stage 2 output
 *
 * DESIGN EQUATIONS (Eschauzier & Huijsing, 1995):
 *   C_c1 = 4 * (g_m1/g_m3) * C_L  (inner nested capacitor)
 *   C_c2 = 2 * (g_m2/g_m3) * C_L  (outer capacitor)
 *
 * For g_m1=g_m2=g_m3 and C_L=10pF:
 *   C_c1 = 40 pF, C_c2 = 20 pF
 *
 * Phase margin with NMC:
 *   PM = 90 - arctan(GBW/p_ndp) (typically > 60 deg)
 *
 * @param g_m1    Stage 1 transconductance (A/V)
 * @param g_m2    Stage 2 transconductance (A/V)
 * @param g_m3    Stage 3 transconductance (A/V)
 * @param C_L     Load capacitance (F)
 * @param C_c1    Output: inner nested capacitor (F)
 * @param C_c2    Output: outer capacitor (F)
 * @param PM      Output: estimated phase margin (deg)
 * @return        1 on success
 *
 * Reference: Eschauzier & Huijsing "Frequency Compensation Techniques
 *            for Low-Power Operational Amplifiers" (1995)
 *            Leung & Mok "NMC in Low-Power CMOS Design" JSSC (2001)
 */
int nested_miller_compensation(double g_m1, double g_m2, double g_m3,
                                double C_L, double *C_c1, double *C_c2,
                                double *PM) {
    if (!C_c1 || !C_c2 || !PM) return 0;
    if (g_m1 <= 0.0 || g_m2 <= 0.0 || g_m3 <= 0.0 || C_L <= 0.0) return 0;

    /* NMC design equations */
    *C_c1 = 4.0 * (g_m1 / g_m3) * C_L;
    *C_c2 = 2.0 * (g_m2 / g_m3) * C_L;

    /* Estimate phase margin */
    /* f_t = g_m1 / (2*pi*C_c1) */
    double f_t = g_m1 / (2.0 * M_PI * (*C_c1));
    /* Non-dominant pole: p_ndp = g_m3 / (2*pi*C_L) */
    double f_ndp = g_m3 / (2.0 * M_PI * C_L);

    /* Phase margin estimate */
    *PM = 90.0 - atan(f_t / f_ndp) * 180.0 / M_PI;

    /* Clamp */
    if (*PM < 0.0) *PM = 0.0;
    if (*PM > 90.0) *PM = 90.0;

    return 1;
}
