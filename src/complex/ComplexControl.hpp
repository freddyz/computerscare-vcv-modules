#pragma once

#include "ComplexWidgets.hpp"

namespace cpx {

// ─── ComplexControlPreset ────────────────────────────────────────────────────

enum class ComplexControlPreset {
	Arrow,        // 2 params (x, y) — drag arrow only
	XYKnobs,      // 2 params (x, y) — no arrow; user places knobs
	RThetaKnobs,  // 2 params (r, θ) — no arrow; user places knobs
	ArrowXY,      // 2 params (x, y) — arrow; user may also place XY knobs
	ArrowPolar,   // 2 params (r, θ) — arrow; user may also place polar knobs
};

enum class ComplexControlStyle {
	Normal,
	Faded,
};

struct ComplexControlSmallKnob : SmallKnob {
	std::shared_ptr<Svg> normalSvg = APP->window->loadSvg(asset::plugin(
		pluginInstance, "res/components/computerscare-small-knob-effed.svg"));
	std::shared_ptr<Svg> fadedSvg = APP->window->loadSvg(asset::plugin(
		pluginInstance, "res/components/computerscare-small-knob-effed-disabled.svg"));
	bool faded = false;

	void setFaded(bool shouldFade) {
		if (faded == shouldFade)
			return;
		faded = shouldFade;
		setSvg(faded ? fadedSvg : normalSvg);
	}
};

// ─── ComplexControl ──────────────────────────────────────────────────────────
// Composite widget: optional arrow drag + optional display.
//
// Module constructor (only required boilerplate):
//   ComplexControl::configParams(this, MY_PARAM, ComplexControlPreset::ArrowXY);
//
// Widget constructor:
//   auto* ctrl = new ComplexControl(module, MY_PARAM, ComplexControlPreset::ArrowXY);
//   ctrl->box = {{10.f, 40.f}, {70.f, 70.f}};
//   ctrl->setShowDisplay(true);
//   addChild(ctrl);
//
//   addParam(createParam<SmallKnob>(Vec(90, 50), module, MY_PARAM + 0));
//   addParam(createParam<SmallKnob>(Vec(90, 80), module, MY_PARAM + 1));
//   auto [la, lb] = ComplexControl::presetLabels(preset, Vec(88, 38), Vec(88, 68));
//   addChild(la); addChild(lb);

struct ComplexControl : Widget {
	ComputerscareComplexBase* module = nullptr;
	int firstParamIdx = 0;
	ComplexControlPreset preset = ComplexControlPreset::ArrowXY;
	ComplexControlStyle style = ComplexControlStyle::Normal;

	ComplexXY*            arrowWidget = nullptr;
	ComplexDisplayWidget* display     = nullptr;
	TransformWidget*      knobA       = nullptr;
	TransformWidget*      knobB       = nullptr;
	ComplexControlSmallKnob* knobAWidget = nullptr;
	ComplexControlSmallKnob* knobBWidget = nullptr;
	ScaledSvgWidget*      labelA      = nullptr;
	ScaledSvgWidget*      labelB      = nullptr;

	// ── Param registration ────────────────────────────────────────────────────
	// Call once from the module constructor.  Always registers exactly 2 params.
	static void configParams(Module* m, int firstIdx, ComplexControlPreset p) {
		bool polar = (p == ComplexControlPreset::RThetaKnobs ||
		              p == ComplexControlPreset::ArrowPolar);
		if (polar) {
			m->configParam(firstIdx,     0.f,          10.f,          1.f, "r");
			m->configParam(firstIdx + 1, -(float)M_PI, (float)M_PI,   0.f, "θ");
		} else {
			m->configParam(firstIdx,     -10.f, 10.f, 0.f, "x");
			m->configParam(firstIdx + 1, -10.f, 10.f, 0.f, "y");
		}
	}

	static constexpr int paramCount() { return 2; }

	// ── SVG label helpers ─────────────────────────────────────────────────────
	// Returns a ready-to-addChild() ScaledSvgWidget label at pos.
	static ScaledSvgWidget* makeLabel(const std::string& svgFilename, Vec pos,
	                                  float scale = 0.5f) {
		ScaledSvgWidget* w = new ScaledSvgWidget(scale);
		w->box.pos = pos;
		w->setSVG("res/complex-labels/" + svgFilename);
		return w;
	}

	// Returns the correct label pair for the preset (x/y or r/θ).
	static std::pair<ScaledSvgWidget*, ScaledSvgWidget*>
	presetLabels(ComplexControlPreset p, Vec pos0, Vec pos1, float scale = 0.5f) {
		bool polar = (p == ComplexControlPreset::RThetaKnobs ||
		              p == ComplexControlPreset::ArrowPolar);
		return {
			makeLabel(polar ? "r.svg"     : "x.svg",   pos0, scale),
			makeLabel(polar ? "theta.svg" : "y.svg",   pos1, scale),
		};
	}

	// ── Construction ─────────────────────────────────────────────────────────
	ComplexControl(ComputerscareComplexBase* m, int firstIdx, ComplexControlPreset p)
		: module(m), firstParamIdx(firstIdx), preset(p) {

		bool hasArrow = (preset == ComplexControlPreset::Arrow    ||
		                 preset == ComplexControlPreset::ArrowXY   ||
		                 preset == ComplexControlPreset::ArrowPolar);
		if (hasArrow) {
			arrowWidget = new ComplexXY(m, firstIdx);
			addChild(arrowWidget);
		}

		bool hasKnobs = (preset == ComplexControlPreset::XYKnobs ||
		                 preset == ComplexControlPreset::RThetaKnobs ||
		                 preset == ComplexControlPreset::ArrowXY ||
		                 preset == ComplexControlPreset::ArrowPolar);
		if (hasKnobs) {
			float knobScale = hasArrow ? 0.52f : 0.78f;

			knobA = new TransformWidget();
			knobA->scale(knobScale);
			knobAWidget = createParam<ComplexControlSmallKnob>(Vec(0.f, 0.f), m, firstIdx);
			knobA->addChild(knobAWidget);
			addChild(knobA);

			knobB = new TransformWidget();
			knobB->scale(knobScale);
			knobBWidget = createParam<ComplexControlSmallKnob>(Vec(0.f, 0.f), m, firstIdx + 1);
			knobB->addChild(knobBWidget);
			addChild(knobB);

			auto labels = presetLabels(preset, Vec(0.f, 0.f), Vec(0.f, 0.f), hasArrow ? 0.26f : 0.34f);
			labelA = labels.first;
			labelB = labels.second;
			addChild(labelA);
			addChild(labelB);
		}
	}

	void setStyle(ComplexControlStyle newStyle) {
		style = newStyle;
		bool shouldFade = style == ComplexControlStyle::Faded;
		if (arrowWidget)
			arrowWidget->setFaded(shouldFade);
		if (knobAWidget)
			knobAWidget->setFaded(shouldFade);
		if (knobBWidget)
			knobBWidget->setFaded(shouldFade);
	}

	// ── Display toggle ────────────────────────────────────────────────────────
	void setShowDisplay(bool show) {
		if (show == (display != nullptr)) return;

		if (show) {
			display = new ComplexDisplayWidget();
			display->module = module;
			display->paramX = firstParamIdx;
			display->paramY = firstParamIdx + 1;
			bool polar = (preset == ComplexControlPreset::RThetaKnobs ||
			              preset == ComplexControlPreset::ArrowPolar);
			display->displayMode = polar
				? ComplexDisplayWidget::DisplayMode::Polar
				: ComplexDisplayWidget::DisplayMode::Rect;
			addChild(display);
		} else {
			removeChild(display);
			delete display;
			display = nullptr;
		}
		layoutChildren();
	}

	// ── Layout ────────────────────────────────────────────────────────────────
	void layoutChildren() {
		constexpr float displayH = 18.f;
		float arrowH = box.size.y - (display ? displayH : 0.f);

		if (arrowWidget) {
			arrowWidget->box = Rect(Vec(0, 0), Vec(box.size.x, arrowH));
		}
		if (knobA && knobB) {
			if (arrowWidget) {
				knobA->box.pos = Vec(1.f, 8.f);
				knobB->box.pos = Vec(box.size.x - 10.f, 8.f);
			} else {
				knobA->box.pos = Vec(1.f, 8.f);
				knobB->box.pos = Vec(box.size.x - 15.f, 8.f);
			}
		}
		if (labelA && labelB) {
			if (arrowWidget) {
				labelA->box.pos = Vec(2.f, 3.f);
				labelB->box.pos = Vec(box.size.x - 9.f, 3.f);
			} else {
				labelA->box.pos = Vec(4.f, 1.f);
				labelB->box.pos = Vec(box.size.x - 12.f, 1.f);
			}
		}
		if (display) {
			display->box = Rect(Vec(0, arrowH), Vec(box.size.x, displayH));
		}
	}

	void onResize(const ResizeEvent& e) override {
		layoutChildren();
		Widget::onResize(e);
	}
};

} // namespace cpx
