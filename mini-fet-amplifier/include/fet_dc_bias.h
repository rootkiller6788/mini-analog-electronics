/**
 * fet_dc_bias.h — FET DC Biasing: Q-Point Analysis and Design
 *
 * Covers L2 Core Concepts: DC bias point establishment,
 * bias stability, load line analysis, bias network topologies.
 *
 * Reference: Sedra & Smith, "Microelectronic Circuits" 8th Ed., Ch.6
 *            Gray, Hurst, Lewis, Meyer "Analysis and Design of Analog IC" 5th Ed., Ch.3
 *
 * MIT 6.002 / Berkeley EE105 / Michigan EECS 411
 */

#ifndef FET_DC_BIAS_H
#define FET_DC_BIAS_H

#include "fet_types.h"
#include <stdbool.h>

/* ──────────────────────────────────────────────
 * L2: Bias Network Configuration Types
 * ────────────────────────────────────────────── */

/** Topologies for establishing DC operating point in FET amplifiers */
typedef enum {
    BIAS_FIXED_GATE = 0,           /* Single resistor to gate, poor stability */
    BIAS_SELF_BIAS = 1,            /* Source degeneration resistor, negative feedback */
    BIAS_VOLTAGE_DIVIDER = 2,      /* R1-R2 divider + source resistor, best discrete bias */
    BIAS_CURRENT_SOURCE = 3,       /* Active current source/sink, IC preferred */
    BIAS_DIODE_CONNECTED = 4,      /* Gate-drain tied (current mirror reference) */
    BIAS_CMOS_COMPLEMENTARY = 5,   /* NMOS+PMOS stacked, self-biased inverter */
    BIAS_BETA_MULTIPLIER = 6,      /* Supply-independent bias generator */
    BIAS_BANDGAP_REFERENCED = 7    /* Temperature-stabilized bias */
} BiasTopology;

/* ──────────────────────────────────────────────
 * L2: DC Load Line Analysis Structures
 * ────────────────────────────────────────────── */

/** DC load line: graphical biasing tool for setting Q-point */
typedef struct {
    double vdd;                /* Supply voltage [V] */
    double rd;                 /* Drain resistor [Ω] (or ∞ for active load) */
    double rs;                 /* Source resistor [Ω] */
    double rg1;                /* Gate divider resistor 1 [Ω] */
    double rg2;                /* Gate divider resistor 2 [Ω] */
    double vgg;                /* Effective Thevenin gate voltage [V] = Vdd * Rg2/(Rg1+Rg2) */
    double rgg;                /* Effective Thevenin gate resistance [Ω] = Rg1||Rg2 */
    double vds_max;            /* Maximum Vds on load line [V] (when Id=0) */
    double id_max;             /* Maximum Id on load line [A] (when Vds=0) */
    double slope;              /* Load line slope = -1/(Rd+Rs) [A/V] */
} DcLoadLine;

/** Bias network component values for discrete design */
typedef struct {
    BiasTopology topology;
    double vdd;                /* Positive supply [V] */
    double vss;                /* Negative supply [V] (or 0 for single supply) */
    double rg1;                /* Gate bias resistor 1 [Ω] */
    double rg2;                /* Gate bias resistor 2 [Ω] */
    double rd;                 /* Drain resistor [Ω] */
    double rs;                 /* Source resistor [Ω] */
    double rl;                 /* Load resistor (AC coupled) [Ω] */
    double rgg;                /* Gate Thevenin equivalent resistance [Ω] = Rg1||Rg2 */
    double vgg;                /* Gate Thevenin equivalent voltage [V] */
    double cc1;                /* Input coupling capacitor [F] */
    double cc2;                /* Output coupling capacitor [F] */
    double cs;                 /* Source bypass capacitor [F] (or 0 if unbypassed) */
    double cg;                 /* Gate bypass capacitor [F] */
    double iref;               /* Reference current for current-source bias [A] */
} BiasNetwork;

/**
 * Design specifications for bias point establishment.
 * The designer specifies a target Q-point; the algorithm computes
 * component values to achieve it.
 */
typedef struct {
    double target_id;          /* Desired drain current [A] */
    double target_vds;         /* Desired drain-source voltage [V] */
    double target_vgs;         /* Desired gate-source voltage [V] */
    double target_vov;         /* Desired overdrive voltage [V] */
    double vdd;                /* Supply voltage [V] */
    double vss;                /* Negative supply [V] or 0 */
    double rin_min;            /* Minimum input resistance [Ω] */
    double rout_target;        /* Target output resistance [Ω] */
    double power_max;          /* Maximum DC power [W] */
    double swing_min;          /* Minimum output swing [V] peak-to-peak */
    bool source_degenerated;   /* Use source degeneration */
    double rs_degen;           /* Source degeneration resistance [Ω] (0 = full bypass) */
} BiasSpec;

/** Result of bias point calculation */
typedef struct {
    FetDcOpPoint qpoint;       /* Computed operating point */
    DcLoadLine load_line;      /* Load line analysis */
    BiasNetwork network;       /* Component values determined */
    double power_dc;           /* DC power dissipation [W] */
    double vout_swing;         /* Available output voltage swing [Vp-p] */
    double sensitivity_id;     /* ∂Id/∂Vth (bias point sensitivity to Vth variation) */
    double tempco_id;          /* Temperature coefficient of Id [A/K] */
    bool converged;            /* Solution convergence flag */
    uint32_t iterations;       /* Newton iteration count */
} BiasResult;

/* ──────────────────────────────────────────────
 * L2: Bias Stability and Temperature Effects
 * ────────────────────────────────────────────── */

/**
 * FET temperature effects on threshold voltage and mobility.
 * Vth(T) = Vth(T0) - α_Vth * (T - T0)
 * μ(T) = μ(T0) * (T/T0)^(-α_μ)
 *
 * Typical: α_Vth ≈ 2 mV/K, α_μ ≈ 1.5 for Si
 */
typedef struct {
    double alpha_vth;          /* Vth temperature coefficient [V/K] */
    double alpha_mu;           /* Mobility temperature exponent */
    double t_nominal;          /* Nominal temperature [K] */
    double vth_nominal;        /* Vth at nominal temperature [V] */
    double mu_nominal;         /* Mobility at nominal temperature [cm^2/V·s] */
} FetTempModel;

/* ──────────────────────────────────────────────
 * L2: Bias Design Functions
 * ────────────────────────────────────────────── */

/**
 * Compute the DC load line for a given supply and resistances.
 * The load line equation: Vds = Vdd - Id*(Rd + Rs)
 * For active loads (current source drain): Rd effectively infinite.
 *
 * Reference: Sedra & Smith §6.3.1 "Biasing by Fixing VGS"
 * Complexity: O(1)
 */
DcLoadLine fet_compute_load_line(double vdd, double vss, double rd, double rs,
                                  double rg1, double rg2);

/**
 * Calculate voltage divider Thevenin equivalent for gate bias.
 * Vgg = Vdd * Rg2/(Rg1+Rg2), Rgg = Rg1||Rg2
 *
 * Complexity: O(1)
 */
void fet_gate_equivalent(double vdd, double rg1, double rg2,
                         double *vgg, double *rgg);

/**
 * Fixed-gate bias Q-point calculation.
 * Vgs = Vgg (gate voltage fixed by divider)
 * Solve Id = (1/2)*β*(Vgg - Id*Rs - Vth)^2 for Id.
 * Uses Newton-Raphson iteration.
 *
 * Complexity: typically O(log(1/ε)) ~ 5-10 iterations
 */
BiasResult fet_bias_fixed_gate(const FetDevice *fet, const BiasSpec *spec);

/**
 * Self-bias (source degeneration) Q-point calculation.
 * Gate tied to ground through Rg; source resistor provides negative feedback.
 * Vgs = -Id*Rs for depletion-mode or JFET (Vgs negative for N-channel).
 *
 * Reference: Sedra & Smith §6.3.4 "Biasing Using a Drain-to-Gate Feedback Resistor"
 * Complexity: O(log(1/ε)) Newton iterations
 */
BiasResult fet_bias_self_bias(const FetDevice *fet, const BiasSpec *spec);

/**
 * Voltage-divider bias — the standard discrete FET biasing method.
 * Gate voltage set by R1-R2 divider; source resistor provides
 * negative feedback for thermal stability.
 *
 * Reference: Sedra & Smith §6.3.3 "Biasing Using a Constant-Current Source"
 * Complexity: O(log(1/ε)) Newton iterations
 */
BiasResult fet_bias_voltage_divider(const FetDevice *fet, const BiasSpec *spec);

/**
 * Active current-source bias — preferred for IC design.
 * Id set by current mirror, independent of Vth and supply variations.
 * Provides best bias stability across process and temperature.
 *
 * Complexity: O(1) (current set by reference)
 */
BiasResult fet_bias_current_source(const FetDevice *fet, double iref,
                                    double vdd, double vss);

/**
 * Iteratively solve for Q-point given a bias network.
 * Uses Newton-Raphson on the nodal equations.
 *
 * @param fet    Device model
 * @param nw     Bias network component values
 * @param qpoint Output: solved operating point
 * @return convergence success
 *
 * Complexity: O(N_iter) with N_iter ~ 5-15 for typical tolerance
 */
bool fet_solve_qpoint(const FetDevice *fet, const BiasNetwork *nw,
                      FetDcOpPoint *qpoint);

/**
 * Compute bias point sensitivity to Vth variation.
 * S_{Id}^{Vth} = (∂Id/∂Vth) * (Vth/Id) — dimensionless sensitivity.
 *
 * Important for yield analysis and robust design.
 * Complexity: O(1)
 */
double fet_bias_sensitivity_vth(const FetDevice *fet, const BiasNetwork *nw,
                                 const FetDcOpPoint *qp);

/**
 * Compute temperature drift of drain current at the bias point.
 * dId/dT considering Vth(T) and μ(T) variations.
 *
 * Complexity: O(1)
 */
double fet_bias_temperature_drift(const FetDevice *fet,
                                   const FetTempModel *tmodel,
                                   const FetDcOpPoint *qp);

/**
 * Validate bias point: check that the FET is in saturation and
 * within safe operating area (SOA): Vds < BV_dss, Id < Id_max, P < P_max.
 *
 * Complexity: O(1)
 */
bool fet_bias_validate(const FetDcOpPoint *qp, double vdd, double pmax,
                       double bv_dss, double id_max);

/**
 * Design bias resistors to achieve a target Q-point with specified
 * stability criteria (source degeneration ratio Rs/Rd).
 *
 * @param fet    Device model
 * @param spec   Target specifications
 * @param result Output: computed bias network and Q-point
 * @return true if feasible solution found
 *
 * Complexity: O(log(1/ε)) iterative solve
 */
bool fet_bias_design(const FetDevice *fet, const BiasSpec *spec,
                     BiasResult *result);

#endif /* FET_DC_BIAS_H */
