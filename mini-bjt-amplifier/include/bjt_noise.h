/**
 * bjt_noise.h ? BJT Noise Analysis
 *
 * BJT amplifiers have intrinsic noise sources that limit the minimum
 * detectable signal. Understanding noise is critical for:
 *   - Low-noise amplifier (LNA) design (RF front-ends)
 *   - Sensor signal conditioning (audio, instrumentation)
 *   - Communication receiver sensitivity
 *
 * Three primary noise mechanisms in BJTs:
 *   1. Thermal noise (Johnson-Nyquist): 4kTR * BW in all resistances
 *   2. Shot noise: 2qI * BW in DC currents crossing junctions
 *   3. Flicker noise (1/f): KF * I^AF / f in base current
 *
 * Reference: Gray & Meyer, Ch. 11; van der Ziel, "Noise in Solid State Devices"
 *            Fukui (1966), "Low-Noise Microwave Transistors"
 *
 * Knowledge Levels:
 *   L1: Noise definitions (noise figure, noise temperature, SNR)
 *   L2: BJT noise sources (shot, thermal, flicker)
 *   L3: Noise spectral density, noise bandwidth
 *   L4: Nyquist theorem (Johnson noise), Schottky theorem (shot noise)
 *   L5: Noise figure calculation, noise matching
 *   L8: Low-noise design optimization, noise measure
 */

#ifndef BJT_NOISE_H
#define BJT_NOISE_H

#include "bjt_model.h"
#include <math.h>

/* Physical constants for noise analysis */
#define BJT_K_BOLTZMANN  1.380649e-23   /* Boltzmann constant [J/K] */
#define BJT_Q_ELECTRON   1.602176634e-19 /* Elementary charge [C] */

/* ---- L1: Noise Metric Definitions ---- */

/* Noise metrics for a BJT amplifier stage. */
typedef struct {
    double vn_total_rms;        /* Total input-referred noise voltage [Vrms] */
    double in_total_rms;        /* Total input-referred noise current [Arms] */
    double vn_spectral_density; /* Input noise voltage density [V/sqrt(Hz)] */
    double in_spectral_density; /* Input noise current density [A/sqrt(Hz)] */
    double NF;                  /* Noise Figure [ratio] */
    double NF_dB;               /* Noise Figure [dB] */
    double Tn;                  /* Equivalent noise temperature [K] */
    double SNR_in;              /* Input SNR [ratio] */
    double SNR_out;             /* Output SNR [ratio] */
    double NEP;                 /* Noise Equivalent Power [W] (for sensor apps) */
    double noise_bandwidth;     /* Effective noise bandwidth [Hz] */
} bjt_noise_metrics_t;

/* ---- L4: Fundamental Noise Laws ---- */

/* Johnson-Nyquist thermal noise voltage spectral density.
 *   vn^2 = 4 * k * T * R  [V^2/Hz]
 *   vn = sqrt(4*k*T*R)    [V/sqrt(Hz)]
 * Reference: Nyquist (1928), Johnson (1928), Physical Review.
 * Complexity: O(1). */
double bjt_noise_thermal_voltage(double resistance_ohm,
                                  double temperature_kelvin);

/* Thermal noise current spectral density.
 *   in^2 = 4*k*T / R  [A^2/Hz]
 * Complexity: O(1). */
double bjt_noise_thermal_current(double resistance_ohm,
                                  double temperature_kelvin);

/* Shot noise current spectral density.
 *   in_shot^2 = 2 * q * I_DC  [A^2/Hz]
 * Applies to collector current (IC) and base current (IB).
 * Reference: Schottky (1918), Annalen der Physik.
 * Complexity: O(1). */
double bjt_noise_shot(double dc_current);

/* Flicker (1/f) noise current spectral density at frequency f.
 *   in_flicker^2 = KF * I_DC^AF / f  [A^2/Hz]
 * Dominant at low frequencies (< 1 kHz typically).
 * Corner frequency fc is where flicker noise = shot noise.
 * Complexity: O(1). */
double bjt_noise_flicker(double dc_current, double frequency_hz,
                          double KF, double AF);

/* ---- L2: BJT Noise Source Modeling ---- */

/* BJT noise model parameters (input-referred). */
typedef struct {
    double rbb;                 /* Base spreading resistance [Ohm] */
    double vn_rb;               /* Thermal noise of rbb [V/sqrt(Hz)] */
    double in_b_shot;           /* Base current shot noise [A/sqrt(Hz)] */
    double in_c_shot;           /* Collector current shot noise [A/sqrt(Hz)] */
    double in_b_flicker;        /* Base current flicker noise [A/sqrt(Hz)] */
    double vn_re;               /* Thermal noise of RE (if present) */
    double vn_rs;               /* Thermal noise of source resistance */
} bjt_noise_sources_t;

/* Compute all noise source spectral densities at frequency f.
 * Complexity: O(1). */
void bjt_noise_compute_sources(const bjt_spice_params_t *dev,
                                const bjt_qpoint_t *qp,
                                double source_resistance,
                                double frequency_hz,
                                double temperature_kelvin,
                                bjt_noise_sources_t *sources);

/* ---- L5: Noise Figure Calculation ---- */

/* Compute total input-referred noise voltage spectral density.
 * Combines all sources: base spreading resistance thermal noise,
 * shot noise in IC and IB reflected to input, and flicker noise.
 *
 * vn_in^2 = 4kT*(Rs + rbb) + 2q*IC*(Rs + rbb + re)^2/|beta|^2
 *          + 2q*IB*(Rs + rbb)^2 + KF*IB^AF*(Rs + rbb)^2/f
 *
 * Reference: Fukui (1966), IEEE Trans. Circuit Theory.
 * Complexity: O(1). */
double bjt_noise_input_voltage_density(const bjt_noise_sources_t *ns,
                                        const bjt_ss_params_t *ss,
                                        double Rs);

/* Compute total input-referred noise current spectral density.
 * Complexity: O(1). */
double bjt_noise_input_current_density(const bjt_noise_sources_t *ns,
                                        const bjt_ss_params_t *ss);

/* Compute Noise Figure from a two-port perspective.
 * NF = 1 + (vn^2 + in^2 * Rs^2) / (4*k*T*Rs)
 *   = total_output_noise / (gain^2 * source_thermal_noise)
 * Complexity: O(1). */
double bjt_noise_figure(const bjt_noise_sources_t *ns,
                         const bjt_ss_params_t *ss,
                         double Rs, double temperature_kelvin);

/* Convert noise figure to noise temperature:
 *   Tn = T0 * (NF - 1)   where T0 = 290 K (IEEE standard).
 * Complexity: O(1). */
double bjt_noise_temperature(double noise_figure, double T0);

/* ---- L5: Noise Matching (Optimum Source Impedance) ---- */

/* Compute the optimum source resistance for minimum noise figure.
 *   Ropt = sqrt(vn^2 / in^2)
 * At this resistance, the noise figure equals NF_min.
 * Complexity: O(1). */
double bjt_noise_optimum_source_resistance(double vn_density,
                                            double in_density);

/* Compute minimum noise figure at optimum source impedance.
 *   NF_min = 1 + vn * in / (2 * k * T)
 * This is the fundamental noise floor of the transistor.
 * Complexity: O(1). */
double bjt_noise_figure_min(double vn_density, double in_density,
                              double temperature_kelvin);

/* Compute noise measure M (a figure of merit for low-noise design):
 *   M = (NF - 1) / (1 - 1/|Av|)
 * Lower M = better device for low-noise cascade design.
 * Reference: Haus & Adler (1958), "Circuit Theory of Linear Noisy Networks".
 * Complexity: O(1). */
double bjt_noise_measure(double NF, double Av);

/* ---- L5: Integrated Noise Over Bandwidth ---- */

/* Integrate white noise over bandwidth BW.
 * vn_rms = vn_density * sqrt(BW)
 * Complexity: O(1). */
double bjt_noise_integrate_white(double spectral_density, double bandwidth);

/* Integrate noise including 1/f component from f_low to f_high.
 * For flicker noise (1/f spectrum):
 *   vn_rms^2 = vn_white^2 * (f_high - f_low)
 *            + K_integral * ln(f_high / f_low)
 * Complexity: O(1). */
double bjt_noise_integrate_with_flicker(double vn_white_density,
                                         double flicker_coefficient,
                                         double f_low, double f_high);

/* Compute the 1/f corner frequency where flicker = white noise.
 * fc = (KF * I^AF) / (2q * I) = KF * I^(AF-1) / (2q)
 * Complexity: O(1). */
double bjt_noise_flicker_corner(double dc_current, double KF,
                                 double AF);

/* ---- L8: Low-Noise Design Optimization ---- */

/* Compute the optimum collector current for minimum noise figure
 * at a given source resistance and frequency.
 * Trade-off: higher IC reduces voltage noise (lower re) but
 *            increases shot noise. There's an optimal IC.
 * Approximate: IC_opt ? VT / (Rs * 2)
 * Reference: Fukui (1966).
 * Complexity: O(1). */
double bjt_noise_optimum_ic(double source_resistance,
                             double temperature_kelvin);

/* Compute SNR given signal and noise powers.
 * SNR = P_signal / P_noise
 * SNR_dB = 10 * log10(SNR)
 * Complexity: O(1). */
double bjt_snr(double signal_power, double noise_power);

/* Compute Noise Equivalent Power (NEP) for sensor applications.
 * NEP = vn_total_rms / Responsivity  [W]
 * This is the signal power needed for SNR = 1.
 * Complexity: O(1). */
double bjt_noise_equivalent_power(double vn_rms, double responsivity);

#endif /* BJT_NOISE_H */
