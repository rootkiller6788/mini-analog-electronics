/**
 * @file    oscillator_analysis.c
 * @brief   Oscillator Analysis — L3 Van der Pol + L4 Leeson Phase Noise + L5 Bode/THD/Jitter
 *
 * @details Implements Van der Pol oscillator numerical simulation (RK4),
 *          Leeson phase noise model, Hajimiri LTV phase noise model,
 *          Bode plot analysis for Barkhausen verification, THD computation,
 *          Allan variance, jitter metrics, and frequency stability analysis.
 *
 * Knowledge covered:
 *   L1: Phase noise, Allan variance, jitter, THD
 *   L3: Van der Pol equation, limit cycle, phase space
 *   L4: Leeson formula (1966), Hajimiri LTV model (1998)
 *   L5: Runge-Kutta integration, Bode sweep, jitter integration
 *
 * Reference:
 *   Leeson, "A Simple Model of Feedback Oscillator Noise Spectrum" (1966)
 *   Hajimiri & Lee, "A General Theory of Phase Noise" (1998)
 *   Demir et al., "Phase Noise in Oscillators" (2000)
 */

#include "oscillator_analysis.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef K_BOLTZMANN
#define K_BOLTZMANN 1.380649e-23
#endif

/* ─── Internal helpers ──────────────────────────────────────────────────── */

static void* safe_malloc(size_t sz) {
    void *p = malloc(sz);
    if (!p) { fprintf(stderr, "oscillator_analysis: malloc(%zu) failed\n", sz); abort(); }
    return p;
}

/* ─── L3: Van der Pol Oscillator ──────────────────────────────────────────── */

/**
 * Initialize Van der Pol oscillator parameters.
 *
 * The Van der Pol equation is the canonical model for self-sustained
 * oscillators, exhibiting a stable limit cycle for all μ > 0.
 *
 * d²x/dt² - μ(1 - x²)·dx/dt + ω₀²x = 0
 *
 * For μ = 0: pure harmonic oscillator, no limit cycle (undamped)
 * For μ << 1: nearly sinusoidal oscillation with amplitude ≈ 2
 * For μ >> 1: relaxation oscillation with sharp transitions
 *
 * The period for small μ: T ≈ 2π/ω₀  (μ=0)
 * The period for large μ: T ≈ μ·(3 - 2·ln2)/ω₀  (μ → ∞)
 */
van_der_pol_params_t van_der_pol_init(double mu, double freq_hz)
{
    van_der_pol_params_t params;
    memset(&params, 0, sizeof(params));

    params.mu = mu;
    params.freq_hz = freq_hz;
    params.omega0 = 2.0 * M_PI * freq_hz;
    params.period_sec = (freq_hz > 0.0) ? 1.0 / freq_hz : 0.0;

    /* For small μ, limit cycle amplitude ≈ 2 */
    params.amplitude_limit = 2.0;
    params.is_relaxation = (mu > 10.0) ? 1 : 0;

    return params;
}

/**
 * Compute the derivative (right-hand side) of the Van der Pol system.
 *
 * State-space form:
 *   dx/dt = y
 *   dy/dt = μ(1 - x²)·y - ω₀²x
 */
void van_der_pol_derivative(const van_der_pol_state_t *state,
                             const van_der_pol_params_t *params,
                             double *dx, double *dy)
{
    if (!state || !params || !dx || !dy) return;

    *dx = state->y;
    *dy = params->mu * (1.0 - state->x * state->x) * state->y
         - params->omega0 * params->omega0 * state->x;
}

/**
 * Simulate Van der Pol oscillator using classical 4th-order Runge-Kutta.
 *
 * RK4 provides O(dt⁴) local truncation error and is the standard
 * method for solving nonlinear ODEs like the Van der Pol equation.
 *
 * For the system dy/dt = f(t, y):
 *   k1 = f(t_n, y_n)
 *   k2 = f(t_n + dt/2, y_n + dt·k1/2)
 *   k3 = f(t_n + dt/2, y_n + dt·k2/2)
 *   k4 = f(t_n + dt, y_n + dt·k3)
 *   y_{n+1} = y_n + (dt/6)·(k1 + 2·k2 + 2·k3 + k4)
 *
 * Stability recommendation: dt ≤ T/(50·max(1, μ)) for accurate results.
 */
van_der_pol_sim_t van_der_pol_simulate(const van_der_pol_params_t *params,
                                          double x0, double y0,
                                          double duration_sec, double dt_sec)
{
    van_der_pol_sim_t sim;
    memset(&sim, 0, sizeof(sim));

    if (!params || duration_sec <= 0.0 || dt_sec <= 0.0) return sim;

    sim.params = *params;
    sim.num_points = (size_t)(duration_sec / dt_sec) + 1;
    if (sim.num_points < 2) sim.num_points = 2;

    sim.time = (double*)safe_malloc(sim.num_points * sizeof(double));
    sim.x    = (double*)safe_malloc(sim.num_points * sizeof(double));
    sim.y    = (double*)safe_malloc(sim.num_points * sizeof(double));

    /* Initial state */
    van_der_pol_state_t s;
    s.x = x0;
    s.y = y0;
    s.t = 0.0;

    sim.time[0] = s.t;
    sim.x[0] = s.x;
    sim.y[0] = s.y;

    for (size_t i = 1; i < sim.num_points; i++) {
        /* RK4 integration */
        double dt = dt_sec;

        /* k1 */
        double dx1, dy1;
        van_der_pol_derivative(&s, params, &dx1, &dy1);

        /* k2 */
        van_der_pol_state_t s2;
        s2.x = s.x + 0.5 * dt * dx1;
        s2.y = s.y + 0.5 * dt * dy1;
        s2.t = s.t + 0.5 * dt;
        double dx2, dy2;
        van_der_pol_derivative(&s2, params, &dx2, &dy2);

        /* k3 */
        van_der_pol_state_t s3;
        s3.x = s.x + 0.5 * dt * dx2;
        s3.y = s.y + 0.5 * dt * dy2;
        s3.t = s.t + 0.5 * dt;
        double dx3, dy3;
        van_der_pol_derivative(&s3, params, &dx3, &dy3);

        /* k4 */
        van_der_pol_state_t s4;
        s4.x = s.x + dt * dx3;
        s4.y = s.y + dt * dy3;
        s4.t = s.t + dt;
        double dx4, dy4;
        van_der_pol_derivative(&s4, params, &dx4, &dy4);

        /* Update */
        s.x = s.x + (dt / 6.0) * (dx1 + 2.0 * dx2 + 2.0 * dx3 + dx4);
        s.y = s.y + (dt / 6.0) * (dy1 + 2.0 * dy2 + 2.0 * dy3 + dy4);
        s.t += dt;

        sim.time[i] = s.t;
        sim.x[i] = s.x;
        sim.y[i] = s.y;
    }

    /* Estimate steady-state amplitude from the last 20% of the signal */
    size_t start_idx = sim.num_points * 4 / 5;
    if (start_idx < 1) start_idx = 1;
    double max_x = 0.0;
    for (size_t i = start_idx; i < sim.num_points; i++) {
        double abs_x = fabs(sim.x[i]);
        if (abs_x > max_x) max_x = abs_x;
    }
    sim.steady_state_amplitude = max_x;

    /* Estimate period from zero-crossings in the steady-state region */
    int crossings = 0;
    double first_t = 0.0, last_t = 0.0;
    for (size_t i = start_idx + 1; i < sim.num_points; i++) {
        if (sim.x[i - 1] * sim.x[i] <= 0.0 && sim.y[i] > 0.0) {
            /* Zero crossing with positive slope */
            if (crossings == 0) {
                first_t = sim.time[i];
            }
            last_t = sim.time[i];
            crossings++;
        }
    }
    if (crossings >= 3) {
        sim.period_measured = (last_t - first_t) / (double)(crossings - 1);
    } else {
        sim.period_measured = params->period_sec;
    }

    /* THD estimate: compute first few harmonics via simplified Goertzel-like
     * approach for the steady-state region */
    double period = sim.period_measured;
    if (period > 0.0) {
        double a1_cos = 0.0, a1_sin = 0.0;
        double a2_rms = 0.0, a3_rms = 0.0;
        size_t period_samples = (size_t)(period / dt_sec);
        if (period_samples > 0 && period_samples < sim.num_points - start_idx) {
            size_t end_idx = start_idx + period_samples * 3; /* 3 cycles */
            if (end_idx > sim.num_points) end_idx = sim.num_points;
            size_t n_pts = end_idx - start_idx;

            for (size_t i = start_idx; i < end_idx; i++) {
                double t = sim.time[i] - sim.time[start_idx];
                double omega0 = 2.0 * M_PI / period;
                a1_cos += sim.x[i] * cos(omega0 * t);
                a1_sin += sim.x[i] * sin(omega0 * t);
                a2_rms += sim.x[i] * cos(2.0 * omega0 * t);
                /* a2_rms accumulates as-is for simplicity */
                a3_rms += sim.x[i] * cos(3.0 * omega0 * t);
            }

            double a1_sq = (a1_cos * a1_cos + a1_sin * a1_sin) / (double)(n_pts * n_pts);
            if (a1_sq > 1.0e-20 && n_pts > 0) {
                /* Simple THD from harmonic RMS */
                double harmonics_sq = (a2_rms * a2_rms + a3_rms * a3_rms) / (double)(n_pts * n_pts);
                sim.thd_percent = sqrt(harmonics_sq / a1_sq) * 100.0;
            }
        }
    }

    return sim;
}

void van_der_pol_sim_free(van_der_pol_sim_t *sim) {
    if (sim) {
        free(sim->time);
        free(sim->x);
        free(sim->y);
        sim->time = NULL;
        sim->x = NULL;
        sim->y = NULL;
        sim->num_points = 0;
    }
}

/* ─── L4: Leeson Phase Noise Model ────────────────────────────────────────── */

/**
 * Compute phase noise using Leeson's formula.
 *
 * Leeson's phase noise model (1966) provides the fundamental relationship
 * between resonator Q, signal power, and phase noise in oscillators.
 *
 * The formula can be separated into three regimes:
 *
 * 1. Close-in (Δf < f_c):  1/f³ noise, slope -30 dB/dec
 *    L(Δf) = L_floor + 10·log₁₀[1 + (f₀/(2QΔf))²] + 10·log₁₀(1 + f_c/|Δf|)
 *
 * 2. Mid-range (f_c < Δf < f₀/(2Q)): 1/f² noise, slope -20 dB/dec
 *    L(Δf) ≈ L_floor + 20·log₁₀[f₀/(2QΔf)]
 *
 * 3. Far-out (Δf > f₀/(2Q)): noise floor, slope 0 dB/dec
 *    L(Δf) ≈ L_floor = 10·log₁₀(F·kT/(2P_s))
 */
leeson_phase_noise_t leeson_phase_noise_compute(double carrier_hz,
                                                   double offset_hz,
                                                   double loaded_q,
                                                   double signal_power_w,
                                                   double noise_factor,
                                                   double flicker_hz,
                                                   double temp_k)
{
    leeson_phase_noise_t result;
    memset(&result, 0, sizeof(result));

    if (carrier_hz <= 0.0 || offset_hz <= 0.0) return result;

    result.freq_carrier_hz = carrier_hz;
    result.freq_offset_hz = offset_hz;
    result.loaded_q = (loaded_q > 0.0) ? loaded_q : 1.0;
    result.signal_power_w = (signal_power_w > 0.0) ? signal_power_w : 1.0e-3;
    result.noise_factor = (noise_factor >= 1.0) ? noise_factor : 1.0;
    result.flicker_corner_hz = (flicker_hz > 0.0) ? flicker_hz : 1.0e6; /* large = no flicker */
    result.temperature_k = (temp_k > 0.0) ? temp_k : 290.0;

    /* Noise floor: L_floor = 10·log₁₀(F·k·T / (2·P_s)) */
    double noise_floor_linear = result.noise_factor * K_BOLTZMANN * result.temperature_k
                                / (2.0 * result.signal_power_w);
    result.floor_noise_dbc_hz = 10.0 * log10(noise_floor_linear);

    /* f₀/(2Q) corner frequency */
    result.corner_f0_2q_hz = carrier_hz / (2.0 * result.loaded_q);

    /* Leeson formula components */
    double f0_2q_term = 1.0 + pow(carrier_hz / (2.0 * result.loaded_q * offset_hz), 2.0);
    double flicker_term = 1.0 + result.flicker_corner_hz / fabs(offset_hz);

    double total_linear = noise_floor_linear * f0_2q_term * flicker_term;
    if (total_linear > 0.0) {
        result.phase_noise_dbc_hz = 10.0 * log10(total_linear);
    } else {
        result.phase_noise_dbc_hz = -200.0;
    }

    return result;
}

/* ─── L4: Hajimiri LTV Phase Noise Model ──────────────────────────────────── */

/**
 * Compute phase noise using Hajimiri's LTV model.
 *
 * The key insight of the LTV model is that noise sensitivity varies
 * with the oscillator's operating point along the limit cycle,
 * characterized by the Impulse Sensitivity Function (ISF) Γ(ω₀τ).
 *
 * The 1/f² region phase noise:
 *   L(Δω) = 10·log₁₀( Γ²_rms · i²_n/Δf / (2 · q²_max · Δω²) )
 *
 * The 1/f³ region (from ISF DC component c₀):
 *   L(Δω) = 10·log₁₀( Γ²_rms · i²_n/Δf · (ω_1/f/Δω) / (2 · q²_max · Δω²) )
 *
 * where ω_1/f is the flicker noise corner frequency.
 */
hajimiri_phase_noise_t hajimiri_phase_noise_compute(double gamma_rms,
                                                      double q_max_coulombs,
                                                      double noise_psd,
                                                      double offset_hz,
                                                      double isf_dc,
                                                      double flicker_hz)
{
    hajimiri_phase_noise_t result;
    memset(&result, 0, sizeof(result));

    if (offset_hz <= 0.0 || q_max_coulombs <= 0.0) return result;

    result.gamma_rms = gamma_rms;
    result.q_max_coulombs = q_max_coulombs;
    result.noise_current_psd = noise_psd;
    result.freq_offset_hz = offset_hz;
    result.isf_dc = isf_dc;

    double delta_omega = 2.0 * M_PI * offset_hz;
    double denom = 2.0 * q_max_coulombs * q_max_coulombs * delta_omega * delta_omega;

    if (denom > 0.0) {
        /* 1/f² phase noise */
        double pn_linear = gamma_rms * gamma_rms * noise_psd / denom;
        if (pn_linear > 1.0e-30) {
            result.phase_noise_dbc_hz = 10.0 * log10(pn_linear);
        } else {
            result.phase_noise_dbc_hz = -200.0;
        }

        /* 1/f³ contribution (if flicker corner is given) */
        if (flicker_hz > 0.0 && isf_dc > 0.0 && offset_hz > 0.0) {
            double pn_flicker = isf_dc * isf_dc * noise_psd * flicker_hz
                                / (denom * offset_hz);
            if (pn_flicker > 1.0e-30) {
                result.flicker_pn_dbc_hz = 10.0 * log10(pn_flicker);
            }
        }
    }

    return result;
}

/* ─── L5: Phase Noise to RMS Jitter ───────────────────────────────────────── */

/**
 * Convert phase noise profile to RMS phase jitter.
 *
 * RMS jitter (in seconds):
 *   σ_t = (1/(2π·f₀)) · sqrt(2 · ∫_{f_low}^{f_high} 10^{L(f)/10} df)
 *
 * The integral is approximated using trapezoidal rule over the
 * provided phase noise data points.
 *
 * For typical communications applications, the integration limits are:
 *   - SONET/SDH: 12 kHz to 20 MHz
 *   - PCIe: 10 kHz to 100 MHz
 *   - GPS: 1 Hz to 10 MHz
 */
double phase_noise_to_rms_jitter(double carrier_hz,
                                   double offset_start_hz,
                                   double offset_end_hz,
                                   const double *pn_dbc_hz,
                                   const double *offset_hz_arr,
                                   size_t num_points)
{
    if (carrier_hz <= 0.0 || !pn_dbc_hz || !offset_hz_arr || num_points < 2)
        return 0.0;

    /* Integrate phase noise over the given range using trapezoidal rule */
    double integral = 0.0;

    for (size_t i = 0; i < num_points - 1; i++) {
        double f1 = offset_hz_arr[i];
        double f2 = offset_hz_arr[i + 1];

        /* Only integrate within the specified range */
        if (f2 < offset_start_hz || f1 > offset_end_hz) continue;

        double f_low  = (f1 < offset_start_hz) ? offset_start_hz : f1;
        double f_high = (f2 > offset_end_hz) ? offset_end_hz : f2;
        if (f_low >= f_high) continue;

        /* Interpolate phase noise at f_low and f_high linearly */
        double t = (f1 != f2) ? (f_low - f1) / (f2 - f1) : 0.0;
        double pn_low  = pn_dbc_hz[i] + t * (pn_dbc_hz[i + 1] - pn_dbc_hz[i]);
        t = (f1 != f2) ? (f_high - f1) / (f2 - f1) : 1.0;
        double pn_high = pn_dbc_hz[i] + t * (pn_dbc_hz[i + 1] - pn_dbc_hz[i]);

        /* Trapezoidal integration of 10^{L(f)/10} */
        double val_low  = pow(10.0, pn_low / 10.0);
        double val_high = pow(10.0, pn_high / 10.0);
        integral += 0.5 * (val_low + val_high) * (f_high - f_low);
    }

    /* Convert integrated phase noise to RMS jitter */
    double rms_jitter_rad = sqrt(2.0 * integral);
    double rms_jitter_sec = rms_jitter_rad / (2.0 * M_PI * carrier_hz);

    return rms_jitter_sec;
}

/* ─── L5: Bode Plot Analysis ──────────────────────────────────────────────── */

/**
 * Create a logarithmically-spaced frequency sweep for Bode analysis.
 */
bode_analysis_t bode_sweep_create(double freq_start_hz, double freq_end_hz,
                                     size_t points_per_decade)
{
    bode_analysis_t bode;
    memset(&bode, 0, sizeof(bode));

    if (freq_start_hz <= 0.0 || freq_end_hz <= freq_start_hz)
        return bode;

    if (points_per_decade < 2) points_per_decade = 10;

    double decades = log10(freq_end_hz / freq_start_hz);
    bode.num_points = (size_t)(decades * (double)points_per_decade) + 2;
    if (bode.num_points < 3) bode.num_points = 3;

    bode.freq_hz      = (double*)safe_malloc(bode.num_points * sizeof(double));
    bode.magnitude_db = (double*)safe_malloc(bode.num_points * sizeof(double));
    bode.phase_deg    = (double*)safe_malloc(bode.num_points * sizeof(double));

    double log_start = log10(freq_start_hz);
    double log_end   = log10(freq_end_hz);
    double log_step  = (log_end - log_start) / (double)(bode.num_points - 1);

    for (size_t i = 0; i < bode.num_points; i++) {
        bode.freq_hz[i] = pow(10.0, log_start + (double)i * log_step);
    }

    return bode;
}

/**
 * Analyze a transfer function over a frequency sweep.
 */
void bode_analyze(bode_analysis_t *bode,
                   void (*transfer_fn)(double w, double *mag, double *phase_deg, void *user_data),
                   void *user_data)
{
    if (!bode || !transfer_fn) return;

    for (size_t i = 0; i < bode->num_points; i++) {
        double omega = 2.0 * M_PI * bode->freq_hz[i];
        double mag, phase;
        transfer_fn(omega, &mag, &phase, user_data);
        bode->magnitude_db[i] = (mag > 1.0e-20) ? 20.0 * log10(mag) : -400.0;
        bode->phase_deg[i] = phase;
    }
}

/**
 * Find gain and phase crossover frequencies by interpolation.
 */
void bode_find_crossings(bode_analysis_t *bode)
{
    if (!bode || bode->num_points < 2) return;

    /* Find 0 dB crossing (gain crossover frequency) */
    for (size_t i = 0; i < bode->num_points - 1; i++) {
        double m1 = bode->magnitude_db[i];
        double m2 = bode->magnitude_db[i + 1];
        if (m1 * m2 <= 0.0 && m1 > m2) {
            /* Interpolate */
            double t = -m1 / (m2 - m1);
            bode->phase_at_0db_hz = bode->freq_hz[i]
                                     + t * (bode->freq_hz[i + 1] - bode->freq_hz[i]);
            /* Phase margin = 180 + phase @ 0dB crossover */
            double p1 = bode->phase_deg[i];
            double p2 = bode->phase_deg[i + 1];
            double phase_at_cross = p1 + t * (p2 - p1);
            bode->phase_margin_deg = 180.0 + phase_at_cross;
            break;
        }
    }

    /* Find -180° crossing (phase crossover frequency) */
    for (size_t i = 0; i < bode->num_points - 1; i++) {
        double p1 = bode->phase_deg[i];
        double p2 = bode->phase_deg[i + 1];
        /* Check if phase crosses -180° */
        if ((p1 + 180.0) * (p2 + 180.0) <= 0.0) {
            double t = -(p1 + 180.0) / (p2 - p1);
            double freq_at_180 = bode->freq_hz[i]
                                  + t * (bode->freq_hz[i + 1] - bode->freq_hz[i]);
            /* Interpolate gain at this frequency */
            double m1 = bode->magnitude_db[i];
            double m2 = bode->magnitude_db[i + 1];
            bode->gain_at_0deg_db = m1 + t * (m2 - m1);
            bode->gain_margin_db = -bode->gain_at_0deg_db;
            break;
        }
    }
}

void bode_analysis_free(bode_analysis_t *bode) {
    if (bode) {
        free(bode->freq_hz);
        free(bode->magnitude_db);
        free(bode->phase_deg);
        bode->freq_hz = NULL;
        bode->magnitude_db = NULL;
        bode->phase_deg = NULL;
        bode->num_points = 0;
    }
}

/* ─── L5: THD Computation ────────────────────────────────────────────────── */

/**
 * Compute Total Harmonic Distortion from harmonic amplitudes.
 *
 * THD = sqrt(Σ_{k=2}^{N} A_k²) / A₁ · 100%
 *
 * A₁ is the fundamental component amplitude.
 * A_k for k >= 2 are the harmonic amplitudes.
 *
 * For a pure sine wave: THD = 0%
 * For a square wave:   THD ≈ 48.3% (only odd harmonics,
 *                       Σ(1/(2k+1)²) from k=1 → THD approaches ~48.3%)
 * For a perfect triangle: THD ≈ 12.1%
 *
 * @note amplitudes[0] corresponds to harmonic number 1 (fundamental)
 */
double thd_compute(const double *amplitudes, size_t num_harmonics)
{
    if (!amplitudes || num_harmonics < 2) return 0.0;

    double fundamental = amplitudes[0];
    if (fabs(fundamental) < 1.0e-20) return 0.0;

    double harmonics_power = 0.0;
    for (size_t k = 1; k < num_harmonics; k++) {
        harmonics_power += amplitudes[k] * amplitudes[k];
    }

    return sqrt(harmonics_power) / fabs(fundamental) * 100.0;
}

/* ─── L5: Allan Variance ──────────────────────────────────────────────────── */

/**
 * Compute Allan variance from fractional frequency measurements.
 *
 * σ²_y(τ) = 1/(2(M-1)) · Σ_{i=1}^{M-1} (ȳ_{i+1} - ȳ_i)²
 *
 * where ȳ_i is the i-th fractional frequency average over interval τ.
 *
 * Allan variance is the standard metric for oscillator stability
 * characterization, revealing noise processes at different time scales.
 */
allan_variance_t allan_variance_compute(const double *freq_measurements,
                                           size_t M, double tau_sec)
{
    allan_variance_t result;
    memset(&result, 0, sizeof(result));

    if (!freq_measurements || M < 2 || tau_sec <= 0.0) return result;

    result.tau_sec = tau_sec;

    double sum_sq = 0.0;
    for (size_t i = 0; i < M - 1; i++) {
        double diff = freq_measurements[i + 1] - freq_measurements[i];
        sum_sq += diff * diff;
    }

    result.allan_variance = sum_sq / (2.0 * (double)(M - 1));
    result.allan_deviation = sqrt(result.allan_variance);
    result.stability_ppb = result.allan_deviation * 1.0e9;

    return result;
}

/* ─── L5: Jitter Metrics from Phase Noise ─────────────────────────────────── */

/**
 * Compute jitter metrics from phase noise data.
 *
 * Converts phase noise profile to:
 *   - RMS phase jitter (radians and seconds)
 *   - Period jitter estimate
 *   - Cycle-to-cycle jitter estimate
 */
jitter_metrics_t jitter_from_phase_noise(double carrier_hz,
                                           const double *pn_dbc_hz,
                                           const double *offset_hz,
                                           size_t num_points)
{
    jitter_metrics_t j;
    memset(&j, 0, sizeof(j));

    if (carrier_hz <= 0.0 || !pn_dbc_hz || !offset_hz || num_points < 2)
        return j;

    /* Typical integration limits for communications: 12 kHz to 20 MHz */
    j.phase_noise_integ_min_hz = 12000.0;
    j.phase_noise_integ_max_hz = 20.0e6;

    j.rms_phase_jitter_ps = phase_noise_to_rms_jitter(carrier_hz,
        j.phase_noise_integ_min_hz, j.phase_noise_integ_max_hz,
        pn_dbc_hz, offset_hz, num_points) * 1.0e12;  /* convert s to ps */

    j.rms_phase_jitter_rad = 2.0 * M_PI * carrier_hz
                              * j.rms_phase_jitter_ps * 1.0e-12;

    /* Period jitter ~ RMS phase jitter (single period) */
    j.period_jitter_ps = j.rms_phase_jitter_ps * 0.7;  /* approximate relationship */

    /* Cycle-to-cycle jitter ~ sqrt(2) × period jitter for white noise */
    j.cycle_to_cycle_jitter_ps = j.period_jitter_ps * sqrt(2.0);

    /* TIE at 1 ms: accumulated jitter grows as sqrt(observation_time) */
    j.tie_at_1ms_ps = j.period_jitter_ps * sqrt(0.001 * carrier_hz);

    return j;
}

/* ─── L5: Frequency Stability Analysis ────────────────────────────────────── */

/**
 * Analyze frequency stability over temperature, supply, and aging.
 *
 * Populates the frequency_stability_t structure with computed
 * stability metrics based on oscillator topology and parameters.
 */
void frequency_stability_analyze(const oscillator_params_t *params,
                                   frequency_stability_t *stability)
{
    if (!params || !stability) return;

    /* Set defaults based on oscillator type */
    switch (params->type) {
        case OSC_TYPE_PIERCE_CRYSTAL:
        case OSC_TYPE_COLPITTS_CRYSTAL:
            /* Crystal: excellent stability */
            stability->temp_coefficient_ppm_per_c = 0.5;  /* AT-cut typical */
            stability->supply_pushing_ppm_per_v = 0.1;
            stability->load_pulling_ppm_per_pf = 5.0;
            stability->aging_ppm_per_year = 3.0;
            stability->short_term_stability_ppb = 1.0;
            stability->temp_range_min_c = -20.0;
            stability->temp_range_max_c = 70.0;
            break;
        case OSC_TYPE_COLPITTS:
        case OSC_TYPE_CLAPP:
            /* LC: moderate stability */
            stability->temp_coefficient_ppm_per_c = 50.0;
            stability->supply_pushing_ppm_per_v = 100.0;
            stability->load_pulling_ppm_per_pf = 500.0;
            stability->aging_ppm_per_year = 200.0;
            stability->short_term_stability_ppb = 100.0;
            stability->temp_range_min_c = -40.0;
            stability->temp_range_max_c = 85.0;
            break;
        case OSC_TYPE_RELAXATION_555:
            /* 555: poor stability */
            stability->temp_coefficient_ppm_per_c = 150.0;
            stability->supply_pushing_ppm_per_v = 1000.0;
            stability->load_pulling_ppm_per_pf = 0.0;
            stability->aging_ppm_per_year = 500.0;
            stability->short_term_stability_ppb = 10000.0;
            stability->temp_range_min_c = 0.0;
            stability->temp_range_max_c = 70.0;
            break;
        default:
            stability->temp_coefficient_ppm_per_c = 100.0;
            stability->supply_pushing_ppm_per_v = 200.0;
            stability->load_pulling_ppm_per_pf = 100.0;
            stability->aging_ppm_per_year = 300.0;
            stability->short_term_stability_ppb = 500.0;
            stability->temp_range_min_c = 0.0;
            stability->temp_range_max_c = 70.0;
            break;
    }
}
