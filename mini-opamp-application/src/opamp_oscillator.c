/**
 * opamp_oscillator.c - Sinusoidal Oscillator Implementations
 *
 * Implements Wien bridge, phase-shift, quadrature oscillator design,
 * Barkhausen criterion verification, frequency-selective network analysis,
 * and oscillator waveform generation.
 *
 * Coverage: L1-L6 (Definitions through Canonical Problems)
 */

#include "opamp_oscillator.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/*============================================================================
 * L3: FREQUENCY-SELECTIVE NETWORK TRANSFER FUNCTIONS
 *============================================================================*/

/**
 * wien_bridge_transfer - Wien bridge network transfer function.
 *
 * The Wien bridge is the frequency-selective positive feedback network
 * in the Wien bridge oscillator, invented by Max Wien in 1891 and
 * famously used by Bill Hewlett in the HP200A audio oscillator (1939),
 * the first product of Hewlett-Packard.
 *
 * NETWORK TOPOLOGY:
 *   Series branch: R + C (input to output)
 *   Shunt branch:  R || C (output to ground)
 *
 * TRANSFER FUNCTION:
 *   beta(s) = Z_shunt / (Z_series + Z_shunt)
 *
 *   Z_series = R + 1/(sC) = (1 + sRC)/(sC)
 *   Z_shunt  = R || (1/(sC)) = R/(1 + sRC)
 *
 *   beta(s) = [R/(1+sRC)] / [(1+sRC)/(sC) + R/(1+sRC)]
 *           = sRC / (s^2*R^2*C^2 + 3sRC + 1)
 *
 * At the oscillation frequency w_0 = 1/(RC):
 *   beta(jw_0) = j / ( -1 + 3j + 1 ) = 1/3
 *   |beta| = 1/3
 *   angle(beta) = 0 degrees
 *
 * Therefore, to satisfy the Barkhausen criterion |A*beta| = 1,
 * the amplifier must provide gain A = 3 exactly.
 *
 * @param R       Resistance (Ohm)
 * @param C       Capacitance (F)
 * @param f       Frequency (Hz)
 * @param mag     Output: |beta(jw)|
 * @param phase   Output: phase(beta) in degrees
 */
void wien_bridge_transfer(double R, double C, double f,
                           double *mag, double *phase) {
    if (!mag || !phase || R <= 0.0 || C <= 0.0 || f <= 0.0) {
        if (mag) *mag = 0.0;
        if (phase) *phase = 0.0;
        return;
    }

    double w = 2.0 * M_PI * f;
    double wRC = w * R * C;

    /* beta(jw) = j*wRC / (1 - w^2*R^2*C^2 + 3j*wRC) */
    double denom_re = 1.0 - wRC * wRC;
    double denom_im = 3.0 * wRC;
    double denom_mag_sq = denom_re * denom_re + denom_im * denom_im;

    double num_re = 0.0;
    double num_im = wRC;

    /* Complex division: beta = num/denom */
    double beta_re = (num_re * denom_re + num_im * denom_im) / denom_mag_sq;
    double beta_im = (num_im * denom_re - num_re * denom_im) / denom_mag_sq;

    *mag = sqrt(beta_re * beta_re + beta_im * beta_im);
    *phase = atan2(beta_im, beta_re) * 180.0 / M_PI;
}

/**
 * phase_shift_transfer - Phase-shift network transfer function.
 *
 * The phase-shift oscillator uses a cascade of RC high-pass or low-pass
 * filters to provide 180 degrees of phase shift at the oscillation
 * frequency. Together with the inverting amplifier's 180-degree phase
 * shift, the total loop phase shift is 360 degrees (or 0 degrees).
 *
 * FOR 3-STAGE RC HIGHPASS (most common):
 *   Each stage provides 60 degrees phase shift at f_osc.
 *
 *   beta(s) = (sRC)^3 / ((sRC)^3 + 6(sRC)^2 + 5sRC + 1)
 *
 *   At f_osc = 1/(2*pi*RC*sqrt(6)):
 *     |beta| = 1/29 (approximately)
 *     phase(beta) = 180 degrees
 *
 *   The amplifier must provide gain of at least 29 for oscillation.
 *   This is why phase-shift oscillators require high-gain amplifiers
 *   and are generally noisier than Wien bridge designs.
 *
 * FOR 4-STAGE RC:
 *   Each stage provides 45 degrees phase shift.
 *   f_osc = 1/(2*pi*RC*sqrt(10/7))
 *   beta = 1/18.4 at f_osc
 *
 * @param R        Resistance per stage (Ohm)
 * @param C        Capacitance per stage (F)
 * @param n_stages Number of RC stages (3 or 4)
 * @param f        Frequency (Hz)
 * @param mag      Output: |beta|
 * @param phase    Output: phase(beta) in degrees
 *
 * Reference: Ginzton & Hollingsworth "Phase-Shift Oscillators" Proc. IRE (1941)
 */
void phase_shift_transfer(double R, double C, int n_stages, double f,
                           double *mag, double *phase) {
    if (!mag || !phase || R <= 0.0 || C <= 0.0 || f <= 0.0) {
        if (mag) *mag = 0.0;
        if (phase) *phase = 0.0;
        return;
    }

    double w = 2.0 * M_PI * f;
    double wRC = w * R * C;

    /* Single-stage HPF transfer: H1(s) = sRC/(1 + sRC) */
    /* Magnitude: |H1| = wRC/sqrt(1+wRC^2) */
    /* Phase: phi1 = 90 - arctan(wRC) */

    double mag_per_stage = wRC / sqrt(1.0 + wRC * wRC);
    double phase_per_stage = 90.0 - atan(wRC) * 180.0 / M_PI;

    /* Cascade: magnitude multiplies, phase adds */
    *mag = pow(mag_per_stage, n_stages);

    double total_phase = n_stages * phase_per_stage;
    /* Normalize phase */
    while (total_phase > 180.0) total_phase -= 360.0;
    while (total_phase < -180.0) total_phase += 360.0;
    *phase = total_phase;
}

/*============================================================================
 * L4: FUNDAMENTAL LAWS - Minimum Gain
 *============================================================================*/

/**
 * minimum_gain_for_oscillation - Required amplifier gain for sustained oscillation.
 *
 * This is a direct consequence of the Barkhausen criterion:
 *   |A * beta| >= 1 at the oscillation frequency.
 *
 * Each oscillator topology has a different beta at f_osc:
 *
 *   Wien bridge:       beta = 1/3    -> A_min = 3
 *   Phase-shift (3):   beta = 1/29   -> A_min = 29
 *   Phase-shift (4):   beta = 1/18.4 -> A_min = 18.4
 *   Quadrature:        beta = 1      -> A_min = 1 (integrator loop)
 *   Twin-T:            beta = 1      -> A_min = 1 (at notch null)
 *   Bubba (4-stage LP): beta = 1/4   -> A_min = 4
 *
 * In practice, A should be set slightly higher than A_min (e.g., +10%)
 * to ensure reliable startup. The amplitude is then stabilized by
 * nonlinear limiting or automatic gain control (AGC).
 *
 * @param type    Oscillator topology type
 * @return        Minimum amplifier gain (V/V)
 */
double minimum_gain_for_oscillation(OscillatorType type) {
    switch (type) {
        case OSC_WIEN_BRIDGE:  return 3.0;
        case OSC_PHASE_SHIFT:  return 29.0;
        case OSC_QUADRATURE:   return 1.0;
        case OSC_TWIN_T:       return 1.0;
        case OSC_BUBBA:        return 4.0;
        default:               return 3.0;
    }
}

/*============================================================================
 * L5: ALGORITHMS/METHODS - Oscillator Design
 *============================================================================*/

/**
 * wien_bridge_design - Design a complete Wien bridge oscillator.
 *
 * The Wien bridge oscillator is the classic low-distortion audio
 * oscillator, producing sine waves from < 1 Hz to > 1 MHz.
 *
 * DESIGN PROCEDURE:
 *   1. Choose C based on frequency range:
 *      f < 100 Hz:   C = 100 nF - 1 uF (film capacitor)
 *      100 Hz-10 kHz: C = 10-100 nF
 *      f > 10 kHz:   C = 1-10 nF
 *   2. Compute R = 1/(2*pi*f_osc*C)
 *   3. Verify R is practical (1k-1M)
 *   4. Set negative feedback for gain = 3:
 *      R_neg_fb / R_pos_fb = 2
 *      Choose R_pos_fb = 10k, R_neg_fb = 20k
 *   5. For ultra-low THD (< 0.01%), add amplitude stabilization:
 *      - Option A: Thermistor in feedback (classic HP200A approach)
 *        As amplitude increases, thermistor heats up, R decreases, gain drops.
 *      - Option B: JFET as voltage-controlled resistor
 *        Rectified output controls JFET gate, adjusting gain.
 *      - Option C: Diode limiter (simplest but highest THD ~0.1-1%)
 *   6. Verify op-amp GBW >> f_osc * 3 (at least 10x for good THD)
 *
 * @param cfg     Oscillator config with f_osc set; component values filled
 * @param opamp   Op-amp parameters
 * @return        1 on success
 *
 * Reference: Hewlett "A New Type Resistance-Capacity Oscillator" (1939)
 *            Williams "Linear Thermistor AGC Wien Bridge" TI App Note
 */
int wien_bridge_design(OscillatorConfig *cfg, const OpAmpParams *opamp) {
    if (!cfg || !opamp || cfg->f_osc <= 0.0) return 0;

    cfg->type = OSC_WIEN_BRIDGE;
    double f = cfg->f_osc;

    /* Choose C based on frequency */
    if (f < 100.0)       cfg->C_freq = 100.0e-9;
    else if (f < 10000.0) cfg->C_freq = 10.0e-9;
    else                  cfg->C_freq = 1.0e-9;

    /* Compute R = 1/(2*pi*f*C) */
    cfg->R_freq = 1.0 / (2.0 * M_PI * f * cfg->C_freq);

    /* Verify practical range */
    if (cfg->R_freq < 100.0 || cfg->R_freq > 1.0e7) {
        /* Adjust C */
        if (cfg->R_freq < 100.0) {
            cfg->C_freq *= 100.0;
            cfg->R_freq = 1.0 / (2.0 * M_PI * f * cfg->C_freq);
        } else {
            cfg->C_freq /= 100.0;
            cfg->R_freq = 1.0 / (2.0 * M_PI * f * cfg->C_freq);
        }
    }

    /* Set gain network: need A = 3, so R_neg_fb/R_pos_fb = 2 */
    cfg->R_pos_fb = 10000.0;     /* 10k Ohm */
    cfg->R_neg_fb = 20000.0;     /* 20k Ohm */

    /* Check op-amp GBW: should be > 10 * f_osc * 3 */
    if (opamp->GBW < 10.0 * f * 3.0) return 0;

    cfg->w_osc = 2.0 * M_PI * f;
    cfg->n_stages = 0;

    return 1;
}

/**
 * phase_shift_oscillator_design - Design a phase-shift oscillator.
 *
 * Uses three RC high-pass sections (each providing 60 deg phase shift
 * at f_osc) cascaded with an inverting amplifier.
 *
 * PROS: Simple, works with any op-amp, wide frequency range.
 * CONS: High gain requirement (29x for 3-stage), higher THD than Wien.
 *
 * FREQUENCY: f_osc = 1/(2*pi*R*C*sqrt(6)) for 3 identical stages.
 *
 * LOADING EFFECT: Each RC stage loads the previous one.
 * The correct frequency accounting for loading is:
 *   f_osc = 1/(2*pi*R*C*sqrt(6 + 4*R_out/R))
 *
 * Using a buffer between stages eliminates loading but requires
 * more op-amps.
 *
 * @param cfg     Oscillator config (f_osc and n_stages set)
 * @param opamp   Op-amp parameters
 * @return        1 on success
 */
int phase_shift_oscillator_design(OscillatorConfig *cfg, const OpAmpParams *opamp) {
    if (!cfg || !opamp || cfg->f_osc <= 0.0) return 0;

    cfg->type = OSC_PHASE_SHIFT;
    double f = cfg->f_osc;
    int n = (cfg->n_stages >= 3) ? cfg->n_stages : 3;
    cfg->n_stages = n;

    /* Choose C */
    if (f < 1000.0)       cfg->C_ps[0] = 100.0e-9;
    else if (f < 100000.0) cfg->C_ps[0] = 10.0e-9;
    else                   cfg->C_ps[0] = 1.0e-9;

    for (int i = 1; i < 3 && i < n; i++) {
        cfg->C_ps[i] = cfg->C_ps[0];
    }

    /* Compute R for n-stage phase-shift oscillator */
    double factor = (n == 4) ? sqrt(10.0/7.0) : sqrt(6.0);
    double R = 1.0 / (2.0 * M_PI * f * cfg->C_ps[0] * factor);

    for (int i = 0; i < n; i++) {
        cfg->R_ps[i] = R;
    }

    /* Amplifier gain must exceed 1/|beta| */
    double beta_min;
    if (n == 3) beta_min = 1.0 / 29.0;
    else if (n == 4) beta_min = 1.0 / 18.4;
    else beta_min = 1.0 / 29.0;

    double A_min = 1.0 / beta_min;
    cfg->R_pos_fb = 10000.0;
    cfg->R_neg_fb = A_min * cfg->R_pos_fb;

    cfg->w_osc = 2.0 * M_PI * f;

    return 1;
}

/**
 * quadrature_oscillator_design - Design quadrature (sine/cosine) oscillator.
 *
 * The quadrature oscillator produces two sinusoids 90 degrees apart:
 *   v1(t) = V_pk * sin(w_osc * t)
 *   v2(t) = V_pk * cos(w_osc * t)
 *
 * TOPOLOGY: Two integrators in a feedback loop.
 *   Integrator 1: H1(s) = -1/(s*R*C)
 *   Integrator 2: H2(s) = -1/(s*R*C)
 *   The loop gain at w_osc = 1/(RC) is exactly -1 (magnitude 1, phase 180).
 *
 * KEY EQUATIONS:
 *   f_osc = 1/(2*pi*R*C) (with identical integrator R and C)
 *   A_min = 1 (inherently satisfied by the integrator loop)
 *
 * ADVANTAGE: No external AGC needed (amplitude limited by op-amp saturation).
 * The oscillation amplitude is determined by the supply rails, giving a
 * predictable peak-to-peak output.
 *
 * Quadrature oscillators are widely used in:
 *   - Lock-in amplifiers (need both sin and cos references)
 *   - IQ modulators/demodulators
 *   - Vector network analyzers
 *
 * @param cfg     Oscillator config with f_osc set
 * @param opamp   Op-amp parameters
 * @return        1 on success
 *
 * Reference: Franco S13.4, Horowitz & Hill S5.16
 */
int quadrature_oscillator_design(OscillatorConfig *cfg, const OpAmpParams *opamp) {
    if (!cfg || !opamp || cfg->f_osc <= 0.0) return 0;

    cfg->type = OSC_QUADRATURE;
    double f = cfg->f_osc;

    /* Choose integrator C */
    cfg->C_freq = 10.0e-9;  /* 10 nF */
    cfg->C_ps[0] = cfg->C_freq;
    cfg->C_ps[1] = cfg->C_freq;

    /* f_osc = 1/(2*pi*R*C) */
    cfg->R_freq = 1.0 / (2.0 * M_PI * f * cfg->C_freq);
    cfg->R_ps[0] = cfg->R_freq;
    cfg->R_ps[1] = cfg->R_freq;

    cfg->w_osc = 2.0 * M_PI * f;
    cfg->n_stages = 2;  /* Two integrators */

    return 1;
}

/*============================================================================
 * L6: CANONICAL PROBLEMS - Oscillator Waveforms
 *============================================================================*/

/**
 * oscillator_output_sample - Sample oscillator output at time t.
 *
 * Generates the oscillator output waveform(s) at a given time.
 * For Wien bridge and phase-shift oscillators, the output after
 * startup is a pure sine wave.
 *
 * @param cfg      Oscillator configuration
 * @param t        Time (s)
 * @param v_sin    Output: sine output (V)
 * @param v_cos    Output: cosine output (V), valid for quadrature
 */
void oscillator_output_sample(const OscillatorConfig *cfg, double t,
                               double *v_sin, double *v_cos) {
    if (!cfg || !v_sin || !v_cos) return;

    double w = cfg->w_osc;
    double V = cfg->V_amplitude;
    if (V <= 0.0) V = 1.0;  /* Default amplitude */

    *v_sin = V * sin(w * t);

    if (cfg->type == OSC_QUADRATURE) {
        *v_cos = V * cos(w * t);
    } else {
        *v_cos = 0.0;
    }
}

/**
 * oscillator_thd_estimate - Estimate total harmonic distortion.
 *
 * THD depends on the amplitude stabilization method:
 *
 *   No AGC (clipping):          THD = 5-10%   (severe clipping)
 *   Diode soft limiter:         THD = 0.1-1%  (moderate, simple)
 *   JFET AGC:                   THD = 0.01-0.1% (good for most uses)
 *   Thermistor (incandescent):  THD < 0.001%  (excellent, slow)
 *   Multiplier-based AGC:       THD < 0.0001% (best, complex)
 *
 * The HP200A used a thermistor for AGC, achieving THD < 0.1%
 * in 1939, which was revolutionary at the time.
 *
 * @param cfg   Oscillator configuration
 * @return      Estimated THD (fraction, 0.01 = 1%)
 */
double oscillator_thd_estimate(const OscillatorConfig *cfg) {
    if (!cfg) return 0.10;  /* 10% default */

    if (!cfg->has_agc) {
        return 0.05;  /* 5%: moderate clipping */
    }

    /* With AGC, THD depends on time constant */
    if (cfg->agc_time_constant > 1.0) {
        return 0.001;  /* 0.1%: slow/accurate AGC (thermistor) */
    } else if (cfg->agc_time_constant > 0.01) {
        return 0.005;  /* 0.5%: moderate AGC (JFET) */
    } else {
        return 0.02;  /* 2%: fast/simple AGC (diode) */
    }
}

/**
 * oscillator_startup_time - Estimate startup time for oscillation.
 *
 * The startup time depends on:
 *   - The excess loop gain (how much > 1)
 *   - The effective Q of the frequency-selective network
 *
 * For Wien bridge with Q_eff = 1/3:
 *   tau = Q_eff / (pi * f_osc * (loop_gain - 1))
 *
 * For 5% excess gain (loop_gain = 1.05, A = 3.15):
 *   tau = (1/3) / (pi * 1000 * 0.05) = 2.1 ms (for f_osc = 1 kHz)
 *   N_cycles = tau * f_osc = 2.1 cycles
 *
 * Higher excess gain => faster startup but worse THD.
 * The art of oscillator design is balancing startup speed against
 * steady-state distortion.
 *
 * @param cfg   Oscillator configuration
 * @return      Time to reach 90% of steady-state amplitude (s)
 */
double oscillator_startup_time(const OscillatorConfig *cfg) {
    if (!cfg || cfg->f_osc <= 0.0) return 0.0;

    double Q_eff;

    switch (cfg->type) {
        case OSC_WIEN_BRIDGE:
            Q_eff = 1.0 / 3.0;  /* Wien bridge Q */
            break;
        case OSC_PHASE_SHIFT:
            Q_eff = 0.5;        /* Phase-shift: moderate Q */
            break;
        case OSC_QUADRATURE:
            Q_eff = 1.0 / sqrt(2.0);  /* Quadrature: Q = 1/sqrt(2) */
            break;
        case OSC_TWIN_T:
            Q_eff = 0.25;       /* Twin-T notch Q */
            break;
        default:
            Q_eff = 0.5;
            break;
    }

    /* Excess loop gain (typical: 5-10%) */
    double excess_gain = 0.10;
    if (cfg->has_agc) excess_gain = 0.20;  /* AGC allows more excess */

    double pi = M_PI;
    double tau = Q_eff / (pi * cfg->f_osc * excess_gain);

    /* 4-5 time constants to reach 90% amplitude */
    return 5.0 * tau;
}
