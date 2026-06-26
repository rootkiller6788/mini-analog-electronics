/**
 * demo_bode.c — Bode Plot Generator for Active Filters
 *
 * Demonstrates frequency response analysis by generating magnitude
 * and phase data for a designed filter, suitable for plotting with
 * gnuplot, MATLAB, or Python.
 */

#include "active_filter_defs.h"
#include "active_filter_sallen_key.h"
#include "active_filter_mfb.h"
#include <stdio.h>
#include <math.h>

int main(void) {
    printf("# Active Filter Bode Plot Data\n");
    printf("# Columns: freq(Hz), mag(dB), phase(deg), group_delay(ms)\n");
    printf("# Filter: Sallen-Key Butterworth LP, 1 kHz, Q=0.707\n");

    double wp = 2.0 * M_PI * 1000.0;
    double qp = 0.7071;
    active_component_values_t comp;
    sallen_key_lp_design(wp, qp, 1.586, 10000.0, &comp);

    double num[3], den[3], gain;
    sallen_key_transfer_function(&comp, ACTIVE_FUNC_LOWPASS,
                                  num, den, &gain);

    /* Log sweep from 10 Hz to 100 kHz */
    for (int i = 0; i < 200; i++) {
        double log_f = log10(10.0) + (log10(100000.0) - log10(10.0)) * i / 199.0;
        double f = pow(10.0, log_f);
        double w = 2.0 * M_PI * f;
        double w2 = w * w;

        double denom_real = den[2] - w2;
        double denom_imag = den[1] * w;
        double denom_mag2 = denom_real*denom_real + denom_imag*denom_imag;

        double mag = gain * den[2] / sqrt(denom_mag2);
        double mag_db = 20.0 * log10(mag > 1e-15 ? mag : 1e-15);
        double phase_rad = atan2(-denom_imag, denom_real);
        double phase_deg = phase_rad * 180.0 / M_PI;

        /* Group delay: τ_g = -dφ/dω */
        double dw = w * 1e-5;
        double w_plus = w + dw;
        double denom_real_p = den[2] - w_plus*w_plus;
        double denom_imag_p = den[1] * w_plus;
        double phase_p = atan2(-denom_imag_p, denom_real_p);

        double w_minus = w - dw;
        double denom_real_m = den[2] - w_minus*w_minus;
        double denom_imag_m = den[1] * w_minus;
        double phase_m = atan2(-denom_imag_m, denom_real_m);

        double dphi = phase_p - phase_m;
        while (dphi > M_PI) dphi -= 2.0 * M_PI;
        while (dphi < -M_PI) dphi += 2.0 * M_PI;
        double tau_g = -dphi / (2.0 * dw) * 1000.0;  /* seconds → ms */

        printf("%.3f %.3f %.3f %.6f\n", f, mag_db, phase_deg, tau_g);
    }

    return 0;
}
