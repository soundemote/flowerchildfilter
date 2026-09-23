#pragma once
#include "plugin.hpp"

#include <string>
#include <vector>

/** Widgets shared by the three FMD panels.

The artwork is delivered as a stack of separate SVG layers per control -- a
base, one or more layers that turn with the value, and a static highlight or
overlay on top. Rack's SvgKnob only rotates a single layer, so LayeredKnob
below generalises it to an arbitrary stack.

All control SVGs are pre-scaled to rack px by `tools/prepare_assets.py`, so
nothing here applies a scale factor: a layer is drawn at the size its SVG
declares and centred on the widget. */
namespace fmd {


/** Every FMD panel is 12 HP at Rack's standard 380 px panel height. */
static const float PANEL_W = 179.f;
static const float PANEL_H = 380.f;

/** ~300 degrees of travel, matching the printed 0..10 scale on the SR panel. */
static const float KNOB_MIN_ANGLE = -0.83f * float(M_PI);
static const float KNOB_MAX_ANGLE = 0.83f * float(M_PI);


// ---------------------------------------------------------------------------
// Panel
// ---------------------------------------------------------------------------

/** Fills the module rect with a raster panel.

Rack's SvgPanel only accepts SVG, but the FMD panels are PNG exports (the
artwork uses soft shadows and layered gradients that nanosvg cannot reproduce),
so the image is drawn straight into the module rect. */
struct PngPanel : widget::Widget {
	std::string path;
	std::shared_ptr<window::Image> image;
	app::PanelBorder* panelBorder;

	PngPanel(const std::string& path = "");
	/** Swaps the artwork. Cheap to call every frame with an unchanged path. */
	void setImagePath(const std::string& path);
	void draw(const DrawArgs& args) override;
};


/** The words printed on the panel -- FREQ, RES, IN, CLIP and the rest.

The panel exports arrived with their type layer switched off, so the labels are
rebuilt into `res/labels/` by `tools/prepare_assets.py` and drawn here as vector
over the raster panel: they stay sharp at any zoom, and Flower Child's two panel
variants share one sheet because only the PngPanel image swaps between them.

The sheet is the full 179 x 380 panel, so the widget sits at the module origin.
Add it straight after setPanel() to put the type above the artwork and below the
controls. */
struct PanelLabels : widget::Widget {
	widget::FramebufferWidget* fb;
	widget::SvgWidget* sw;

	/** `resPath` is relative to the plugin dir, e.g. "res/labels/SuperLove.svg". */
	PanelLabels(const std::string& resPath);
};


// ---------------------------------------------------------------------------
// Knobs
// ---------------------------------------------------------------------------

/** A knob built from a stack of SVG layers, any subset of which turns with the
parameter value. Layers are added bottom-first. */
struct LayeredKnob : app::Knob {
	struct Layer {
		widget::TransformWidget* tw;
		widget::SvgWidget* sw;
		bool rotates;
	};

	widget::FramebufferWidget* fb;
	app::CircularShadow* shadow;
	std::vector<Layer> layers;
	float angle = 0.f;

	LayeredKnob();
	/** `resPath` is relative to the plugin dir, e.g. "res/SuperLove/...svg". */
	void addLayer(const std::string& resPath, bool rotates);
	/** Recentres every layer and applies `angle` to the rotating ones. */
	void updateLayers();
	void onChange(const ChangeEvent& e) override;
};


/** Dark attenuverter trimmer: base, TURN, overlay, pointer (bottom to top). */
struct FmdTrimmer : LayeredKnob {
	FmdTrimmer();
};

/** The CLIP control in the output row is the bright/silver trimmer on both
mockups, not the dark one used for the attenuverters. */
struct FmdClipTrimmer : LayeredKnob {
	FmdClipTrimmer();
};


// ---------------------------------------------------------------------------
// Buttons
// ---------------------------------------------------------------------------

/** A switch whose frames may differ in size (the Flower Child LED button grows
when lit), so each frame is centred rather than corner-aligned. */
struct FramesSwitch : app::Switch {
	widget::FramebufferWidget* fb;
	widget::SvgWidget* sw;
	std::vector<std::shared_ptr<window::Svg>> frames;
	int frameIndex = -1;

	FramesSwitch();
	void addFrame(const std::string& resPath);
	void selectFrame(int index);
	void onChange(const ChangeEvent& e) override;
};


/** One button of a mutually exclusive group. All buttons in the group share a
single parameter; clicking one sets that parameter to this button's index, and
each button lights only while the parameter equals its own index. */
struct RadioButton : FramesSwitch {
	int index = 0;
	/** Optional glyph drawn on top of the button face. */
	widget::SvgWidget* icon = NULL;

	void setIcon(const std::string& resPath);
	void step() override;
	void onDragStart(const DragStartEvent& e) override;
	void onDragEnd(const DragEndEvent& e) override {}
	/** Frame selection is driven by step(), not by the raw parameter value. */
	void onChange(const ChangeEvent& e) override {}
};


// ---------------------------------------------------------------------------
// Sliders
// ---------------------------------------------------------------------------

/** Vertical Polivoks-style fader. The panel artwork already draws the slot, so
only the handle is supplied; the widget box describes the travel. */
struct FmdFader : app::SvgSlider {
	FmdFader();
};

/** Horizontal 4-position filter mode selector on Super Love.
Click +1 (wraps), Ctrl-click −1. Drag maps mouse X onto the four ticks. */
struct FmdModeSlider : app::SvgSlider {
	float dragOldValue = NAN;
	bool dragSlid = false;
	float dragDist = 0.f;
	float dragAccumX = 0.f;

	FmdModeSlider();
	void onDragStart(const DragStartEvent& e) override;
	void onDragMove(const DragMoveEvent& e) override;
	void onDragEnd(const DragEndEvent& e) override;
	void onDoubleClick(const DoubleClickEvent& e) override;
	void onHoverScroll(const HoverScrollEvent& e) override;
};


// ---------------------------------------------------------------------------
// Ports
// ---------------------------------------------------------------------------

/** The FMD panel artwork already includes the corner screws, so no ScrewSilver
children are added by the module widgets. */
struct FmdPort : app::SvgPort {
	FmdPort();
};


} // namespace fmd
