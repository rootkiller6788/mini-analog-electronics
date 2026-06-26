/**
 * active_filter_defs.h — Core Type Definitions for Active Filters
 *
 * L1 Definitions: active filter types, op-amp topologies,
 * component structures, gain definitions, filter specifications.
 *
 * Active filters use op-amps with RC networks to realize filter
 * transfer functions without inductors. Key advantages over passive
 * LC filters: no magnetics, adjustable gain, easy cascading,
 * impedance buffering between stages.
 *
 * Courses: MIT 6.002, Stanford EE101B, Berkeley EE105, ETH 227-0455
 * Reference: Sedra & Smith (2020) Microelectronic Circuits, Ch. 16
 *            Franco, Design with Operational Amplifiers (2014)
 *            Huijsing, Operational Amplifiers: Theory and Design (2017)
 */

#ifndef ACTIVE_FILTER_DEFS_H
#define ACTIVE_FILTER_DEFS_H

#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <complex.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================
 * L1: Active Filter Topology Types — 10 canonical topologies
 * ================================================================== */

/** Active filter topology enumeration.
 *  Each topology represents a distinct circuit arrangement of op-amps
 *  and RC networks with different sensitivity and tuning characteristics.
 *
 *  Sallen-Key: Single op-amp, non-inverting, low component count,
 *              excellent for unity-gain LP/HP designs. Limited Q range.
 *  MFB:        Multiple feedback (infinite-gain), inverting, superior
 *              stopband rejection, better for high-Q BP designs.
 *  State-Var:  3-4 op-amps, simultaneous LP/BP/HP outputs, easily
 *              tunable, excellent for voltage-controlled filters.
 *  Biquad:     Two-integrator-loop (Tow-Thomas), like state-variable
 *              but optimized for cascaded high-order realizations.
 *  GIC:        Generalized immittance converter, simulates inductors
 *              using op-amps, enables LC ladder simulation.
 *  FDNR:       Frequency-dependent negative resistance, dual of GIC,
 *              transforms LC ladder to RC-active ladder.
 *  Gyrator:    Two-op-amp circuit that converts capacitive load to
 *              inductive input impedance (Antoniou gyrator).
 *  Leapfrog:   Simulates doubly-terminated LC ladder using integrators.
 *              Low sensitivity, good for high-order elliptic filters.
 *  SCF:        Switched-capacitor filter, clock-tunable, MOS-compatible,
 *              replaces resistors with capacitor switching.
 *  GmC:        Transconductance-C filter, OTA-based, open-loop,
 *              suitable for high-frequency and integrated applications. */
typedef enum {
    ACTIVE_TOPO_SALLEN_KEY      = 0,  /* Sallen-Key (VCVS) single op-amp */
    ACTIVE_TOPO_MFB             = 1,  /* Multiple Feedback (Rauch) */
    ACTIVE_TOPO_STATE_VARIABLE  = 2,  /* State-variable (KHN) universal */
    ACTIVE_TOPO_BIQUAD          = 3,  /* Tow-Thomas two-integrator biquad */
    ACTIVE_TOPO_GIC             = 4,  /* Generalized Immittance Converter */
    ACTIVE_TOPO_FDNR            = 5,  /* Frequency-Dependent Negative Res. */
    ACTIVE_TOPO_GYRATOR         = 6,  /* Antoniou gyrator inductor sim */
    ACTIVE_TOPO_LEAPFROG        = 7,  /* LC ladder leapfrog simulation */
    ACTIVE_TOPO_SCF             = 8,  /* Switched-capacitor filter */
    ACTIVE_TOPO_GMC             = 9   /* Gm-C (OTA-C) transconductance */
} active_topology_t;

/* ==================================================================
 * L1: Filter Approximation Types for Active Realization
 * ================================================================== */

/** Approximation type — optimization criterion for pole/zero placement.
 *  Butterworth:   Maximally flat magnitude at DC (no ripple). |H| = 1/√(1+w^{2N})
 *  Chebyshev I:   Equiripple in passband, monotonic stopband.
 *  Chebyshev II:  Monotonic passband, equiripple in stopband (inverse).
 *  Bessel:        Maximally flat group delay (best phase linearity).
 *  Elliptic:      Equiripple in both passband and stopband (Cauer).
 *  Linear_Phase:  Equiripple group delay error (0.05° to 0.5° spec).
 *  Transitional:  Blend between Butterworth and Bessel characteristics.
 *  Gaussian:      Gaussian magnitude shape, minimal time-bandwidth. */
typedef enum {
    ACTIVE_APPROX_BUTTERWORTH   = 0,
    ACTIVE_APPROX_CHEBYSHEV_I   = 1,
    ACTIVE_APPROX_CHEBYSHEV_II  = 2,
    ACTIVE_APPROX_BESSEL        = 3,
    ACTIVE_APPROX_ELLIPTIC      = 4,
    ACTIVE_APPROX_LINEAR_PHASE  = 5,
    ACTIVE_APPROX_TRANSITIONAL  = 6,
    ACTIVE_APPROX_GAUSSIAN      = 7
} active_approx_t;

/** Filter function type — the desired frequency response shape. */
typedef enum {
    ACTIVE_FUNC_LOWPASS   = 0,  /* DC pass, HF attenuation */
    ACTIVE_FUNC_HIGHPASS  = 1,  /* HF pass, DC attenuation */
    ACTIVE_FUNC_BANDPASS  = 2,  /* Pass a band, attenuate both sides */
    ACTIVE_FUNC_BANDSTOP  = 3,  /* Notch one band, pass elsewhere */
    ACTIVE_FUNC_ALLPASS   = 4,  /* Constant magnitude, variable phase */
    ACTIVE_FUNC_NOTCH     = 5,  /* Narrow deep notch (twin-T, bridged-T) */
    ACTIVE_FUNC_LOWSHELF  = 6,  /* Boost/cut below corner frequency */
    ACTIVE_FUNC_HIGHSHELF = 7   /* Boost/cut above corner frequency */
} active_filter_func_t;

/* ==================================================================
 * L1: Op-Amp Model Selection for Design
 * ================================================================== */

/** Op-amp model level for design accuracy.
 *  IDEAL:       Infinite gain, BW, slew rate; zero offset, noise.
 *  SINGLE_POLE: Finite GBW, single dominant pole roll-off (-20 dB/dec).
 *  TWO_POLE:    Two poles, phase margin effects, potential instability.
 *  FULL:        Includes slew rate, offset voltage, bias current,
 *               CMRR, PSRR, output swing limits. */
typedef enum {
    ACTIVE_OPAMP_IDEAL       = 0,
    ACTIVE_OPAMP_SINGLE_POLE = 1,
    ACTIVE_OPAMP_TWO_POLE    = 2,
    ACTIVE_OPAMP_FULL        = 3
} active_opamp_model_t;

/* ==================================================================
 * L1: Core Component Data Structures
 * ================================================================== */

/** Resistor specification for active filter design.
 *  E-series: E12, E24, E48, E96, E192 standard values.
 *  Tolerance: ±0.1% to ±10% affects sensitivity.
 *  TC: temperature coefficient (ppm/°C) affects thermal drift. */
typedef struct {
    double resistance;   /* Resistance value in ohms */
    int    e_series;     /* E-series: 12, 24, 48, 96, or 192 */
    double tolerance_pct;/* Tolerance in percent (±) */
    double tc_ppm;       /* Temperature coefficient in ppm/°C */
    double power_rating; /* Power rating in watts */
} active_resistor_t;

/** Capacitor specification for active filter design.
 *  Dielectric affects: tolerance, TC, voltage coefficient, leakage.
 *  COG/NP0: best stability, low TC (±30 ppm), small values (<100nF).
 *  X7R: moderate stability, ±15% over temp, up to μF range.
 *  Film: polypropylene/polyester, excellent for audio filters.
 *  Electrolytic: large values, poor tolerance/ESR, not for filters. */
typedef struct {
    double capacitance;    /* Capacitance in farads */
    double tolerance_pct;  /* Tolerance in percent (±) */
    double tc_ppm;         /* Temperature coefficient in ppm/°C */
    double esr;            /* Equivalent series resistance in ohms */
    int    dielectric;     /* 0=COG, 1=X7R, 2=Film, 3=Electrolytic */
    double voltage_rating; /* Maximum voltage rating */
} active_capacitor_t;

/** Op-amp parameters for non-ideal analysis.
 *  GBW: Gain-bandwidth product in Hz. For single-pole model,
 *       A(s) = GBW / (s + GBW/A0) ≈ GBW/s for f >> f_dominant.
 *  Slew rate: Max output voltage change rate (V/μs).
 *             Limits large-signal bandwidth: f_max = SR / (2π·Vpk).
 *  Phase margin: 180° + ∠(β·A(f_unity)). Must be > 45° for stability.
 *  CMRR: Common-mode rejection ratio (dB), affects DC accuracy.
 *  PSRR: Power supply rejection ratio (dB), couples supply noise. */
typedef struct {
    double gbw_hz;            /* Gain-bandwidth product (Hz) */
    double open_loop_gain_db; /* DC open-loop gain (dB) */
    double slew_rate_v_us;    /* Slew rate (V/μs) */
    double phase_margin_deg;  /* Phase margin (degrees) */
    double cmrr_db;           /* Common-mode rejection ratio */
    double psrr_db;           /* Power supply rejection ratio */
    double input_offset_uv;   /* Input offset voltage (μV) */
    double input_bias_na;     /* Input bias current (nA) */
    double input_noise_nv_rt; /* Input voltage noise density (nV/√Hz) */
    double output_swing_v;    /* Output voltage swing (±V from rail) */
} active_opamp_params_t;

/* ==================================================================
 * L1: Filter Specification Structure
 * ================================================================== */

/** Complete active filter design specification.
 *  Translates application requirements into design parameters.
 *
 *  For BP/BS: f_center = sqrt(f_low * f_high), Q = f_center / BW
 *  For Notch: Q determines notch depth and width.
 *  Gain may be 0 dB (unity) for standard filters, adjustable for
 *  Sallen-Key with gain (K > 1 adds peaking). */
typedef struct {
    active_filter_func_t  function;      /* LP, HP, BP, BS, etc. */
    active_approx_t       approx;        /* Approximation family */
    active_topology_t     topology;      /* Circuit topology */
    active_opamp_model_t  opamp_model;   /* Op-amp modeling level */
    int                   order;         /* Filter order (1..10) */
    double                f_low;         /* Lower -3dB/cutoff freq (Hz) */
    double                f_high;        /* Upper -3dB/cutoff freq (Hz, 0=unused) */
    double                f_center;      /* Center frequency for BP/BS (Hz) */
    double                bandwidth;     /* Bandwidth for BP/BS (Hz) */
    double                passband_ripple_db; /* Max passband ripple (dB) */
    double                stopband_atten_db;  /* Min stopband attenuation (dB) */
    double                gain_db;       /* Passband gain (dB) */
    double                q_factor;      /* Quality factor (for second-order) */
    double                impedance_ohm; /* Design impedance level */
} active_filter_spec_t;

/* ==================================================================
 * L1: Active Biquad Section — Second-Order Building Block
 * ================================================================== */

/** Normalized active biquad section.
 *  Represents one second-order stage in a cascade.
 *  H(s) = K * (s^2 + (wz/Qz)*s + wz^2) / (s^2 + (wp/Qp)*s + wp^2)
 *
 *  wp = pole natural frequency (rad/s)
 *  Qp = pole quality factor (dimensionless)
 *  wz = zero natural frequency (rad/s), infinite for all-pole filters
 *  Qz = zero quality factor
 *  K  = DC gain
 *
 *  For LP: wz → ∞, numerator = K · ωp²
 *  For HP: wz = 0, numerator = K · s²
 *  For BP: wz = ωp, Qz = Qp, numerator = K · (ωp/Qp) · s */
typedef struct {
    double wp;          /* Pole natural frequency (rad/s) */
    double qp;          /* Pole Q factor */
    double wz;          /* Zero natural frequency (rad/s) */
    double qz;          /* Zero Q factor */
    double gain;        /* Section gain K */
    int    is_allpole;  /* 1 if no finite zeros (LP/HP), 0 otherwise */
} active_biquad_section_t;

/* ==================================================================
 * L1: Component Value Set — Physical Implementation Parameters
 * ================================================================== */

/** RC component values for one active filter stage.
 *  These are the physical resistor and capacitor values that
 *  realize the desired biquad transfer function.
 *
 *  For Sallen-Key LP: R1,R2 + C1,C2 + (R3,R4 for gain)
 *  For MFB LP: R1,R2,R3 + C1,C2
 *  For Sallen-Key HP: C1,C2 + R1,R2 + (R3,R4 for gain)
 *  For MFB BP: R1,R2,R3 + C1,C2 */
typedef struct {
    double r1;  /* First resistor (ohms) */
    double r2;  /* Second resistor (ohms) */
    double r3;  /* Third resistor (ohms, 0 if unused) */
    double r4;  /* Fourth resistor (ohms, 0 if unused) */
    double r5;  /* Fifth resistor (ohms, 0 if unused) */
    double c1;  /* First capacitor (farads) */
    double c2;  /* Second capacitor (farads) */
    double c3;  /* Third capacitor (farads, 0 if unused) */
} active_component_values_t;

/* ==================================================================
 * L1: Complete Active Filter Structure
 * ================================================================== */

/** Full active filter realization.
 *  Represents a complete active filter: specification, designed
 *  biquad sections with component values, and non-ideal analysis.
 *
 *  A high-order filter is realized as a cascade of second-order
 *  sections (plus one first-order for odd-order filters). */
typedef struct {
    active_filter_spec_t       spec;             /* Design specification */
    active_biquad_section_t   *biquads;          /* Array of biquad sections */
    int                        num_biquads;      /* Number of biquad sections */
    active_component_values_t *components;       /* RC values per section */
    double                     actual_gain_db;   /* Realized passband gain (dB) */
    double                     actual_fc_hz;     /* Realized cutoff frequency (Hz) */
    double                     sensitivity_worst; /* Worst-case sensitivity */
    int                        is_stable;        /* 1 if stable */
    char                       design_notes[256]; /* Design method notes */
} active_filter_t;

/* ==================================================================
 * L1: Frequency Response Point
 * ================================================================== */

/** Single-point frequency response evaluation.
 *  Magnitude and phase at one frequency — building block for
 *  complete Bode plot and frequency response analysis. */
typedef struct {
    double freq_hz;       /* Frequency in Hz */
    double magnitude;     /* |H(jω)| linear */
    double magnitude_db;  /* |H(jω)| in dB */
    double phase_rad;     /* arg(H(jω)) in radians */
    double phase_deg;     /* arg(H(jω)) in degrees */
    double group_delay_s; /* -dφ/dω in seconds */
    double real;          /* Re(H(jω)) */
    double imag;          /* Im(H(jω)) */
} active_freq_point_t;

/** Complete frequency response sweep.
 *  Used for Bode plots, Nyquist diagrams, and filter verification. */
typedef struct {
    active_freq_point_t *points;
    int                  num_points;
    double               f_start;
    double               f_stop;
    int                  log_spacing;  /* 1 for log, 0 for linear */
} active_freq_response_t;

/* ==================================================================
 * L1: Sensitivity Analysis Results
 * ================================================================== */

/** Component sensitivity measures.
 *  S_x^Q = (∂Q/∂x)·(x/Q) — fractional change in Q per fractional
 *  change in component x. Lower is better (< 1 target).
 *  Critical for manufacturing yield and temperature stability. */
typedef struct {
    double s_r1_q;    /* Sensitivity of Q to R1 */
    double s_r2_q;    /* Sensitivity of Q to R2 */
    double s_r3_q;    /* Sensitivity of Q to R3 */
    double s_r4_q;    /* Sensitivity of Q to R4 */
    double s_c1_q;    /* Sensitivity of Q to C1 */
    double s_c2_q;    /* Sensitivity of Q to C2 */
    double s_r1_f0;   /* Sensitivity of ω0 to R1 */
    double s_r2_f0;   /* Sensitivity of ω0 to R2 */
    double s_r3_f0;   /* Sensitivity of ω0 to R3 */
    double s_r4_f0;   /* Sensitivity of ω0 to R4 */
    double s_c1_f0;   /* Sensitivity of ω0 to C1 */
    double s_c2_f0;   /* Sensitivity of ω0 to C2 */
    double worst_case; /* Maximum absolute sensitivity */
} active_sensitivity_t;

/* ==================================================================
 * L1: Tuning and Optimization Parameters
 * ================================================================== */

/** Filter tuning configuration.
 *  Used for adjusting component values to compensate for
 *  tolerances via potentiometers, switched arrays, or DAC control. */
typedef struct {
    int    tune_method;     /* 0=none, 1=manual trim, 2=auto-cal, 3=digipot */
    double target_freq_hz;  /* Target center/cutoff frequency */
    double target_q;        /* Target Q factor */
    double target_gain_db;  /* Target gain */
    double freq_tolerance;  /* Acceptable frequency error (%) */
    double q_tolerance;     /* Acceptable Q error (%) */
    int    max_iterations;  /* Max auto-calibration iterations */
} active_tuning_config_t;

/* ==================================================================
 * L1: Error Codes
 * ================================================================== */

typedef enum {
    ACTIVE_OK                   =  0,
    ACTIVE_ERR_NULL_PTR         = -1,
    ACTIVE_ERR_INVALID_ORDER    = -2,
    ACTIVE_ERR_INVALID_FREQ     = -3,
    ACTIVE_ERR_INVALID_Q        = -4,
    ACTIVE_ERR_INVALID_TOPOLOGY = -5,
    ACTIVE_ERR_UNSTABLE         = -6,
    ACTIVE_ERR_COMPONENT_RANGE  = -7,  /* Component outside practical range */
    ACTIVE_ERR_MEMORY           = -8,
    ACTIVE_ERR_NO_CONVERGE      = -9,  /* Iterative design didn't converge */
    ACTIVE_ERR_NOT_IMPL         = -10,
    ACTIVE_ERR_ZERO_DIV         = -11,
    ACTIVE_ERR_OVERFLOW         = -12,
    ACTIVE_ERR_GBW_LIMIT        = -13, /* Op-amp GBW insufficient */
    ACTIVE_ERR_SLEW_LIMIT       = -14  /* Slew rate insufficient */
} active_error_t;

/* ==================================================================
 * L1: Inline Utility Functions
 * ================================================================== */

/** Initialize filter specification with safe defaults.
 *  Default: 1 kHz Butterworth LP, Sallen-Key, unity gain, ideal op-amp. */
static inline active_filter_spec_t active_filter_spec_init(void) {
    return (active_filter_spec_t){
        .function = ACTIVE_FUNC_LOWPASS,
        .approx = ACTIVE_APPROX_BUTTERWORTH,
        .topology = ACTIVE_TOPO_SALLEN_KEY,
        .opamp_model = ACTIVE_OPAMP_IDEAL,
        .order = 2,
        .f_low = 1000.0,
        .f_high = 0.0,
        .f_center = 0.0,
        .bandwidth = 0.0,
        .passband_ripple_db = 0.0,
        .stopband_atten_db = 60.0,
        .gain_db = 0.0,
        .q_factor = 0.7071,
        .impedance_ohm = 10000.0
    };
}

/** Validate filter specification for basic sanity.
 *  Returns 1 if valid, 0 if any parameter is out of range. */
static inline int active_filter_spec_is_valid(const active_filter_spec_t *spec) {
    if (!spec) return 0;
    if (spec->order < 1 || spec->order > 10) return 0;
    if (spec->f_low < 0.0) return 0;
    if (spec->passband_ripple_db < 0.0) return 0;
    if (spec->stopband_atten_db < 0.0) return 0;
    if (spec->gain_db < -120.0 || spec->gain_db > 120.0) return 0;
    if (spec->impedance_ohm < 100.0 || spec->impedance_ohm > 1e9) return 0;
    if (spec->function == ACTIVE_FUNC_BANDPASS ||
        spec->function == ACTIVE_FUNC_BANDSTOP) {
        if (spec->f_center <= 0.0 && spec->f_low <= 0.0) return 0;
    }
    return 1;
}

/** dB to linear conversion: lin = 10^(dB/20) */
static inline double active_db_to_linear(double db) {
    return pow(10.0, db / 20.0);
}

/** Linear to dB conversion: dB = 20*log10(linear) */
static inline double active_linear_to_db(double linear) {
    if (linear <= 0.0) return -200.0;
    return 20.0 * log10(linear);
}

/** Calculate Q from -3 dB bandwidth: Q = f0 / BW */
static inline double active_q_from_bandwidth(double f0, double bw) {
    if (bw <= 0.0 || f0 <= 0.0) return 0.0;
    return f0 / bw;
}

/** Calculate BW from Q: BW = f0 / Q */
static inline double active_bw_from_q(double f0, double q) {
    if (q <= 0.0) return 1e12;
    return f0 / q;
}

/** Calculate center frequency from band edges (geometric mean).
 *  f_center = sqrt(f_low * f_high) — exact for BP on log scale. */
static inline double active_center_freq(double f_low, double f_high) {
    if (f_low <= 0.0 || f_high <= f_low) return 0.0;
    return sqrt(f_low * f_high);
}

/* ==================================================================
 * L3: Non-inline utility function declarations (implemented in core.c)
 * ================================================================== */

double active_nearest_eseries(double value, int e_series);

double active_parallel_resistance(double r1, double r2);

double active_series_capacitance(double c1, double c2);

void active_impedance_scale(active_component_values_t *comp, double z_factor);

double active_filter_find_cutoff(const active_biquad_section_t *biquads,
                                  int num_biquads,
                                  active_filter_func_t func,
                                  double f_low, double f_high,
                                  double passband_db);

/* Core biquad operations */
void active_biquad_init(double wp, double qp, double gain,
                         active_filter_func_t func,
                         active_biquad_section_t *bq);

double complex active_biquad_evaluate(const active_biquad_section_t *bq,
                                       double omega);

double active_biquad_magnitude_db(const active_biquad_section_t *bq,
                                   double omega);

double active_biquad_phase_rad(const active_biquad_section_t *bq,
                                double omega);

double active_biquad_group_delay(const active_biquad_section_t *bq,
                                  double omega);

double complex active_cascade_evaluate(const active_biquad_section_t *biquads,
                                        int num_biquads, double omega);

double active_cascade_magnitude_db(const active_biquad_section_t *biquads,
                                    int num_biquads, double omega);

int active_freq_response_sweep(const active_biquad_section_t *biquads,
                                int num_biquads,
                                double f_start, double f_stop,
                                int num_points, int log_spacing,
                                active_freq_response_t *response);

void active_freq_response_free(active_freq_response_t *response);

/* Filter management */
int active_filter_alloc(const active_filter_spec_t *spec,
                         active_filter_t **filter);

void active_filter_free(active_filter_t *filter);

/* Pole normalization */
void active_normalize_poles(double wp_norm, double qp,
                             active_filter_func_t func,
                             double fc, double bw,
                             double *wp_actual, double *qp_actual);

/* Order estimation */
int active_estimate_order_butterworth(double passband_edge_hz,
                                       double stopband_edge_hz,
                                       double passband_ripple_db,
                                       double stopband_atten_db);

int active_estimate_order_chebyshev(double passband_edge_hz,
                                     double stopband_edge_hz,
                                     double passband_ripple_db,
                                     double stopband_atten_db);

int active_component_range_check(const active_component_values_t *comp,
                                  char *warning);

#ifdef __cplusplus
}
#endif

#endif /* ACTIVE_FILTER_DEFS_H */
