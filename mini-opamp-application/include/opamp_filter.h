#ifndef OPAMP_FILTER_H
#define OPAMP_FILTER_H

/**
 * opamp_filter.h - Active Filter Design with Operational Amplifiers
 *
 * Comprehensive implementation of active RC filter topologies using
 * operational amplifiers. Covers Sallen-Key, Multiple Feedback (MFB),
 * state-variable, and Tow-Thomas biquad topologies.
 *
 * Knowledge Coverage:
 *   L1 (Definitions): FilterSpec, FilterType, FilterTopology enums/structs
 *   L2 (Core Concepts): Active vs passive filtering, Q-factor, damping
 *   L3 (Math Structures): Butterworth, Chebyshev, Bessel, elliptic polynomials
 *   L4 (Fundamental Laws): Filter approximation theory, sensitivity functions
 *   L5 (Algorithms/Methods): Filter synthesis, component value computation
 *   L6 (Canonical Problems): Low-pass, high-pass, band-pass, band-stop design
 *
 * Reference: Franco "Design with Op Amps and Analog ICs" Ch.4-5
 *            Van Valkenburg "Analog Filter Design" (1982)
 *            TI SLOA049 (Active Filter Design Techniques)
 *            Analog Devices MT-210 (Sallen-Key Filters)
 *            Deliyannis "Continuous-Time Active Filter Design" (1999)
 *
 * Course: MIT 6.002, Berkeley EE105, ETH 227-0436, TUM EI
 */

#include "opamp_basic.h"

/*============================================================================
 * L1: DEFINITIONS - Filter Type and Specification
 *============================================================================*/

/** Filter approximation types */
typedef enum {
    FILTER_BUTTERWORTH = 0,     /**< Maximally flat passband, moderate roll-off */
    FILTER_CHEBYSHEV,           /**< Equal-ripple passband, steeper roll-off */
    FILTER_INVERSE_CHEBYSHEV,   /**< Equal-ripple stopband */
    FILTER_BESSEL,              /**< Maximally flat group delay (linear phase) */
    FILTER_ELLIPTIC,            /**< Equal-ripple passband and stopband (Cauer) */
    FILTER_GAUSSIAN,            /**< Gaussian magnitude, no overshoot */
    FILTER_TYPE_COUNT
} FilterApproxType;

/** Filter band type */
typedef enum {
    FILTER_BAND_LOWPASS = 0,
    FILTER_BAND_HIGHPASS,
    FILTER_BAND_BANDPASS,
    FILTER_BAND_BANDSTOP,
    FILTER_BAND_ALLPASS,
    FILTER_BAND_COUNT
} FilterBandType;

/** Active filter topologies */
typedef enum {
    FILTER_TOPOLOGY_SALLEN_KEY = 0,  /**< Sallen-Key (VCVS), positive feedback */
    FILTER_TOPOLOGY_MFB,             /**< Multiple Feedback (Rauch) */
    FILTER_TOPOLOGY_STATE_VARIABLE,  /**< State-variable (KHN) universal */
    FILTER_TOPOLOGY_TOW_THOMAS,      /**< Tow-Thomas biquad */
    FILTER_TOPOLOGY_AKERBERG_MOSSBERG, /**< Akerberg-Mossberg (fully differential) */
    FILTER_TOPOLOGY_COUNT
} FilterTopology;

/**
 * FilterSpec - Complete active filter design specification.
 *
 * Defines the target performance parameters that any filter design
 * must achieve.
 */
typedef struct {
    FilterBandType band_type;       /**< Lowpass, highpass, etc. */
    FilterApproxType approx_type;   /**< Butterworth, Chebyshev, etc. */
    int order;                      /**< Filter order N (1-10) */
    double f_c;                     /**< Cutoff/center frequency (Hz) */
    double f_s;                     /**< Stopband frequency (Hz) */
    double passband_ripple;         /**< Passband ripple (dB), Chebyshev */
    double stopband_attenuation;    /**< Minimum stopband attenuation (dB) */
    double passband_gain;           /**< Passband gain (V/V), default 1.0 */
    double Q;                       /**< Quality factor (for BP/BS) */
    double BW;                      /**< Bandwidth (Hz) for BP/BS */
} FilterSpec;

/**
 * FilterStage - Single second-order biquad stage parameters.
 *
 * Each biquad implements:
 *   H(s) = K * (s^2 + w_z*s/Q_z + w_z^2) / (s^2 + w_p*s/Q_p + w_p^2)
 *
 * High-order filters are cascaded biquads.
 */
typedef struct {
    double w_p;                     /**< Pole frequency (rad/s) */
    double Q_p;                     /**< Pole quality factor */
    double w_z;                     /**< Zero frequency (rad/s) */
    double Q_z;                     /**< Zero quality factor */
    double K;                       /**< Stage gain (V/V) */
    /* Component values after synthesis */
    double R1, R2, R3, R4;          /**< Resistor values (Ohm) */
    double C1, C2;                  /**< Capacitor values (F) */
} FilterStage;

/**
 * ActiveFilter - Complete active filter design result.
 */
typedef struct {
    FilterSpec spec;                /**< Design specification */
    FilterTopology topology;        /**< Chosen topology */
    int n_stages;                   /**< Number of biquad stages */
    FilterStage stages[10];         /**< Biquad stages (max order 20) */
    double total_Q;                 /**< Overall filter Q */
    FilterStage *active_stages;     /**< Pointer to active stages */
} ActiveFilter;

/*============================================================================
 * L2: CORE CONCEPTS - Filter Fundamentals
 *============================================================================*/

/**
 * filter_order_for_attenuation - Compute minimum Butterworth order.
 *
 * For Butterworth: N >= log10((10^(As/10) - 1) / (10^(Ap/10) - 1)) / (2*log10(f_s/f_p))
 * where As = stopband attenuation (dB), Ap = passband ripple (dB).
 *
 * @param As     Stopband attenuation (dB)
 * @param f_s    Stopband frequency (Hz)
 * @param f_p    Passband edge frequency (Hz)
 * @return       Minimum integer order N
 *
 * Reference: Van Valkenburg "Analog Filter Design" Ch.3
 */
int filter_order_for_attenuation(double As, double f_s, double f_p);

/**
 * filter_butterworth_poles - Compute Butterworth pole locations in s-plane.
 *
 * Butterworth poles lie on a unit circle in the s-plane:
 *   s_k = exp(j*(pi/2 + pi/(2N) + k*pi/N)) for k = 0, 1, ..., N-1
 *
 * The poles are in the left-half plane (negative real part), ensuring stability.
 *
 * @param order     Filter order
 * @param w_c       Cutoff frequency (rad/s)
 * @param poles_re  Output: real parts of poles
 * @param poles_im  Output: imaginary parts of poles
 *
 * Reference: Butterworth "On the Theory of Filter Amplifiers" (1930)
 */
void filter_butterworth_poles(int order, double w_c,
                               double *poles_re, double *poles_im);

/**
 * filter_butterworth_polynomial - Compute Butterworth denominator polynomial.
 *
 * B_n(s) = product_{k=1..n} (s - p_k) where p_k are Butterworth poles.
 *
 * First few Butterworth polynomials:
 *   B1: s + 1
 *   B2: s^2 + 1.4142s + 1
 *   B3: s^3 + 2s^2 + 2s + 1  = (s+1)(s^2+s+1)
 *   B4: s^4 + 2.6131s^3 + 3.4142s^2 + 2.6131s + 1
 *
 * @param order     Filter order
 * @param coeffs    Output: polynomial coefficients (a0 + a1*s + ... + aN*s^N)
 *                  coeffs[0] = constant term, coeffs[N] = s^N coefficient
 */
void filter_butterworth_polynomial(int order, double *coeffs);

/**
 * filter_chebyshev_poles - Compute Chebyshev Type I pole locations.
 *
 * Poles lie on an ellipse in the s-plane:
 *   s_k = -sinh(eta)*sin(theta_k) + j*cosh(eta)*cos(theta_k)
 *   where eta = asinh(1/epsilon)/N, epsilon = sqrt(10^(ripple/10) - 1)
 *         theta_k = pi/2 + (2k+1)*pi/(2N)
 *
 * @param order        Filter order
 * @param ripple_db    Passband ripple (dB)
 * @param w_c          Cutoff frequency (rad/s)
 * @param poles_re     Output: real parts
 * @param poles_im     Output: imaginary parts
 *
 * Reference: Chebyshev filter theory, Daniels "Approximation Methods..." (1974)
 */
void filter_chebyshev_poles(int order, double ripple_db, double w_c,
                             double *poles_re, double *poles_im);

/*============================================================================
 * L3: MATHEMATICAL STRUCTURES - Biquad Transfer Functions
 *============================================================================*/

/**
 * filter_biquad_coefficients - Compute normalized biquad coefficients.
 *
 * For a second-order filter section:
 *   H(s) = (a2*s^2 + a1*s + a0) / (s^2 + (w_p/Q_p)*s + w_p^2)
 *
 * Standard forms:
 *   Low-pass:  H(s) = H0 * w_p^2 / (s^2 + (w_p/Q_p)*s + w_p^2)
 *   High-pass: H(s) = H0 * s^2 / (s^2 + (w_p/Q_p)*s + w_p^2)
 *   Band-pass: H(s) = H0 * (w_p/Q_p)*s / (s^2 + (w_p/Q_p)*s + w_p^2)
 *   Band-stop: H(s) = H0 * (s^2 + w_z^2) / (s^2 + (w_p/Q_p)*s + w_p^2)
 *
 * @param band_type  Lowpass, highpass, bandpass, bandstop
 * @param w_p        Pole frequency (rad/s)
 * @param Q_p        Pole Q
 * @param K          DC gain
 * @param a_coeffs   Output: [a0, a1, a2] numerator coefficients
 * @param b_coeffs   Output: [b0, b1, b2] denominator (b2 = 1 always)
 */
void filter_biquad_coefficients(FilterBandType band_type,
                                 double w_p, double Q_p, double K,
                                 double *a_coeffs, double *b_coeffs);

/*============================================================================
 * L4: FUNDAMENTAL LAWS - Sensitivity Theory
 *============================================================================*/

/**
 * filter_sensitivity_wp - Compute sensitivity of pole frequency to components.
 *
 * The sensitivity function S_x^y = (dx/x) / (dy/y) = (partial y / partial x) * (x/y)
 *
 * For Sallen-Key low-pass:
 *   w_p = 1 / sqrt(R1*R2*C1*C2)
 *   S_R1^wp = S_R2^wp = S_C1^wp = S_C2^wp = -1/2
 *
 * Lower sensitivity => better manufacturability.
 *
 * @param topology    Filter topology
 * @param stage       Filter stage with component values
 * @param S_R1_wp     Output: sensitivity to R1
 * @param S_R2_wp     Output: sensitivity to R2
 * @param S_C1_wp     Output: sensitivity to C1
 * @param S_C2_wp     Output: sensitivity to C2
 *
 * Reference: Moschytz "Linear Integrated Networks: Fundamentals" (1974)
 */
void filter_sensitivity_wp(FilterTopology topology, const FilterStage *stage,
                            double *S_R1_wp, double *S_R2_wp,
                            double *S_C1_wp, double *S_C2_wp);

/**
 * filter_sensitivity_Q - Compute sensitivity of Q to passive components.
 *
 * For Sallen-Key with gain K:
 *   Q = 1 / (3 - K)    (for equal-R, equal-C design)
 *
 * As K -> 3, Q -> infinity (oscillation). The Q-sensitivity to K is:
 *   S_K^Q = K / (3 - K) — extremely high near K = 3
 *
 * This is why Sallen-Key is limited to moderate Q (< 10).
 *
 * @param topology    Filter topology
 * @param stage       Filter stage
 * @param S_R_Q       Component Q-sensitivities (6-element output)
 */
void filter_sensitivity_Q(FilterTopology topology, const FilterStage *stage,
                           double S_comp_Q[6]);

/*============================================================================
 * L5: ALGORITHMS/METHODS - Filter Synthesis
 *============================================================================*/

/**
 * filter_sallen_key_lp_design - Design Sallen-Key low-pass filter stage.
 *
 * Design equations (equal-component design):
 *   Choose C1 = C2 = C (standard value)
 *   R = 1 / (2*pi*f_c * C)
 *   R1 = R2 = R for Q = 0.5 (Butterworth)
 *   For arbitrary Q: R1 = R * 2Q, R2 = R / (2Q)
 *   K = 3 - 1/Q (gain for desired Q)
 *
 * For unity-gain Sallen-Key (K=1):
 *   Q = 0.5 (no Q enhancement possible without gain)
 *
 * @param stage   Filter stage (f_c and Q must be set in w_p, Q_p)
 * @return        1 on success
 *
 * Reference: Sallen & Key "A Practical Method of Designing RC Active Filters" (1955)
 */
int filter_sallen_key_lp_design(FilterStage *stage);

/**
 * filter_mfb_lp_design - Design Multiple Feedback low-pass filter.
 *
 * MFB topology advantages over Sallen-Key:
 *   - Lower sensitivity to component variations
 *   - Better for high-Q designs (Q up to 20)
 *   - Inverting topology (phase inversion)
 *
 * Design equations:
 *   R2 = 1/(2*Q*w_p*C1)   (choose C1 = C, then compute R2)
 *   R1 = R2/(-2*K)
 *   R3 = -2*Q*K/(w_p*C1)
 *   C2 = C1 (standard practice)
 *
 * @param stage   Filter stage with w_p, Q_p, K set; component values filled
 * @return        1 on success
 *
 * Reference: Rauch "A simplified MFB filter" (1965)
 */
int filter_mfb_lp_design(FilterStage *stage);

/**
 * filter_state_variable_design - Design state-variable (KHN) universal filter.
 *
 * The KHN filter provides simultaneous LP, BP, HP outputs:
 *   LP: H_LP(s) = -w_p^2 / (s^2 + (w_p/Q)*s + w_p^2)
 *   BP: H_BP(s) = -(w_p/Q)*s / (s^2 + (w_p/Q)*s + w_p^2)
 *   HP: H_HP(s) = -s^2 / (s^2 + (w_p/Q)*s + w_p^2)
 *
 * With three op-amps and equal R, C:
 *   f_c = 1/(2*pi*R*C)
 *   Q = (R4+R5)/(3*R5)  (independent of f_c)
 *
 * @param stage   Filter stage (only one output type selected)
 * @param band    Which output to use
 * @return        1 on success
 *
 * Reference: Kerwin, Huelsman, Newcomb "State-Variable Synthesis..." (1967)
 */
int filter_state_variable_design(FilterStage *stage, FilterBandType band);

/*============================================================================
 * L6: CANONICAL PROBLEMS - Complete Filter Solutions
 *============================================================================*/

/**
 * filter_cascade_design - Cascade biquads to realize high-order filter.
 *
 * Design flow:
 *   1. Compute required order from spec
 *   2. Generate pole/zero locations for chosen approximation
 *   3. Pair poles into biquads (pair high-Q with low-Q for best dynamic range)
 *   4. Synthesize each biquad using chosen topology
 *   5. Order biquads by Q (ascending for LP, descending for HP)
 *   6. Apply frequency and impedance scaling
 *
 * @param filter   Filter object (spec must be filled)
 * @param topology Chosen filter topology
 * @param opamp    Op-amp parameters for bandwidth verification
 * @return         1 on success, 0 if infeasible
 *
 * Reference: Laker & Sansen "Design of Analog Integrated Circuits..." (1994)
 */
int filter_cascade_design(ActiveFilter *filter, FilterTopology topology,
                           const OpAmpParams *opamp);

/**
 * filter_frequency_response - Evaluate filter response at frequency f.
 *
 * Computes |H(jw)| and phase by cascading all biquad responses.
 *
 * @param filter     Designed active filter
 * @param f          Frequency (Hz)
 * @param magnitude  Output: |H(j*2*pi*f)|
 * @param phase_deg  Output: phase(H) in degrees
 */
void filter_frequency_response(const ActiveFilter *filter, double f,
                                double *magnitude, double *phase_deg);

/**
 * filter_bode_plot_data - Generate Bode plot data points.
 *
 * @param filter     Designed active filter
 * @param f_start    Start frequency (Hz)
 * @param f_stop     Stop frequency (Hz)
 * @param n_points   Number of points (log-spaced)
 * @param freq       Output: frequency array
 * @param mag_db     Output: magnitude in dB
 * @param phase_deg  Output: phase in degrees
 */
void filter_bode_plot_data(const ActiveFilter *filter,
                            double f_start, double f_stop, int n_points,
                            double *freq, double *mag_db, double *phase_deg);

/**
 * filter_group_delay - Compute group delay at frequency f.
 *
 * tau_g(w) = -d(phase)/dw = -d(angle(H(jw)))/dw
 *
 * Constant group delay => linear phase => no waveform distortion.
 * Bessel filters are designed for maximally flat group delay.
 *
 * @param filter     Designed active filter
 * @param f          Frequency (Hz)
 * @return           Group delay (seconds)
 */
double filter_group_delay(const ActiveFilter *filter, double f);

#endif /* OPAMP_FILTER_H */
