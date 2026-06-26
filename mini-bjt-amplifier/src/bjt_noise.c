/**
 * bjt_noise.c ? BJT Noise Analysis Implementation
 *
 * Implements the three fundamental noise mechanisms in BJTs:
 *   1. Thermal (Johnson-Nyquist) noise in resistances
 *   2. Shot noise in junction currents
 *   3. Flicker (1/f) noise from surface states
 *
 * And the derived metrics: Noise Figure (NF), noise temperature,
 * optimum source impedance matching, and integrated noise.
 *
 * Reference: Gray & Meyer, Ch. 11
 *            van der Ziel, "Noise in Solid State Devices and Circuits"
 *            Fukui, "Low-Noise Microwave Transistors" (IEEE, 1966)
 */

#include "bjt_noise.h"
#include <math.h>

/* ================================================================
 * Fundamental Noise Mechanisms (L4: Physical Laws)
 * ================================================================ */

double bjt_noise_thermal_voltage(double resistance_ohm,
                                  double temperature_kelvin)
{
    /* Johnson-Nyquist thermal noise voltage spectral density.
     *
     *   vn^2 = 4 * k * T * R  [V^2/Hz]
     *   vn_spectral = sqrt(4*k*T*R)  [V/sqrt(Hz)]
     *
     * This is white noise: constant spectral density at all frequencies
     * (up to quantum limits at extremely high frequencies).
     *
     * Origin: random thermal agitation of charge carriers in any
     * resistive element, independent of DC current.
     *
     * Example: R = 50 ?, T = 300 K
     *   vn = sqrt(4 * 1.38e-23 * 300 * 50) = sqrt(8.28e-19)
     *      = 0.91 nV/sqrt(Hz)
     *
     * Reference: Nyquist, H. (1928), "Thermal Agitation of Electric
     *   Charge in Conductors", Physical Review, vol. 32, pp. 110-113. */

    if (resistance_ohm <= 0.0) return 0.0;
    if (temperature_kelvin <= 0.0) return 0.0;

    return sqrt(4.0 * BJT_K_BOLTZMANN * temperature_kelvin * resistance_ohm);
}

double bjt_noise_thermal_current(double resistance_ohm,
                                  double temperature_kelvin)
{
    /* Thermal noise current spectral density.
     *
     *   in^2 = 4*k*T / R  [A^2/Hz]
     *
     * Norton equivalent of the Thevenin voltage noise source.
     * For large R, current noise is low; for small R, current noise is high. */

    if (resistance_ohm <= 0.0) return 1e12;  /* Infinite for zero R */
    if (temperature_kelvin <= 0.0) return 0.0;

    return sqrt(4.0 * BJT_K_BOLTZMANN * temperature_kelvin / resistance_ohm);
}

double bjt_noise_shot(double dc_current)
{
    /* Shot noise current spectral density.
     *
     *   in_shot^2 = 2 * q * I_DC  [A^2/Hz]
     *   in_shot_spectral = sqrt(2*q*I_DC)  [A/sqrt(Hz)]
     *
     * Shot noise arises from the discrete nature of charge carriers
     * crossing a potential barrier (pn junction). Each carrier crossing
     * is a Poisson process, giving a white noise spectrum.
     *
     * In BJTs, shot noise appears in:
     *   - Collector current: in_c^2 = 2 * q * IC
     *   - Base current: in_b^2 = 2 * q * IB
     *
     * Example: IC = 1 mA
     *   in_c = sqrt(2 * 1.602e-19 * 1e-3) = sqrt(3.204e-22)
     *        = 17.9 pA/sqrt(Hz)
     *
     * Reference: Schottky, W. (1918), "Uber spontane Stromschwankungen
     *   in verschiedenen Elektrizitatsleitern", Annalen der Physik. */

    if (dc_current <= 0.0) return 0.0;
    return sqrt(2.0 * BJT_Q_ELECTRON * dc_current);
}

double bjt_noise_flicker(double dc_current, double frequency_hz,
                          double KF, double AF)
{
    /* Flicker (1/f) noise current spectral density.
     *
     *   in_flicker^2 = KF * I_DC^AF / f  [A^2/Hz]
     *
     * The 1/f power spectrum means noise power is concentrated at
     * low frequencies. The corner frequency fc is where flicker
     * noise equals white (shot + thermal) noise.
     *
     * Origin: traps at the Si-SiO2 interface randomly capture and
     * release carriers, creating conductance fluctuations with a
     * 1/f power spectrum.
     *
     * For BJTs: flicker noise is primarily in base current.
     *   AF ? 1, KF ? 1e-16 to 1e-12 (device-dependent).
     *
     * Reference: Hooge, F.N. (1994), "1/f Noise Sources",
     *   IEEE Trans. Electron Devices. */

    if (dc_current <= 0.0 || frequency_hz <= 0.0) return 0.0;
    if (KF <= 0.0) return 0.0;

    return sqrt(KF * pow(dc_current, AF) / frequency_hz);
}

/* ================================================================
 * Complete Noise Source Calculation
 * ================================================================ */

void bjt_noise_compute_sources(const bjt_spice_params_t *dev,
                                const bjt_qpoint_t *qp,
                                double source_resistance,
                                double frequency_hz,
                                double temperature_kelvin,
                                bjt_noise_sources_t *sources)
{
    /* Compute all noise source spectral densities for a BJT
     * at a given operating point and frequency.
     *
     * The input-referred noise model includes:
     *   1. Source resistance thermal noise: vn_rs^2 = 4kT*Rs
     *   2. Base spreading resistance thermal noise: vn_rb^2 = 4kT*rbb
     *   3. Collector shot noise: in_c^2 = 2q*IC (reflected to input)
     *   4. Base shot noise: in_b^2 = 2q*IB
     *   5. Base flicker noise: in_f^2 = KF*IB^AF/f
     *
     * These are combined into equivalent input voltage and current
     * noise sources in later functions. */

    if (!dev || !qp || !sources) return;

    /* Set base spreading resistance from device params */
    sources->rbb = dev->RB;

    /* Thermal noise of rbb */
    sources->vn_rb = bjt_noise_thermal_voltage(sources->rbb,
                                                temperature_kelvin);

    /* Thermal noise of source resistance */
    sources->vn_rs = bjt_noise_thermal_voltage(source_resistance,
                                                temperature_kelvin);

    /* Thermal noise of RE (external, if present) */
    if (dev->RE > 0.0) {
        sources->vn_re = bjt_noise_thermal_voltage(dev->RE,
                                                    temperature_kelvin);
    } else {
        sources->vn_re = 0.0;
    }

    /* Shot noise in collector current */
    sources->in_c_shot = bjt_noise_shot(qp->IC);

    /* Shot noise in base current */
    sources->in_b_shot = bjt_noise_shot(qp->IB);

    /* Flicker noise in base current */
    sources->in_b_flicker = bjt_noise_flicker(qp->IB, frequency_hz,
                                               dev->KF, dev->AF);
}

/* ================================================================
 * Input-Referred Noise Spectral Densities
 * ================================================================ */

double bjt_noise_input_voltage_density(const bjt_noise_sources_t *ns,
                                        const bjt_ss_params_t *ss,
                                        double Rs)
{
    /* Total input-referred noise voltage spectral density.
     *
     * All noise sources are referred to the input as an equivalent
     * series voltage source. The key reflection rules:
     *   - Collector shot noise ? vn_eq = in_c * (re + Rb/(beta+1))
     *     where in_c flows through re, creating a voltage drop that
     *     appears as an input voltage noise.
     *   - Base current noise ? vn_eq = in_b * Rb
     *     where Rb = Rs + rbb (total base-side resistance).
     *
     * Total (simplified for mid-band, neglecting reactances):
     *   vn_in^2 = vn_Rs^2 + vn_rbb^2
     *           + in_c^2 * (re + (Rs+rbb)/(beta+1))^2   [reflected collector shot]
     *           + in_b^2 * (Rs + rbb)^2                   [base current noise]
     *           + in_b_flicker^2 * (Rs + rbb)^2           [1/f noise]
     *
     * Reference: Gray & Meyer, Eqs. (11.95)-(11.98). */

    double vn_sum_sq, Rb, in_c_reflected;

    if (!ns || !ss) return 0.0;

    Rb = Rs + ns->rbb;

    /* Source and base spreading resistance thermal noise */
    vn_sum_sq = ns->vn_rs * ns->vn_rs + ns->vn_rb * ns->vn_rb;

    /* External RE thermal noise (if present) */
    vn_sum_sq += ns->vn_re * ns->vn_re;

    /* Collector shot noise reflected to input:
     * in_c creates a voltage drop across re (plus reflected R resistances).
     * vn_in_from_ic = in_c * (re + Rb/(beta+1)) */
    {
        double re_eff = ss->re + Rb / (ss->beta + 1.0);
        in_c_reflected = ns->in_c_shot * re_eff;
        vn_sum_sq += in_c_reflected * in_c_reflected;
    }

    /* Base current shot noise creates noise voltage across Rb */
    {
        double vn_base_shot = ns->in_b_shot * Rb;
        vn_sum_sq += vn_base_shot * vn_base_shot;
    }

    /* Base flicker noise */
    {
        double vn_base_flicker = ns->in_b_flicker * Rb;
        vn_sum_sq += vn_base_flicker * vn_base_flicker;
    }

    return sqrt(vn_sum_sq);
}

double bjt_noise_input_current_density(const bjt_noise_sources_t *ns,
                                        const bjt_ss_params_t *ss)
{
    /* Total input-referred noise current spectral density.
     *
     * The equivalent input current noise includes:
     *   - Base shot noise: in_b^2 = 2q*IB
     *   - Base flicker noise: in_f^2 = KF*IB^AF/f
     *   - Collector shot noise reflected: in_c/(beta+1)
     *
     * Total: in_in^2 = in_b_shot^2 + in_b_flicker^2 + (in_c_shot/(beta+1))^2 */

    double in_sum_sq;

    if (!ns || !ss) return 0.0;

    in_sum_sq = ns->in_b_shot * ns->in_b_shot
              + ns->in_b_flicker * ns->in_b_flicker;

    /* Collector shot noise reflected to base as current noise */
    {
        double in_c_reflected = ns->in_c_shot / (ss->beta + 1.0);
        in_sum_sq += in_c_reflected * in_c_reflected;
    }

    return sqrt(in_sum_sq);
}

/* ================================================================
 * Noise Figure Calculation
 * ================================================================ */

double bjt_noise_figure(const bjt_noise_sources_t *ns,
                         const bjt_ss_params_t *ss,
                         double Rs, double temperature_kelvin)
{
    /* Noise Figure (NF) of the BJT amplifier.
     *
     * NF = SNR_in / SNR_out  [ratio]
     *    = 1 + (amplifier_noise_power) / (source_noise_power * gain)
     *
     * For a two-port model with input-referred noise sources vn and in:
     *   NF = 1 + (vn^2 + in^2 * Rs^2) / (4*k*T*Rs)
     *
     * where vn^2 and in^2 are the squared spectral densities of the
     * equivalent input noise sources.
     *
     * NF_dB = 10 * log10(NF)
     *
     * NF = 1 (0 dB) means the amplifier adds no noise (ideal).
     * NF = 2 (3 dB) means the amplifier doubles the noise power.
     *
     * The noise figure is minimized when Rs = Ropt = vn/in,
     * giving NF_min.
     *
     * Reference: Haus, H.A. & Adler, R.B. (1958), "Circuit Theory
     *   of Linear Noisy Networks", MIT Press. */

    double vn_density, in_density;
    double source_noise_power, amp_noise_power;

    if (!ns || !ss) return 1e6;  /* Very poor NF if no data */

    vn_density = bjt_noise_input_voltage_density(ns, ss, Rs);
    in_density = bjt_noise_input_current_density(ns, ss);

    /* Source thermal noise power: 4*k*T*Rs */
    source_noise_power = 4.0 * BJT_K_BOLTZMANN * temperature_kelvin * Rs;

    if (source_noise_power <= 0.0) {
        return 1e6;  /* Degenerate case: Rs = 0 */
    }

    /* Amplifier added noise power: vn^2 + in^2 * Rs^2 */
    amp_noise_power = vn_density * vn_density
                    + in_density * in_density * Rs * Rs;

    return 1.0 + amp_noise_power / source_noise_power;
}

double bjt_noise_temperature(double noise_figure, double T0)
{
    /* Equivalent noise temperature:
     *
     *   Tn = T0 * (NF - 1)
     *
     * The noise temperature represents the temperature increase of
     * the source resistance that would produce the same noise power
     * as the amplifier itself.
     *
     * IEEE standard reference temperature: T0 = 290 K.
     *
     * Example: NF = 2 (3 dB) ? Tn = 290 K.
     *   The amplifier adds as much noise as the source at 290 K. */

    if (noise_figure < 1.0) return 0.0;
    return T0 * (noise_figure - 1.0);
}

/* ================================================================
 * Noise Matching
 * ================================================================ */

double bjt_noise_optimum_source_resistance(double vn_density,
                                            double in_density)
{
    /* Optimum source resistance for minimum noise figure:
     *
     *   Ropt = sqrt(vn^2 / in^2) = vn / in
     *
     * At this source resistance, the source thermal noise balances
     * the amplifier's voltage and current noise contributions.
     *
     * Physical interpretation: Ropt is the source resistance where
     * the amplifier's equivalent noise voltage and noise current
     * produce equal contributions when referred to the input.
     *
     * For a BJT: Ropt ? beta * VT / IC = rpi (approximately).
     *
     * Designing for Ropt is critical in LNA (Low-Noise Amplifier)
     * applications like radio astronomy and satellite receivers. */

    if (in_density <= 0.0) return 1e9;  /* Ideally infinite */
    return vn_density / in_density;
}

double bjt_noise_figure_min(double vn_density, double in_density,
                              double temperature_kelvin)
{
    /* Minimum noise figure at optimum source impedance:
     *
     *   NF_min = 1 + (vn * in) / (2 * k * T)
     *
     * This is the fundamental noise floor of the device ? the
     * lowest possible noise figure, achievable only when the
     * source is matched to Ropt.
     *
     * Reference: Fukui, H. (1966), "Available Power Gain, Noise Figure,
     *   and Noise Measure of Two-Ports", IEEE Trans. Circuit Theory. */

    double numerator;

    if (temperature_kelvin <= 0.0) return 1e6;

    numerator = vn_density * in_density;

    return 1.0 + numerator / (2.0 * BJT_K_BOLTZMANN * temperature_kelvin);
}

double bjt_noise_measure(double NF, double Av)
{
    /* Noise measure M:
     *
     *   M = (NF - 1) / (1 - 1/|Av|)
     *
     * The noise measure accounts for the fact that gain reduces the
     * impact of subsequent stage noise. A lower M indicates a better
     * device for cascaded low-noise systems.
     *
     * For |Av| >> 1: M ? NF - 1.
     * For unity gain: M ? ? (useless as a low-noise stage).
     *
     * Reference: Haus & Adler (1958). */

    double abs_av;

    if (NF < 1.0) return 0.0;

    abs_av = fabs(Av);
    if (abs_av <= 1.0) return 1e6;  /* No gain means infinite noise measure */

    return (NF - 1.0) / (1.0 - 1.0 / abs_av);
}

/* ================================================================
 * Integrated Noise Over Bandwidth
 * ================================================================ */

double bjt_noise_integrate_white(double spectral_density, double bandwidth)
{
    /* Integrate white noise over a given bandwidth.
     *
     * For white noise (constant spectral density S(f) = vn^2):
     *   vn_rms = vn_density * sqrt(BW)
     *
     * This is the total RMS noise voltage over the bandwidth.
     *
     * Example: vn = 1 nV/sqrt(Hz), BW = 1 MHz
     *   vn_rms = 1e-9 * sqrt(1e6) = 1e-9 * 1000 = 1 uVrms
     *
     * This means a signal must exceed ~1 uV to be detectable
     * above the noise floor (assuming SNR > 1). */

    if (bandwidth <= 0.0) return 0.0;
    return spectral_density * sqrt(bandwidth);
}

double bjt_noise_integrate_with_flicker(double vn_white_density,
                                         double flicker_coefficient,
                                         double f_low, double f_high)
{
    /* Integrate noise including 1/f component.
     *
     * For white + flicker noise:
     *   vn_rms^2 = vn_white^2 * (f_high - f_low)
     *            + K_flicker * ln(f_high / f_low)
     *
     * The 1/f contribution becomes significant when f_low is
     * extremely small (DC measurements) or K_flicker is large.
     *
     * For audio (20 Hz - 20 kHz), flicker noise can dominate at
     * the low end, degrading SNR for low-frequency signals. */

    double vn_white_sq, vn_flicker_sq;

    if (f_low <= 0.0 || f_high <= f_low) return 0.0;

    /* White noise contribution */
    vn_white_sq = vn_white_density * vn_white_density
                * (f_high - f_low);

    /* Flicker noise contribution: K_f * ln(f_high/f_low) */
    vn_flicker_sq = flicker_coefficient * flicker_coefficient
                  * log(f_high / f_low);

    return sqrt(vn_white_sq + vn_flicker_sq);
}

double bjt_noise_flicker_corner(double dc_current, double KF,
                                 double AF)
{
    /* Compute the 1/f corner frequency where flicker noise equals
     * white (shot) noise.
     *
     * At the corner frequency fc:
     *   KF * I_DC^AF / fc = 2 * q * I_DC
     *   fc = KF * I_DC^(AF-1) / (2q)
     *
     * For AF = 1:
     *   fc = KF / (2q)
     *
     * For typical BJT: KF ? 1e-16, q = 1.6e-19
     *   fc ? 1e-16 / (3.2e-19) ? 313 Hz
     *
     * Below fc, flicker noise dominates; above, shot noise dominates. */

    if (dc_current <= 0.0 || KF <= 0.0) return 0.0;

    return KF * pow(dc_current, AF - 1.0) / (2.0 * BJT_Q_ELECTRON);
}

/* ================================================================
 * Low-Noise Design Optimization
 * ================================================================ */

double bjt_noise_optimum_ic(double source_resistance,
                             double temperature_kelvin)
{
    /* Optimum collector current for minimum noise figure.
     *
     * The trade-off:
     *   Increasing IC: decreases re (lowers voltage noise from in_c * re)
     *                   increases shot noise (in_c^2 = 2q*IC)
     *
     * The optimum balances these two effects:
     *   IC_opt ? VT / (2 * Rs)  (approximate, from Fukui's analysis)
     *
     * Example: Rs = 50 ?, VT = 26 mV
     *   IC_opt ? 0.026 / (2*50) = 0.26 mA
     *
     * For Rs = 600 ? (audio): IC_opt ? 22 uA
     * For Rs = 50 ? (RF): IC_opt ? 260 uA
     *
     * Reference: Fukui, H. (1966), IEEE Trans. CT. */

    double vt;

    if (source_resistance <= 0.0) return 1e-3;  /* Default 1 mA */

    vt = bjt_vt(temperature_kelvin);
    return vt / (2.0 * source_resistance);
}

double bjt_snr(double signal_power, double noise_power)
{
    /* Signal-to-Noise Ratio (SNR):
     *
     *   SNR = P_signal / P_noise
     *   SNR_dB = 10 * log10(SNR)
     *
     * SNR is the fundamental measure of signal quality.
     * Typical requirements:
     *   - Voice communication: SNR > 10 dB (barely intelligible)
     *   - Audio: SNR > 60 dB (CD quality)
     *   - Digital comm (QPSK): SNR > 10 dB for BER < 1e-6
     *   - GPS: SNR can be < 0 dB (signal below noise, recovered by
     *     correlation gain from spreading code) */

    if (noise_power <= 0.0) return 1e12;  /* Infinite SNR for zero noise */
    return signal_power / noise_power;
}

double bjt_noise_equivalent_power(double vn_rms, double responsivity)
{
    /* Noise Equivalent Power (NEP) for sensor applications.
     *
     * NEP = vn_rms / Responsivity  [W]
     *
     * NEP is the input signal power required to achieve SNR = 1
     * in a 1 Hz bandwidth. It's the minimum detectable power.
     *
     * Used extensively in photodetector, thermal sensor, and
     * instrumentation amplifier specifications.
     *
     * Example: photodiode + transimpedance amp (TIA)
     *   Responsivity = 0.5 A/W, vn_rms = 10 nV/sqrt(Hz)
     *   NEP = 10e-9 / 0.5 = 20 nW (=-47 dBm)
     *
     * Lower NEP = better sensitivity. */

    if (responsivity <= 0.0) return 1e6;
    return vn_rms / responsivity;
}
