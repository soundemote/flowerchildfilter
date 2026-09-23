#include "plugin.hpp"
#include "SuperLoveFilter.hpp"
#include "FmdCv.hpp"
#include "FmdWidgets.hpp"

#include <cmath>

namespace {
// Knob 0…1 → gain 0…4×. CCW = 0, noon = 1×, CW = 4×. Default 0.5×.
float driveGainFromKnob(float t) {
	t = clamp(t, 0.f, 1.f);
	return 4.f * t * t;
}
float driveKnobFromGain(float g) {
	g = clamp(g, 0.f, 4.f);
	return std::sqrt(g * 0.25f);
}
}

struct SuperLoveDriveQuantity : ParamQuantity {
	float getDisplayValue() override {
		return driveGainFromKnob(getValue());
	}
	void setDisplayValue(float v) override {
		setValue(driveKnobFromGain(v));
	}
};

/*  Super Love -- 12 HP stereo LP18 / LP24 / HP / BP filter (Superlove Rev2 DSP).

    Chaos is fixed at 0 (no Chaos control). Panel NOISE → Rev2 noise inject
    (0…1 knob maps to 0…2 amplitude).

    Control positions come from
    `image assets/Super Love/260731 Super Potions.svg` (716 x 1518, i.e. 4x the
    179 x 380 panel). That file marks 21 crosshairs but omits the SPREAD
    attenuverter; the panel artwork clearly prints a "- +" pair for it opposite
    the RES attenuverter, so it is mirrored about the panel centre
    (179 - 23.94 = 155.06). Everything else is used verbatim. */

struct SuperLove : Module {
	enum ParamId {
		FREQ_PARAM,
		RES_PARAM,
		NOISE_PARAM,
		DRIVE_PARAM,
		SPREAD_PARAM,
		MODE_PARAM,
		CLIP_PARAM, // kept for patch param-ID stability; unused (clip is LED-only)
		RES_CV_PARAM,
		NOISE_CV_PARAM,
		FREQ_CV_PARAM,
		DRIVE_CV_PARAM,
		SPREAD_CV_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		RES_INPUT,
		NOISE_INPUT,
		FREQ_INPUT,
		DRIVE_INPUT,
		SPREAD_INPUT,
		IN_L_INPUT,
		IN_R_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		OUT_L_OUTPUT,
		OUT_R_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		CLIP_LIGHT,       // R  E10035 → 33001F → 000227 → off
		CLIP_LIGHT_GREEN, // G
		CLIP_LIGHT_BLUE,  // B
		LIGHTS_LEN
	};

	fmd::super_love::Core core;
	/** Clip indicator envelope 1→0, linear decay over clipLightSlewSec. */
	float clipLightEnv = 0.f;
	// Clip light: half as sensitive as the previous ±5 V trip (now ±10 V).
	static constexpr float clipLightThresh = 10.f;
	static constexpr float clipLightSlewSec = 0.3f;

	SuperLove() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		configParam(FREQ_PARAM, 0.f, 10.f, 6.f, "Frequency", " Hz", 2.f, 20.f);
		configParam(RES_PARAM, 0.f, 1.f, 0.3f, "Resonance", "%", 0.f, 100.f);
		configParam(NOISE_PARAM, 0.f, 1.f, 0.f, "Noise", "%", 0.f, 100.f);
		configParam<SuperLoveDriveQuantity>(DRIVE_PARAM, 0.f, 1.f, std::sqrt(0.125f), "Drive", "x");
		configParam(SPREAD_PARAM, -1.f, 1.f, 0.f, "Spread", "%", 0.f, 100.f);
		// CLIP_PARAM slot retained so later param IDs stay stable; no widget / no soft-clip.
		configParam(CLIP_PARAM, 0.f, 1.f, 0.f, "Clip");

		// Printed on the panel above the slider, left to right: LP18 / LP24 / HP / BP.
		configSwitch(MODE_PARAM, 0.f, 3.f, 1.f, "Mode", {"LP18", "LP24", "HP", "BP"});

		configParam(RES_CV_PARAM, -1.f, 1.f, 0.f, "Resonance CV", "%", 0.f, 100.f);
		configParam(NOISE_CV_PARAM, -1.f, 1.f, 0.f, "Noise CV", "%", 0.f, 100.f);
		configParam(FREQ_CV_PARAM, -1.f, 1.f, 0.f, "Frequency CV", "%", 0.f, 100.f);
		configParam(DRIVE_CV_PARAM, -1.f, 1.f, 0.f, "Drive CV", "%", 0.f, 100.f);
		configParam(SPREAD_CV_PARAM, -1.f, 1.f, 0.f, "Spread CV", "%", 0.f, 100.f);

		configInput(RES_INPUT, "Resonance CV");
		configInput(NOISE_INPUT, "Noise CV");
		configInput(FREQ_INPUT, "Frequency 1V/oct");
		configInput(DRIVE_INPUT, "Drive CV");
		configInput(SPREAD_INPUT, "Spread CV");
		configInput(IN_L_INPUT, "Left audio");
		configInput(IN_R_INPUT, "Right audio");

		configOutput(OUT_L_OUTPUT, "Left audio");
		configOutput(OUT_R_OUTPUT, "Right audio");

		configLight(CLIP_LIGHT, "Clip");

		configBypass(IN_L_INPUT, OUT_L_OUTPUT);
		configBypass(IN_R_INPUT, OUT_R_OUTPUT);
	}

	void processBypass(const ProcessArgs& args) override {
		(void) args;
		const bool stereo = inputs[IN_L_INPUT].isConnected() && inputs[IN_R_INPUT].isConnected();
		float v = 0.f;
		if (stereo) {
			outputs[OUT_L_OUTPUT].setVoltage(inputs[IN_L_INPUT].getVoltage());
			outputs[OUT_R_OUTPUT].setVoltage(inputs[IN_R_INPUT].getVoltage());
			return;
		}
		if (inputs[IN_L_INPUT].isConnected())
			v = inputs[IN_L_INPUT].getVoltage();
		else if (inputs[IN_R_INPUT].isConnected())
			v = inputs[IN_R_INPUT].getVoltage();
		outputs[OUT_L_OUTPUT].setVoltage(v);
		outputs[OUT_R_OUTPUT].setVoltage(v);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		core.setSampleRate(e.sampleRate);
		core.reset();
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		core.reset();
		clipLightEnv = 0.f;
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "driveMap", json_integer(3));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* mapJ = json_object_get(rootJ, "driveMap");
		int map = mapJ ? (int) json_integer_value(mapJ) : 0;
		if (map >= 3)
			return;
		float g;
		if (map == 2) {
			float t = clamp(params[DRIVE_PARAM].getValue(), 0.f, 1.f);
			g = 0.25f * std::exp2(4.f * t);
		} else {
			g = params[DRIVE_PARAM].getValue();
		}
		params[DRIVE_PARAM].setValue(driveKnobFromGain(g));
	}

	static void clipLedRgb(float e, float* r, float* g, float* b) {
		const float stops[][4] = {
			{1.00f, 0xE1 / 255.f, 0x00 / 255.f, 0x35 / 255.f},
			{0.35f, 0x33 / 255.f, 0x00 / 255.f, 0x1F / 255.f},
			{0.10f, 0x00 / 255.f, 0x02 / 255.f, 0x27 / 255.f},
			{0.00f, 0.f,          0.f,          0.f},
		};
		*r = *g = *b = 0.f;
		if (e >= stops[0][0]) {
			*r = stops[0][1]; *g = stops[0][2]; *b = stops[0][3];
			return;
		}
		for (int i = 0; i < 3; i++) {
			if (e >= stops[i + 1][0]) {
				const float span = stops[i][0] - stops[i + 1][0];
				const float t = (span > 0.f) ? (e - stops[i + 1][0]) / span : 0.f;
				*r = stops[i + 1][1] + t * (stops[i][1] - stops[i + 1][1]);
				*g = stops[i + 1][2] + t * (stops[i][2] - stops[i + 1][2]);
				*b = stops[i + 1][3] + t * (stops[i][3] - stops[i + 1][3]);
				return;
			}
		}
	}

	void process(const ProcessArgs& args) override {
		if (core.sampleRate != args.sampleRate)
			core.setSampleRate(args.sampleRate);

		float freqNorm = clamp(params[FREQ_PARAM].getValue() / 10.f, 0.f, 1.f);
		if (inputs[FREQ_INPUT].isConnected())
			freqNorm = clamp(freqNorm + inputs[FREQ_INPUT].getVoltage() * 0.1f * params[FREQ_CV_PARAM].getValue(), 0.f, 1.f);

		float res = fmd::modulated(params[RES_PARAM].getValue(), inputs[RES_INPUT], params[RES_CV_PARAM].getValue());
		// Panel Noise 0…1 (HP/BP → ±0…0.2 into feedback; LP → ±0…2 into In). Chaos fixed at face 0.
		float noise01 = fmd::modulated(params[NOISE_PARAM].getValue(), inputs[NOISE_INPUT], params[NOISE_CV_PARAM].getValue());
		float drive = driveGainFromKnob(fmd::modulated(
			params[DRIVE_PARAM].getValue(), inputs[DRIVE_INPUT], params[DRIVE_CV_PARAM].getValue()));
		float spread = fmd::modulatedBipolar(params[SPREAD_PARAM].getValue(), inputs[SPREAD_INPUT], params[SPREAD_CV_PARAM].getValue());
		int panelMode = clamp((int) std::round(params[MODE_PARAM].getValue()), 0, 3);
		auto mode = fmd::super_love::modeFromPanel(panelMode);

		// IN1 only → mono (one voice, both outs identical). IN1+IN2 → stereo.
		const bool stereo = inputs[IN_L_INPUT].isConnected() && inputs[IN_R_INPUT].isConnected();
		float in[2] = {0.f, 0.f};
		if (stereo) {
			in[0] = inputs[IN_L_INPUT].getVoltage();
			in[1] = inputs[IN_R_INPUT].getVoltage();
		} else if (inputs[IN_L_INPUT].isConnected()) {
			in[0] = inputs[IN_L_INPUT].getVoltage();
		} else if (inputs[IN_R_INPUT].isConnected()) {
			in[0] = inputs[IN_R_INPUT].getVoltage();
		}

		float out[2] = {0.f, 0.f};
		core.process(in, out, freqNorm, res, clamp(noise01, 0.f, 1.f), drive, spread, mode, stereo);
		outputs[OUT_L_OUTPUT].setVoltage(out[0]);
		outputs[OUT_R_OUTPUT].setVoltage(out[1]);

		const float peakIn = std::max(std::fabs(in[0]), std::fabs(in[1])) * drive;
		const float peakOut = std::max(std::fabs(out[0]), std::fabs(out[1]));
		float clipAmt = 0.f;
		if (peakIn > clipLightThresh)
			clipAmt += 0.25f;
		if (peakOut > clipLightThresh)
			clipAmt += 1.f;
		if (clipAmt > 1.f)
			clipAmt = 1.f;
		if (clipAmt > 0.f) {
			clipLightEnv = clipAmt;
		} else if (clipLightEnv > 0.f) {
			clipLightEnv -= args.sampleTime / clipLightSlewSec;
			if (clipLightEnv < 0.f)
				clipLightEnv = 0.f;
		}
		float r = 0.f, g = 0.f, b = 0.f;
		clipLedRgb(clipLightEnv, &r, &g, &b);
		lights[CLIP_LIGHT].setBrightness(r);
		lights[CLIP_LIGHT_GREEN].setBrightness(g);
		lights[CLIP_LIGHT_BLUE].setBrightness(b);
	}
};


// ---------------------------------------------------------------------------


struct SlLargeKnob : fmd::LayeredKnob {
	SlLargeKnob() {
		addLayer("res/SuperLove/KnobLarge-1-base.svg", false);
		addLayer("res/SuperLove/KnobLarge-2-turn.svg", true);
		addLayer("res/SuperLove/KnobLarge-3-overlay.svg", false);
	}
};

struct SlMedKnob : fmd::LayeredKnob {
	SlMedKnob() {
		addLayer("res/SuperLove/KnobMed-1-base.svg", false);
		addLayer("res/SuperLove/KnobMed-2-turn.svg", true);
		addLayer("res/SuperLove/KnobMed-3-overlay.svg", false);
	}
};

struct SlSmallKnob : fmd::LayeredKnob {
	SlSmallKnob() {
		addLayer("res/SuperLove/KnobSmall-1-base.svg", false);
		addLayer("res/SuperLove/KnobSmall-2-turn.svg", true);
		addLayer("res/SuperLove/KnobSmall-3-overlay.svg", false);
	}
};


struct ClipLed : GrayModuleLightWidget {
	ClipLed() {
		addBaseColor(nvgRGB(0xff, 0x00, 0x00));
		addBaseColor(nvgRGB(0x00, 0xff, 0x00));
		addBaseColor(nvgRGB(0x00, 0x00, 0xff));
	}
};


struct SuperLoveWidget : ModuleWidget {
	SuperLoveWidget(SuperLove* module) {
		setModule(module);
		setPanel(new fmd::PngPanel(asset::plugin(pluginInstance, "res/panels/SuperLove.png")));
		addChild(new fmd::PanelLabels("res/labels/SuperLove.svg"));

		// -- NOISE / DRIVE and their attenuverters --------------------------
		addParam(createParamCentered<SlSmallKnob>(Vec(25.75f, 69.59f), module, SuperLove::NOISE_PARAM));
		addParam(createParamCentered<SlSmallKnob>(Vec(153.12f, 69.59f), module, SuperLove::DRIVE_PARAM));
		addParam(createParamCentered<fmd::FmdTrimmer>(Vec(25.44f, 118.78f), module, SuperLove::NOISE_CV_PARAM));
		addParam(createParamCentered<fmd::FmdTrimmer>(Vec(153.12f, 118.78f), module, SuperLove::DRIVE_CV_PARAM));

		// -- FREQ and its attenuverter --------------------------------------
		addParam(createParamCentered<SlLargeKnob>(Vec(89.44f, 116.84f), module, SuperLove::FREQ_PARAM));
		addParam(createParamCentered<fmd::FmdTrimmer>(Vec(89.44f, 189.25f), module, SuperLove::FREQ_CV_PARAM));

		// -- RES / SPREAD and their attenuverters ---------------------------
		addParam(createParamCentered<SlMedKnob>(Vec(34.12f, 185.37f), module, SuperLove::RES_PARAM));
		addParam(createParamCentered<SlMedKnob>(Vec(144.88f, 185.37f), module, SuperLove::SPREAD_PARAM));
		addParam(createParamCentered<fmd::FmdTrimmer>(Vec(23.94f, 238.69f), module, SuperLove::RES_CV_PARAM));
		addParam(createParamCentered<fmd::FmdTrimmer>(Vec(155.06f, 238.69f), module, SuperLove::SPREAD_CV_PARAM));

		// -- LP18 / LP24 / HP / BP selector ---------------------------------
		addParam(createParamCentered<fmd::FmdModeSlider>(Vec(89.50f, 248.39f), module, SuperLove::MODE_PARAM));

		// -- CV inputs ------------------------------------------------------
		addInput(createInputCentered<fmd::FmdPort>(Vec(22.88f, 288.50f), module, SuperLove::RES_INPUT));
		addInput(createInputCentered<fmd::FmdPort>(Vec(56.12f, 288.50f), module, SuperLove::NOISE_INPUT));
		addInput(createInputCentered<fmd::FmdPort>(Vec(89.50f, 288.50f), module, SuperLove::FREQ_INPUT));
		addInput(createInputCentered<fmd::FmdPort>(Vec(122.75f, 288.50f), module, SuperLove::DRIVE_INPUT));
		addInput(createInputCentered<fmd::FmdPort>(Vec(156.12f, 288.50f), module, SuperLove::SPREAD_INPUT));

		// -- IN / CLIP LED / OUT --------------------------------------------
		addInput(createInputCentered<fmd::FmdPort>(Vec(22.88f, 332.94f), module, SuperLove::IN_L_INPUT));
		addInput(createInputCentered<fmd::FmdPort>(Vec(56.12f, 332.94f), module, SuperLove::IN_R_INPUT));
		addChild(createLightCentered<LargeLight<ClipLed>>(Vec(89.50f, 332.94f), module, SuperLove::CLIP_LIGHT));
		addOutput(createOutputCentered<fmd::FmdPort>(Vec(122.75f, 332.94f), module, SuperLove::OUT_L_OUTPUT));
		addOutput(createOutputCentered<fmd::FmdPort>(Vec(156.12f, 332.94f), module, SuperLove::OUT_R_OUTPUT));
	}
};


Model* modelSuperLove = createModel<SuperLove, SuperLoveWidget>("SuperLove");
