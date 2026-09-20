#pragma once
#include "plugin.hpp"

namespace fmd {

/** Sums a 0..1 knob with an attenuverted CV input. 10 V covers the full range. */
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
