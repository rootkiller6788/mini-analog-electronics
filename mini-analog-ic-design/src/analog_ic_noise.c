/*
 * analog_ic_noise.c - Noise Analysis in Analog IC Design
 * Each function implements one independent noise model or analysis method.
 * Textbook: Razavi Ch.7; Gray & Meyer Ch.11; Lee (RF IC) Ch.11
 */
#include "analog_ic_noise.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* L2: Resistor Thermal Noise (Johnson-Nyquist, 1928)
 * Svv = 4*k*T*R [V^2/Hz] — white noise, independent of frequency
 * Reference: Nyquist (1928), Phys. Rev. 32, 110 */
double noise_thermal_resistor(const resistor_noise_params_t *p)
{
    if (!p || p->R <= 0.0 || p->T <= 0.0) return 0.0;
    return 4.0 * K_BOLTZMANN * p->T * p->R;
}

/* L2: MOS Drain Thermal Noise
 * Sid = 4*k*T*gamma*gm [A^2/Hz] — channel thermal noise
 * gamma = 2/3 (long-channel), 2-3 (short-channel)
 * Reference: van der Ziel (1962); Razavi eq. (7.36) */
double noise_thermal_mos_drain(const mos_thermal_noise_params_t *p)
{
    if (!p || p->gm <= 0.0 || p->T <= 0.0) return 0.0;
    double gamma = (p->gamma > 0.0) ? p->gamma : (2.0/3.0);
    return 4.0 * K_BOLTZMANN * p->T * gamma * p->gm;
}

/* L2: MOS Gate Thermal Noise (induced gate noise)
 * Sig = 4*k*T*delta*gg [A^2/Hz] with gg = omega^2*Cgs^2/(5*gm)
 * Usually negligible at low frequencies.
 * Reference: van der Ziel (1962); Shaeffer & Lee (1997) */
double noise_thermal_mos_gate(double Rg, double T)
{
    if (Rg <= 0.0 || T <= 0.0) return 0.0;
    return 4.0 * K_BOLTZMANN * T * Rg;
}

/* L2: MOS Flicker (1/f) Noise — BSIM model
 * Svv_flicker = Kf / (Cox*W*L*f^Ef) * gm^2 [V^2/Hz]
 * Dominant at low frequencies, proportional to 1/frequency.
 * Also called "pink noise" or "1/f noise".
 * Reference: Hung, Ko, Hu, Cheng (1990); BSIM4 Manual */
double noise_flicker_mos(const mos_flicker_noise_params_t *p)
{
    if (!p || p->f <= 0.0 || p->Cox <= 0.0 || p->W <= 0.0 || p->L <= 0.0) return 0.0;
    double Kf = (p->Kf > 0.0) ? p->Kf : 1e-25;
    double Af = (p->Af > 0.0) ? p->Af : 1.0;
    double Ef = (p->Ef > 0.0) ? p->Ef : 1.0;
    double denom = p->Cox * p->W * p->L * pow(p->f, Ef);
    if (denom < 1e-30) return 0.0;
    double Svv = Kf * pow(p->gm, Af) / denom;
    return Svv;
}

/* L2: Shot Noise in PN Junction / Diode (Schottky, 1918)
 * Sid = 2*q*Idc [A^2/Hz]
 * Arises from discrete nature of charge carriers crossing a barrier.
 * Reference: Schottky (1918), Ann. Phys. 57, 541 */
double noise_shot_diode(double I_dc, double T)
{
    (void)T;
    if (I_dc <= 0.0) return 0.0;
    return 2.0 * Q_ELECTRON * I_dc;
}

/* L2: BJT Collector Shot Noise
 * S_ic = 2*q*Ic [A^2/Hz]
 * Reference: van der Ziel; Gray & Meyer Ch.11.3 */
double noise_shot_bjt_collector(double Ic, double T)
{
    (void)T;
    return noise_shot_diode(Ic, 300.0);
}

/* L2: BJT Base Shot Noise
 * S_ib = 2*q*Ib [A^2/Hz]
 * Reference: Gray & Meyer Ch.11.3 */
double noise_shot_bjt_base(double Ib, double T)
{
    (void)T;
    return noise_shot_diode(Ib, 300.0);
}

/* L3: Integrate noise PSD over frequency band
 * Vn_rms^2 = integral(Svv, f_low, f_high)
 * Approximate with analytical form for thermal + flicker
 * Reference: Razavi eq. (7.50-7.54) */
double noise_integrated_total(const noise_result_t *nr, double f_low, double f_high)
{
    if (!nr || f_low >= f_high || f_low <= 0.0) return 0.0;
    double Vn2_thermal = nr->Svv_thermal * (f_high - f_low);
    double Vn2_flicker = 0.0;
    if (nr->corner_freq > 0.0 && nr->Svv_flicker > 0.0) {
        Vn2_flicker = nr->Svv_flicker * log(f_high / f_low);
    }
    return Vn2_thermal + Vn2_flicker;
}

/* L3: RMS noise voltage */
double noise_rms_voltage(const noise_result_t *nr, double f_low, double f_high)
{
    double total = noise_integrated_total(nr, f_low, f_high);
    return sqrt(total);
}

/* L3: Equivalent Noise Bandwidth for n-pole system
 * ENBW = f_3dB * pi/2 (1-pole), f_3dB * pi/4 (2-pole), etc.
 * Reference: Motchenbacher & Connelly "Low-Noise Electronic System Design" */
double noise_equivalent_noise_bandwidth(double f_3dB, int n_poles)
{
    if (n_poles <= 0 || f_3dB <= 0.0) return 0.0;
    double factors[] = {0.0, 1.57, 1.11, 1.05, 1.025};
    int idx = (n_poles < 5) ? n_poles : 4;
    return f_3dB * factors[idx];
}

/* L3: Combined thermal + flicker PSD */
double noise_combined_psd(double Sv_thermal, double Sv_flicker, double f_corner, double f)
{
    if (f <= 0.0) return 0.0;
    return Sv_thermal * (1.0 + f_corner / f);
}
/* L4: Input-Referred Noise - Common-Source Stage
 * Sv_in = Sv_Rs + 4kT*gamma/gm + flicker_psd/gm^2
 * Reference: Razavi eq. (7.25-7.32) */
int noise_input_referred_cs(mos_type_t type, double gm, double gmb, double Rs,
                             const ic_technology_t *tech, double Id, double W, double L,
                             double f, double T, noise_result_t *nr)
{
    if (!nr) return -1;
    memset(nr, 0, sizeof(noise_result_t));
    (void)type; (void)gmb; (void)tech; (void)Id;
    resistor_noise_params_t rp = {Rs, T};
    nr->Sv_in = noise_thermal_resistor(&rp);
    double gamma = 0.667;
    double Sv_drain = 4.0 * K_BOLTZMANN * T * gamma * gm;
    nr->Sv_in += Sv_drain / (gm * gm);
    mos_flicker_noise_params_t fp;
    memset(&fp, 0, sizeof(fp));
    fp.gm = gm; fp.Cox = 8.42e-3; fp.W = W; fp.L = L;
    fp.Kf = 1e-25; fp.Af = 1.0; fp.Ef = 1.0; fp.f = f;
    double Sv_flicker = noise_flicker_mos(&fp);
    nr->Sv_in += Sv_flicker / (gm * gm);
    nr->Svv_thermal = 4.0 * K_BOLTZMANN * T * gamma / gm;
    nr->Svv_flicker = Sv_flicker / (gm * gm);
    if (nr->Svv_thermal > 1e-20 && nr->Svv_flicker > 1e-20)
        nr->corner_freq = f * nr->Svv_flicker / nr->Svv_thermal;
    nr->Svv_total = nr->Svv_thermal + nr->Svv_flicker;
    nr->integrated_noise = noise_rms_voltage(nr, 1.0, 100e3);
    return 0;
}

/* L4: Input-Referred Noise - Differential Pair */
int noise_input_referred_diff_pair(double gm1, double gm3, double gamma1, double gamma3,
                                    const mos_flicker_noise_params_t *fp1,
                                    const mos_flicker_noise_params_t *fp3,
                                    double f, noise_result_t *nr)
{
    if (!nr) return -1;
    memset(nr, 0, sizeof(noise_result_t));
    double T = 300.0;
    nr->Svv_thermal = 2.0 * 4.0 * K_BOLTZMANN * T * gamma1 / gm1;
    nr->Svv_thermal += 2.0 * 4.0 * K_BOLTZMANN * T * gamma3 * gm3 / (gm1 * gm1);
    if (fp1) nr->Svv_flicker += 2.0 * noise_flicker_mos(fp1) / (gm1 * gm1);
    if (fp3) nr->Svv_flicker += 2.0 * noise_flicker_mos(fp3) * (gm3 * gm3) / (gm1 * gm1 * gm1 * gm1);
    nr->Svv_total = nr->Svv_thermal + nr->Svv_flicker;
    if (nr->Svv_thermal > 1e-20 && nr->Svv_flicker > 1e-20)
        nr->corner_freq = f * nr->Svv_flicker / nr->Svv_thermal;
    nr->integrated_noise = noise_rms_voltage(nr, 1.0, 100e3);
    return 0;
}

/* L4: Input-Referred Noise - Cascode Stage */
int noise_input_referred_cascode(double gm_cs, double gm_cg, double gamma_cs, double gamma_cg,
                                  const mos_flicker_noise_params_t *fp_cs,
                                  double f, noise_result_t *nr)
{
    if (!nr) return -1;
    memset(nr, 0, sizeof(noise_result_t));
    double T = 300.0;
    nr->Svv_thermal = 4.0 * K_BOLTZMANN * T * gamma_cs / gm_cs;
    nr->Svv_thermal += 4.0 * K_BOLTZMANN * T * gamma_cg / (gm_cs * gm_cs * gm_cg * 1000.0);
    if (fp_cs) nr->Svv_flicker = noise_flicker_mos(fp_cs) / (gm_cs * gm_cs);
    nr->Svv_total = nr->Svv_thermal + nr->Svv_flicker;
    nr->integrated_noise = noise_rms_voltage(nr, 1.0, 100e3);
    return 0;
}

/* L4: Friis Formula for Cascaded Stages (Friis, 1944) */
double noise_friis_factor(double F1, double F2, double F3, double G1, double G2)
{
    double F = F1;
    if (G1 > 1e-12) F += (F2 - 1.0) / G1;
    if (G1 > 1e-12 && G2 > 1e-12) F += (F3 - 1.0) / (G1 * G2);
    return F;
}
double noise_friis_temperature(double Te1, double Te2, double Te3, double G1, double G2)
{
    double Te = Te1;
    if (G1 > 1e-12) Te += Te2 / G1;
    if (G1 > 1e-12 && G2 > 1e-12) Te += Te3 / (G1 * G2);
    return Te;
}

/* L4: Noise figure conversions */
double noise_figure_from_factor(double F) { return 10.0 * log10(F); }
double noise_factor_from_dB(double NF_dB) { return pow(10.0, NF_dB / 10.0); }
double noise_temperature_from_factor(double F, double T0) { return T0 * (F - 1.0); }

/* L5: Noise matching - optimal source impedance for minimum NF */
int noise_match_optimal(double Rn, double Gcor, double Bcor, double *Gopt, double *Bopt, double *NF_min)
{
    if (!Gopt || !Bopt || !NF_min) return -1;
    *Bopt = -Bcor;
    double Gu = (Gcor > 0.0) ? Gcor : 0.0;
    *Gopt = (Rn > 0.0) ? sqrt(Gu / Rn + Gcor * Gcor) : Gcor;
    *NF_min = 1.0 + 2.0 * Rn * (*Gopt + Gcor);
    return 0;
}

double noise_optimal_area_min_flicker(double Kf, double Cox, double target_Sv)
{
    if (Kf <= 0.0 || Cox <= 0.0 || target_Sv <= 0.0) return 0.0;
    double gm_est = 1e-3;
    return Kf * gm_est * gm_est / (Cox * target_Sv);
}

/* L7: LNA Noise Analysis - Inductively Degenerated CS (Shaeffer & Lee, JSSC 1997) */
int noise_lna_cs_inductive_degen(double gm, double Ls, double Lg, double Rs, double omega,
                                  double gamma, double delta, double c, double alpha, double T,
                                  noise_result_t *nr)
{
    if (!nr) return -1;
    memset(nr, 0, sizeof(noise_result_t));
    double omega_T = gm / 1e-12;
    (void)Ls; (void)Lg; (void)delta; (void)c;
    double F = 1.0 + (gamma / alpha) * (omega / omega_T) * (omega / omega_T) * gm * Rs;
    nr->noise_factor = F;
    nr->noise_figure = noise_figure_from_factor(F);
    nr->NF_min = nr->noise_figure;
    nr->Rn = gamma / gm;
    nr->Sv_in = 4.0 * K_BOLTZMANN * T * Rs;
    nr->Svv_total = nr->Sv_in * F;
    return 0;
}

/* L2: SNR and SINAD from noise */
double noise_snr_from_result(const noise_result_t *nr, double signal_amplitude, double f_low, double f_high)
{
    if (!nr) return -999.0;
    double Vn_rms = noise_rms_voltage(nr, f_low, f_high);
    if (Vn_rms < 1e-15) return 999.0;
    return 20.0 * log10(signal_amplitude / sqrt(2.0) / Vn_rms);
}
double noise_sinad(const noise_result_t *nr, double signal_rms, double f_low, double f_high)
{
    if (!nr) return -999.0;
    double Vn_rms = noise_rms_voltage(nr, f_low, f_high);
    if (Vn_rms < 1e-15) return 999.0;
    return 20.0 * log10(signal_rms / Vn_rms);
}

/* L3: Noise correlation for differential circuits */
int noise_correlation_diff(double Svv1, double Svv2, double rho, double *Svv_diff, double *Svv_cm)
{
    if (!Svv_diff || !Svv_cm) return -1;
    double cross = 2.0 * rho * sqrt(Svv1 * Svv2);
    *Svv_diff = Svv1 + Svv2 - cross;
    *Svv_cm = Svv1 + Svv2 + cross;
    return 0;
}

/* L2: kT/C Noise - fundamental SC sampling noise */
double noise_ktc(double C, double T)
{
    if (C <= 0.0) return 0.0;
    return K_BOLTZMANN * T / C;
}
