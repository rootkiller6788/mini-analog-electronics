/**
 * example_schmitt_trigger.c - Schmitt Trigger and Waveform Generator Example
 *
 * Demonstrates the design and analysis of nonlinear op-amp circuits:
 *   1. Schmitt trigger with hysteresis design
 *   2. Precision full-wave rectifier simulation
 *   3. Triangle/square wave generator design
 *   4. Input signal processing with noise immunity
 */

#include "opamp_nonlinear.h"
#include "opamp_basic.h"
#include <stdio.h>
#include <math.h>

int main(void) {
    printf("============================================================\n");
    printf("  Schmitt Trigger & Waveform Generator Example\n");
    printf("============================================================\n\n");

    /* ===========================================================
     * PART 1: SCHMITT TRIGGER DESIGN
     * =========================================================== */
    printf("--- Part 1: Schmitt Trigger Design ---\n\n");

    ComparatorConfig st;
    st.V_OH = 13.0;    /* Output high (V) */
    st.V_OL = -13.0;   /* Output low (V) */
    st.V_ref = 0.0;    /* Reference voltage: 0V (symmetric) */

    /* Design for hysteresis band of 2V centered at 0V */
    double V_TH_desired = 1.0;
    double V_TL_desired = -1.0;

    comparator_design_hysteresis(&st, V_TH_desired, V_TL_desired);

    printf("Schmitt Trigger with 2V hysteresis band:\n");
    printf("  V_OH = %.1f V, V_OL = %.1f V\n", st.V_OH, st.V_OL);
    printf("  V_TH = %.3f V (upper threshold)\n", st.V_TH);
    printf("  V_TL = %.3f V (lower threshold)\n", st.V_TL);
    printf("  Hysteresis V_H = %.3f V\n", schmitt_hysteresis_band(&st));
    printf("  R1 = %.1f kOhm, R2 = %.1f kOhm\n\n", st.R1 * 1e-3, st.R2 * 1e-3);

    /* Simulate Schmitt trigger with a noisy input */
    printf("Response to noisy 1 Hz sine wave (1.5V amplitude + 0.3V noise):\n");
    printf("  Time(s)   V_in(V)   V_out(V)\n");
    printf("  -------   -------   --------\n");

    double prev_output = st.V_OL;
    double f_sig = 1.0;  /* 1 Hz signal */

    for (int i = 0; i <= 20; i++) {
        double t = i * 0.05;  /* 50 ms samples */
        double v_sig = 1.5 * sin(2.0 * M_PI * f_sig * t);
        /* Add simulated noise */
        double v_noise = 0.3 * sin(2.0 * M_PI * 17.3 * t);  /* Noise at 17.3 Hz */
        double v_in = v_sig + v_noise;

        double v_out = schmitt_trigger_output(&st, v_in, prev_output);
        printf("  %7.3f   %7.3f   %7.1f\n", t, v_in, v_out);
        prev_output = v_out;
    }

    /* Noise margin check */
    double V_noise_rms = 0.3 / sqrt(2.0);  /* RMS of 0.3V amplitude noise */
    int noise_ok = schmitt_noise_margin(&st, V_noise_rms);
    printf("\nNoise margin check: hysteresis = %.3f V, noise RMS = %.3f V\n",
           schmitt_hysteresis_band(&st), V_noise_rms);
    printf("  %s (need hysteresis >= 5*noise_RMS = %.3f V)\n\n",
           noise_ok ? "ADEQUATE" : "INADEQUATE", 5.0 * V_noise_rms);

    /* ===========================================================
     * PART 2: PRECISION RECTIFIER
     * =========================================================== */
    printf("--- Part 2: Precision Full-Wave Rectifier ---\n\n");

    printf("Precision full-wave rectifier (ideal diode in feedback loop):\n");
    printf("  Input (V)    HW Out (V)   FW Out (V) = |Vin|\n");
    printf("  ---------    ----------   ------------------\n");

    for (int i = -10; i <= 10; i++) {
        double v_in = i * 0.2;  /* -2V to +2V */
        double hw = precision_hw_rectifier(v_in, 10000.0, 10000.0, 0.7);
        double fw = precision_fw_rectifier(v_in, 10000.0, 10000.0, 10000.0, 0.7);
        printf("  %7.2f      %7.3f      %7.3f\n", v_in, hw, fw);
    }

    /* ===========================================================
     * PART 3: WAVEFORM GENERATOR
     * =========================================================== */
    printf("\n--- Part 3: Triangle/Square Wave Generator ---\n\n");

    WaveformGenConfig wfg;
    wfg.V_sat = 12.0;  /* Op-amp saturation */

    double target_f = 1000.0;     /* 1 kHz */
    double target_Vpp = 8.0;      /* 8V peak-to-peak triangle */

    waveform_gen_design(&wfg, target_f, target_Vpp);

    printf("Triangle/Square Wave Generator Design:\n");
    printf("  Target frequency: %.0f Hz\n", target_f);
    printf("  Target amplitude: %.1f Vpp\n", target_Vpp);
    printf("  R_int = %.1f kOhm\n", wfg.R_int * 1e-3);
    printf("  C_int = %.1f nF\n", wfg.C_int * 1e9);
    printf("  R1_st = %.1f kOhm\n", wfg.R1_st * 1e-3);
    printf("  R2_st = %.1f kOhm\n", wfg.R2_st * 1e-3);
    printf("  Actual frequency: %.1f Hz\n\n", wfg.freq);

    printf("Waveform samples (one period):\n");
    printf("  Time (us)    V_tri (V)    V_sq (V)\n");
    printf("  ---------    ---------    --------\n");

    double period = 1.0 / wfg.freq;
    for (int i = 0; i <= 10; i++) {
        double t = i * period / 10.0;
        double v_tri, v_sq;
        waveform_gen_triangle_sample(&wfg, t, &v_tri, &v_sq);
        printf("  %7.1f     %7.2f     %7.1f\n", t * 1e6, v_tri, v_sq);
    }

    /* ===========================================================
     * PART 4: PEAK DETECTOR
     * =========================================================== */
    printf("\n--- Part 4: Peak Detector ---\n\n");

    printf("Peak detector response to decaying sine wave:\n");
    printf("  Sample    V_in(V)    V_peak(V)\n");
    printf("  ------    -------    ---------\n");

    double prev_peak = 0.0;
    double C_hold = 1.0e-6;    /* 1 uF hold capacitor */
    double R_discharge = 1.0e6; /* 1 MOhm discharge (1s time constant) */
    double dt = 0.01;           /* 10 ms sample interval */

    for (int i = 0; i < 15; i++) {
        double t = i * dt;
        double v_in = 5.0 * exp(-t * 2.0) * fabs(sin(2.0 * M_PI * 10.0 * t));
        double v_peak = peak_detector_output(v_in, C_hold, R_discharge, dt, &prev_peak);
        printf("  %4d      %7.3f     %7.3f\n", i, v_in, v_peak);
    }

    printf("\n============================================================\n");
    printf("  Schmitt Trigger & Waveform Generator Example Complete\n");
    printf("============================================================\n");

    return 0;
}
