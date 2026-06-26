/**
 * active_filter_advanced.h — Switched-Capacitor, Gm-C, and Advanced Topologies
 *
 * L8 Advanced Topics: Switched-capacitor filters (SCF), Gm-C (OTA-C)
 * filters, and N-path filters — the dominant integrated-circuit
 * filter technologies.
 *
 * Switched-Capacitor Filters:
 * - Replace resistors with switched capacitors: R_eq = 1/(f_clk·C)
 * - Time constant set by capacitor ratio and clock frequency
 * - Capacitor matching: ±0.1% (MOS) vs ±1% (discrete resistors)
 * - Clock-tunable: f_cutoff ∝ f_clk
 * - Anti-aliasing required at input and output
 *
 * Gm-C (OTA-C) Filters:
 * - Use transconductance amplifiers instead of op-amps
 * - Open-loop operation → highest bandwidth (GHz range)
 * - Transconductance Gm tunable via bias current
 * - Requires automatic tuning to compensate Gm variation (±30%)
 * - Suitable for disk-drive read channels, RF front-ends
 *
 * N-Path Filters:
 * - Commutated capacitor networks with rotating switches
 * - Creates bandpass response centered at switching frequency
 * - High Q achievable without high-order active stages
 * - Impedance translation: Z_in(f) = Z_in(f - N·f_sw) / N
 *
 * Courses: Berkeley EE140, Stanford EE214, ETH 227-0455
 * Reference: Gregorian & Temes, Analog MOS Integrated Circuits
 *            for Signal Processing (1986)
 *            Nauta, "Analog CMOS Filters for Very High Frequencies"
 *            (1993)
 */

#ifndef ACTIVE_FILTER_ADVANCED_H
#define ACTIVE_FILTER_ADVANCED_H

#include "active_filter_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================
 * L8: Switched-Capacitor Filter (SCF) Design
 * ================================================================== */

/** Switched-capacitor integrator design parameters. */
typedef struct {
    double clock_freq_hz;     /* Switching clock frequency (Hz) */
    double c_integration;     /* Integration capacitor (F) */
    double c_switched;        /* Switched input capacitor (F) */
    double c_ratio;           /* C_switched / C_integration ratio */
    double equivalent_r;      /* Equivalent resistance: 1/(f_clk·C_switched) */
    double integrator_tau;    /* Time constant: R_eq·C_int = 1/(f_clk·ratio) */
} sc_integrator_t;

/** Design a switched-capacitor integrator.
 *  L8: The SC integrator is the fundamental building block of SCFs.
 *  Time constant: τ = C_int / (f_clk · C_switched)
 *  Equivalent resistance: R_eq = 1 / (f_clk · C_switched)
 *
 *  Key advantage: τ depends on capacitor RATIO, not absolute values.
 *  MOS capacitor matching achieves ±0.1%, far better than
 *  resistor absolute tolerance (±1-5%).
 *
 *  Charge transfer per clock cycle: ΔQ = C_sw · V_in
 *  Output update: V_out[n+1] = V_out[n] - (C_sw/C_int) · V_in[n]
 *
 *  @param tau_desired  Desired time constant (seconds)
 *  @param f_clk        Clock frequency (Hz)
 *  @param c_max        Maximum capacitance constraint (F)
 *  @param integrator   Output SC integrator parameters
 *  @return             ACTIVE_OK or error code
 *  Complexity: O(1) */
int sc_integrator_design(double tau_desired, double f_clk, double c_max,
                          sc_integrator_t *integrator);

/** Design a switched-capacitor first-order lowpass filter.
 *  L8: SC LPF = SC integrator with feedback around it.
 *  f_cutoff = f_clk · C_switched / (2π · C_integration)
 *
 *  Transfer function (z-domain):
 *    H(z) = -(C1/C2) / (1 - (1 + C1/C2)·z^{-1})
 *
 *  @param fc       Desired -3dB cutoff frequency (Hz)
 *  @param gain_db  DC gain (dB)
 *  @param f_clk    Clock frequency (typically 50-100× fc)
 *  @param c_max    Maximum capacitor (F, for IC area constraint)
 *  @param comp     Output component values (C values in farads)
 *  @param ratio    Output capacitor ratio for implementation
 *  @return         ACTIVE_OK or error
 *  Complexity: O(1) */
int sc_lowpass_design(double fc, double gain_db, double f_clk,
                       double c_max, active_component_values_t *comp,
                       double *ratio);

/** Design a switched-capacitor biquad (second-order section).
 *  L8: SC biquad uses two integrators in a loop (like Tow-Thomas
 *  but with SC equivalents). Can realize LP, BP, HP functions.
 *
 *  Key design parameters:
 *    ω₀ = f_clk · √(C1·C2 / (CA·CB))  (capacitor ratios)
 *    Q  = √(CA·CB / (C1·C2)) · (CQ/CF)
 *
 *  All parameters determined by capacitor RATIOS — robust to
 *  process variation if capacitors are well-matched.
 *
 *  Reference: Fleischer & Laker, "A Family of Active Switched
 *            Capacitor Biquad Building Blocks," BSTJ, 1979
 *
 *  @param f0       Center/cutoff frequency (Hz)
 *  @param q        Quality factor
 *  @param function LP, BP, or HP
 *  @param f_clk    Clock frequency (Hz)
 *  @param c_max    Maximum capacitor (F)
 *  @param comp     Output: SC capacitor values (stored as C1-C5)
 *  @return         ACTIVE_OK
 *  Complexity: O(1) */
int sc_biquad_design(double f0, double q, active_filter_func_t function,
                      double f_clk, double c_max,
                      active_component_values_t *comp);

/** Compute aliasing effects in SC filter.
 *  L8: SC filters sample the input at f_clk. The output spectrum
 *  contains images at multiples of f_clk. A continuous-time
 *  anti-aliasing filter is needed at the input, and a smoothing
 *  (reconstruction) filter at the output.
 *
 *  Aliased signal power ratio:
 *    P_alias/P_signal ≈ Σ_k |H(f + k·f_clk)/H(f)|²
 *
 *  @param f_signal  Signal frequency (Hz)
 *  @param f_clk     Clock frequency (Hz)
 *  @param fc_input  Input anti-aliasing filter cutoff (Hz)
 *  @param alias_db  Output: aliased signal ratio (dB)
 *  Complexity: O(1) */
int sc_alias_analysis(double f_signal, double f_clk, double fc_input,
                       double *alias_db);

/* ==================================================================
 * L8: Gm-C (OTA-C) Filter Design
 * ================================================================== */

/** Gm-C filter parameters — transconductance-based design. */
typedef struct {
    double gm;              /* Transconductance (S or A/V) */
    double c_load;          /* Load capacitance (F) */
    double gm_c_ratio;      /* gm/C ratio = ω0 (rad/s) */
    double bias_current;    /* Bias current (A) */
    double linear_range_v;  /* Linear input range (V) */
    double noise_density;   /* Input-referred noise (V²/Hz) */
    double power_w;         /* Power consumption (W) */
} gmc_params_t;

/** Design a Gm-C integrator (fundamental Gm-C building block).
 *  L8: A simple OTA with capacitive load acts as an integrator.
 *  H(s) = gm/(s·C) = ω_u/s  where ω_u = gm/C is unity-gain frequency.
 *
 *  For a differential OTA with source degeneration:
 *    gm = I_bias / (n·V_T)  (weak inversion)
 *    gm = √(2·μ·Cox·(W/L)·I_bias)  (strong inversion)
 *
 *  Tunable via bias current: ω_u ∝ I_bias (weak inv) or √I_bias (strong inv).
 *
 *  @param unity_gain_freq  Desired unity-gain frequency ω_u (rad/s)
 *  @param c_load           Load capacitance (F)
 *  @param params           Output Gm-C parameters
 *  @return                 ACTIVE_OK
 *  Complexity: O(1) */
int gmc_integrator_design(double unity_gain_freq, double c_load,
                           gmc_params_t *params);

/** Design a Gm-C biquadratic filter.
 *  L8: Gm-C biquad uses 3-4 OTAs with capacitors in a feedback
 *  configuration. The canonical Gm-C biquad topology:
 *
 *  Two OTAs as gyrators (simulate inductor), two as transconductors.
 *    ω₀ = gm/√(C1·C2)
 *    Q  = √(C2/C1)
 *
 *  All parameters are gm/C ratios — fully integratable.
 *  Requires on-chip automatic tuning loop to compensate gm
 *  variation over process and temperature (±30% typical).
 *
 *  Reference: Geiger & Sanchez-Sinencio, "Active Filter Design
 *            Using Operational Transconductance Amplifiers: A
 *            Tutorial," IEEE CAS Magazine, 1985
 *
 *  @param f0       Center/cutoff frequency (Hz)
 *  @param q        Quality factor
 *  @param function LP, BP, or HP
 *  @param c_max    Maximum capacitance (F)
 *  @param params   Output Gm-C parameters for each OTA
 *  @param comp     Output capacitor values
 *  @return         ACTIVE_OK
 *  Complexity: O(1) */
int gmc_biquad_design(double f0, double q, active_filter_func_t function,
                       double c_max, gmc_params_t *params,
                       active_component_values_t *comp);

/** Compute total harmonic distortion (THD) for Gm-C filter.
 *  L8: Gm-C nonlinearity from OTA V-I characteristic:
 *  I_out = I_bias · tanh(V_in / (2·n·V_T))
 *
 *  For small signals: THD ≈ (1/32)·(Vpk / (n·V_T))²
 *  For Vpk = 100mV, n·V_T = 25mV: THD ≈ (1/32)·(4)² ≈ 0.5 = -6 dB (bad!)
 *
 *  Source degeneration or linearization techniques
 *  (cross-coupled pairs, resistive degeneration) can reduce THD
 *  by 20-40 dB at the cost of reduced gm.
 *
 *  @param params          Gm-C parameters
 *  @param vpk             Peak signal amplitude (V)
 *  @param with_degenerate 1 if source degeneration used
 *  @return                THD (fraction, 0.01 = 1% = -40dB)
 *  Complexity: O(1) */
double gmc_thd_estimate(const gmc_params_t *params, double vpk,
                         int with_degenerate);

/** Design on-chip automatic tuning loop for Gm-C filter.
 *  L8: Master-slave tuning uses a PLL-like loop:
 *  - Master Gm-C integrator is tuned to a reference frequency
 *  - Tuning voltage/current is copied to slave filter Gm-C stages
 *  - Reference typically derived from crystal oscillator
 *
 *  Lock range: ±30% of gm nominal (covers process ± 3σ)
 *  Acquisition time: ~100 reference cycles
 *
 *  @param f_ref       Reference frequency (Hz, from crystal)
 *  @param gm_nominal  Nominal transconductance (S)
 *  @param gm_range_pct Expected gm variation range (%)
 *  @param loop_bw     Loop bandwidth (Hz, typically f_ref/100)
 *  @param tune_range  Output: tuning range achieved (ratio)
 *  @return            ACTIVE_OK
 *  Complexity: O(1) */
int gmc_autotune_design(double f_ref, double gm_nominal,
                         double gm_range_pct, double loop_bw,
                         double *tune_range);

/* ==================================================================
 * L8: N-Path Filter Design
 * ================================================================== */

/** Design an N-path bandpass filter.
 *  L8: An N-path filter uses N identical paths (each a lowpass RC),
 *  sequentially connected to input/output by rotating switches at
 *  frequency f_sw. The result is a bandpass response centered at f_sw.
 *
 *  Impedance translation principle:
 *    Z_in(f) = (N/π²)·Z_bb(f - f_sw)  (for N even)
 *  where Z_bb is the baseband impedance of each path.
 *
 *  Filter characteristics:
 *    Center frequency: f_sw (switch clock)
 *    Bandwidth: 1/(2π·R·C) of each path's RC LPF
 *    Q = f_sw / BW = f_sw · 2π·R·C
 *
 *  Clock-tunable BP with very high Q from simple RC LPFs.
 *  Harmonic folding: passes signals at f_sw ± k·N·f_sw (k integer).
 *
 *  Reference: Franks & Sandberg, "An Alternative Approach to the
 *            Realization of Network Transfer Functions: The N-Path
 *            Filter," BSTJ, 1960
 *
 *  @param f_center  Desired center frequency (Hz) = f_sw
 *  @param bw        Desired bandwidth (Hz)
 *  @param n_paths   Number of paths (typically 4 or 8)
 *  @param r_scale   Resistance per path (ohms)
 *  @param comp      Output: R and C values per path
 *  @param harmonic_reject Output: harmonic rejection ratio at 3·f_sw (dB)
 *  @return          ACTIVE_OK
 *  Complexity: O(1) */
int npath_bp_design(double f_center, double bw, int n_paths,
                     double r_scale, active_component_values_t *comp,
                     double *harmonic_reject);

/* ==================================================================
 * L9: Research — MEMS Resonator-Based Filters
 * ================================================================== */

/** MEMS resonator equivalent circuit parameters.
 *  L9: Micro-electromechanical resonators can replace LC tanks
 *  in filters with Q > 10,000. The Butterworth-Van Dyke (BVD)
 *  model represents the electromechanical behavior:
 *
 *  R_m: motional resistance (mechanical loss)
 *  L_m: motional inductance (equivalent mass)
 *  C_m: motional capacitance (equivalent spring compliance)
 *  C_0: static capacitance (electrode capacitance)
 *
 *  f_series = 1/(2π·√(L_m·C_m))     — series resonance
 *  f_parallel = f_s·√(1 + C_m/C_0)  — parallel resonance
 *  Q = 1/(2π·f_s·R_m·C_m) = √(L_m/C_m)/R_m */
typedef struct {
    double r_motional;      /* Motional resistance R_m (ohms) */
    double l_motional;      /* Motional inductance L_m (H) */
    double c_motional;      /* Motional capacitance C_m (F) */
    double c_static;        /* Static capacitance C_0 (F) */
    double f_series;        /* Series resonant frequency (Hz) */
    double f_parallel;      /* Parallel resonant frequency (Hz) */
    double q_factor;        /* Quality factor */
    double kt2;             /* Electromechanical coupling coefficient */
} mems_resonator_t;

/** Model MEMS resonator in active filter context.
 *  L9: Incorporates BVD model into an active filter design.
 *  The resonator is used as a frequency-selective feedback element.
 *  Q > 10,000 enables ultra-narrowband filters without high-order
 *  active stages.
 *
 *  @param resonator  MEMS resonator parameters
 *  @param r_load     Load resistance driving the resonator (ohms)
 *  @param gain       Amplifier gain required
 *  @param comp       Output: interface component values
 *  @return           ACTIVE_OK
 *  Complexity: O(1) */
int mems_filter_interface(const mems_resonator_t *resonator,
                           double r_load, double *gain,
                           active_component_values_t *comp);

#ifdef __cplusplus
}
#endif

#endif /* ACTIVE_FILTER_ADVANCED_H */
