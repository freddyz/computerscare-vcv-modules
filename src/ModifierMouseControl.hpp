#pragma once

#include "Computerscare.hpp"

struct ComputerscareMouseControlTab {
  float y = 0.f;
  const char* label = "";

  ComputerscareMouseControlTab() = default;
  ComputerscareMouseControlTab(float y, const char* label)
      : y(y), label(label) {}
};

struct ComputerscareModifierMouseControl {
  bool hovered = false;
  bool focused = false;
  bool editing = false;
  int controlMode = 0;
  math::Vec dragStart;
  float dragStartX = 0.f;
  float dragStartY = 0.f;

  static bool& editKeyOn() {
    static bool on = false;
    return on;
  }

  static float clampParamValue(engine::ParamQuantity* pq, float value) {
    if (!pq || !pq->isBounded()) return value;
    return math::clamp(value, pq->getMinValue(), pq->getMaxValue());
  }

  static void setParamValue(engine::ParamQuantity* pq, float value) {
    if (!pq) return;
    pq->setValue(clampParamValue(pq, value));
  }

  static float getRackModSpeed() {
    if (!APP || !APP->window) return 1.f;
    int mods = APP->window->getMods() & (RACK_MOD_CTRL | GLFW_MOD_SHIFT);
    if (mods == RACK_MOD_CTRL) return 1.f / 10.f;
    if (mods == GLFW_MOD_SHIFT) return 4.f;
    if (mods == (RACK_MOD_CTRL | GLFW_MOD_SHIFT)) return 1.f / 100.f;
    return 1.f;
  }

  int getControlMode() const { return controlMode; }

  bool isSecondaryMode() const { return controlMode == 1; }

  static bool isEditToggleKey(const event::HoverKey& e) {
    return e.keyName == "q";
  }

  static bool isModeCycleKey(const event::HoverKey& e) {
    return e.keyName == "a";
  }

  bool shouldControl(Widget* owner) const {
    return editKeyOn() && isMouseOverOwner(owner);
  }

  bool shouldShowHint(Widget* owner) const { return isMouseOverOwner(owner); }

  static bool isMouseOverOwner(Widget* owner) {
    if (!owner || !APP || !APP->scene) return false;
    math::Vec mouse = APP->scene->getMousePos();
    math::Vec pos = owner->getAbsoluteOffset(math::Vec());
    float zoom = owner->getAbsoluteZoom();
    math::Vec size = owner->box.size.mult(zoom);
    return mouse.x >= pos.x && mouse.y >= pos.y && mouse.x <= pos.x + size.x &&
           mouse.y <= pos.y + size.y;
  }

  static void drawEditBadge(const Widget::DrawArgs& args, math::Vec pos) {
    if (!editKeyOn()) return;
    double t = glfwGetTime();
    float pulse = 0.5f + 0.5f * std::sin((float)t * 5.5f);
    float radius = 8.5f + pulse * 1.8f;
    float alpha = 0.72f + pulse * 0.28f;
    nvgSave(args.vg);
    nvgBeginPath(args.vg);
    nvgCircle(args.vg, pos.x, pos.y, radius);
    nvgFillColor(args.vg, nvgRGBAf(1.f, 0.48f, 0.17f, alpha));
    nvgFill(args.vg);
    nvgStrokeWidth(args.vg, 1.5f + pulse * 0.8f);
    nvgStrokeColor(args.vg, nvgRGB(0x21, 0x12, 0x08));
    nvgStroke(args.vg);
    std::shared_ptr<Font> font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/Oswald-Regular.ttf"));
    if (font) {
      nvgFontFaceId(args.vg, font->handle);
      nvgFontSize(args.vg, 13.f);
      nvgTextLetterSpacing(args.vg, 0.f);
      nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
      nvgFillColor(args.vg, nvgRGB(0xff, 0xfb, 0xf2));
      nvgText(args.vg, pos.x, pos.y + 0.5f, "Q", nullptr);
    }
    nvgRestore(args.vg);
  }

  static void drawPanelHint(const Widget::DrawArgs& args, math::Rect panelRect,
                            Widget* owner) {
    if (!isMouseOverOwner(owner)) return;
    bool isActive = editKeyOn();
    NVGcolor accent =
        isActive ? nvgRGB(0xff, 0x7b, 0x2c) : nvgRGBA(0xff, 0xfb, 0xf2, 0x9c);

    nvgSave(args.vg);
    if (isActive) {
      nvgBeginPath(args.vg);
      nvgRect(args.vg, panelRect.pos.x, panelRect.pos.y, 3.f, panelRect.size.y);
      nvgFillColor(args.vg, accent);
      nvgFill(args.vg);
    }

    std::shared_ptr<Font> font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/Oswald-Regular.ttf"));
    if (font) {
      const char* label = "Q EDIT  A MODE";
      nvgFontFaceId(args.vg, font->handle);
      nvgFontSize(args.vg, 16.f);
      nvgTextLetterSpacing(args.vg, 0.f);
      nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
      float x = panelRect.pos.x + 14.f;
      float y = panelRect.pos.y + 14.f;
      float bounds[4] = {};
      nvgTextBounds(args.vg, x, y, label, nullptr, bounds);
      nvgBeginPath(args.vg);
      nvgRoundedRect(args.vg, bounds[0] - 7.f, bounds[1] - 5.f,
                     bounds[2] - bounds[0] + 14.f, bounds[3] - bounds[1] + 10.f,
                     2.f);
      nvgFillColor(args.vg, nvgRGBA(0x12, 0x10, 0x16, 0xe8));
      nvgFill(args.vg);
      nvgFillColor(args.vg, accent);
      nvgText(args.vg, x, y, label, nullptr);
    }
    nvgRestore(args.vg);
  }

  static void drawControlTabs(
      const Widget::DrawArgs& args, float panelRight,
      const std::vector<ComputerscareMouseControlTab>& tabs) {
    if (!editKeyOn() || tabs.empty()) return;
    std::shared_ptr<Font> font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/Oswald-Regular.ttf"));
    if (!font) return;

    for (const ComputerscareMouseControlTab& tab : tabs) {
      const float tabW = 52.f;
      const float tabH = 16.f;
      const float x = panelRight - 18.f;
      nvgSave(args.vg);
      nvgTranslate(args.vg, x, tab.y);
      nvgRotate(args.vg, -(float)M_PI * 0.5f);
      nvgBeginPath(args.vg);
      nvgRoundedRect(args.vg, -tabW * 0.5f - 2.f, -tabH * 0.5f - 2.f,
                     tabW + 4.f, tabH + 4.f, 3.f);
      nvgFillColor(args.vg, nvgRGBA(0x12, 0x10, 0x16, 0xe8));
      nvgFill(args.vg);
      nvgBeginPath(args.vg);
      nvgRoundedRect(args.vg, -tabW * 0.5f, -tabH * 0.5f, tabW, tabH, 2.f);
      nvgFillColor(args.vg, nvgRGB(0xff, 0x7b, 0x2c));
      nvgFill(args.vg);
      nvgStrokeWidth(args.vg, 1.25f);
      nvgStrokeColor(args.vg, nvgRGB(0x21, 0x12, 0x08));
      nvgStroke(args.vg);
      nvgFontFaceId(args.vg, font->handle);
      nvgFontSize(args.vg, 10.f);
      nvgTextLetterSpacing(args.vg, 0.f);
      nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
      nvgFillColor(args.vg, nvgRGB(0xff, 0xfb, 0xf2));
      nvgText(args.vg, 0.f, 0.5f, tab.label, nullptr);
      nvgRestore(args.vg);
    }
  }

  void onHover(const event::Hover& e) { hovered = true; }

  void onLeave(const event::Leave& e) { hovered = false; }

  bool onHoverKey(Widget* owner, const event::HoverKey& e) {
    if (e.isConsumed()) return false;
    if (isModeCycleKey(e)) {
      if (e.action == GLFW_PRESS) controlMode = (controlMode + 1) % 2;
      e.consume(owner);
      return true;
    }
    if (!isEditToggleKey(e)) return false;
    if (e.action == GLFW_PRESS) editKeyOn() = !editKeyOn();
    if (!editKeyOn()) editing = false;
    focused = editKeyOn();
    e.consume(owner);
    return true;
  }

  bool beginDrag(Widget* owner, const event::Button& e, float startX,
                 float startY) {
    if (e.isConsumed() || e.button != GLFW_MOUSE_BUTTON_LEFT ||
        e.action != GLFW_PRESS || !shouldControl(owner)) {
      return false;
    }
    focused = true;
    editing = true;
    dragStart = e.pos;
    dragStartX = startX;
    dragStartY = startY;
    e.consume(owner);
    return true;
  }

  bool endDrag(Widget* owner, const event::Button& e) {
    if (!editing || e.button != GLFW_MOUSE_BUTTON_LEFT ||
        e.action != GLFW_RELEASE) {
      return false;
    }
    editing = false;
    e.consume(owner);
    return true;
  }
};
