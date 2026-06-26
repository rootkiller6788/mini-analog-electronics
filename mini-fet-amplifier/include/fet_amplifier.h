/**
 * fet_amplifier.h — FET Amplifier Design, Analysis, and Optimization
 *
 * Covers L5 Algorithms/Methods (gain design, gm/Id methodology, load line),
 * L6 Canonical Problems (CS/CD/CG design, cascode, differential pair),
 * L7 Applications (LNA, PA, buffer, instrumentation).
 *
 * Reference: Sedra & Smith, "Microelectronic Circuits" 8th Ed., Ch.7-10
 *            Razavi, "Design of Analog CMOS Integrated Circuits" 2nd Ed., Ch.3-5
 *            Sansen, "Analog Design Essentials" Ch.1-5
 *            Lee, "The Design of CMOS Radio-Frequency Integrated Circuits" 2nd Ed.
 *
 * MIT 6.002, 6.301 / Berkeley EE140, EE240 / Stanford EE114, EE214
 * Michigan EECS 411, 511 / ETH 227-0455, 227-0465
 */

#ifndef FET_AMPLIFIER_H
#define FET_AMPLIFIER_H

#include "fet_types.h"
#include "fet_dc_bias.h"
#include "fet_small_signal.h"
#include "fet_ac_analysis.h"
#include "fet_noise.h"

/* ──────────────────────────────────────────────
 * L5: Amplifier Design Specifications
 * ────────────────────────────────────────────── */

/** Complete amplifier design specification */
typedef struct {
    /* Primary AC specifications */
    double target_gain_db;           /* Target voltage gain [dB] */
    double target_bw_hz;             /* Target -3dB bandwidth [Hz] */
    double target_gbw_hz;            /* Target gain-bandwidth product [Hz] */
    /* Impedance specifications */
    double rin_target;               /* Target input resistance [Ω] (min) */
    double rout_target;              /* Target output resistance [Ω] (max) */
    /* Signal handling */
    double vout_swing_min;           /* Minimum output swing [Vp-p] */
    double vout_swing_target;        /* Target output swing [Vp-p] */
    double vin_max;                  /* Maximum input signal [Vp] for linearity */
    /* Noise specifications */
    double nf_max_db;                /* Maximum noise figure [dB] */
    double vn_input_max_nv;          /* Maximum input noise [nV/√Hz] */
    /* Distortion specifications */
    double hd3_max_dbc;              /* Maximum HD3 [dBc] */
    double iip3_min_dbm;             /* Minimum IIP3 [dBm] */
    /* Supply and power */
    double vdd;                      /* Positive supply voltage [V] */
    double vss;                      /* Negative supply [V] (0 for single-supply) */
    double power_max;                /* Maximum DC power [W] */
    double power_target;             /* Target DC power [W] */
    /* Load and source */
    double rs_source;                /* Source resistance [Ω] */
    double cl_load;                  /* Load capacitance [F] */
    double rl_load;                  /* Load resistance [Ω] */
    /* Technology constraints */
    double l_min;                    /* Minimum channel length [m] */
    double vth_typical;              /* Typical Vth for this technology [V] */
    double gm_id_target;             /* Target gm/Id [1/V] for design */
    /* Other */
    FetAmpConfig config;             /* Amplifier topology */
    double temperature;              /* Operating temperature [K] */
} AmpDesignSpec;

/** Amplifier design result */
typedef struct {
    FetAmpConfig config;             /* Selected topology */
    FetDevice fet;                   /* Designed device parameters */
    BiasNetwork bias;                /* Bias network */
    FetDcOpPoint qpoint;             /* Operating point */
    FetAmpMetrics metrics;           /* Performance metrics */
    HybridPiModel hp_model;          /* Small-signal model */
    TransferFunction tf;             /* Transfer function */
    OctcResult octc;                 /* OCTC bandwidth estimate */
    FetNoiseFigure nf_result;        /* Noise figure */
    double power_actual;             /* Actual DC power [W] */
    double efficiency_pct;           /* Power efficiency [%] */
    bool spec_met;                   /* All specifications met? */
    uint32_t spec_fail_count;        /* Number of failed specifications */
} AmpDesignResult;

/* ──────────────────────────────────────────────
 * L5: gm/Id Design Methodology
 * ────────────────────────────────────────────── */

/**
 * gm/Id lookup table entry.
 * The gm/Id methodology is the standard analog IC design approach
 * for submicron transistors where square-law breaks down.
 *
 * gmid = gm/Id characterizes the trade-off between
 * speed (high gm/Id) and efficiency (low gm/Id).
 */
typedef struct {
    double gmid;                     /* gm/Id ratio [1/V] */
    double vgs;                      /* Gate-source voltage [V] */
    double vds;                      /* Drain-source voltage [V] */
    double id_over_w;                /* Id/(W/L) [A] normalized current density */
    double gm_over_id;               /* gm/Id = same as gmid [1/V] */
    double gm_gds;                   /* Intrinsic gain gm/gds [V/V] */
    double ft;                       /* Transit frequency [Hz] */
    double cgg_over_wl;              /* Normalized gate capacitance [F/μm^2] */
    double vdsat;                    /* Saturation voltage [V] */
} GmIdEntry;

/** gm/Id lookup table for design space exploration */
#define GMID_TABLE_SIZE 128

typedef struct {
    GmIdEntry entries[GMID_TABLE_SIZE];
    uint32_t n_entries;
    double l;                        /* Channel length [m] for this table */
    double vds;                      /* Vds [V] for this table */
    double vsb;                      /* Source-body voltage [V] */
} GmIdTable;

/**
 * Design a transistor using gm/Id methodology.
 *
 * Given gm/Id target:
 * 1. Look up Id/(W/L) from gm/Id table
 * 2. Compute W = (Id_target / Id_over_W)
 * 3. Verify ft, gm/gds, Vdsat meet requirements
 *
 * Reference: Silveira, Flandre, Jespers (1996), "A gm/ID Based Methodology"
 * Complexity: O(log n) binary search on table
 */
bool fet_design_gmid(const GmIdTable *table, double target_gm, double target_id,
                     double l, double *w_out, GmIdEntry *result_entry);

/**
 * Interpolate gm/Id table to find value at arbitrary gm/Id.
 * Uses linear interpolation between nearest table entries.
 *
 * Complexity: O(log n) for search + O(1) for interpolation
 */
bool fet_gmid_interpolate(const GmIdTable *table, double gmid,
                          GmIdEntry *result);

/* ──────────────────────────────────────────────
 * L5: Amplifier Design Functions
 * ────────────────────────────────────────────── */

/**
 * Design a common-source amplifier to meet specifications.
 *
 * Core design procedure:
 * 1. Select bias point (Id, Vds) for target gain/power
 * 2. Size transistor (W/L) for required gm
 * 3. Design bias network for Q-point stability
 * 4. Verify AC performance (gain, bandwidth, noise)
 * 5. Iterate if specs not met
 *
 * This is the most fundamental FET amplifier design problem.
 *
 * Complexity: O(iter) with iter ~ 3-8 design iterations
 */
AmpDesignResult fet_design_cs_amplifier(const AmpDesignSpec *spec,
                                         const FetTechParams *tech);

/**
 * Design a common-drain (source follower) amplifier.
 *
 * Used for impedance buffering: high Rin, low Rout, Av ≈ 1.
 * Critical for driving capacitive loads.
 *
 * Complexity: O(iter) design iterations
 */
AmpDesignResult fet_design_cd_amplifier(const AmpDesignSpec *spec,
                                         const FetTechParams *tech);

/**
 * Design a common-gate amplifier.
 *
 * Low input impedance ≈ 1/gm, used for current-mode circuits
 * and as the upper device in cascode topology.
 *
 * Complexity: O(iter) design iterations
 */
AmpDesignResult fet_design_cg_amplifier(const AmpDesignSpec *spec,
                                         const FetTechParams *tech);

/**
 * Design a cascode amplifier (CS cascaded with CG).
 *
 * Key advantage: eliminates Miller multiplication of Cgd,
 * providing wide bandwidth with high voltage gain.
 *
 * ro_cascode ≈ gm2 * ro1 * ro2 (greatly increased output resistance)
 *
 * Complexity: O(iter) — more complex due to 2-stage interaction
 */
AmpDesignResult fet_design_cascode_amplifier(const AmpDesignSpec *spec,
                                              const FetTechParams *tech);

/**
 * Design a differential pair amplifier with FETs.
 *
 * Key characteristics:
 * - Common-mode rejection ratio (CMRR)
 * - Differential gain vs common-mode gain
 * - Input common-mode range (ICMR)
 * - Offset voltage analysis
 *
 * Reference: Sedra & Smith §8.1-8.5; Razavi §4.4-4.5
 * Complexity: O(iter) design iterations
 */
AmpDesignResult fet_design_diffpair_amplifier(const AmpDesignSpec *spec,
                                               const FetTechParams *tech);

/* ──────────────────────────────────────────────
 * L5: Amplifier Analysis Functions
 * ────────────────────────────────────────────── */

/**
 * Full performance analysis of a designed amplifier.
 * Computes all metrics: gain, bandwidth, noise, distortion, stability.
 *
 * Complexity: O(1) — evaluates closed-form expressions
 */
FetAmpMetrics fet_analyze_amplifier(const FetDevice *fet,
                                     const BiasNetwork *bias,
                                     FetAmpConfig config,
                                     double rs_source, double rl_load,
                                     double cl_load);

/**
 * Compute CMRR for a differential pair.
 *
 * CMRR = |Adm| / |Acm|
 *
 * Adm = gm * (ro || Rd)    [differential-mode gain]
 * Acm ≈ Rd / (2*Rss)       [common-mode gain, Rss = tail current source resistance]
 * CMRR ≈ 2 * gm * Rss      [for large gm*ro]
 *
 * Complexity: O(1)
 */
double fet_diffpair_cmrr(double gm, double ro, double rd, double rss);

/**
 * Input common-mode range of a differential pair.
 *
 * ICMR_low = V_ov_tail + Vov_input + Vss
 * ICMR_high = Vdd - V_ov_load - |Vth_p|  (for PMOS input)
 *
 * Complexity: O(1)
 */
void fet_diffpair_icmr(double vdd, double vss, double vov_tail,
                        double vov_input, double vt_input,
                        double vov_load, double vt_load,
                        bool pmos_input, double *icmr_low, double *icmr_high);

/**
 * Calculate systematic offset voltage of a differential pair.
 *
 * Vos_sys = ΔVth + (Vov/2)*(Δ(W/L)/(W/L) + ΔRd/Rd)
 *
 * Complexity: O(1)
 */
double fet_diffpair_systematic_offset(double delta_vth, double vov,
                                       double delta_wl_ratio,
                                       double delta_rd_ratio);

/* ──────────────────────────────────────────────
 * L6: Current Mirror Design
 * ────────────────────────────────────────────── */

/**
 * Simple FET current mirror.
 * Iout = Iref * (W/L)_2 / (W/L)_1   [ideal]
 *
 * Finite output resistance: ro_mirror = ro2 = 1/(λ*Id)
 * Mismatch: ΔI/I = Δ(W/L)/(W/L) + ΔVth/Vov
 *
 * Complexity: O(1)
 */
double fet_current_mirror_ratio(double wl_out, double wl_ref,
                                 double lambda, double vds_out, double vds_ref,
                                 double *actual_ratio, double *rout_mirror);

/**
 * Cascode current mirror for enhanced output resistance.
 * Rout ≈ gm2 * ro1 * ro2 (much larger than simple mirror)
 *
 * Reference: Sedra & Smith §7.4
 * Complexity: O(1)
 */
double fet_cascode_mirror_rout(double gm2, double ro1, double ro2);

/**
 * Wilson current mirror — high output resistance with feedback.
 *
 * Reference: Wilson (1968), "A monolithic junction FET-NPN operational amplifier"
 * Complexity: O(1)
 */
double fet_wilson_mirror_rout(double gm1, double gm2, double gm3,
                               double ro1, double ro2, double ro3,
                               double *iout_actual);

/**
 * Wide-swing cascode bias generator.
 * Produces bias voltages that maximize output swing while maintaining
 * high output resistance.
 *
 * Complexity: O(1)
 */
void fet_wide_swing_cascode_bias(double vdd, double vov, double vt,
                                  double *vb1, double *vb2,
                                  double *vb3, double *vb4);

/* ──────────────────────────────────────────────
 * L7: Application-Specific Amplifier Design
 * ────────────────────────────────────────────── */

/**
 * Low-Noise Amplifier (LNA) design for RF front-end.
 * Optimizes for minimum noise figure while providing sufficient gain.
 *
 * Target applications: GPS (1575 MHz), WiFi (2.4/5 GHz), Cellular
 *
 * Key metrics: NF < 2dB, S21 > 15dB, S11 < -10dB, IIP3 > -10dBm
 *
 * Complexity: O(1) analytical design equations
 */
AmpDesignResult fet_design_lna(const AmpDesignSpec *spec,
                                const FetTechParams *tech,
                                double target_freq_hz);

/**
 * Power Amplifier (PA) design for RF output stage.
 * Optimizes for power-added efficiency (PAE) and linearity.
 *
 * PAE = (Pout - Pin) / Pdc
 *
 * Target applications: Cellular handset, WiFi, IoT transmitter
 */
AmpDesignResult fet_design_pa(const AmpDesignSpec *spec,
                               const FetTechParams *tech,
                               double target_freq_hz,
                               double *pae_actual);

/**
 * Instrumentation amplifier front-end using FET input.
 * Very high input impedance, high CMRR, programmable gain.
 *
 * Applications: ECG/EEG biopotential, bridge sensors, thermocouples
 */
typedef struct {
    double gain;                     /* Overall gain [V/V] */
    double cmrr_db;                  /* Common-mode rejection [dB] */
    double rin;                      /* Input resistance [Ω] (typically > 1GΩ) */
    double vn_rti;                   /* Input-referred noise [Vrms] */
    double offset_drift;             /* Offset drift [μV/K] */
    double bw_hz;                    /* Bandwidth [Hz] */
} InAmpMetrics;

AmpDesignResult fet_design_instrumentation_amp(const AmpDesignSpec *spec,
                                                const FetTechParams *tech,
                                                InAmpMetrics *inamp);

#endif /* FET_AMPLIFIER_H */
