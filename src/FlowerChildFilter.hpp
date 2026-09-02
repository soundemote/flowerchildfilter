#pragma once

// Flower Child Filter — Clean (Rev1) and Dirty (Rev2) voices.
// Ported from soemdsp-sandbox/native_modules/flower_child_filter/flower_child_filter.cpp
// (modes 0 and 1 only). Uses the same sandbox maths helpers for curve / trig / pitch.

// Only the maths the Clean/Dirty path needs — not the full sandbox umbrella.
#include "vendor/sandbox_native_maths/scalar_helpers.h"
#include "vendor/sandbox_native_maths/exp_log.h"
#include "vendor/sandbox_native_maths/graph.h"
#include "vendor/sandbox_native_maths/analog_filter_trig.h"

#include <cmath>

namespace fmd {
namespace flower_child {

enum Mode {
	MODE_CLEAN = 0,  // Rev1 — plain sine feedback oscillator
	MODE_DIRTY = 1,  // Rev2 — ellipse waveshaper, harder makeup (AGGR)
};

struct Voice {
	double phase = 0.0;
	double phaseOffset = 0.0;
	double stage1Y1 = 0.0;
	double stage2Y1 = 0.0;
	double selfMod = 0.0;
	unsigned int rngState = 0x9E3779B9u;

	void reset(unsigned int seed = 0x9E3779B9u) {
		phase = 0.0;
		phaseOffset = 0.0;
		stage1Y1 = 0.0;
		stage2Y1 = 0.0;
		selfMod = 0.0;
		rngState = seed ? seed : 0x9E3779B9u;
	}
};

namespace detail {

static inline double dsp_sqrt(double x) {
	if (x <= 0.0)
		return 0.0;
	double guess = x;
	for (int i = 0; i < 24; i++)
		guess = 0.5 * (guess + x / guess);
	return guess;
}

static inline double clampd(double v, double lo, double hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}

static inline double jmap01(double v, double outMin, double outMax) {
	return outMin + (outMax - outMin) * v;
}

static inline double curveShape(double v, double tension) {
	double t = tension;
	double denom = 2.0 * t * v - t - 1.0;
	if (denom == 0.0)
		return v;
	return (t * v - v) / denom;
}

static inline double evalResonanceGraph(double x, double n0y, double breakpoint, double n2y, double skew) {
	soemdsp_maths::Graph g;
	g.addNode(0.0, n0y, 0.0, soemdsp_maths::Graph::Shape::LINEAR);
	g.addNode(breakpoint, n0y, 0.0, soemdsp_maths::Graph::Shape::LINEAR);
	g.addNode(1.0, n2y, skew, soemdsp_maths::Graph::Shape::RATIONAL);
	return g.getValue(x);
}

static inline double pitchToFreq(double pitch) {
	return 440.0 * soemdsp_maths::dsp_exp2((pitch - 69.0) / 12.0);
}

static inline double waveSine(double phase) {
	return soemdsp_maths::dsp_sin(phase * soemdsp_maths::kTwoPi);
}

static inline double waveEllipse(double phase, double ellipseC) {
	double sinX = soemdsp_maths::dsp_sin(phase * soemdsp_maths::kTwoPi);
	double cosX = soemdsp_maths::dsp_cos(phase * soemdsp_maths::kTwoPi);
	double sqrtVal = dsp_sqrt(cosX * cosX + (ellipseC * sinX) * (ellipseC * sinX));
	if (sqrtVal < 1e-12)
		sqrtVal = 1e-12;
	return cosX / sqrtVal;
}

static inline double nextNoiseBipolar(unsigned int* state) {
	unsigned int x = *state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	*state = x;
	return ((double)x / 4294967295.0) * 2.0 - 1.0;
}

static inline double onePoleCoefficient(double cutoffHz, double sampleRate) {
	double rawWc = soemdsp_maths::kTwoPi * cutoffHz / sampleRate;
	double wc = clampd(rawWc, 1e-9, soemdsp_maths::kPi * 0.98);
	double s = soemdsp_maths::dsp_sin_0_pi(wc);
	double c = soemdsp_maths::dsp_cos_0_pi(wc);
	double t = soemdsp_maths::dsp_tan_neg_halfquarter(0.25 * (wc - soemdsp_maths::kPi));
	double denom = s - c * t;
	if (denom > -1e-12 && denom < 1e-12)
		denom = (denom >= 0.0) ? 1e-12 : -1e-12;
	return t / denom;
}

static inline double onePoleStep(double* y1, double input, double a) {
	double y0 = input;
	y0 = y0 / (1.0 + y0 * y0);
	*y1 = y0 + a * (y0 - *y1);
	return *y1;
}

} // namespace detail


/** One mono voice of the original Flower Child filter (Clean or Dirty).
    Parameters match the sandbox C API: frequency/resonance/chaos in 0..1. */
inline double processSample(
	Voice& s,
	double input,
	double frequency,
	double resonance,
	double chaosAmount,
	Mode mode,
	double sampleRate
) {
	using namespace detail;

	const double safeRate = sampleRate < 1.0 ? 44100.0 : sampleRate;
	const double freqNorm = clampd(frequency, 0.0, 1.0);
	const double reso = clampd(resonance, 0.0, 1.0);
	const double chaos = clampd(chaosAmount, 0.0, 1.0);
	const bool dirty = mode != MODE_CLEAN;

	const double maxNormFreq = safeRate <= 44100.0 ? 0.928 : 1.0;
	const double normalizedFreqInUse = jmap01(freqNorm < maxNormFreq ? freqNorm : maxNormFreq, 3.0, 161.0);
	const double frequencyHz = pitchToFreq(normalizedFreqInUse);

	// FM/PM crossfade is always 0 in the original (curve domain clamp) — pure FM.
	const double cutoff1 = frequencyHz * 0.164312;
	const double cutoff2 = frequencyHz * 0.366131;
	const double a1 = onePoleCoefficient(cutoff1, safeRate);
	const double a2 = onePoleCoefficient(cutoff2, safeRate);

	double breakpoint, cap;
	if (dirty) {
		if (safeRate <= 44100.0) { breakpoint = 0.816054; cap = 0.602339; }
		else if (safeRate <= 88200.0) { breakpoint = 0.902657; cap = 0.654971; }
		else { breakpoint = 0.977649; cap = 0.760234; }
	}
	else {
		if (safeRate <= 44100.0) { breakpoint = 0.732441; cap = 0.649123; }
		else if (safeRate <= 88200.0) { breakpoint = 0.816054; cap = 0.818713; }
		else { breakpoint = 0.879599; cap = 0.807018; }
	}
	const double cappedTarget = reso < cap ? reso : cap;

	double selfModAmp = 1.0;
	double ellipseC = -1.0;
	if (!dirty) {
		const double graphValue = evalResonanceGraph(reso, reso, breakpoint, cappedTarget, -0.38);
		selfModAmp = jmap01(curveShape(graphValue, 0.4), 0.0368, 0.6333);
	}
	else {
		const double graphValue = evalResonanceGraph(freqNorm, reso, breakpoint, cappedTarget, -0.38);
		ellipseC = jmap01(curveShape(graphValue, -0.6), -1.0, 0.00001);
	}

	const double clampLimit = dirty ? 1.198 : 1.0;
	double inputSignal = clampd(-input, -clampLimit, clampLimit);

	if (chaos > 0.0)
		inputSignal += nextNoiseBipolar(&s.rngState) * chaos;

	inputSignal = s.selfMod + 0.035848699999999845 * inputSignal;

	const double mod = 1.4 * inputSignal;
	const double fm = mod;

	s.phaseOffset = 0.0;
	const double incAmt = (frequencyHz * fm) / safeRate;
	s.phase = s.phase + incAmt;
	s.phase = s.phase - soemdsp_maths::dsp_floor(s.phase);
	double unipolarPhase = s.phase + s.phaseOffset;
	unipolarPhase = unipolarPhase - soemdsp_maths::dsp_floor(unipolarPhase);

	double oscValue = dirty
		? waveEllipse(unipolarPhase, ellipseC) * 0.1
		: waveSine(unipolarPhase) * 1.3;

	double out = onePoleStep(&s.stage1Y1, oscValue, a1);
	out = onePoleStep(&s.stage2Y1, out, a2);

	s.selfMod = dirty ? out * 0.465 : out * selfModAmp;

	return dirty ? out * 5.22 : out * 1.31;
}


/** Stereo core used by the VCV Flower Child module. */
struct Core {
	Voice voice[2];
	float sampleRate = 44100.f;

	void setSampleRate(float sr) {
		sampleRate = (sr > 0.f) ? sr : 44100.f;
	}

	void reset() {
		voice[0].reset(0x9E3779B9u);
		voice[1].reset(0x9E3779B9u + 2654435761u);
	}

	/** Process one stereo frame.
	    freqNorm / res / chaos are 0..1 (original slider domain).
	    drive scales the audio fed into the DSP (panel knob; not in the original).
	    spread detunes the two channels' frequency norms (panel knob).
	    clipAmount soft-limits the Rack-voltage output (panel knob).
	    dirty selects Dirty/AGGR vs Clean.
	    in/out are Rack audio voltages (nominally ±5 V). */
	void process(
		const float in[2],
		float out[2],
		float freqNorm,
		float res,
		float chaos,
		float drive,
		float spread,
		float clipAmount,
		bool dirty
	) {
		Mode mode = dirty ? MODE_DIRTY : MODE_CLEAN;

		// Original DSP expects roughly ±1 audio; Rack cables are ±5 V.
		auto clampf = [](float v, float lo, float hi) {
			return v < lo ? lo : (v > hi ? hi : v);
		};

		const float driveGain = 0.2f + clampf(drive, 0.f, 1.f) * 1.8f;
		const float spreadAmt = clampf(spread, 0.f, 1.f) * 0.15f;

		for (int c = 0; c < 2; c++) {
			float freqC = freqNorm + (c == 0 ? -spreadAmt : spreadAmt);
			freqC = clampf(freqC, 0.f, 1.f);

			double x = double(in[c]) / 5.0 * double(driveGain);
			double y = processSample(
				voice[c],
				x,
				double(freqC),
				double(clampf(res, 0.f, 1.f)),
				double(clampf(chaos, 0.f, 1.f)),
				mode,
				double(sampleRate));

			float v = float(y) * 5.f;

			// Panel CLIP — soft ceiling after the real DSP, not a substitute for it.
			float clip = clampf(clipAmount, 0.f, 1.f);
			if (clip > 0.f) {
				float lim = 10.f * (1.f - 0.85f * clip);
				if (lim < 0.5f)
					lim = 0.5f;
				v = lim * std::tanh(v / lim);
			}

			if (!std::isfinite(v))
				v = 0.f;
			out[c] = v;
		}
	}
};

} // namespace flower_child
} // namespace fmd
