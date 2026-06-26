/**
 * fet_noise.h — FET Noise Models and Analysis
 *
 * Covers L1 Definitions (thermal, flicker, shot noise sources),
 * L3 Mathematical Structures (noise spectral densities, correlation),
 * L4 Fundamental Laws (Nyquist theorem, noise figure, Friis formula),
 * L5 Algorithms (input-referred noise calculation, noise matching).
 *
 * Reference: Sansen, "Analog Design Essentials" Ch.3
 *            Gray & Meyer, "Analysis and Design of Analog IC" 5th Ed., Ch.11
 *            Razavi, "RF Microelectronics" 2nd Ed., Ch.2
 *            van der Ziel, "Noise in Solid State Devices and Circuits" (1986)
 *
 * MIT 6.776 / Stanford EE314 / Berkeley EE242 / TU Munich EI7001
 */

#ifndef FET_NOISE_H
#define FET_NOISE_H

#include "fet_types.h"
#include "fet_small_signal.h"

/* ──────────────────────────────────────────────
 * L1: Noise Source Types in FETs
 * ────────────────────────────────────────────── */

/** Fundamental noise mechanisms in FET devices */
typedef enum {
    NOISE_THERMAL_CHANNEL = 0,   /* Channel thermal noise: 4kT·γ·gm */
    NOISE_FLICKER = 1,           /* 1/f noise: Kf·Id^Af/(Cox·L^2·f^Ef) */
    NOISE_GATE_INDUCED = 2,      /* Induced gate noise: δ·4kT·gg */
    NOISE_SHOT_GATE = 3,         /* Gate leakage shot noise (negligible for MOSFET) */
    NOISE_BURST_POPCORN = 4,     /* Random telegraph signal (RTS) noise */
    NOISE_SUBSTRATE_THERMAL = 5, /* Substrate resistance thermal noise */
    NOISE_DIFFUSION = 6,         /* Diffusion noise in subthreshold */
    NOISE_AVALANCHE = 7          /* Avalanche noise near breakdown */
} NoiseMechanism;

/**
 * Noise spectral density — represents power spectral density (PSD)
 * of a noise source as function of frequency.
 */
typedef struct {
    double sv;                    /* Voltage noise PSD [V^2/Hz] */
    double si;                    /* Current noise PSD [A^2/Hz] */
    double sv_si_corr;            /* Cross-correlation PSD [V·A/Hz] */
    double frequency;             /* Frequency [Hz] at which PSD is evaluated */
} NoisePSD;

/* ──────────────────────────────────────────────
 * L1: FET Noise Model Parameters
 * ────────────────────────────────────────────── */

/**
 * Comprehensive FET noise model parameters.
 * These are technology-dependent and geometry-dependent.
 */
typedef struct {
    /* Channel thermal noise */
    double gamma;                  /* Excess noise factor (2/3 for long-ch, 1-3 for short-ch) */
    double gg_coefficient;         /* Induced gate noise conductance factor */
    /* Flicker (1/f) noise parameters */
    double kf;                     /* Flicker noise coefficient [C^2/cm^2 or V^2·F] */
    double af;                     /* Flicker noise exponent for Id (≈1) */
    double ef;                     /* Flicker noise exponent for frequency (≈1) */
    /* Induced gate noise */
    double delta;                  /* Induced gate noise coefficient (≈4/3 for long-ch) */
    /* Gate resistance noise */
    double rg;                     /* Gate poly resistance [Ω] */
    double n_fingers;              /* Number of gate fingers (reduces effective Rg by nf^2) */
    /* Substrate noise */
    double rsub;                   /* Substrate resistance [Ω] (for epi/well) */
    /* Flicker noise corner frequency estimate */
    double fc_flicker;             /* Corner frequency [Hz] where 1/f = thermal */
} FetNoiseParams;

/* ──────────────────────────────────────────────
 * L4: Nyquist Theorem — Thermal Noise
 * ────────────────────────────────────────────── */

/**
 * Nyquist thermal noise PSD for a resistor.
 * Sv = 4*k*T*R [V^2/Hz]
 * Si = 4*k*T/R [A^2/Hz]
 *
 * This is the fundamental noise floor from thermodynamic principles.
 *
 * Reference: Nyquist (1928), "Thermal Agitation of Electric Charge in Conductors"
 * Complexity: O(1)
 */
static inline double thermal_noise_sv(double r, double temp_k) {
    return 4.0 * FET_K_BOLTZMANN * temp_k * r;
}

static inline double thermal_noise_si(double r, double temp_k) {
    return 4.0 * FET_K_BOLTZMANN * temp_k / r;
}

/* ──────────────────────────────────────────────
 * L3: FET Noise Spectral Densities
 * ────────────────────────────────────────────── */

/**
 * Channel thermal noise PSD (drain current noise).
 * Sid = 4*k*T*γ*gm [A^2/Hz]
 *
 * γ = 2/3 for long-channel devices in saturation
 * γ = 1~3 for short-channel devices (hot carrier effects)
 *
 * Reference: van der Ziel (1962), "Thermal Noise in Field-Effect Transistors"
 * Complexity: O(1)
 */
double fet_channel_thermal_noise_si(double gm, double gamma, double temp_k);

/**
 * Flicker (1/f) noise PSD (drain current noise).
 * Sid_1f = Kf * Id^Af / (Cox * L^2 * f^Ef) [A^2/Hz]
 *
 * Also expressed as input-referred voltage:
 * Svg_1f = Kf / (Cox * W * L * f)  (common simplified model)
 *
 * Reference: Hooge (1969), "1/f noise is no surface effect"
 *            Hung et al. (1990), "A unified model for the flicker noise"
 * Complexity: O(1)
 */
double fet_flicker_noise_si(double kf, double id, double af,
                             double cox, double l, double freq, double ef);

/**
 * Induced gate noise PSD.
 * Sig = 4*k*T*δ*gg [A^2/Hz]
 * where gg = ω^2*Cgs^2/(5*gm) for long-channel devices.
 *
 * This noise is partially correlated with channel thermal noise.
 * |c| ≈ j*0.395 for long-channel saturation (theoretical).
 *
 * Reference: van der Ziel (1963), "Gate noise in field effect transistors"
 * Complexity: O(1)
 */
double fet_induced_gate_noise_si(double gm, double cgs, double freq,
                                  double delta, double temp_k);

/**
 * Gate resistance thermal noise (input-referred voltage).
 * Svg_rg = 4*k*T*Rg [V^2/Hz]
 *
 * Rg_effective = Rg_poly / (12 * n_fingers^2) for double-sided contact.
 *
 * Important for RF LNA design.
 * Complexity: O(1)
 */
double fet_gate_resistance_noise_sv(double rg, double n_fingers, double temp_k);

/**
 * Total drain current noise PSD (thermal + flicker).
 * Sid_total(f) = Sid_thermal + Sid_flicker(f)
 *
 * Complexity: O(1)
 */
double fet_total_drain_noise_si(double gm, double gamma, double temp_k,
                                 double kf, double id, double af,
                                 double cox, double l, double freq, double ef);

/**
 * Compute flicker noise corner frequency.
 * fc is where Sid_thermal = Sid_flicker.
 *
 * fc = (Kf * gm * Id^Af) / (4*k*T*γ * Cox * L^2)
 *
 * Complexity: O(1)
 */
double fet_flicker_corner_frequency(double kf, double id, double af,
                                     double gm, double gamma,
                                     double cox, double l, double temp_k);

/* ──────────────────────────────────────────────
 * L4: Noise Figure and Friis Formula
 * ────────────────────────────────────────────── */

/**
 * Input-referred noise model for a FET amplifier.
 * All noise sources referred to gate input:
 *   vn_in^2 = vn_Rg^2 + vn_channel^2 (input-referred) + in_d^2 * |Zs|^2
 */
typedef struct {
    double vn_sv_total;          /* Total input-referred voltage noise [V^2/Hz] */
    double in_si_total;          /* Total input-referred current noise [A^2/Hz] */
    double vn_in_referred;       /* Equivalent input noise voltage with Zs effect [V^2/Hz] */
    double noise_factor;         /* Noise factor F (linear) */
    double noise_figure_db;      /* Noise figure NF [dB] */
    double rn;                   /* Equivalent noise resistance [Ω] */
    double gn;                   /* Equivalent noise conductance [S] */
    double zopt_real;            /* Optimal source impedance (real part) [Ω] */
    double zopt_imag;            /* Optimal source impedance (imag part) [Ω] */
    double nf_min_db;            /* Minimum noise figure [dB] */
    double freq;                 /* Frequency of analysis [Hz] */
} FetNoiseFigure;

/**
 * Compute noise figure for a FET amplifier with given source impedance.
 *
 * NF = SNR_in / SNR_out = 1 + (Noise_added / Noise_from_source)
 *
 * Four-noise-parameter model (Rn, Gn, Yopt):
 * F = Fmin + (Rn/Gs)*|Ys - Yopt|^2
 *
 * where Ys = 1/Zs is the source admittance.
 *
 * Reference: Haus et al. (1960), "Representation of Noise in Linear Twoports"
 *            IRE Subcommittee 7.9 on Noise
 * Complexity: O(1)
 */
FetNoiseFigure fet_noise_figure_calculate(const FetNoiseParams *fnp,
                                           const FetSmallSignalParams *ssp,
                                           double rs_source, double xs_source,
                                           double freq, double temp_k);

/**
 * Noise factor for cascaded stages (Friis formula).
 *
 * F_total = F1 + (F2 - 1)/Gav1 + (F3 - 1)/(Gav1*Gav2) + ...
 *
 * This shows that the first stage dominates the noise figure.
 * Reference: Friis (1944), "Noise Figures of Radio Receivers"
 *
 * Complexity: O(n_stages)
 */
double fet_cascade_noise_factor(const double noise_factors[],
                                 const double available_gains[],
                                 uint32_t n_stages);

/**
 * Compute integrated noise over a frequency band.
 * Integrates input-referred noise PSD from f_low to f_high.
 *
 * vn_rms^2 = ∫ Sv_in(f) df from f_low to f_high
 *
 * Accounts for both thermal (constant) and flicker (1/f) contributions.
 *
 * Complexity: O(1) closed-form integration
 */
double fet_integrated_noise_vrms(const FetNoiseParams *fnp,
                                  const FetSmallSignalParams *ssp,
                                  double f_low, double f_high,
                                  double rs_source, double temp_k);

/**
 * Signal-to-noise ratio at the amplifier output.
 *
 * SNR_out = (Vout_rms)^2 / (Vn_out_rms)^2
 *
 * Complexity: O(1)
 */
double fet_output_snr(double vout_rms, double vn_out_rms);

/**
 * Dynamic range (DR) = 20*log10(Vmax_rms / Vn_in_rms)
 * where Vmax is limited by either distortion or clipping.
 *
 * Complexity: O(1)
 */
double fet_dynamic_range(double vmax_rms, double vn_in_rms);

#endif /* FET_NOISE_H */
