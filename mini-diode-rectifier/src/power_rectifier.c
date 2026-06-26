/**
 * power_rectifier.c -- High-Power and Three-Phase Rectifier Implementation
 *
 * Implements three-phase rectifier analysis, multi-pulse systems,
 * thermal management, power loss breakdown, and inrush analysis.
 *
 * Reference: Mohan, Undeland & Robbins Ch.5-6; Rashid Ch.3-4
 */

#include "power_rectifier.h"
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * L6: Three-Phase Rectifier Analysis
 * ============================================================================ */

ThreePhaseResult power_three_phase_analyze(const ThreePhaseConfig *cfg)
{
    /* Three-phase rectifier analysis with commutation overlap.
     *
     * Core Theory:
     *
     * For a 6-pulse bridge with sinusoidal input V_ll = V_m_ll * sin(wt):
     *   The output is the envelope of the line-to-line voltages.
     *   In one period (2*pi), there are 6 pulses, one every 60 degrees.
     *
     * Ideal DC output (no commutation, no diode drop):
     *   V_dc = (1 / (pi/3)) * integral_{pi/3}^{2*pi/3} V_m_ll * sin(wt) d(wt)
     *        = (3/pi) * V_m_ll * [-cos(wt)]_{pi/3}^{2*pi/3}
     *        = (3/pi) * V_m_ll * [cos(pi/3) - cos(2*pi/3)]
     *        = (3/pi) * V_m_ll * [0.5 - (-0.5)]
     *        = (3/pi) * V_m_ll = 0.955 * V_m_ll
     *
     * Since V_m_ll = sqrt(2) * V_ll_rms:
     *   V_dc = 3*sqrt(2)/pi * V_ll_rms ≈ 1.3505 * V_ll_rms
     *
     * Alternatively expressed as: V_dc = 2.34 * V_ln_rms (wye)
     *   where V_ln_rms = V_ll_rms / sqrt(3)
     *
     * Commutation overlap:
     *   When source inductance L_s is present, current cannot change
     *   instantaneously between phases. During commutation, two diodes
     *   conduct simultaneously, causing a voltage reduction.
     *
     *   cos(mu) = 1 - (2 * w * L_s * I_dc) / (sqrt(3) * V_m_ll)
     *   V_drop = (3 * w * L_s * I_dc) / pi   (linear approximation)
     *
     * Ripple:
     *   The 6-pulse output has ripple at 6*f_line (300 Hz at 50 Hz).
     *   V_r_pp = V_dc * (1 - cos(pi/6)) = V_dc * (1 - 0.866) ≈ 0.134*V_dc
     *   But actually the ripple is from peak to valley:
     *   V_r_pp = V_m_ll * (1 - cos(pi/6)) ≈ 0.068 * V_dc (only 4.2%!)
     *
     * This inherently low ripple (4.2% vs 48% for single-phase) is why
     * three-phase is preferred for high-power applications.
     *
     * Ref: Mohan Sec.5.3, Rashid Sec.3.6
     */
    ThreePhaseResult r = {0};

    if (!cfg) return r;

    double V_m_ll = cfg->V_ll_rms * 1.4142135623730951;
    double V_f = cfg->V_f;
    double f = cfg->freq_Hz;
    double R = cfg->R_load;
    double L_s = cfg->L_source;

    if (R <= 0.0) return r;

    if (cfg->use_half_wave) {
        /* 3-pulse half-wave */
        r.pulse_number = 3;
        double V_ln_rms = cfg->V_ll_rms;
        if (cfg->connection == THREEPHASE_DELTA)
            V_ln_rms = cfg->V_ll_rms / 1.7320508075688772;

        double V_m_ln = V_ln_rms * 1.4142135623730951;
        r.V_dc = (3.0 * 1.7320508075688772 / (2.0 * M_PI)) * V_m_ln - V_f;
        r.V_r_pp = r.V_dc * 0.25;  /* ~25% ripple for 3-pulse */
        r.ripple_freq_Hz = 3.0 * f;
    } else {
        /* 6-pulse full bridge */
        r.pulse_number = 6;
        r.V_dc = (3.0 * 1.4142135623730951 / M_PI) * cfg->V_ll_rms - 2.0 * V_f;
        r.V_r_pp = r.V_dc * 0.068;  /* ~6.8% for 6-pulse (with diode drops) */
        r.ripple_freq_Hz = 6.0 * f;
    }

    /* Commutation overlap */
    if (L_s > 0.0 && r.V_dc > 0.0) {
        double w = 2.0 * M_PI * f;
        r.I_dc = r.V_dc / R;
        double cos_mu_arg = 1.0 - (2.0 * w * L_s * r.I_dc) / (1.7320508 * V_m_ll);
        if (cos_mu_arg < -1.0) cos_mu_arg = -1.0;
        if (cos_mu_arg > 1.0) cos_mu_arg = 1.0;
        r.commutation_angle_rad = acos(cos_mu_arg);
        r.V_drop_commutation = (3.0 * w * L_s * r.I_dc) / M_PI;
        r.V_dc -= r.V_drop_commutation;
    }

    if (r.V_dc < 0.0) r.V_dc = 0.0;

    /* Derived values */
    r.I_dc = r.V_dc / R;
    r.V_rms_output = sqrt(r.V_dc * r.V_dc + (r.V_r_pp / (2.0 * 1.73205)) *
                          (r.V_r_pp / (2.0 * 1.73205)));

    r.I_diode_avg = r.I_dc / (double)r.pulse_number;
    r.I_diode_rms = r.I_dc / sqrt((double)r.pulse_number);
    r.I_diode_peak = r.I_dc * 1.2;  /* Slightly above average in 6-pulse */

    r.PIV = V_m_ll;  /* PIV per diode = peak line-to-line voltage */
    r.P_dc = r.V_dc * r.I_dc;
    r.ripple_factor = (r.V_r_pp / (2.0 * 1.73205)) / r.V_dc;

    if (r.P_dc > 0.0) {
        double P_loss = 2.0 * V_f * r.I_dc + r.V_drop_commutation * r.I_dc;
        r.efficiency = r.P_dc / (r.P_dc + P_loss);
    }

    return r;
}

/* ============================================================================
 * L8: Multi-Pulse Rectifier Analysis
 * ============================================================================ */

MultiPulseResult power_multipulse_analyze(const MultiPulseConfig *cfg)
{
    /* Multi-pulse rectifier analysis (12, 18, 24-pulse).
     *
     * Higher pulse numbers are achieved by using phase-shifting
     * transformers with multiple secondary windings.
     *
     * 12-pulse: Two 6-pulse bridges, one fed from delta secondary,
     *           the other from wye secondary — 30 degree phase shift.
     *
     * The output voltage ripple frequency = pulse_count * f_line.
     * Ripple magnitude decreases as 1/pulse_count.
     *
     * Input current harmonics:
     *   Characteristic harmonics = k * pulse_count +/- 1 (k = 1,2,3,...)
     *   6-pulse:  5th, 7th, 11th, 13th, 17th, 19th
     *   12-pulse: 11th, 13th, 23rd, 25th
     *   18-pulse: 17th, 19th, 35th, 37th
     *   24-pulse: 23rd, 25th, 47th, 49th
     *
     * This is why Boeing 787 uses multi-pulse systems: IEEE 519
     * harmonic limits at the point of common coupling (PCC) are
     * easier to meet with higher pulse numbers.
     *
     * Ref: IEEE Std 519-2014, Mohan Ch.5.8
     */
    MultiPulseResult r = {0};

    if (!cfg || cfg->pulse_count < 6) return r;

    int p = cfg->pulse_count;
    double V_m_ll = cfg->V_ll_rms * 1.4142135623730951;

    /* DC output: approaches ideal as pulse count increases */
    /* V_dc = (p / pi) * sin(pi/p) * V_m — derived from the envelope integral */
    double sin_pi_over_p = sin(M_PI / (double)p);
    r.V_dc = ((double)p / M_PI) * sin_pi_over_p * V_m_ll - 2.0 * cfg->V_f;

    /* Ripple: proportional to 1 - cos(pi/p) */
    r.V_r_pp = V_m_ll * (1.0 - cos(M_PI / (double)p));
    r.ripple_factor = r.V_r_pp / (r.V_dc * 2.0 * 1.73205);
    r.ripple_freq_Hz = (double)p * cfg->freq_Hz;

    /* THD estimation */
    double I_fund = cfg->V_ll_rms / cfg->R_load;  /* Fundamental current */
    r.fundamental_I_rms = I_fund;
    r.dominant_harmonic = p - 1;  /* e.g., 11th for 12-pulse */

    /* THD ~= 1/pulse_count * some factor (simplified) */
    r.THD_input_current = 30.0 / ((double)p / 6.0);  /* 6-pulse ≈ 30%, scales down */

    /* Efficiency */
    double I_dc = r.V_dc / cfg->R_load;
    double P_loss = (double)(p / 3) * cfg->V_f * I_dc;  /* Diodes conducting at once */
    if (I_dc > 0.0 && r.V_dc > 0.0)
        r.efficiency = (r.V_dc * I_dc) / (r.V_dc * I_dc + P_loss);

    return r;
}

/* ============================================================================
 * L1: Thermal Analysis
 * ============================================================================ */

ThermalResult power_thermal_analyze(const ThermalConfig *cfg)
{
    /* Junction temperature estimation using the thermal resistance model.
     *
     * Ohm's law analogy for heat transfer:
     *   T_junction - T_ambient = P_dissipated * (Rth_jc + Rth_cs + Rth_sa)
     *   Analogous to: V = I * R
     *
     * Temperature -> Voltage
     * Power -> Current
     * Thermal resistance -> Electrical resistance
     *
     * Thermal resistances:
     *   Rth_jc: junction-to-case (package-dependent, e.g., 1.5 K/W for TO-220)
     *   Rth_cs: case-to-sink (thermal compound, typically 0.5 K/W)
     *   Rth_sa: sink-to-ambient (depends on heatsink size and airflow)
     *
     * Arrhenius reliability model:
     *   MTBF(T) / MTBF(T_ref) = exp( (E_a/k) * (1/T_ref - 1/T) )
     *   where E_a ≈ 0.7 eV for silicon semiconductor failure mechanisms.
     *   Every 10 K increase halves the expected lifetime (rough rule of thumb).
     *
     * This is critical for: Toyota under-hood electronics (Ta up to 125C),
     * Boeing avionics (high altitude, reduced convection), NASA space
     * applications (radiation + vacuum = only conduction/radiation cooling).
     *
     * Ref: MIL-HDBK-217F reliability prediction.
     */
    ThermalResult r = {0};

    if (!cfg) return r;

    double Rth_total = cfg->Rth_jc + cfg->Rth_cs + cfg->Rth_sa;

    r.Tj = cfg->Ta + cfg->P_dissipated * Rth_total;
    r.Tc = r.Tj - cfg->P_dissipated * cfg->Rth_jc;
    r.Th = cfg->Ta + cfg->P_dissipated * cfg->Rth_sa;

    r.margin_K = cfg->Tj_max - r.Tj;
    r.needs_heatsink = (r.margin_K < 20.0) ? 1 : 0;

    /* Required heatsink Rth to maintain margin */
    if (cfg->P_dissipated > 0.0) {
        r.Rth_sa_required = (cfg->Tj_max - cfg->Ta) / cfg->P_dissipated
                            - cfg->Rth_jc - cfg->Rth_cs;
        if (r.Rth_sa_required < 0.0) r.Rth_sa_required = 0.0;
    }

    /* Arrhenius MTBF factor (E_a = 0.7 eV, T_ref = 300K) */
    double E_a_eV = 0.7;
    double k_eV_K = 8.617333262145e-5;  /* Boltzmann in eV/K */
    double T_ref = 300.0;
    r.MTBF_factor = exp((E_a_eV / k_eV_K) * (1.0 / T_ref - 1.0 / r.Tj));

    return r;
}

/* ============================================================================
 * L7: Power Loss Breakdown
 * ============================================================================ */

PowerLossBreakdown power_loss_breakdown(double V_f, double I_avg_total,
                                        int n_diodes_conducting,
                                        double f_line, double V_reverse,
                                        double I_rr, double t_rr)
{
    /* Detailed power loss analysis for rectifier.
     *
     * Three loss mechanisms:
     *
     * 1. Conduction loss (dominant at low frequency):
     *    P_cond = n_conducting * V_f * I_avg_per_diode
     *    For silicon: V_f ≈ 0.7-1.2V depending on current.
     *    For SiC Schottky: V_f ≈ 0.9-1.5V but zero Q_rr.
     *
     * 2. Reverse recovery loss (dominant at high frequency):
     *    E_rr ≈ 0.5 * V_r * Q_rr  (soft recovery)
     *    E_rr ≈ V_r * Q_rr        (snappy recovery, worst case)
     *    P_rr = E_rr * f_line
     *    This is why SiC/GaN with Q_rr ≈ 0 enables MHz-range operation.
     *
     * 3. Leakage loss (typically negligible):
     *    P_leak = V_reverse * I_leakage
     *    I_leakage ≈ nA-uA for Si, nA for SiC at rated voltage.
     *
     * Total efficiency:
     *    eta = P_out / (P_out + P_total_loss)
     *
     * Example: 100W DC output, Si bridge at 60Hz:
     *   P_cond ≈ 2 * 0.9V * 8.3A ≈ 15W → eta ≈ 87%
     *
     * Same with SiC diodes: V_f=1.5V → P_cond ≈ 25W but zero P_rr.
     * At 60Hz the advantage is small; at 100kHz, SiC wins dramatically.
     */
    PowerLossBreakdown r = {0};

    if (n_diodes_conducting <= 0) n_diodes_conducting = 1;

    double I_avg_per_diode = I_avg_total / (double)n_diodes_conducting;

    /* Conduction loss */
    r.P_conduction = (double)n_diodes_conducting * V_f * I_avg_per_diode;

    /* Reverse recovery loss */
    double Q_rr = 0.5 * I_rr * t_rr;  /* Triangular charge approximation */
    double E_rr = 0.5 * V_reverse * Q_rr;
    r.P_reverse_recovery = E_rr * f_line * (double)n_diodes_conducting;

    /* Leakage (assume 1 uA typical) */
    r.P_leakage = V_reverse * 1e-6 * (double)n_diodes_conducting;

    r.P_total = r.P_conduction + r.P_reverse_recovery + r.P_leakage;

    /* Efficiency: need output power input */
    double P_out = V_f * I_avg_total;  /* Placeholder: actual depends on circuit */
    if (P_out + r.P_total > 0.0)
        r.efficiency = P_out / (P_out + r.P_total);

    return r;
}

/* ============================================================================
 * L7: Inrush Current Analysis
 * ============================================================================ */

InrushAnalysis power_inrush_analyze(double V_m, double C_filter,
                                    double R_source, double L_source,
                                    double NTC_R25)
{
    /* Capacitor charging inrush analysis.
     *
     * When AC power is first applied, the filter capacitor appears as
     * a short circuit. The only current limiting is from source impedance.
     *
     * Peak inrush (at t=0+, capacitor voltage = 0):
     *   I_peak = V_m / R_source
     *
     * For a 120Vrms supply with 0.5 ohm source impedance:
     *   I_peak = 170 / 0.5 = 340 A! (enough to weld switch contacts)
     *
     * I^2 * t (energy let-through, used for fuse selection):
     *   I2t ≈ (V_m^2 * C) / (2 * R_source) for RC charging
     *   Units: A^2·s
     *
     * NTC thermistor solution:
     *   NTC has high resistance when cold (e.g., 10 ohm at 25C)
     *   and low resistance when hot (e.g., 0.5 ohm at operating temp).
     *   This limits inrush: I_peak = 170 / (0.5 + 10) ≈ 16 A.
     *
     * Tesla Supercharger approach:
     *   Uses a pre-charge relay with series resistor to charge the
     *   DC link capacitor, then bypasses with main contactor.
     *   Eliminates NTC power loss during normal operation.
     *
     * iPhone charger approach:
     *   Small NTC in series with bridge input, negligible at 5W.
     *
     * Ref: Pressman "Switching Power Supply Design" Ch.3.
     */
    InrushAnalysis r = {0};

    if (R_source <= 0.0 || C_filter <= 0.0) return r;

    r.I_peak_inrush = V_m / R_source;

    /* I2t for RC charging */
    r.I2t = (V_m * V_m * C_filter) / (2.0 * R_source);

    /* Charging time constants */
    double tau = R_source * C_filter;
    r.t_charge_to_63pct = tau;
    r.t_charge_to_90pct = -tau * log(0.1);  /* t = -RC * ln(1 - 0.9) = -RC * ln(0.1) */

    /* NTC recommendation */
    if (NTC_R25 > 0.0) {
        double I_peak_with_NTC = V_m / (R_source + NTC_R25);
        r.NTC_resistance_cold = NTC_R25;
        /* Recalc with NTC */
        r.I_peak_inrush = I_peak_with_NTC;
    } else {
        r.NTC_resistance_cold = 0.0;
    }

    /* Pre-charge recommendation if I_peak > 100A */
    r.needs_precharge = (r.I_peak_inrush > 100.0) ? 1 : 0;

    return r;
}

/* ============================================================================
 * L7: Efficiency Comparison (1-phase vs 3-phase)
 * ============================================================================ */

void power_efficiency_compare(double V_dc_target, double I_load,
                              double *eff_1ph_bridge, double *eff_3ph_bridge)
{
    /* Quick comparison of single-phase bridge vs three-phase bridge efficiency.
     *
     * Three-phase advantages:
     *   - Lower ripple → smaller filter → less loss
     *   - Higher TUF → smaller transformer
     *   - Balanced loading → no neutral current
     *   - Lower peak current stress on diodes
     *
     * Single-phase advantages:
     *   - Simpler, cheaper for low power
     *   - Only 4 diodes vs 6
     *
     * Typical crossover point: ~1-3 kW.
     * Below 1 kW: single-phase sufficient.
     * Above 5 kW: three-phase strongly preferred.
     */
    double V_f = 0.9;  /* Typical silicon diode drop at moderate current */

    /* Single-phase bridge loss */
    double P_loss_1ph = 2.0 * V_f * I_load;
    if (eff_1ph_bridge) {
        double P_out = V_dc_target * I_load;
        *eff_1ph_bridge = P_out / (P_out + P_loss_1ph);
    }

    /* Three-phase bridge loss (slightly lower current per diode) */
    double P_loss_3ph = 2.0 * V_f * I_load * 0.85;  /* Lower peak → lower V_f */
    if (eff_3ph_bridge) {
        double P_out = V_dc_target * I_load;
        *eff_3ph_bridge = P_out / (P_out + P_loss_3ph);
    }
}

/* ============================================================================
 * L8: Synchronous Rectifier Efficiency
 * ============================================================================ */

double power_synchronous_rectifier_efficiency(double I_load, double R_ds_on,
                                              double V_f_diode, double V_out)
{
    /* Compare MOSFET synchronous rectification vs diode.
     *
     * Diode loss: P_diode = V_f * I_load
     * MOSFET loss: P_mosfet = I_load^2 * R_ds_on
     *
     * Crossover: V_f * I = I^2 * R_ds_on → I = V_f / R_ds_on
     *
     * Example: V_f = 0.7V, R_ds_on = 10 mOhm:
     *   Crossover at I = 0.7 / 0.01 = 70 A
     *   Below 70A: diode has lower loss!
     *   Above 70A: MOSFET wins.
     *
     * Modern MOSFETs with R_ds_on < 1 mOhm (e.g., Infineon OptiMOS):
     *   Crossover at I = 0.7 / 0.001 = 700 A
     *   MOSFET is better at all practical currents.
     *
     * However, synchronous rectification needs gate drive circuitry,
     * adding complexity. The trade-off is efficiency vs. simplicity.
     *
     * Example applications:
     *   - iPhone 20W USB-C charger: uses synchronous rectification (GaN FETs)
     *   - Tesla Model 3 onboard charger: SiC MOSFET synchronous rectification
     *   - Data center 48V bus converters: synchronous rectification mandatory
     */
    if (I_load <= 0.0 || V_out <= 0.0) return 0.0;

    double P_diode = V_f_diode * I_load;
    double P_fet = I_load * I_load * R_ds_on;

    double P_out = V_out * I_load;
    double eff_diode = P_out / (P_out + P_diode);
    double eff_fet = P_out / (P_out + P_fet);

    /* Return the better efficiency */
    return (eff_fet > eff_diode) ? eff_fet : eff_diode;
}

/* ============================================================================
 * L7: Line Current Harmonic Analysis
 * ============================================================================ */

void power_line_harmonics(double I_fundamental, double conduction_angle_rad,
                          int n_harmonics, double *harmonics)
{
    /* Compute harmonic amplitudes of rectifier input current.
     *
     * The input current of a single-phase bridge with capacitor filter
     * is a pulse train at twice line frequency.
     *
     * Fourier series of a pulse train with conduction angle 2*theta:
     *   I_n = (4*I_peak/(n*pi*theta)) * sin(n*theta) * cos(n*pi/2)
     *   For odd harmonics only (even harmonics cancel).
     *
     * Harmonic amplitudes (relative to fundamental) for typical
     * capacitor-input rectifier (theta ~ 30 degrees):
     *   3rd: 80% (can exceed fundamental in some cases!)
     *   5th: 60%
     *   7th: 40%
     *   9th: 25%
     *
     * IEC 61000-3-2 Class A limits (for equipment <= 16A/phase):
     *   3rd harmonic: 2.30 A max
     *   5th harmonic: 1.14 A max
     *   7th harmonic: 0.77 A max
     *
     * This is why PFC (Power Factor Correction) boost converters
     * are mandatory above 75W in many jurisdictions (EN 61000-3-2).
     * The Tesla on-board charger, iPhone charger, and most modern
     * supplies use active PFC to meet these limits.
     *
     * Ref: IEC 61000-3-2:2018, Mohan Ch.5.6
     */
    if (!harmonics || n_harmonics <= 0) return;

    double I_peak = I_fundamental * M_PI / (2.0 * conduction_angle_rad);

    for (int n = 1; n <= n_harmonics; n++) {
        if (n % 2 == 0) {
            harmonics[n - 1] = 0.0;  /* Even harmonics cancel */
        } else {
            double theta = conduction_angle_rad;
            harmonics[n - 1] = (4.0 * I_peak / (n * M_PI)) *
                               sin((double)n * theta / 2.0);
        }
    }
}