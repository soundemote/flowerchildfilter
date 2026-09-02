# Filter core tests

Standalone tests for `src/FmdDsp.hpp`. They need **only a C++ compiler** — no
Rack, no Rack SDK, no GUI, no audio device.

```sh
cd tests
make          # builds and runs
make clean
```

Exit code is 0 when everything passes, 1 otherwise, so it drops straight into
CI or a pre-commit hook.

## How it builds without Rack

`FmdDsp.hpp` uses exactly four things from Rack: `clamp()`, `random::uniform()`,
`engine::Input` and `M_PI`. `fmd_test_shim.hpp` supplies stand-ins, and
`FmdDsp.hpp` picks it up when `FMD_DSP_TEST_SHIM` is defined:

```cpp
#ifdef FMD_DSP_TEST_SHIM
#include "fmd_test_shim.hpp"
#else
#include "plugin.hpp"
#endif
```

The shim's RNG is a fixed-seed LCG, so a failing run reproduces exactly rather
than depending on Rack's global random state.

If the DSP ever grows a fifth Rack dependency, this build breaks — which is the
point. The filter core is meant to stay portable so `FlowerChildFilterCore` can
drop into `FilterCore::process()` without dragging the panel code along.

## What is covered

| Group | Checks |
|---|---|
| **Numerical stability** | Sweeps 4 modes × 5 cutoffs × resonance, drive, grit and clip × crunch × aggressive — 2160 combinations, each fed a 10 V sine with periodic full-scale impulses. Asserts no NaN or infinity anywhere and that output stays bounded. |
| **Frequency response** | LP12 passes lows and rejects highs; HP does the reverse; BP peaks at its cutoff; LP6 rolls off more gently than LP12; RES lifts the cutoff region. Measured as steady-state RMS of a real sine sweep, not by inspecting coefficients. |
| **Signal path** | Silence in gives silence out; NOISE generates signal from nothing; SPREAD decorrelates the channels; CLIP lowers peak level. |
| **Parameter and CV maths** | FREQ bottoms at 20 Hz, is calibrated in octaves, tracks 1 V/oct and clamps below Nyquist. Attenuverters: centre blocks CV completely, full open covers the range, negative inverts, unpatched leaves the knob alone, out-of-range CV clamps. |
| **State** | `reset()` fully clears a hard-ringing filter. |

21 checks total.

## When the real DSP lands

These tests describe the **control surface**, not the placeholder implementation
— cutoff means cutoff, resonance emphasises, 1 V is an octave, a centred
attenuverter passes nothing. They should keep passing once
`FlowerChildFilterCore` replaces the placeholder.

Two that may legitimately need adjusting:

- *LP6 rolls off more gently than LP12* assumes a 6 dB/oct and a 12 dB/oct mode
  exist. Only Super Love exposes both.
- *output stays bounded* asserts a peak under 200 V. A self-oscillating design
  may want a different ceiling, but the assertion should stay — an unbounded
  filter is a bug regardless of algorithm.
