#pragma once

#include "../Computerscare.hpp"

// Tiny numbered control. The dot and irregular face turn; the number stays
// upright. Native Knob interaction preserves mapping, reset, and undo.
struct NumberDotKnob : app::Knob {
  int previewValue = 1;
  NumberDotKnob() {
    box.size = Vec(18, 18);
    snap = true;
    smooth = false;
  }
  void draw(const DrawArgs& args) override {
    auto* quantity = getParamQuantity();
    float value = quantity ? quantity->getValue() : float(previewValue);
    float fraction =
        quantity ? quantity->getScaledValue() : (value - 1.f) / 15.f;
    float angle = (-.75f + 1.5f * fraction) * float(M_PI);
    auto* vg = args.vg;
    nvgSave(vg);
    nvgTranslate(vg, 9, 9);
    nvgRotate(vg, angle);
    nvgBeginPath(vg);
    for (int i = 0; i < 13; ++i) {
      float a = i * 2.f * float(M_PI) / 13.f;
      float radius = 8.6f * (.96f + .045f * std::sin(i * 7.3f + 2.f));
      float x = std::sin(a) * radius;
      float y = -std::cos(a) * radius;
      if (i == 0)
        nvgMoveTo(vg, x, y);
      else
        nvgLineTo(vg, x, y);
    }
    nvgClosePath(vg);
    nvgFillColor(vg, nvgRGB(61, 173, 180));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGB(18, 47, 43));
    nvgStrokeWidth(vg, .7f);
    nvgStroke(vg);
    nvgBeginPath(vg);
    nvgCircle(vg, 0, -6.8f, .95f);
    nvgFillColor(vg, nvgRGB(12, 42, 38));
    nvgFill(vg);
    nvgRestore(vg);
    auto font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/DejaVuSansMono.ttf"));
    if (font) {
      std::string text = std::to_string(int(std::round(value)));
      nvgFontFaceId(vg, font->handle);
      nvgFontSize(vg, 8);
      nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
      nvgFillColor(vg, nvgRGB(9, 27, 24));
      nvgText(vg, 9, 9, text.c_str(), nullptr);
    }
    app::ParamWidget::draw(args);
  }
};
