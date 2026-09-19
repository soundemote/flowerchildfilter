#pragma once

// This header needs only four things from Rack: clamp(), random::uniform(),
// engine::Input and M_PI. tests/ supplies stand-ins for them so the DSP can be
// compiled and exercised without Rack -- see tests/README.md.
#ifdef FMD_DSP_TEST_SHIM
#include "fmd_test_shim.hpp"
#else
#include "plugin.hpp"
#endif

#include <cmath>

/*  PLACEHOLDER DSP for Shaped Resonator and Super Love --------------------

    Flower Child no longer uses this file — see FlowerChildFilter.hpp for the
    real Clean/Dirty port from soemdsp-sandbox.

    What remains below is a stand-in TPT state-variable filter so the other two
    modules keep audible knobs until their sandbox DSP is wired the same way.

    ------------------------------------------------------------------------ */

namespace fmd {


/** Everything one sample of filtering needs, after knobs and CV are summed. */
struct FilterParams {
	/** Centre frequency in Hz, already offset by the FREQ CV. */
	float freqHz = 1000.f;
	/** Resonance, 0..1. */
	float res = 0.f;
	/** Input drive, 0..1. */
	float drive = 0.f;
	/** Stereo cutoff offset in octaves at full travel, bipolar -1..1 (0 = none). */
	float spread = 0.f;
	/** Dirt: injected noise on Flower Child / Super Love, resonance-path
	    nonlinearity on Shaped Resonator. 0..1. */
	float grit = 0.f;
	/** Output soft clipper amount, 0..1. */
	float clip = 0.f;
	/** Response, see FilterCore::Mode. */
	int mode = 1;
	/** Flower Child's AGGR switch: harder saturation in the resonance path. */
	bool aggressive = false;
	/** Shaped Resonator drives its grit through the resonance path instead of
	    mixing noise into the input. */
	bool gritIsCrunch = false;
};


/** Topology-preserving-transform state variable filter (Zavalishin / Cytomic).
Stable at any cutoff and resonance, and gives LP, BP and HP from one pass. */
struct Svf {
	float ic1eq = 0.f;
	float ic2eq = 0.f;
	float g = 0.f;
	float k = 1.f;
	float a1 = 0.f, a2 = 0.f, a3 = 0.f;

	void reset() {
		ic1eq = 0.f;
		ic2eq = 0.f;
	}

	void setParams(float cutoffHz, float q, float sampleRate) {
		float nyquist = sampleRate * 0.5f;
		cutoffHz = clamp(cutoffHz, 5.f, nyquist * 0.98f);
		g = std::tan(float(M_PI) * cutoffHz / sampleRate);
		k = 1.f / std::max(q, 0.02f);
		a1 = 1.f / (1.f + g * (g + k));
		a2 = g * a1;
		a3 = g * a2;
	}

	/** Returns low, band and high outputs for one input sample. */
	void process(float in, float& lp, float& bp, float& hp) {
		float v3 = in - ic2eq;
		float v1 = a1 * ic1eq + a2 * v3;
		float v2 = ic2eq + a2 * ic1eq + a3 * v3;
		ic1eq = 2.f * v1 - ic1eq;
		ic2eq = 2.f * v2 - ic2eq;

		bp = v1;
		lp = v2;
		hp = in - k * v1 - v2;

		// Keep the integrators finite if a host feeds NaN or a denormal storm.
		if (!std::isfinite(ic1eq) || !std::isfinite(ic2eq))
			reset();
	}
};


/** One-pole low pass, used for the 6 dB/oct mode. */
struct OnePole {
	float z = 0.f;
	float a = 0.f;

	void reset() {
		z = 0.f;
	}

	void setCutoff(float cutoffHz, float sampleRate) {
		float g = std::tan(float(M_PI) * clamp(cutoffHz, 5.f, sampleRate * 0.49f) / sampleRate);
		a = g / (1.f + g);
	}

	float process(float in) {
		z += a * (in - z);
		if (!std::isfinite(z))
			z = 0.f;
		return z;
	}
};


/** Stereo filter voice shared by all three modules. */
struct FilterCore {
	enum Mode {
		MODE_LP6,
		MODE_LP12,
		MODE_BP,
		MODE_HP,
		NUM_MODES
	};

	Svf svf[2];
	OnePole onePole[2];
	float sampleRate = 44100.f;

	void setSampleRate(float sr) {
		sampleRate = (sr > 0.f) ? sr : 44100.f;
	}

	void reset() {
		for (int c = 0; c < 2; c++) {
			svf[c].reset();
			onePole[c].reset();
		}
	}

	/** Soft saturator, roughly unity at low levels. */
	static float saturate(float x, float amount) {
		float gain = 1.f + amount * 24.f;
		return std::tanh(x * gain) / std::sqrt(gain);
	}

	/** Symmetric soft clipper for the output stage. */
	static float softClip(float x, float amount) {
		if (amount <= 0.f)
			return x;
		float gain = 1.f + amount * 8.f;
		return std::tanh(x * gain) / std::tanh(gain) * (1.f + amount * 0.5f);
	}

	void process(const float in[2], float out[2], const FilterParams& p) {
		// Resonance 0..1 maps to Q 0.5..25, exponentially so the top of the
		// knob is where self-oscillation lives.
		float q = 0.5f * std::pow(50.f, clamp(p.res, 0.f, 1.f));

		// SPREAD detunes the two channels by up to +/- one octave (bipolar; 0 = none).
		float spreadOct = clamp(p.spread, -1.f, 1.f);
		float freq[2] = {
			p.freqHz * std::pow(2.f, -spreadOct),
			p.freqHz * std::pow(2.f, spreadOct),
		};

		for (int c = 0; c < 2; c++) {
			svf[c].setParams(freq[c], q, sampleRate);
			onePole[c].setCutoff(freq[c], sampleRate);

			float x = in[c];

			// Flower Child / Super Love mix noise into the filter input.
			if (p.grit > 0.f && !p.gritIsCrunch)
				x += (random::uniform() * 2.f - 1.f) * p.grit * p.grit * 2.f;

			if (p.drive > 0.f)
				x = saturate(x, p.drive);

			float lp, bp, hp;
			svf[c].process(x, lp, bp, hp);

			// Shaped Resonator's CRNCH, and Flower Child's AGGR, bend the
			// resonant peak rather than the input.
			if (p.gritIsCrunch && p.grit > 0.f)
				bp = std::tanh(bp * (1.f + p.grit * 12.f)) * (1.f - p.grit * 0.4f);
			if (p.aggressive)
				bp = std::tanh(bp * 2.f);

			float y;
			switch (p.mode) {
				case MODE_LP6:  y = onePole[c].process(x); break;
				case MODE_BP:   y = bp; break;
				case MODE_HP:   y = hp; break;
				case MODE_LP12:
				default:        y = lp; break;
			}

			// Fold the reshaped band output back in so CRNCH/AGGR are audible
			// in the low and high modes too.
			if (p.mode != MODE_BP && (p.aggressive || (p.gritIsCrunch && p.grit > 0.f)))
				y += bp * 0.25f * (p.aggressive ? 1.f : p.grit);

			out[c] = softClip(y, clamp(p.clip, 0.f, 1.f));

			if (!std::isfinite(out[c]))
				out[c] = 0.f;
		}
	}
};


/** The FREQ knob is calibrated in octaves above 20 Hz (0..10 spans 20 Hz to
20.5 kHz), so CV can simply be added as volts per octave. */
inline float freqFromOctaves(float octaves, float cvVolts) {
	return clamp(20.f * std::pow(2.f, octaves + cvVolts), 5.f, 22000.f);
}


/** Sums a 0..1 knob with an attenuverted CV input. 10 V covers the full range.
Rack's Port accessors are not const, so the input is taken by mutable ref. */
inline float modulated(float knob, engine::Input& input, float attenuverter) {
	float v = knob;
	if (input.isConnected())
		v += input.getVoltage() * 0.1f * attenuverter;
	return clamp(v, 0.f, 1.f);
}


/** Same as modulated(), but for bipolar -1..1 knobs (center = none). */
inline float modulatedBipolar(float knob, engine::Input& input, float attenuverter) {
	float v = knob;
	if (input.isConnected())
		v += input.getVoltage() * 0.1f * attenuverter;
	return clamp(v, -1.f, 1.f);
}


} // namespace fmd
