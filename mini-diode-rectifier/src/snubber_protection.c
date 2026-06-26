/**
 * snubber_protection.c -- Snubber Circuits and Diode Protection Implementation
 *
 * Implements RC snubber design, reverse recovery analysis, RCD clamp design,
 * transformer ringing damper, TVS selection, and wide-bandgap comparison.
 *
 * Reference: Mohan, Undeland & Robbins Ch.28; Pressman Ch.7; Paul Ch.12
 */

#include "snubber_protection.h"
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * L5: RC Turn-off Snubber Design
 * ============================================================================ */

RCSnubberResult snubber_rc_design(const RCSnubberConfig *cfg)
{
    /* Optimized RC turn-off snubber design.
     *
     * Design procedure (McMurray 1972):
     *
     * 1. Energy to be absorbed: E_L = 0.5 * L_parasitic * I_load^2
     *    This energy must be stored in the snubber capacitor.
     *
     * 2. Snubber capacitance: C_s = 2 * E_L / (V_overshoot^2)
     *    = L_parasitic * I_load^2 / (V_overshoot^2)
     *    where V_overshoot = overshoot_fraction * V_bus
     *
     * 3. Snubber resistance for critical damping:
     *    R_s = sqrt(L_parasitic / C_s)
     *    For under-damped: ringing but lower R_s loss.
     *    For over-damped: no ringing but higher initial voltage stress.
     *    Critical damping (zeta=1) is usually the best compromise.
     *
     * 4. Power in R_s: P_Rs = C_s * V_bus^2 * f_switching
     *    This energy is lost each cycle. At high frequency,
     *    snubber losses become significant.
     *
     * 5. dv/dt achieved: dv/dt = V_bus / (R_s * C_s)
     *    Must be below the diode's maximum dv/dt rating.
     *
     * Example: V_bus=400V, I_load=10A, L_p=100nH, f_sw=100kHz:
     *   E_L = 0.5 * 100e-9 * 100 = 5 uJ
     *   For 20% overshoot: V_overshoot = 80V
     *   C_s = 100e-9 * 100 / (80^2) = 1.56 nF
     *   R_s = sqrt(100e-9 / 1.56e-9) = 8.0 ohm
     *   P_Rs = 1.56e-9 * 400^2 * 100e3 = 25 W (significant!)
     *
     * This power loss is why "lossless snubbers" were developed
     * (Cuk snubber, Undeland snubber) for high-frequency applications.
     *
     * Ref: W. McMurray, "Optimum Snubbers for Power Semiconductors",
     *      IEEE Trans. IA, Vol. IA-8, 1972.
     */
    RCSnubberResult r = {0};

    if (!cfg || cfg->L_parasitic <= 0.0) return r;

    double L_p = cfg->L_parasitic;
    double I = cfg->I_load;
    double V_bus = cfg->V_bus;
    double f_sw = cfg->f_switching;
    double dv_dt_max = cfg->dv_dt_max;
    double overshoot_frac = cfg->overshoot_fraction;

    if (overshoot_frac <= 0.0) overshoot_frac = 0.2;  /* Default: 20% */

    double V_overshoot = overshoot_frac * V_bus;
    if (V_overshoot < 1.0) V_overshoot = 1.0;

    /* Energy-based C_s */
    r.C_s = L_p * I * I / (V_overshoot * V_overshoot);

    /* Critical damping R_s */
    r.R_s = sqrt(L_p / r.C_s);

    /* Power dissipation in R_s */
    r.P_Rs = r.C_s * V_bus * V_bus * f_sw;

    /* dv/dt with snubber */
    r.dv_dt_actual = V_bus / (r.R_s * r.C_s);

    /* Peak overshoot with snubber */
    r.V_overshoot_peak = V_bus * (1.0 + overshoot_frac * 0.5);

    /* Settling time (5 tau) */
    r.t_settle = 5.0 * r.R_s * r.C_s;

    /* Damping factor */
    double zeta = r.R_s / (2.0 * sqrt(L_p / r.C_s));
    r.damping_factor = zeta;

    /* Validate */
    r.is_valid = 1;
    if (dv_dt_max > 0.0 && r.dv_dt_actual > dv_dt_max) r.is_valid = 0;
    if (r.P_Rs > 100.0) r.is_valid = 0;  /* Excessive snubber loss */

    return r;
}

/* ============================================================================
 * L5: Reverse Recovery Analysis
 * ============================================================================ */

ReverseRecoveryResult snubber_reverse_recovery_analyze(
    const ReverseRecoveryParams *params)
{
    /* Reverse recovery analysis using the charge model.
     *
     * When a diode commutates from forward to reverse:
     *
     * Phase t_a (storage phase):
     *   Stored minority carriers are removed. Current decreases from +I_F
     *   through zero to -I_rrm at a rate di/dt.
     *   t_a = I_rrm / (di/dt) for approximately triangular recovery.
     *
     * Phase t_b (fall phase):
     *   The depletion region forms. Current returns from -I_rrm to 0.
     *   t_b depends on the "snappiness" of the diode.
     *
     * S-factor = t_b / t_a:
     *   S < 0.8: Soft recovery — gentle, low EMI
     *   S ≈ 1.0: Normal recovery
     *   S > 1.0: Snappy recovery — abrupt, high EMI, but lower E_rr
     *
     * Reverse recovery charge: Q_rr = 0.5 * I_rrm * t_rr (triangular model)
     *
     * Energy loss: E_rr ≈ 0.5 * V_R * Q_rr (soft recovery)
     *
     * Temperature dependence:
     *   Q_rr(T) ≈ Q_rr(25C) * 2^((T-25)/45) — roughly doubles every 45K.
     *   This is why reverse recovery losses increase dramatically at
     *   high temperature, and why SiC/GaN (zero Q_rr at all temps) are
     *   preferred for high-temperature operation.
     *
     * Voltage overshoot without snubber (parasitic L in loop):
     *   V_overshoot = V_R + L_loop * di/dt
     *   This can reach kilovolts and destroy the diode!
     */
    ReverseRecoveryResult r = {0};

    if (!params) return r;

    double I_F = params->I_F;
    double di_dt = params->di_dt;
    double V_R = params->V_R;
    double t_rr = params->t_rr;
    double Q_rr = params->Q_rr;
    double S = params->S_factor;

    /* If Q_rr and t_rr are given, compute I_rrm */
    if (Q_rr > 0.0 && t_rr > 0.0) {
        r.I_rrm_calculated = 2.0 * Q_rr / t_rr;
    } else if (di_dt > 0.0 && t_rr > 0.0) {
        /* Compute from di/dt: triangular model */
        r.I_rrm_calculated = di_dt * t_rr / (1.0 + S);
        r.t_a = t_rr / (1.0 + S);
        r.t_b = t_rr - r.t_a;
    } else {
        r.I_rrm_calculated = params->I_rrm;
    }

    /* If S-factor is given, compute t_a, t_b */
    if (S > 0.0 && t_rr > 0.0) {
        r.t_a = t_rr / (1.0 + S);
        r.t_b = r.t_a * S;
    }

    /* Energy per switching event */
    r.E_rr_per_switch = 0.5 * V_R * Q_rr;
    if (S > 1.5) {
        /* Snappy: additional ringing energy */
        r.E_rr_per_switch *= 1.3;
    }

    /* Power loss (at some switching frequency — caller multiplies) */
    r.P_rr_total = 0.0;  /* Caller supplies frequency */

    /* Voltage overshoot without snubber */
    /* L_loop * di/dt added to V_R */
    double L_loop_typical = 50e-9;  /* 50 nH typical commutation loop */
    r.V_overshoot_no_snubber = V_R + L_loop_typical * di_dt;

    return r;
}

/* ============================================================================
 * L6: Complete Snubber Design
 * ============================================================================ */

SnubberDesignOutput snubber_design_complete(const SnubberDesignInput *input)
{
    /* Complete snubber design evaluating multiple options.
     *
     * Decision tree:
     * 1. If f_op < 1 kHz: Simple RC snubber is usually sufficient
     * 2. If 1 kHz < f_op < 100 kHz: RCD clamp may be more efficient
     * 3. If f_op > 100 kHz: Consider lossless snubber or wide-bandgap
     * 4. If Q_rr is significant: Add RC snubber to manage recovery spike
     *
     * The snubber trades off:
     *   - Reduced voltage stress (protects the diode)
     *   - Increased power loss (heat in snubber resistor)
     *   - Reduced EMI (controlled dv/dt)
     */
    SnubberDesignOutput out = {0};

    if (!input) return out;

    /* RC snubber for turn-off */
    RCSnubberConfig rc_cfg = {
        .V_bus = input->V_bus,
        .I_load = input->I_load_max,
        .L_parasitic = input->L_loop,
        .C_parasitic = 100e-12,  /* 100 pF typical */
        .f_switching = input->f_op,
        .dv_dt_max = 1e9,        /* 1000 V/us */
        .overshoot_fraction = input->V_overshoot_max / input->V_bus
    };

    out.rc = snubber_rc_design(&rc_cfg);

    /* RCD clamp values */
    out.C_clamp = out.rc.C_s * 10.0;  /* Clamp C is typically larger */
    out.R_clamp = out.rc.R_s * 0.5;   /* Clamp R can be smaller */

    /* Total snubber power */
    out.P_total_snubber = out.rc.P_Rs;

    /* Efficiency impact */
    double P_out = input->V_bus * input->I_load_max;
    if (P_out > 0.0)
        out.efficiency_impact = out.P_total_snubber / P_out;

    /* Recommendation */
    if (input->f_op > 100e3) {
        out.recommended = SNUBBER_LOSS_LESS;
    } else if (input->f_op > 1e3) {
        out.recommended = SNUBBER_RCD_TURNOFF;
    } else {
        out.recommended = SNUBBER_RC_TURNOFF;
    }

    /* TVS needed if overshoot > 2x bus */
    out.requires_tvs = (out.rc.V_overshoot_peak > 2.0 * input->V_bus) ? 1 : 0;

    return out;
}

/* ============================================================================
 * L5: RCD Clamp Design
 * ============================================================================ */

void snubber_rcd_clamp_design(double V_bus, double L_leakage, double I_peak,
                              double f_sw, double V_clamp_target,
                              double *R_clamp, double *C_clamp, double *P_R)
{
    /* RCD clamp for flyback/forward converter transformer.
     *
     * The RCD clamp limits the voltage spike caused by leakage
     * inductance when the primary switch turns off.
     *
     * Energy stored in leakage inductance per cycle:
     *   E_leak = 0.5 * L_leakage * I_peak^2
     *
     * This energy must be dissipated in the clamp resistor each cycle:
     *   P_clamp = E_leak * f_sw
     *
     * Clamp capacitor voltage: V_clamp (set to V_clamp_target)
     * The capacitor must be large enough to keep V_clamp roughly constant:
     *   C_clamp >> P_clamp / (V_clamp^2 * f_sw)
     *   Typically: C_clamp = 10 * P_clamp / (V_clamp_target^2 * f_sw)
     *
     * Clamp resistor: R_clamp = V_clamp^2 / P_clamp
     *
     * RCD clamp is widely used in:
     *   - iPhone charger flyback converter
     *   - Tesla on-board charger DC-DC stage
     *   - Toyota inverter gate drive power supply
     */
    if (!R_clamp || !C_clamp || !P_R) return;
    if (f_sw <= 0.0 || V_clamp_target <= 0.0) return;

    double E_leak = 0.5 * L_leakage * I_peak * I_peak;
    *P_R = E_leak * f_sw;

    *R_clamp = (V_clamp_target * V_clamp_target) / (*P_R);
    if (*R_clamp <= 0.0) *R_clamp = 1e3;

    *C_clamp = 10.0 * (*P_R) / (V_clamp_target * V_clamp_target * f_sw);
    if (*C_clamp < 1e-9) *C_clamp = 1e-9;  /* 1 nF minimum */
}

/* ============================================================================
 * L6: Transformer Ringing Damper
 * ============================================================================ */

void snubber_transformer_damper(double L_leakage, double C_winding,
                                double f_ring, double V_secondary_max,
                                double *R_damp, double *C_damp, double *P_R)
{
    /* RC damper for transformer secondary ringing suppression.
     *
     * Transformer leakage inductance and winding capacitance form
     * an LC tank that rings at:
     *   f_ring = 1 / (2 * pi * sqrt(L_leakage * C_winding))
     *
     * The characteristic impedance of this tank:
     *   Z_char = sqrt(L_leakage / C_winding)
     *
     * For critical damping of the parallel LC:
     *   R_damp = Z_char = sqrt(L_leakage / C_winding)
     *   C_damp = 2-5x C_winding (to dominate the parasitic C)
     *
     * Power dissipated in damper:
     *   P_R ≈ 0.5 * C_winding * V_sec^2 * f_ring  (energy sloshed per cycle)
     *
     * Without this damper, the ringing can:
     *   - Radiate EMI (conducted and radiated)
     *   - Exceed diode voltage ratings
     *   - Cause false triggering in nearby circuits
     *
     * Common in:
     *   - iPhone charger flyback transformer secondary
     *   - Boeing 787 APU rectifier isolation transformer
     *   - Any transformer-coupled power supply > 10W
     */
    if (!R_damp || !C_damp || !P_R) return;
    if (L_leakage <= 0.0 || C_winding <= 0.0) return;

    /* Characteristic impedance */
    *R_damp = sqrt(L_leakage / C_winding);

    /* Damping capacitor: 3x winding capacitance */
    *C_damp = C_winding * 3.0;

    /* Power estimation */
    if (f_ring > 0.0) {
        *P_R = 0.5 * C_winding * V_secondary_max * V_secondary_max * f_ring;
    } else {
        /* Estimate f_ring from L and C */
        double f_est = 1.0 / (2.0 * M_PI * sqrt(L_leakage * C_winding));
        *P_R = 0.5 * C_winding * V_secondary_max * V_secondary_max * f_est;
    }

    /* Cap power to reasonable values */
    if (*P_R > 10.0) *P_R = 10.0;
}

/* ============================================================================
 * L2: TVS Selection
 * ============================================================================ */

void snubber_tvs_select(double V_operating_max, double V_damage,
                        double I_surge, double t_surge,
                        TVSParams *selected_tvs)
{
    /* Select a TVS diode based on protection requirements.
     *
     * Selection criteria:
     * 1. V_RWM >= V_operating_max: TVS must not conduct during normal operation
     *    (otherwise it overheats and fails)
     *
     * 2. V_C <= V_damage: Clamping voltage must protect the downstream circuit
     *    Margin of 20-30% recommended.
     *
     * 3. P_PPM >= surge power: TVS must handle the expected transient
     *    Surge power depends on waveform (10/1000us vs 8/20us, etc.)
     *
     * 4. Package and capacitance: Consider signal integrity impact
     *    for high-speed data lines (USB 3.0, HDMI, Ethernet).
     *
     * IEC 61000-4-5 surge levels:
     *   Level 1: 0.5 kV, 250 A  (protected environment)
     *   Level 2: 1.0 kV, 500 A  (partially protected)
     *   Level 3: 2.0 kV, 1000 A (industrial)
     *   Level 4: 4.0 kV, 2000 A (severe industrial)
     *
     * This is standard practice in:
     *   - Toyota automotive (ISO 7637-2 load dump: 87V, 50ms pulse)
     *   - Boeing avionics (DO-160 lightning indirect effects, Level 3-5)
     *   - Fukushima-grade nuclear instrumentation (IEC 61000-4-5 Level 4+)
     *   - iPhone USB VBUS (IEC 61000-4-2 ESD + surge)
     */
    if (!selected_tvs) return;

    /* Compute required TVS parameters */
    selected_tvs->V_RWM = V_operating_max * 1.15;  /* 15% margin */

    /* V_BR typically 10-20% above V_RWM */
    selected_tvs->V_BR = selected_tvs->V_RWM * 1.1;

    /* V_C must be safely below damage threshold */
    selected_tvs->V_C = V_damage * 0.8;
    if (selected_tvs->V_C < selected_tvs->V_BR)
        selected_tvs->V_C = selected_tvs->V_BR * 1.3;

    /* Peak pulse current handling */
    selected_tvs->I_PP = I_surge;

    /* Peak pulse power (derated for non-standard waveforms) */
    double surge_power = V_damage * I_surge;
    /* 10/1000 us is standard test waveform */
    if (t_surge < 1000e-6) {
        /* For shorter pulses, derate proportionally by sqrt(t) rule */
        surge_power *= sqrt(t_surge / 1000e-6);
    } else {
        /* For longer pulses, full power handling */
        surge_power *= 1.0;
    }
    selected_tvs->P_PPM = surge_power;

    /* Junction capacitance (estimate for typical TVS) */
    if (selected_tvs->V_RWM > 24.0)
        selected_tvs->C_j = 50e-12;   /* Higher voltage → lower C */
    else if (selected_tvs->V_RWM > 5.0)
        selected_tvs->C_j = 200e-12;
    else
        selected_tvs->C_j = 1000e-12; /* Low voltage → high C */
}

/* ============================================================================
 * L8: Wide Bandgap (SiC/GaN) Benefit Quantification
 * ============================================================================ */

double snubber_wide_bandgap_benefit(double V_bus, double I_load, double f_sw,
                                    double Q_rr_si, double V_f_si,
                                    double R_ds_on_gan, double V_f_sic)
{
    /* Quantify the efficiency advantage of SiC/GaN over silicon.
     *
     * Loss components comparison:
     *
     * Silicon diode:
     *   P_cond_Si = V_f_si * I_load
     *   P_rr_Si = 0.5 * V_bus * Q_rr_si * f_sw  (reverse recovery loss)
     *   P_snub_Si = C_snub * V_bus^2 * f_sw  (snubber loss, needed for Si)
     *   P_total_Si = P_cond_Si + P_rr_Si + P_snub_Si
     *
     * SiC Schottky diode:
     *   P_cond_SiC = V_f_sic * I_load  (slightly higher V_f)
     *   P_rr_SiC ≈ 0  (majority carrier device, negligible Q_rr)
     *   P_snub_SiC ≈ 0  (no snubber needed, no ringing)
     *   P_total_SiC = P_cond_SiC
     *
     * GaN HEMT (synchronous rectification):
     *   P_cond_GaN = I_load^2 * R_ds_on_gan  (resistive, no knee voltage)
     *   P_rr_GaN ≈ 0  (no body diode conduction in sync rect)
     *   P_total_GaN = P_cond_GaN + P_gate_drive
     *
     * Crossover analysis:
     *   At low I: Si diode wins (lower V_f than SiC, no gate drive)
     *   At medium I: SiC wins (no Q_rr, better at high f)
     *   At high I: GaN wins (I^2*R can be lower than I*V_f)
     *
     * Real-world example (240V, 10A, 200kHz):
     *   Si: P_cond=7W + P_rr=2.4W + P_snub=1W ≈ 10.4W
     *   SiC: P_cond=15W + P_rr=0 ≈ 15W  (worse at low freq!)
     *   GaN (10mOhm): P_cond=1W ≈ 1W  (best overall)
     *
     * But at 60Hz: Si=7W, SiC=15W, GaN=1W → GaN always wins
     * At 60Hz with 50A: Si=35W, SiC=53W, GaN=25W
     *
     * Note: The above comparisons are idealized. Real implementations
     * include additional losses (gate drive, snubber, magnetic).
     *
     * Ref: Baliga "Advanced Power MOSFET Concepts" (2010)
     */
    double P_Si_cond = V_f_si * I_load;
    double P_Si_rr = 0.5 * V_bus * Q_rr_si * f_sw;
    double P_Si_snub = 0.002 * V_bus * V_bus * f_sw;  /* ~2nF snubber */
    double P_Si_total = P_Si_cond + P_Si_rr + P_Si_snub;

    double P_SiC_cond = V_f_sic * I_load;
    double P_SiC_total = P_SiC_cond;  /* No Q_rr, no snubber */

    double P_GaN_cond = I_load * I_load * R_ds_on_gan;
    double P_GaN_total = P_GaN_cond + 0.5;  /* +0.5W gate drive */

    /* Return the best efficiency, using Si as baseline */
    double P_out = V_bus * I_load;
    double eff_Si = P_out / (P_out + P_Si_total);
    double eff_SiC = P_out / (P_out + P_SiC_total);
    double eff_GaN = P_out / (P_out + P_GaN_total);

    /* Best of three */
    double best = eff_Si;
    if (eff_SiC > best) best = eff_SiC;
    if (eff_GaN > best) best = eff_GaN;

    return best;
}

/* ============================================================================
 * L6: Parasitic Inductance Estimation
 * ============================================================================ */

double snubber_estimate_loop_inductance(double trace_length_cm,
                                        double trace_width_mm,
                                        double loop_height_mm,
                                        int n_devices)
{
    /* Estimate parasitic inductance of a commutation loop.
     *
     * PCB trace inductance (microstrip approximation):
     *   L ≈ 2 * len * ln(2*h/w)  [nH]
     *   where len is in cm, h is height above ground plane in mm,
     *   w is trace width in mm.
     *
     * More accurate formula for a rectangular loop:
     *   L_loop ≈ mu_0 * len / (2*pi) * ln(2*h/w + w/(2*h))
     *   ≈ 2 * len * ln(1 + 2*pi*h/w)  [nH]
     *
     * Package inductance per device:
     *   TO-220: ~8 nH
     *   TO-247: ~12 nH
     *   DPAK: ~5 nH
     *   SMD (DFN 5x6): ~2 nH
     *
     * Total loop inductance is critical for:
     *   - Snubber design (determines required C_s)
     *   - EMI compliance (ringing frequency and amplitude)
     *   - Switching loss (V_overshoot from L*di/dt)
     *
     * Rule of thumb from Pressman:
     *   "Every 10 nH of loop inductance causes ~10V overshoot
     *    per 1 A/ns of di/dt."
     *
     * Example: 50 A/ns switching, 20 nH loop → 100 V overshoot!
     *
     * This is why Tesla and other EV inverter designs use laminated
     * bus bars and carefully optimized PCB layouts to minimize L_loop.
     */
    if (trace_length_cm <= 0.0) return 10e-9 * (double)n_devices;

    /* Trace inductance per cm (nH/cm) */
    double h = loop_height_mm;
    double w = trace_width_mm;
    if (h <= 0.0) h = 1.6;  /* Standard 1.6mm PCB */
    if (w <= 0.0) w = 1.0;

    double L_per_cm_nH = 2.0 * log(2.0 * h / w);
    if (L_per_cm_nH < 0.0) L_per_cm_nH = 0.5;

    double L_trace_nH = trace_length_cm * L_per_cm_nH;

    /* Device package inductance (~8 nH per TO-220 equivalent) */
    double L_package_nH = 8.0 * (double)n_devices;

    /* Total in henries */
    return (L_trace_nH + L_package_nH) * 1e-9;
}