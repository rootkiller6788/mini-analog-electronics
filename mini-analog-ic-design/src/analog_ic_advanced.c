/*
 * analog_ic_advanced.c - Advanced Analog IC Design Topics
 * L7: Applications (charge pump, LDO, ADC driver)
 * L8: Subthreshold, Chopper, Auto-zero, Translinear, Gate driver
 */
#include "analog_ic_basis.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* L7: Charge Pump for PLL
 * delta_V = I_cp * t_pulse / C_loop
 * Reference: Gardner (1980); Razavi "RF Microelectronics" Ch.8 */
double charge_pump_voltage_step(double I_cp, double t_pulse, double C_loop)
{
    if (C_loop <= 0.0) return 0.0;
    return I_cp * t_pulse / C_loop;
}

/* L7: Charge pump current mismatch -> static phase offset */
double charge_pump_phase_error(double I_up, double I_down, double t_reset, double T_ref)
{
    double I_avg = (I_up + I_down) / 2.0;
    double delta_I = fabs(I_up - I_down);
    if (I_avg <= 0.0 || T_ref <= 0.0) return 0.0;
    return 2.0 * M_PI * (delta_I / I_avg) * (t_reset / T_ref);
}

/* L7: LDO Dropout Voltage for PMOS pass device
 * V_dropout = I_load * R_dson
 * Reference: Rincon-Mora "Analog IC Design with LDOs" (2009) */
double ldo_dropout_voltage(double I_load, const ic_technology_t *tech,
                            double W_pass, double L_pass, double Vgs)
{
    if (!tech || W_pass <= 0.0 || L_pass <= 0.0) return 0.0;
    double Vth_p = tech->Vth_p;
    double mu_p = tech->mu_p * 1e-4;
    double Cox = tech->Cox;
    double Vov = Vgs - Vth_p;
    if (Vov <= 0.0) return 999.0;
    double R_dson = 1.0 / (mu_p * Cox * (W_pass / L_pass) * Vov);
    return I_load * R_dson;
}

/* L7: LDO efficiency */
double ldo_efficiency(double Vout, double Vin, double I_load, double I_q)
{
    if (Vin <= 0.0 || (I_load + I_q) <= 0.0) return 0.0;
    return (Vout / Vin) * (I_load / (I_load + I_q));
}

double ldo_power_dissipation(double Vin, double Vout, double I_load, double I_q)
{
    return (Vin - Vout) * I_load + Vin * I_q;
}

/* L7: ADC Driver Settling Time
 * Reference: Kester "Data Conversion Handbook" (ADI) Ch.6 */
double adc_settling_time_required(int N_bits, double tau, double error_fraction)
{
    if (tau <= 0.0) return 0.0;
    double target_error = error_fraction / (double)(1 << (N_bits + 1));
    return -tau * log(target_error);
}

double adc_driver_gbw_required(int N_bits, double t_acquisition, double feedback_factor)
{
    if (t_acquisition <= 0.0 || feedback_factor <= 0.0) return 0.0;
    double N_tau = (double)(N_bits + 1) * log(2.0);
    return N_tau / (2.0 * M_PI * t_acquisition * feedback_factor);
}

/* L8: Subthreshold Swing [mV/dec] */
double subthreshold_swing(double T)
{
    double Vt = THERMAL_VT(T); double n = 1.4;
    return n * Vt * log(10.0) * 1e3;
}

/* L8: Subthreshold gm/Id max = 1/(n*Vt) — fundamental limit */
double subthreshold_gm_id_max(double T)
{
    if (T <= 0.0) return 0.0;
    return 1.0 / (1.4 * THERMAL_VT(T));
}

double subthreshold_noise_psd(double Id)
{
    if (Id <= 0.0) return 0.0;
    return 2.0 * Q_ELECTRON * Id;
}

/* L8: Chopper Stabilization
 * Reference: Enz & Temes, Proc. IEEE 1996 */
double chopper_minimum_frequency(double f_corner, double signal_bw, double margin)
{
    return (f_corner + signal_bw) * (1.0 + margin);
}

double chopper_residual_offset(double delta_Q_inj, double C_coupling)
{
    if (C_coupling <= 0.0) return 0.0;
    return delta_Q_inj / C_coupling;
}

/* L8: Auto-Zeroing
 * Reference: Enz & Temes (1996) */
double autozero_noise_psd(double C_az, double f_chop, double T)
{
    if (C_az <= 0.0 || f_chop <= 0.0) return 0.0;
    double ktc = K_BOLTZMANN * T / C_az;
    double BW_noise = 1e6;
    return ktc * 2.0 * BW_noise / f_chop;
}

double autozero_offset_reduction(double Vos_original, double A_aux)
{
    if (A_aux <= 0.0) return Vos_original;
    return Vos_original / A_aux;
}

/* L7: Comparator Analysis */
double comparator_input_offset(double delta_Vth, double delta_beta, double beta, double Vov)
{
    double term2 = (beta > 1e-12) ? (Vov / 2.0) * (delta_beta / beta) : 0.0;
    return delta_Vth + term2;
}

double comparator_delay(double C_load, double V_swing, double I_tail)
{
    if (I_tail <= 0.0) return 1e-6;
    return C_load * V_swing / I_tail;
}

double comparator_metastability_error(double t_resolve, double gm, double C_load)
{
    if (gm <= 0.0 || C_load <= 0.0) return 1.0;
    return exp(-t_resolve * gm / C_load);
}

/* L8: Translinear Circuit Principle (Gilbert, 1968) */
double translinear_loop_product(double I1, double I2, double I3)
{
    if (I2 <= 0.0) return 0.0;
    return I1 * I3 / I2;
}

/* L7: Hall-effect current sensor (Tesla motor drives) */
double hall_sensor_output(double G, double R_sense, double I_motor)
{
    return G * R_sense * I_motor;
}

/* L7: Gate driver peak current */
double gate_driver_peak_current(double C_iss, double V_drive, double t_sw)
{
    if (t_sw <= 0.0) return 0.0;
    return C_iss * V_drive / t_sw;
}

/* L7: ISO 26262 ASIL safety — TMR failure probability */
double tmr_failure_probability(double P_single)
{
    return 3.0 * P_single * P_single;
}

/* L7: Switched-capacitor common-mode feedback (SC-CMFB)
 * Used in fully-differential op-amps to set output common-mode.
 * DC level: Vcm_out = Vcm_ref (ideal)
 * Reference: Razavi Ch.10.6; Castello & Gray (1985) */
double sc_cmfb_settling_time(double C_cmfb, double C_load, double f_sw)
{
    if (f_sw <= 0.0 || C_cmfb + C_load <= 0.0) return 0.0;
    double tau = (C_cmfb + C_load) / (C_cmfb * f_sw);
    return 5.0 * tau; /* 5*tau settling to 0.7% */
}

/* L7: Pipeline ADC stage residue amplifier
 * V_residue = 2*Vin - D*Vref, where D in {0,1}
 * The multiplying DAC (MDAC) is the core of pipeline ADCs.
 * Reference: Lewis et al. (1992); Kester Ch.5 */
double mdac_residue(double Vin, int D, double Vref, double interstage_gain)
{
    double ideal = interstage_gain * Vin - (double)D * Vref;
    return ideal;
}

/* L7: Sigma-Delta modulator integrator leakage
 * H(z) = z^-1/(1 - alpha*z^-1), where alpha = 1 - 1/Av_dc
 * Finite DC gain causes integrator leakage.
 * Reference: Schreier & Temes "Understanding Delta-Sigma Data Converters" */
double sdm_integrator_leakage(double Av_dc)
{
    if (Av_dc <= 0.0) return 1.0;
    return 1.0 / (1.0 + Av_dc);
}

/* L8: Time-Interleaved ADC mismatch
 * Gain mismatch between channels causes spurs at fs/M +/- fin
 * Reference: Black & Hodges (1980); Razavi "RF Microelectronics" */
double ti_adc_gain_error_spur(double delta_gain, double M)
{
    (void)M;
    return delta_gain * 20.0 / log(10.0); /* Approx spur in dBc */
}

/* L8: Dynamic element matching (DEM) for DAC linearity
 * Rotates mismatch errors to high frequencies.
 * Reference: Van de Plassche "CMOS Integrated ADC and DAC" (2003) */
double dem_mismatch_shaping(double sigma_mismatch, double OSR)
{
    if (OSR <= 0.0) return sigma_mismatch;
    return sigma_mismatch / sqrt(OSR);
}

/* L8: Subthreshold region design — Inversion Coefficient
 * IC = Id / (2*n*mu*Cox*(W/L)*Vt^2)
 * IC < 0.1: weak inversion (WI)
 * 0.1 < IC < 10: moderate inversion (MI)
 * IC > 10: strong inversion (SI)
 * Reference: Sansen §1.4; Binkley "Tradeoffs and Optimization" (2008) */
double inversion_coefficient(double Id, const ic_technology_t *tech, mos_type_t type,
                              double W, double L, double T)
{
    if (!tech || W <= 0.0 || L <= 0.0 || T <= 0.0) return 0.0;
    double mu = (type == MOS_TYPE_NMOS) ? tech->mu_n : tech->mu_p;
    mu *= 1e-4;
    double Cox = tech->Cox;
    double Vt = THERMAL_VT(T);
    double n = 1.4;
    double Ispec = 2.0 * n * mu * Cox * (W / L) * Vt * Vt;
    if (Ispec <= 0.0) return 0.0;
    return Id / Ispec;
}

/* L8: Moderate inversion design sweet spot
 * gm/Id ~ 15 1/V, ft ~ 10*GBW provides good tradeoff.
 * This is the "sweet spot" for many analog designs.
 * Reference: Sansen §1.4.3; Murmann (gm/ID) */
double moderate_inversion_gm_id_opt(double T)
{
    (void)T;
    return 15.0; /* 1/V — sweet spot */
}
