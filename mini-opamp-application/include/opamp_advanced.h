#ifndef OPAMP_ADVANCED_H
#define OPAMP_ADVANCED_H

/**
 * opamp_advanced.h - Advanced Op-Amp Topics
 *
 * Covers advanced operational amplifier analysis and design topics:
 * stability analysis, frequency compensation, noise optimization,
 * fully differential amplifiers, current-feedback amplifiers,
 * and composite amplifier architectures.
 *
 * Knowledge Coverage:
 *   L1 (Definitions): PhaseMargin, NoiseFigure, CFAParams structs
 *   L2 (Core Concepts): Stability margins, noise figure, CFA operation
 *   L3 (Math Structures): Bode stability analysis, noise spectral density
 *   L4 (Fundamental Laws): Nyquist stability criterion, Friis noise formula
 *   L5 (Algorithms/Methods): Lead/lag compensation, noise budgeting
 *   L6 (Canonical Problems): Compensated amplifier, low-noise preamplifier
 *   L7 (Applications): Biomedical preamp, audio low-noise design
 *   L8 (Advanced Topics): Nested Miller compensation, chopper stabilization
 *
 * Reference: Gray, Hurst, Lewis, Meyer "Analysis and Design of Analog ICs" Ch.9
 *            Franco Ch.8 (Stability), Ch.7 (Noise)
 *            Sedra & Smith Ch.10 (Frequency Response), Ch.13 (Noise)
 *            Kester "Op Amp Noise" (Analog Devices MT-047)
 *            Mancini "Op Amps for Everyone" Ch.10-12 (TI)
 *
 * Course: MIT 6.775 (CMOS Analog), Berkeley EE140 (Analog IC),
 *   Stanford EE214 (Advanced Analog), ETH 227-0166 (Analog IC Design)
 */

#include "opamp_basic.h"
#include "opamp_config.h"

/*============================================================================
 * L1: DEFINITIONS - Advanced Parameter Types
 *============================================================================*/

/**
 * StabilityMargins - Phase margin and gain margin analysis.
 *
 * Phase Margin (PM): PM = 180 + phase(L(jw)) at the frequency where |L(jw)| = 1 (0 dB)
 * Gain Margin (GM): GM = -20*log10(|L(jw)|) at the frequency where phase(L) = -180 deg
 *
 * Stability criteria (Bode):
 *   PM > 0 deg (typically > 45 deg for good transient response)
 *   GM > 0 dB  (typically > 10 dB)
 *
 * Reference: Bode "Network Analysis and Feedback Amplifier Design" (1945)
 */
typedef struct {
    double phase_margin;        /**< Phase margin (degrees) */
    double gain_margin;         /**< Gain margin (dB) */
    double unity_gain_freq;     /**< Frequency where |L(jw)| = 1 (Hz) */
    double phase_cross_freq;     /**< Frequency where phase = -180 deg (Hz) */
    int is_stable;              /**< 1 = stable (PM > 0 and GM > 0) */
    int is_robust_stable;       /**< 1 = robust stable (PM > 45 and GM > 10) */
} StabilityMargins;

/**
 * CompensationNetwork - Frequency compensation component values.
 *
 * Types:
 *   Dominant-pole compensation: Single capacitor C_c from high-Z node to ground
 *   Miller compensation: C_c between input and output of gain stage
 *   Lead compensation: R-C network in feedback path
 *   Lag compensation: R-C network to ground
 *
 * Reference: Gray & Meyer S9.4-9.5
 */
typedef struct {
    int comp_type;              /**< 0=dominant-pole, 1=Miller, 2=lead, 3=lag */
    double C_c;                 /**< Compensation capacitor (F) */
    double R_c;                 /**< Compensation resistor (Ohm), 0 if none */
    double R_z;                 /**< Zero-cancelling resistor (Ohm) */
    double new_f_p1;            /**< New dominant pole after compensation (Hz) */
    double new_f_ndp;           /**< New non-dominant pole (Hz) */
    double new_PM;              /**< Phase margin after compensation (deg) */
} CompensationNetwork;

/**
 * FullyDiffAmp - Fully differential amplifier configuration.
 *
 * Advantages over single-ended:
 *   - 2x output swing for same supply
 *   - Rejects common-mode noise and even-order harmonics
 *   - No need for bypass capacitors
 *   - Essential for modern low-voltage designs (1.8V, 1.2V)
 *
 * Common-mode feedback (CMFB) is required to set output common-mode
 * voltage, as the differential feedback alone does not determine it.
 *
 * Reference: Razavi "Design of Analog CMOS Integrated Circuits" Ch.9 (2017)
 */
typedef struct {
    double R_f_diff;            /**< Differential feedback resistor (Ohm) */
    double R_g_diff;            /**< Differential gain resistor (Ohm) */
    double R_cm;                /**< Common-mode sensing resistors (Ohm) */
    double V_cm_ref;            /**< Target output common-mode voltage (V) */
    double A_diff;              /**< Differential-mode gain (V/V) */
    double A_cm;                /**< Common-mode gain (V/V) */
    double CMRR;                /**< Common-mode rejection ratio (dB) */
    double V_ocm;               /**< Actual output common-mode voltage (V) */
} FullyDiffAmp;

/**
 * CompositeAmpConfig - Composite amplifier (op-amp + buffer/preamplifier).
 *
 * Combining a precision op-amp with a high-speed buffer can achieve
 * both high DC precision and wide bandwidth.
 *
 * Examples:
 *   - OPA627 (precision) + BUF634 (high-current buffer)
 *   - AD797 (low-noise) + AD811 (high-speed CFA) for wideband low-noise
 *
 * Reference: Jung "Op Amp Applications Handbook" Ch.6 (2005)
 */
typedef struct {
    double A1_gain;             /**< First-stage gain (V/V) */
    double A2_gain;             /**< Second-stage gain (V/V) */
    double f_p1_1;              /**< First op-amp dominant pole (Hz) */
    double f_p1_2;              /**< Second op-amp dominant pole (Hz) */
    double composite_GBW;       /**< Composite gain-bandwidth product (Hz) */
    double composite_noise;     /**< Composite input noise (V/sqrt(Hz)) */
    int stable;                 /**< Composite stability flag */
} CompositeAmpConfig;

/*============================================================================
 * L2: CORE CONCEPTS - Advanced Stability and Noise
 *============================================================================*/

/**
 * analyze_stability_margins - Full Bode-based stability analysis.
 *
 * Computes phase margin, gain margin, unity-gain frequency, and
 * phase crossover frequency from the loop gain L(s) = A_ol(s) * beta.
 *
 * For a dominant-pole op-amp with feedback:
 *   PM = 90 deg - arctan(f_t / f_p2) (two-pole approximation)
 *
 * @param model   Op-amp pole-zero model
 * @param beta    Feedback factor (dimensionless)
 * @param margins Output: stability margins structure
 *
 * Reference: Sedra & Smith S10.10 (Stability of feedback amplifiers)
 */
void analyze_stability_margins(const OpAmpPoleZeroModel *model,
                                double beta, StabilityMargins *margins);

/**
 * compute_noise_RTI - Complete input-referred noise analysis.
 *
 * Noise sources in an op-amp circuit:
 *   1. Op-amp voltage noise e_n (V/sqrt(Hz))
 *   2. Op-amp current noise i_n (A/sqrt(Hz)), flowing through source resistance
 *   3. Johnson noise of resistors: e_R = sqrt(4kTR) (V/sqrt(Hz))
 *
 * Total input-referred noise (uncorrelated sum):
 *   e_ni_total = sqrt(e_n^2 + (i_n*R_eq)^2 + 4kT*R_eq)
 *
 * where R_eq is the equivalent resistance seen from the op-amp inputs.
 *
 * @param noise     Op-amp noise model
 * @param R_source  Source resistance (Ohm)
 * @param R_f       Feedback resistor (Ohm)
 * @param R_g       Ground/gain resistor (Ohm)
 * @param f         Frequency of interest (Hz)
 * @return          Total input-referred noise density (V/sqrt(Hz))
 *
 * Reference: TI Application Report SLVA043 (Noise Analysis)
 *            Motchenbacher & Connelly Ch.5
 */
double compute_noise_RTI(const OpAmpNoiseModel *noise,
                          double R_source, double R_f, double R_g, double f);

/**
 * compute_noise_figure - Noise figure of op-amp circuit.
 *
 * NF = 10 * log10( SNR_in / SNR_out )
 *    = 10 * log10( 1 + e_ni^2 / (4kTR_s) )
 *
 * NF = 0 dB means the circuit adds no noise (impossible in practice).
 * NF < 3 dB is considered excellent.
 *
 * @param noise     Op-amp noise model
 * @param R_source  Source resistance (Ohm)
 * @param f         Frequency (Hz)
 * @return          Noise figure (dB)
 */
double compute_noise_figure(const OpAmpNoiseModel *noise,
                             double R_source, double f);

/*============================================================================
 * L3: MATHEMATICAL STRUCTURES - Bode/Nyquist Stability Theory
 *============================================================================*/

/**
 * loop_gain_magnitude_phase - Compute |L(jw)| and phase at frequency f.
 *
 * L(jw) = A_ol(jw) * beta
 *
 * @param model     Op-amp pole-zero model
 * @param beta      Feedback factor
 * @param f         Frequency (Hz)
 * @param mag_db    Output: |L(jw)| in dB
 * @param phase_deg Output: phase(L(jw)) in degrees
 */
void loop_gain_magnitude_phase(const OpAmpPoleZeroModel *model,
                                double beta, double f,
                                double *mag_db, double *phase_deg);

/**
 * find_unity_gain_frequency - Find f_t by iterative search.
 *
 * Binary search for the frequency where |L(jw)| = 1 (0 dB).
 *
 * @param model    Op-amp model
 * @param beta     Feedback factor
 * @param f_start  Search start (Hz)
 * @param f_end    Search end (Hz)
 * @param tol      Tolerance (fractional)
 * @return         Unity-gain frequency (Hz)
 */
double find_unity_gain_frequency(const OpAmpPoleZeroModel *model,
                                  double beta, double f_start,
                                  double f_end, double tol);

/*============================================================================
 * L4: FUNDAMENTAL LAWS - Nyquist Stability Criterion
 *============================================================================*/

/**
 * nyquist_stability_check - Determine stability from Nyquist criterion.
 *
 * Nyquist criterion (1932):
 *   A feedback system is stable if and only if the Nyquist plot of L(jw)
 *   does NOT encircle the point (-1, j0).
 *
 * For minimum-phase systems (all poles in LHP), this simplifies to:
 *   |L(jw)| < 1 when phase(L(jw)) = -180 deg
 *
 * @param model    Op-amp model
 * @param beta     Feedback factor
 * @return         1 if stable, 0 if unstable
 *
 * Reference: Nyquist "Regeneration Theory" (1932)
 *            Ogata "Modern Control Engineering" Ch.7 (2010)
 */
int nyquist_stability_check(const OpAmpPoleZeroModel *model, double beta);

/*============================================================================
 * L5: ALGORITHMS/METHODS - Compensation Design
 *============================================================================*/

/**
 * design_miller_compensation - Design Miller compensation network.
 *
 * Miller effect: C_c effective = C_c * (1 + |A2|), where A2 is the
 * gain of the second stage. This multiplies the effective capacitance,
 * enabling smaller physical capacitors.
 *
 * Pole splitting: Miller compensation splits the two low-frequency poles
 * apart, moving one to lower frequency (dominant) and the other to higher
 * frequency (non-dominant), improving stability.
 *
 * Design:
 *   1. Choose C_c to place dominant pole such that f_t = g_m1 / (2*pi*C_c)
 *   2. Ensure non-dominant pole is at > 3*f_t for PM > 70 deg
 *   3. Add R_z = 1/(g_m2) to cancel RHP zero if needed
 *
 * @param pm_uncompensated  Phase margin without compensation (deg)
 * @param target_PM         Target phase margin (deg, typically 60)
 * @param GBW               Op-amp gain-bandwidth product (Hz)
 * @param g_m2              Second-stage transconductance (A/V)
 * @param comp              Output: compensation network
 * @return                  1 on success
 *
 * Reference: Gray & Meyer S9.4.2 (Miller compensation)
 *            Razavi "Design of Analog CMOS ICs" Ch.10 (2017)
 */
int design_miller_compensation(double pm_uncompensated, double target_PM,
                                double GBW, double g_m2,
                                CompensationNetwork *comp);

/**
 * design_lead_compensation - Design lead compensation network.
 *
 * Lead compensation adds a zero to increase phase at the unity-gain
 * frequency, improving phase margin.
 *
 * Lead network: R1-C1 in parallel with R2
 *   Transfer function: H(s) = (1 + s*R1*C1) / (1 + s*(R1||R2)*C1)
 *   Zero at f_z = 1/(2*pi*R1*C1)
 *   Pole at f_p = 1/(2*pi*(R1||R2)*C1)
 *
 * @param f_unity     Unity-gain frequency (Hz)
 * @param PM_before   Phase margin before compensation (deg)
 * @param PM_target   Target phase margin (deg)
 * @param comp        Output: compensation network values
 * @return            1 on success
 */
int design_lead_compensation(double f_unity, double PM_before,
                              double PM_target, CompensationNetwork *comp);

/*============================================================================
 * L6: CANONICAL PROBLEMS - Compensated Amplifiers
 *============================================================================*/

/**
 * compensated_amplifier_closed_loop_response - Compute response of
 * compensated amplifier.
 *
 * After compensation, compute the overall closed-loop transfer function,
 * including the effect of the compensation network.
 *
 * @param cfg         Amplifier configuration
 * @param opamp       Op-amp parameters (original)
 * @param comp        Compensation network
 * @param f           Frequency (Hz)
 * @param mag_db      Output: closed-loop gain magnitude (dB)
 * @param phase_deg   Output: phase (deg)
 */
void compensated_amplifier_closed_loop_response(const AmplifierConfig *cfg,
                                                  const OpAmpParams *opamp,
                                                  const CompensationNetwork *comp,
                                                  double f,
                                                  double *mag_db,
                                                  double *phase_deg);

/**
 * fully_diff_amp_common_mode_analysis - Analyze CMFB loop stability.
 *
 * The common-mode feedback loop must be stable for proper operation
 * of fully differential amplifiers. This function computes the CM
 * loop gain and checks stability.
 *
 * @param fda       Fully differential amplifier config
 * @param model     Op-amp pole-zero model
 * @param margins   Output: CM loop stability margins
 * @return          1 if CM loop stable, 0 if unstable
 */
int fully_diff_amp_common_mode_analysis(const FullyDiffAmp *fda,
                                         const OpAmpPoleZeroModel *model,
                                         StabilityMargins *margins);

/*============================================================================
 * L7: APPLICATIONS - Real-World Design
 *============================================================================*/

/**
 * low_noise_preamplifier_design - Design for minimum noise figure.
 *
 * Application: Biomedical ECG preamplifier, audio microphone preamp.
 *
 * Optimal source resistance for minimum noise figure:
 *   R_s_opt = e_n / i_n
 *
 * Noise figure at optimum:
 *   NF_min = 10*log10(1 + e_n*i_n/(2kT)) dB
 *
 * @param noise          Op-amp noise model
 * @param target_gain    Desired voltage gain (V/V)
 * @param bandwidth      Signal bandwidth (Hz)
 * @param R_s_opt        Output: optimal source resistance (Ohm)
 * @param NF_min         Output: minimum achievable NF (dB)
 * @param recommended_R_f Output: recommended feedback resistor (Ohm)
 * @return               1 on success
 *
 * Reference: Motchenbacher & Connelly "Low-Noise Electronic Design" (1993)
 *            Analog Devices AN-358 (Noise and Operational Amplifier Circuits)
 */
int low_noise_preamplifier_design(const OpAmpNoiseModel *noise,
                                   double target_gain, double bandwidth,
                                   double *R_s_opt, double *NF_min,
                                   double *recommended_R_f);

/**
 * chopper_stabilized_amp_analysis - Analysis of chopper amplifier.
 *
 * Chopper stabilization modulates the input signal to a higher frequency
 * (above the 1/f noise corner), amplifies, then demodulates back.
 * This eliminates 1/f noise and DC offset.
 *
 * Effective input offset after chopping: V_os_eff = V_os / A_ol (virtually zero)
 * Residual noise density: e_n_residual = e_n_white (1/f noise eliminated)
 *
 *   f_chop: chopping frequency (typically 1-100 kHz)
 *   f_sig: maximum signal frequency (must be << f_chop to avoid aliasing)
 *
 * @param noise      Op-amp noise model
 * @param f_chop     Chopping frequency (Hz)
 * @param f_sig      Maximum signal bandwidth (Hz)
 * @param V_os       Original offset voltage (V)
 * @param eff_offset Output: effective residual offset (V)
 * @param eff_noise  Output: effective input noise density (V/sqrt(Hz))
 *
 * Reference: Enz & Temes "Circuit Techniques for Reducing the Effects of
 *            Op-Amp Imperfections" Proc. IEEE (1996)
 */
void chopper_stabilized_amp_analysis(const OpAmpNoiseModel *noise,
                                      double f_chop, double f_sig, double V_os,
                                      double *eff_offset, double *eff_noise);

/*============================================================================
 * L8: ADVANCED TOPICS - Nested Miller Compensation
 *============================================================================*/

/**
 * nested_miller_compensation - Design NMC for three-stage amplifier.
 *
 * Nested Miller Compensation (NMC) stabilizes three-stage amplifiers
 * by using two Miller capacitors in a nested configuration.
 *
 * For an op-amp with three gain stages (g_m1, g_m2, g_m3) and load C_L:
 *   C_c1 = g_m1 * C_c2 / g_m2 (nested)
 *   C_c2 = 2 * g_m2 * C_L / (g_m3)
 *
 * This achieves PM > 60 deg while preserving wide bandwidth.
 *
 * @param g_m1    First stage transconductance (A/V)
 * @param g_m2    Second stage transconductance (A/V)
 * @param g_m3    Third stage transconductance (A/V)
 * @param C_L     Load capacitance (F)
 * @param C_c1    Output: first compensation capacitor (F)
 * @param C_c2    Output: second compensation capacitor (F)
 * @param PM      Output: estimated phase margin (deg)
 * @return        1 on success
 *
 * Reference: Eschauzier & Huijsing "Frequency Compensation Techniques
 *            for Low-Power Operational Amplifiers" (1995)
 *            Leung & Mok "Nested Miller Compensation in Low-Power CMOS Design" (2001)
 */
int nested_miller_compensation(double g_m1, double g_m2, double g_m3,
                                double C_L, double *C_c1, double *C_c2,
                                double *PM);

#endif /* OPAMP_ADVANCED_H */
