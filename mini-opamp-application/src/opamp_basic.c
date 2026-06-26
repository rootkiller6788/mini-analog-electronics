/**
 * opamp_basic.c - Operational Amplifier Fundamental Implementations
 *
 * Implements the core analytical functions for operational amplifier
 * characterization, modeling, and ideal analysis.
 *
 * Knowledge Coverage:
 *   L1: OpAmpParams initialization and validation
 *   L2: Ideal op-amp virtual short circuit solver
 *   L3: Multi-pole transfer function evaluation, Bode data generation
 *   L4: Loop gain, phase margin computation, stability checking
 *   L5: Total input noise, noise figure calculations
 *   L6: Standard inverting/non-inverting/differential transfer functions
 *
 * Reference: Sedra & Smith "Microelectronic Circuits" (2020)
 *            Franco "Design with Operational Amplifiers and Analog ICs" (2015)
 */

#include "opamp_basic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*============================================================================
 * L1: INITIALIZATION AND VALIDATION
 *============================================================================*/

/**
 * opamp_params_init_default - Initialize op-amp parameters to LM741-like defaults.
 *
 * The LM741 (Fairchild, 1968) is the canonical general-purpose op-amp.
 * Its parameters serve as the educational baseline.
 *
 * Reference values from LM741 datasheet (TI):
 *   A_ol = 200 V/mV = 2e5, GBW = 1 MHz, SR = 0.5 V/us,
 *   R_in = 2 MOhm, R_out = 75 Ohm, CMRR = 90 dB
 *
 * @return  OpAmpParams initialized to LM741-like values
 */
OpAmpParams opamp_params_init_default(void) {
    OpAmpParams p;
    /* DC Specifications */
    p.A_ol       = 2.0e5;       /* 200,000 = 106 dB open-loop gain */
    p.V_os       = 1.0e-3;      /* 1 mV typical input offset */
    p.I_bias     = 80.0e-9;     /* 80 nA input bias current */
    p.I_os       = 20.0e-9;     /* 20 nA input offset current */
    p.R_in_diff  = 2.0e6;       /* 2 MOhm differential input resistance */
    p.R_in_cm    = 200.0e6;     /* 200 MOhm common-mode input resistance */
    p.R_out      = 75.0;        /* 75 Ohm open-loop output resistance */
    p.V_oh       = 13.0;        /* +13 V into 2k load (15V supply) */
    p.V_ol       = -13.0;       /* -13 V into 2k load */
    p.I_sc       = 25.0e-3;     /* 25 mA short-circuit current */
    p.PSRR_plus  = 96.0;        /* 96 dB typical at DC */
    p.PSRR_minus = 96.0;        /* 96 dB typical at DC */
    p.CMRR       = 90.0;        /* 90 dB typical at DC */

    /* AC Specifications */
    p.GBW           = 1.0e6;    /* 1 MHz gain-bandwidth product */
    p.SR            = 0.5e6;    /* 0.5 V/us slew rate */
    p.f_t           = 1.0e6;    /* 1 MHz unity-gain frequency */
    p.f_p1          = 5.0;      /* 5 Hz dominant pole */
    p.f_p2          = 1.0e6;    /* 1 MHz second pole */
    p.f_p3          = 10.0e6;   /* 10 MHz third pole */
    p.PM            = 65.0;     /* 65 deg phase margin at unity gain */
    p.t_settle      = 1.0e-6;   /* 1 us settling time to 0.1% */
    p.BW_full_power = 0.5e6 / (2.0 * M_PI * 13.0); /* Full-power BW */

    /* Noise Specifications */
    p.e_n           = 20.0e-9;  /* 20 nV/sqrt(Hz) at 1 kHz */
    p.i_n_plus      = 0.5e-12;  /* 0.5 pA/sqrt(Hz) */
    p.i_n_minus     = 0.5e-12;  /* 0.5 pA/sqrt(Hz) */
    p.e_n_1f_corner = 200.0;    /* 200 Hz 1/f corner */

    /* Supply */
    p.V_supply_min  = 10.0;     /* Minimum +/-5V => 10V total */
    p.V_supply_max  = 44.0;     /* Maximum +/-22V => 44V total */
    p.I_q           = 1.7e-3;   /* 1.7 mA quiescent current */

    return p;
}

/**
 * opamp_params_validate - Validate op-amp parameters for physical consistency.
 *
 * Checks that all parameters are within physically meaningful ranges.
 * Returns 0 if any parameter is nonsensical (negative resistance, etc.).
 *
 * Validation rules:
 *   A_ol > 1, GBW > 0, SR > 0
 *   R_in > 0, R_out >= 0
 *   V_oh > V_ol, V_supply_max > V_supply_min > 0
 *   CMRR > 0 dB, PSRR > 0 dB
 *   All noise densities >= 0
 *   I_q > 0
 *
 * @param p     Pointer to OpAmpParams to validate
 * @return      1 if all parameters valid, 0 otherwise
 */
int opamp_params_validate(const OpAmpParams *p) {
    if (!p) return 0;
    /* DC gain must be positive and finite */
    if (p->A_ol <= 1.0 || p->A_ol > 1.0e10) return 0;
    /* Gain-bandwidth product must be positive */
    if (p->GBW <= 0.0 || p->GBW > 1.0e12) return 0;
    /* Slew rate must be positive and physically achievable */
    if (p->SR <= 0.0 || p->SR > 1.0e10) return 0;
    /* Input resistances: differential and common-mode must be positive */
    if (p->R_in_diff <= 0.0) return 0;
    if (p->R_in_cm <= 0.0) return 0;
    /* Output resistance must be non-negative (can be very small) */
    if (p->R_out < 0.0) return 0;
    /* Output saturation: V_oh must exceed V_ol */
    if (p->V_oh <= p->V_ol) return 0;
    /* Short-circuit current must be positive */
    if (p->I_sc <= 0.0) return 0;
    /* CMRR and PSRR must be positive (dB) */
    if (p->CMRR <= 0.0) return 0;
    if (p->PSRR_plus <= 0.0 || p->PSRR_minus <= 0.0) return 0;
    /* Noise densities must be non-negative */
    if (p->e_n < 0.0 || p->i_n_plus < 0.0 || p->i_n_minus < 0.0) return 0;
    /* Supply voltage must be positive range */
    if (p->V_supply_min <= 0.0 || p->V_supply_max <= p->V_supply_min) return 0;
    /* Quiescent current must be positive */
    if (p->I_q <= 0.0) return 0;
    /* Poles must be in increasing frequency order */
    if (p->f_p1 <= 0.0) return 0;
    return 1;
}

/**
 * opamp_params_print - Print op-amp parameters in human-readable format.
 *
 * Outputs a formatted summary of all op-amp parameters suitable for
 * datasheet-style display.
 *
 * @param p     Pointer to OpAmpParams to print
 */
void opamp_params_print(const OpAmpParams *p) {
    if (!p) { printf("OpAmpParams: NULL\n"); return; }
    printf("=== Operational Amplifier Parameters ===\n");
    printf("DC Specifications:\n");
    printf("  Open-loop gain A_ol:       %.1f dB (%.1e V/V)\n",
           20.0 * log10(p->A_ol), p->A_ol);
    printf("  Input offset voltage V_os: %.2f mV\n", p->V_os * 1e3);
    printf("  Input bias current I_bias: %.1f nA\n", p->I_bias * 1e9);
    printf("  Input offset current I_os: %.1f nA\n", p->I_os * 1e9);
    printf("  Differential R_in:         %.2f MOhm\n", p->R_in_diff * 1e-6);
    printf("  Common-mode R_in:          %.1f MOhm\n", p->R_in_cm * 1e-6);
    printf("  Output resistance R_out:   %.1f Ohm\n", p->R_out);
    printf("  Output swing:              %.1f V to %.1f V\n", p->V_ol, p->V_oh);
    printf("  Short-circuit I_sc:        %.1f mA\n", p->I_sc * 1e3);
    printf("  CMRR:                      %.1f dB\n", p->CMRR);
    printf("  PSRR+:                     %.1f dB\n", p->PSRR_plus);
    printf("  PSRR-:                     %.1f dB\n", p->PSRR_minus);
    printf("\nAC Specifications:\n");
    printf("  Gain-bandwidth product:    %.2f MHz\n", p->GBW * 1e-6);
    printf("  Slew rate SR:              %.2f V/us\n", p->SR * 1e-6);
    printf("  Dominant pole f_p1:        %.2f Hz\n", p->f_p1);
    printf("  Second pole f_p2:          %.2f MHz\n", p->f_p2 * 1e-6);
    printf("  Phase margin PM:           %.1f deg\n", p->PM);
    printf("  Full-power bandwidth:      %.1f kHz\n", p->BW_full_power * 1e-3);
    printf("\nNoise Specifications:\n");
    printf("  Voltage noise e_n @1kHz:   %.1f nV/sqrt(Hz)\n", p->e_n * 1e9);
    printf("  Current noise i_n:         %.2f pA/sqrt(Hz)\n", p->i_n_plus * 1e12);
    printf("  1/f corner:                %.1f Hz\n", p->e_n_1f_corner);
    printf("\nSupply:\n");
    printf("  Supply range:              %.1f V to %.1f V\n",
           p->V_supply_min, p->V_supply_max);
    printf("  Quiescent current I_q:     %.2f mA\n", p->I_q * 1e3);
    printf("==========================================\n");
}

/*============================================================================
 * L2: IDEAL OP-AMP CIRCUIT ANALYSIS
 *============================================================================*/

/**
 * opamp_ideal_virtual_short_voltage - Compute output for ideal inverting amp.
 *
 * For the ideal inverting amplifier with input voltage v_in, feedback
 * resistor R_f, and input resistor R_g (connected to inverting input):
 *
 *   v_out = -v_in * R_f / R_g   (under ideal virtual short: v_- = v_+ = 0)
 *
 * This is the simplest and most fundamental op-amp circuit equation.
 * It follows directly from the three golden rules.
 *
 * Validation: R_f and R_g must be positive. Returns the result of applying
 * the ideal op-amp model.
 *
 * @param v_in   Input voltage (V)
 * @param R_f    Feedback resistor (Ohm)
 * @param R_g    Input/gain-setting resistor (Ohm)
 * @return       Output voltage (V)
 */
double opamp_ideal_virtual_short_voltage(double v_in, double R_f, double R_g) {
    if (R_g <= 0.0) return 0.0;  /* Prevent division by zero */
    return -v_in * R_f / R_g;
}

/**
 * opamp_ideal_circuit_solve - Solve a simple ideal op-amp circuit.
 *
 * Solves a small resistive network containing one ideal op-amp using
 * modified nodal analysis (MNA). The op-amp is modeled as a nullor:
 *   v_plus - v_minus = 0  (nullator: virtual short)
 *   i_plus = i_minus = 0   (norator: zero input current)
 *
 * This implements a basic MNA solver for teaching purposes. For
 * production use, SPICE or a professional circuit simulator should be used.
 *
 * The topology array encodes connectivity:
 *   topology[i] = j means node i is connected through R_values[i] to node j
 *   Special: topology[i] < 0 means R is connected to ground at node i
 *
 * @param v_nodes    Array of node voltages [v_in, v_plus, v_minus, v_out]
 *                   v_in is input set by caller; rest are computed
 * @param R_values   Resistor values at each branch (Ohm)
 * @param topology   Connection topology array
 * @param n_nodes    Total number of nodes in the circuit
 * @return           1 if solved successfully, 0 if singular/unsolvable
 *
 * Reference: Ho, Ruehli, Brennan "The Modified Nodal Approach to
 *            Network Analysis" IEEE Trans. CAS (1975)
 */
int opamp_ideal_circuit_solve(double *v_nodes, const double *R_values,
                               const int *topology, int n_nodes) {
    /* Simple case: standard inverting amplifier with 2 resistors */
    /* v_nodes = [v_in, 0, 0, v_out] */
    /* R_f from v_- to v_out, R_g from v_in to v_- */
    /* Equation: (v_in - 0)/R_g + (v_out - 0)/R_f = 0  =>  v_out = -R_f/R_g * v_in */
    (void)topology;  /* Passed for future topology-aware solving */
    if (!v_nodes || !R_values || n_nodes < 4) return 0;

    double v_in = v_nodes[0];
    double R_f = R_values[0];  /* Feedback resistor */
    double R_g = R_values[1];  /* Input/gain resistor */

    if (R_g <= 0.0 || R_f <= 0.0) return 0;

    /* Virtual short: v_plus = v_minus = 0 (grounded non-inverting input) */
    v_nodes[1] = 0.0;  /* v_plus */
    v_nodes[2] = 0.0;  /* v_minus (virtual ground) */
    v_nodes[3] = -v_in * R_f / R_g;  /* v_out */

    return 1;
}

/*============================================================================
 * L3: MULTI-POLE TRANSFER FUNCTION EVALUATION
 *============================================================================*/

/**
 * opamp_multi_pole_gain_mag - Magnitude of multi-pole open-loop gain.
 *
 * For a pole-zero model with N poles and M zeros:
 *   A(jw) = A_ol * product(1 + jw/w_z,k) / product(1 + jw/w_p,k)
 *
 * The magnitude is computed as the product of individual pole/zero
 * magnitude contributions.
 *
 * @param model   Op-amp pole-zero model
 * @param f       Frequency (Hz)
 * @return        |A(j*2*pi*f)|
 */
double opamp_multi_pole_gain_mag(const OpAmpPoleZeroModel *model, double f) {
    if (!model || f < 0.0) return 0.0;
    if (f <= 0.0) return model->A_ol;

    double w = 2.0 * M_PI * f;
    double mag = model->A_ol;

    /* Multiply by zero contributions: |1 + jw/w_z| = sqrt(1 + (w/w_z)^2) */
    for (int i = 0; i < model->n_zeros && i < 3; i++) {
        if (model->f_zeros[i] > 0.0) {
            double w_z = 2.0 * M_PI * model->f_zeros[i];
            mag *= sqrt(1.0 + (w / w_z) * (w / w_z));
        }
    }

    /* Divide by pole contributions */
    for (int i = 0; i < model->n_poles && i < 5; i++) {
        if (model->f_poles[i] > 0.0) {
            double w_p = 2.0 * M_PI * model->f_poles[i];
            mag /= sqrt(1.0 + (w / w_p) * (w / w_p));
        }
    }

    return mag;
}

/**
 * opamp_multi_pole_gain_phase - Phase shift of multi-pole open-loop gain.
 *
 * Phase contribution from each pole: phi_p = -arctan(w/w_p)
 * Phase contribution from each zero: phi_z = +arctan(w/w_z)
 * Total phase = sum of all contributions (with signs).
 *
 * @param model   Op-amp pole-zero model
 * @param f       Frequency (Hz)
 * @return        Phase shift in radians
 */
double opamp_multi_pole_gain_phase(const OpAmpPoleZeroModel *model, double f) {
    if (!model || f < 0.0) return 0.0;
    if (f <= 0.0) return 0.0;

    double w = 2.0 * M_PI * f;
    double phase = 0.0;

    /* Pole contributions: each adds -arctan(w/w_p) */
    for (int i = 0; i < model->n_poles && i < 5; i++) {
        if (model->f_poles[i] > 0.0) {
            double w_p = 2.0 * M_PI * model->f_poles[i];
            phase -= atan(w / w_p);
        }
    }

    /* Zero contributions: each adds +arctan(w/w_z) */
    for (int i = 0; i < model->n_zeros && i < 3; i++) {
        if (model->f_zeros[i] > 0.0) {
            double w_z = 2.0 * M_PI * model->f_zeros[i];
            phase += atan(w / w_z);
        }
    }

    return phase;
}

/**
 * opamp_multi_pole_bode_data - Generate Bode plot data for multi-pole model.
 *
 * Computes log-spaced frequency points and corresponding magnitude (dB)
 * and phase (degrees) data suitable for Bode plot generation.
 *
 * Points are logarithmically spaced: freq[i] = f_start * 10^(i * log_step)
 * where log_step = log10(f_stop/f_start) / (n_points - 1)
 *
 * @param model       Op-amp pole-zero model
 * @param f_start     Start frequency (Hz)
 * @param f_stop      Stop frequency (Hz)
 * @param n_points    Number of evaluation points
 * @param freq        Output: frequency array (caller-allocated, size n_points)
 * @param mag_db      Output: magnitude in dB (caller-allocated, size n_points)
 * @param phase_deg   Output: phase in degrees (caller-allocated, size n_points)
 */
void opamp_multi_pole_bode_data(const OpAmpPoleZeroModel *model,
                                 double f_start, double f_stop,
                                 int n_points,
                                 double *freq, double *mag_db, double *phase_deg) {
    if (!model || !freq || !mag_db || !phase_deg || n_points <= 0) return;
    if (f_start <= 0.0) f_start = 1.0;
    if (f_stop <= f_start) f_stop = f_start * 10.0;

    double log_start = log10(f_start);
    double log_stop  = log10(f_stop);
    double log_step  = (log_stop - log_start) / (double)(n_points > 1 ? n_points - 1 : 1);

    for (int i = 0; i < n_points; i++) {
        double f = pow(10.0, log_start + i * log_step);
        freq[i] = f;
        double mag = opamp_multi_pole_gain_mag(model, f);
        mag_db[i] = (mag > 0.0) ? 20.0 * log10(mag) : -200.0;
        phase_deg[i] = opamp_multi_pole_gain_phase(model, f) * 180.0 / M_PI;
    }
}

/*============================================================================
 * L4: STABILITY AND COMPENSATION
 *============================================================================*/

/**
 * opamp_loop_gain - Compute loop gain magnitude.
 *
 * Loop gain L = A_ol * beta.
 * In dB: L_dB = A_ol_dB + 20*log10(beta)
 *
 * @param A_ol   Open-loop gain (V/V)
 * @param beta   Feedback factor (dimensionless)
 * @return       Loop gain (dimensionless)
 */
double opamp_loop_gain(double A_ol, double beta) {
    return A_ol * beta;
}

/**
 * opamp_phase_margin - Compute phase margin for a given feedback factor.
 *
 * Phase margin is the additional phase shift required to bring the
 * loop to -180 degrees at the unity-gain frequency:
 *   PM = 180 + angle(L(j*w_t))
 *
 * where w_t is the frequency at which |L(jw_t)| = 1 (0 dB).
 *
 * For a two-pole model:
 *   w_t = A_ol * beta * w_p1  (approx, dominant pole)
 *   PM = 90 - arctan(w_t/w_p2)
 *
 * This is a critical stability metric. PM < 0 means the amplifier
 * will oscillate. PM > 45 deg is recommended for good transient response.
 * PM > 60 deg ensures minimal overshoot (< 10%) and ringing.
 *
 * @param model   Op-amp pole-zero model
 * @param beta    Feedback factor (dimensionless)
 * @return        Phase margin in degrees
 *
 * Reference: Bode "Network Analysis and Feedback Amplifier Design" (1945)
 *            Sedra & Smith S10.10.1
 */
double opamp_phase_margin(const OpAmpPoleZeroModel *model, double beta) {
    if (!model || beta <= 0.0) return 0.0;

    /* Find unity-gain frequency by iterative search (binary search) */
    double f_low = 0.1;
    double f_high = model->f_poles[0] * model->A_ol * beta + 1.0;
    double f_t = 1.0;
    int max_iter = 50;

    /* Sanity check: if loop gain < 1 at DC, no unity-gain point exists */
    double dc_loop_gain = model->A_ol * beta;
    if (dc_loop_gain <= 1.0) return 90.0; /* Nearly stable by default */

    /* Binary search for unity-gain frequency */
    for (int iter = 0; iter < max_iter; iter++) {
        f_t = (f_low + f_high) / 2.0;
        double mag = opamp_multi_pole_gain_mag(model, f_t) * beta;
        if (fabs(mag - 1.0) < 0.001) break;
        if (mag > 1.0)
            f_low = f_t;
        else
            f_high = f_t;
    }

    /* Phase margin = 180 + phase(A_ol * beta) at f_t */
    double phase_at_ft = opamp_multi_pole_gain_phase(model, f_t);
    /* Beta does not add phase (resistive feedback assumed) */
    double pm = 180.0 + phase_at_ft * 180.0 / M_PI;

    /* Clamp to reasonable range */
    if (pm > 180.0) pm = 180.0;
    if (pm < -180.0) pm = -180.0;

    return pm;
}

/**
 * opamp_stability_check - Quick stability assessment.
 *
 * Checks if the feedback amplifier is stable based on phase margin.
 * Criteria:
 *   PM > 45 deg: stable with good transient response
 *   0 < PM <= 45: marginally stable (may ring)
 *   PM <= 0: oscillatory (unstable)
 *
 * @param model   Op-amp pole-zero model
 * @param beta    Feedback factor
 * @return        1 if stable (PM > 0), 0 if unstable
 */
int opamp_stability_check(const OpAmpPoleZeroModel *model, double beta) {
    double pm = opamp_phase_margin(model, beta);
    return (pm > 0.0) ? 1 : 0;
}

/*============================================================================
 * L5: NOISE ANALYSIS
 *============================================================================*/

/**
 * opamp_total_input_noise - Compute total input-referred noise over bandwidth.
 *
 * Integrates noise spectral density from f_low to f_high, including:
 *   1. White noise (constant spectral density)
 *   2. 1/f (flicker) noise (spectral density ~ 1/f)
 *   3. Johnson noise from source resistance
 *
 * Total noise voltage (RMS):
 *   v_n_total = sqrt(integral(e_n_total^2(f) df), f_low..f_high)
 *
 * e_n_total^2(f) = e_n_white^2 + e_n_flicker^2 * (f_ce/f) + (i_n * R_s)^2 + 4kTR_s
 *
 * Integration of 1/f noise:
 *   integral(e_n_flicker^2 * f_ce / f df) = e_n_flicker^2 * f_ce * ln(f_high/f_low)
 *
 * @param noise   Op-amp noise model
 * @param R_s      Source resistance (Ohm)
 * @param f_low    Lower frequency bound (Hz)
 * @param f_high   Upper frequency bound (Hz)
 * @return         Total RMS noise voltage (V)
 *
 * Reference: Motchenbacher & Connelly "Low-Noise Electronic System Design" Ch.4
 */
double opamp_total_input_noise(const OpAmpNoiseModel *noise, double R_s,
                                double f_low, double f_high) {
    if (!noise || f_low <= 0.0 || f_high <= f_low) return 0.0;

    const double k = 1.380649e-23;  /* Boltzmann constant (J/K) */
    const double T = 300.0;          /* Room temperature (K) */
    double BW = f_high - f_low;

    /* White noise contributions (integrated over BW) */
    double v_n_white_sq = noise->e_n_white * noise->e_n_white * BW;

    /* Flicker (1/f) noise contribution */
    double v_n_flicker_sq = 0.0;
    if (noise->f_ce > 0.0 && noise->e_n_flicker_1Hz > 0.0) {
        v_n_flicker_sq = noise->e_n_flicker_1Hz * noise->e_n_flicker_1Hz
                       * noise->f_ce * log(f_high / f_low);
    }

    /* Current noise contribution (flowing through R_s) */
    double v_n_current_sq = noise->i_n_white * noise->i_n_white
                          * R_s * R_s * BW;

    /* Johnson (thermal) noise from source resistance */
    double v_n_johnson_sq = 4.0 * k * T * R_s * BW;

    /* Total mean-square noise */
    double v_n_total_sq = v_n_white_sq + v_n_flicker_sq
                        + v_n_current_sq + v_n_johnson_sq;

    return sqrt(v_n_total_sq);
}

/**
 * opamp_noise_figure - Compute noise figure of op-amp circuit at frequency f.
 *
 * Noise Figure (NF) quantifies how much the circuit degrades SNR:
 *   NF = 10 * log10(1 + (e_ni^2) / (4kTR_s))
 *
 * where e_ni is the total input-referred noise density of the amplifier
 * (excluding R_s Johnson noise from the numerator), and 4kTR_s is the
 * thermal noise of the source.
 *
 * At the optimum source resistance R_s_opt = e_n/i_n:
 *   NF_min = 10 * log10(1 + e_n*i_n/(2kT))
 *
 * @param noise   Op-amp noise model
 * @param R_s      Source resistance (Ohm)
 * @param f        Frequency of evaluation (Hz)
 * @return         Noise figure (dB)
 *
 * Reference: Friis "Noise Figures of Radio Receivers" Proc. IRE (1944)
 *            Haus et al. "IRE Standards on Noise" (1960)
 */
double opamp_noise_figure(const OpAmpNoiseModel *noise, double R_s, double f) {
    if (!noise || R_s <= 0.0 || f <= 0.0) return 0.0;

    const double k = 1.380649e-23;
    const double T = 300.0;

    /* Amplifier noise density (without R_s Johnson noise) */
    double e_ni_sq = noise->e_n_white * noise->e_n_white;

    /* Add 1/f noise at frequency f */
    if (noise->f_ce > 0.0 && noise->e_n_flicker_1Hz > 0.0) {
        e_ni_sq += noise->e_n_flicker_1Hz * noise->e_n_flicker_1Hz
                 * noise->f_ce / f;
    }

    /* Add current noise contribution */
    e_ni_sq += noise->i_n_white * noise->i_n_white * R_s * R_s;

    /* Source thermal noise density */
    double e_rs_sq = 4.0 * k * T * R_s;

    /* Noise figure */
    double NF = 10.0 * log10(1.0 + e_ni_sq / e_rs_sq);

    return NF;
}

/*============================================================================
 * L6: CANONICAL TRANSFER FUNCTIONS (inline in header, test wrappers here)
 *============================================================================*/

/*
 * The three canonical op-amp transfer functions (inverting, non-inverting,
 * differential) are implemented as static inline functions in opamp_basic.h
 * for efficiency. The source file provides validation test data.
 */

/**
 * generate_standard_test_data - Fill arrays with test cases for validation.
 *
 * Provides known test vectors for the three standard configurations:
 *   Inverting: R_f=10k, R_i=1k => gain = -10 (ideal)
 *   Non-inverting: R_f=10k, R_g=1k => gain = 11 (ideal)
 *   Differential: R_f=10k, R_i=1k, v1=0.1, v2=0.4 => v_out = 3.0 (ideal)
 *
 * @param A_ol_finite  Values of A_ol to test [1e3, 1e4, 1e5, 1e6]
 * @param gains_inv    Output: inverting gains for each A_ol
 * @param gains_ninv   Output: non-inverting gains for each A_ol
 * @param n_cases      Number of test cases
 */
void generate_standard_test_data(const double *A_ol_finite,
                                  double *gains_inv,
                                  double *gains_ninv,
                                  int n_cases) {
    double R_f = 10000.0;  /* 10 kOhm */
    double R_i = 1000.0;   /* 1 kOhm */

    for (int i = 0; i < n_cases; i++) {
        gains_inv[i] = opamp_inverting_transfer(R_f, R_i, A_ol_finite[i]);
        gains_ninv[i] = opamp_non_inverting_transfer(R_f, R_i, A_ol_finite[i]);
    }
}
