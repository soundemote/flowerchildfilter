Soundemote VCV Rack collection. 1 module so far: Superlove Filter

## Superlove Filter

![Superlove](docs/superlove.png)

Superlove Filter uses a hand crafted "Phase-feedback wavetable" algorithm that recreates the style of a boutique MS20 clone with Doepfer Xtreme amounts of distortion. Using the lowpass filter on a sawtooth makes for good basses. Filter with medium drive and resonance for growling lows and smooth chirpy highs. Lower the resonance and turn up the drive for warmth and saturation. The bandpass and highpass filters are a different beast going quickly into screaming self oscillation, but if dialed carefully these modes allow for musical leads with an organic quality. This filter will feel at home in 80s and 90s music. Superlove algorithm is different from most filters. At the heart of the LP18/LP24 algorithm sits a soft-edged triangle waveshaper. The soft edge prevents aliasing and is responsible for Superlove's growly texture.

![Superlove signal flow](docs/superlove-signal.svg)

| | |
| --- | --- |
| **FREQ** | Cutoff. 1V/oct on the FREQ jack. |
| **RES** | Feedback into triangle wavetable phase. Self-oscillates in all modes. Screams in HP / BP. |
| **DRIVE** | Input gain (0×–4×). Noon is 1×. Default 0.5×. |
| **NOISE** | Noise into the filter. |
| **SPREAD** | Stereo frequency offset. |
| **MODES** | LP18, LP24, BP, HP. |

IN 1 only is mono (same signal on both outs). IN 1 and IN 2 is stereo. SPREAD applies in stereo. CLIP: driven input (jack × Drive) over ±10 V is 0.25 (blue); output over ±10 V is 1.0 (red); both clamp at 1.0.

## Audio Demos

* https://youtu.be/36fkv5bXsv0 Bandpass filter max resonance
* https://youtu.be/0YeF2PfKqDg Highpass filter various settings

## Building

You need the [Rack SDK](https://vcvrack.com/downloads/) and a MinGW-w64 toolchain (Windows), or the standard toolchain for your OS.

```sh
export RACK_DIR=/path/to/Rack-SDK
make
make install
```

`make dist` writes `dist/FMD/` and a `.vcvplugin` next to it. Copy either into the Rack plugins directory (`%LOCALAPPDATA%\Rack2\plugins-win-x64\` on Windows) if you skip `make install`.

## License

Proprietary. The plugin package includes the [VCV Rack plugin EULA](https://vcvrack.com/eula.md) as `LICENSE-VCV.md`.
