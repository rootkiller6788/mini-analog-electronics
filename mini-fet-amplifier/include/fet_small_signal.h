/**
 * fet_small_signal.h — FET Small-Signal Model and Analysis
 *
 * Covers L2 Core Concepts (small-signal equivalent circuit),
 * L3 Mathematical Structures (hybrid-π, T-model, y-parameters),
 * L4 Fundamental Laws (gain, impedance derivations).
 *
 * Reference: Sedra & Smith, "Microelectronic Circuits" 8th Ed., Ch.7
 *            Razavi, "Design of Analog CMOS Integrated Circuits" 2nd Ed.
 *            Gray & Meyer, "Analysis and Design of Analog IC" 5th Ed.
 *
 * MIT 6.002 / Berkeley EE105 / Stanford EE114 / ETH 227-0455
 */

#ifndef FET_SMALL_SIGNAL_H
#define FET_SMALL_SIGNAL_H

#include "fet_types.h"
#include <complex.h>

/* ──────────────────────────────────────────────
 * L3: Two-Port Network Parameters for FET Amplifiers
 * ────────────────────────────────────────────── */

/** Y-parameters (admittance matrix) for small-signal FET model
 *
 *  [ i1 ]   [ y11  y12 ] [ v1 ]
 *  [ i2 ] = [ y21  y22 ] [ v2 ]
 *
 *  y11 = input admittance (gate)
 *  y12 = reverse transfer admittance (Cgd feedback)
 *  y21 = forward transfer admittance (gm)
 *  y22 = output admittance (gds + s*Cds)
 */
typedef struct {
    double complex y11;      /* Input admittance [S] */
    double complex y12;      /* Reverse transfer [S] */
    double complex y21;      /* Forward transfer [S] */
    double complex y22;      /* Output admittance [S] */
    double frequency;        /* Evaluation frequency [Hz] */
} FetYParams;

/** S-parameters (scattering matrix) for RF FET amplifiers
 *
 *  [ b1 ]   [ S11  S12 ] [ a1 ]
 *  [ b2 ] = [ S21  S22 ] [ a2 ]
 *
 *  Referenced to Z0 characteristic impedance.
 */
typedef struct {
    double complex s11;      /* Input reflection coefficient */
    double complex s12;      /* Reverse transmission coefficient */
    double complex s21;      /* Forward transmission coefficient */
    double complex s22;      /* Output reflection coefficient */
    double z0;               /* Reference impedance [Ω] (typically 50Ω) */
    double frequency;        /* Evaluation frequency [Hz] */
} FetSParams;

/* ──────────────────────────────────────────────
 * L3: Hybrid-π Small-Signal Model Extraction
 * ────────────────────────────────────────────── */

/** Complete hybrid-π small-signal equivalent circuit parameters
 *  including extrinsic parasitics for RF accuracy.
 */
typedef struct {
    /* Intrinsic FET parameters */
    double gm;               /* Transconductance [S] */
    double gds;              /* Output conductance [S] */
    double gmb;              /* Body transconductance [S] */
    /* Intrinsic capacitances (bias-dependent) */
    double cgs;              /* Gate-source capacitance [F] */
    double cgd;              /* Gate-drain capacitance [F] */
    double cgb;              /* Gate-bulk capacitance [F] */
    double cds;              /* Drain-source capacitance [F] */
    /* Extrinsic parasitics */
    double rg;               /* Gate series resistance [Ω] */
    double rs;               /* Source series resistance [Ω] */
    double rd;               /* Drain series resistance [Ω] */
    double rsub;             /* Substrate resistance [Ω] */
    /* Junction capacitances */
    double csb;              /* Source-bulk junction capacitance [F] */
    double cdb;              /* Drain-bulk junction capacitance [F] */
} HybridPiModel;

/**
 * Extraction of hybrid-π parameters from DC operating point
 * and device geometry/technology.
 *
 * Reference: Tsividis §4.3 "Small-signal modeling"
 * Complexity: O(1) — closed-form expressions
 */
HybridPiModel fet_extract_hybrid_pi(const FetDevice *fet);

/* ──────────────────────────────────────────────
 * L3: T-Model (simplified, useful for CG and CD analysis)
 * ────────────────────────────────────────────── */

typedef struct {
    double gm;               /* Transconductance [S] */
    double rs;               /* Source resistance = 1/gm [Ω] */
    double ro;               /* Output resistance = 1/gds [Ω] */
    double r_body;           /* Body effect resistance = 1/gmb [Ω] */
} FetTModel;

/**
 * Extract T-model parameters from hybrid-π parameters.
 * T-model especially useful for common-gate and common-drain analysis.
 *
 * Complexity: O(1)
 */
FetTModel fet_hybrid_pi_to_tmodel(const HybridPiModel *hp);

/* ──────────────────────────────────────────────
 * L2: Amplifier Performance Metrics
 * ────────────────────────────────────────────── */

/**
 * Small-signal performance of a single-stage FET amplifier.
 * Covers DC operating point and AC small-signal figures.
 */
typedef struct {
    FetAmpConfig config;     /* Amplifier topology */
    FetDcOpPoint qpoint;     /* DC operating point */
    /* Gains */
    double av;               /* Voltage gain Av = vout/vin [V/V] */
    double av_db;            /* Voltage gain [dB] */
    double ai;               /* Current gain Ai = iout/iin [A/A] */
    double ai_db;            /* Current gain [dB] */
    double gm_eff;           /* Effective transconductance [S] */
    double ap;               /* Power gain [W/W] */
    double ap_db;            /* Power gain [dB] */
    /* Impedances */
    double rin;              /* Input resistance [Ω] */
    double rout;             /* Output resistance [Ω] */
    double rin_w_miller;     /* Input resistance accounting for Miller effect [Ω] */
    double cin;              /* Input capacitance [F] */
    double cout;             /* Output capacitance [F] */
    /* Frequency */
    double f_low;            /* Lower -3dB frequency [Hz] */
    double f_high;           /* Upper -3dB frequency [Hz] */
    double bw;               /* Bandwidth = f_high - f_low [Hz] */
    double gbw;              /* Gain-bandwidth product [Hz] */
    /* Distortion estimates */
    double hd2_1mv;          /* HD2 at 1mV input [dBc] */
    double hd3_1mv;          /* HD3 at 1mV input [dBc] */
    double iip3;             /* Input-referred 3rd-order intercept [dBm] */
    /* Noise */
    double vn_input;         /* Input-referred noise voltage [V/√Hz] */
    double in_input;         /* Input-referred noise current [A/√Hz] */
    double nf_db;            /* Noise figure [dB] */
    double nf_min_db;        /* Minimum noise figure [dB] */
} FetAmpMetrics;

/* ──────────────────────────────────────────────
 * L4: Fundamental Formulas for Amplifier Gain
 * ────────────────────────────────────────────── */

/**
 * Common-Source amplifier voltage gain (ideal, no load).
 * Av = -gm * (ro || Rd)
 *
 * The negative sign indicates 180° phase inversion.
 * Complexity: O(1)
 */
double fet_cs_gain_ideal(double gm, double ro, double rd);

/**
 * Common-Source amplifier with source degeneration.
 * Av ≈ -Rd / (1/gm + Rs)  for gm*ro >> 1
 *
 * Source degeneration reduces gain but improves linearity
 * and increases input/output impedance.
 * Complexity: O(1)
 */
double fet_cs_gain_degenerated(double gm, double ro, double rd, double rs);

/**
 * Common-Gate amplifier voltage gain.
 * Av = gm * (ro || Rd)  (non-inverting)
 *
 * Low input impedance ≈ 1/gm makes CG good for current-mode circuits.
 * Complexity: O(1)
 */
double fet_cg_gain_ideal(double gm, double ro, double rd, double rsig);

/**
 * Common-Drain (Source Follower) voltage gain.
 * Av = gm*Rs / (1 + gm*Rs) ≈ 1 for gm*Rs >> 1
 *
 * Unity gain, high input impedance, low output impedance — ideal buffer.
 * Complexity: O(1)
 */
double fet_cd_gain_ideal(double gm, double gmb, double ro, double rs);

/**
 * Compute output resistance of a source follower.
 * Rout = (1/gm) || (1/gmb) || ro || Rs
 * Complexity: O(1)
 */
double fet_cd_output_resistance(double gm, double gmb, double ro, double rs);

/* ──────────────────────────────────────────────
 * L3: Y-parameter Conversion Functions
 * ────────────────────────────────────────────── */

/**
 * Compute Y-parameters from hybrid-π model at a given frequency.
 *
 * y11 = s*(Cgs + Cgd)
 * y12 = -s*Cgd
 * y21 = gm - s*Cgd
 * y22 = gds + s*(Cds + Cgd)
 *
 * Complexity: O(1)
 */
FetYParams fet_hp_to_yparams(const HybridPiModel *hp, double freq_hz);

/**
 * Convert Y-parameters to S-parameters.
 *
 * S = (I - Z0*Y) * (I + Z0*Y)^(-1)
 * where Z0 is the reference impedance (typically 50Ω).
 *
 * Reference: Pozar "Microwave Engineering" 4th Ed., §4.3
 * Complexity: O(1)
 */
FetSParams fet_yparams_to_sparams(const FetYParams *y, double z0);

/**
 * Convert S-parameters back to Y-parameters.
 * Useful for de-embedding and parameter extraction.
 *
 * Complexity: O(1)
 */
FetYParams fet_sparams_to_yparams(const FetSParams *s);

/**
 * Compute Rollett stability factor K (Linville factor).
 *
 * K = (1 - |S11|^2 - |S22|^2 + |Δ|^2) / (2 * |S12*S21|)
 * Δ = S11*S22 - S12*S21
 *
 * K > 1 and |Δ| < 1 → unconditionally stable.
 *
 * Reference: Rollett (1962), "Stability and Power-Gain Invariants"
 * Complexity: O(1)
 */
double fet_rollett_stability_k(const FetSParams *s);

/**
 * Compute the maximum available gain (MAG) when K >= 1.
 * MAG = |S21/S12| * (K - sqrt(K^2 - 1))
 * Complexity: O(1)
 */
double fet_max_available_gain(const FetSParams *s);

#endif /* FET_SMALL_SIGNAL_H */
