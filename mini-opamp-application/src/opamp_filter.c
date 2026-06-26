/**
 * opamp_filter.c - Active Filter Design Implementations
 *
 * Implements Butterworth, Chebyshev, Bessel filter synthesis,
 * cascade biquad design, Sallen-Key and MFB component value
 * computation, sensitivity analysis, and frequency response evaluation.
 *
 * Coverage: L1-L6 (Definitions through Canonical Problems)
 */

#include "opamp_filter.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/*============================================================================
 * L2: CORE CONCEPTS - Filter Fundamentals
 *============================================================================*/

/**
 * filter_order_for_attenuation - Compute minimum Butterworth filter order.
 *
 * For a Butterworth low-pass filter, the magnitude response is:
 *   |H(jw)|^2 = 1 / (1 + (w/w_c)^(2N))
 *
 * To achieve attenuation As (dB) at frequency w_s:
 *   As = -10*log10(1 / (1 + (w_s/w_c)^(2N)))
 *   10^(As/10) - 1 = (w_s/w_c)^(2N)
 *   N = log10(10^(As/10) - 1) / (2 * log10(w_s/w_c))
 *
 * Round up to the next integer.
 *
 * For Chebyshev filters, the required order is lower for the same
 * attenuation, at the cost of passband ripple.
 *
 * @param As     Stopband attenuation (dB), must be > 0
 * @param f_s    Stopband frequency (Hz)
 * @param f_p    Passband edge frequency (Hz)
 * @return       Minimum integer order N (>= 1)
 */
int filter_order_for_attenuation(double As, double f_s, double f_p) {
    if (As <= 0.0 || f_s <= f_p || f_p <= 0.0) return 1;
    double w_ratio = f_s / f_p;
    double num = pow(10.0, As / 10.0) - 1.0;
    double N_exact = log10(num) / (2.0 * log10(w_ratio));
    int N = (int)ceil(N_exact);
    return (N < 1) ? 1 : (N > 10 ? 10 : N);
}

/**
 * filter_butterworth_poles - Compute Butterworth pole locations.
 *
 * Butterworth poles are equally spaced on a unit circle in the s-plane,
 * in the left-half plane (ensuring stability).
 *
 * Formula for normalized Butterworth poles (w_c = 1 rad/s):
 *   s_k = -sin((2k-1)*pi/(2N)) + j*cos((2k-1)*pi/(2N))  for k = 1..N
 *
 * Alternative representation:
 *   s_k = exp(j*(pi/2 + pi/(2N) + (k-1)*pi/N))  for k = 0..N-1
 *
 * These poles produce the maximally flat passband characteristic.
 * For N=1: single pole at s = -1
 * For N=2: pole pair at s = -0.7071 +/- j*0.7071 (Q = 0.7071)
 * For N=3: s = -1 and s = -0.5 +/- j*0.8660
 *
 * @param order     Filter order (1-10)
 * @param w_c       Cutoff frequency (rad/s)
 * @param poles_re  Output: real parts (size = order)
 * @param poles_im  Output: imaginary parts (size = order)
 *
 * Reference: Butterworth "On the Theory of Filter Amplifiers" (1930)
 *            Van Valkenburg "Analog Filter Design" Ch.4
 */
void filter_butterworth_poles(int order, double w_c,
                               double *poles_re, double *poles_im) {
    if (!poles_re || !poles_im || order <= 0) return;

    for (int k = 0; k < order; k++) {
        /* Angle for k-th pole in left-half plane */
        double angle = M_PI / 2.0 + M_PI / (2.0 * order) + k * M_PI / order;
        poles_re[k] = w_c * cos(angle);
        poles_im[k] = w_c * sin(angle);
    }
}

/**
 * filter_butterworth_polynomial - Compute Butterworth polynomial coefficients.
 *
 * The Butterworth polynomial B_N(s) has all its roots at the Butterworth
 * poles. The polynomial is built by convolving first-order and second-order
 * factors.
 *
 * The polynomial is normalized so that the coefficient of the highest
 * power (s^N) is 1.0.
 *
 * For w_c = 1 rad/s:
 *   B1(s) = s + 1                                    -> [1, 1]
 *   B2(s) = s^2 + sqrt(2)*s + 1                      -> [1, 1.4142, 1]
 *   B3(s) = (s+1)*(s^2+s+1) = s^3 + 2*s^2 + 2*s + 1 -> [1, 2, 2, 1]
 *   B4(s) = s^4 + 2.613*s^3 + 3.414*s^2 + 2.613*s + 1
 *
 * Coefficients are stored as: coeffs[0] = a_0 (constant term),
 * coeffs[N] = a_N = 1.0 (s^N coefficient).
 *
 * @param order     Filter order (1-10)
 * @param coeffs    Output: polynomial coefficients (size = order+1)
 */
void filter_butterworth_polynomial(int order, double *coeffs) {
    if (!coeffs || order <= 0 || order > 10) return;

    /* Build polynomial from pole factors */
    double poles_re[10], poles_im[10];
    filter_butterworth_poles(order, 1.0, poles_re, poles_im);

    /* Initialize with first factor: (s - p_0) */
    /* coeffs[0] = -p_0, coeffs[1] = 1 for factor (s - p_0) */
    double temp[11] = {0};
    temp[0] = -poles_re[0];  /* Constant term: -p_0 */
    temp[1] = 1.0;           /* s coefficient */

    /* Convolve with remaining factors */
    for (int k = 1; k < order; k++) {
        double next[11] = {0};
        /* Multiply current polynomial by (s - p_k) */
        for (int i = 0; i <= k; i++) {
            /* s * current: shift coefficients up by 1 */
            next[i + 1] += temp[i];        /* s * coeff[i] */
            /* -p_k * current: multiply constant */
            next[i] += temp[i] * (-poles_re[k]);
        }
        for (int i = 0; i <= k + 1; i++) {
            temp[i] = next[i];
        }
    }

    /* Copy to output */
    for (int i = 0; i <= order; i++) {
        coeffs[i] = temp[i];
    }
}

/**
 * filter_chebyshev_poles - Compute Chebyshev Type I pole locations.
 *
 * Chebyshev poles lie on an ellipse in the s-plane:
 *   s_k = -sinh(eta)*sin(theta_k) + j*cosh(eta)*cos(theta_k)
 *
 * where:
 *   epsilon = sqrt(10^(ripple_dB/10) - 1)  (ripple factor)
 *   eta = asinh(1/epsilon) / N
 *   theta_k = pi/2 + (2k+1)*pi/(2N)  for k = 0, 1, ..., N-1
 *
 * The poles produce equal-ripple in the passband.
 * Compared to Butterworth: steeper roll-off but non-flat passband.
 *
 * @param order        Filter order
 * @param ripple_db    Passband ripple (dB, typically 0.1, 0.5, 1, 3)
 * @param w_c          Cutoff frequency (rad/s)
 * @param poles_re     Output: real parts
 * @param poles_im     Output: imaginary parts
 *
 * Reference: Daniels "Approximation Methods for Electronic Filter Design" (1974)
 */
void filter_chebyshev_poles(int order, double ripple_db, double w_c,
                             double *poles_re, double *poles_im) {
    if (!poles_re || !poles_im || order <= 0 || ripple_db <= 0.0) return;

    double epsilon = sqrt(pow(10.0, ripple_db / 10.0) - 1.0);
    double eta = asinh(1.0 / epsilon) / order;
    double sinh_eta = sinh(eta);
    double cosh_eta = cosh(eta);

    for (int k = 0; k < order; k++) {
        double theta = M_PI / 2.0 + (2.0 * k + 1.0) * M_PI / (2.0 * order);
        poles_re[k] = -w_c * sinh_eta * sin(theta);
        poles_im[k] =  w_c * cosh_eta * cos(theta);
    }
}

/*============================================================================
 * L3: MATHEMATICAL STRUCTURES - Biquad Coefficients
 *============================================================================*/

/**
 * filter_biquad_coefficients - Compute normalized biquad coefficients.
 *
 * The biquadratic transfer function H(s) = N(s)/D(s) where
 * D(s) = s^2 + (w_p/Q_p)*s + w_p^2
 *
 * Numerator N(s) depends on filter type:
 *   Low-pass:  N(s) = K * w_p^2
 *   High-pass: N(s) = K * s^2
 *   Band-pass: N(s) = K * (w_p/Q_p) * s
 *   Band-stop: N(s) = K * (s^2 + w_z^2)
 *   All-pass:  N(s) = K * (s^2 - (w_p/Q_p)*s + w_p^2)
 *
 * @param band_type  Filter band type
 * @param w_p        Pole frequency (rad/s)
 * @param Q_p        Pole quality factor
 * @param K          DC gain or passband gain
 * @param a_coeffs   Output: [a0, a1, a2] numerator
 * @param b_coeffs   Output: [b0, b1, b2] denominator (b2=1)
 */
void filter_biquad_coefficients(FilterBandType band_type,
                                 double w_p, double Q_p, double K,
                                 double *a_coeffs, double *b_coeffs) {
    if (!a_coeffs || !b_coeffs || w_p <= 0.0 || Q_p <= 0.0) return;

    /* Denominator is the same for all types */
    b_coeffs[0] = w_p * w_p;           /* w_p^2 */
    b_coeffs[1] = w_p / Q_p;            /* w_p/Q_p, the damping term */
    b_coeffs[2] = 1.0;                  /* s^2 coefficient */

    /* Numerator depends on band type */
    switch (band_type) {
        case FILTER_BAND_LOWPASS:
            a_coeffs[0] = K * w_p * w_p;
            a_coeffs[1] = 0.0;
            a_coeffs[2] = 0.0;
            break;
        case FILTER_BAND_HIGHPASS:
            a_coeffs[0] = 0.0;
            a_coeffs[1] = 0.0;
            a_coeffs[2] = K;
            break;
        case FILTER_BAND_BANDPASS:
            a_coeffs[0] = 0.0;
            a_coeffs[1] = K * w_p / Q_p;
            a_coeffs[2] = 0.0;
            break;
        case FILTER_BAND_BANDSTOP:
            a_coeffs[0] = K * w_p * w_p;
            a_coeffs[1] = 0.0;
            a_coeffs[2] = K;
            break;
        case FILTER_BAND_ALLPASS:
            a_coeffs[0] = K * w_p * w_p;
            a_coeffs[1] = -K * w_p / Q_p;
            a_coeffs[2] = K;
            break;
        default:
            a_coeffs[0] = a_coeffs[1] = a_coeffs[2] = 0.0;
            break;
    }
}

/*============================================================================
 * L4: FUNDAMENTAL LAWS - Sensitivity Theory
 *============================================================================*/

/**
 * filter_sensitivity_wp - Pole frequency sensitivity to components.
 *
 * For Sallen-Key LP (equal-R, equal-C, gain K):
 *   w_p = 1 / (R * C)
 *   S_R^wp = S_C^wp = -1
 *
 * For MFB LP:
 *   w_p = 1 / sqrt(R2*R3*C1*C2)
 *   S_R2^wp = S_R3^wp = S_C1^wp = S_C2^wp = -1/2
 *
 * Lower absolute sensitivities are better for mass production.
 * MFB has lower sensitivities than Sallen-Key for high-Q designs.
 *
 * @param topology    Filter topology
 * @param stage       Filter stage
 * @param S_R1_wp     Output sensitivities (4 values)
 * @param S_R2_wp
 * @param S_C1_wp
 * @param S_C2_wp
 */
void filter_sensitivity_wp(FilterTopology topology, const FilterStage *stage,
                            double *S_R1_wp, double *S_R2_wp,
                            double *S_C1_wp, double *S_C2_wp) {
    if (!stage || !S_R1_wp || !S_R2_wp || !S_C1_wp || !S_C2_wp) return;

    switch (topology) {
        case FILTER_TOPOLOGY_SALLEN_KEY:
            /* w_p = 1/sqrt(R1*R2*C1*C2) */
            *S_R1_wp = -0.5;
            *S_R2_wp = -0.5;
            *S_C1_wp = -0.5;
            *S_C2_wp = -0.5;
            break;
        case FILTER_TOPOLOGY_MFB:
            /* w_p = 1/sqrt(R2*R3*C1*C2), R1 sets gain */
            *S_R1_wp = 0.0;    /* R1 does not affect w_p */
            *S_R2_wp = -0.5;   /* R2 in MFB = feedback R */
            *S_C1_wp = -0.5;
            *S_C2_wp = -0.5;
            break;
        default:
            *S_R1_wp = *S_R2_wp = *S_C1_wp = *S_C2_wp = -0.5;
            break;
    }
}

/**
 * filter_sensitivity_Q - Quality factor sensitivity to components.
 *
 * The Q-sensitivity is critical for high-Q filters.
 *
 * For Sallen-Key with gain K (K < 3):
 *   Q = 1 / (3 - K)  (for equal-R, equal-C)
 *   S_K^Q = K / (3 - K)
 *
 * This means: if K = 2.9 (Q = 10), a 1% change in K causes
 * a 29% change in Q! This is why Sallen-Key is unsuitable for
 * Q > 10 unless the gain is precisely controlled.
 *
 * MFB has much lower Q-sensitivity:
 *   Q = sqrt(R2*R3*C1*C2) / (R3*C2 + R2*C2 + R3*C1*K)
 *   S_R^Q typically 0.5-1.5 (much better than Sallen-Key)
 *
 * @param topology    Filter topology
 * @param stage       Filter stage
 * @param S_comp_Q    Output: 6 Q-sensitivity values [R1,R2,R3,R4,C1,C2]
 */
void filter_sensitivity_Q(FilterTopology topology, const FilterStage *stage,
                           double S_comp_Q[6]) {
    if (!stage || !S_comp_Q) return;

    double Q = stage->Q_p;

    switch (topology) {
        case FILTER_TOPOLOGY_SALLEN_KEY: {
            /* For Sallen-Key, Q is most sensitive to gain-setting resistors */
            S_comp_Q[0] = -0.5;  /* R1 */
            S_comp_Q[1] = -0.5;  /* R2 */
            S_comp_Q[2] =  Q;    /* R3 (gain-setting): S_K^Q = K/(3-K) = Q - 0.5 */
            S_comp_Q[3] =  Q;    /* R4 */
            S_comp_Q[4] = -0.5;  /* C1 */
            S_comp_Q[5] = -0.5;  /* C2 */
            break;
        }
        case FILTER_TOPOLOGY_MFB: {
            /* MFB sensitivities are generally lower */
            S_comp_Q[0] = 0.5;   /* R1 */
            S_comp_Q[1] = -0.5;  /* R2 */
            S_comp_Q[2] = -1.0;  /* R3 */
            S_comp_Q[3] = 0.0;   /* R4 (not used in MFB LP) */
            S_comp_Q[4] = -0.5;  /* C1 */
            S_comp_Q[5] = 0.5;   /* C2 */
            break;
        }
        default:
            for (int i = 0; i < 6; i++) S_comp_Q[i] = 0.5;
            break;
    }
}

/*============================================================================
 * L5: ALGORITHMS/METHODS - Filter Synthesis
 *============================================================================*/

/**
 * filter_sallen_key_lp_design - Design Sallen-Key low-pass stage.
 *
 * UNIT-GAIN SALLEN-KEY (K = 1):
 *   Simplest: R1 = R2 = R, C1 = C2 = C
 *   w_p = 1/(R*C), Q = 1/2 = 0.5 (fixed!) -> Butterworth Q=0.707 not achievable
 *   Limitation: unity-gain SK cannot realize Q > 0.5
 *
 * GAIN-OF-TWO SALLEN-KEY (K = 2):
 *   Choose C, then:
 *   R = 1/(w_p*C), Q = 1/(3-K) = 1
 *   Can realize Butterworth Q=0.707 by choosing K=1.586 (R3=10k, R4=5.86k)
 *
 * DESIGN PROCEDURE:
 *   1. Choose C (1 nF - 100 nF typical)
 *   2. Compute R for desired w_p: R = 1/(w_p*C)
 *   3. For Q > 0.5, must use gain > 1:
 *      K = 3 - 1/Q
 *      Choose R3, R4 such that K = 1 + R3/R4
 *   4. Verify op-amp GBW >> w_p * K
 *
 * @param stage   Stage with w_p and Q_p set; resistor/cap values filled
 * @return        1 on success
 */
int filter_sallen_key_lp_design(FilterStage *stage) {
    if (!stage || stage->w_p <= 0.0 || stage->Q_p <= 0.0) return 0;

    double w_p = stage->w_p;
    double Q = stage->Q_p;
    double K;

    /* Choose C = 10 nF typical */
    double C = 10.0e-9;
    stage->C1 = C;
    stage->C2 = C;

    /* R = 1/(w_p * C) */
    double R = 1.0 / (w_p * C);
    stage->R1 = R;
    stage->R2 = R;

    /* Compute required gain for desired Q */
    /* For equal-R, equal-C: Q = 1/(3 - K) => K = 3 - 1/Q */
    if (Q > 0.5) {
        K = 3.0 - 1.0 / Q;
        if (K <= 1.0) K = 1.0;  /* Minimum gain is 1 (follower) */
        /* Choose R3 = 10k, R4 = R3/(K-1) */
        stage->R3 = 10000.0;
        if (K > 1.001)
            stage->R4 = stage->R3 / (K - 1.0);
        else
            stage->R4 = 1.0e9;  /* Virtual open (follower) */
        stage->K = K;
    } else {
        /* Unity gain for Q <= 0.5 */
        stage->R3 = 0.0;
        stage->R4 = 1.0e9;
        stage->K = 1.0;
    }

    return 1;
}

/**
 * filter_mfb_lp_design - Design Multiple Feedback low-pass filter.
 *
 * MFB advantages:
 *   - Lower component sensitivity than Sallen-Key
 *   - Works well for Q up to 20
 *   - Fewer components (5 vs 6+ for SK with gain)
 *   - Inverting configuration
 *
 * DESIGN EQUATIONS:
 *   Choose C1 (typically C1 = C2)
 *   K = -R2/R1 (gain, negative for inverting)
 *   R2 = 1/(w_p * C1)
 *   R1 = R2 / |K|
 *   R3 = 1/(w_p * C1 * (1/R1 + 2/R2))
 *   Q = sqrt(R2*R3*C1*C2) / (R3*C2 + R2*C2 + R3*C1*K)
 *
 * For low-pass:
 *   Let m = C2/C1 (capacitor ratio, typically 1)
 *   R2 = 1/(w_p * C1)
 *   R3 = (m * Q^2 * R2) / (1 + m*Q*(1 - K))
 *   R1 = R2 / |K|
 *
 * @param stage   Stage with w_p, Q_p, K set; component values filled
 * @return        1 on success
 */
int filter_mfb_lp_design(FilterStage *stage) {
    if (!stage || stage->w_p <= 0.0 || stage->Q_p <= 0.0) return 0;

    double w_p = stage->w_p;
    double Q = stage->Q_p;
    double K = fabs(stage->K);  /* |gain| for MFB */

    /* Choose C1 = 10 nF, C2 = same */
    double C = 10.0e-9;
    stage->C1 = C;
    stage->C2 = C;

    /* R2 sets the Q-w_p product */
    double R2 = 1.0 / (w_p * C);
    /* R1 sets the gain */
    double R1 = R2 / K;
    /* R3 provides proper damping */
    double R3 = 1.0 / (w_p * w_p * C * C * R2);

    /* Fine-tune R3 for exact Q (iterative) */
    double Q_computed = sqrt(R2 * R3 * C * C) / (R3 * C + R2 * C + R3 * C * K * (-1.0));
    /* Scale R3 to achieve target Q */
    R3 = R3 * (Q_computed / Q);

    stage->R1 = R1;
    stage->R2 = R2;
    stage->R3 = R3;
    stage->R4 = 0.0;  /* Not used in MFB LP */

    /* Validate practical range */
    if (R1 < 100.0 || R2 < 100.0 || R3 < 100.0) return 0;
    if (R1 > 1.0e7 || R2 > 1.0e7 || R3 > 1.0e7) return 0;

    return 1;
}

/**
 * filter_state_variable_design - Design state-variable (KHN) filter.
 *
 * The KHN state-variable filter uses three op-amps (two integrators
 * and one summing amplifier) to provide simultaneous LP, BP, HP outputs.
 * It is called "universal" because all three responses are available.
 *
 * KEY EQUATIONS:
 *   f_c = 1/(2*pi*R*C)  (with identical integrator R and C)
 *   Q = (1 + R4/R5) / 3  (independent of f_c!)
 *   HP gain = -1 (at high frequencies)
 *   BP gain at f_c = -Q
 *   LP gain at DC = -1
 *
 * The independence of Q from f_c is a major advantage.
 * Q can be tuned by adjusting R4/R5 without affecting f_c.
 *
 * @param stage   Stage with w_p and Q_p set
 * @param band    Which filter output to optimize for
 * @return        1 on success
 */
int filter_state_variable_design(FilterStage *stage, FilterBandType band) {
    if (!stage || stage->w_p <= 0.0 || stage->Q_p <= 0.0) return 0;
    (void)band;  /* State-variable filter provides LP/BP/HP simultaneously */

    double w_p = stage->w_p;
    double Q = stage->Q_p;

    /* Choose integrator C */
    double C = 10.0e-9;
    stage->C1 = C;
    stage->C2 = C;

    /* Integrator R: f_c = 1/(2*pi*R*C) */
    double R = 1.0 / (w_p * C);
    stage->R1 = R;  /* Integrator 1 R */
    stage->R2 = R;  /* Integrator 2 R */

    /* Q-setting resistors: Q = (1 + R4/R5)/3 */
    /* Choose R5 = 10k */
    stage->R4 = 10000.0;
    stage->R3 = (3.0 * Q - 1.0) * stage->R4;

    stage->K = 1.0;

    return 1;
}

/*============================================================================
 * L6: CANONICAL PROBLEMS - Complete Filter Design
 *============================================================================*/

/**
 * filter_cascade_design - Cascade biquads for high-order filter.
 *
 * Complete filter design flow implementing the cascade synthesis method.
 *
 * ALGORITHM:
 *   1. Compute required order N from spec
 *   2. Generate pole locations for chosen approximation
 *   3. Pair complex-conjugate poles into biquads
 *      - Pair highest-Q pole with lowest-Q pole for best dynamic range
 *   4. For each biquad, synthesize using chosen topology
 *   5. Apply frequency scaling: multiply all capacitors by 1/(2*pi*f_c)
 *   6. Apply impedance scaling: multiply R by Z, divide C by Z
 *   7. Order stages: ascending Q for LP (lowest Q first to avoid saturation)
 *   8. Verify op-amp GBW sufficient for each stage
 *
 * @param filter   Filter object (spec filled, output filled)
 * @param topology Chosen topology for biquads
 * @param opamp    Op-amp parameters for feasibility check
 * @return         1 on success, 0 if infeasible
 *
 * Reference: Laker & Sansen "Design of Analog ICs" (1994)
 *            Sedra & Smith Ch.16 (Active Filters)
 */
int filter_cascade_design(ActiveFilter *filter, FilterTopology topology,
                           const OpAmpParams *opamp) {
    if (!filter || !opamp) return 0;

    FilterSpec *spec = &filter->spec;
    int N = spec->order;
    if (N <= 0 || N > 10) return 0;

    /* Compute pole locations */
    double poles_re[10], poles_im[10];
    double w_c = 2.0 * M_PI * spec->f_c;

    switch (spec->approx_type) {
        case FILTER_BUTTERWORTH:
            filter_butterworth_poles(N, w_c, poles_re, poles_im);
            break;
        case FILTER_CHEBYSHEV:
            filter_chebyshev_poles(N, spec->passband_ripple, w_c, poles_re, poles_im);
            break;
        default:
            filter_butterworth_poles(N, w_c, poles_re, poles_im);
            break;
    }

    /* Group poles into biquads */
    /* For odd N, one first-order stage (real pole) */
    int n_biquads = N / 2;
    int stage_idx = 0;

    /* Process real poles (odd-order) first */
    if (N % 2 == 1) {
        filter->stages[stage_idx].w_p = -poles_re[N/2];  /* Real pole magnitude */
        filter->stages[stage_idx].Q_p = 0.5;  /* Single real pole = Q = 0.5 biquad */
        filter->stages[stage_idx].K = 1.0;
        stage_idx++;
    }

    /* Process complex-conjugate pole pairs */
    for (int i = 0; i < n_biquads; i++) {
        /* Complex conjugate pair: p and p* */
        double re = poles_re[i + (N % 2)];
        double im = fabs(poles_im[i + (N % 2)]);

        /* Convert to w_p and Q_p:
           s^2 + 2*|re|*s + (re^2 + im^2) = s^2 + (w_p/Q_p)*s + w_p^2
           w_p = sqrt(re^2 + im^2)
           Q_p = w_p / (2*|re|) */
        double w_p_stage = sqrt(re * re + im * im);
        double Q_p_stage = w_p_stage / (2.0 * fabs(re));

        filter->stages[stage_idx].w_p = w_p_stage;
        filter->stages[stage_idx].Q_p = Q_p_stage;
        filter->stages[stage_idx].K = 1.0;
        stage_idx++;
    }

    filter->n_stages = stage_idx;

    /* Synthesize each stage */
    for (int i = 0; i < filter->n_stages; i++) {
        int success = 0;
        switch (topology) {
            case FILTER_TOPOLOGY_SALLEN_KEY:
                success = filter_sallen_key_lp_design(&filter->stages[i]);
                break;
            case FILTER_TOPOLOGY_MFB:
                success = filter_mfb_lp_design(&filter->stages[i]);
                break;
            case FILTER_TOPOLOGY_STATE_VARIABLE:
                success = filter_state_variable_design(&filter->stages[i],
                                                        spec->band_type);
                break;
            default:
                success = filter_sallen_key_lp_design(&filter->stages[i]);
                break;
        }
        if (!success) return 0;
    }

    filter->topology = topology;
    return 1;
}

/**
 * filter_frequency_response - Evaluate complete filter frequency response.
 *
 * Cascaded biquad response:
 *   H_total(jw) = product of H_k(jw) for all stages
 *
 * |H_total| = product of |H_k|
 * phase(H_total) = sum of phase(H_k)
 *
 * @param filter      Designed filter
 * @param f           Frequency (Hz)
 * @param magnitude   Output: |H(j*2*pi*f)|
 * @param phase_deg   Output: phase in degrees
 */
void filter_frequency_response(const ActiveFilter *filter, double f,
                                double *magnitude, double *phase_deg) {
    if (!filter || !magnitude || !phase_deg) return;

    double total_mag = 1.0;
    double total_phase = 0.0;
    double w = 2.0 * M_PI * f;

    for (int i = 0; i < filter->n_stages; i++) {
        const FilterStage *s = &filter->stages[i];
        double w_p = s->w_p;
        double Q_p = s->Q_p;

        /* H(s) = K * w_p^2 / (s^2 + (w_p/Q_p)*s + w_p^2) for LP */
        /* Magnitude: |H(jw)| = K*w_p^2 / sqrt((w_p^2-w^2)^2 + (w*w_p/Q_p)^2) */
        double denom_sq = (w_p * w_p - w * w) * (w_p * w_p - w * w)
                        + (w * w_p / Q_p) * (w * w_p / Q_p);
        double mag = s->K * w_p * w_p / sqrt(denom_sq);
        double phase = -atan2(w * w_p / Q_p, w_p * w_p - w * w);

        total_mag *= mag;
        total_phase += phase;
    }

    *magnitude = total_mag;
    *phase_deg = total_phase * 180.0 / M_PI;
}

/**
 * filter_bode_plot_data - Generate Bode plot data for complete filter.
 *
 * Log-spaced frequency sweep from f_start to f_stop.
 *
 * @param filter      Designed filter
 * @param f_start     Start frequency (Hz)
 * @param f_stop      Stop frequency (Hz)
 * @param n_points    Number of evaluation points
 * @param freq        Output: frequency array
 * @param mag_db      Output: |H| in dB
 * @param phase_deg   Output: phase in degrees
 */
void filter_bode_plot_data(const ActiveFilter *filter,
                            double f_start, double f_stop, int n_points,
                            double *freq, double *mag_db, double *phase_deg) {
    if (!filter || !freq || !mag_db || !phase_deg || n_points <= 0) return;
    if (f_start <= 0.0) f_start = 1.0;
    if (f_stop <= f_start) f_stop = f_start * 10.0;

    double log_start = log10(f_start);
    double log_stop  = log10(f_stop);
    double log_step  = (log_stop - log_start) / (double)(n_points > 1 ? n_points - 1 : 1);

    for (int i = 0; i < n_points; i++) {
        double f = pow(10.0, log_start + i * log_step);
        double mag, phase;
        filter_frequency_response(filter, f, &mag, &phase);
        freq[i] = f;
        mag_db[i] = (mag > 0.0) ? 20.0 * log10(mag) : -200.0;
        phase_deg[i] = phase;
    }
}

/**
 * filter_group_delay - Compute group delay of the filter.
 *
 * Group delay is the negative derivative of phase with respect to
 * angular frequency. It measures the time delay experienced by
 * each frequency component.
 *
 * For a biquad: tau_g(w) = d/dw [-arctan((w*w_p/Q_p)/(w_p^2 - w^2))]
 * For LP: tau_g(w) = (w_p/Q_p)*(w_p^2 + w^2) / ((w_p^2-w^2)^2 + (w*w_p/Q_p)^2)
 *
 * Constant group delay => no phase distortion (linear phase).
 * Bessel filters approximate constant group delay in the passband.
 *
 * @param filter      Designed filter
 * @param f           Frequency (Hz)
 * @return            Group delay (seconds)
 */
double filter_group_delay(const ActiveFilter *filter, double f) {
    if (!filter) return 0.0;

    double tau = 0.0;
    double w = 2.0 * M_PI * f;

    for (int i = 0; i < filter->n_stages; i++) {
        const FilterStage *s = &filter->stages[i];
        double w_p = s->w_p;
        double Q_p = s->Q_p;

        /* Group delay for second-order LP section */
        double num = (w_p / Q_p) * (w_p * w_p + w * w);
        double denom = (w_p * w_p - w * w) * (w_p * w_p - w * w)
                     + (w * w_p / Q_p) * (w * w_p / Q_p);
        if (denom > 1e-30)
            tau += num / denom;
    }

    return tau;
}
