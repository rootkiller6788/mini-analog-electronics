# Course Tree - mini-bjt-amplifier

## Prerequisite Chain

```
Semiconductor Physics
  → BJT Device Physics (Ebers-Moll, Hybrid-pi)
    → DC Bias Analysis
    → Small-Signal Models
    → Frequency Response
      → Amplifier Configurations (CE, CC, CB, Cascode)
        → Advanced Topics (Differential, Darlington, Multistage, Noise)
          → Applications (Audio, Sensor, RF, Instrumentation, LNA)
```

## Internal Dependencies
- bjt_model.h/c → Foundation (all modules depend)
- bjt_dc_bias.h/c → bjt_model
- bjt_small_signal.h/c → bjt_model
- bjt_frequency.h/c → bjt_model, bjt_small_signal
- bjt_amplifier.h/c → all above
- bjt_noise.h/c → bjt_model

## External Dependencies
- KVL/KCL → mini-circuit-analysis
- Complex numbers → mini-signal-system-theory
- math.h → Standard C library
