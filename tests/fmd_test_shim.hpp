#pragma once

/*  Stand-ins for the handful of Rack symbols that FmdDsp.hpp uses, so the DSP
    can be compiled and exercised on its own -- no Rack, no SDK, no GUI.

    Only four things are needed: clamp(), random::uniform(), engine::Input and
    M_PI. If the DSP ever grows a new Rack dependency, this file is where the
    build will break, which is a useful signal in itself: the filter core is
    meant to stay portable so it can be swapped for FlowerChildFilterCore. */

#include <algorithm>
#include <cmath>
#include <cstdint>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


/** rack::math::clamp, reachable unqualified as it is inside the plugin. */
inline float clamp(float x, float low, float high) {
	return std::max(low, std::min(high, x));
}

inline int clamp(int x, int low, int high) {
	return std::max(low, std::min(high, x));
}


namespace random {

/** Deterministic replacement for Rack's global RNG, so a failing test can be
reproduced exactly. Numerical Recipes LCG; quality is irrelevant here. */
inline uint32_t& state() {
	static uint32_t s = 22222u;
	return s;
}

inline void seed(uint32_t s) {
	state() = s ? s : 1u;
}

/** Uniform in [0, 1), matching the contract FmdDsp.hpp relies on. */
inline float uniform() {
	state() = state() * 1664525u + 1013904223u;
	return float(state() >> 8) / float(1u << 24);
}

} // namespace random


namespace engine {

/** Just enough of rack::engine::Input for fmd::modulated(). */
struct Input {
	float voltage = 0.f;
	bool connected = false;

	bool isConnected() const {
		return connected;
	}

	float getVoltage() const {
		return voltage;
	}

	void patch(float v) {
		voltage = v;
		connected = true;
	}
};

} // namespace engine
