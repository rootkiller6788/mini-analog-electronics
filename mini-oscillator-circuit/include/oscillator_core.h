/**
 * @file    oscillator_core.h
 * @brief   Core Oscillator Definitions — L1 Definitions + L2 Core Concepts
 *
 * @details Defines fundamental data types, constants, and parameters for
 *          all oscillator families: harmonic (sinusoidal), relaxation,
 *          and crystal-based. Covers the Barkhausen criterion, oscillation
 *          conditions, and quality metrics shared across topologies.
 *
 * Knowledge Mapping:
 *   L1 - Definitions:
 *     - Oscillation frequency (f_osc)
 *     - Barkhausen criterion: |AB| = 1, ∠AB = 0 (or 2πn)
 *     - Loop gain magnitude and loop phase
 *     - Quality factor Q (energy stored / energy lost per cycle)
 *     - Duty cycle (for relaxation oscillators)
 *     - Amplitude / peak-to-peak voltage
 *     - Start-up gain margin
 *     - Phase noise (dBc/Hz at offset Δf)
 *     - Negative resistance (for LC oscillators)
 *   L2 - Core Concepts:
 *     - Positive feedback mechanism
 *     - Frequency-selective network classification
 *     - Amplitude stabilization / limiting
 *     - Start-up condition (small-signal loop gain > 1)
 *     - Steady-state condition (gain compression to exactly 1)
 *     - Harmonic distortion (THD)
 *     - Frequency stability (ppm over temperature / aging)
 *
 * Reference:
 *   Sedra & Smith, "Microelectronic Circuits" (2020), Ch.17
 *   Razavi, "Design of Analog CMOS Integrated Circuits" (2017), Ch.14
 *   Vittoz et al., "High-Performance Crystal Oscillator Circuits" (1988)
 */

#ifndef OSCILLATOR_CORE_H
#define OSCILLATOR_CORE_H

#include <stddef.h>
#include <stdint.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ─── L1: Oscillator Topology Enumeration ─────────────────────────────────── */

/**
 * @brief Oscillator family classification.
 *
 * Each topology uses a distinct frequency-selective network and feedback mechanism.
 */
typedef enum {
    OSC_TYPE_RC_PHASE_SHIFT = 0,  /**< 3-stage RC ladder + inverting amplifier                */
    OSC_TYPE_WIEN_BRIDGE,         /**< Lead-lag RC network + non-inverting amplifier (gain=3)  */
    OSC_TYPE_TWIN_T,              /**< Twin-T notch filter in negative feedback path           */
    OSC_TYPE_COLPITTS,            /**< Capacitive divider + inductor (LC oscillator)            */
    OSC_TYPE_HARTLEY,             /**< Inductive divider + capacitor (LC oscillator)            */
    OSC_TYPE_CLAPP,               /**< Colpitts variant with series capacitor for stability     */
    OSC_TYPE_ARMSTRONG,           /**< Transformer-coupled LC feedback (tickler coil)           */
    OSC_TYPE_CROSS_COUPLED_LC,    /**< Differential cross-coupled pair + LC tank                */
    OSC_TYPE_PIERCE_CRYSTAL,      /**< Crystal in π-network with inverter (most common XO)      */
    OSC_TYPE_COLPITTS_CRYSTAL,    /**< Crystal replaces inductor in Colpitts                    */
    OSC_TYPE_RELAXATION_555,      /**< 555 timer in astable configuration                      */
    OSC_TYPE_RELAXATION_SCHMITT,  /**< Schmitt-trigger RC relaxation oscillator                 */
    OSC_TYPE_RELAXATION_OPAMP,    /**< Op-amp based relaxation (integrator + Schmitt)           */
    OSC_TYPE_RING,                /**< Odd number of inverters in a loop                       */
    OSC_TYPE_VCO_LC,              /**< Voltage-controlled LC tank with varactor                */
    OSC_TYPE_VCO_RING,            /**< Voltage-controlled ring oscillator                      */
    OSC_TYPE_COUNT                /**< Sentinel: total number of oscillator types              */
} oscillator_type_t;

/* ─── L1: Oscillation Mode ────────────────────────────────────────────────── */

/**
 * @brief Oscillation mode classification.
 */
typedef enum {
    OSC_MODE_HARMONIC = 0,    /**< Sinusoidal / near-sinusoidal output (LC, crystal, Wien) */
    OSC_MODE_RELAXATION,      /**< Non-sinusoidal (square/triangle/sawtooth), RC timing     */
    OSC_MODE_INJECTION_LOCKED /**< Oscillator locked to external injection signal           */
} oscillation_mode_t;

/* ─── L1: Active Device Type ──────────────────────────────────────────────── */

/**
 * @brief Active device used in the oscillator.
 */
typedef enum {
    ACTIVE_BJT = 0,     /**< Bipolar Junction Transistor                    */
    ACTIVE_FET,         /**< Field Effect Transistor (JFET or MOSFET)       */
    ACTIVE_OPAMP,       /**< Operational Amplifier                          */
    ACTIVE_CMOS_INV,    /**< CMOS Inverter (for Pierce / ring oscillators)  */
    ACTIVE_COMPARATOR,  /**< Voltage Comparator (for relaxation oscillators) */
    ACTIVE_OTA          /**< Operational Transconductance Amplifier          */
} active_device_t;

/* ─── L1: Barkhausen Criterion Structure ──────────────────────────────────── */

/**
 * @brief Barkhausen criterion for sustained sinusoidal oscillation.
 *
 *        For a feedback loop with forward gain A(jω) and feedback
 *        network β(jω), the loop gain is L(jω) = A(jω)·β(jω).
 *
 *        Barkhausen criterion:
 *        (1) |L(jω₀)| = 1   — magnitude condition
 *        (2) ∠L(jω₀) = 0   — phase condition (or 2πn)
 *
 *        Practically, we design for |L(jω₀)| > 1 at start-up
 *        (small-signal) and let nonlinearity compress the gain
 *        to exactly 1 at steady-state.
 */
typedef struct {
    double loop_gain_magnitude;  /**< |A·β| at oscillation frequency                  */
    double loop_phase_deg;       /**< ∠(A·β) at oscillation frequency (degrees)       */
    double freq_hz;              /**< Frequency where phase condition is satisfied     */
    int    magnitude_satisfied;  /**< 1 if |L| >= 1.0, 0 otherwise                    */
    int    phase_satisfied;      /**< 1 if |∠L mod 360°| < tolerance, 0 otherwise     */
    double start_up_margin_db;   /**< Start-up gain margin = 20·log₁₀(|L|) in dB      */
} barkhausen_criterion_t;

/* ─── L1: Oscillator Parameters ───────────────────────────────────────────── */

/**
 * @brief Generic oscillator design parameters — applicable to all topologies.
 */
typedef struct {
    oscillator_type_t type;         /**< Oscillator topology type                        */
    oscillation_mode_t mode;        /**< Harmonic, relaxation, or injection-locked       */
    active_device_t active;         /**< Active device type                              */
    double target_freq_hz;          /**< Target oscillation frequency (Hz)               */
    double actual_freq_hz;          /**< Computed actual frequency (after component tol)  */
    double frequency_tolerance_ppm; /**< Frequency deviation in parts-per-million        */
    double amplitude_vpp;           /**< Peak-to-peak output amplitude (V)               */
    double dc_offset_v;             /**< DC offset of output (V)                         */
    double supply_voltage_v;        /**< Supply voltage VDD or VCC (V)                   */
    double supply_current_ma;       /**< Supply current draw (mA)                        */
    double power_dissipation_mw;    /**< Power dissipation (mW)                          */
    double startup_time_us;         /**< Estimated start-up time (µs)                    */
    double thd_percent;             /**< Total harmonic distortion (%)                   */
    double phase_noise_dbc_hz;      /**< Phase noise at 10 kHz offset (dBc/Hz)           */
} oscillator_params_t;

/* ─── L1: Quality Factor Q ────────────────────────────────────────────────── */

/**
 * @brief Quality factor defines the frequency selectivity of a resonant circuit.
 *
 *        Q = 2π · (energy stored per cycle) / (energy dissipated per cycle)
 *
 *        For a series RLC:  Q = ω₀·L / R = 1 / (ω₀·R·C)
 *        For a parallel RLC: Q = R / (ω₀·L) = ω₀·R·C
 *
 *        For crystal oscillators, Q ranges from 10,000 to 2,000,000,
 *        giving exceptional frequency stability.
 *
 *        Q also determines the oscillator phase noise via Leeson's formula:
 *        L(Δf) ∝ 1/Q² for small frequency offsets.
 */
typedef struct {
    double q_value;                /**< Quality factor (dimensionless)                */
    double resonant_freq_hz;       /**< Resonant frequency f₀ = 1/(2π√(LC)) (Hz)     */
    double bandwidth_3db_hz;       /**< 3-dB bandwidth = f₀/Q (Hz)                   */
    double damping_ratio;          /**< Damping ratio ζ = 1/(2Q)                      */
    double energy_stored_j;        /**< Peak energy stored (Joules)                   */
    double energy_lost_per_cycle_j;/**< Energy dissipated per cycle (Joules)          */
    double equivalent_resistance;  /**< Equivalent series or parallel resistance (Ω)  */
} quality_factor_t;

/* ─── L1: Frequency Stability Parameters ──────────────────────────────────── */

/**
 * @brief Frequency stability over temperature, supply voltage, and aging.
 *
 *        Temperature stability is critical for crystal oscillators: AT-cut
 *        crystals achieve ±10 ppm over -20°C to +70°C. TCXO (temperature-
 *        compensated) achieves ±0.5 ppm; OCXO (oven-controlled) achieves
 *        ±0.001 ppm.
 */
typedef struct {
    double temp_coefficient_ppm_per_c; /**< Frequency vs temperature (ppm/°C)     */
    double supply_pushing_ppm_per_v;   /**< Frequency vs supply voltage (ppm/V)   */
    double load_pulling_ppm_per_pf;    /**< Frequency vs load capacitance (ppm/pF)*/
    double aging_ppm_per_year;         /**< Long-term frequency drift (ppm/year)  */
    double short_term_stability_ppb;   /**< Allan deviation at τ=1s (ppb)         */
    double temp_range_min_c;           /**< Operating temperature range min (°C)  */
    double temp_range_max_c;           /**< Operating temperature range max (°C)  */
} frequency_stability_t;

/* ─── L1: Phase Noise ─────────────────────────────────────────────────────── */

/**
 * @brief Phase noise characterizes the spectral purity of an oscillator.
 *
 *        Defined as the single-sideband noise power in a 1 Hz bandwidth
 *        at an offset Δf from the carrier, relative to the carrier power:
 *
 *        L(Δf) = 10·log₁₀( P_noise(Δf, 1Hz) / P_carrier )  [dBc/Hz]
 *
 *        Leeson's model (1966):
 *        L(Δf) = 10·log₁₀{ (F·k·T / 2·P_s) · [1 + (f₀/(2·Q·Δf))²] · (1 + Δf_c/|Δf|) }
 *
 *        where F = noise factor, k = Boltzmann constant, T = temperature,
 *        P_s = signal power, f₀ = oscillation frequency, Q = loaded Q,
 *        Δf_c = flicker corner frequency.
 */
typedef struct {
    double offset_freq_hz;       /**< Frequency offset from carrier (Hz)            */
    double phase_noise_dbc_hz;   /**< L(Δf) at this offset (dBc/Hz)                 */
    double carrier_power_dbm;    /**< Carrier power (dBm)                           */
    double flicker_corner_hz;    /**< 1/f flicker noise corner frequency (Hz)       */
    double noise_factor_db;      /**< Active device noise factor F (dB)             */
    double loaded_q;             /**< Loaded quality factor of the resonator        */
    double rms_jitter_ps;        /**< RMS phase jitter integrated over offset range */
    double integrated_pn_dbc;    /**< Integrated phase noise over band (dBc)         */
} phase_noise_t;

/* ─── L2: Loop Gain Analysis ──────────────────────────────────────────────── */

/**
 * @brief Loop gain frequency response for oscillator design.
 *
 *        The loop gain T(jω) = A(jω)·β(jω) is evaluated over a frequency
 *        sweep to determine the oscillation frequency (where ∠T = 0) and
 *        verify the magnitude condition.
 */
typedef struct {
    size_t num_points;           /**< Number of frequency points                   */
    double *freq_hz;             /**< Frequency sweep values (Hz), length n_points */
    double *magnitude;           /**< |T(jω)| at each frequency                    */
    double *phase_deg;           /**< ∠T(jω) in degrees at each frequency          */
    double *magnitude_db;        /**< 20·log₁₀(|T(jω)|) in dB                      */
    double osc_freq_hz;          /**< Interpolated frequency where ∠T = 0          */
    double gain_at_osc_db;       /**< |T| in dB at oscillation frequency           */
    double phase_margin_deg;     /**< Phase margin (for stability analysis)         */
    double gain_margin_db;       /**< Gain margin                                   */
} loop_gain_sweep_t;

/* ─── L1: Component Tolerances ────────────────────────────────────────────── */

/**
 * @brief Component tolerance analysis for oscillator frequency accuracy.
 *
 *        Monte Carlo / worst-case frequency deviation due to
 *        resistor (±1%, ±5%), capacitor (±5%, ±10%, ±20%),
 *        and inductor (±5%, ±10%) tolerances.
 */
typedef struct {
    double r_tolerance_percent;  /**< Resistor tolerance (%)                        */
    double c_tolerance_percent;  /**< Capacitor tolerance (%)                       */
    double l_tolerance_percent;  /**< Inductor tolerance (%)                        */
    double freq_dev_min_hz;      /**< Minimum frequency in tolerance range (Hz)     */
    double freq_dev_max_hz;      /**< Maximum frequency in tolerance range (Hz)     */
    double freq_dev_ppm;         /**< Worst-case frequency deviation (ppm)          */
    int    monte_carlo_samples;  /**< Number of Monte Carlo samples (0 = analytical) */
} tolerance_analysis_t;

/* ─── Function Declarations — L4 Barkhausen Verification ─────────────────── */

/**
 * @brief Evaluate the Barkhausen criterion at a specific frequency.
 *
 *        Given loop gain magnitude |AB| and phase ∠AB at frequency f,
 *        determines whether oscillation can be sustained.
 *
 * @param magnitude  |AB| at frequency f
 * @param phase_deg  ∠AB in degrees at frequency f
 * @param freq_hz    Frequency under evaluation (Hz)
 * @return           Barkhausen criterion evaluation result
 *
 * @note phase_tol_deg defaults to 1°, magnitude tolerance to 0.01
 */
barkhausen_criterion_t  barkhausen_evaluate(double magnitude, double phase_deg,
                                              double freq_hz);

/**
 * @brief Check the start-up condition for oscillation.
 *
 *        For reliable start-up, the small-signal loop gain must exceed 1
 *        (typically |L| >= 3-5 for RC, |L| >= 2 for LC oscillators).
 *
 * @param gain_db  Small-signal loop gain in dB at the oscillation frequency
 * @param osc_type Oscillator topology (affects recommended margin)
 * @return         1 if start-up condition is satisfied, 0 otherwise
 */
int  barkhausen_startup_check(double gain_db, oscillator_type_t osc_type);

/**
 * @brief Compute oscillation frequency from Barkhausen phase condition on a
 *        frequency sweep. Finds the zero-crossing of angle(T(f)) via interpolation.
 *
 * @param sweep  Loop gain sweep data
 * @return       Oscillation frequency (Hz), or -1 if no zero-crossing found
 */
double  barkhausen_find_osc_freq(const loop_gain_sweep_t *sweep);

/**
 * @brief Initialize oscillator parameters with sensible defaults.
 *
 * @param type       Oscillator topology
 * @param freq_hz    Target frequency
 * @param supply_v   Supply voltage
 * @return           Initialized parameter struct
 */
oscillator_params_t  oscillator_params_init(oscillator_type_t type,
                                              double freq_hz, double supply_v);

/**
 * @brief Compute realistic THD estimate for a given oscillator topology
 *        based on empirical models.
 *
 * @param params  Oscillator parameters
 * @return        Estimated THD in percent
 */
double  oscillator_estimate_thd(const oscillator_params_t *params);

/**
 * @brief Compute Quality Factor Q from RLC component values.
 *
 * @param r  Resistance (Ohm)
 * @param l  Inductance (H) — 0 for RC circuits
 * @param c  Capacitance (F)
 * @param is_parallel  1 = parallel RLC, 0 = series RLC
 * @return   Quality factor struct
 */
quality_factor_t  quality_factor_compute(double r, double l, double c,
                                           int is_parallel);

/**
 * @brief Compute Q from resonant frequency and 3-dB bandwidth.
 *
 * @param f0_hz   Resonant frequency (Hz)
 * @param bw_hz   3-dB bandwidth (Hz)
 * @return        Q value
 */
double  quality_factor_from_bandwidth(double f0_hz, double bw_hz);

/**
 * @brief Estimate start-up time for an oscillator.
 *
 *        tau_startup ≈ Q / (pi * f0) * ln(V_final / V_noise)
 *
 * @param q_factor       Quality factor
 * @param freq_hz        Oscillation frequency (Hz)
 * @param v_noise_uv     RMS noise voltage at start-up (uV)
 * @param v_final_v      Final amplitude (V)
 * @return               Start-up time estimate (seconds)
 */
double  oscillator_startup_time_estimate(double q_factor, double freq_hz,
                                           double v_noise_uv, double v_final_v);

/**
 * @brief Compute frequency deviation due to component tolerances.
 *
 *        Uses worst-case analysis based on sensitivity coefficients.
 *
 * @param nominal_freq_hz  Nominal oscillation frequency (Hz)
 * @param tol              Component tolerance data (used as in/out)
 * @return                 Tolerance analysis result
 */
tolerance_analysis_t  oscillator_tolerance_analyze(double nominal_freq_hz,
                                                     const tolerance_analysis_t *tol);

/* ─── Memory Management ─────────────────────────────────────────────────── */

void  loop_gain_sweep_free(loop_gain_sweep_t *sweep);

#endif /* OSCILLATOR_CORE_H */
