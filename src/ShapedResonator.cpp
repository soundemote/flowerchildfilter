#include "plugin.hpp"
#include "ShapedResonatorFilter.hpp"
#include "FmdDsp.hpp"
#include "FmdWidgets.hpp"

/*  Shaped Resonator -- 12 HP stereo resonant filter.

    Control positions come from
    `image assets/Shaped Resonator/260731 Shaped Resonator positions.svg`
    (a 716 x 1518 drawing, i.e. 4x the 179 x 380 panel).

    The mockup prints RES / CRNCH / DRV / SPRD under the four faders and runs the
    third CV trace down to the FREQ knob, so the top row of jacks is
    RES, CRNCH, FREQ, DRV, SPRD. The positions file marks each fader's handle at
    its mockup value rather than the centre of its slot; the slot itself is
    drawn on the panel from y=137 to y=215, so the faders are placed on that
    span and the marked handle positions are used as the default values. */

struct ShapedResonator : Module {
	enum ParamId {
		FREQ_PARAM,
		RES_PARAM,
		CRUNCH_PARAM,
		DRIVE_PARAM,
		SPREAD_PARAM,
		SHAPE_PARAM,
		CLIP_PARAM,
		RES_CV_PARAM,
		CRUNCH_CV_PARAM,
		FREQ_CV_PARAM,
		DRIVE_CV_PARAM,
		SPREAD_CV_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		RES_INPUT,
		CRUNCH_INPUT,
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

	fmd::shaped_resonator::Core core;

	ShapedResonator() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, 0);

		configParam(FREQ_PARAM, 0.f, 10.f, 5.5f, "Frequency", " Hz", 2.f, 20.f);
		configParam(RES_PARAM, 0.f, 1.f, 0.5f, "Resonance", "%", 0.f, 100.f);
		configParam(CRUNCH_PARAM, 0.f, 1.f, 0.35f, "Crunch", "%", 0.f, 100.f);
		configParam(DRIVE_PARAM, 0.f, 1.f, 0.35f, "Drive", "%", 0.f, 100.f);
		configParam(SPREAD_PARAM, -1.f, 1.f, 0.f, "Spread", "%", 0.f, 100.f);
		configParam(CLIP_PARAM, 0.f, 1.f, 0.25f, "Clip", "%", 0.f, 100.f);
		configSwitch(SHAPE_PARAM, 0.f, 2.f, 0.f, "Shape", {"Band", "Low", "High"});

		configParam(RES_CV_PARAM, -1.f, 1.f, 0.f, "Resonance CV", "%", 0.f, 100.f);
		configParam(CRUNCH_CV_PARAM, -1.f, 1.f, 0.f, "Crunch CV", "%", 0.f, 100.f);
		configParam(FREQ_CV_PARAM, -1.f, 1.f, 0.f, "Frequency CV", "%", 0.f, 100.f);
		configParam(DRIVE_CV_PARAM, -1.f, 1.f, 0.f, "Drive CV", "%", 0.f, 100.f);
		configParam(SPREAD_CV_PARAM, -1.f, 1.f, 0.f, "Spread CV", "%", 0.f, 100.f);

		configInput(RES_INPUT, "Resonance CV");
		configInput(CRUNCH_INPUT, "Crunch CV");
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

		float freqNorm = clamp(params[FREQ_PARAM].getValue() / 10.f, 0.f, 1.f);
		if (inputs[FREQ_INPUT].isConnected())
			freqNorm = clamp(freqNorm + inputs[FREQ_INPUT].getVoltage() * 0.1f * params[FREQ_CV_PARAM].getValue(), 0.f, 1.f);

		float res = fmd::modulated(params[RES_PARAM].getValue(), inputs[RES_INPUT], params[RES_CV_PARAM].getValue());
		float crunch = fmd::modulated(params[CRUNCH_PARAM].getValue(), inputs[CRUNCH_INPUT], params[CRUNCH_CV_PARAM].getValue());
		float drive = fmd::modulated(params[DRIVE_PARAM].getValue(), inputs[DRIVE_INPUT], params[DRIVE_CV_PARAM].getValue());
		float spread = fmd::modulatedBipolar(params[SPREAD_PARAM].getValue(), inputs[SPREAD_INPUT], params[SPREAD_CV_PARAM].getValue());
		float clip = params[CLIP_PARAM].getValue();
		int shape = clamp((int) std::round(params[SHAPE_PARAM].getValue()), 0, 2);
		auto mode = (fmd::shaped_resonator::Mode) shape;

		float left = inputs[IN_L_INPUT].getVoltage();
		float in[2] = {
			left,
			inputs[IN_R_INPUT].isConnected() ? inputs[IN_R_INPUT].getVoltage() : left,
		};
		float out[2] = {0.f, 0.f};
		core.process(in, out, freqNorm, res, crunch, drive, spread, clip, mode);

		outputs[OUT_L_OUTPUT].setVoltage(out[0]);
		outputs[OUT_R_OUTPUT].setVoltage(out[1]);
	}
};

// Shape buttons 0/1/2 select Sinusoid / Triangle / Sawtooth resonators.


// ---------------------------------------------------------------------------


struct SrBigKnob : fmd::LayeredKnob {
	SrBigKnob() {
		addLayer("res/ShapedResonator/KnobBig-1-base.svg", false);
		addLayer("res/ShapedResonator/KnobBig-2-turn.svg", true);
		addLayer("res/ShapedResonator/KnobBig-3-overlay.svg", false);
	}
};

/** One of the three shape buttons. `Index` selects which value it commits. */
template <int Index>
struct SrShapeButton : fmd::RadioButton {
	SrShapeButton() {
		index = Index;
		addFrame("res/ShapedResonator/Button-0-off.svg");
		addFrame("res/ShapedResonator/Button-1-on.svg");
		setIcon(string::f("res/ShapedResonator/IconShape%d.svg", Index + 1));
	}
};


struct ShapedResonatorWidget : ModuleWidget {
	ShapedResonatorWidget(ShapedResonator* module) {
		setModule(module);
		setPanel(new fmd::PngPanel(asset::plugin(pluginInstance, "res/panels/ShapedResonator.png")));
		addChild(new fmd::PanelLabels("res/labels/ShapedResonator.svg"));

		// -- CV inputs ------------------------------------------------------
		addInput(createInputCentered<fmd::FmdPort>(Vec(22.87f, 54.66f), module, ShapedResonator::RES_INPUT));
		addInput(createInputCentered<fmd::FmdPort>(Vec(56.12f, 54.66f), module, ShapedResonator::CRUNCH_INPUT));
		addInput(createInputCentered<fmd::FmdPort>(Vec(89.49f, 54.66f), module, ShapedResonator::FREQ_INPUT));
		addInput(createInputCentered<fmd::FmdPort>(Vec(122.74f, 54.66f), module, ShapedResonator::DRIVE_INPUT));
		addInput(createInputCentered<fmd::FmdPort>(Vec(156.12f, 54.66f), module, ShapedResonator::SPREAD_INPUT));

		// -- attenuverter row ----------------------------------------------
		addParam(createParamCentered<fmd::FmdTrimmer>(Vec(22.87f, 98.84f), module, ShapedResonator::RES_CV_PARAM));
		addParam(createParamCentered<fmd::FmdTrimmer>(Vec(56.12f, 98.84f), module, ShapedResonator::CRUNCH_CV_PARAM));
		addParam(createParamCentered<fmd::FmdTrimmer>(Vec(89.62f, 98.84f), module, ShapedResonator::FREQ_CV_PARAM));
		addParam(createParamCentered<fmd::FmdTrimmer>(Vec(122.74f, 98.84f), module, ShapedResonator::DRIVE_CV_PARAM));
		addParam(createParamCentered<fmd::FmdTrimmer>(Vec(156.12f, 98.84f), module, ShapedResonator::SPREAD_CV_PARAM));

		// -- faders, centred on the slots printed at y=137..215 -------------
		const float faderY = 176.f;
		addParam(createParamCentered<fmd::FmdFader>(Vec(22.74f, faderY), module, ShapedResonator::RES_PARAM));
		addParam(createParamCentered<fmd::FmdFader>(Vec(56.12f, faderY), module, ShapedResonator::CRUNCH_PARAM));
		addParam(createParamCentered<fmd::FmdFader>(Vec(122.74f, faderY), module, ShapedResonator::DRIVE_PARAM));
		addParam(createParamCentered<fmd::FmdFader>(Vec(156.24f, faderY), module, ShapedResonator::SPREAD_PARAM));

		// -- shape buttons ---------------------------------------------------
		addParam(createParamCentered<SrShapeButton<0>>(Vec(89.37f, 152.29f), module, ShapedResonator::SHAPE_PARAM));
		addParam(createParamCentered<SrShapeButton<1>>(Vec(89.49f, 171.56f), module, ShapedResonator::SHAPE_PARAM));
		addParam(createParamCentered<SrShapeButton<2>>(Vec(89.49f, 190.84f), module, ShapedResonator::SHAPE_PARAM));

		// -- FREQ --------------------------------------------------------------
		addParam(createParamCentered<SrBigKnob>(Vec(89.49f, 258.05f), module, ShapedResonator::FREQ_PARAM));

		// -- IN / CLIP / OUT ---------------------------------------------------
		addInput(createInputCentered<fmd::FmdPort>(Vec(22.87f, 332.90f), module, ShapedResonator::IN_L_INPUT));
		addInput(createInputCentered<fmd::FmdPort>(Vec(56.12f, 332.90f), module, ShapedResonator::IN_R_INPUT));
		addParam(createParamCentered<fmd::FmdClipTrimmer>(Vec(89.49f, 332.77f), module, ShapedResonator::CLIP_PARAM));
		addOutput(createOutputCentered<fmd::FmdPort>(Vec(122.74f, 332.90f), module, ShapedResonator::OUT_L_OUTPUT));
		addOutput(createOutputCentered<fmd::FmdPort>(Vec(155.99f, 332.90f), module, ShapedResonator::OUT_R_OUTPUT));
	}
};


Model* modelShapedResonator = createModel<ShapedResonator, ShapedResonatorWidget>("ShapedResonator");
