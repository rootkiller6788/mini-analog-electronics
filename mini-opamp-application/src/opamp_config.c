/**
 * opamp_config.c - Amplifier Configuration Implementations
 *
 * Implements the analysis and design functions for all standard
 * operational amplifier configurations.
 *
 * Knowledge Coverage:
 *   L1: Configuration type management, parameter setting
 *   L2: Closed-loop gain, input/output impedance computation
 *   L3: Feedback factor, noise gain, bandwidth calculations
 *   L4: Resistor design, sensitivity analysis
 *   L5: Three-op-amp instrumentation amplifier design
 *   L6: DC operating point solver, load drive verification
 */

#include "opamp_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/*============================================================================
 * L2: CORE CONCEPTS - Configuration Transfer Functions
 *============================================================================*/

/**
 * opamp_cfg_closed_loop_gain - Compute closed-loop DC gain for configuration.
 *
 * The generalized feedback gain formula:
 *   A_cl = A_forward / (1 + A_ol * beta)
 *
 * where A_forward depends on the configuration and beta is the feedback factor.
 *
 * For different configurations:
 *   Inverting:      A_forward = -R_f/R_i * A_ol*beta/(1 + A_ol*beta)
 *                   where beta = R_i/(R_i + R_f)
 *   Non-inverting:  A_cl = (1 + R_f/R_g) * A_ol*beta/(1 + A_ol*beta)
 *                   where beta = R_g/(R_g + R_f)
 *   Voltage follower: A_cl = A_ol/(1 + A_ol)  (beta = 1)
 *   Summing (inv):  v_out = -R_f * sum(v_k/R_k)
 *
 * For very large A_ol >> 1/beta:
 *   Inverting: A_cl = -R_f/R_i (ideal)
 *   Non-inverting: A_cl = 1 + R_f/R_g (ideal)
 *   Follower: A_cl = 1 (ideal)
 *
 * @param cfg     Amplifier configuration
 * @param opamp   Op-amp parameters
 * @return        Closed-loop DC gain (V/V)
 *
 * Reference: Sedra & Smith S2.2-2.5, Black (1934)
 */
double opamp_cfg_closed_loop_gain(const AmplifierConfig *cfg,
                                   const OpAmpParams *opamp) {
    if (!cfg || !opamp) return 0.0;

    double beta = opamp_cfg_feedback_factor(cfg);
    double A_ol = opamp->A_ol;
    double loop_gain = A_ol * beta;
    double correction = loop_gain / (1.0 + loop_gain);

    switch (cfg->type) {
        case OPAMP_CFG_INVERTING: {
            if (cfg->R_i <= 0.0) return -A_ol * correction;
            double A_ideal = -cfg->R_f / cfg->R_i;
            return A_ideal * correction;
        }
        case OPAMP_CFG_NON_INVERTING: {
            if (cfg->R_g <= 0.0) return A_ol * correction;  /* Unity gain? No, open loop */
            double A_ideal = 1.0 + cfg->R_f / cfg->R_g;
            return A_ideal * correction;
        }
        case OPAMP_CFG_VOLTAGE_FOLLOWER:
            return 1.0 * correction;  /* Ideal gain = 1 */
        case OPAMP_CFG_DIFFERENTIAL: {
            if (cfg->R_i <= 0.0) return 0.0;
            double A_ideal = cfg->R_f / cfg->R_i;
            return A_ideal * correction;
        }
        case OPAMP_CFG_TRANSCONDUCTANCE:
            /* Voltage-to-current converter: I_out = v_in/R (Howland pump) */
            return correction;  /* Gain < 1 V/V for transconductance */
        case OPAMP_CFG_TRANSIMPEDANCE:
            /* Current-to-voltage converter: V_out = -I_in * R_f */
            if (cfg->R_f <= 0.0) return 0.0;
            return -cfg->R_f * correction;
        case OPAMP_CFG_SUMMING_INVERTING: {
            /* For summing, each input has gain -R_f/R_k */
            if (cfg->R_i <= 0.0) return 0.0;
            return -cfg->R_f / cfg->R_i * correction;
        }
        default:
            return correction;  /* Generic */
    }
}

/**
 * opamp_cfg_input_impedance - Compute amplifier input impedance.
 *
 * The input impedance depends strongly on the topology:
 *
 * INVERTING CONFIGURATION:
 *   Z_in = R_i  (the virtual ground at the inverting input makes
 *                the input impedance simply the input resistor)
 *   The op-amp's own input impedance is bootstrapped out of the equation
 *   because the inverting input is at virtual ground (v- = 0).
 *
 * NON-INVERTING CONFIGURATION:
 *   Z_in = R_in_cm || (R_in_diff * (1 + A_ol*beta))
 *   The differential input resistance is multiplied by the loop gain,
 *   a phenomenon called "bootstrapping" — the feedback makes both inputs
 *   nearly equal, so very little current flows through R_in_diff.
 *   Typical values: > 100 MOhm for JFET-input op-amps.
 *
 * VOLTAGE FOLLOWER:
 *   Z_in is highest here because beta = 1 (maximum bootstrapping).
 *   Z_in = R_in_cm || (R_in_diff * (1 + A_ol))
 *   For LM741: Z_in = 200M || (2M * 200k) = 200M || 400G = ~200 MOhm
 *
 * DIFFERENTIAL:
 *   Z_in(diff) = 2 * R_i  (each input sees R_i to virtual ground)
 *   Z_in(cm) = R_in_cm (limited by op-amp, not resistors)
 *
 * @param cfg     Amplifier configuration
 * @param opamp   Op-amp parameters
 * @return        Input impedance (Ohm)
 *
 * Reference: Franco S2.5, Sedra & Smith S10.10
 */
double opamp_cfg_input_impedance(const AmplifierConfig *cfg,
                                  const OpAmpParams *opamp) {
    if (!cfg || !opamp) return 0.0;

    switch (cfg->type) {
        case OPAMP_CFG_INVERTING:
        case OPAMP_CFG_SUMMING_INVERTING:
            return cfg->R_i;

        case OPAMP_CFG_NON_INVERTING: {
            double beta = opamp_cfg_feedback_factor(cfg);
            double Z_diff_boosted = opamp->R_in_diff * (1.0 + opamp->A_ol * beta);
            /* Parallel combination of boosted diff and common-mode */
            double Z_par = 1.0 / (1.0 / Z_diff_boosted + 1.0 / opamp->R_in_cm);
            return Z_par;
        }

        case OPAMP_CFG_VOLTAGE_FOLLOWER: {
            double Z_diff_boosted = opamp->R_in_diff * (1.0 + opamp->A_ol);
            double Z_par = 1.0 / (1.0 / Z_diff_boosted + 1.0 / opamp->R_in_cm);
            return Z_par;
        }

        case OPAMP_CFG_DIFFERENTIAL:
            /* Differential input impedance: 2 * R_i (both inputs) */
            return 2.0 * cfg->R_i;

        default:
            return opamp->R_in_diff;
    }
}

/**
 * opamp_cfg_output_impedance - Compute closed-loop output impedance.
 *
 * Negative feedback reduces the open-loop output impedance:
 *   Z_out_cl = Z_out_ol / (1 + A_ol * beta)
 *
 * This is one of the fundamental benefits of negative feedback:
 * it reduces output impedance, making the amplifier a better
 * voltage source (less voltage drop under load).
 *
 * For voltage follower (beta = 1):
 *   Z_out_cl = 75 / (1 + 200000) = 0.375 mOhm  (LM741, near-zero!)
 *
 * However, at high frequencies where A_ol drops, Z_out rises.
 * At f_t (A_ol = 1):
 *   Z_out_cl = Z_out_ol / 2 (still reduced, but less dramatically)
 *
 * @param cfg     Amplifier configuration
 * @param opamp   Op-amp parameters
 * @return        Closed-loop output impedance (Ohm)
 */
double opamp_cfg_output_impedance(const AmplifierConfig *cfg,
                                   const OpAmpParams *opamp) {
    if (!cfg || !opamp) return 0.0;
    double beta = opamp_cfg_feedback_factor(cfg);
    double loop_gain = opamp->A_ol * beta;
    return opamp->R_out / (1.0 + loop_gain);
}

/*============================================================================
 * L3: MATHEMATICAL STRUCTURES - Transfer Function Parameters
 *============================================================================*/

/**
 * opamp_cfg_feedback_factor - Compute feedback factor beta.
 *
 * Beta is the fraction of the output voltage fed back to the inverting
 * (or non-inverting, for positive feedback) input.
 *
 * Configuration-specific formulas:
 *   Non-inverting: beta = R_g / (R_g + R_f)
 *   Inverting:     beta = R_i / (R_i + R_f)
 *   Follower:      beta = 1.0
 *   Differential:  beta = R_i / (R_i + R_f)
 *   Integrator:    beta(s) = 1/(s*R_f*C_f + 1)  (at DC: beta = 1)
 *
 * Note: For the inverting amplifier, the feedback factor (beta) is
 * the same as for the non-inverting with the same resistor values.
 * However, the forward-path gain is different, which is why the
 * inverting amplifier has a different closed-loop gain expression.
 *
 * @param cfg    Amplifier configuration
 * @return       Feedback factor (dimensionless)
 */
double opamp_cfg_feedback_factor(const AmplifierConfig *cfg) {
    if (!cfg) return 0.0;
    switch (cfg->type) {
        case OPAMP_CFG_INVERTING:
        case OPAMP_CFG_SUMMING_INVERTING:
            if (cfg->R_i <= 0.0 || cfg->R_f <= 0.0) return 0.0;
            return cfg->R_i / (cfg->R_i + cfg->R_f);
        case OPAMP_CFG_NON_INVERTING:
            if (cfg->R_g <= 0.0 || cfg->R_f <= 0.0) return 0.0;
            return cfg->R_g / (cfg->R_g + cfg->R_f);
        case OPAMP_CFG_VOLTAGE_FOLLOWER:
            return 1.0;
        case OPAMP_CFG_DIFFERENTIAL:
            if (cfg->R_i <= 0.0 || cfg->R_f <= 0.0) return 0.0;
            return cfg->R_i / (cfg->R_i + cfg->R_f);
        case OPAMP_CFG_INTEGRATOR:
            /* At DC, integrator has beta = 1 (capacitor is open) */
            return 1.0;
        case OPAMP_CFG_DIFFERENTIATOR:
            /* At DC, differentiator has beta = 0 (capacitor is open at input) */
            return 0.0;
        default:
            return 0.0;
    }
}

/**
 * opamp_cfg_noise_gain - Compute noise gain of the configuration.
 *
 * Noise gain is a crucial concept often overlooked by beginners.
 *
 * Definition: NG = 1/beta = the gain from the non-inverting input
 * to the output with the inverting input grounded.
 *
 * For NON-INVERTING: Signal gain = Noise gain = 1 + R_f/R_g
 * For INVERTING:     Signal gain = -R_f/R_i
 *                    Noise gain   = 1 + R_f/R_i
 *
 * KEY INSIGHT: An inverting amplifier with |gain| = 1 has noise gain = 2.
 * This means it amplifies the op-amp's input noise by 2, even though
 * the signal gain is only -1. This has significant implications for
 * low-noise design: for the same |signal gain|, a non-inverting
 * configuration is always lower noise than an inverting one.
 *
 * For BANDWIDTH: The closed-loop bandwidth is always
 *   BW_cl = GBW / NG  (for dominant-pole compensated op-amps)
 *
 * This is because the feedback always sees the noise gain, not the
 * signal gain.
 *
 * @param cfg    Amplifier configuration
 * @return       Noise gain (V/V, dimensionless)
 */
double opamp_cfg_noise_gain(const AmplifierConfig *cfg) {
    if (!cfg) return 0.0;
    double beta = opamp_cfg_feedback_factor(cfg);
    if (beta <= 0.0) {
        /* For differentiator, noise gain is very high */
        if (cfg->type == OPAMP_CFG_DIFFERENTIATOR) return 1e9;
        return 1e9;
    }
    return 1.0 / beta;
}

/**
 * opamp_cfg_bandwidth - Compute closed-loop -3dB bandwidth.
 *
 * Uses the GBW product relationship:
 *   BW_cl = GBW / NG
 *
 * where NG is the noise gain (not the signal gain).
 * This is one of the most important design equations in op-amp circuits.
 *
 * Example: GBW = 1 MHz, design a non-inverting amplifier with gain = 100
 *   NG = 100, BW_cl = 1 MHz / 100 = 10 kHz
 *
 * Example: Same GBW, inverting amplifier with signal gain = -10
 *   NG = 1 + 10 = 11, BW_cl = 1 MHz / 11 = 90.9 kHz
 *   (The bandwidth is determined by NG = 11, not the signal gain = 10)
 *
 * @param cfg     Amplifier configuration
 * @param opamp   Op-amp parameters (GBW used)
 * @return        -3dB bandwidth (Hz)
 */
double opamp_cfg_bandwidth(const AmplifierConfig *cfg,
                            const OpAmpParams *opamp) {
    if (!cfg || !opamp) return 0.0;
    double NG = opamp_cfg_noise_gain(cfg);
    if (NG <= 0.0) return opamp->GBW;
    return opamp->GBW / NG;
}

/*============================================================================
 * L4: FUNDAMENTAL LAWS - Feedback Design
 *============================================================================*/

/**
 * opamp_cfg_design_resistors - Design practical resistor values.
 *
 * Given the target gain and optional total resistance constraint,
 * computes optimal resistor values.
 *
 * Design methodology:
 *   1. Choose R_i (or R_g) to set input impedance
 *   2. For inverting: Z_in = R_i, so choose R_i based on source impedance
 *      Rule: R_i >= 10 * R_source to avoid loading
 *   3. For non-inverting: Z_in is bootstrapped high, so R_g can be small
 *      Rule: R_g >= 100 Ohm to limit power consumption
 *   4. Compute R_f = |gain| * R_i (inverting) or R_f = (gain-1) * R_g
 *   5. Practical limits: 100 Ohm <= R <= 1 MOhm
 *      - Too small: excessive power, op-amp can't drive
 *      - Too large: noise, parasitic capacitance, bias current errors
 *   6. If total resistance constrained: minimize noise by balancing R values
 *
 * @param cfg      Configuration (type and constraint set)
 * @param opamp    Op-amp parameters
 * @param gain     Target gain magnitude (not dB)
 * @param total_R  Total resistance constraint (0 = no constraint)
 * @return         1 on success, 0 if infeasible
 */
int opamp_cfg_design_resistors(AmplifierConfig *cfg, const OpAmpParams *opamp,
                                double gain, double total_R) {
    if (!cfg || !opamp || gain <= 0.0) return 0;

    switch (cfg->type) {
        case OPAMP_CFG_INVERTING: {
            /* Choose R_i for Z_in = R_i; typical 1k-100k */
            if (total_R > 0.0) {
                /* R_i + R_f = total_R, R_f/R_i = gain */
                cfg->R_i = total_R / (1.0 + gain);
                cfg->R_f = total_R - cfg->R_i;
            } else {
                /* Default: Z_in = 10k Ohm */
                cfg->R_i = 10000.0;
                cfg->R_f = gain * cfg->R_i;
            }
            break;
        }
        case OPAMP_CFG_NON_INVERTING: {
            double G = gain;
            if (total_R > 0.0) {
                /* R_g + R_f = total_R, (R_g + R_f)/R_g = G => R_f/R_g = G-1 */
                cfg->R_g = total_R / G;
                cfg->R_f = total_R - cfg->R_g;
            } else {
                cfg->R_g = 1000.0;  /* 1k Ohm typical */
                cfg->R_f = (G - 1.0) * cfg->R_g;
            }
            break;
        }
        case OPAMP_CFG_DIFFERENTIAL: {
            /* All four resistors equal for unity gain, or R_f/R_i for gain */
            if (total_R > 0.0) {
                cfg->R_i = total_R / (2.0 * (1.0 + gain));
                cfg->R_f = gain * cfg->R_i;
            } else {
                cfg->R_i = 10000.0;
                cfg->R_f = gain * cfg->R_i;
            }
            break;
        }
        default:
            return 0;
    }

    /* Validate practical range */
    if (cfg->R_f < 100.0 || cfg->R_f > 1.0e6) return 0;

    return 1;
}

/**
 * opamp_cfg_sensitivity_analysis - Analyze gain sensitivity to resistor tolerance.
 *
 * Resistor tolerance directly affects closed-loop gain accuracy.
 * For non-inverting amplifier:
 *   dG/G = (R_f/(R_f+R_g)) * (dR_f/R_f) - (R_g/(R_f+R_g)) * (dR_g/R_g)
 *   For high gains (R_f >> R_g): dG/G ~ dR_f/R_f (feedback resistor dominates)
 *
 * For inverting amplifier:
 *   dG/G = dR_f/R_f - dR_i/R_i
 *   Both resistors affect gain equally.
 *
 * Worst-case analysis: assume all resistors at tolerance extremes.
 *
 * This analysis is critical for precision applications (e.g.,
 * instrumentation, measurement systems) where gain accuracy matters.
 *
 * @param cfg            Amplifier configuration
 * @param tolerance_pct  Resistor tolerance (%)
 * @param gain_min       Output: minimum gain (worst case)
 * @param gain_max       Output: maximum gain (worst case)
 */
void opamp_cfg_sensitivity_analysis(const AmplifierConfig *cfg,
                                     double tolerance_pct,
                                     double *gain_min, double *gain_max) {
    if (!cfg || !gain_min || !gain_max) return;

    double tol = tolerance_pct / 100.0;
    double G_nom = cfg->target_gain;

    switch (cfg->type) {
        case OPAMP_CFG_INVERTING: {
            /* G = -R_f/R_i, worst case: R_f high, R_i low */
            double G_max_mag = (cfg->R_f * (1.0 + tol)) / (cfg->R_i * (1.0 - tol));
            double G_min_mag = (cfg->R_f * (1.0 - tol)) / (cfg->R_i * (1.0 + tol));
            *gain_max = -G_min_mag;  /* More negative = larger magnitude */
            *gain_min = -G_max_mag;  /* Less negative = smaller magnitude */
            break;
        }
        case OPAMP_CFG_NON_INVERTING: {
            /* G = 1 + R_f/R_g */
            double G_max_val = 1.0 + (cfg->R_f * (1.0 + tol)) / (cfg->R_g * (1.0 - tol));
            double G_min_val = 1.0 + (cfg->R_f * (1.0 - tol)) / (cfg->R_g * (1.0 + tol));
            *gain_max = G_max_val;
            *gain_min = G_min_val;
            break;
        }
        case OPAMP_CFG_DIFFERENTIAL: {
            double G_max_val = (cfg->R_f * (1.0 + tol)) / (cfg->R_i * (1.0 - tol));
            double G_min_val = (cfg->R_f * (1.0 - tol)) / (cfg->R_i * (1.0 + tol));
            *gain_max = G_max_val;
            *gain_min = G_min_val;
            break;
        }
        default:
            *gain_min = G_nom * (1.0 - tol);
            *gain_max = G_nom * (1.0 + tol);
            break;
    }
}

/*============================================================================
 * L5: ALGORITHMS - Instrumentation Amplifier Design
 *============================================================================*/

/**
 * opamp_ia_design - Design a three-op-amp instrumentation amplifier.
 *
 * The classic three-op-amp instrumentation amplifier (developed by
 * Analog Devices in the 1970s) provides:
 *   - Very high input impedance (bootstrapped)
 *   - High CMRR (> 100 dB with matched resistors)
 *   - Adjustable gain via single resistor R_gain
 *
 * STAGE 1 (Input Buffers):
 *   Two non-inverting amplifiers with cross-coupled R_gain:
 *   V_out1+ = V_in+ * (1 + R_f1/R_gain) - V_in- * (R_f1/R_gain)
 *   V_out1- = V_in- * (1 + R_f1/R_gain) - V_in+ * (R_f1/R_gain)
 *   Differential gain G1 = 1 + 2*R_f1/R_gain
 *
 * STAGE 2 (Difference Amplifier):
 *   V_out = (V_out1+ - V_out1-) * (R_f2/R_i2)  (with R3 = R_i2 for balance)
 *   Gain G2 = R_f2/R_i2
 *
 * TOTAL GAIN: G = G1 * G2 = (1 + 2*R_f1/R_gain) * (R_f2/R_i2)
 *
 * @param config       IA configuration with targets
 * @param opamp        Op-amp parameters
 * @param actual_gain  Output: achieved gain
 * @param actual_CMRR  Output: estimated CMRR (dB)
 * @return             1 on success
 *
 * Reference: Analog Devices AD620/INA128 datasheets
 *            Kitchin & Counts "A Designer's Guide to Instrumentation Amplifiers"
 */
int opamp_ia_design(InstrumentationAmpConfig *config, const OpAmpParams *opamp,
                     double *actual_gain, double *actual_CMRR) {
    if (!config || !opamp) return 0;

    double G = config->target_gain;

    /* Distribute gain between stages */
    /* Stage 1 should provide most of the gain for best CMRR */
    double G1, G2;

    if (G <= 10.0) {
        /* Low gain: stage 2 does most of the work */
        G1 = 1.0 + 2.0 * sqrt(G);  /* Typical: G1 = 7 for G=10 */
        G2 = G / G1;
    } else if (G <= 100.0) {
        G1 = 1.0 + 0.2 * G;
        G2 = G / G1;
    } else {
        /* High gain: stage 1 dominates */
        G1 = sqrt(G) * 2.0;
        G2 = G / G1;
    }

    /* Choose R_gain, R_f1: G1 = 1 + 2*R_f1/R_gain */
    config->R_f1 = 10000.0;  /* 10k Ohm standard */
    config->R_gain = (2.0 * config->R_f1) / (G1 - 1.0);
    if (config->R_gain < 100.0) {
        config->R_f1 = config->R_gain * (G1 - 1.0) / 2.0;
        if (config->R_f1 < 100.0) config->R_f1 = 100.0;
    }

    /* Choose R_i2, R_f2: G2 = R_f2/R_i2 */
    config->R_i2 = 10000.0;  /* 10k Ohm */
    config->R_f2 = G2 * config->R_i2;
    config->R_3 = config->R_i2;  /* For CMRR matching */

    if (actual_gain) {
        G1 = 1.0 + 2.0 * config->R_f1 / config->R_gain;
        G2 = config->R_f2 / config->R_i2;
        *actual_gain = G1 * G2;
    }

    /* Estimate CMRR */
    if (actual_CMRR) {
        double mismatch = 0.001;  /* Assume 0.1% resistor matching */
        *actual_CMRR = opamp_ia_cmrr_estimate(config, opamp, mismatch);
    }

    return 1;
}

/**
 * opamp_ia_cmrr_estimate - Estimate CMRR of three-op-amp IA.
 *
 * The total CMRR of a three-op-amp IA is limited by:
 *   1. Stage 1 op-amp CMRR (typically > 100 dB, bootstrapped)
 *   2. Stage 2 resistor mismatch (dominant limit, unless laser-trimmed)
 *   3. Stage 2 op-amp CMRR
 *
 * For resistive mismatch epsilon in stage 2:
 *   CMRR_R (dB) = -20*log10(4 * epsilon / G1)
 *
 * Then combined with op-amp CMRR (opa_cmrr):
 *   1/CMRR_total = 1/CMRR_R + 1/(opa_cmrr * G1)  (combined in linear)
 *
 * Example: G1 = 10, epsilon = 0.001 (0.1% resistors)
 *   CMRR_R = -20*log10(4*0.001/10) = -20*log10(0.0004) = 68 dB
 *   With op-amp CMRR of 90 dB: CMRR_total = 67 dB
 *
 * This shows why resistor matching is CRITICAL for IA performance.
 * 0.01% resistors give CMRR_R = 88 dB.
 * Integrated IAs (AD620, INA128) use laser-trimmed resistors for > 100 dB CMRR.
 *
 * @param config    IA configuration
 * @param opamp     Op-amp parameters
 * @param mismatch  Resistor fractional mismatch (e.g., 0.001 for 0.1%)
 * @return          Estimated total CMRR (dB)
 */
double opamp_ia_cmrr_estimate(const InstrumentationAmpConfig *config,
                               const OpAmpParams *opamp, double mismatch) {
    if (!config || !opamp || mismatch <= 0.0) return 0.0;

    double G1 = 1.0 + 2.0 * config->R_f1 / config->R_gain;

    /* CMRR due to resistor mismatch in stage 2 */
    double CMRR_R = -20.0 * log10(4.0 * mismatch / G1);
    if (CMRR_R < 0.0) CMRR_R = 120.0;

    /* Combine with op-amp CMRR */
    double cmrr_r_linear = pow(10.0, CMRR_R / 20.0);
    double cmrr_opa_linear = pow(10.0, opamp->CMRR / 20.0);

    /* Stage 2 op-amp CMRR is effectively improved by G1 */
    double cmrr_opa_effective = cmrr_opa_linear * G1;

    /* Parallel combination */
    double cmrr_total_linear = 1.0 / (1.0 / cmrr_r_linear + 1.0 / cmrr_opa_effective);

    return 20.0 * log10(cmrr_total_linear);
}

/*============================================================================
 * L6: CANONICAL PROBLEMS - DC Operating Point and Load Analysis
 *============================================================================*/

/**
 * opamp_cfg_solve_dc_operating_point - Solve DC operating point via MNA.
 *
 * Implements a simplified MNA solver for a single-op-amp circuit.
 * The op-amp is modeled as a voltage-controlled voltage source (VCVS):
 *   v_out = A_ol * (v_plus - v_minus)
 *
 * The circuit is assumed to have the following topology:
 *   Node 0: Input (v_in)
 *   Node 1: Non-inverting input (v_plus)
 *   Node 2: Inverting input (v_minus)
 *   Node 3: Output (v_out)
 *   R_g: between node 0 (or ground) and node 2
 *   R_f: between node 2 and node 3
 *   R_load: between node 3 and ground
 *
 * The MNA equations are:
 *   For non-inverting:
 *     v_plus = v_in
 *     v_out = A_ol * (v_plus - v_minus)
 *     (v_minus - v_out)/R_f + v_minus/R_g = 0  (KCL at v_minus)
 *
 * Solving: v_out = v_in * (1 + R_f/R_g) * loop_gain/(1 + loop_gain)
 *
 * @param cfg      Amplifier configuration
 * @param opamp    Op-amp parameters
 * @param v_in     Input voltage (V)
 * @param v_nodes  Output: [v_plus, v_minus, v_out, v_load]
 * @param i_branch Output: [i_in, i_fb, i_load]
 * @return         1 if linear, 0 if saturated
 */
int opamp_cfg_solve_dc_operating_point(const AmplifierConfig *cfg,
                                        const OpAmpParams *opamp,
                                        double v_in,
                                        double v_nodes[4],
                                        double i_branch[3]) {
    if (!cfg || !opamp || !v_nodes || !i_branch) return 0;

    double A_ol = opamp->A_ol;
    double v_plus, v_minus, v_out;

    switch (cfg->type) {
        case OPAMP_CFG_NON_INVERTING: {
            v_plus = v_in;
            /* Solve iteratively: v_out = A*(v_in - v_minus), v_minus = v_out*R_g/(R_g+R_f) */
            double R_f = cfg->R_f;
            double R_g = cfg->R_g;
            if (R_f <= 0.0 || R_g <= 0.0) return 0;
            double beta = R_g / (R_g + R_f);
            double loop = A_ol * beta;
            v_out = v_in * (1.0 / beta) * loop / (1.0 + loop);
            v_minus = v_out * beta;
            break;
        }
        case OPAMP_CFG_INVERTING: {
            v_plus = 0.0;
            double R_f = cfg->R_f;
            double R_i = cfg->R_i;
            if (R_f <= 0.0 || R_i <= 0.0) return 0;
            double beta = R_i / (R_i + R_f);
            double loop = A_ol * beta;
            v_out = -v_in * (R_f / R_i) * loop / (1.0 + loop);
            v_minus = (v_in * R_f + 0.0 * R_i) / (R_i + R_f);
            v_minus = v_out * beta + v_in * (1.0 - beta);
            break;
        }
        default:
            return 0;
    }

    /* Check saturation */
    int linear = opamp_check_linear_region(v_out, opamp->V_oh, opamp->V_ol, 0.5);
    if (!linear) {
        if (v_out > opamp->V_oh) v_out = opamp->V_oh;
        if (v_out < opamp->V_ol) v_out = opamp->V_ol;
    }

    v_nodes[0] = v_plus;
    v_nodes[1] = v_minus;
    v_nodes[2] = v_out;
    v_nodes[3] = v_out * cfg->R_load / (cfg->R_load + opamp->R_out);

    /* Branch currents */
    i_branch[0] = 0.0;  /* Input current (op-amp inputs are high-Z) */
    if (cfg->R_f > 0.0)
        i_branch[1] = (v_out - v_minus) / cfg->R_f;  /* Feedback current */
    else
        i_branch[1] = 0.0;

    if (cfg->R_load > 0.0)
        i_branch[2] = v_out / cfg->R_load;  /* Load current */
    else
        i_branch[2] = 0.0;

    return linear;
}

/**
 * opamp_cfg_load_drive_check - Verify op-amp can drive the specified load.
 *
 * Checks three conditions:
 *   1. Output current < I_sc (short-circuit protection limit)
 *   2. Output voltage within saturation limits
 *   3. Load resistance is not too low
 *
 * For the LM741: I_sc = 25 mA, so R_load_min = V_oh/I_sc = 13/0.025 = 520 Ohm.
 * For modern op-amps (e.g., OPA551): I_sc = 200 mA, R_load_min = 65 Ohm.
 *
 * @param cfg      Amplifier configuration
 * @param opamp    Op-amp parameters
 * @param v_out    Required output voltage (V)
 * @return         1 if load can be driven, 0 if insufficient drive
 */
int opamp_cfg_load_drive_check(const AmplifierConfig *cfg,
                                const OpAmpParams *opamp,
                                double v_out) {
    if (!cfg || !opamp) return 0;

    /* Check 1: Output current */
    double i_load = 0.0;
    if (cfg->R_load > 0.0) {
        i_load = v_out / cfg->R_load;
        /* Add feedback network current */
        double i_fb = 0.0;
        if (cfg->R_f + (cfg->type == OPAMP_CFG_INVERTING ? cfg->R_i : cfg->R_g) > 0.0) {
            double R_total = cfg->R_f + (cfg->type == OPAMP_CFG_INVERTING ?
                                          cfg->R_i : cfg->R_g);
            if (R_total > 0.0) i_fb = v_out / R_total;
        }
        double i_total = fabs(i_load) + fabs(i_fb);
        if (i_total > opamp->I_sc) return 0;
    }

    /* Check 2: Output voltage within range */
    if (!opamp_check_linear_region(v_out, opamp->V_oh, opamp->V_ol, 0.5))
        return 0;

    /* Check 3: Minimum load resistance */
    if (cfg->R_load > 0.0 && cfg->R_load < opamp->V_oh / opamp->I_sc)
        return 0;

    return 1;
}
