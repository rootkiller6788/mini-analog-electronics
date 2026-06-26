/*
 * analog_ic_bandgap.h - Bandgap Voltage Reference Design
 *
 * L1: Bandgap voltage definition (1.2V silicon bandgap)
 * L2: PTAT (Proportional To Absolute Temperature) and CTAT (Complementary)
 * L3: Curvature correction, higher-order temperature compensation
 * L4: Widlar bandgap, Brokaw bandgap, Kuijk bandgap ！ fundamental topologies
 * L5: Trimming and TC optimization algorithms
 * L6: Complete bandgap reference design for ADC/DAC/LDO applications
 * L7: Sub-1V bandgap, low-power bandgap for IoT (e.g., ESP32, nRF52)
 * L8: Curvature-compensated bandgap, subthreshold bandgap
 *
 * Course: Berkeley EE240 Ch.11
 *         Stanford EE214 Ch.9 (Voltage References)
 *         MIT 6.775 Ch.8
 *
 * Textbook: Razavi "Design of Analog CMOS ICs" (2017) Ch.11
 *           Gray & Meyer (2009) Ch.12
 *           Rincon-Mora "Voltage References" (2002)
 */
#ifndef ANALOG_IC_BANDGAP_H
#define ANALOG_IC_BANDGAP_H

#include "analog_ic_basis.h"

/* L1: Bandgap Topology Types */
typedef enum {
    BGR_WIDLAR = 0,         /* Widlar bandgap (1971) ！ first practical BGR */
    BGR_BROKAW = 1,         /* Brokaw bandgap (1974) ！ industry standard */
    BGR_KUIJK = 2,          /* Kuijk bandgap (1973) ！ CMOS compatible */
    BGR_BANBA = 3,          /* Banba bandgap (1999) ！ sub-1V */
    BGR_SUBTHRESHOLD = 4,   /* Subthreshold CMOS bandgap */
    BGR_CURVATURE_CORRECTED = 5  /* Second-order curvature corrected */
} bandgap_type_t;

/* L1: Bandgap Reference Specifications */
typedef struct {
    bandgap_type_t topology;
    double Vdd;                 /* Supply voltage [V] */
    double Vdd_min;             /* Minimum supply for regulation [V] */
    double Vref_target;         /* Target reference voltage [V] (~1.2V for Si bandgap) */
    double TC_target;           /* Target temperature coefficient [ppm/C] */
    double power_target;        /* Power budget [W] */
    double PSRR_target;         /* PSRR at DC [dB] */
    double noise_target;        /* Output noise at 1kHz [V/sqrt(Hz)] */
    double startup_time_target; /* Startup time [s] */
    double T_min, T_max;        /* Temperature range [K] */
    const ic_technology_t *tech;
} bandgap_spec_t;

/* L1: Bandgap Design Result */
typedef struct {
    /* DC characteristics */
    double Vref;                /* Output reference voltage [V] */
    double Vref_T_nominal;      /* Vref at nominal temp (300K) [V] */
    double Vref_T_min;          /* Vref at T_min [V] */
    double Vref_T_max;          /* Vref at T_max [V] */

    /* Temperature coefficient (box method) */
    double TC;                  /* Temperature coefficient [ppm/C] */
    double dVref_dT;            /* Slope [V/K] */

    /* Curvature */
    double curvature_error;     /* Residual curvature error [V] */
    double peak_to_peak_error;  /* Vref p-p variation over temperature [V] */

    /* PTAT & CTAT components */
    double V_ptat;              /* PTAT voltage component [V] */
    double V_ctat;              /* CTAT voltage component [V] */
    double dVptat_dT;           /* PTAT slope [V/K] */
    double dVctat_dT;           /* CTAT slope [V/K] */
    double gain_ptat;           /* PTAT gain (K) */
    double gain_ctat;           /* CTAT gain */

    /* Component values */
    double R1, R2, R3;          /* Resistor values [ohm] */
    double ratio_n;             /* BJT emitter area ratio */
    double I_bias;              /* Bias current [A] */

    /* Device dimensions */
    double W_pmos, L_pmos;      /* PMOS current source dimensions [m] */
    double W_opamp;             /* Op-amp input pair width [m] */

    /* Performance */
    double PSRR;                /* Power supply rejection ratio [dB] */
    double output_noise;        /* Output noise at 1kHz [V/sqrt(Hz)] */
    double line_reg;            /* Line regulation [mV/V] */
    double load_reg;            /* Load regulation [mV/mA] */
    double power;               /* Power consumption [W] */
    double startup_time;        /* Estimated startup time [s] */

    /* L5: Trim */
    double trim_range;          /* Available trim range [V] */
    int    trim_steps;          /* Number of trim steps */
    double trim_resolution;     /* Trim resolution [mV/step] */
} bandgap_result_t;

/* L2: PTAT voltage generator parameters */
typedef struct {
    double I_ptat;              /* PTAT current [A] */
    double V_ptat;              /* PTAT voltage [V] */
    double R_ptat;              /* PTAT resistance [ohm] */
    double ratio_n;             /* BJT area ratio */
    double T;                   /* Temperature [K] */
} ptat_params_t;

/* L2: Vbe (CTAT) temperature characteristics */
typedef struct {
    double Vbe_nominal;         /* Vbe at T0 = 300K [V] */
    double Vgo;                 /* Bandgap extrapolated to 0K [V] (1.206V for Si) */
    double eta;                 /* Temperature exponent (~4 - n) */
    double T0;                  /* Reference temperature [K] (300K) */
} vbe_temp_params_t;

/* =========================================================================
 * Function Declarations
 * ========================================================================= */

/* L2: PTAT voltage generation (delta Vbe = kT/q * ln(n)) */
double bgr_ptat_voltage(double T, double ratio_n);
double bgr_ptat_current(double T, double R, double ratio_n);

/* L2: CTAT voltage (Vbe) as function of temperature */
double bgr_vbe_temperature(double T, const vbe_temp_params_t *params);
double bgr_vbe_dT(const vbe_temp_params_t *params, double T);

/* L4: Bandgap voltage ！ combine PTAT + CTAT */
double bgr_combine_vref(double V_ptat_gain, double T, double vbe_params[], double T0);

/* L6: Complete bandgap design */
int bgr_design_widlar(const bandgap_spec_t *spec, bandgap_result_t *res);
int bgr_design_brokaw(const bandgap_spec_t *spec, bandgap_result_t *res);
int bgr_design_kuijk(const bandgap_spec_t *spec, bandgap_result_t *res);
int bgr_design_banba(const bandgap_spec_t *spec, bandgap_result_t *res);

/* L3: Calculate temperature coefficient (box method) */
double bgr_calculate_TC(double Vmax, double Vmin, double Vnom, double T_min, double T_max);

/* L5: Optimal PTAT gain for first-order compensation */
double bgr_optimal_ptat_gain(double Vgo, double eta, double T_nominal);
double bgr_ptat_correction_factor(double Vgo, double eta, double T);

/* L5: Trim calculation */
int bgr_calculate_trim(const bandgap_result_t *res, double target_Vref,
                        double trim_step, double *trim_code, double *trimmed_Vref);

/* L2: Line regulation estimation */
double bgr_line_regulation(double PSRR, double Vdd);

/* L2: Startup time estimation */
double bgr_startup_time(double C_load, double I_startup, double Vref_target);

/* L3: Curvature error estimation (second-order) */
double bgr_curvature_error(double T_min, double T_max, double eta, double T_nominal);

/* L8: Curvature-compensated bandgap with second-order correction */
int bgr_design_curvature_corrected(const bandgap_spec_t *spec, bandgap_result_t *res);

/* L3: PSRR estimation based on op-amp gain */
double bgr_psrr_estimate(double A_opamp, double gm_pmos, double ro_pmos, double R_load);

/* L1: Nominal Vbe parameter extraction from technology */
vbe_temp_params_t bgr_vbe_params_default(double Vbe25, double Ic);

/* L7: Sub-1V bandgap using resistor division */
double bgr_sub1v_resistor_divider(double Vref, double R1, double R2);

#endif
