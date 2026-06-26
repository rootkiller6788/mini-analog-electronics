/**
 * fet_ac_analysis.h — FET Amplifier Frequency Response Analysis
 *
 * Covers L3 Mathematical Structures (transfer functions, Bode plots, pole-zero analysis),
 * L4 Fundamental Laws (Miller theorem, open-circuit time constants),
 * L6 Canonical Problems (bandwidth estimation, compensation).
 *
 * Reference: Sedra & Smith, "Microelectronic Circuits" 8th Ed., Ch.9-10
 *            Razavi, "Design of Analog CMOS Integrated Circuits" 2nd Ed., Ch.6
 *            Gray & Meyer, "Analysis and Design of Analog IC" 5th Ed., Ch.7
 *
 * MIT 6.002 / Berkeley EE105 / Stanford EE114 / Illinois ECE 451
 */

#ifndef FET_AC_ANALYSIS_H
#define FET_AC_ANALYSIS_H

#include "fet_types.h"
#include "fet_small_signal.h"
#include <complex.h>

/* ──────────────────────────────────────────────
 * L3: Transfer Function Representation
 * ────────────────────────────────────────────── */

/**
 * Rational transfer function: H(s) = N(s)/D(s)
 *
 *            b0 + b1*s + b2*s^2 + ... + bm*s^m
 *  H(s) = ----------------------------------------
 *            a0 + a1*s + a2*s^2 + ... + an*s^n
 *
 * Laplace domain representation of FET amplifier's frequency response.
 */
#define FET_MAX_ORDER 8

typedef struct {
    uint32_t num_order;              /* Numerator polynomial order */
    uint32_t den_order;              /* Denominator polynomial order */
    double complex zeros[FET_MAX_ORDER];  /* Zeros of transfer function */
    double complex poles[FET_MAX_ORDER];  /* Poles of transfer function */
    double k;                             /* Gain constant */
    double dc_gain;                       /* DC gain H(0) = b0/a0 */
} TransferFunction;

/**
 * Bode plot data: magnitude and phase vs frequency.
 * Fundamental tool for frequency-domain analysis.
 */
typedef struct {
    uint32_t n_points;               /* Number of frequency points */
    double *frequencies;             /* Frequency array [Hz] */
    double *magnitude_db;            /* |H(jω)| [dB] */
    double *phase_deg;               /* ∠H(jω) [degrees] */
    TransferFunction *tf;            /* Underlying transfer function */
} BodeData;

/* ──────────────────────────────────────────────
 * L3: Miller Theorem Structures
 * ────────────────────────────────────────────── */

/**
 * Miller theorem: An impedance Z between input and output nodes
 * can be decomposed into two grounded impedances:
 *
 * Z1 = Z / (1 - Av)   [input side]
 * Z2 = Z * Av / (Av - 1)   [output side]
 *
 * where Av = Vout/Vin is the voltage gain between the nodes.
 *
 * This is crucial for understanding why Cgd (gate-drain capacitance)
 * has such a large effect on FET amplifier bandwidth.
 */
typedef struct {
    double z_original;               /* Original bridging impedance [Ω] */
    double av_node;                  /* Voltage gain between the nodes */
    double z_input_equiv;            /* Equivalent input impedance [Ω] = Z/(1-Av) */
    double z_output_equiv;           /* Equivalent output impedance [Ω] = Z*Av/(Av-1) */
    double c_original;               /* Original bridging capacitance [F] */
    double c_input_miller;           /* Miller multiplied input capacitance [F] */
    double c_output_miller;          /* Miller multiplied output capacitance [F] */
} MillerDecomposition;

/**
 * Apply Miller theorem to Cgd of a FET amplifier.
 * Cin_miller = Cgd * (1 - Av)  [where Av is negative for CS, giving large Cin]
 * Cout_miller = Cgd * (1 - 1/Av) ≈ Cgd for large |Av|
 *
 * Complexity: O(1)
 */
MillerDecomposition fet_miller_decompose(double cgd, double av, double freq);

/* ──────────────────────────────────────────────
 * L4: Open-Circuit Time Constants (OCTC) Method
 * ────────────────────────────────────────────── */

/**
 * OCTC method estimates the upper -3dB frequency.
 *
 * ωh ≈ 1 / Σ(Ri_open * Ci)
 *
 * where Ri_open is the resistance seen by capacitor Ci with
 * all other capacitors open-circuited.
 *
 * This is a widely used hand-analysis technique for bandwidth estimation.
 *
 * Reference: Gray & Meyer §7.3; Sedra & Smith §9.4
 */

/** Single time constant contribution */
typedef struct {
    const char *name;                /* Capacitor identifier (e.g., "Cgs", "Cgd_miller") */
    double capacitance;              /* Capacitance [F] */
    double resistance_seen;          /* Resistance seen by this capacitor [Ω] */
    double time_constant;            /* τ = R*C [s] */
    double frequency_corner;         /* f = 1/(2πτ) [Hz] */
} TimeConstant;

#define FET_OCTC_MAX_CAPS 12

typedef struct {
    TimeConstant tc[FET_OCTC_MAX_CAPS];  /* Time constant contributions */
    uint32_t n_caps;                     /* Number of capacitors considered */
    double sum_tau;                      /* Σ Ri*Ci [s] */
    double fh_estimate;                  /* Estimated ωh/(2π) [Hz] */
    double fh_actual;                    /* Actual fh from exact analysis [Hz] */
    double error_pct;                   /* OCTC error [%] (typically < 10%) */
} OctcResult;

/**
 * Compute OCTC for a CS amplifier stage.
 *
 * Capacitors considered:
 *   1. Cgs: R_seen = Rsig' (source resistance looking into gate)
 *   2. Cgd (Miller input): R_seen = Rsig' || Rin(miller)
 *   3. Cgd (Miller output): R_seen = ro || Rd || RL
 *   4. Cds: R_seen = ro || Rd || RL
 *   5. CL (load cap): R_seen = ro || Rd || RL
 *   6. Cs (source bypass): R_seen = (1/gm) || Rs (if unbypassed)
 *
 * Complexity: O(N_caps) where N_caps ≤ FET_OCTC_MAX_CAPS
 */
OctcResult fet_octc_common_source(const HybridPiModel *hp,
                                   double rd, double rl, double rsig,
                                   double cs, double cl);

/**
 * Compute OCTC for a common-gate amplifier stage.
 * CG configuration has inherently wider bandwidth (no Miller effect).
 *
 * Complexity: O(N_caps)
 */
OctcResult fet_octc_common_gate(const HybridPiModel *hp,
                                 double rd, double rl, double rsig);

/**
 * Compute OCTC for a cascode amplifier (CS + CG).
 * The cascode mitigates Miller effect → much wider bandwidth.
 *
 * Complexity: O(N_caps)
 */
OctcResult fet_octc_cascode(const HybridPiModel *hp_cs,
                              const HybridPiModel *hp_cg,
                              double rd, double rl, double rsig);

/* ──────────────────────────────────────────────
 * L5: Transfer Function Analysis Functions
 * ────────────────────────────────────────────── */

/**
 * Construct transfer function from poles and zeros.
 *
 *      H(s) = K * Π(s - zi) / Π(s - pj)
 *
 * Complexity: O(n*m) for evaluation, O(1) for construction
 */
TransferFunction fet_tf_from_pzmap(const double complex poles[], uint32_t np,
                                    const double complex zeros[], uint32_t nz,
                                    double k);

/**
 * Evaluate transfer function at a given complex frequency s = σ + jω.
 * Complexity: O(n+m) where n=den_order, m=num_order
 */
double complex fet_tf_evaluate(const TransferFunction *tf, double complex s);

/**
 * Compute the magnitude of H(jω) at frequency f.
 * Complexity: O(1)
 */
double fet_tf_magnitude(const TransferFunction *tf, double freq_hz);

/**
 * Compute the phase of H(jω) at frequency f.
 * Complexity: O(1)
 */
double fet_tf_phase(const TransferFunction *tf, double freq_hz);

/**
 * Generate Bode plot data (magnitude + phase) over a frequency range.
 * Uses logarithmic frequency spacing.
 *
 * @param tf      Transfer function
 * @param f_start Start frequency [Hz]
 * @param f_end   End frequency [Hz]
 * @param n_pts   Number of points per decade (minimum 10)
 * @param bode    Output: Bode plot data (caller must free bode->frequencies,
 *                bode->magnitude_db, bode->phase_deg)
 *
 * Complexity: O(n_pts * (n+m))
 */
void fet_bode_generate(const TransferFunction *tf, double f_start,
                       double f_end, uint32_t n_pts, BodeData *bode);

/**
 * Free BodeData internal allocations.
 * Complexity: O(1)
 */
void fet_bode_free(BodeData *bode);

/**
 * Find -3dB bandwidth from magnitude data.
 * Searches for f where |H| = |H_max|/sqrt(2) = |H_max| - 3dB.
 *
 * Complexity: O(log N) binary search on monotonic data
 */
double fet_bandwidth_from_bode(const BodeData *bode);

/**
 * Compute the gain-bandwidth product (GBW).
 * GBW = |Av_mid| * BW (for dominant-pole amplifiers).
 *
 * Important figure of merit for amplifier comparison.
 * Complexity: O(1)
 */
double fet_gain_bandwidth_product(double av_mid, double bw_hz);

/**
 * Estimate dominant pole from OCTC result.
 * fp_dominant = 1 / (2π * Στi)
 *
 * Complexity: O(1)
 */
double fet_dominant_pole_from_octc(const OctcResult *octc);

/**
 * Compute total harmonic distortion estimate from small-signal parameters.
 * Uses power series expansion of FET I-V nonlinearity.
 *
 * Id(vgs) ≈ Id_Q + gm*vgs + (gm2/2)*vgs^2 + (gm3/6)*vgs^3
 * where gm2 = ∂²Id/∂Vgs², gm3 = ∂³Id/∂Vgs³
 *
 * HD2 ≈ (1/4) * (gm2/gm) * Vm    (for small Vm)
 * HD3 ≈ (1/24) * (gm3/gm) * Vm^2
 *
 * Reference: Sansen "Analog Design Essentials" §5.3
 * Complexity: O(1)
 */
void fet_distortion_estimate(double gm, double gm2, double gm3,
                              double vm_amplitude, double *hd2_db, double *hd3_db);

#endif /* FET_AC_ANALYSIS_H */
