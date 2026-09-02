#include "plugin.hpp"
#include "FlowerChildFilter.hpp"
#include "FmdWidgets.hpp"

/*  Flower Child -- 12 HP stereo multimode filter.

    Control positions are taken verbatim from
    `image assets/Flower Child/210225 Flower Child positions.svg`, converted from
    that file's 72 dpi drawing space (171.97 x 364.45) into Rack's 179 x 380
    panel space. Labels come from FMD-FC-panel-type.svg: FREQ, AGGR, RES, NOISE,
    DRIVE, SPREAD, IN, CLIP, OUT.

    The knob-to-label assignment is that type layer's, read off the glyph
    positions once it was finally rendered (`res/labels/FlowerChild.svg`). Every
    label sits ~29 px above the control it names, which FREQ, AGGR, IN, CLIP and
    OUT all confirm, and on that offset the four voice knobs read:

        RES    (24.9, 125.5) -> outer left  (25.86, 154.36)
        NOISE  (63.4, 152.6) -> inner left  (64.08, 182.76)
        DRIVE  (114.0, 152.6) -> inner right (115.04, 182.76)
        SPREAD (152.6, 125.5) -> outer right (153.26, 154.36)

    So RES and SPREAD are the outer pair, not the inner one. The CV trimmer and
    jack columns below are unlabelled on this panel and keep Super Love's
    printed order, RES / NOISE / FREQ / DRIVE / SPREAD left to right, which
    stays consistent with the knobs above. */

struct FlowerChild : Module {
	enum ParamId {
		FREQ_PARAM,
		RES_PARAM,
		NOISE_PARAM,
		DRIVE_PARAM,
		SPREAD_PARAM,
		AGGR_PARAM,
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

	fmd::flower_child::Core core;

	FlowerChild() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, 0);

		// FREQ is the original 0..1 slider domain (mapped inside the DSP to
		// MIDI pitches 3..161, then to Hz). Tooltip shows percent of that range.
		configParam(FREQ_PARAM, 0.f, 1.f, 0.5f, "Frequency", "%", 0.f, 100.f);

		configParam(RES_PARAM, 0.f, 1.f, 0.2f, "Resonance", "%", 0.f, 100.f);
		configParam(NOISE_PARAM, 0.f, 1.f, 0.f, "Noise", "%", 0.f, 100.f);
		configParam(DRIVE_PARAM, 0.f, 1.f, 0.2f, "Drive", "%", 0.f, 100.f);
		configParam(SPREAD_PARAM, 0.f, 1.f, 0.f, "Spread", "%", 0.f, 100.f);
		configParam(CLIP_PARAM, 0.f, 1.f, 0.25f, "Clip", "%", 0.f, 100.f);
		configSwitch(AGGR_PARAM, 0.f, 1.f, 0.f, "Aggressive", {"Clean", "Dirty"});

		configParam(RES_CV_PARAM, -1.f, 1.f, 0.f, "Resonance CV", "%", 0.f, 100.f);
		configParam(NOISE_CV_PARAM, -1.f, 1.f, 0.f, "Noise CV", "%", 0.f, 100.f);
		configParam(FREQ_CV_PARAM, -1.f, 1.f, 0.f, "Frequency CV", "%", 0.f, 100.f);
		configParam(DRIVE_CV_PARAM, -1.f, 1.f, 0.f, "Drive CV", "%", 0.f, 100.f);
		configParam(SPREAD_CV_PARAM, -1.f, 1.f, 0.f, "Spread CV", "%", 0.f, 100.f);

		configInput(RES_INPUT, "Resonance CV");
		configInput(NOISE_INPUT, "Noise CV");
		configInput(FREQ_INPUT, "Frequency CV");
		configInput(DRIVE_INPUT, "Drive CV");
		configInput(SPREAD_INPUT, "Spread CV");
		configInput(IN_L_INPUT, "Left audio");
		configInput(IN_R_INPUT, "Right audio");

		configOutput(OUT_L_OUTPUT, "Left audio");
		configOutput(OUT_R_OUTPUT, "Right audio");

		configBypass(IN_L_INPUT, OUT_L_OUTPUT);
		configBypass(IN_R_INPUT, OUT_R_OUTPUT);

		getParamQuantity(AGGR_PARAM)->description = "Clean (Rev1) or Dirty (Rev2) feedback oscillator";
		getParamQuantity(NOISE_PARAM)->description = "Chaos amount into the feedback path";
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		core.setSampleRate(e.sampleRate);
		core.reset();
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		core.reset();
	}

	static float modulated01(float knob, Input& input, float attenuverter) {
		float v = knob;
		if (input.isConnected())
			v += input.getVoltage() * 0.1f * attenuverter;
		return clamp(v, 0.f, 1.f);
	}

	void process(const ProcessArgs& args) override {
		core.setSampleRate(args.sampleRate);

		float freq = modulated01(params[FREQ_PARAM].getValue(), inputs[FREQ_INPUT], params[FREQ_CV_PARAM].getValue());
		float res = modulated01(params[RES_PARAM].getValue(), inputs[RES_INPUT], params[RES_CV_PARAM].getValue());
		float chaos = modulated01(params[NOISE_PARAM].getValue(), inputs[NOISE_INPUT], params[NOISE_CV_PARAM].getValue());
		float drive = modulated01(params[DRIVE_PARAM].getValue(), inputs[DRIVE_INPUT], params[DRIVE_CV_PARAM].getValue());
		float spread = modulated01(params[SPREAD_PARAM].getValue(), inputs[SPREAD_INPUT], params[SPREAD_CV_PARAM].getValue());
		float clip = params[CLIP_PARAM].getValue();
		bool dirty = params[AGGR_PARAM].getValue() > 0.5f;

		// A patched left input feeds both channels when the right is empty.
		float left = inputs[IN_L_INPUT].getVoltage();
		float in[2] = {
			left,
			inputs[IN_R_INPUT].isConnected() ? inputs[IN_R_INPUT].getVoltage() : left,
		};
		float out[2] = {0.f, 0.f};
		core.process(in, out, freq, res, chaos, drive, spread, clip, dirty);

		outputs[OUT_L_OUTPUT].setVoltage(out[0]);
		outputs[OUT_R_OUTPUT].setVoltage(out[1]);
	}
};


// ---------------------------------------------------------------------------


struct FcBigKnob : fmd::LayeredKnob {
	FcBigKnob() {
		addLayer("res/FlowerChild/KnobBig-1-base.svg", false);
		addLayer("res/FlowerChild/KnobBig-2-topturn.svg", true);
		addLayer("res/FlowerChild/KnobBig-3-highlight.svg", false);
		addLayer("res/FlowerChild/KnobBig-4-skirtturn.svg", true);
		addLayer("res/FlowerChild/KnobBig-5-toplight.svg", false);
		addLayer("res/FlowerChild/KnobBig-6-top.svg", false);
	}
};

struct FcSmallKnob : fmd::LayeredKnob {
	FcSmallKnob() {
		addLayer("res/FlowerChild/KnobSmall-1-base.svg", false);
		addLayer("res/FlowerChild/KnobSmall-2-turn.svg", true);
		addLayer("res/FlowerChild/KnobSmall-3-top.svg", false);
	}
};

struct FcLedButton : fmd::FramesSwitch {
	FcLedButton() {
		addFrame("res/FlowerChild/ButtonLED-0-off.svg");
		addFrame("res/FlowerChild/ButtonLED-1-on.svg");
	}
};


struct FlowerChildWidget : ModuleWidget {
	fmd::PngPanel* pngPanel;

	FlowerChildWidget(FlowerChild* module) {
		setModule(module);

		pngPanel = new fmd::PngPanel(asset::plugin(pluginInstance, "res/panels/FlowerChild.png"));
		setPanel(pngPanel);

		// One sheet serves both panel variants -- step() swaps only the raster.
		addChild(new fmd::PanelLabels("res/labels/FlowerChild.svg"));

		// -- FREQ, sitting over the flower ---------------------------------
		addParam(createParamCentered<FcBigKnob>(Vec(89.56f, 91.17f), module, FlowerChild::FREQ_PARAM));
		addParam(createParamCentered<FcLedButton>(Vec(25.86f, 66.77f), module, FlowerChild::AGGR_PARAM));

		// -- the four voice knobs, in the order the type layer names them ---
		addParam(createParamCentered<FcSmallKnob>(Vec(25.86f, 154.36f), module, FlowerChild::RES_PARAM));
		addParam(createParamCentered<FcSmallKnob>(Vec(153.26f, 154.36f), module, FlowerChild::SPREAD_PARAM));
		addParam(createParamCentered<FcSmallKnob>(Vec(64.08f, 182.76f), module, FlowerChild::NOISE_PARAM));
		addParam(createParamCentered<FcSmallKnob>(Vec(115.04f, 182.76f), module, FlowerChild::DRIVE_PARAM));

		// -- attenuverter row ----------------------------------------------
		addParam(createParamCentered<fmd::FmdTrimmer>(Vec(22.73f, 241.57f), module, FlowerChild::RES_CV_PARAM));
		addParam(createParamCentered<fmd::FmdTrimmer>(Vec(56.33f, 241.57f), module, FlowerChild::NOISE_CV_PARAM));
		addParam(createParamCentered<fmd::FmdTrimmer>(Vec(89.68f, 241.57f), module, FlowerChild::FREQ_CV_PARAM));
		addParam(createParamCentered<fmd::FmdTrimmer>(Vec(122.91f, 241.57f), module, FlowerChild::DRIVE_CV_PARAM));
		addParam(createParamCentered<fmd::FmdTrimmer>(Vec(156.26f, 241.57f), module, FlowerChild::SPREAD_CV_PARAM));

		// -- CV inputs ------------------------------------------------------
		addInput(createInputCentered<fmd::FmdPort>(Vec(22.98f, 288.49f), module, FlowerChild::RES_INPUT));
		addInput(createInputCentered<fmd::FmdPort>(Vec(56.21f, 288.49f), module, FlowerChild::NOISE_INPUT));
		addInput(createInputCentered<fmd::FmdPort>(Vec(89.56f, 288.49f), module, FlowerChild::FREQ_INPUT));
		addInput(createInputCentered<fmd::FmdPort>(Vec(122.78f, 288.49f), module, FlowerChild::DRIVE_INPUT));
		addInput(createInputCentered<fmd::FmdPort>(Vec(156.13f, 288.49f), module, FlowerChild::SPREAD_INPUT));

		// -- IN / CLIP / OUT ------------------------------------------------
		addInput(createInputCentered<fmd::FmdPort>(Vec(22.98f, 332.90f), module, FlowerChild::IN_L_INPUT));
		addInput(createInputCentered<fmd::FmdPort>(Vec(56.21f, 332.90f), module, FlowerChild::IN_R_INPUT));
		addParam(createParamCentered<fmd::FmdClipTrimmer>(Vec(89.56f, 332.78f), module, FlowerChild::CLIP_PARAM));
		addOutput(createOutputCentered<fmd::FmdPort>(Vec(122.78f, 332.90f), module, FlowerChild::OUT_L_OUTPUT));
		addOutput(createOutputCentered<fmd::FmdPort>(Vec(156.01f, 332.90f), module, FlowerChild::OUT_R_OUTPUT));
	}

	void step() override {
		ModuleWidget::step();

		// AGGR swaps in the alternate panel artwork.
		bool aggressive = module && module->params[FlowerChild::AGGR_PARAM].getValue() > 0.5f;
		pngPanel->setImagePath(asset::plugin(pluginInstance,
			aggressive ? "res/panels/FlowerChildAggr.png" : "res/panels/FlowerChild.png"));
	}
};


Model* modelFlowerChild = createModel<FlowerChild, FlowerChildWidget>("FlowerChild");
