/*
 * analog_ic_basis.h - Analog IC Design: Core Device Models & Biasing
 *
 * L1: Fundamental device parameters and operating point definitions
 * L2: Small-signal model extraction, DC biasing methodology
 * L3: Square-law, subthreshold, velocity-saturation equations
 * L4: MOS square-law, BJT exponential law, Pelgrom's mismatch law
 *
 * Course: Berkeley EE140/EE240 (Analog IC Design)
 *         Stanford EE214 (Advanced Analog IC Design)
 *         MIT 6.775 (CMOS Analog/Mixed-Signal)
 *         ETH 227-0166 (Analog Integrated Circuits)
 *         TU Munich (Analog CMOS Circuit Design)
 *         Tsinghua (Analog IC Design)
 *
 * Textbook: Razavi "Design of Analog CMOS ICs" (2017) Ch.2-4
 *           Gray, Hurst, Lewis, Meyer "Analysis & Design of Analog ICs" (2009) Ch.1-3
 *           Sansen "Analog Design Essentials" (2006) Ch.1-4
 */
#ifndef ANALOG_IC_BASIS_H
#define ANALOG_IC_BASIS_H

#include <stddef.h>
#include <stdint.h>
#include <math.h>

/* =========================================================================
 * L1: Physical Constants for IC Design
 * ========================================================================= */
#define K_BOLTZMANN     1.380649e-23
#define Q_ELECTRON       1.602176634e-19
#define EPS0_SI          8.854187817e-12
#define EPSR_SIO2        3.9
#define EPSR_SI          11.7
#define THERMAL_VT(T)   (K_BOLTZMANN * (T) / Q_ELECTRON)

/* =========================================================================
 * L1: MOS Transistor Type Definitions
 * ========================================================================= */
typedef enum {
    MOS_TYPE_NMOS = 0,
    MOS_TYPE_PMOS = 1
} mos_type_t;

typedef enum {
    MOS_REGION_CUTOFF = 0,
    MOS_REGION_TRIODE = 1,
    MOS_REGION_SATURATION = 2,
    MOS_REGION_SUBTHRESHOLD = 3,
    MOS_REGION_VELOCITY_SAT = 4
} mos_region_t;

/* L1: MOS Process/Technology Parameters ? physical parameters of a CMOS node */
typedef struct {
    double Vth_n;            /* NMOS threshold voltage [V] */
    double Vth_p;            /* PMOS threshold voltage [V] (magnitude) */
    double mu_n;             /* NMOS mobility [cm^2/V?s] */
    double mu_p;             /* PMOS mobility [cm^2/V?s] */
    double tox;              /* Gate oxide thickness [m] */
    double Cox;              /* Gate oxide capacitance per unit area [F/m^2] */
    double L_min;            /* Minimum channel length [m] */
    double W_min;            /* Minimum channel width [m] */
    double lambda_n;         /* NMOS channel length modulation [1/V] */
    double lambda_p;         /* PMOS channel length modulation [1/V] */
    double gamma_n;          /* NMOS body effect coefficient [sqrt(V)] */
    double gamma_p;          /* PMOS body effect coefficient [sqrt(V)] */
    double phi_f_n;          /* NMOS Fermi potential [V] */
    double phi_f_p;          /* PMOS Fermi potential [V] */
    double Vdsat_n;          /* NMOS velocity saturation Vdsat [V] */
    double Vdsat_p;          /* PMOS velocity saturation Vdsat [V] */
    double Avt;              /* Pelgrom coefficient Avt [mV*um] */
    double Abeta;            /* Pelgrom coefficient Abeta [%*um] */
} ic_technology_t;

/* L1: MOS Single Transistor ? geometry + DC operating point */
typedef struct {
    mos_type_t type;
    double W;                /* Channel width [m] */
    double L;                /* Channel length [m] */
    int    m;                /* Multiplicity (parallel fingers) */
    double Vgs;              /* Gate-source voltage [V] */
    double Vds;              /* Drain-source voltage [V] */
    double Vsb;              /* Source-bulk voltage [V] */
    double Id;               /* Drain current [A] */
    mos_region_t region;
} mos_transistor_t;

/* L2: MOS Small-Signal Parameters ? extracted from DC operating point */
typedef struct {
    double gm;               /* Transconductance [S] = dId/dVgs */
    double gds;              /* Output conductance [S] = dId/dVds = 1/ro */
    double gmb;              /* Body transconductance [S] = dId/dVsb */
    double Cgs;              /* Gate-source capacitance [F] */
    double Cgd;              /* Gate-drain capacitance [F] */
    double Cdb;              /* Drain-bulk junction capacitance [F] */
    double Csb;              /* Source-bulk junction capacitance [F] */
    double Cgb;              /* Gate-bulk capacitance [F] */
    double ft;               /* Transition frequency [Hz] */
    double ro;               /* Output resistance [ohm] = 1/gds */
    double intrinsic_gain;   /* gm*ro = gm/gds */
} mos_ss_params_t;

/* L1: BJT Transistor ? geometry + DC operating point (Sedra/Smith Ch.7) */
typedef enum {
    BJT_TYPE_NPN = 0,
    BJT_TYPE_PNP = 1
} bjt_type_t;

typedef enum {
    BJT_REGION_CUTOFF = 0,
    BJT_REGION_FORWARD_ACTIVE = 1,
    BJT_REGION_REVERSE_ACTIVE = 2,
    BJT_REGION_SATURATION = 3
} bjt_region_t;

typedef struct {
    bjt_type_t type;
    double Is;               /* Saturation current [A] */
    double beta_f;           /* Forward current gain */
    double beta_r;           /* Reverse current gain */
    double VA;               /* Early voltage [V] */
    double Vbe;              /* Base-emitter voltage [V] */
    double Vce;              /* Collector-emitter voltage [V] */
    double Ic;               /* Collector current [A] */
    double Ib;               /* Base current [A] */
    bjt_region_t region;
    double T;                /* Temperature [K] */
} bjt_transistor_t;

/* L1: BJT Small-Signal Parameters */
typedef struct {
    double gm;               /* Transconductance [S] = Ic/VT */
    double rpi;              /* Base-emitter resistance [ohm] = beta/gm */
    double ro;               /* Output resistance [ohm] = VA/Ic */
    double rmu;              /* Collector-base resistance [ohm] */
    double Cpi;              /* Base-emitter capacitance [F] */
    double Cmu;              /* Collector-base capacitance [F] */
    double ft;               /* Transition frequency [Hz] */
} bjt_ss_params_t;

/* L1: Biasing topology types */
typedef enum {
    BIAS_TYPE_RESISTOR_DIVIDER = 0,
    BIAS_TYPE_CURRENT_SOURCE  = 1,
    BIAS_TYPE_SELF_BIAS       = 2,
    BIAS_TYPE_BETA_MULTIPLIER = 3,
    BIAS_TYPE_CONSTANT_GM     = 4,
    BIAS_TYPE_WIDLAR          = 5
} bias_type_t;

/* L2: Biasing specification ? target operating point constraints */
typedef struct {
    double Vdd;              /* Supply voltage [V] */
    double Id_target;        /* Target drain current [A] */
    double Vdsat_target;     /* Target overdrive Vgs-Vth [V] */
    double Vds_min;          /* Minimum Vds for saturation [V] */
    double power_budget;     /* Power budget [W] */
} bias_spec_t;

/* L3: gm/ID Methodology Data Point (Silveira-Flandre-Jespers 1996 / Murmann) */
typedef struct {
    double Vgs;              /* Gate-source voltage [V] */
    double gm_over_Id;       /* gm/Id [1/V] ? transconductance efficiency */
    double ft;               /* Transit frequency [Hz] */
    double Vdsat;            /* Saturation voltage [V] */
    double Id_over_W;        /* Normalized current Id/(W/L) [A] */
    double gm_gds;           /* Intrinsic gain [V/V] */
} gmid_data_point_t;

/* L2: Process Corner Enumeration */
typedef enum {
    CORNER_TT = 0,           /* Typical NMOS, Typical PMOS */
    CORNER_FF = 1,           /* Fast NMOS, Fast PMOS */
    CORNER_SS = 2,           /* Slow NMOS, Slow PMOS */
    CORNER_FS = 3,           /* Fast NMOS, Slow PMOS */
    CORNER_SF = 4            /* Slow NMOS, Fast PMOS */
} process_corner_t;

typedef enum {
    TEMP_CORNER_NOMINAL = 0,
    TEMP_CORNER_COLD     = 1,
    TEMP_CORNER_HOT      = 2
} temperature_corner_t;

/* L4: Pelgrom Mismatch Model (Pelgrom et al., JSSC 1989) */
typedef struct {
    double sigma_Vth;        /* Vth mismatch std dev [V] */
    double sigma_beta;       /* beta mismatch std dev [ratio] */
    double Avt;              /* Pelgrom Avt [mV*um] */
    double Abeta;            /* Pelgrom Abeta [%*um] */
} mismatch_params_t;

typedef struct {
    double delta_Vth;        /* Vth mismatch sample [V] */
    double delta_beta;       /* Beta mismatch sample [ratio] */
    double delta_Id;         /* Resulting Id mismatch [A] */
} mismatch_sample_t;

/* =========================================================================
 * Function Declarations ? L1 through L4
 * ========================================================================= */

/* L1: Initialize technology parameters with default values for common nodes */
ic_technology_t *ic_tech_init_180nm(void);
ic_technology_t *ic_tech_init_65nm(void);
ic_technology_t *ic_tech_init_28nm(void);
void ic_tech_free(ic_technology_t *t);

/* L3/L4: MOS Drain Current Equations */
double mos_id_square_law(const ic_technology_t *tech, const mos_transistor_t *m);
double mos_id_velocity_sat(const ic_technology_t *tech, const mos_transistor_t *m);
double mos_id_subthreshold(const ic_technology_t *tech, const mos_transistor_t *m, double T);
double mos_id_unified(const ic_technology_t *tech, const mos_transistor_t *m, double T);

/* L3: Body Effect ? Vth shift due to source-bulk bias */
double mos_vth_body_effect(const ic_technology_t *tech, mos_type_t type,
                           double Vth0, double Vsb);

/* L2: Small-Signal Parameter Extraction (Razavi Ch.2) */
int mos_extract_ss_params(const ic_technology_t *tech, const mos_transistor_t *m,
                          double T, mos_ss_params_t *ss);

/* L2: BJT DC current ? Gummel-Poon simplified forward-active */
double bjt_ic(const bjt_transistor_t *bjt);

/* L2: BJT Small-Signal Parameter Extraction */
int bjt_extract_ss_params(const bjt_transistor_t *bjt, bjt_ss_params_t *ss);

/* L2: Detect MOS operating region from Vgs, Vds, Vth */
mos_region_t mos_detect_region(const ic_technology_t *tech, const mos_transistor_t *m);

/* L2: MOS Sizing ? compute required W/L for target Id and overdrive */
double mos_sizing_wl(const ic_technology_t *tech, mos_type_t type,
                     double Id, double Vov, double L);

/* L2: gm/ID methodology ? compute transconductance efficiency */
double mos_gm_over_id(const ic_technology_t *tech, const mos_transistor_t *m, double T);

/* L4: Beta-multiplier self-biasing reference (Razavi Ch.11) */
double bias_beta_multiplier_current(const ic_technology_t *tech,
                                     double R, double K, double T);

/* L4: Widlar current source (Widlar, JSSC 1965) */
double bias_widlar_current(double Vcc, double R1, double R2,
                            const bjt_transistor_t *bjt_ref);

/* L4: Supply-independent threshold-referenced bias */
double bias_supply_independent(const ic_technology_t *tech,
                                double R, double T);

/* L4: Pelgrom mismatch sampling */
mismatch_sample_t mismatch_pelgrom_sample(const mismatch_params_t *params,
                                           double W, double L, uint64_t seed);
mismatch_params_t mismatch_from_tech(const ic_technology_t *tech);

/* L2: gm/ID lookup table generation */
int gmid_generate_table(const ic_technology_t *tech, mos_type_t type,
                         double T, double L, int n_points,
                         gmid_data_point_t *table);
double gmid_find_vgs(const gmid_data_point_t *table, int n_points,
                      double target_gmid);

/* L2: Process corner scaling */
void ic_tech_apply_corner(ic_technology_t *tech, process_corner_t corner,
                          temperature_corner_t temp);

#endif /* ANALOG_IC_BASIS_H */
