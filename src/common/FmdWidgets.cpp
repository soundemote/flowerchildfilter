#include "FmdWidgets.hpp"

#include <cmath>

namespace fmd {


static std::shared_ptr<window::Svg> loadRes(const std::string& resPath) {
	return APP->window->loadSvg(asset::plugin(pluginInstance, resPath));
}


// ---------------------------------------------------------------------------
// PngPanel
// ---------------------------------------------------------------------------

PngPanel::PngPanel(const std::string& path) {
	box.size = math::Vec(PANEL_W, PANEL_H);

	panelBorder = new app::PanelBorder;
	panelBorder->box.size = box.size;
	addChild(panelBorder);

	setImagePath(path);
}


void PngPanel::setImagePath(const std::string& path) {
	if (path == this->path)
		return;
	this->path = path;
	// Dropped here, reloaded on the next draw() where a NanoVG context exists.
	image = NULL;
}


void PngPanel::draw(const DrawArgs& args) {
	if (!image && !path.empty())
		image = APP->window->loadImage(path);

	if (image && image->handle >= 0) {
		NVGpaint paint = nvgImagePattern(args.vg, 0.f, 0.f, box.size.x, box.size.y, 0.f, image->handle, 1.f);
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
		nvgFillPaint(args.vg, paint);
		nvgFill(args.vg);
	}
	else {
		// Artwork missing: draw a flat panel so the module is still usable.
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
		nvgFillColor(args.vg, nvgRGB(0x1a, 0x1a, 0x1a));
		nvgFill(args.vg);
	}

	Widget::draw(args);
}


// ---------------------------------------------------------------------------
// PanelLabels
// ---------------------------------------------------------------------------

PanelLabels::PanelLabels(const std::string& resPath) {
	box.size = math::Vec(PANEL_W, PANEL_H);

	// The type never changes, so it is rendered once into a framebuffer rather
	// than re-tessellated every frame -- a label sheet is up to 36 glyph paths.
	fb = new widget::FramebufferWidget;
	addChild(fb);

	sw = new widget::SvgWidget;
	sw->setSvg(loadRes(resPath));
	fb->addChild(sw);
	// SvgWidget::wrap() leaves this at zero if the sheet failed to load, which
	// is the right outcome: no labels rather than a framebuffer of nothing.
	fb->box.size = sw->box.size;
}


// ---------------------------------------------------------------------------
// LayeredKnob
// ---------------------------------------------------------------------------

LayeredKnob::LayeredKnob() {
	minAngle = KNOB_MIN_ANGLE;
	maxAngle = KNOB_MAX_ANGLE;

	// Widget::box.size defaults to (INFINITY, INFINITY); addLayer() grows the
	// box to fit each layer, so it has to start at zero.
	box.size = math::Vec();

	fb = new widget::FramebufferWidget;
	addChild(fb);

	shadow = new app::CircularShadow;
	shadow->box.size = math::Vec();
	fb->addChild(shadow);
}


void LayeredKnob::addLayer(const std::string& resPath, bool rotates) {
	Layer layer;
	layer.rotates = rotates;
	layer.tw = new widget::TransformWidget;
	layer.sw = new widget::SvgWidget;
	layer.sw->setSvg(loadRes(resPath));
	layer.tw->addChild(layer.sw);
	fb->addChild(layer.tw);
	layers.push_back(layer);

	// The widget is as large as its largest layer -- normally the base, but the
	// Flower Child cap overhangs its skirt by a fraction of a pixel.
	box.size = box.size.max(layer.sw->box.size);
	fb->box.size = box.size;
	shadow->box.size = box.size;
	shadow->box.pos = math::Vec(0.f, box.size.y * 0.1f);

	updateLayers();
}


void LayeredKnob::updateLayers() {
	math::Vec center = box.size.div(2.f);

	for (size_t i = 0; i < layers.size(); i++) {
		Layer& layer = layers[i];
		layer.tw->box.size = box.size;
		layer.sw->box.pos = center.minus(layer.sw->box.size.div(2.f));

		// TransformWidget applies the last-pushed transform first, so this
		// reads bottom-up: move the layer centre to the origin, rotate, move
		// it back. Matches the order used by Rack's own SvgKnob.
		layer.tw->identity();
		if (layer.rotates) {
			layer.tw->translate(center);
			layer.tw->rotate(angle);
			layer.tw->translate(center.neg());
		}
	}
	fb->setDirty();
}


void LayeredKnob::onChange(const ChangeEvent& e) {
	engine::ParamQuantity* pq = getParamQuantity();
	if (pq) {
		float minValue = pq->getMinValue();
		float maxValue = pq->getMaxValue();
		float newAngle;
		if (std::isfinite(minValue) && std::isfinite(maxValue) && maxValue > minValue)
			newAngle = math::rescale(pq->getValue(), minValue, maxValue, minAngle, maxAngle);
		else
			newAngle = std::fmod(pq->getValue(), 1.f) * 2.f * float(M_PI);

		if (newAngle != angle) {
			angle = newAngle;
			updateLayers();
		}
	}
	app::Knob::onChange(e);
}


FmdTrimmer::FmdTrimmer() {
	// The separate KnobTrimmer-01-pointer asset is not stacked here: it is
	// drawn on its own tiny page, so the offset that would place it away from
	// the centre is lost. The turn layer already carries a visible indicator.
	addLayer("res/common/TrimmerDark-1-base.svg", false);
	addLayer("res/common/TrimmerDark-2-turn.svg", true);
	addLayer("res/common/TrimmerDark-3-overlay.svg", false);
}


FmdClipTrimmer::FmdClipTrimmer() {
	addLayer("res/common/Trimmer-1-base.svg", false);
	addLayer("res/common/Trimmer-2-turn.svg", true);
	addLayer("res/common/Trimmer-3-overlay.svg", false);
}


// ---------------------------------------------------------------------------
// FramesSwitch / RadioButton
// ---------------------------------------------------------------------------

FramesSwitch::FramesSwitch() {
	// As in LayeredKnob: addFrame() grows the box, so start from zero rather
	// than Widget's default of (INFINITY, INFINITY).
	box.size = math::Vec();

	fb = new widget::FramebufferWidget;
	addChild(fb);

	sw = new widget::SvgWidget;
	fb->addChild(sw);
}


void FramesSwitch::addFrame(const std::string& resPath) {
	std::shared_ptr<window::Svg> svg = loadRes(resPath);
	frames.push_back(svg);

	// The lit Flower Child button is larger than the unlit one because of its
	// glow, so the widget takes the largest frame and every frame is centred.
	box.size = box.size.max(svg->getSize());
	fb->box.size = box.size;

	int wanted = (frameIndex < 0) ? 0 : frameIndex;
	frameIndex = -1;
	selectFrame(wanted);
}


void FramesSwitch::selectFrame(int index) {
	if (frames.empty())
		return;
	index = math::clamp(index, 0, (int) frames.size() - 1);
	if (index == frameIndex)
		return;

	frameIndex = index;
	sw->setSvg(frames[index]);
	sw->box.pos = box.size.div(2.f).minus(sw->box.size.div(2.f));
	fb->setDirty();
}


void FramesSwitch::onChange(const ChangeEvent& e) {
	engine::ParamQuantity* pq = getParamQuantity();
	if (pq)
		selectFrame((int) std::round(pq->getValue() - pq->getMinValue()));
	app::Switch::onChange(e);
}


void RadioButton::setIcon(const std::string& resPath) {
	icon = new widget::SvgWidget;
	icon->setSvg(loadRes(resPath));
	icon->box.pos = box.size.div(2.f).minus(icon->box.size.div(2.f));
	// Added to the widget rather than the framebuffer so it stays above the
	// button face regardless of which frame is showing.
	addChild(icon);
}


void RadioButton::step() {
	engine::ParamQuantity* pq = getParamQuantity();
	if (pq)
		selectFrame((int) std::round(pq->getValue()) == index ? 1 : 0);
	app::Switch::step();
}


void RadioButton::onDragStart(const DragStartEvent& e) {
	if (e.button != GLFW_MOUSE_BUTTON_LEFT)
		return;

	engine::ParamQuantity* pq = getParamQuantity();
	if (!pq)
		return;

	float oldValue = pq->getValue();
	pq->setValue((float) index);
	float newValue = pq->getValue();

	if (oldValue != newValue && module) {
		history::ParamChange* h = new history::ParamChange;
		h->name = "select " + pq->getLabel();
		h->moduleId = module->id;
		h->paramId = paramId;
		h->oldValue = oldValue;
		h->newValue = newValue;
		APP->history->push(h);
	}
}


// ---------------------------------------------------------------------------
// Sliders
// ---------------------------------------------------------------------------

FmdFader::FmdFader() {
	setHandleSvg(loadRes("res/ShapedResonator/FaderHandle.svg"));

	// The slot printed on the panel runs from y=137 to y=215 in panel space.
	box.size = math::Vec(17.f, 78.f);
	fb->box.size = box.size;

	float half = handle->box.size.y / 2.f;
	setHandlePosCentered(math::Vec(box.size.x / 2.f, box.size.y - half),
	                     math::Vec(box.size.x / 2.f, half));
}


FmdModeSlider::FmdModeSlider() {
	horizontal = true;
	snap = true;

	setHandleSvg(loadRes("res/SuperLove/SliderHandle.svg"));

	box.size = math::Vec(48.f, 16.f);
	fb->box.size = box.size;

	// Travel taken from the handle position marked in the positions artwork
	// (x=106.19 at the HP end), mirrored about the slot centre at x=90.
	setHandlePosCentered(math::Vec(7.81f, box.size.y / 2.f),
	                     math::Vec(40.19f, box.size.y / 2.f));
}


void FmdModeSlider::onDragStart(const DragStartEvent& e) {
	if (e.button != GLFW_MOUSE_BUTTON_LEFT)
		return;
	engine::ParamQuantity* pq = getParamQuantity();
	dragOldValue = pq ? pq->getValue() : NAN;
	dragSlid = false;
	dragDist = 0.f;
	ParamWidget::onDragStart(e);
}


void FmdModeSlider::onDragMove(const DragMoveEvent& e) {
	if (e.button != GLFW_MOUSE_BUTTON_LEFT)
		return;

	dragDist += e.mouseDelta.norm();
	const float clickPx = 8.f;
	if (dragDist >= clickPx) {
		dragSlid = true;
		engine::ParamQuantity* pq = getParamQuantity();
		if (pq && handle) {
			math::Vec local = APP->scene->mousePos.minus(getAbsoluteOffset(math::Vec()));
			float minX = minHandlePos.x + handle->box.size.x * 0.5f;
			float maxX = maxHandlePos.x + handle->box.size.x * 0.5f;
			float t = (maxX > minX) ? (local.x - minX) / (maxX - minX) : 0.f;
			t = math::clamp(t, 0.f, 1.f);
			pq->setValue(math::rescale(t, 0.f, 1.f, pq->getMinValue(), pq->getMaxValue()));
		}
	}
	ParamWidget::onDragMove(e);
}


void FmdModeSlider::onDragEnd(const DragEndEvent& e) {
	if (e.button != GLFW_MOUSE_BUTTON_LEFT)
		return;

	engine::ParamQuantity* pq = getParamQuantity();
	if (pq && !dragSlid) {
		int mods = APP->window->getMods();
		if ((mods & RACK_MOD_MASK) == RACK_MOD_CTRL) {
			if (pq->isMin())
				pq->setMax();
			else
				pq->setValue(std::round(pq->getValue()) - 1.f);
		}
		else if ((mods & RACK_MOD_MASK) == 0) {
			if (pq->isMax())
				pq->setMin();
			else
				pq->setValue(std::round(pq->getValue()) + 1.f);
		}
	}

	if (pq && module && !std::isnan(dragOldValue) && dragOldValue != pq->getValue()) {
		history::ParamChange* h = new history::ParamChange;
		h->name = "move " + pq->getLabel();
		h->moduleId = module->id;
		h->paramId = paramId;
		h->oldValue = dragOldValue;
		h->newValue = pq->getValue();
		APP->history->push(h);
	}
	dragOldValue = NAN;
	dragSlid = false;
	ParamWidget::onDragEnd(e);
}


void FmdModeSlider::onDoubleClick(const DoubleClickEvent& e) {
	widget::OpaqueWidget::onDoubleClick(e);
}


void FmdModeSlider::onHoverScroll(const HoverScrollEvent& e) {
	ParamWidget::onHoverScroll(e);
}


// ---------------------------------------------------------------------------
// Ports
// ---------------------------------------------------------------------------

FmdPort::FmdPort() {
	setSvg(loadRes("res/common/Jack.svg"));
}


} // namespace fmd
