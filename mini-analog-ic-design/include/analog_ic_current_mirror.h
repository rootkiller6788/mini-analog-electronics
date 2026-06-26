/*
 * analog_ic_current_mirror.h - Current Mirror Circuits for Analog IC Design
 *
 * L1: Current mirror topology definitions, matching parameters
 * L2: Output impedance, compliance voltage, systematic offset
 * L3: Small-signal equivalent circuit analysis
 * L4: Mismatch analysis using Pelgrom's model
 * L5: Wilson, cascode, regulated, wide-swing mirror design
 * L6: Current mirror design for op-amp biasing and loads
 *
 * Course: Berkeley EE140 Ch.5, EE240 Ch.3
 *         Stanford EE214 Ch.3
 *         MIT 6.775 Ch.2
 *
 * Textbook: Razavi "Design of Analog CMOS ICs" (2017) Ch.5
 *           Gray & Meyer (2009) Ch.4
 */
#ifndef ANALOG_IC_CURRENT_MIRROR_H
#define ANALOG_IC_CURRENT_MIRROR_H

#include "analog_ic_basis.h"

/* L1: Current Mirror Topology Types */
typedef enum {
    CM_SIMPLE = 0,           /* Simple (basic) current mirror */
    CM_CASCODE = 1,          /* Cascode current mirror */
    CM_WILSON = 2,           /* Wilson current mirror */
    CM_REGULATED = 3,        /* Regulated cascode (gain-boosted) */
    CM_WIDE_SWING = 4,       /* Wide-swing cascode */
    CM_ACTIVE_FEEDBACK = 5,  /* Active-feedback mirror */
    CM_LOW_VOLTAGE = 6       /* Low-voltage cascode (Sooch) */
} current_mirror_type_t;

/* L1: Current Mirror Design Specifications */
typedef struct {
    current_mirror_type_t type;
    mos_type_t device_type;
    double I_ref;               /* Reference (input) current [A] */
    double I_out_target;        /* Target output current [A] */
    double ratio;               /* Desired current ratio Iout/Iref */
    double Vdd;                 /* Supply voltage [V] */
    double Vout_min;            /* Minimum required output voltage [V] */
    double Rout_min;            /* Minimum output resistance [ohm] */
    double matching_target;     /* Target matching accuracy [%] */
    const ic_technology_t *tech;
    double T;                   /* Temperature [K] */
} cm_spec_t;

/* L1: Current Mirror Design Result */
typedef struct {
    /* Device dimensions */
    double W_in, L_in;          /* Input device dimensions [m] */
    double W_out, L_out;        /* Output device dimensions [m] */
    double W_casc, L_casc;      /* Cascode device dimensions [m] */
    int m_in, m_out;            /* Multiplicity */

    /* Performance */
    double I_out;               /* Actual output current [A] */
    double ratio_actual;        /* Actual Iout/Iref ratio */
    double Rout;                /* Output resistance [ohm] */
    double Vout_min_actual;     /* Min output voltage (compliance) [V] */
    double Vdsat_in;            /* Input device Vdsat [V] */
    double Vdsat_out;           /* Output device Vdsat [V] */

    /* Systematic error sources */
    double systematic_error;    /* Systematic current error [%] */
    double random_error_sigma;  /* Random mismatch 1-sigma [%] */
    double Vds_error;           /* Vds mismatch contribution [V] */

    /* Noise */
    double output_noise;        /* Output current noise [A/sqrt(Hz)] at 1kHz */

    /* Mismatch (L4: Pelgrom) */
    mismatch_sample_t mismatch;

    /* Power */
    double power;               /* Power consumption [W] */
    double Vb_casc;             /* Cascode gate bias voltage [V] */
} cm_result_t;

/* L2: Cascode bias generation parameters */
typedef struct {
    double Vb;              /* Cascode gate bias voltage [V] */
    double I_bias;          /* Bias current for cascode biasing [A] */
    double W_bias, L_bias;  /* Bias device dimensions [m] */
} cascode_bias_t;

/* =========================================================================
 * Function Declarations
 * ========================================================================= */

/* L2: Basic current mirror analysis */
int cm_design_simple(const cm_spec_t *spec, cm_result_t *res);
int cm_design_cascode(const cm_spec_t *spec, cm_result_t *res);
int cm_design_wilson(const cm_spec_t *spec, cm_result_t *res);
int cm_design_regulated(const cm_spec_t *spec, cm_result_t *res);
int cm_design_wide_swing(const cm_spec_t *spec, cm_result_t *res);
int cm_design_low_voltage(const cm_spec_t *spec, cm_result_t *res);

/* L3: Output impedance of different mirror topologies */
double cm_output_resistance_simple(double gds, double gm, double gmb);
double cm_output_resistance_cascode(double gm_casc, double gds_casc, double gds_in, double gmb_casc);
double cm_output_resistance_wilson(double gm1, double gds1, double gm2, double gds2, double gds3);
double cm_output_resistance_regulated(double gm_boost, double gm_main, double gds_main, double gds_load);

/* L2: Minimum output voltage (compliance voltage) */
double cm_compliance_simple(double Vdsat);
double cm_compliance_cascode(double Vdsat_in, double Vdsat_casc, double Vth);
double cm_compliance_wide_swing(double Vdsat);
double cm_compliance_low_voltage(double Vdsat_in, double Vdsat_casc);

/* L4: Mismatch analysis — systematic and random current errors */
double cm_mismatch_systematic(double I_ref, double Vds_in, double Vds_out, double lambda);
double cm_mismatch_random_sigma(const mismatch_params_t *mp, double W1, double L1, double W2, double L2, double gm_Id);
double cm_mismatch_total(double systematic, double random_sigma, double confidence);

/* L2: Beta-multiplier for generating cascode bias */
int cm_generate_cascode_bias(const ic_technology_t *tech, double I_bias, double Vdd, double T, cascode_bias_t *bias);

/* L5: Optimal sizing for minimum mismatch */
int cm_optimal_sizing_min_mismatch(const cm_spec_t *spec, double target_sigma, double *W, double *L);

/* L2: Current mirror frequency response */
double cm_bandwidth_simple(double gm, double Cgs, double Cgd, double Cdb);
double cm_current_transfer_error(double Rout, double Rload);

/* L3: BJT current mirror analysis */
double cm_bjt_output_resistance(double VA, double Ic);
double cm_bjt_current_error(double beta_f, double ratio);
double cm_bjt_wilson_error(double beta_f);

/* L7: Power supply rejection of current mirrors */
double cm_psr_simple(double gds, double Rout, double Vdd);
double cm_psr_cascode(double gds_casc, double Rout, double gm_casc, double Vdd);

#endif
