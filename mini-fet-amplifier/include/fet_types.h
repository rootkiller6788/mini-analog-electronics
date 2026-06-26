/**
 * fet_types.h — FET Device Types, Parameters, and Operating Regions
 *
 * Covers L1 Definitions: MOSFET/JFET structure, device parameters,
 * operating region classification, SPICE model parameters.
 *
 * Reference: Sedra & Smith, "Microelectronic Circuits" 8th Ed., Ch.5-6
 *            Tsividis & McAndrew, "Operation and Modeling of the MOS Transistor" 3rd Ed.
 *
 * MIT 6.002 / Berkeley EE105 / Stanford EE114 / ETH 227-0455
 */

#ifndef FET_TYPES_H
#define FET_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ──────────────────────────────────────────────
 * L1: FET Device Category Enumeration
 * ────────────────────────────────────────────── */

/** FET transistor family classification */
typedef enum {
    FET_JFET_N_CHANNEL = 0,       /* N-channel Junction FET */
    FET_JFET_P_CHANNEL = 1,       /* P-channel Junction FET */
    FET_MOSFET_N_ENHANCEMENT = 2, /* N-channel enhancement-mode MOSFET */
    FET_MOSFET_P_ENHANCEMENT = 3, /* P-channel enhancement-mode MOSFET */
    FET_MOSFET_N_DEPLETION = 4,   /* N-channel depletion-mode MOSFET */
    FET_MOSFET_P_DEPLETION = 5,   /* P-channel depletion-mode MOSFET */
    FET_MESFET_N = 6,             /* N-channel MESFET (GaAs) */
    FET_MESFET_P = 7,             /* P-channel MESFET (GaAs) */
    FET_HEMT = 8,                 /* High Electron Mobility Transistor */
    FET_FINFET_N = 9,             /* N-channel FinFET (advanced node) */
    FET_FINFET_P = 10             /* P-channel FinFET (advanced node) */
} FetType;

/** FET operating region — fundamental to understanding amplifier bias */
typedef enum {
    FET_REGION_CUTOFF = 0,           /* VGS < Vt (enhancement) or VGS < Vp (JFET/depletion) */
    FET_REGION_TRIODE = 1,           /* VDS < VGS - Vt, linear/resistive region */
    FET_REGION_SATURATION = 2,       /* VDS >= VGS - Vt, active/amplification region */
    FET_REGION_SUBTHRESHOLD = 3,     /* VGS slightly below Vt, weak inversion */
    FET_REGION_VELOCITY_SAT = 4,     /* Short-channel velocity saturation */
    FET_REGION_BREAKDOWN = 5         /* Drain-source breakdown */
} FetRegion;

/** FET amplifier topology configurations — L1 & L2 */
typedef enum {
    FET_AMP_COMMON_SOURCE = 0,            /* CS: voltage gain, phase inversion */
    FET_AMP_COMMON_GATE = 1,              /* CG: low input Z, no phase inversion */
    FET_AMP_COMMON_DRAIN = 2,             /* CD: source follower, unity gain buffer */
    FET_AMP_CASCODE = 3,                  /* CS+CG: wide bandwidth, high output Z */
    FET_AMP_DIFFERENTIAL_PAIR = 4,        /* Differential: CMRR, balanced */
    FET_AMP_CURRENT_MIRROR = 5,           /* Current mirror: biasing */
    FET_AMP_FOLDED_CASCODE = 6,           /* Folded cascode: rail-to-rail input */
    FET_AMP_TELESCOPIC_CASCODE = 7,       /* Telescopic cascode: high gain */
    FET_AMP_COMPLEMENTARY_CS = 8,         /* CMOS push-pull inverter amplifier */
    FET_AMP_REGULATED_CASCODE = 9         /* Regulated/gain-boosted cascode */
} FetAmpConfig;

/* ──────────────────────────────────────────────
 * L1: MOSFET Physical and Electrical Parameters
 * ────────────────────────────────────────────── */

/** Fundamental physical constants for FET modeling */
#define FET_Q_ELECTRON     1.602176634e-19   /* Electron charge [C] */
#define FET_K_BOLTZMANN    1.380649e-23      /* Boltzmann constant [J/K] */
#define FET_EPSILON_0      8.8541878128e-12  /* Vacuum permittivity [F/m] */
#define FET_EPSILON_SIO2   3.9               /* SiO2 relative permittivity */
#define FET_EPSILON_SI     11.7              /* Si relative permittivity */
#define FET_THERMAL_VT_300K 0.02585          /* Thermal voltage kT/q at 300K [V] */

/** MOSFET technology parameters — L1: device-level definitions */
typedef struct {
    double tox;             /* Gate oxide thickness [m] */
    double cox;             /* Gate oxide capacitance per unit area [F/m^2] = εox/tox */
    double mu_n;            /* Electron mobility [cm^2/V·s] */
    double mu_p;            /* Hole mobility [cm^2/V·s] */
    double vt_n;            /* NMOS threshold voltage [V] */
    double vt_p;            /* PMOS threshold voltage [V] (typically negative) */
    double gamma_body;      /* Body effect coefficient [V^1/2] */
    double phi_f;           /* Fermi potential [V] */
    double lambda;          /* Channel-length modulation factor [1/V] */
    double cj0;             /* Zero-bias junction capacitance [F/m^2] */
    double mj;              /* Junction grading coefficient */
    double pb;              /* Junction built-in potential [V] */
    double cgso;            /* Gate-source overlap capacitance [F/m] */
    double cgdo;            /* Gate-drain overlap capacitance [F/m] */
    double rsh;             /* Sheet resistance [Ω/sq] */
    double n_sub;           /* Substrate doping concentration [cm^-3] */
    double xj;              /* Junction depth [m] */
    double ld;              /* Lateral diffusion [m] */
    double vmax;            /* Saturation velocity [m/s] */
    double theta;           /* Mobility degradation factor [1/V] */
    double eta;             /* Subthreshold swing factor */
} FetTechParams;

/* ──────────────────────────────────────────────
 * L1: FET Device Geometry Parameters
 * ────────────────────────────────────────────── */

/** Device geometry — distinct from technology parameters */
typedef struct {
    double w;               /* Channel width [m] */
    double l;               /* Channel length [m] */
    double w_over_l;        /* Aspect ratio W/L */
    double ad;              /* Drain diffusion area [m^2] */
    double as;              /* Source diffusion area [m^2] */
    double pd;              /* Drain diffusion perimeter [m] */
    double ps;              /* Source diffusion perimeter [m] */
    uint32_t nf;            /* Number of fingers */
    double m;               /* Multiplicity factor */
} FetGeometry;

/* ──────────────────────────────────────────────
 * L1: DC Operating Point (Bias Point)
 * ────────────────────────────────────────────── */

/** Complete DC operating point — the Q-point for amplifier design */
typedef struct {
    double vgs;             /* Gate-source voltage [V] */
    double vds;             /* Drain-source voltage [V] */
    double vbs;             /* Body-source voltage [V] (body effect) */
    double id;              /* Drain current [A] */
    double is;              /* Source current [A] */
    double ig;              /* Gate leakage current [A] (nominally 0 for MOSFET) */
    double gm;              /* Transconductance [S] = ∂Id/∂Vgs */
    double gds;             /* Output conductance [S] = 1/rds = ∂Id/∂Vds */
    double gmb;             /* Body transconductance [S] = ∂Id/∂Vbs */
    double vth_effective;   /* Effective threshold with body effect [V] */
    double vdsat;           /* Drain-source saturation voltage [V] = Vgs - Vth */
    double vov;             /* Overdrive voltage [V] = Vgs - Vth */
    FetRegion region;       /* Operating region */
    bool valid;             /* Q-point convergence flag */
} FetDcOpPoint;

/* ──────────────────────────────────────────────
 * L1: Small-Signal Parameters
 * ────────────────────────────────────────────── */

/** Small-signal equivalent circuit parameters (hybrid-π model) */
typedef struct {
    double gm;              /* Transconductance [S] */
    double gds;             /* Output conductance [S] */
    double gmb;             /* Body transconductance [S] */
    double rds;             /* Output resistance [Ω] = 1/gds */
    double cgs;             /* Gate-source capacitance [F] */
    double cgd;             /* Gate-drain capacitance (Miller) [F] */
    double cgb;             /* Gate-bulk capacitance [F] */
    double cds;             /* Drain-source junction capacitance [F] */
    double csb;             /* Source-bulk junction capacitance [F] */
    double cdb;             /* Drain-bulk junction capacitance [F] */
    double ft;              /* Transit frequency [Hz] where |h21|=1 */
    double fmax;            /* Maximum oscillation frequency [Hz] */
    double rg;              /* Gate resistance [Ω] (poly/contact resistance) */
    double rs;              /* Source series resistance [Ω] */
    double rd;              /* Drain series resistance [Ω] */
} FetSmallSignalParams;

/* ──────────────────────────────────────────────
 * L1: Complete FET Device Model
 * ────────────────────────────────────────────── */

/** Aggregated FET device model combining technology, geometry, and state */
typedef struct {
    FetType type;                    /* Device type classification */
    FetTechParams tech;              /* Technology parameters */
    FetGeometry geo;                 /* Physical geometry */
    FetDcOpPoint bias;               /* DC operating point */
    FetSmallSignalParams ss;         /* Small-signal parameters */
    double temperature;              /* Operating temperature [K] */
    const char *model_name;          /* SPICE model name */
} FetDevice;

/* ──────────────────────────────────────────────
 * L3: Mathematical Structure — FET I-V Equations
 * The central square-law and related physics
 * ────────────────────────────────────────────── */

/**
 * Compute the gate oxide capacitance per unit area.
 * Cox = ε0 * ε_SiO2 / tox
 * Complexity: O(1)
 */
static inline double fet_cox_from_tox(double tox) {
    return (FET_EPSILON_0 * FET_EPSILON_SIO2) / tox;
}

/**
 * Compute process transconductance parameter k' = μ * Cox.
 * k'n = μn * Cox for NMOS; k'p = μp * Cox for PMOS.
 * Units: [A/V^2]
 * Complexity: O(1)
 */
static inline double fet_kprime(double mu, double cox) {
    return mu * 1e-4 * cox;  /* mu in cm^2/V·s -> m^2/V·s */
}

/**
 * Device transconductance parameter: β = k' * (W/L) = μ*Cox*(W/L).
 * Also written as K = (1/2) * μ*Cox*(W/L) in some textbooks.
 * Complexity: O(1)
 */
static inline double fet_beta(double kp, double w_over_l) {
    return kp * w_over_l;
}

/**
 * MOSFET drain current in SATURATION (square-law, long-channel).
 * Id = (1/2) * μn*Cox * (W/L) * (Vgs - Vth)^2 * (1 + λ*Vds)
 *
 * This is the fundamental Shockley square-law for MOSFETs.
 * Reference: Sedra & Smith Eq.(5.17); S.M. Sze "Physics of Semiconductor Devices"
 * Complexity: O(1)
 */
static inline double fet_id_saturation(double beta, double vgs, double vth,
                                        double lambda, double vds) {
    double vov = vgs - vth;
    if (vov <= 0.0) return 0.0;
    return 0.5 * beta * vov * vov * (1.0 + lambda * vds);
}

/**
 * MOSFET drain current in TRIODE (linear) region.
 * Id = μn*Cox * (W/L) * [(Vgs - Vth)*Vds - Vds^2/2] * (1 + λ*Vds)
 *
 * Reference: Sedra & Smith Eq.(5.13)
 * Complexity: O(1)
 */
static inline double fet_id_triode(double beta, double vgs, double vth,
                                    double vds, double lambda) {
    double vov = vgs - vth;
    if (vov <= 0.0) return 0.0;
    double inner = vov * vds - 0.5 * vds * vds;
    if (inner < 0.0) inner = 0.0;
    return beta * inner * (1.0 + lambda * vds);
}

/**
 * Subthreshold conduction (weak inversion).
 * Id = I0 * (W/L) * exp((Vgs - Vth) / (n*Vt)) * (1 - exp(-Vds/Vt))
 *
 * Reference: Tsividis, "Operation and Modeling of the MOS Transistor"
 * n: subthreshold swing factor (typically 1.0-2.0)
 * I0: technology-dependent current (typically 100-500 nA for W/L=1)
 * Complexity: O(1)
 */
static inline double fet_id_subthreshold(double w_over_l, double vgs,
                                          double vth, double vds, double n,
                                          double i0) {
    double vt = FET_THERMAL_VT_300K;
    return i0 * w_over_l * exp((vgs - vth) / (n * vt))
           * (1.0 - exp(-vds / vt));
}

/**
 * Determine FET operating region based on terminal voltages.
 * Long-channel classification:
 *   Vgs < Vth  → CUTOFF
 *   Vds < Vov  → TRIODE
 *   Vds >= Vov → SATURATION
 * Complexity: O(1)
 */
static inline FetRegion fet_classify_region(double vgs, double vds,
                                              double vth) {
    double vov = vgs - vth;
    if (vgs <= vth) {
        return (vgs > vth - 0.1) ? FET_REGION_SUBTHRESHOLD : FET_REGION_CUTOFF;
    }
    if (vds < vov) {
        return FET_REGION_TRIODE;
    }
    return FET_REGION_SATURATION;
}

/**
 * Transconductance in saturation: gm = ∂Id/∂Vgs = β*(Vgs-Vth)*(1+λ*Vds)
 * Also: gm = 2*Id/Vov = sqrt(2*β*Id*(1+λ*Vds))
 * Complexity: O(1)
 */
static inline double fet_gm_saturation(double beta, double vgs, double vth,
                                        double lambda, double vds) {
    double vov = vgs - vth;
    if (vov <= 0.0) return 0.0;
    return beta * vov * (1.0 + lambda * vds);
}

/**
 * Output conductance: gds = ∂Id/∂Vds = λ*Id / (1+λ*Vds) ≈ λ*Id
 * Output resistance: rds = 1/gds
 * Complexity: O(1)
 */
static inline double fet_gds_saturation(double beta, double vov,
                                         double lambda, double vds) {
    double id = 0.5 * beta * vov * vov * (1.0 + lambda * vds);
    double gds = lambda * id / (1.0 + lambda * vds);
    return gds;
}

/**
 * Body effect: threshold voltage shift due to source-bulk bias.
 * Vth = Vth0 + γ * (sqrt(|2φf + Vsb|) - sqrt(|2φf|))
 *
 * Reference: Sedra & Smith Eq.(5.30); Body effect coefficient
 * Complexity: O(1)
 */
static inline double fet_vth_body_effect(double vth0, double gamma,
                                           double phi_f, double vsb) {
    double term = sqrt(fabs(2.0 * phi_f + vsb)) - sqrt(fabs(2.0 * phi_f));
    double sign = (vsb >= 0) ? 1.0 : -1.0;
    return vth0 + gamma * sign * term;
}

#endif /* FET_TYPES_H */
