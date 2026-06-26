/**
 * rectifier_topology.c -- Rectifier Topology Analysis Implementation
 *
 * Implements analysis of half-wave, full-wave center-tapped, bridge,
 * voltage multiplier, and SCR phase-controlled rectifiers.
 *
 * Reference: Sedra & Smith Ch.4; Rashid Ch.3-4; Mohan Ch.5
 */

#include "rectifier_topology.h"
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * L5: Half-Wave Rectifier Analysis
 * ============================================================================ */

HalfWaveResult rectifier_half_wave_analyze(const RectifierConfig *cfg)
{
    /* Half-wave rectifier analysis with resistive load.
     *
     * The half-wave rectified sine wave v(t) = V_m*sin(omega*t) for 0 < t < T/2,
     * zero otherwise. Its Fourier series is:
     *   v(t) = V_m/pi + (V_m/2)*sin(omega*t) - (2*V_m/pi)*sum_{k=1}^{inf} cos(2k*omega*t)/(4k^2-1)
     *
     * Key derived quantities:
     *   V_dc = (1/T) * integral_{0}^{T/2} V_m*sin(wt) dt = V_m / pi
     *   V_rms^2 = (1/T) * integral_{0}^{T/2} V_m^2*sin^2(wt) dt = V_m^2 / 4
     *   V_rms_output = V_m / 2
     *
     * With diode forward drop V_f (piecewise model):
     *   Effective peak = V_m - V_f
     *   V_dc = (V_m - V_f) / pi
     *   Conduction occurs only when V_in > V_f.
     *
     * PIV = V_m (maximum reverse voltage = negative peak of input)
     *
     * Ripple factor (formula from Fourier analysis):
     *   RF = sqrt( (V_rms/V_dc)^2 - 1 ) = sqrt( (pi/2)^2 - 1 ) ≈ 1.211
     *   This high ripple (121%) is why half-wave rectifiers need large filters.
     *
     * Efficiency: eta = P_dc / P_ac = (V_dc^2/R) / (V_rms^2/R)
     *   = (V_m/pi)^2 / (V_m/2)^2 = 4/pi^2 ≈ 0.405 or 40.5%
     *
     * TUF (Transformer Utilization Factor):
     *   TUF = P_dc / (V_s * I_s) where V_s, I_s are transformer secondary ratings.
     *   For half-wave: TUF = 0.287 (very poor — transformer carries DC).
     *
     * Ref: Sedra & Smith Example 4.5, Rashid Sec.3.2
     */
    HalfWaveResult r = {0};

    if (!cfg) return r;

    double V_m = cfg->V_in_peak;
    if (V_m <= cfg->V_f) return r;  /* No conduction possible */

    double V_eff = V_m - cfg->V_f;  /* Effective peak after diode drop */

    r.V_dc = V_eff / M_PI;
    r.V_rms_output = V_eff / 2.0;
    r.I_dc = r.V_dc / cfg->R_load;
    r.I_rms = r.V_rms_output / cfg->R_load;
    r.P_dc = r.V_dc * r.I_dc;
    r.P_ac = r.V_rms_output * r.I_rms;
    r.PIV = V_m;  /* Peak inverse voltage = V_m for half-wave */
    r.form_factor = r.V_rms_output / r.V_dc;  /* ~1.571 */

    /* Ripple factor = sqrt(FF^2 - 1) */
    r.ripple_factor = sqrt(r.form_factor * r.form_factor - 1.0);  /* ~1.211 */

    /* Efficiency = P_dc / P_ac */
    if (r.P_ac > 1e-15)
        r.efficiency = r.P_dc / r.P_ac;  /* ~0.405 */

    /* TUF = P_dc / (V_rms_input * I_rms) */
    double V_rms_input = V_m / 1.41421356;
    if (V_rms_input > 0.0 && r.I_rms > 0.0)
        r.TUF = r.P_dc / (V_rms_input * r.I_rms);

    /* Peak repetitive current = V_m / R_load */
    r.peak_repetitive_current = V_m / cfg->R_load;

    /* Conduction angle: diode conducts from arcsin(V_f/V_m) to pi - arcsin(V_f/V_m) */
    double theta_on = asin(cfg->V_f / V_m);
    r.conduction_angle_rad = M_PI - 2.0 * theta_on;

    return r;
}

/* ============================================================================
 * L5: Full-Wave Center-Tapped Analysis
 * ============================================================================ */

FullWaveCTResult rectifier_fullwave_ct_analyze(const RectifierConfig *cfg)
{
    /* Full-wave center-tapped rectifier.
     *
     * Uses a center-tapped transformer secondary. Each half-secondary
     * provides V_m (not 2*V_m). Diodes conduct alternately.
     *
     * Output waveform: |V_m * sin(wt)| for full period (absolute value of sine).
     * Fourier series of full-wave rectified sine:
     *   v(t) = 2*V_m/pi - (4*V_m/pi)*sum_{k=1}^{inf} cos(2k*omega*t)/(4k^2-1)
     *
     * V_dc = 2 * V_m / pi            (twice half-wave!)
     * V_rms = V_m / sqrt(2)
     *
     * PIV = 2 * V_m  ← CRITICAL! Each diode sees the full secondary voltage
     *                  when the other diode conducts, because the CT point
     *                  is at V_m during the opposing half-cycle.
     *
     * Efficiency = 8/pi^2 ≈ 0.812 (81.2%)
     * TUF = 0.693 (center-tapped transformer winding utilization)
     *
     * Ripple factor = sqrt( (pi/(2*sqrt(2)))^2 - 1 ) ≈ 0.482 (48.2%)
     * Ripple frequency = 2 * f_line
     *
     * Ref: Sedra & Smith Sec.4.5.2, Rashid Sec.3.3
     */
    FullWaveCTResult r = {0};

    if (!cfg) return r;

    double V_m = cfg->V_in_peak;
    if (V_m <= cfg->V_f) return r;

    double V_eff = V_m - cfg->V_f;

    r.V_dc = 2.0 * V_eff / M_PI;
    r.V_rms_output = V_eff / 1.4142135623730951;
    r.I_dc = r.V_dc / cfg->R_load;
    r.I_rms = r.V_rms_output / cfg->R_load;
    r.P_dc = r.V_dc * r.I_dc;
    r.P_ac = r.V_rms_output * r.I_rms;
    r.PIV = 2.0 * V_m;  /* Key: twice the peak! */
    r.ripple_freq_Hz = 2.0 * cfg->freq_Hz;

    r.form_factor = r.V_rms_output / r.V_dc;
    r.ripple_factor = sqrt(r.form_factor * r.form_factor - 1.0);

    if (r.P_ac > 1e-15)
        r.efficiency = r.P_dc / r.P_ac;

    double V_rms_input = V_m / 1.41421356;
    if (V_rms_input > 0.0 && r.I_rms > 1e-15)
        r.TUF = r.P_dc / (V_rms_input * r.I_rms);

    r.peak_repetitive_current = V_m / cfg->R_load;
    r.diode_avg_current = r.I_dc / 2.0;  /* Each diode conducts half the time */

    return r;
}

/* ============================================================================
 * L5: Bridge Rectifier Analysis
 * ============================================================================ */

BridgeResult rectifier_bridge_analyze(const RectifierConfig *cfg)
{
    /* Full-wave bridge rectifier.
     *
     * Four diodes in a Graetz bridge configuration. Two diodes conduct
     * during each half-cycle, so total forward drop = 2 * V_f.
     *
     * Output waveform: identical to full-wave CT = |V_m*sin(wt)|
     *
     * V_dc = 2 * V_m / pi - 2 * V_f    (two diode drops subtracted)
     * PIV = V_m - V_f                  (half of CT configuration!)
     *
     * The bridge's key advantage over CT:
     *   - PIV is halved (V_m vs 2*V_m)
     *   - No center-tapped transformer needed
     *   - Better TUF (0.812 vs 0.693)
     *   - Diode count is higher (4 vs 2)
     *
     * Efficiency = 8/pi^2 ≈ 0.812 (ideal), reduced by 2*V_f in practice.
     *
     * Ref: Sedra & Smith Sec.4.5.3, Rashid Sec.3.4
     */
    BridgeResult r = {0};

    if (!cfg) return r;

    double V_m = cfg->V_in_peak;
    r.total_diode_drop = 2.0 * cfg->V_f;

    if (V_m <= r.total_diode_drop) return r;

    double V_eff = V_m - r.total_diode_drop;

    r.V_dc = 2.0 * V_eff / M_PI;
    r.V_rms_output = V_eff / 1.4142135623730951;
    r.I_dc = r.V_dc / cfg->R_load;
    r.I_rms = r.V_rms_output / cfg->R_load;
    r.P_dc = r.V_dc * r.I_dc;
    r.P_ac = r.V_rms_output * r.I_rms;
    r.PIV = V_m - cfg->V_f;  /* Bridge PIV = V_m - V_f */
    r.ripple_freq_Hz = 2.0 * cfg->freq_Hz;

    r.form_factor = r.V_rms_output / r.V_dc;
    r.ripple_factor = sqrt(r.form_factor * r.form_factor - 1.0);

    if (r.P_ac > 1e-15)
        r.efficiency = r.P_dc / r.P_ac;

    double V_rms_input = V_m / 1.41421356;
    if (V_rms_input > 0.0 && r.I_rms > 1e-15)
        r.TUF = r.P_dc / (V_rms_input * r.I_rms * 0.707);  /* Adjusted for bridge */

    r.peak_repetitive_current = V_m / cfg->R_load;
    r.diode_avg_current = r.I_dc / 2.0;

    return r;
}

/* ============================================================================
 * L5: Voltage Multiplier Analysis
 * ============================================================================ */

VoltageMultiplierResult rectifier_voltage_multiplier_analyze(
    const VoltageMultiplierConfig *cfg)
{
    /* Cockroft-Walton voltage multiplier analysis.
     *
     * Invented by John Cockroft and Ernest Walton in 1932 at the
     * Cavendish Laboratory, Cambridge, to accelerate particles.
     * They won the 1951 Nobel Prize for this work.
     *
     * N-stage multiplier:
     *   No-load output: V_out = 2 * N * V_m
     *   Loaded ripple:   V_ripple = I_load/(f*C) * N*(N+1)/2
     *   Voltage drop:    V_drop = I_load/(f*C) * (2*N^3/3 + N^2/2 - N/6)
     *
     * These formulas come from analyzing the charge transfer per cycle.
     * Every capacitor except the first transfers charge twice per cycle
     * (once charging, once discharging), causing the N^3 dependence of
     * the voltage drop at high stage counts.
     *
     * The output impedance is approximately:
     *   Z_out ≈ 1/(f*C) * (2*N^3/3 + N^2/2 - N/6)
     *
     * Ref: Cockroft & Walton (1932), Proc. Royal Society A, 136, 619-630
     */
    VoltageMultiplierResult r = {0};

    if (!cfg || cfg->n_stages < 1) return r;

    int N = cfg->n_stages;
    double V_m = cfg->V_in_peak;
    double f = cfg->freq_Hz;
    double C = cfg->C_stage;

    /* No-load output voltage */
    r.V_out_no_load = 2.0 * N * (V_m - cfg->V_f);

    /* Under load */
    if (cfg->I_load > 0.0 && f > 0.0 && C > 0.0) {
        /* Ripple: V_ripple = I_load / (f * C) * N*(N+1)/2 */
        r.V_ripple_pp = cfg->I_load / (f * C) * (N * (N + 1.0) / 2.0);

        /* Voltage drop */
        double N_d = (double)N;
        r.V_drop = cfg->I_load / (f * C) *
                   (2.0 * N_d * N_d * N_d / 3.0 + N_d * N_d / 2.0 - N_d / 6.0);

        r.V_out_load = r.V_out_no_load - r.V_drop;
        if (r.V_out_load < 0.0) r.V_out_load = 0.0;

        /* Output impedance */
        r.output_impedance = r.V_drop / cfg->I_load;
    } else {
        r.V_out_load = r.V_out_no_load;
        r.V_ripple_pp = 0.0;
        r.V_drop = 0.0;
        r.output_impedance = 0.0;
    }

    r.PIV_per_stage = 2.0 * V_m;
    r.effective_stages = N;

    return r;
}

/* ============================================================================
 * L5: SCR Phase-Controlled Rectifier
 * ============================================================================ */

SCRRectifierResult rectifier_scr_analyze(const SCRRectifierConfig *cfg)
{
    /* Phase-controlled rectifier using SCRs (thyristors).
     *
     * By delaying the turn-on (firing angle alpha), the average DC
     * output voltage is reduced, allowing controlled rectification.
     *
     * For single-phase bridge:
     *   V_dc(alpha) = V_m/pi * (1 + cos(alpha))
     *   V_rms(alpha) = V_m * sqrt( (pi - alpha + sin(2*alpha)/2) / (2*pi) )
     *
     * For alpha=0: full conduction (same as diode bridge)
     * For alpha=pi/2: V_dc = V_m/pi ≈ 0.318*V_m
     * For alpha=pi: V_dc = 0 (no conduction)
     *
     * Displacement factor: cos(alpha)
     *   The fundamental component of input current lags the voltage by alpha.
     *   This is a key disadvantage: SCR rectifiers have poor power factor
     *   at reduced output voltage.
     *
     * Applications:
     *   - Toyota hybrid vehicle motor generators use PWM rectifiers
     *     (active front-end) to avoid this power factor issue
     *   - Legacy DC motor drives (Detroit auto plants)
     *   - Maglev train power supplies (often 12-pulse SCR)
     *
     * Ref: Rashid Ch.4, Mohan Ch.5.3
     */
    SCRRectifierResult r = {0};

    if (!cfg) return r;

    double V_m = cfg->V_in_rms * 1.4142135623730951;
    double alpha = cfg->alpha_rad;

    /* Clamp alpha to [0, pi] */
    if (alpha < 0.0) alpha = 0.0;
    if (alpha > M_PI) alpha = M_PI;

    /* Bridge type */
    r.V_dc = V_m / M_PI * (1.0 + cos(alpha));
    if (r.V_dc < 0.0) r.V_dc = 0.0;

    /* RMS output voltage */
    double term = M_PI - alpha + sin(2.0 * alpha) / 2.0;
    r.V_rms_output = V_m * sqrt(term / (2.0 * M_PI));

    r.I_dc = r.V_dc / cfg->R_load;
    r.P_dc = r.V_dc * r.I_dc;
    r.displacement_factor = cos(alpha);
    r.power_factor = r.displacement_factor;  /* Simplified (no harmonic contribution) */
    r.firing_angle_deg = alpha * 180.0 / M_PI;

    return r;
}

/* ============================================================================
 * L2: Optimal Topology Selection
 * ============================================================================ */

RectifierType rectifier_select_topology(double V_in_rms, double I_load,
                                        int need_isolation, int cost_sensitive)
{
    /* Heuristic topology selection based on design constraints.
     *
     * Decision tree:
     * 1. Very low power (<1W), cost-sensitive: half-wave (simplest)
     * 2. Low power (1-100W), no isolation needed: bridge rectifier
     * 3. Medium power with isolation: full-wave CT or bridge with transformer
     * 4. High voltage needed from low AC: voltage multiplier
     * 5. High power (>1kW): three-phase bridge
     * 6. Variable output needed: SCR-controlled
     *
     * This is a simplified decision engine — real designs consider
     * many more factors (EMI, power factor, ripple, thermal, cost).
     */
    if (cost_sensitive && I_load < 0.1 && V_in_rms < 24.0)
        return RECTIFIER_HALF_WAVE;

    if (I_load > 10.0)
        return RECTIFIER_THREE_PHASE_BRIDGE;

    if (need_isolation)
        return RECTIFIER_FULL_WAVE_CT;

    return RECTIFIER_BRIDGE;  /* Default: bridge is the workhorse */
}

/* ============================================================================
 * L2: Rectifier Comparison
 * ============================================================================ */

void rectifier_compare_topologies(RectifierType t1, RectifierType t2,
                                  const RectifierConfig *cfg)
{
    /* Side-by-side comparison of two rectifier topologies.
     * Educational tool for understanding trade-offs.
     */
    if (!cfg) {
        printf("Error: NULL configuration\n");
        return;
    }

    printf("=== Rectifier Topology Comparison ===\n");
    printf("%-20s %-20s\n", "Metric", "Value");
    printf("----------------------------------------\n");

    /* Analyze both topologies and print key metrics */
    double V_m = cfg->V_in_peak;

    printf("Input: V_rms=%.2f V, V_peak=%.2f V, f=%.1f Hz, R_load=%.1f ohm\n",
           cfg->V_in_rms, V_m, cfg->freq_Hz, cfg->R_load);
    printf("Diode forward drop V_f = %.2f V\n\n", cfg->V_f);

    /* Type 1 analysis */
    if (t1 == RECTIFIER_HALF_WAVE) {
        HalfWaveResult hw = rectifier_half_wave_analyze(cfg);
        printf("[Half-Wave]\n");
        printf("  V_dc = %.3f V, V_rms = %.3f V\n", hw.V_dc, hw.V_rms_output);
        printf("  PIV = %.1f V, Ripple = %.3f, Eff = %.1f%%\n",
               hw.PIV, hw.ripple_factor, hw.efficiency * 100.0);
        printf("  TUF = %.3f, Conduction angle = %.2f rad\n",
               hw.TUF, hw.conduction_angle_rad);
    } else if (t1 == RECTIFIER_BRIDGE) {
        BridgeResult br = rectifier_bridge_analyze(cfg);
        printf("[Bridge]\n");
        printf("  V_dc = %.3f V, V_rms = %.3f V\n", br.V_dc, br.V_rms_output);
        printf("  PIV = %.1f V, Ripple = %.3f, Eff = %.1f%%\n",
               br.PIV, br.ripple_factor, br.efficiency * 100.0);
        printf("  TUF = %.3f, Ripple freq = %.0f Hz\n",
               br.TUF, br.ripple_freq_Hz);
    }

    printf("----------------------------------------\n");
}

/* ============================================================================
 * L6: Quick DC Voltage Estimate
 * ============================================================================ */

double rectifier_estimate_dc_voltage(RectifierType type, double V_in_peak,
                                     double V_f)
{
    /* Quick V_dc estimation for design prototyping.
     *
     * Half-wave:  V_dc ≈ V_m/pi ≈ 0.318 * V_m
     * Full-wave:  V_dc ≈ 2*V_m/pi ≈ 0.637 * V_m
     * Bridge:     V_dc ≈ 2*V_m/pi - 2*V_f
     * Three-phase: V_dc ≈ 1.35 * V_ll_rms (for bridge)
     *
     * These are no-load ideal values. Actual V_dc under load is lower
     * due to filter capacitor discharge and transformer regulation.
     */
    switch (type) {
        case RECTIFIER_HALF_WAVE:
            return (V_in_peak - V_f) / M_PI;
        case RECTIFIER_FULL_WAVE_CT:
            return 2.0 * (V_in_peak - V_f) / M_PI;
        case RECTIFIER_BRIDGE:
            return 2.0 * V_in_peak / M_PI - 2.0 * V_f;
        case RECTIFIER_THREE_PHASE_BRIDGE:
            return 1.35 * V_in_peak;  /* V_in_peak here interpreted as V_ll_rms */
        default:
            return 2.0 * V_in_peak / M_PI;  /* Default: full-wave approximation */
    }
}

/* ============================================================================
 * L6: PIV Calculation
 * ============================================================================ */

double rectifier_calculate_piv(RectifierType type, double V_in_peak, double V_f)
{
    /* Peak Inverse Voltage — the critical voltage rating for diodes.
     *
     * The diode must withstand this reverse voltage without breakdown.
     * A safety margin of 1.5x to 2x is typically applied in practical designs.
     *
     * Half-wave: PIV = V_m (diode blocks negative half-cycle)
     * Full-wave CT: PIV = 2*V_m (one diode sees the full winding voltage)
     * Bridge: PIV = V_m (each diode only sees the line voltage)
     *
     * The reason full-wave CT has 2x PIV:
     * When D1 conducts, the anode of D2 is at -V_m (through the conducting
     * half-winding), while its cathode is at +V_m (through D1's conduction).
     * Total reverse voltage: V_m - (-V_m) = 2*V_m.
     */
    switch (type) {
        case RECTIFIER_HALF_WAVE:
            return V_in_peak;
        case RECTIFIER_FULL_WAVE_CT:
            return 2.0 * V_in_peak;
        case RECTIFIER_BRIDGE:
            return V_in_peak - V_f;
        case RECTIFIER_THREE_PHASE_BRIDGE:
            return V_in_peak;  /* V_in_peak = V_ll_peak */
        default:
            return V_in_peak;
    }
}

/* ============================================================================
 * L2: Ripple Frequency
 * ============================================================================ */

double rectifier_ripple_frequency(RectifierType type, double line_freq_Hz)
{
    /* Fundamental ripple frequency at the rectifier output.
     *
     * This determines the filter capacitor size (lower f_ripple → larger C needed).
     *
     * Half-wave: f_ripple = f_line      (50/60 Hz ripple)
     * Full-wave: f_ripple = 2 * f_line   (100/120 Hz ripple)
     * 3-phase half: f_ripple = 3 * f_line (150/180 Hz)
     * 3-phase bridge: f_ripple = 6 * f_line (300/360 Hz)
     *
     * Higher ripple frequency = smaller filter components = higher power density.
     * This is why aircraft (Boeing 787) use 400 Hz AC: smaller transformers and filters.
     */
    switch (type) {
        case RECTIFIER_HALF_WAVE:
            return line_freq_Hz;
        case RECTIFIER_FULL_WAVE_CT:
        case RECTIFIER_BRIDGE:
            return 2.0 * line_freq_Hz;
        case RECTIFIER_THREE_PHASE_HALF:
            return 3.0 * line_freq_Hz;
        case RECTIFIER_THREE_PHASE_BRIDGE:
            return 6.0 * line_freq_Hz;
        default:
            return 2.0 * line_freq_Hz;
    }
}