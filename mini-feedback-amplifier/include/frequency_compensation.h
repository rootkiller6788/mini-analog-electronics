/**
 * @file frequency_compensation.h
 * @brief Frequency Compensation Techniques for Feedback Amplifiers
 *
 * Covers the theory and design of frequency compensation networks
 * that ensure stability in feedback amplifiers:
 *   - Miller (pole-splitting) compensation
 *   - Dominant-pole compensation
 *   - Lead compensation (adds zero before crossover)
 *   - Lag compensation (adds pole to reduce crossover)
 *   - Lead-lag compensation (combined approach)
 *   - Feedforward compensation
 *   - Nested Miller compensation (for 3-stage amplifiers)
 *
 * Key Theorem (Miller, 1920):
 *   A capacitor C between input and output of an inverting gain stage
 *   with gain -A is equivalent to an input capacitance of (1+A)·C.
 *   This "Miller multiplication" enables pole splitting.
 *
 * References:
 *   - Sedra & Smith §11.7 — Frequency Compensation
 *   - Razavi "Design of Analog CMOS ICs", 2nd Ed (2017), Ch. 10
 *   - Gray, Hurst, Lewis & Meyer §9.4 — Compensation of Feedback Amps
 *   - Eschauzier & Huijsing, "Frequency Compensation Techniques for
 *     Low-Power Operational Amplifiers" (1995)
 *
 * Knowledge Layer: L5 (Compensation Algorithms/Methods)
 */

#ifndef FREQUENCY_COMPENSATION_H
#define FREQUENCY_COMPENSATION_H

#include "feedback_amplifier.h"
#include "stability_analysis.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
 * L1: Compensation Network Definitions
 * -------------------------------------------------------------------------- */

/** Compensation strategy enumeration */
typedef enum {
    COMP_NONE           = 0,  /**< No compensation applied                     */
    COMP_DOMINANT_POLE  = 1,  /**< Add single dominant pole by shunt C to gnd */
    COMP_MILLER         = 2,  /**< Miller capacitance across gain stage       */
    COMP_LEAD           = 3,  /**< R-C network adding a zero before crossover */
    COMP_LAG            = 4,  /**< R-C network adding a pole before crossover */
    COMP_LEAD_LAG       = 5,  /**< Combined lead (high-freq boost) + lag      */
    COMP_FEEDFORWARD    = 6,  /**< Direct signal path bypassing slow stages   */
    COMP_NESTED_MILLER  = 7   /**< Nested Miller for 3-stage amplifiers       */
} CompensationType;

/**
 * @brief Dominant-pole compensation parameters
 *
 * Adds a single pole ω_pd = 1/(R·C) shunting a high-impedance node to ground.
 * The dominant pole is placed low enough that |L(jω)| crosses 0 dB with
 * a -20 dB/decade slope, ensuring at least 45° phase margin.
 */
typedef struct {
    double shunt_capacitance_f;  /**< Compensation capacitor value (Farads)    */
    double node_resistance_ohm;  /**< Effective resistance at the node (Ω)     */
    double dominant_pole_hz;     /**< New dominant pole frequency (Hz)         */
    double new_unity_gain_hz;    /**< Resulting unity-gain frequency (Hz)      */
} DominantPoleComp;

/**
 * @brief Miller compensation parameters
 *
 * A capacitor C_c is connected between the input and output of the
 * second stage (inverting), creating:
 *   - Input: effective capacitance C_in = (1 + A_2)·C_c
 *   - Output: effective capacitance C_out ≈ C_c
 *
 * This splits the two poles: the dominant pole moves to a lower frequency
 * and the non-dominant pole moves to a higher frequency, improving PM.
 */
typedef struct {
    double miller_cap_f;         /**< Miller capacitance C_c (Farads)          */
    double stage_gain;           /**< Gain of the stage across which C_c sits */
    double input_resistance_ohm; /**< Effective resistance at input node (Ω)  */
    double output_resistance_ohm;/**< Effective resistance at output node (Ω)  */
    double original_pole1_hz;    /**< Pole 1 frequency before compensation    */
    double original_pole2_hz;    /**< Pole 2 frequency before compensation    */
    double new_pole1_hz;         /**< New (dominant) pole frequency           */
    double new_pole2_hz;         /**< New (non-dominant/split) pole freq      */
    double new_unity_gain_hz;    /**< New unity-gain frequency                */
} MillerCompensation;

/**
 * @brief Lead compensation network parameters
 *
 * Transfer function: H_lead(s) = (1 + s/ω_z) / (1 + s/ω_p)
 * where ω_z < ω_p (zero before pole → phase advance)
 *
 * Adding phase lead near the unity-gain crossover increases phase margin.
 * Implemented as: C in parallel with R1, series with R2.
 */
typedef struct {
    double zero_freq_hz;         /**< Zero frequency ω_z (Hz)                  */
    double pole_freq_hz;         /**< Pole frequency ω_p (Hz)                  */
    double max_phase_lead_deg;   /**< Maximum phase lead provided (degrees)    */
    double max_phase_freq_hz;    /**< Frequency of maximum phase lead (Hz)     */
    double r1_ohm;               /**< Resistor R1 (Ω)                          */
    double r2_ohm;               /**< Resistor R2 (Ω)                          */
    double cap_f;                /**< Capacitor C (F)                          */
} LeadCompensation;

/**
 * @brief Lag compensation network parameters
 *
 * Transfer function: H_lag(s) = (1 + s/ω_z) / (1 + s/ω_p)
 * where ω_p < ω_z (pole before zero → magnitude reduction at HF)
 *
 * Lag compensation reduces the unity-gain frequency by attenuating
 * high-frequency gain, improving phase margin at the expense of bandwidth.
 */
typedef struct {
    double pole_freq_hz;         /**< Pole frequency ω_p (Hz)                  */
    double zero_freq_hz;         /**< Zero frequency ω_z (Hz)                  */
    double dc_attenuation_db;    /**< DC attenuation (ω_p/ω_z in dB)          */
    double r1_ohm;               /**< Series resistor (Ω)                      */
    double r2_ohm;               /**< Shunt resistor (Ω)                       */
    double cap_f;                /**< Capacitor (F)                            */
} LagCompensation;

/**
 * @brief Complete compensation result
 *
 * After applying a compensation strategy to a feedback amplifier,
 * this structure holds the updated parameters and stability margins.
 */
typedef struct {
    CompensationType    type;           /**< Which compensation was applied    */
    FeedbackAmplifier   comp_amplifier; /**< Amplifier parameters after comp   */
    StabilityMargins    margins_after;  /**< Stability margins after comp      */
    double              pm_improvement_deg;  /**< PM increase (degrees)       */
    double              bandwidth_change_hz; /**< BW change (neg = reduction) */
    union {
        DominantPoleComp dominant;
        MillerCompensation miller;
        LeadCompensation   lead;
        LagCompensation    lag;
    } network;                          /**< Compensation network details      */
} CompensationResult;

/* -----------------------------------------------------------------------------
 * L5: Compensation Design Methods
 * -------------------------------------------------------------------------- */

/**
 * @brief Design dominant-pole compensation.
 *
 * Places a new dominant pole at the frequency required to make the
 * loop gain cross 0 dB with a single-pole roll-off (-20 dB/decade).
 * This guarantees at least 45° phase margin.
 *
 * Design procedure:
 *   1. Find frequency ω_g where |L(jω_g)| drops to the value at
 *      which the second-pole contribution reaches -20 dB/dec
 *   2. Place dominant pole at ω_pd = ω_g / A₀
 *   3. The compensated unity-gain frequency ≈ ω_g / A₀·β
 *
 * Complexity: O(1)
 *
 * @param amp           Original amplifier parameters
 * @param target_pm_deg Desired phase margin (degrees, typically 45-60)
 * @return              Compensation result (partially populated)
 */
CompensationResult comp_design_dominant_pole(const FeedbackAmplifier *amp,
                                              double target_pm_deg);

/**
 * @brief Design Miller (pole-splitting) compensation.
 *
 * For a two-stage amplifier with poles at ω_p1 and ω_p2, a Miller
 * capacitor C_c across the second stage achieves pole splitting:
 *
 *   ω'_p1 ≈ 1 / (R₁·(C₁ + (1+A₂)·C_c))
 *   ω'_p2 ≈ g_m2 / (C_L + C_c)
 *
 * The pole-splitting principle (Sedra & Smith §11.7.2):
 *   As C_c increases, ω'_p1 decreases and ω'_p2 increases,
 *   "splitting" the poles apart for better stability.
 *
 * Complexity: O(1)
 *
 * @param amp               Amplifier parameters
 * @param stage_gain        Gain of the stage receiving Miller C
 * @param r_in_ohm          Input resistance at compensation node
 * @param r_out_ohm         Output resistance at compensation node
 * @param target_pm_deg     Desired phase margin (degrees)
 * @return                  Compensation result
 */
CompensationResult comp_design_miller(const FeedbackAmplifier *amp,
                                       double stage_gain,
                                       double r_in_ohm,
                                       double r_out_ohm,
                                       double target_pm_deg);

/**
 * @brief Design lead compensation network.
 *
 * Places a zero at the existing unity-gain frequency and a pole
 * at approximately 10× higher, providing phase boost of:
 *
 *   φ_max = arcsin((α-1)/(α+1))  where α = ω_p / ω_z
 *
 * For α = 10, φ_max ≈ 55° added at ω_max = √(ω_z·ω_p).
 *
 * Reference: Ogata §11.2 — Lead Compensation
 * Complexity: O(1)
 *
 * @param amp              Amplifier parameters
 * @param target_pm_deg    Desired phase margin
 * @return                 Compensation result
 */
CompensationResult comp_design_lead(const FeedbackAmplifier *amp,
                                     double target_pm_deg);

/**
 * @brief Design lag compensation network.
 *
 * Places a pole-zero pair below the existing unity-gain frequency
 * to reduce the gain crossover frequency, trading bandwidth for
 * phase margin improvement.
 *
 * Pole at ω_p = 0.1 · ω_u_new, Zero at ω_z = 10 · ω_p
 *
 * Reference: Ogata §11.3 — Lag Compensation
 * Complexity: O(1)
 *
 * @param amp              Amplifier parameters
 * @param target_pm_deg    Desired phase margin
 * @return                 Compensation result
 */
CompensationResult comp_design_lag(const FeedbackAmplifier *amp,
                                    double target_pm_deg);

/**
 * @brief Design lead-lag compensation.
 *
 * Combines lead (phase boost near crossover) with lag (gain reduction
 * at high frequency). The lead network provides the required PM, while
 * the lag network ensures adequate low-frequency loop gain.
 *
 * Design sequence: design lag first (for adequate PM), then lead
 * (to boost phase at the new crossover).
 *
 * Reference: Ogata §11.4 — Lead-Lag Compensation
 * Complexity: O(1)
 *
 * @param amp              Amplifier parameters
 * @param target_pm_deg    Desired phase margin
 * @return                 Compensation result
 */
CompensationResult comp_design_lead_lag(const FeedbackAmplifier *amp,
                                         double target_pm_deg);

/* -----------------------------------------------------------------------------
 * L5: Compensation Analysis (Post-Design Verification)
 * -------------------------------------------------------------------------- */

/**
 * @brief Verify compensation effectiveness by computing stability margins.
 *
 * Applies the designed compensation to the amplifier model and
 * recomputes stability margins, verifying that the target phase
 * margin is achieved.
 *
 * Complexity: O(1)
 *
 * @param result   [in/out] Compensation result (updated with margins)
 * @return         0 if PM target met, negative if still insufficient
 */
int comp_verify(const FeedbackAmplifier *original,
                CompensationResult *result);

/**
 * @brief Compute the transfer function of the compensation network.
 *
 * Returns T_comp(s) = V_out(s) / V_in(s) for the compensation network,
 * which can be cascaded with the amplifier transfer function.
 *
 * Complexity: O(1)
 *
 * @param result  Compensation result
 * @param tf      [out] Transfer function of the compensation network
 */
void comp_network_transfer_function(const CompensationResult *result,
                                     TransferFunction *tf);

/* -----------------------------------------------------------------------------
 * L5: Pole Placement & Splitting
 * -------------------------------------------------------------------------- */

/**
 * @brief Compute pole-splitting effect from Miller capacitance.
 *
 * For a two-stage amplifier:
 *   Input node:  C_in = C_gs + (1 + |A_2|)·C_c  (Miller multiplication)
 *   Output node: C_out = C_L + C_c·(1 + 1/|A_2|) ≈ C_L + C_c
 *
 * Resulting poles:
 *   ω_p1 = 1 / (R_1 · C_in),  ω_p2 = 1 / (R_2 · C_out)
 *
 * Complexity: O(1)
 *
 * @param original_p1_hz    Pole 1 before compensation (Hz)
 * @param original_p2_hz    Pole 2 before compensation (Hz)
 * @param stage_gain        DC gain of the inverting stage
 * @param c_miller          Miller capacitance (F)
 * @param new_p1_hz         [out] New dominant pole frequency (Hz)
 * @param new_p2_hz         [out] New non-dominant pole frequency (Hz)
 */
void comp_pole_splitting(double original_p1_hz,
                          double original_p2_hz,
                          double stage_gain,
                          double c_miller,
                          double *new_p1_hz,
                          double *new_p2_hz);

/* -----------------------------------------------------------------------------
 * L8: Nested Miller Compensation (Advanced)
 * -------------------------------------------------------------------------- */

/**
 * @brief Design nested Miller compensation (NMC) for 3-stage amplifiers.
 *
 * For a three-stage amplifier with poles at ω_p1, ω_p2, ω_p3,
 * NMC uses two Miller capacitors: C_c1 across stages 2–3 and
 * C_c2 across stages 1–3 (nested).
 *
 * The design places:
 *   ω'_p1 = 1 / (g_m2·R_2·g_m3·R_3·R_1·C_c2)  (dominant)
 *   ω'_p2 = g_m2 / (C_L + C_c1)                  (non-dominant #1)
 *   ω'_p3 = g_m3 / C_L                           (non-dominant #2)
 *
 * References:
 *   - Eschauzier & Huijsing, 1995
 *   - Leung & Mok, "Analysis of Multistage Amplifier Frequency
 *     Compensation", IEEE TCAS-I, 2001
 *
 * Complexity: O(1)
 *
 * @param amp             Amplifier parameters (must have 3 poles)
 * @param gm2             Transconductance of 2nd stage (S)
 * @param gm3             Transconductance of 3rd stage (S)
 * @param r2_ohm          Output resistance of 2nd stage (Ω)
 * @param r3_ohm          Output resistance of 3rd stage (Ω)
 * @param load_cap        Load capacitance (F)
 * @param target_pm_deg   Target phase margin
 * @return                Compensation result
 */
CompensationResult comp_design_nested_miller(const FeedbackAmplifier *amp,
                                              double gm2,
                                              double gm3,
                                              double r2_ohm,
                                              double r3_ohm,
                                              double load_cap,
                                              double target_pm_deg);

/**
 * @brief Check if Nested Miller compensation is applicable.
 *
 * NMC is applicable for 3-stage amplifiers with:
 *   - Three distinct pole frequencies
 *   - High DC gain (>60 dB)
 *   - Capacitive load (output pole is dominant in the forward path)
 *
 * Complexity: O(1)
 *
 * @param amp  Amplifier parameters
 * @return     1 if NMC applicable, 0 otherwise
 */
int comp_is_nmc_applicable(const FeedbackAmplifier *amp);

#ifdef __cplusplus
}
#endif

#endif /* FREQUENCY_COMPENSATION_H */
