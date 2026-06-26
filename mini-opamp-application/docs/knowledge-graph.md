# Knowledge Graph — mini-opamp-application
## Nine-Layer Knowledge Coverage

### L1: Definitions (COMPLETE)
- OpAmpParams struct (A_ol, GBW, SR, CMRR, PSRR, V_os, I_bias, noise, poles)
- IdealOpAmp struct (golden rules: infinite A, infinite Z_in, zero Z_out)
- OpAmpPoleZeroModel (s-domain multi-pole model)
- OpAmpNoiseModel (white noise, 1/f flicker noise, current noise)
- AmplifierConfig (inverting, non-inverting, differential, IA, etc.)
- ComparatorConfig (hysteresis thresholds, V_OH, V_OL)
- PrecisionRectifierConfig (half-wave, full-wave, peak detector)
- LogAmpConfig (log/antilog amplifier parameters)
- WaveformGenConfig (triangle/square generator parameters)
- FilterSpec (Butterworth, Chebyshev, Bessel, elliptic)
- FilterStage (biquad stage: w_p, Q_p, K, component values)
- OscillatorConfig (Wien bridge, phase-shift, quadrature)
- StabilityMargins (PM, GM, f_t, f_180)
- CompensationNetwork (Miller, lead/lag compensation)
- FullyDiffAmp (common-mode feedback analysis)
- InstrumentationAmpConfig (three-op-amp IA)
- CompositeAmpConfig (multi-stage composite amplifier)

### L2: Core Concepts (COMPLETE)
- Virtual short / virtual ground principle
- Negative feedback theory (Black 1934)
- Positive feedback and regenerative switching
- Noise gain vs signal gain distinction
- Q-factor and damping in active filters
- Barkhausen oscillation criterion
- Common-mode rejection in differential amplifiers
- Hysteresis for noise immunity (Schmitt trigger)
- Gain-bandwidth trade-off (GBW product)
- Slew rate limitation
- Bootstrapping (input impedance enhancement)
- Miller effect and pole splitting
- Frequency compensation concepts
- Noise figure and SNR degradation

### L3: Mathematical Structures (COMPLETE)
- s-domain transfer functions (pole-zero representation)
- Multi-pole Bode plot generation (log-spaced)
- Biquadratic transfer function coefficients
- Butterworth pole locations (unit circle, LHP)
- Chebyshev pole locations (ellipse in s-plane)
- Butterworth polynomial computation (convolution)
- Phase margin from pole-zero model
- Noise spectral density integration (white + 1/f)
- Wien bridge transfer function analysis
- Phase-shift network transfer function
- Hysteresis loop threshold equations
- Sensitivity functions S_x^y for filter components

### L4: Fundamental Laws (COMPLETE)
- Op-amp golden rules (3 axioms: v+=v-, i+=i-=0, Z_out=0)
- Black's feedback formula: A_cl = A_fwd / (1 + A_ol*beta)
- Gain-bandwidth product constancy: BW_cl = GBW / NG
- Barkhausen criterion: |A*beta| = 1, phase = 0 mod 360 deg
- Nyquist stability criterion: no (-1,j0) encirclement
- Phase margin requirement: PM > 45 deg for good response
- Noise figure: NF = 10*log10(1 + e_ni^2/4kTR_s)
- Johnson-Nyquist noise: e_R = sqrt(4kTR)
- Bandgap voltage reference: V_ref = V_BE + K*V_T
- Feedback impedance reduction: Z_out_cl = Z_out_ol/(1 + T)
- Foster's reactance theorem (filter synthesis basis)
- Sallen-Key Q sensitivity: S_K^Q = K/(3-K)

### L5: Algorithms/Methods (COMPLETE)
- Modified Nodal Analysis (MNA) for op-amp circuits
- Binary search for unity-gain frequency
- Butterworth order computation from attenuation spec
- Butterworth pole generation (left-half plane)
- Chebyshev pole generation (elliptic s-plane)
- Butterworth polynomial convolution
- Biquad coefficient computation for all band types
- Sallen-Key component value synthesis
- MFB (Multiple Feedback) component value synthesis
- State-variable (KHN) filter design
- Cascade biquad synthesis (pole pairing, ordering)
- Resistor value design for target gain
- Schmitt trigger threshold computation
- Precision rectifier diode drop elimination
- Logarithmic amplifier I-V characteristic exploitation
- Wien bridge oscillator design
- Phase-shift oscillator design
- Quadrature oscillator design
- Miller compensation capacitor sizing
- Lead compensation network design
- Noise integration over bandwidth (white + 1/f)
- Noise figure computation at optimal R_source
- IA CMRR estimation with resistor mismatch
- Group delay computation for filter characterization

### L6: Canonical Problems (COMPLETE)
- Inverting amplifier design and analysis
- Non-inverting amplifier design and analysis
- Voltage follower (buffer) analysis
- Differential amplifier with finite CMRR
- Three-op-amp instrumentation amplifier (IA) design
- Summing amplifier (inverting configuration)
- 4th-order Butterworth active low-pass filter
- Sallen-Key low-pass filter stage
- MFB low-pass filter stage
- State-variable universal filter (KHN)
- Comparator with hysteresis (Schmitt trigger)
- Precision half-wave rectifier
- Precision full-wave rectifier (absolute value)
- Precision peak detector
- Logarithmic amplifier (multi-decade compression)
- Wien bridge sine wave oscillator
- Phase-shift sine wave oscillator
- Quadrature oscillator (sin/cos output)
- Triangle/square wave generator
- Miller-compensated two-stage amplifier
- Lead-compensated feedback amplifier

### L7: Applications (COMPLETE — 6 items)
- Biomedical ECG preamplifier (low-noise design)
- Audio microphone preamp (noise optimization)
- Photodiode transimpedance amplifier (TIA)
- Active anti-aliasing filter for ADC (Butterworth LP)
- GPS receiver baseband filter (100 Hz LP)
- Industrial sensor signal conditioning (IA for strain gauge)

### L8: Advanced Topics (COMPLETE — 5 items)
- Nested Miller Compensation (NMC) for three-stage amplifiers
- Chopper-stabilized amplifier analysis (offset/1/f elimination)
- Fully differential amplifier CMFB stability
- Lead/lag compensation network design
- Composite amplifier architecture (precision + high-speed)

### L9: Research Frontiers (PARTIAL — documented)
- Sub-1V op-amp design for IoT (ultra-low power)
- Time-based amplification (VCO-based ADC front-end)
- Memristor-based programmable gain amplifiers
- Neuromorphic amplifier arrays (spike-based)
- Terahertz CMOS amplifiers (documented reference)
