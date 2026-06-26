/**
 * active_filter_advanced.c — Switched-Capacitor, Gm-C, N-Path Filters
 *
 * L8 Advanced Topics: Implementation of integrated-circuit filter
 * technologies that go beyond discrete op-amp + RC designs.
 *
 * Switched-Capacitor Filters (SCF):
 *   Replace resistors with capacitor switching at clock rate f_clk.
 *   R_eq = 1/(f_clk·C_switched). Time constants set by capacitor
 *   RATIOS, not absolute values → ±0.1% accuracy in CMOS.
 *
 * Gm-C (OTA-C) Filters:
 *   Use operational transconductance amplifiers (OTAs) instead of
 *   op-amps. Open-loop operation enables GHz bandwidth. Tunable via
 *   bias current. Requires on-chip automatic tuning.
 *
 * N-Path Filters:
 *   Commutated capacitor networks that translate a baseband lowpass
 *   response to a bandpass at the switching frequency. High Q from
 *   simple RC networks.
 *
 * Reference:
 *   Gregorian & Temes, Analog MOS Integrated Circuits for Signal
 *   Processing (1986)
 *   Nauta, "Analog CMOS Filters for Very High Frequencies" (1993)
 *   Franks & Sandberg, "An Alternative Approach to the Realization
 *   of Network Transfer Functions: The N-Path Filter," BSTJ, 1960
 *
 * Courses: Berkeley EE140, Stanford EE214, ETH 227-0455
 */

#include "active_filter_advanced.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ==================================================================
 * L8: Switched-Capacitor Filter Design
 * ================================================================== */

/**
 * sc_integrator_design — Design switched-capacitor integrator.
 *
 * L8: The SC integrator is the fundamental building block of
 * all switched-capacitor filters.
 *
 * Operating principle: During φ₁ (first clock phase), C_switched
 * charges to V_in. During φ₂, C_switched dumps its charge into
 * C_integration. The charge transferred per clock cycle is:
 *
 *   ΔQ = C_switched · V_in[n]
 *
 * The output voltage updates:
 *   V_out[n+1] = V_out[n] - (C_switched/C_integration) · V_in[n]
 *
 * In the z-domain:
 *   H(z) = V_out(z)/V_in(z) = -(C1/C2) · z^{-1} / (1 - z^{-1})
 *
 * Equivalent continuous-time integrator:
 *   H(s) = -1/(s·τ) where τ = C_integration / (f_clk · C_switched)
 *
 * The equivalent resistance is: R_eq = 1/(f_clk · C_switched)
 * This "resistor" is accurate to capacitor matching (~0.1%),
 * far better than physical resistors (~1-5%).
 *
 * Reference: Hosticka, Brodersen & Gray, "MOS Sampled Data
 *            Recursive Filters Using Switched Capacitor
 *            Integrators," IEEE JSSC, vol. SC-12, 1977.
 */
int sc_integrator_design(double tau_desired, double f_clk, double c_max,
                          sc_integrator_t *integrator)
{
    if (!integrator || tau_desired <= 0.0 || f_clk <= 0.0 || c_max <= 0.0)
        return ACTIVE_ERR_NULL_PTR;

    /* τ = C_int / (f_clk · C_sw) = 1/(f_clk · ratio)
     * → ratio = C_sw/C_int = 1/(f_clk · τ) */
    double ratio = 1.0 / (f_clk * tau_desired);

    if (ratio > 1.0) ratio = 1.0;  /* Practical limit */
    if (ratio < 0.001) ratio = 0.001;

    /* Choose C_int = c_max (uses max available capacitance for noise) */
    double c_int = c_max;
    double c_sw = ratio * c_int;

    /* Check practical limits */
    if (c_sw < 0.1e-12) c_sw = 0.1e-12;  /* Min 0.1 pF practical */
    if (c_sw > c_max) c_sw = c_max;

    integrator->clock_freq_hz = f_clk;
    integrator->c_integration = c_int;
    integrator->c_switched = c_sw;
    integrator->c_ratio = c_sw / c_int;
    integrator->equivalent_r = 1.0 / (f_clk * c_sw);
    integrator->integrator_tau = 1.0 / (f_clk * (c_sw / c_int));

    return ACTIVE_OK;
}

/**
 * sc_lowpass_design — First-order SC lowpass filter.
 *
 * L8: SC LPF = SC integrator with unity-gain feedback.
 *
 * Transfer function (z-domain):
 *   H(z) = -(C1/C2) / (1 - z^{-1} · (1 + C1/C2))
 *
 * Magnitude response:
 *   |H(e^{jωT})| = (C1/C2) / √(1 + (1+C1/C2)^2 - 2(1+C1/C2)cos(ωT))
 *
 * -3dB cutoff frequency (for f << f_clk):
 *   f_c ≈ f_clk · (C1/C2) / (2π)
 *
 * @param fc      Desired -3dB frequency (Hz)
 * @param gain_db Low-frequency gain (dB), typically 0 (unity gain)
 * @param f_clk   Clock frequency (Hz), ≥ 50·fc
 * @param c_max   Maximum capacitor value (F)
 * @param comp    Output capacitor values
 * @param ratio   Output: C_switched/C_integration ratio
 */
int sc_lowpass_design(double fc, double gain_db, double f_clk,
                       double c_max, active_component_values_t *comp,
                       double *ratio)
{
    if (!comp || fc <= 0.0 || f_clk <= 0.0 || c_max <= 0.0)
        return ACTIVE_ERR_NULL_PTR;

    /* Clock must be >> signal frequency (oversampling) */
    if (f_clk < 50.0 * fc) return ACTIVE_ERR_INVALID_FREQ;

    (void)gain_db;  /* Unity-gain SC LPF; gain is set by capacitor ratio */

    /* For SC LPF: f_c = f_clk · ratio / (2π) → ratio = 2π·fc/f_clk */
    double cap_ratio = 2.0 * M_PI * fc / f_clk;
    if (cap_ratio > 0.5) cap_ratio = 0.5;  /* Limit for stability */

    /* Choose C_integration = c_max · 0.5 (leaves margin) */
    double c_int = c_max * 0.5;
    double c_sw = cap_ratio * c_int;

    if (c_sw < 0.05e-12) c_sw = 0.05e-12;  /* Practical minimum */

    /* Store capacitor values (using C1, C2 for SC caps) */
    comp->r1 = 0.0;
    comp->r2 = 0.0;
    comp->r3 = 0.0;
    comp->r4 = 0.0;
    comp->r5 = 0.0;
    comp->c1 = c_sw;        /* Switched capacitor */
    comp->c2 = c_int;       /* Integration capacitor */
    comp->c3 = 0.0;

    if (ratio) *ratio = cap_ratio;

    return ACTIVE_OK;
}

/**
 * sc_biquad_design — Second-order switched-capacitor biquad.
 *
 * L8: The Fleischer-Laker SC biquad is the universal building
 * block for high-order SC filters. It uses two SC integrators
 * in a loop, similar to the active-RC Tow-Thomas biquad.
 *
 * ω₀ = f_clk · √(C1·C2 / (CA·CB))
 * Q  = (CA·CB / (C1·C2)) · (CQ / CF)
 *
 * All parameters set by capacitor RATIOS — immune to process
 * variation to the extent that capacitors match.
 *
 * Reference: Fleischer & Laker, "A Family of Active Switched
 *            Capacitor Biquad Building Blocks," Bell System
 *            Technical Journal, vol. 58, pp. 2235-2269, 1979.
 */
int sc_biquad_design(double f0, double q, active_filter_func_t function,
                      double f_clk, double c_max,
                      active_component_values_t *comp)
{
    if (!comp || f0 <= 0.0 || q <= 0.0 || f_clk <= 0.0)
        return ACTIVE_ERR_NULL_PTR;

    double w0 = 2.0 * M_PI * f0;

    /* Normalized frequency: ω₀·T = ω₀/f_clk */
    double w0_norm = w0 / f_clk;
    if (w0_norm > 0.5) return ACTIVE_ERR_INVALID_FREQ;  /* Nyquist */

    /* For Fleischer-Laker biquad:
     * ω₀·T ≈ √(C1·C2) / CA  (simplified for certain configurations)
     * Q ≈ CA / (C_feedback) · (some function)
     *
     * Simplified capacitor assignment:
     * CA = CB = c_max (largest caps)
     * C1 = C2 = CA · w0_norm
     * CQ = CA / (2·Q)  (Q-setting feedback cap)
     */
    double ca = c_max;
    double cb = c_max;
    double c1 = ca * w0_norm;
    double c2 = cb * w0_norm;
    double cq = ca / (2.0 * q);

    (void)function;  /* Function selection could be added */

    comp->r1 = 0.0;  /* No resistors in SC */
    comp->r2 = 0.0;
    comp->r3 = 0.0;
    comp->r4 = 0.0;
    comp->r5 = 0.0;
    comp->c1 = c1;   /* SC capacitor 1 */
    comp->c2 = c2;   /* SC capacitor 2 */
    comp->c3 = cq;   /* Q-setting capacitor */

    return ACTIVE_OK;
}

/**
 * sc_alias_analysis — Compute aliasing in SC filters.
 *
 * L8: SC filters are sampled-data systems. The output spectrum
 * contains images of the input at multiples of f_clk.
 *
 * For an input tone at f_sig:
 * - Baseband: H(f_sig) · V_in
 * - First image: H(f_clk - f_sig) · V_in
 * - Second image: H(f_clk + f_sig) · V_in
 *
 * Alias ratio:
 *   A_dB = 20·log10(|H(f_clk-f_sig)/H(f_sig)| + |H(f_clk+f_sig)/H(f_sig)|)
 *
 * Anti-aliasing filter requirement: f_sig_max < f_clk/2 (Nyquist).
 * In practice, f_sig < f_clk/10 for adequate image suppression.
 */
int sc_alias_analysis(double f_signal, double f_clk, double fc_input,
                       double *alias_db)
{
    if (!alias_db || f_signal <= 0.0 || f_clk <= 0.0)
        return ACTIVE_ERR_NULL_PTR;

    /* First aliased image frequency */
    double f_alias1 = f_clk - f_signal;
    if (f_alias1 < 0) f_alias1 = f_signal - f_clk;

    /* Assume first-order anti-aliasing filter at input: |H(f)| = 1/√(1+(f/fc)²) */
    double h_signal = 1.0 / sqrt(1.0 + (f_signal/fc_input)*(f_signal/fc_input));
    double h_alias1 = 1.0 / sqrt(1.0 + (f_alias1/fc_input)*(f_alias1/fc_input));

    /* Alias ratio */
    double ratio = h_alias1 / h_signal;
    if (ratio < 1e-10) ratio = 1e-10;
    *alias_db = -20.0 * log10(ratio);

    return ACTIVE_OK;
}

/* ==================================================================
 * L8: Gm-C (OTA-C) Filter Design
 * ================================================================== */

/**
 * gmc_integrator_design — Design Gm-C integrator.
 *
 * L8: A simple OTA driving a capacitor acts as an integrator:
 *   V_out = (gm/sC) · V_in
 *   H(s) = gm/(sC) = ω_u/s
 *
 * Unity-gain frequency: ω_u = gm/C
 *
 * For a differential pair OTA:
 *   gm = I_bias / (n·V_T)  (weak inversion, BJT-like)
 *   gm = √(2·μ_n·C_ox·(W/L)·I_bias)  (strong inversion, MOSFET)
 *
 * Tunability: ω_u ∝ I_bias (weak inversion) or √I_bias (strong inv)
 * This enables voltage- or current-controlled filters.
 *
 * Design trade-off:
 * - Higher gm → higher ω_u → higher power
 * - Larger C → lower noise, but lower ω_u for same gm
 */
int gmc_integrator_design(double unity_gain_freq, double c_load,
                           gmc_params_t *params)
{
    if (!params || unity_gain_freq <= 0.0 || c_load <= 0.0)
        return ACTIVE_ERR_NULL_PTR;

    params->c_load = c_load;
    params->gm_c_ratio = unity_gain_freq;  /* ω_u = gm/C */
    params->gm = unity_gain_freq * c_load;

    /* Estimate bias current for strong inversion:
     * gm = √(2·μ_n·C_ox·(W/L)·I_bias)
     * Assume: μ_n·C_ox ≈ 200μA/V², W/L = 100
     * → gm = √(2·200e-6·100·I_bias) = √(0.04·I_bias)
     * → I_bias = gm²/0.04 */
    double kp = 200e-6;  /* μ_n·C_ox for typical CMOS */
    double wl = 100.0;
    params->bias_current = (params->gm * params->gm) / (2.0 * kp * wl);

    /* Linear range (for 1% THD with simple diff pair):
     * V_linear ≈ 2·n·V_T where n ≈ 1.5, V_T ≈ 26mV
     * → V_linear ≈ 78mV */
    params->linear_range_v = 0.078;  /* 2·n·V_T */

    /* Noise: input-referred noise density ≈ 4·k·T·γ/gm
     * (γ ≈ 2/3 for long-channel, ~2 for short-channel) */
    double k_boltzmann = 1.38e-23;
    double temp = 300.0;
    double gamma = 2.0;
    params->noise_density = 4.0 * k_boltzmann * temp * gamma / params->gm;

    /* Power: P = V_dd · I_bias · N_otas
     * Assume V_dd = 1.8V */
    params->power_w = 1.8 * params->bias_current;

    return ACTIVE_OK;
}

/**
 * gmc_biquad_design — Gm-C biquad design.
 *
 * L8: Gm-C biquad uses 4 OTAs and 2 capacitors:
 *
 * OTA1: Input transconductor (gm1)
 * OTA2: Feedback transconductor (gm2) — sets Q
 * OTA3: First integrator output driver (gm3)
 * OTA4: Second integrator output driver (gm4)
 *
 * ω₀ = √(gm3·gm4/(C1·C2))
 * Q  = √(C2/C1)·(gm1/gm2) (approximate)
 *
 * Common design: gm1=gm2=gm3=gm4=gm, C1=C2=C
 * → ω₀ = gm/C, Q = gm1/gm2
 */
int gmc_biquad_design(double f0, double q, active_filter_func_t function,
                       double c_max, gmc_params_t *params,
                       active_component_values_t *comp)
{
    if (!params || !comp || f0 <= 0.0 || q <= 0.0 || c_max <= 0.0)
        return ACTIVE_ERR_NULL_PTR;

    double w0 = 2.0 * M_PI * f0;

    /* Equal-gm, equal-C design */
    double c = c_max * 0.5;  /* Use half max for headroom */
    double gm = w0 * c;       /* ω₀ = gm/C */

    /* For LP and HP (all-pole), use canonical Gm-C biquad.
     * Q-setting via transconductor ratio: gm_q = gm / q */
    (void)(gm / q);  /* gm_q — reserved for future Q-tuning implementation */

    /* Design the primary OTA */
    gmc_integrator_design(w0, c, params);

    /* Store capacitors */
    comp->r1 = 0.0;
    comp->r2 = 0.0;
    comp->r3 = 0.0;
    comp->r4 = 0.0;
    comp->r5 = 0.0;
    comp->c1 = c;   /* Integrator C1 */
    comp->c2 = c;   /* Integrator C2 */
    comp->c3 = 0.0;

    (void)function;
    return ACTIVE_OK;
}

/**
 * gmc_thd_estimate — Estimate THD in Gm-C filter.
 *
 * L8: OTA nonlinearity is the dominant distortion source in Gm-C filters.
 *
 * Simple differential pair: I_out = I_bias · tanh(V_diff/(2·n·V_T))
 *
 * Taylor expansion: tanh(x) ≈ x - x³/3 + 2x⁵/15 - ...
 * For V_diff << 2·n·V_T: near-linear.
 * For V_diff approaching 2·n·V_T: significant third-harmonic.
 *
 * THD ≈ (1/32) · (V_pk / (n·V_T))²  (for simple diff pair)
 *
 * Source degeneration reduces THD by a factor of (1+N)² where
 * N = gm·R_degen is the degeneration factor. But it also reduces
 * effective gm by the same factor.
 */
double gmc_thd_estimate(const gmc_params_t *params, double vpk,
                         int with_degenerate)
{
    if (!params || vpk <= 0.0) return 1.0;

    double n_vt = 0.039;  /* n·V_T at room temp (1.5·26mV) */
    double thd = (1.0 / 32.0) * (vpk / n_vt) * (vpk / n_vt);

    if (with_degenerate) {
        /* Degeneration reduces THD ~ (1+N)², but N ≈ 5-10 typical */
        double degen = 5.0;
        thd /= ((1.0 + degen) * (1.0 + degen));
    }

    if (thd > 1.0) thd = 1.0;
    return thd;
}

/**
 * gmc_autotune_design — On-chip automatic tuning for Gm-C.
 *
 * L8: Gm-C filters require automatic tuning because gm varies
 * ±30% over process, voltage, and temperature (PVT).
 *
 * Master-slave tuning:
 * - Master: a Gm-C integrator locked to a reference clock
 * - PLL-like loop adjusts bias current until gm/C = ω_ref
 * - Slave: copies the bias voltage/current to filter OTAs
 *
 * Tuning range must cover gm variation:
 *   I_bias_range ≥ (gm_max/gm_min)² for strong inversion
 *   or (gm_max/gm_min) for weak inversion
 */
int gmc_autotune_design(double f_ref, double gm_nominal,
                         double gm_range_pct, double loop_bw,
                         double *tune_range)
{
    if (!tune_range || f_ref <= 0.0 || gm_nominal <= 0.0)
        return ACTIVE_ERR_NULL_PTR;

    /* Required tuning range ratio:
     * For strong inversion: gm ∝ √I_bias → I_range = gm_range²
     * For weak inversion: gm ∝ I_bias → I_range = gm_range */
    double gm_ratio = 1.0 + gm_range_pct / 100.0;
    double i_range_strong = gm_ratio * gm_ratio;

    /* Use strong inversion estimate (worst case) */
    *tune_range = i_range_strong;

    /* Loop considerations:
     * - Lock range: must cover worst-case initial gm error
     * - Capture range: ±50% of lock range typical
     * - Lock time: ~10/loop_bw */
    (void)loop_bw;  /* Could be used for lock time estimation */

    return ACTIVE_OK;
}

/* ==================================================================
 * L8: N-Path Filter
 * ================================================================== */

/**
 * npath_bp_design — Design N-path bandpass filter.
 *
 * L8: An N-path filter creates a high-Q bandpass response
 * centered at the switching frequency using simple RC lowpass
 * filters in each path.
 *
 * Principle (impedance translation):
 *   Z_in(f) = (N/π²) · Z_bb(f - f_sw)
 *
 * where Z_bb is the baseband impedance of each path's RC LPF.
 *
 * For an RC LPF in each path:
 *   H_BB(s) = 1/(1 + sRC)
 *   → BP response: |H_BP(f)| centered at f_sw with BW = 1/(2πRC)
 *   → Q = f_sw / BW = f_sw · 2πRC
 *
 * Advantages:
 *   - Very high Q achievable (100-1000) from simple RC
 *   - Center frequency set by clock (easily tunable)
 *   - No precision capacitors or trimming needed
 *
 * Disadvantages:
 *   - Harmonic folding: responds at f_sw ± k·N·f_sw (k≥1)
 *   - N-phase clock generation required
 *   - Switch resistance affects Q and noise
 *
 * Reference: Ghaffari, Klumperink, Soer & Nauta, "Tunable
 *            High-Q N-Path Band-Pass Filters: Modeling and
 *            Verification," IEEE JSSC, vol. 46, 2011.
 */
int npath_bp_design(double f_center, double bw, int n_paths,
                     double r_scale, active_component_values_t *comp,
                     double *harmonic_reject)
{
    if (!comp || f_center <= 0.0 || bw <= 0.0 || n_paths < 2)
        return ACTIVE_ERR_NULL_PTR;

    /* Each path is an RC LPF with fc = BW */
    double r = r_scale;
    double c = 1.0 / (2.0 * M_PI * bw * r);

    comp->r1 = r;
    comp->r2 = 0.0;
    comp->r3 = 0.0;
    comp->r4 = 0.0;
    comp->r5 = (double)n_paths;  /* Store N in R5 */
    comp->c1 = c;
    comp->c2 = 0.0;
    comp->c3 = 0.0;

    /* Harmonic rejection at 3rd harmonic:
     * At f = 3·f_sw, the baseband response is |H_BB(2·f_sw)|
     * (because the desired signal is at f_sw, worst alias at 3f_sw).
     * HR ≈ 1/|H_BB(2·f_sw)| ≈ √(1+(2f_sw/BW)²) ≈ 2·Q */
    double q = f_center / bw;
    if (harmonic_reject) {
        *harmonic_reject = 20.0 * log10(2.0 * q);
    }

    return ACTIVE_OK;
}

/* ==================================================================
 * L9: MEMS Resonator Interface
 * ================================================================== */

/**
 * mems_filter_interface — Interface MEMS resonator to active filter.
 *
 * L9: MEMS resonators achieve Q > 10,000 at MHz frequencies,
 * far exceeding what's possible with RC-active or even LC filters.
 * However, they require specific impedance matching and gain.
 *
 * BVD model: R_m + L_m + C_m in series, all in parallel with C_0.
 *
 * The motional resistance R_m is typically 50Ω-10kΩ.
 * The motional inductance L_m is enormous (mH-kH equivalent).
 * C_m is femtofarads, C_0 is a few pF.
 *
 * For narrowband BP filtering:
 * - f_0 = 1/(2π·√(L_m·C_m)) (series resonance)
 * - The resonator is placed in a bridge or feedback configuration
 * - An amplifier provides the gain needed to overcome R_m losses
 * - Overall Q ≈ Q_mems (preserved through active interface)
 *
 * Reference: Nguyen, "MEMS Technology for Timing and Frequency
 *            Control," IEEE Trans. UFFC, vol. 54, 2007.
 */
int mems_filter_interface(const mems_resonator_t *resonator,
                           double r_load, double *gain,
                           active_component_values_t *comp)
{
    if (!resonator || !gain || !comp || r_load <= 0.0)
        return ACTIVE_ERR_NULL_PTR;

    /* The MEMS resonator at series resonance: impedance = R_m (purely resistive)
     * Need to match to amplifier input.
     *
     * For a transimpedance amplifier configuration:
     *   Z_feedback = R_f (transimpedance gain)
     *   Input current through resonator at resonance: i_in = V_in / R_m
     *   V_out = -i_in · R_f = -V_in · R_f/R_m
     *
     * Required gain to overcome losses:
     *   gain_needed = 2·R_m / r_load (for unity overall gain) */
    *gain = 2.0 * resonator->r_motional / r_load;

    /* Interface components */
    comp->r1 = resonator->r_motional;  /* Motional resistance */
    comp->r2 = r_load;                 /* Load resistance */
    comp->r3 = 0.0;
    comp->r4 = 0.0;
    comp->r5 = 0.0;
    comp->c1 = resonator->c_motional;
    comp->c2 = resonator->c_static;
    comp->c3 = 0.0;

    return ACTIVE_OK;
}
