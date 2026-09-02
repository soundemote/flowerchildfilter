/* Smoke test for the real Flower Child Clean/Dirty DSP (no Rack). */

#include "../src/FlowerChildFilter.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;

static void check(bool ok, const char* what) {
	if (ok)
		std::printf("  pass  %s\n", what);
	else {
		std::printf("  FAIL  %s\n", what);
		g_fail++;
	}
}

int main() {
	using namespace fmd::flower_child;

	const double sr = 48000.0;
	Voice clean, dirty;
	clean.reset(1);
	dirty.reset(2);

	double maxClean = 0.0, maxDirty = 0.0;
	bool finite = true;

	for (int i = 0; i < 48000; i++) {
		double in = 0.5 * std::sin(2.0 * 3.141592653589793 * 220.0 * double(i) / sr);
		double yc = processSample(clean, in, 0.5, 0.7, 0.0, MODE_CLEAN, sr);
		double yd = processSample(dirty, in, 0.5, 0.7, 0.0, MODE_DIRTY, sr);
		if (!std::isfinite(yc) || !std::isfinite(yd))
			finite = false;
		maxClean = std::fabs(yc) > maxClean ? std::fabs(yc) : maxClean;
		maxDirty = std::fabs(yd) > maxDirty ? std::fabs(yd) : maxDirty;
	}

	std::printf("Flower Child DSP smoke\n");
	std::printf("  info  maxClean=%.4g maxDirty=%.4g\n", maxClean, maxDirty);
	check(finite, "Clean and Dirty stay finite for 1s at res=0.7");
	check(maxClean > 1e-4, "Clean produces audible output");
	check(maxDirty > 1e-4, "Dirty produces audible output");
	// Modes must diverge: same input, different oscillator / makeup path.
	check(std::fabs(maxClean - maxDirty) > 1e-3, "Clean and Dirty produce different levels");

	// Feedback needs a kick — zero input from a cold start stays silent
	// (phase increment is frequencyHz * fm, and fm starts at 0). Impulse then
	// high resonance should leave lasting energy in the self-mod path.
	Voice osc;
	osc.reset(3);
	double energy = 0.0;
	for (int i = 0; i < 48000; i++) {
		double in = (i == 0) ? 1.0 : 0.0;
		double y = processSample(osc, in, 0.55, 0.95, 0.0, MODE_CLEAN, sr);
		if (i > 24000)
			energy += y * y;
	}
	check(std::isfinite(energy) && energy > 1e-4, "Clean rings after impulse at high resonance");

	// Stereo core wrapper
	Core core;
	core.setSampleRate(48000.f);
	core.reset();
	float in[2] = {1.f, -1.f};
	float out[2] = {0.f, 0.f};
	bool coreOk = true;
	for (int i = 0; i < 1000; i++) {
		core.process(in, out, 0.4f, 0.3f, 0.1f, 0.2f, 0.5f, 0.25f, false);
		if (!std::isfinite(out[0]) || !std::isfinite(out[1]))
			coreOk = false;
	}
	check(coreOk, "Stereo Core stays finite");

	std::printf("\n%d failure(s)\n", g_fail);
	return g_fail ? 1 : 0;
}
