/*  Standalone tests for the FMD filter core.

    Builds without Rack or the Rack SDK -- see fmd_test_shim.hpp. Run with:

        tests/run.sh          (or: make -C tests)

    These check the things that are painful to judge by ear: that the filter is
    numerically stable everywhere on its control surface, that each mode really
    has the frequency response its label claims, and that the parameter/CV maths
    does what the panel promises. When the placeholder DSP is replaced by
    FlowerChildFilterCore, these should keep passing. */

#define FMD_DSP_TEST_SHIM
#include "../src/FmdDsp.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

static const float SR = 48000.f;

static int g_failures = 0;
static int g_checks = 0;

static void check(bool ok, const std::string& what, const std::string& detail = "") {
	g_checks++;
	if (ok) {
		std::printf("  pass  %s\n", what.c_str());
	}
	else {
		g_failures++;
		std::printf("  FAIL  %s%s%s\n", what.c_str(),
			detail.empty() ? "" : "  --  ", detail.c_str());
	}
}

static std::string f2s(float v) {
	char buf[64];
	std::snprintf(buf, sizeof(buf), "%.4g", double(v));
	return buf;
}

static fmd::FilterParams baseParams(int mode, float cutoffHz, float res) {
	fmd::FilterParams p;
	p.freqHz = cutoffHz;
	p.res = res;
	p.drive = 0.f;
	p.spread = 0.f;
	p.grit = 0.f;
	p.clip = 0.f;
	p.mode = mode;
	p.aggressive = false;
	p.gritIsCrunch = false;
	return p;
}

/** Steady-state RMS of the left output for a sine at `testHz`. The first half
of the run is discarded so the filter has settled. */
static float responseRms(int mode, float cutoffHz, float testHz, float res = 0.2f) {
	fmd::FilterCore core;
	core.setSampleRate(SR);
	core.reset();
	fmd::FilterParams p = baseParams(mode, cutoffHz, res);

	const int total = int(SR * 0.5f);
	const int settle = total / 2;
	double acc = 0.0;
	int n = 0;

	for (int i = 0; i < total; i++) {
		float s = 5.f * std::sin(2.f * float(M_PI) * testHz * float(i) / SR);
		float in[2] = {s, s};
		float out[2] = {0.f, 0.f};
		core.process(in, out, p);
		if (i >= settle) {
			acc += double(out[0]) * double(out[0]);
			n++;
		}
	}
	return float(std::sqrt(acc / double(n)));
}


// ---------------------------------------------------------------------------

static void testStability() {
	std::printf("\nnumerical stability across the whole control surface\n");

	const int modes[] = {
		fmd::FilterCore::MODE_LP6, fmd::FilterCore::MODE_LP12,
		fmd::FilterCore::MODE_BP, fmd::FilterCore::MODE_HP,
	};
	const float freqs[] = {20.f, 100.f, 1000.f, 8000.f, 20000.f};
	const float levels[] = {0.f, 0.5f, 1.f};

	int combos = 0;
	float worst = 0.f;
	bool allFinite = true;

	for (int mode : modes)
	for (float freq : freqs)
	for (float res : levels)
	for (float drive : levels)
	for (float grit : levels)
	for (float clip : levels)
	for (int crunch = 0; crunch < 2; crunch++)
	for (int aggr = 0; aggr < 2; aggr++) {
		fmd::FilterCore core;
		core.setSampleRate(SR);
		core.reset();

		fmd::FilterParams p = baseParams(mode, freq, res);
		p.drive = drive;
		p.grit = grit;
		p.clip = clip;
		p.spread = 0.5f;
		p.gritIsCrunch = (crunch != 0);
		p.aggressive = (aggr != 0);
		combos++;

		for (int i = 0; i < 4000; i++) {
			// Hot input plus a periodic full-scale impulse, to provoke ringing.
			float s = 10.f * std::sin(2.f * float(M_PI) * 220.f * float(i) / SR);
			if (i % 977 == 0)
				s += 10.f;
			float in[2] = {s, -s};
			float out[2] = {0.f, 0.f};
			core.process(in, out, p);

			for (int c = 0; c < 2; c++) {
				if (!std::isfinite(out[c]))
					allFinite = false;
				worst = std::max(worst, std::fabs(out[c]));
			}
		}
	}

	check(allFinite, "no NaN or infinity anywhere",
		"swept " + std::to_string(combos) + " parameter combinations");
	check(worst < 200.f, "output stays bounded under a 10 V drive",
		"peak |out| = " + f2s(worst) + " V");
}


static void testFrequencyResponse() {
	std::printf("\neach mode has the response its label claims (cutoff 1 kHz)\n");

	float lpLow = responseRms(fmd::FilterCore::MODE_LP12, 1000.f, 200.f);
	float lpHigh = responseRms(fmd::FilterCore::MODE_LP12, 1000.f, 8000.f);
	check(lpLow > lpHigh * 5.f, "LP12 passes 200 Hz and rejects 8 kHz",
		"200 Hz = " + f2s(lpLow) + " V, 8 kHz = " + f2s(lpHigh) + " V");

	float hpLow = responseRms(fmd::FilterCore::MODE_HP, 1000.f, 200.f);
	float hpHigh = responseRms(fmd::FilterCore::MODE_HP, 1000.f, 8000.f);
	check(hpHigh > hpLow * 5.f, "HP passes 8 kHz and rejects 200 Hz",
		"200 Hz = " + f2s(hpLow) + " V, 8 kHz = " + f2s(hpHigh) + " V");

	float bpLow = responseRms(fmd::FilterCore::MODE_BP, 1000.f, 200.f);
	float bpMid = responseRms(fmd::FilterCore::MODE_BP, 1000.f, 1000.f);
	float bpHigh = responseRms(fmd::FilterCore::MODE_BP, 1000.f, 8000.f);
	check(bpMid > bpLow && bpMid > bpHigh, "BP peaks at its cutoff",
		"200 Hz = " + f2s(bpLow) + ", 1 kHz = " + f2s(bpMid) + ", 8 kHz = " + f2s(bpHigh));

	// One pole is 6 dB/oct against the SVF's 12, so it must let more through.
	float lp6High = responseRms(fmd::FilterCore::MODE_LP6, 1000.f, 8000.f);
	check(lp6High > lpHigh, "LP6 rolls off more gently than LP12",
		"LP6 = " + f2s(lp6High) + " V vs LP12 = " + f2s(lpHigh) + " V");

	// Resonance should lift the region around the cutoff.
	float flat = responseRms(fmd::FilterCore::MODE_LP12, 1000.f, 1000.f, 0.0f);
	float resonant = responseRms(fmd::FilterCore::MODE_LP12, 1000.f, 1000.f, 0.9f);
	check(resonant > flat * 1.5f, "RES emphasises the cutoff frequency",
		"res 0 = " + f2s(flat) + " V, res 0.9 = " + f2s(resonant) + " V");
}


static void testSignalPath() {
	std::printf("\nsignal path behaviour\n");

	// Silence in, silence out -- as long as the noise source is off.
	{
		fmd::FilterCore core;
		core.setSampleRate(SR);
		core.reset();
		fmd::FilterParams p = baseParams(fmd::FilterCore::MODE_LP12, 1000.f, 0.9f);
		float peak = 0.f;
		for (int i = 0; i < 4000; i++) {
			float in[2] = {0.f, 0.f};
			float out[2] = {0.f, 0.f};
			core.process(in, out, p);
			peak = std::max(peak, std::fabs(out[0]));
		}
		check(peak < 1e-6f, "silent input gives a silent output",
			"peak = " + f2s(peak) + " V");
	}

	// NOISE generates signal from silence.
	{
		fmd::FilterCore core;
		core.setSampleRate(SR);
		core.reset();
		fmd::FilterParams p = baseParams(fmd::FilterCore::MODE_LP12, 4000.f, 0.2f);
		p.grit = 0.6f;
		double acc = 0.0;
		for (int i = 0; i < 8000; i++) {
			float in[2] = {0.f, 0.f};
			float out[2] = {0.f, 0.f};
			core.process(in, out, p);
			if (i > 2000)
				acc += double(out[0]) * double(out[0]);
		}
		float rms = float(std::sqrt(acc / 6000.0));
		check(rms > 0.01f, "NOISE produces signal with nothing patched in",
			"rms = " + f2s(rms) + " V");
	}

	// SPREAD must actually decorrelate the two channels.
	{
		fmd::FilterCore core;
		core.setSampleRate(SR);
		core.reset();
		fmd::FilterParams p = baseParams(fmd::FilterCore::MODE_BP, 1000.f, 0.5f);
		p.spread = 0.8f;
		float maxDiff = 0.f;
		for (int i = 0; i < 8000; i++) {
			float s = 5.f * std::sin(2.f * float(M_PI) * 900.f * float(i) / SR);
			float in[2] = {s, s};
			float out[2] = {0.f, 0.f};
			core.process(in, out, p);
			if (i > 2000)
				maxDiff = std::max(maxDiff, std::fabs(out[0] - out[1]));
		}
		check(maxDiff > 0.1f, "SPREAD makes the two channels differ",
			"max |L-R| = " + f2s(maxDiff) + " V");
	}

	// CLIP has to tame a hot signal.
	{
		float openPeak = 0.f, clippedPeak = 0.f;
		for (int pass = 0; pass < 2; pass++) {
			fmd::FilterCore core;
			core.setSampleRate(SR);
			core.reset();
			fmd::FilterParams p = baseParams(fmd::FilterCore::MODE_LP12, 4000.f, 0.2f);
			p.clip = (pass == 0) ? 0.f : 1.f;
			float peak = 0.f;
			for (int i = 0; i < 8000; i++) {
				float s = 5.f * std::sin(2.f * float(M_PI) * 200.f * float(i) / SR);
				float in[2] = {s, s};
				float out[2] = {0.f, 0.f};
				core.process(in, out, p);
				if (i > 2000)
					peak = std::max(peak, std::fabs(out[0]));
			}
			(pass == 0 ? openPeak : clippedPeak) = peak;
		}
		check(clippedPeak < openPeak, "CLIP reduces peak level",
			"open = " + f2s(openPeak) + " V, clipped = " + f2s(clippedPeak) + " V");
	}
}


static void testParameterMapping() {
	std::printf("\nparameter and CV maths\n");

	check(std::fabs(fmd::freqFromOctaves(0.f, 0.f) - 20.f) < 0.01f,
		"FREQ at minimum is 20 Hz",
		f2s(fmd::freqFromOctaves(0.f, 0.f)) + " Hz");

	check(std::fabs(fmd::freqFromOctaves(9.f, 0.f) - 10240.f) < 1.f,
		"FREQ knob is calibrated in octaves",
		f2s(fmd::freqFromOctaves(9.f, 0.f)) + " Hz at 9 octaves");

	check(std::fabs(fmd::freqFromOctaves(5.f, 1.f) - fmd::freqFromOctaves(6.f, 0.f)) < 0.01f,
		"FREQ CV tracks 1 V/octave");

	check(fmd::freqFromOctaves(10.f, 5.f) <= 22000.f,
		"FREQ is clamped below Nyquist at 44.1 kHz",
		f2s(fmd::freqFromOctaves(10.f, 5.f)) + " Hz");

	// The attenuverter gotcha: centred means no CV gets through.
	{
		engine::Input in;
		in.patch(10.f);
		check(std::fabs(fmd::modulated(0.3f, in, 0.f) - 0.3f) < 1e-6f,
			"attenuverter at centre blocks CV entirely");
		check(std::fabs(fmd::modulated(0.f, in, 1.f) - 1.f) < 1e-6f,
			"10 V with the attenuverter open covers the full range");
		check(std::fabs(fmd::modulated(1.f, in, -1.f) - 0.f) < 1e-6f,
			"a negative attenuverter inverts the CV");
	}
	{
		engine::Input unpatched;
		check(std::fabs(fmd::modulated(0.42f, unpatched, 1.f) - 0.42f) < 1e-6f,
			"an unpatched CV input leaves the knob alone");
	}
	{
		engine::Input in;
		in.patch(-50.f);
		float v = fmd::modulated(0.5f, in, 1.f);
		check(v >= 0.f && v <= 1.f, "out-of-range CV is clamped to 0..1",
			"got " + f2s(v));
	}
}


static void testReset() {
	std::printf("\nstate handling\n");

	fmd::FilterCore core;
	core.setSampleRate(SR);
	fmd::FilterParams p = baseParams(fmd::FilterCore::MODE_BP, 800.f, 0.95f);

	// Ring the filter hard, then reset and confirm nothing is left over.
	for (int i = 0; i < 2000; i++) {
		float in[2] = {(i < 10) ? 10.f : 0.f, (i < 10) ? 10.f : 0.f};
		float out[2] = {0.f, 0.f};
		core.process(in, out, p);
	}
	core.reset();

	float peak = 0.f;
	for (int i = 0; i < 500; i++) {
		float in[2] = {0.f, 0.f};
		float out[2] = {0.f, 0.f};
		core.process(in, out, p);
		peak = std::max(peak, std::fabs(out[0]));
	}
	check(peak < 1e-6f, "reset() clears the filter state",
		"residual = " + f2s(peak) + " V");
}


int main() {
	std::printf("FMD filter core tests  (sample rate %.0f Hz)\n", double(SR));

	testStability();
	testFrequencyResponse();
	testSignalPath();
	testParameterMapping();
	testReset();

	std::printf("\n%d checks, %d failed\n", g_checks, g_failures);
	if (g_failures == 0)
		std::printf("OK\n");
	return g_failures == 0 ? 0 : 1;
}
