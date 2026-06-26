/**
 * bjt_model.c ? BJT Device Model Implementation
 *
 * Implements the Shockley/Ebers-Moll equations, small-signal parameter
 * extraction, and two-port conversions. All functions are O(1) and
 * based on the standard semiconductor device physics from:
 *   Sedra & Smith, "Microelectronic Circuits", 8th ed.
 *   Gray, Hurst, Lewis & Meyer, "Analysis and Design of Analog ICs", 5th ed.
 */

#include "bjt_model.h"
#include <math.h>
#include <stdlib.h>

/* ---- Thermal Voltage ---- */

double bjt_vt(double temperature_kelvin)
{
    /* VT = kT/q. Guard against absolute zero (T <= 0). */
    if (temperature_kelvin <= 0.0) {
        return BJT_VT_300K;  /* Return nominal value for invalid input */
    }
    return BJT_K_OVER_Q * temperature_kelvin;
}

/* ---- Transconductance ---- */

double bjt_gm(double ic, double vt)
{
    /* gm = dIC/dVBE = IC/VT ? the fundamental linearization of Shockley.
     * Guard against VT = 0 (divide-by-zero). */
    if (vt <= 0.0 || ic <= 0.0) {
        return 0.0;
    }
    return ic / vt;
}

/* ---- Intrinsic Emitter Resistance ---- */

double bjt_re(double ie, double vt)
{
    /* re = VT/IE.
     * For forward-active: IE ? IC, so re ? 1/gm.
     * Guard against IE = 0. */
    if (ie <= 0.0 || vt <= 0.0) {
        return 0.0;
    }
    return vt / ie;
}

/* ---- Base-Emitter Input Resistance (r-pi) ---- */

double bjt_rpi(double beta, double gm)
{
    /* rpi = beta/gm = beta * VT / IC.
     * This is the incremental resistance looking into the base. */
    if (gm <= 0.0 || beta <= 0.0) {
        return 0.0;
    }
    return beta / gm;
}

/* ---- Output Resistance (Early Effect) ---- */

double bjt_ro(double va_early, double ic)
{
    /* ro = VA/IC.
     * The Early effect: collector current increases with VCE
     * even in the forward-active region, modeled as a finite
     * output resistance. VA (Early voltage) typically 50-200V. */
    if (ic <= 1e-15 || va_early <= 0.0) {
        return 1e12;  /* Near-infinite resistance = ideal current source */
    }
    return va_early / ic;
}

/* ---- Alpha-Beta Conversions ---- */

double bjt_alpha_from_beta(double beta)
{
    /* alpha = IC/IE = beta/(beta+1).
     * alpha is always < 1 but very close for high beta. */
    if (beta <= 0.0) {
        return 0.0;
    }
    return beta / (beta + 1.0);
}

double bjt_beta_from_alpha(double alpha)
{
    /* beta = IC/IB = alpha/(1-alpha).
     * Guard against alpha >= 1 (invalid). */
    if (alpha <= 0.0 || alpha >= 1.0) {
        return 0.0;
    }
    return alpha / (1.0 - alpha);
}

/* ---- Full Ebers-Moll Equation ---- */

double bjt_ebers_moll_ic(double vbe, double vbc, double vt,
                          double IS, double BF, double BR,
                          double NF, double NR, double VAF, double VAR)
{
    /* Ebers-Moll (injection) model for IC:
     *
     * IC = IS * (exp(VBE/(NF*VT)) - 1) * (1 + VCE/VAF)   [forward injection]
     *    - IS/BR * (exp(VBC/(NR*VT)) - 1)                 [reverse injection]
     *
     * Where VCE = VBE - VBC (approx).
     *
     * The Early effect (1 + VCE/VAF) models base-width modulation:
     * increasing VCE narrows the base, increasing IC.
     *
     * Reference: Ebers, J.J. and Moll, J.L.,
     *   "Large-Signal Behavior of Junction Transistors,"
     *   Proc. IRE, vol. 42, pp. 1761-1772, Dec. 1954.
     */

    double vce, forward_term, reverse_term, early_factor;

    if (vt <= 0.0 || IS <= 0.0) {
        return 0.0;
    }

    /* Compute VCE = VCB inverted sign convention: VCE = VBE - VBC */
    vce = vbe - vbc;

    /* Early effect factor for forward portion.
     * Limit to reasonable range to avoid overflow. */
    if (VAF > 0.0) {
        early_factor = 1.0 + vce / VAF;
        if (early_factor < 0.0) early_factor = 0.0;
    } else {
        early_factor = 1.0;
    }

    /* Forward injection: carrier injection from emitter into base */
    if (vbe / (NF * vt) > 40.0) {
        /* exp(40) ? 2.35e17 ? near overflow, use large approximation */
        forward_term = IS * exp(40.0) * early_factor / BF;
    } else if (vbe / (NF * vt) < -40.0) {
        forward_term = -IS * early_factor / BF;
    } else {
        forward_term = IS * (exp(vbe / (NF * vt)) - 1.0) * early_factor / BF;
    }

    /* Reverse injection: carrier injection from collector into base.
     * In forward-active, VBC < 0, so this term is negligible. */
    if (BR > 0.0) {
        double early_rev = 1.0;
        if (VAR > 0.0) {
            early_rev = 1.0 - vce / VAR;  /* VEC = -VCE for reverse */
            if (early_rev < 0.0) early_rev = 0.0;
        }
        if (vbc / (NR * vt) > 40.0) {
            reverse_term = IS * exp(40.0) * early_rev / BR;
        } else if (vbc / (NR * vt) < -40.0) {
            reverse_term = -IS * early_rev / BR;
        } else {
            reverse_term = IS * (exp(vbc / (NR * vt)) - 1.0) * early_rev / BR;
        }
    } else {
        reverse_term = 0.0;
    }

    /* Net IC = BF * forward_term - reverse_term. */
    return BF * forward_term - reverse_term;
}

/* ---- Inverse Shockley: VBE from IC ---- */

double bjt_vbe_from_ic(double ic, double IS, double vt, double nf)
{
    /* VBE = NF * VT * ln(IC/IS + 1).
     * Guard against IC/IS <= 0.
     * For typical forward-active: IC >> IS, so VBE ? NF*VT*ln(IC/IS).
     * At IC = 1 mA, IS = 1e-16, VT = 26 mV:
     *   VBE ? 1 * 0.026 * ln(1e-3/1e-16) = 0.026 * 29.93 ? 0.778 V */
    double ratio;
    if (ic <= 0.0 || IS <= 0.0 || vt <= 0.0 || nf <= 0.0) {
        return 0.0;
    }
    ratio = ic / IS;
    if (ratio < 0.0) ratio = 0.0;
    return nf * vt * log(ratio + 1.0);
}

/* ---- Operating Region Determination ---- */

bjt_region_t bjt_determine_region(double vbe, double vbc,
                                   double vt, bjt_polarity_t pol)
{
    /* For NPN (forward active): VBE > 0 (forward biased), VBC < 0 (reverse).
     * The threshold for "on" is typically ~0.5V for silicon.
     * For PNP, voltages are negative ? handled by polarity reversal. */
    double vbe_abs, vbc_abs;
    double vth = 0.5;  /* Silicon turn-on threshold at low currents */

    if (pol == BJT_PNP) {
        if (vbe > -vth && vbc > -vth) {
            return BJT_REGION_CUTOFF;
        } else if (vbe < -vth && vbc > -vth) {
            return BJT_REGION_FORWARD_ACTIVE;
        } else if (vbe > -vth && vbc < -vth) {
            return BJT_REGION_REVERSE_ACTIVE;
        } else {
            return BJT_REGION_SATURATION;
        }
    } else {
        if (vbe < vth && vbc < vth) {
            return BJT_REGION_CUTOFF;
        } else if (vbe >= vth && vbc < vth) {
            return BJT_REGION_FORWARD_ACTIVE;
        } else if (vbe < vth && vbc >= vth) {
            return BJT_REGION_REVERSE_ACTIVE;
        } else {
            return BJT_REGION_SATURATION;
        }
    }
}

/* ---- Default SPICE Parameters ---- */

void bjt_spice_default_npn(bjt_spice_params_t *p)
{
    if (!p) return;
    /* Typical small-signal NPN (2N3904-like) at 300K */
    p->IS  = 6.734e-15;    /* Transport saturation current */
    p->BF  = 416.4;         /* Forward beta */
    p->BR  = 0.7371;        /* Reverse beta */
    p->NF  = 1.0;           /* Forward emission coefficient */
    p->NR  = 1.0;           /* Reverse emission coefficient */
    p->VAF = 74.03;         /* Forward Early voltage [V] */
    p->VAR = 10.0;          /* Reverse Early voltage [V] */
    p->IKF = 0.06678;       /* Forward knee current [A] */
    p->IKR = 0.01;          /* Reverse knee current [A] */
    p->ISE = 6.734e-15;     /* B-E leakage saturation */
    p->ISC = 1.0e-15;       /* B-C leakage saturation */
    p->NE  = 1.259;         /* B-E leakage emission coeff */
    p->NC  = 1.5;           /* B-C leakage emission coeff */
    p->CJE = 4.5e-12;       /* B-E zero-bias cap [F] */
    p->CJC = 3.638e-12;     /* B-C zero-bias cap [F] */
    p->CJS = 0.0;           /* C-S zero-bias cap */
    p->VJE = 0.75;          /* B-E built-in potential */
    p->VJC = 0.75;          /* B-C built-in potential */
    p->VJS = 0.75;          /* C-S built-in potential */
    p->MJE = 0.33;          /* B-E grading (step junction) */
    p->MJC = 0.3085;        /* B-C grading */
    p->MJS = 0.5;           /* C-S grading */
    p->TF  = 3.012e-10;     /* Forward transit time [s] (~300 ps) */
    p->TR  = 1.0e-8;        /* Reverse transit time [s] */
    p->RB  = 10.0;          /* Base resistance */
    p->RC  = 1.0;           /* Collector resistance */
    p->RE  = 0.0;           /* Emitter resistance */
    p->KF  = 1.0e-16;       /* Flicker noise coefficient */
    p->AF  = 1.0;           /* Flicker noise exponent */
    p->XTI = 3.0;           /* IS temperature exponent */
    p->EG  = 1.11;          /* Silicon bandgap [eV] */
    p->TNOM = 300.15;       /* Nominal temperature [K] (27 C) */
}

void bjt_spice_default_pnp(bjt_spice_params_t *p)
{
    if (!p) return;
    /* Typical small-signal PNP (2N3906-like) at 300K */
    p->IS  = 1.41e-14;
    p->BF  = 180.7;
    p->BR  = 4.977;
    p->NF  = 1.0;
    p->NR  = 1.0;
    p->VAF = 35.0;
    p->VAR = 5.0;
    p->IKF = 0.05;
    p->IKR = 0.01;
    p->ISE = 1.41e-14;
    p->ISC = 1.0e-15;
    p->NE  = 1.5;
    p->NC  = 2.0;
    p->CJE = 1.0e-11;
    p->CJC = 7.0e-12;
    p->CJS = 0.0;
    p->VJE = 0.75;
    p->VJC = 0.75;
    p->VJS = 0.75;
    p->MJE = 0.33;
    p->MJC = 0.33;
    p->MJS = 0.5;
    p->TF  = 5.0e-10;
    p->TR  = 1.0e-8;
    p->RB  = 10.0;
    p->RC  = 1.0;
    p->RE  = 0.0;
    p->KF  = 1.0e-16;
    p->AF  = 1.0;
    p->XTI = 3.0;
    p->EG  = 1.11;
    p->TNOM = 300.15;
}

/* ---- Small-Signal Parameter Extraction ---- */

void bjt_compute_ss_params(const bjt_qpoint_t *qp,
                            const bjt_spice_params_t *sp,
                            double temperature_kelvin,
                            bjt_ss_params_t *ss)
{
    double vt, beta;

    if (!qp || !sp || !ss) return;

    vt = bjt_vt(temperature_kelvin);

    /* Current gain at the Q-point.
     * beta = IC/IB. If IB is zero or very small, use BF from SPICE params. */
    if (qp->IB > 1e-15) {
        beta = qp->IC / qp->IB;
    } else {
        beta = sp->BF;
    }

    /* Clamp beta to reasonable range */
    if (beta <= 0.0) beta = sp->BF;
    if (beta > 10000.0) beta = 10000.0;

    ss->gm    = bjt_gm(qp->IC, vt);
    ss->rpi   = bjt_rpi(beta, ss->gm);
    ss->ro    = bjt_ro(sp->VAF, qp->IC);
    ss->rmu   = 10.0 * beta * ss->ro;  /* Typically very large, often neglected */
    ss->re    = bjt_re(qp->IE, vt);
    ss->alpha = bjt_alpha_from_beta(beta);
    ss->beta  = beta;

    /* Capacitances from SPICE parameters.
     * Cpi = diffusion capacitance (TF*gm) + depletion capacitance (CJE at VBE). */
    ss->Cpi = bjt_diffusion_capacitance(sp->TF, ss->gm)
            + bjt_junction_cap(sp->CJE, sp->VJE, qp->VBE, sp->MJE);

    /* Cmu = collector-base depletion capacitance (at VBC). */
    ss->Cmu = bjt_junction_cap(sp->CJC, sp->VJC, qp->VBC, sp->MJC);

    /* Frequency parameters */
    ss->fT     = bjt_compute_fT(ss);
    ss->f_beta = bjt_compute_f_beta(ss);
}

/* ---- h-Parameter Conversion ---- */

void bjt_h_from_ss(const bjt_ss_params_t *ss, bjt_h_params_t *hp)
{
    if (!ss || !hp) return;

    /* Common-emitter h-parameters from hybrid-pi:
     * h11 = hie = input impedance with output shorted.
     * With output shorted (vce=0), rpi is the only path from B to E
     * (rmu is shorted by the output, ro is shorted).
     *   hie = rpi
     *
     * h12 = hre = reverse voltage transfer ratio.
     * In hybrid-pi: hre ? rpi / (rpi + rmu) ? rpi/rmu ? 0.
     * For most analyses, hre ? 0 (negligible feedback at low freqs).
     *
     * h21 = hfe = forward current gain = beta.
     *   hfe = beta = gm * rpi
     *
     * h22 = hoe = output admittance with input open.
     * With input open, the collector sees ro with some effect from rmu.
     *   hoe ? 1/ro
     */
    hp->h11 = ss->rpi;
    hp->h12 = (ss->rmu > 0.0) ? ss->rpi / ss->rmu : 0.0;
    hp->h21 = ss->beta;
    hp->h22 = (ss->ro > 0.0) ? 1.0 / ss->ro : 0.0;
}

/* ---- y-Parameter Conversion ---- */

void bjt_y_from_ss(const bjt_ss_params_t *ss, double frequency_hz,
                    bjt_y_params_t *yp)
{
    /* CE y-parameters from hybrid-pi at frequency f:
     *
     * | y11  y12 |   | 1/rpi + jw(Cpi+Cmu)      -jw*Cmu          |
     * | y21  y22 | = | gm - jw*Cmu              1/ro + jw*Cmu    |
     *
     * where w = 2*pi*f.
     *
     * Reference: Gray & Meyer, Ch. 7, "Frequency Response".
     */
    double w, wCpi, wCmu;

    if (!ss || !yp) return;

    w = 2.0 * M_PI * frequency_hz;
    wCpi = w * ss->Cpi;
    wCmu = w * ss->Cmu;

    /* y11 = 1/rpi + jw(Cpi+Cmu). In rectangular: Re=1/rpi, Im=w(Cpi+Cmu). */
    yp->y11 = (ss->rpi > 0.0) ? 1.0 / ss->rpi : 0.0;
    /* We store y-parameters as real for DC analysis; for AC the imaginary
     * part matters. We use a convention where the magnitude represents the
     * admittance of the reactive component at frequency f.
     * y11_magnitude = sqrt((1/rpi)^2 + (w*(Cpi+Cmu))^2).
     * But for simplicity in this library, we store the conductance part
     * and the susceptance needs separate handling. For DC (f=0):
     */
    if (frequency_hz <= 0.0) {
        yp->y11 = (ss->rpi > 0.0) ? 1.0 / ss->rpi : 0.0;
        yp->y12 = 0.0;
        yp->y21 = ss->gm;
        yp->y22 = (ss->ro > 0.0) ? 1.0 / ss->ro : 0.0;
    } else {
        /* For AC, we store the magnitude of each admittance component.
         * This is a simplified representation suitable for gain calculations. */
        double g_pi = (ss->rpi > 0.0) ? 1.0 / ss->rpi : 0.0;
        double g_o  = (ss->ro > 0.0) ? 1.0 / ss->ro : 0.0;
        double b_pi = w * (ss->Cpi + ss->Cmu);
        double b_mu = w * ss->Cmu;

        yp->y11 = sqrt(g_pi * g_pi + b_pi * b_pi);
        yp->y12 = b_mu;   /* jwCmu magnitude */
        yp->y21 = sqrt(ss->gm * ss->gm + b_mu * b_mu);
        yp->y22 = sqrt(g_o * g_o + b_mu * b_mu);
    }
}

/* ---- z-Parameter Conversion ---- */

void bjt_z_from_ss(const bjt_ss_params_t *ss, double frequency_hz,
                    bjt_z_params_t *zp)
{
    /* Compute y-parameters first, then invert the 2x2 matrix.
     * |z11  z12|   | y22  -y12 | / det(Y)
     * |z21  z22| = | -y21  y11 | /
     *
     * det(Y) = y11*y22 - y12*y21
     */
    bjt_y_params_t yp;
    double det;

    if (!ss || !zp) return;

    bjt_y_from_ss(ss, frequency_hz, &yp);

    det = yp.y11 * yp.y22 - yp.y12 * yp.y21;

    if (fabs(det) < 1e-30) {
        /* Singular matrix ? return zero impedance (short circuit) */
        zp->z11 = zp->z12 = zp->z21 = zp->z22 = 0.0;
        return;
    }

    zp->z11 =  yp.y22 / det;
    zp->z12 = -yp.y12 / det;
    zp->z21 = -yp.y21 / det;
    zp->z22 =  yp.y11 / det;
}

/* ---- Frequency Parameters ---- */

double bjt_compute_fT(const bjt_ss_params_t *ss)
{
    /* fT = gm / (2*pi * (Cpi + Cmu)).
     * The transition frequency where |h21(f)| = 1.
     * For the 2N3904: gm ? 38.7 mS at 1 mA,
     *   Cpi+Cmu ? 8 pF => fT ? 38.7e-3/(2*pi*8e-12) ? 770 MHz.
     * Real device: ~300 MHz (due to additional parasitics). */
    double c_total;
    if (!ss || ss->gm <= 0.0) return 0.0;

    c_total = ss->Cpi + ss->Cmu;
    if (c_total <= 0.0) return 1e12;  /* Infinite BW for zero capacitance */

    return ss->gm / (2.0 * M_PI * c_total);
}

double bjt_compute_f_beta(const bjt_ss_params_t *ss)
{
    /* f_beta = fT / beta.
     * This is the -3dB frequency of the short-circuit current gain.
     * For beta = 100 and fT = 300 MHz: f_beta = 3 MHz. */
    if (!ss || ss->beta <= 0.0) return 0.0;
    return ss->fT / ss->beta;
}

/* ---- Gummel-Poon Base Charge ---- */

double bjt_gummel_poon_qb(double vbe, double vbc, double vt,
                           double VAF, double VAR, double IKF, double IKR,
                           double IS, double NF)
{
    /* Gummel-Poon charge-control model introduces the normalized
     * base charge qb that accounts for:
     * 1. Base-width modulation (Early effect): q1 term
     * 2. High-level injection (Kirk effect): q2 term
     *
     * qb = q1/2 + sqrt((q1/2)^2 + q2)
     *
     * where:
     *   q1 = 1 / (1 - VBC/VAR - VBE/VAF)  (Early effect)
     *   q2 = (IS/IKF)*(exp(VBE/(NF*VT))-1) + (IS/IKR)*(exp(VBC/(NR*VT))-1)
     *
     * In forward active (VBE >> 0, VBC << 0):
     *   q1 ? 1 + VCE/VAF
     *   q2 ? IS/IKF * exp(VBE/(NF*VT)) = IC/IKF
     *
     * Simplified: qb ? (1+VCE/VAF)/2 + sqrt(((1+VCE/VAF)/2)^2 + IC/IKF)
     */

    double q1, q2, half_q1;
    double vce = vbe - vbc;

    /* q1: Early effect factor */
    q1 = 1.0;
    if (fabs(VAR) > 1e-6) {
        q1 += vce / VAR;   /* VCB reversal for NPN */
    }
    if (fabs(VAF) > 1e-6) {
        q1 += vce / VAF;
    }
    if (q1 < 0.1) q1 = 0.1;  /* Prevent negative/zero q1 */

    /* q2: High-level injection terms */
    q2 = 0.0;
    if (IKF > 1e-15 && vt > 0.0) {
        double forward_inj = IS * (exp(vbe / (NF * vt)) - 1.0);
        q2 += forward_inj / IKF;
    }
    if (IKR > 1e-15 && vt > 0.0) {
        double reverse_inj = IS * (exp(vbc / (NF * vt)) - 1.0);
        q2 += reverse_inj / IKR;
    }
    if (q2 < 0.0) q2 = 0.0;

    half_q1 = q1 / 2.0;
    return half_q1 + sqrt(half_q1 * half_q1 + q2);
}

/* ---- Junction Capacitance ---- */

double bjt_junction_cap(double cj0, double vj, double v_applied,
                         double mj)
{
    /* Standard SPICE pn-junction capacitance model:
     *
     * For V_applied < FC * VJ:
     *   Cj = CJ0 / (1 - V_applied/VJ)^MJ
     *
     * For V_applied >= FC * VJ (forward bias, avoids singularity):
     *   Cj = CJ0 * (1 - FC)^(-MJ) * (1 - FC*(1+MJ) + MJ*V_applied/VJ)
     *
     * where FC = 0.5 typically (SPICE default).
     *
     * Note: For a reverse-biased junction, V_applied < 0,
     * so (1 - V_applied/VJ) > 1 ? Cj < CJ0.
     */

    double FC = 0.5;
    double arg;

    if (cj0 <= 0.0 || vj <= 0.0) return 0.0;

    if (v_applied < FC * vj) {
        /* Reverse bias / low forward bias region */
        arg = 1.0 - v_applied / vj;
        if (arg <= 0.0) arg = 1e-10;  /* Avoid division by zero */
        return cj0 / pow(arg, mj);
    } else {
        /* High forward bias ? use linear approximation */
        double f1 = pow(1.0 - FC, -mj);
        double f2 = 1.0 - FC * (1.0 + mj);
        double f3 = mj * v_applied / vj;
        return cj0 * f1 * (f2 + f3);
    }
}

/* ---- Utility Conversions ---- */

double bjt_gain_to_db(double gain_linear)
{
    double abs_gain = fabs(gain_linear);
    if (abs_gain < 1e-30) return -600.0;  /* -inf dB, clipped */
    return 20.0 * log10(abs_gain);
}

double bjt_gain_from_db(double gain_db)
{
    return pow(10.0, gain_db / 20.0);
}

/* ---- Base Spreading Resistance Effect ---- */

double bjt_rbb_effect(double rbb, double rpi)
{
    /* The base spreading resistance rbb' is the physical resistance
     * between the external base terminal and the intrinsic base region.
     * It appears in series with rpi:
     *   rpi_total = rbb' + rpi
     *
     * rbb' contributes thermal noise and creates a zero in the
     * frequency response at w = 1/(rbb' * Cmu).
     */
    return rbb + rpi;
}

/* ---- Diffusion Capacitance ---- */

double bjt_diffusion_capacitance(double tf, double gm)
{
    /* Cd = TF * gm = TF * IC/VT.
     * The diffusion capacitance represents charge stored in the base
     * due to minority carriers in transit (forward transit time TF).
     * For TF = 300 ps, IC = 1 mA, VT = 26 mV:
     *   Cd = 3e-10 * 1e-3/2.6e-2 = 1.15e-11 = 11.5 pF.
     * This is typically much larger than the depletion capacitance Cje. */
    if (tf <= 0.0 || gm <= 0.0) return 0.0;
    return tf * gm;
}
