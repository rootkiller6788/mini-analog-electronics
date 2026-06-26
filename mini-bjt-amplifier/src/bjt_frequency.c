/**
 * bjt_frequency.c ? BJT Frequency Response Implementation
 *
 * Implements low-frequency (coupling/bypass capacitor), high-frequency
 * (Miller effect, OCTC), and transfer-function analysis for BJT amplifiers.
 *
 * The frequency response of a BJT amplifier is band-pass:
 *   Low end: determined by coupling/bypass capacitors (high-pass poles)
 *   Mid-band: constant gain (all caps = short/open as appropriate)
 *   High end: determined by transistor internal capacitances (low-pass poles)
 *
 * Reference: Sedra & Smith, Ch. 10; Gray & Meyer, Ch. 7
 *            Bode, "Network Analysis and Feedback Amplifier Design" (1945)
 */

#include "bjt_frequency.h"
#include <math.h>
#include <stdio.h>

/* ================================================================
 * Low-Frequency Cut-off Analysis (Coupling / Bypass Capacitors)
 * ================================================================ */

double bjt_freq_fL_input_coupling(double C_in, double Rs, double Rin)
{
    /* Low-frequency pole due to input coupling capacitor.
     *
     * f_L_in = 1 / (2*pi * C_in * (Rs + Rin))
     *
     * The input coupling capacitor C_in and the total input resistance
     * (source Rs + amplifier Rin) form a high-pass filter.
     *
     * Example: C_in = 10 uF, Rs = 600 ?, Rin = 10 k?
     *   f_L_in = 1/(2*pi*1e-5*10600) ? 1.5 Hz  (good for audio) */
    if (C_in <= 0.0 || (Rs + Rin) <= 0.0) return 0.0;
    return 1.0 / (2.0 * M_PI * C_in * (Rs + Rin));
}

double bjt_freq_fL_output_coupling(double C_out, double Rout, double RL)
{
    /* Low-frequency pole due to output coupling capacitor.
     *
     * f_L_out = 1 / (2*pi * C_out * (Rout + RL))
     *
     * The output coupling capacitor and the total output resistance
     * (amplifier Rout + load RL) form a high-pass filter. */
    if (C_out <= 0.0 || (Rout + RL) <= 0.0) return 0.0;
    return 1.0 / (2.0 * M_PI * C_out * (Rout + RL));
}

double bjt_freq_fL_emitter_bypass(double C_E, double RE,
                                   double re, double Rs, double beta)
{
    /* Low-frequency pole due to emitter bypass capacitor.
     *
     * The bypass capacitor C_E is in parallel with RE. The resistance
     * seen from C_E's terminals is:
     *   R_CE = RE || (re + Rs/(beta+1))
     *
     * So: f_L_E = 1 / (2*pi * C_E * R_CE)
     *
     * Typically, this is the dominant low-frequency pole because
     * R_CE is small (e.g., 25-100 ?), requiring a large C_E
     * (e.g., 100 uF) for low f_L. */

    double r_ce, reb;

    if (C_E <= 0.0) return 0.0;

    /* Resistance seen looking back from C_E:
     * From the emitter, looking back through re to the base.
     * The source resistance Rs is divided by (beta+1) when
     * reflected to the emitter side. */
    reb = re + Rs / (beta + 1.0);

    if (RE <= 0.0) {
        r_ce = reb;
    } else {
        r_ce = (RE * reb) / (RE + reb);
    }

    if (r_ce <= 0.0) return 0.0;
    return 1.0 / (2.0 * M_PI * C_E * r_ce);
}

double bjt_freq_fL_total(const double *fL_individual, int n)
{
    /* Total low cut-off frequency from multiple high-pass poles.
     *
     * For n independent high-pass stages (coupling/bypass capacitors),
     * the overall -3dB frequency has contributions from each pole.
     *
     * Approximate formula (sum-of-squares, good for separated poles):
     *   f_L_total ? sqrt( sum_i (f_L_i)^2 )
     *
     * For exact solution, we'd solve for f where:
     *   prod_i |f_L_i / sqrt(f^2 + f_L_i^2)| = 1/sqrt(2)
     *
     * But the RMS approximation is accurate to ~15% for typical designs. */

    double sum_sq = 0.0;
    int i;

    if (!fL_individual || n <= 0) return 0.0;

    for (i = 0; i < n; i++) {
        sum_sq += fL_individual[i] * fL_individual[i];
    }

    return sqrt(sum_sq);
}

/* ================================================================
 * High-Frequency Analysis
 * ================================================================ */

double bjt_freq_fH_input_pole(const bjt_ss_params_t *ss, double Rs)
{
    /* High-frequency pole due to Cpi (input capacitance).
     *
     * f_H_pi = 1 / (2*pi * Cpi * (rpi || Rs))
     *
     * This is the bandwidth due to the input capacitance alone
     * (typically NOT dominant ? the Miller-multiplied Cmu dominates). */

    double r_par;

    if (!ss || ss->Cpi <= 0.0) return 1e12;

    if (ss->rpi > 0.0 && Rs > 0.0) {
        r_par = (ss->rpi * Rs) / (ss->rpi + Rs);
    } else if (ss->rpi > 0.0) {
        r_par = ss->rpi;
    } else {
        r_par = Rs;
    }

    if (r_par <= 0.0) return 1e12;
    return 1.0 / (2.0 * M_PI * ss->Cpi * r_par);
}

double bjt_freq_fH_miller_input(const bjt_ss_params_t *ss,
                                 double Rs, double Av_magnitude)
{
    /* High-frequency pole due to Miller-multiplied Cmu at the input.
     *
     * C_miller_in = Cmu * (1 + |Av|)
     * f_H_miller = 1 / (2*pi * C_miller_in * (rpi || Rs))
     *
     * This is typically the DOMINANT high-frequency pole in CE amplifiers.
     *
     * Example: Cmu = 2 pF, |Av| = 100, Rs = 50 ?, rpi = 2.6 k?
     *   C_miller = 2e-12 * 101 = 202 pF
     *   R_par = 50 || 2600 ? 49 ?
     *   f_H = 1/(2*pi*202e-12*49) ? 16 MHz
     *
     * Without Miller effect: f_H would be ~800 MHz.
     * This is why cascode (eliminating Miller) is critical for wideband. */

    double cm, r_par;

    if (!ss || ss->Cmu <= 0.0) return 1e12;

    /* Miller-multiplied capacitance at input */
    cm = ss->Cmu * (1.0 + Av_magnitude);

    /* Resistance seen by C_miller_in */
    if (ss->rpi > 0.0 && Rs > 0.0) {
        r_par = (ss->rpi * Rs) / (ss->rpi + Rs);
    } else if (ss->rpi > 0.0) {
        r_par = ss->rpi;
    } else {
        r_par = Rs;
    }

    if (r_par <= 0.0 || cm <= 0.0) return 1e12;
    return 1.0 / (2.0 * M_PI * cm * r_par);
}

double bjt_freq_fH_miller_output(const bjt_ss_params_t *ss,
                                  double RC, double RL)
{
    /* High-frequency pole due to Cmu at the output side.
     *
     * C_miller_out = Cmu * (1 + 1/|Av|) ? Cmu (for large |Av|)
     * f_H_mu_out = 1 / (2*pi * Cmu * (RC || RL))
     *
     * This pole is typically much higher than the input Miller pole
     * and is rarely dominant. */

    double r_par;

    if (!ss || ss->Cmu <= 0.0) return 1e12;

    /* Output resistance */
    if (RC > 0.0 && RL > 0.0) {
        r_par = (RC * RL) / (RC + RL);
    } else if (RC > 0.0) {
        r_par = RC;
    } else {
        r_par = RL;
    }

    if (r_par <= 0.0) return 1e12;
    return 1.0 / (2.0 * M_PI * ss->Cmu * r_par);
}

/* ================================================================
 * Open-Circuit Time Constants (OCTC) Method
 * ================================================================ */

double bjt_freq_fH_octc(const double *r_i0, const double *c_i, int n_caps)
{
    /* OCTC method for estimating f_H.
     *
     * f_H ? 1 / (2*pi * sum_i (R_i0 * C_i))
     *
     * where R_i0 is the resistance seen by capacitor C_i with all
     * other capacitors treated as open circuits (since at high
     * frequencies, capacitors approach short circuits ? looking at
     * each capacitor independently with others open gives good approx).
     *
     * The OCTC method is remarkably accurate (typically 10-20%)
     * and is a standard technique in analog IC design.
     *
     * Reference: Gray, Hurst, Lewis & Meyer, "Analysis and Design
     *   of Analog Integrated Circuits", 5th ed., Sec. 7.3. */

    double sum_tau = 0.0;
    int i;

    if (!r_i0 || !c_i || n_caps <= 0) return 1e12;

    for (i = 0; i < n_caps; i++) {
        sum_tau += r_i0[i] * c_i[i];
    }

    if (sum_tau <= 0.0) return 1e12;
    return 1.0 / (2.0 * M_PI * sum_tau);
}

/* ================================================================
 * Short-Circuit Time Constants (SCTC) Method
 * ================================================================ */

double bjt_freq_fL_sctc(const double *r_is, const double *c_i, int n_caps)
{
    /* SCTC method for estimating f_L.
     *
     * f_L ? (1 / (2*pi)) * sum_i (1 / (R_is * C_i))
     *
     * where R_is is the resistance seen by capacitor C_i with all
     * other capacitors treated as short circuits (since at low
     * frequencies, capacitors approach open circuits ? looking at
     * each individually with others shorted gives good approx).
     *
     * This is the dual of OCTC for low-frequency estimation.
     *
     * Reference: Gray & Meyer, Sec. 7.3. */

    double sum_inv_tau = 0.0;
    int i;

    if (!r_is || !c_i || n_caps <= 0) return 0.0;

    for (i = 0; i < n_caps; i++) {
        if (r_is[i] > 0.0 && c_i[i] > 0.0) {
            sum_inv_tau += 1.0 / (r_is[i] * c_i[i]);
        }
    }

    if (sum_inv_tau <= 0.0) return 0.0;
    return sum_inv_tau / (2.0 * M_PI);
}

/* ================================================================
 * Gain-Bandwidth Analysis
 * ================================================================ */

double bjt_freq_gbw_product(double Av_mid_magnitude, double f_H)
{
    /* Gain-Bandwidth Product (GBW or GBWP).
     *
     * For a dominant-pole amplifier:
     *   GBW = |Av_mid| * f_H
     *
     * GBW is approximately constant for a given transistor/biasing,
     * representing the fundamental gain-bandwidth trade-off.
     *
     * For a BJT with fT = 300 MHz:
     *   Gain of 10 => BW ? 30 MHz
     *   Gain of 100 => BW ? 3 MHz
     *
     * This constancy arises because the dominant pole comes from
     * Miller-multiplied Cmu, and both Av and the Miller factor
     * scale together. */

    return Av_mid_magnitude * f_H;
}

double bjt_freq_bandwidth_with_feedback(double BW_open_loop,
                                         double loop_gain_T)
{
    /* Bandwidth extension with negative feedback.
     *
     * Negative feedback extends bandwidth at the cost of gain:
     *   Av_closed = Av_open / (1 + T)
     *   BW_closed = BW_open * (1 + T)
     *   GBW_closed = GBW_open  (conserved)
     *
     * where T = loop gain = beta*Av_open for series-shunt feedback.
     *
     * This is one of the most powerful results in feedback amplifier
     * theory ? you can trade gain for bandwidth at constant GBW. */

    if (loop_gain_T <= -1.0) return BW_open_loop;  /* Avoid negative BW */
    return BW_open_loop * (1.0 + loop_gain_T);
}

/* ================================================================
 * Transfer Function Evaluation
 * ================================================================ */

double bjt_tf_magnitude(const bjt_transfer_function_t *tf, double frequency_hz)
{
    /* Evaluate |H(jw)| at frequency f.
     *
     * |H(jw)| = |A0| * prod_i |1 + jw/w_zi| / prod_i |1 + jw/w_pi|
     *
     * where w = 2*pi*f.
     *
     * Each pole factor contributes |1 + jw/wp| = sqrt(1 + (w/wp)^2).
     * Each zero factor contributes |1 + jw/wz| = sqrt(1 + (w/wz)^2). */

    double w, mag;
    int i;

    if (!tf) return 0.0;

    w = 2.0 * M_PI * frequency_hz;
    mag = fabs(tf->A0);

    /* Numerator: zeros */
    for (i = 0; i < tf->n_zeros; i++) {
        if (tf->zeros_rad_per_s[i] > 0.0) {
            double ratio = w / tf->zeros_rad_per_s[i];
            mag *= sqrt(1.0 + ratio * ratio);
        }
    }

    /* Denominator: poles */
    for (i = 0; i < tf->n_poles; i++) {
        if (tf->poles_rad_per_s[i] > 0.0) {
            double ratio = w / tf->poles_rad_per_s[i];
            mag /= sqrt(1.0 + ratio * ratio);
        }
    }

    return mag;
}

double bjt_tf_phase(const bjt_transfer_function_t *tf, double frequency_hz)
{
    /* Evaluate the phase of H(jw).
     *
     * angle(H(jw)) = sum_i atan(w/w_zi) - sum_i atan(w/w_pi)
     *
     * Plus pi if A0 is negative (as in CE amplifier phase inversion). */

    double w, phase;
    int i;

    if (!tf) return 0.0;

    w = 2.0 * M_PI * frequency_hz;
    phase = 0.0;

    /* Zeros contribute positive phase */
    for (i = 0; i < tf->n_zeros; i++) {
        if (tf->zeros_rad_per_s[i] > 0.0) {
            phase += atan(w / tf->zeros_rad_per_s[i]);
        }
    }

    /* Poles contribute negative phase */
    for (i = 0; i < tf->n_poles; i++) {
        if (tf->poles_rad_per_s[i] > 0.0) {
            phase -= atan(w / tf->poles_rad_per_s[i]);
        }
    }

    /* CE inversion: A0 < 0 adds pi (180?) */
    if (tf->A0 < 0.0) {
        phase += M_PI;
    }

    return phase;
}

void bjt_tf_ce_build(double Av_mid, double fL_in, double fL_out,
                      double fL_bypass, double fH_miller, double fH_out,
                      bjt_transfer_function_t *tf)
{
    /* Build a transfer function for a CE amplifier.
     *
     * The CE amplifier has:
     * - 3 low-frequency poles (if all coupling/bypass caps present)
     * - 1 dominant high-frequency pole (Miller)
     * - 1 secondary high-frequency pole (output)
     * - No zeros (for the standard mid-band model)
     *
     * The transfer function:
     * H(s) = Av_mid * (s/w_L1)/(1+s/w_L1) * (s/w_L2)/(1+s/w_L2)
     *               * (s/w_L3)/(1+s/w_L3) * 1/((1+s/w_H1)*(1+s/w_H2))
     *
     * where each low-freq factor s/w_L / (1+s/w_L) is a high-pass
     * (coupling/bypass capacitor effect). */

    if (!tf) return;

    tf->A0 = Av_mid;
    tf->n_poles = 0;
    tf->n_zeros = 0;

    /* Low-frequency poles (high-pass) */
    if (fL_in > 0.0 && tf->n_poles < BJT_MAX_POLES) {
        tf->poles_rad_per_s[tf->n_poles++] = 2.0 * M_PI * fL_in;
        /* Corresponding zero at the origin (s / (s + w_L)) */
        if (tf->n_zeros < BJT_MAX_ZEROS) {
            /* Zero at DC: represents the high-pass nature.
             * Actually it's s/(s+w_L), zero at 0, pole at -w_L.
             * We represent the zero as very small frequency. */
            tf->zeros_rad_per_s[tf->n_zeros++] = 0.001;  /* ~ DC */
        }
    }

    if (fL_out > 0.0 && tf->n_poles < BJT_MAX_POLES) {
        tf->poles_rad_per_s[tf->n_poles++] = 2.0 * M_PI * fL_out;
    }

    if (fL_bypass > 0.0 && tf->n_poles < BJT_MAX_POLES) {
        tf->poles_rad_per_s[tf->n_poles++] = 2.0 * M_PI * fL_bypass;
    }

    /* High-frequency poles (low-pass) */
    if (fH_miller > 0.0 && tf->n_poles < BJT_MAX_POLES) {
        tf->poles_rad_per_s[tf->n_poles++] = 2.0 * M_PI * fH_miller;
    }

    if (fH_out > 0.0 && tf->n_poles < BJT_MAX_POLES) {
        tf->poles_rad_per_s[tf->n_poles++] = 2.0 * M_PI * fH_out;
    }
}

/* ================================================================
 * Cut-off Frequency Finders (Binary Search)
 * ================================================================ */

double bjt_tf_find_fL(const bjt_transfer_function_t *tf,
                       double f_min, double f_max, double epsilon)
{
    /* Find the low -3dB cut-off frequency using binary search.
     *
     * At f_L: |H(f_L)| / |A0| = 1/sqrt(2). Since |H(f)|/|A0| is
     * monotonically increasing for a band-pass amplifier at low
     * frequencies, binary search works well.
     *
     * Complexity: O(log((f_max-f_min)/epsilon) * (n_poles+n_zeros)). */

    double lo, hi, mid, mag_ratio;
    double target = 1.0 / sqrt(2.0);  /* -3dB = 0.707 */
    double A0_mag;
    int max_iter = 50;
    int iter;

    if (!tf || f_min >= f_max) return f_min;

    A0_mag = fabs(tf->A0);
    if (A0_mag <= 0.0) return 0.0;

    lo = f_min;
    hi = f_max;

    /* Check if f_min is already below -3dB */
    mag_ratio = bjt_tf_magnitude(tf, lo) / A0_mag;
    if (mag_ratio >= target) return lo;

    /* Check if f_max is above -3dB */
    mag_ratio = bjt_tf_magnitude(tf, hi) / A0_mag;
    if (mag_ratio <= target) return hi;

    for (iter = 0; iter < max_iter; iter++) {
        if (hi - lo < epsilon) break;
        mid = (lo + hi) / 2.0;
        mag_ratio = bjt_tf_magnitude(tf, mid) / A0_mag;

        if (mag_ratio < target) {
            lo = mid;
        } else {
            hi = mid;
        }
    }

    return (lo + hi) / 2.0;
}

double bjt_tf_find_fH(const bjt_transfer_function_t *tf,
                       double f_min, double f_max, double epsilon)
{
    /* Find the high -3dB cut-off frequency using binary search.
     *
     * At f_H: |H(f_H)| / |A0| = 1/sqrt(2). |H(f)|/|A0| is
     * monotonically decreasing at high frequencies. */

    double lo, hi, mid, mag_ratio;
    double target = 1.0 / sqrt(2.0);
    double A0_mag;
    int max_iter = 50;
    int iter;

    if (!tf || f_min >= f_max) return f_max;

    A0_mag = fabs(tf->A0);
    if (A0_mag <= 0.0) return f_max;

    lo = f_min;
    hi = f_max;

    /* Check endpoints */
    mag_ratio = bjt_tf_magnitude(tf, lo) / A0_mag;
    if (mag_ratio <= target) return lo;

    mag_ratio = bjt_tf_magnitude(tf, hi) / A0_mag;
    if (mag_ratio >= target) return hi;

    for (iter = 0; iter < max_iter; iter++) {
        if (hi - lo < epsilon) break;
        mid = (lo + hi) / 2.0;
        mag_ratio = bjt_tf_magnitude(tf, mid) / A0_mag;

        if (mag_ratio > target) {
            lo = mid;
        } else {
            hi = mid;
        }
    }

    return (lo + hi) / 2.0;
}

/* ================================================================
 * Bode Plot Generation
 * ================================================================ */

void bjt_bode_generate(const bjt_transfer_function_t *tf,
                        double f_start, double f_end, int n_points,
                        bjt_bode_point_t *points)
{
    /* Generate Bode plot data points at logarithmically-spaced frequencies.
     *
     * For n_points logarithmically spaced between f_start and f_end:
     *   f[i] = f_start * (f_end/f_start)^(i/(n_points-1))
     *
     * At each frequency, compute magnitude in dB and phase in degrees. */

    double log_start, log_end, log_step;
    int i;

    if (!tf || !points || n_points <= 0) return;

    if (f_start <= 0.0) f_start = 1.0;
    if (f_end <= f_start) f_end = f_start * 1000.0;

    log_start = log10(f_start);
    log_end   = log10(f_end);

    for (i = 0; i < n_points; i++) {
        double log_f, f, mag, phase;

        if (n_points == 1) {
            log_f = (log_start + log_end) / 2.0;
        } else {
            log_f = log_start + (log_end - log_start) * i / (n_points - 1.0);
        }

        f = pow(10.0, log_f);
        mag = bjt_tf_magnitude(tf, f);
        phase = bjt_tf_phase(tf, f);

        points[i].frequency_hz = f;
        points[i].magnitude_db = bjt_gain_to_db(mag);
        points[i].phase_degrees = phase * 180.0 / M_PI;
    }
}

/* ================================================================
 * Dominant Pole Approximation
 * ================================================================ */

double bjt_tf_dominant_pole(const bjt_transfer_function_t *tf)
{
    /* Find the dominant (lowest-frequency) pole.
     *
     * For high-frequency: the dominant pole is the lowest-frequency pole.
     * The dominant pole approximation sets f_H ? f_p_dominant,
     * valid when the dominant pole is sufficiently separated from
     * the next pole (factor of 4 or more). */

    double min_pole = 1e30;
    int i;

    if (!tf || tf->n_poles <= 0) return 1e12;

    for (i = 0; i < tf->n_poles; i++) {
        if (tf->poles_rad_per_s[i] > 0.0 &&
            tf->poles_rad_per_s[i] < min_pole) {
            min_pole = tf->poles_rad_per_s[i];
        }
    }

    if (min_pole >= 1e30) return 1e12;
    return min_pole / (2.0 * M_PI);  /* Convert rad/s to Hz */
}

double bjt_tf_bandwidth_two_pole(double f_p1, double f_p2)
{
    /* Estimate total bandwidth when two poles interact.
     *
     * If poles are widely separated (>4x): BW ? f_p1 (the dominant pole).
     *
     * If poles are close: the combined -3dB point is lower than f_p1.
     * For two identical real poles: f_-3dB ? 0.64 * f_p.
     *
     * General formula for two real poles:
     *   f_-3dB = f_p * sqrt( sqrt(2) - 1 ) when f_p1 = f_p2 = f_p
     *
     * Approximation (root-sum-of-squares of time constants):
     *   1/BW^2 ? 1/f_p1^2 + 1/f_p2^2 */

    if (f_p1 <= 0.0 && f_p2 <= 0.0) return 0.0;
    if (f_p1 <= 0.0) return f_p2;
    if (f_p2 <= 0.0) return f_p1;

    /* Root-sum-of-squares approximation for bandwidth */
    return 1.0 / sqrt(1.0/(f_p1*f_p1) + 1.0/(f_p2*f_p2));
}
