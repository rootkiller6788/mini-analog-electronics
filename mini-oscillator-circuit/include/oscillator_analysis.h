/**
 * @file    oscillator_analysis.h
 * @brief   Oscillator Analysis — L3 Math Structures + L4 Fundamental Laws + L5 Algorithms
 *
 * @details Phase noise analysis (Leeson's formula, Hajimiri model), frequency
 *          stability analysis, Bode plot analysis for Barkhausen, THD analysis,
 *          and transient start-up simulation (Van der Pol numerical solution).
 *          This module provides the analytical backbone used across all
 *          oscillator topologies.
 *
 * Knowledge Mapping:
 *   L1 - Definitions:
 *     - Phase noise L(Δf) dBc/Hz
 *     - Allan variance σ²y(τ)
 *     - Jitter (cycle-to-cycle, period, RMS)
 *     - Amplitude noise AM vs phase noise PM
 *   L2 - Core Concepts:
 *     - Noise sources in oscillators (thermal, shot, flicker)
 *     - Noise shaping by the resonator (Leeson effect)
 *     - Oscillator as a nonlinear dynamical system
 *   L3 - Mathematical Structures:
 *     - Van der Pol equation: d²x/dt² - μ(1-x²)dx/dt + ω₀²x = 0
 *     - Phase space / limit cycle analysis
 *     - LTI phase noise model (Leeson)
 *     - Linear time-varying (LTV) model (Hajimiri)
 *   L4 - Fundamental Laws:
 *     - Leeson's phase noise formula (1966)
 *     - Hajimiri's impulse sensitivity function (ISF) (1998)
 *     - Nyquist stability criterion for oscillator loop
 *   L5 - Algorithms/Methods:
 *     - Phase noise computation
 *     - Bode plot generation and Barkhausen verification
 *     - Van der Pol numerical integration (Runge-Kutta 4)
 *     - THD computation from Fourier coefficients
 *   L6 - Canonical Problems:
 *     - Compute phase noise of a 10 MHz Colpitts oscillator
 *     - Solve Van der Pol equation numerically and plot limit cycle
 *
 * Reference:
 *   Leeson, "A Simple Model of Feedback Oscillator Noise Spectrum" (1966)
 *   Hajimiri & Lee, "A General Theory of Phase Noise" (1998)
 *   Demir, Mehrotra, Roychowdhury, "Phase Noise in Oscillators" (2000)
 */

#ifndef OSCILLATOR_ANALYSIS_H
#define OSCILLATOR_ANALYSIS_H

#include "oscillator_core.h"

/* ─── L3: Van der Pol Oscillator ──────────────────────────────────────────── */

/**
 * @brief State of the Van der Pol oscillator at a time instant.
 *
 *        Van der Pol equation:  d²x/dt² - μ(1 - x²)·dx/dt + ω₀²·x = 0
 *
 *        State-space form:
 *          dx/dt = y
 *          dy/dt = μ(1 - x²)·y - ω₀²·x
 *
 *        μ = 0: linear harmonic oscillator (sinusoidal)
 *        μ small: quasi-sinusoidal (limit cycle near unit circle)
 *        μ large: relaxation oscillation (fast/slow dynamics)
 */
typedef struct {
    double x;                     /**< Displacement / normalized voltage                */
    double y;                     /**< Velocity / normalized derivative                 */
    double t;                     /**< Time (seconds)                                   */
} van_der_pol_state_t;

/**
 * @brief Van der Pol oscillator parameters.
 */
typedef struct {
    double mu;                    /**< Nonlinearity parameter μ (dimensionless)          */
    double omega0;                /**< Natural frequency ω₀ (rad/s)                     */
    double freq_hz;               /**< f₀ = ω₀/(2π) (Hz)                               */
    double period_sec;            /**< T = 2π/ω₀ for μ=0 (seconds)                      */
    double amplitude_limit;       /**< Limit cycle amplitude ≈ 2 for small μ            */
    int    is_relaxation;         /**< 1 if μ > 10 (relaxation regime), 0 if small μ    */
} van_der_pol_params_t;

/**
 * @brief Van der Pol simulation result (trajectory over time).
 */
typedef struct {
    van_der_pol_params_t params;  /**< Simulation parameters                             */
    size_t num_points;            /**< Number of time steps                              */
    double *time;                 /**< Time array (seconds), length num_points           */
    double *x;                    /**< x(t) values, length num_points                    */
    double *y;                    /**< y(t) values, length num_points                    */
    double steady_state_amplitude;/**< Steady-state limit cycle amplitude                */
    double period_measured;       /**< Measured period from zero-crossings (seconds)     */
    double thd_percent;           /**< THD of x(t) from Fourier analysis                 */
} van_der_pol_sim_t;

/* ─── L4: Leeson Phase Noise Model ────────────────────────────────────────── */

/**
 * @brief Components of Leeson's phase noise formula.
 *
 *        L(Δf) = 10·log₁₀{ (F·k·T / 2·P_sig) · [1 + (f₀/(2·Q_L·Δf))²] · (1 + f_c/|Δf|) }
 *
 *        Regions:
 *          1. |Δf| < f_c:  1/f³ noise (flicker FM), slopes -30 dB/dec
 *          2. f_c < |Δf| < f₀/(2Q):  1/f² noise (thermal FM), slopes -20 dB/dec
 *          3. |Δf| > f₀/(2Q):  flat noise floor, 0 dB/dec
 */
typedef struct {
    double freq_carrier_hz;       /**< Carrier frequency f₀ (Hz)                        */
    double freq_offset_hz;        /**< Frequency offset from carrier Δf (Hz)             */
    double boltzmann_k;           /**< Boltzmann constant (J/K), 1.380649e-23            */
    double temperature_k;         /**< Temperature (K), 290 = 17°C                       */
    double noise_factor;          /**< Noise factor F of active device (linear, >= 1)    */
    double signal_power_w;        /**< Signal power at tank P_sig (W)                    */
    double loaded_q;              /**< Loaded Q of resonator                             */
    double flicker_corner_hz;     /**< 1/f noise corner frequency f_c (Hz)               */
    double phase_noise_dbc_hz;    /**< Computed L(Δf) (dBc/Hz)                           */
    double floor_noise_dbc_hz;    /**< Noise floor = 10·log₁₀(F·k·T/(2·P_sig))          */
    double corner_f0_2q_hz;       /**< f₀/(2Q) corner frequency (Hz)                     */
} leeson_phase_noise_t;

/* ─── L4: Hajimiri LTV Phase Noise Model ──────────────────────────────────── */

/**
 * @brief Hajimiri impulse sensitivity function (ISF) based phase noise model.
 *
 *        The ISF Γ(ω₀τ) characterizes the sensitivity of the oscillator's
 *        phase to an impulse injected at phase τ. The effective ISF Γ_rms
 *        determines the 1/f² phase noise region.
 *
 *        L(Δf) = 10·log₁₀( Γ²_rms · i²_n/Δf / (2·q²_max·Δω²) )
 *
 *        where q_max = maximum charge swing across the tank capacitor,
 *        i²_n/Δf = noise current power spectral density.
 */
typedef struct {
    double gamma_rms;             /**< RMS value of the ISF (dimensionless)             */
    double q_max_coulombs;        /**< Maximum charge swing q_max (Coulombs)            */
    double noise_current_psd;     /**< Noise current PSD i²_n/Δf (A²/Hz)                */
    double freq_offset_hz;        /**< Frequency offset Δf (Hz)                          */
    double phase_noise_dbc_hz;    /**< L(Δf) from LTV model (dBc/Hz)                     */
    double isf_dc;                /**< DC component of ISF (affects 1/f³ noise)         */
    double flicker_pn_dbc_hz;     /**< 1/f³ phase noise component (dBc/Hz)              */
} hajimiri_phase_noise_t;

/* ─── L5: Bode Plot Analysis for Oscillators ──────────────────────────────── */

/**
 * @brief Bode plot data for loop gain T(jω) = A(jω)·β(jω).
 *
 *        Used to graphically verify the Barkhausen criterion and assess
 *        stability margins.
 */
typedef struct {
    size_t num_points;            /**< Number of frequency points                      */
    double *freq_hz;              /**< Frequency sweep (Hz)                            */
    double *magnitude_db;         /**< |T(jω)| in dB                                   */
    double *phase_deg;            /**< ∠T(jω) in degrees                               */
    double phase_at_0db_hz;       /**< Frequency where |T| = 0 dB (Hz)                 */
    double gain_at_0deg_db;       /**< |T| at ∠T = 0° (dB)                             */
    double phase_margin_deg;      /**< Phase margin = 180° + ∠T at 0 dB crossing       */
    double gain_margin_db;        /**< Gain margin = -|T| at 180° phase crossing       */
} bode_analysis_t;

/* ─── L1: Allan Variance ─────────────────────────────────────────────────── */

/**
 * @brief Allan variance for oscillator frequency stability characterization.
 *
 *        σ²_y(τ) = 1/(2(M-1)) · Σ_{i=1}^{M-1} (ȳ_{i+1} - ȳ_i)²
 *
 *        where ȳ_i is the average fractional frequency over the i-th
 *        observation interval of length τ.
 *
 *        Allan deviation σ_y(τ) reveals:
 *          - White FM:    σ_y ∝ τ^{-1/2}
 *          - Flicker FM:  σ_y ≈ constant
 *          - Random walk FM: σ_y ∝ τ^{1/2}
 */
typedef struct {
    double tau_sec;               /**< Observation interval τ (seconds)                 */
    double allan_variance;        /**< σ²_y(τ) (dimensionless)                          */
    double allan_deviation;       /**< σ_y(τ) = √(σ²_y) (dimensionless)                */
    double stability_ppb;         /**< σ_y(τ) expressed in ppb (×10⁻⁹)                  */
} allan_variance_t;

/* ─── L1: Jitter Metrics ─────────────────────────────────────────────────── */

/**
 * @brief Oscillator jitter metrics in both time and frequency domains.
 *
 *        Period jitter:    standard deviation of individual period measurements
 *        Cycle-to-cycle:   std dev of difference between consecutive periods
 *        RMS phase jitter: integrated phase noise converted to time domain
 *        TIE:              time interval error (accumulated jitter)
 */
typedef struct {
    double period_jitter_ps;      /**< RMS period jitter (ps)                           */
    double cycle_to_cycle_jitter_ps;/**< RMS cycle-to-cycle jitter (ps)                 */
    double rms_phase_jitter_ps;   /**< RMS phase jitter from integrated PN (ps)         */
    double rms_phase_jitter_rad;  /**< RMS phase jitter (radians)                       */
    double tie_at_1ms_ps;         /**< TIE at 1 ms observation (ps)                     */
    double phase_noise_integ_min_hz;/**< Lower integration limit (Hz)                   */
    double phase_noise_integ_max_hz;/**< Upper integration limit (Hz)                   */
} jitter_metrics_t;

/* ─── Function Declarations ───────────────────────────────────────────────── */

/* ─── Van der Pol (L3-L6) ─────────────────────────────────────────────────── */

/**
 * @brief Initialize Van der Pol oscillator parameters.
 *
 * @param mu         Nonlinearity parameter
 * @param freq_hz    Natural frequency (Hz)
 * @return           Initialized parameters
 */
van_der_pol_params_t  van_der_pol_init(double mu, double freq_hz);

/**
 * @brief Compute the derivative of the Van der Pol oscillator at a state.
 *
 *        dx/dt = y
 *        dy/dt = μ(1 - x²)·y - ω₀²·x
 *
 * @param state   Current state (x, y, t)
 * @param params  Oscillator parameters
 * @param dx      [out] dx/dt
 * @param dy      [out] dy/dt
 */
void  van_der_pol_derivative(const van_der_pol_state_t *state,
                              const van_der_pol_params_t *params,
                              double *dx, double *dy);

/**
 * @brief Simulate the Van der Pol oscillator using RK4 integration.
 *
 *        Integrates from t=0 to t=duration with step size dt.
 *
 * @param params       Oscillator parameters
 * @param x0           Initial condition x(0)
 * @param y0           Initial condition y(0)
 * @param duration_sec Total simulation time (s)
 * @param dt_sec       Time step (s), typically T/1000 or smaller
 * @return             Simulation result (caller must free)
 */
van_der_pol_sim_t  van_der_pol_simulate(const van_der_pol_params_t *params,
                                           double x0, double y0,
                                           double duration_sec, double dt_sec);

/**
 * @brief Free Van der Pol simulation result.
 */
void  van_der_pol_sim_free(van_der_pol_sim_t *sim);

/* ─── Phase Noise (L4-L5) ─────────────────────────────────────────────────── */

/**
 * @brief Compute phase noise using Leeson formula.
 *
 * @param carrier_hz    Carrier frequency f₀ (Hz)
 * @param offset_hz     Offset frequency Δf (Hz)
 * @param loaded_q      Loaded quality factor Q
 * @param signal_power_w  Signal power P_s (W)
 * @param noise_factor  Device noise factor F (>= 1)
 * @param flicker_hz    1/f corner frequency (Hz), or 0
 * @param temp_k        Temperature (K), or 290 for room temp
 * @return              Leeson phase noise result
 */
leeson_phase_noise_t  leeson_phase_noise_compute(double carrier_hz,
                                                    double offset_hz,
                                                    double loaded_q,
                                                    double signal_power_w,
                                                    double noise_factor,
                                                    double flicker_hz,
                                                    double temp_k);

/**
 * @brief Compute phase noise using Hajimiri LTV model.
 *
 * @param gamma_rms       RMS ISF
 * @param q_max_coulombs  Max charge swing (C)
 * @param noise_psd       Noise current PSD (A²/Hz)
 * @param offset_hz       Frequency offset (Hz)
 * @param isf_dc          DC component of ISF (for 1/f³ noise)
 * @param flicker_hz      1/f corner frequency (Hz)
 * @return                Hajimiri phase noise result
 */
hajimiri_phase_noise_t  hajimiri_phase_noise_compute(double gamma_rms,
                                                       double q_max_coulombs,
                                                       double noise_psd,
                                                       double offset_hz,
                                                       double isf_dc,
                                                       double flicker_hz);

/**
 * @brief Convert phase noise to RMS jitter (integrated over a frequency band).
 *
 *        σ_t = (1/2πf₀)·√(2·∫_{f_low}^{f_high} 10^{L(f)/10} df)
 *
 * @param carrier_hz    Carrier frequency (Hz)
 * @param offset_start_hz  Lower integration limit (Hz)
 * @param offset_end_hz    Upper integration limit (Hz)
 * @param pn_dbc_hz     Array of phase noise values (length num_points)
 * @param offset_hz_arr Array of offset frequencies (length num_points)
 * @param num_points    Number of data points
 * @return              RMS phase jitter in seconds
 */
double  phase_noise_to_rms_jitter(double carrier_hz,
                                    double offset_start_hz,
                                    double offset_end_hz,
                                    const double *pn_dbc_hz,
                                    const double *offset_hz_arr,
                                    size_t num_points);

/* ─── Bode Analysis (L5) ──────────────────────────────────────────────────── */

/**
 * @brief Create a logarithmically-spaced frequency sweep for Bode analysis.
 *
 * @param freq_start_hz  Start frequency (Hz)
 * @param freq_end_hz    End frequency (Hz)
 * @param points_per_decade  Resolution (e.g., 100)
 * @return               Frequency sweep array (caller frees)
 */
bode_analysis_t  bode_sweep_create(double freq_start_hz, double freq_end_hz,
                                      size_t points_per_decade);

/**
 * @brief Analyze a transfer function over a frequency sweep (Bode plot).
 *
 *        Evaluates T(s) at s = jω for each point in the sweep.
 *        The user provides a callback that computes T(jω).
 *
 * @param bode       [in/out] Bode sweep (freq_hz pre-filled)
 * @param transfer_fn User function: void fn(double w, double *mag, double *phase_deg)
 * @param user_data  Opaque pointer passed to transfer_fn
 */
void  bode_analyze(bode_analysis_t *bode,
                    void (*transfer_fn)(double w, double *mag, double *phase_deg, void *user_data),
                    void *user_data);

/**
 * @brief Find the frequency where phase crosses 0° and where magnitude crosses 0 dB.
 *
 *        Uses linear interpolation between sweep points.
 *
 * @param bode  [in/out] Bode data — osc_freq and margins are filled in
 */
void  bode_find_crossings(bode_analysis_t *bode);

/**
 * @brief Free Bode analysis data.
 */
void  bode_analysis_free(bode_analysis_t *bode);

/* ─── THD Analysis (L5) ───────────────────────────────────────────────────── */

/**
 * @brief Compute Total Harmonic Distortion from amplitude spectrum.
 *
 *        THD = √(Σ_{k=2}^{N} A_k²) / A₁ · 100%
 *
 *        where A_k is the amplitude of the k-th harmonic.
 *
 * @param amplitudes   Array of harmonic amplitudes (A₁ at index 1, etc.)
 * @param num_harmonics Number of harmonics (including fundamental)
 * @return             THD in percent
 */
double  thd_compute(const double *amplitudes, size_t num_harmonics);

/* ─── Frequency Stability (L5) ────────────────────────────────────────────── */

/**
 * @brief Compute Allan variance from a series of fractional frequency measurements.
 *
 * @param freq_measurements  Array of average fractional frequency y_i (length M)
 * @param M                  Number of measurements
 * @param tau_sec            Observation interval (seconds)
 * @return                   Allan variance result
 */
allan_variance_t  allan_variance_compute(const double *freq_measurements,
                                            size_t M, double tau_sec);

/**
 * @brief Compute jitter metrics from phase noise data.
 *
 * @param carrier_hz    Carrier frequency (Hz)
 * @param pn_dbc_hz     Phase noise array (dBc/Hz)
 * @param offset_hz     Offset frequency array (Hz)
 * @param num_points    Number of data points
 * @return              Jitter metrics
 */
jitter_metrics_t  jitter_from_phase_noise(double carrier_hz,
                                            const double *pn_dbc_hz,
                                            const double *offset_hz,
                                            size_t num_points);

/**
 * @brief Compute frequency stability over temperature range.
 *
 * @param params        Oscillator parameters
 * @param stability     [in/out] Stability data (fill in temp range + tolerances)
 */
void  frequency_stability_analyze(const oscillator_params_t *params,
                                    frequency_stability_t *stability);

#endif /* OSCILLATOR_ANALYSIS_H */
