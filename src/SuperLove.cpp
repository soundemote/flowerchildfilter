#include "plugin.hpp"
#include "FmdDsp.hpp"
#include "FmdWidgets.hpp"

/*  Super Love -- 12 HP stereo LP6 / LP12 / BP / HP filter.

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
		CLIP_PARAM,
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

	fmd::FilterCore core;

	SuperLove() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, 0);

		configParam(FREQ_PARAM, 0.f, 10.f, 6.f, "Frequency", " Hz", 2.f, 20.f);
		configParam(RES_PARAM, 0.f, 1.f, 0.3f, "Resonance", "%", 0.f, 100.f);
		configParam(NOISE_PARAM, 0.f, 1.f, 0.f, "Noise", "%", 0.f, 100.f);
		configParam(DRIVE_PARAM, 0.f, 1.f, 0.25f, "Drive", "%", 0.f, 100.f);
		configParam(SPREAD_PARAM, 0.f, 1.f, 0.f, "Spread", "%", 0.f, 100.f);
		configParam(CLIP_PARAM, 0.f, 1.f, 0.25f, "Clip", "%", 0.f, 100.f);

		// Printed on the panel above the slider, left to right.
		configSwitch(MODE_PARAM, 0.f, 3.f, 1.f, "Mode", {"LP6", "LP12", "BP", "HP"});

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

		configBypass(IN_L_INPUT, OUT_L_OUTPUT);
		configBypass(IN_R_INPUT, OUT_R_OUTPUT);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		core.setSampleRate(e.sampleRate);
		core.reset();
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		core.reset();
	}

	void process(const ProcessArgs& args) override {
		core.setSampleRate(args.sampleRate);

		fmd::FilterParams p;
		p.freqHz = fmd::freqFromOctaves(
			params[FREQ_PARAM].getValue(),
			inputs[FREQ_INPUT].isConnected()
				? inputs[FREQ_INPUT].getVoltage() * params[FREQ_CV_PARAM].getValue()
				: 0.f);
		p.res = fmd::modulated(params[RES_PARAM].getValue(), inputs[RES_INPUT], params[RES_CV_PARAM].getValue());
		p.grit = fmd::modulated(params[NOISE_PARAM].getValue(), inputs[NOISE_INPUT], params[NOISE_CV_PARAM].getValue());
		p.drive = fmd::modulated(params[DRIVE_PARAM].getValue(), inputs[DRIVE_INPUT], params[DRIVE_CV_PARAM].getValue());
		p.spread = fmd::modulated(params[SPREAD_PARAM].getValue(), inputs[SPREAD_INPUT], params[SPREAD_CV_PARAM].getValue());
		p.clip = params[CLIP_PARAM].getValue();

		// The slider positions read LP6, LP12, BP, HP -- the same order as
		// FilterCore::Mode, so the parameter maps straight through.
		p.mode = clamp((int) std::round(params[MODE_PARAM].getValue()), 0, fmd::FilterCore::NUM_MODES - 1);

		float left = inputs[IN_L_INPUT].getVoltage();
		float in[2] = {
			left,
			inputs[IN_R_INPUT].isConnected() ? inputs[IN_R_INPUT].getVoltage() : left,
		};
		float out[2] = {0.f, 0.f};
		core.process(in, out, p);

		outputs[OUT_L_OUTPUT].setVoltage(out[0]);
		outputs[OUT_R_OUTPUT].setVoltage(out[1]);
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

		// -- LP6 / LP12 / BP / HP selector ----------------------------------
		addParam(createParamCentered<fmd::FmdModeSlider>(Vec(89.50f, 248.39f), module, SuperLove::MODE_PARAM));

		// -- CV inputs ------------------------------------------------------
		addInput(createInputCentered<fmd::FmdPort>(Vec(22.88f, 288.50f), module, SuperLove::RES_INPUT));
		addInput(createInputCentered<fmd::FmdPort>(Vec(56.12f, 288.50f), module, SuperLove::NOISE_INPUT));
		addInput(createInputCentered<fmd::FmdPort>(Vec(89.50f, 288.50f), module, SuperLove::FREQ_INPUT));
		addInput(createInputCentered<fmd::FmdPort>(Vec(122.75f, 288.50f), module, SuperLove::DRIVE_INPUT));
		addInput(createInputCentered<fmd::FmdPort>(Vec(156.12f, 288.50f), module, SuperLove::SPREAD_INPUT));

		// -- IN / CLIP / OUT ------------------------------------------------
		addInput(createInputCentered<fmd::FmdPort>(Vec(22.88f, 332.94f), module, SuperLove::IN_L_INPUT));
		addInput(createInputCentered<fmd::FmdPort>(Vec(56.12f, 332.94f), module, SuperLove::IN_R_INPUT));
		addParam(createParamCentered<fmd::FmdClipTrimmer>(Vec(89.50f, 332.94f), module, SuperLove::CLIP_PARAM));
		addOutput(createOutputCentered<fmd::FmdPort>(Vec(122.75f, 332.94f), module, SuperLove::OUT_L_OUTPUT));
		addOutput(createOutputCentered<fmd::FmdPort>(Vec(156.12f, 332.94f), module, SuperLove::OUT_R_OUTPUT));
	}
};


Model* modelSuperLove = createModel<SuperLove, SuperLoveWidget>("SuperLove");
