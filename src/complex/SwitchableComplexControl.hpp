#pragma once

#include "ComplexControl.hpp"

namespace cpx {

struct ComplexControlViewModeSwitch : app::Switch {
  std::string label;
  int textAlign = NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE;
  float fontSize = 9.f;

  void onButton(const event::Button& e) override { Widget::onButton(e); }

  void draw(const DrawArgs& args) override {
    nvgFontSize(args.vg, fontSize);
    nvgTextAlign(args.vg, textAlign);
    nvgFillColor(args.vg, nvgRGB(28, 34, 28));
    float textX = box.size.x * 0.5f;
    if (textAlign & NVG_ALIGN_RIGHT)
      textX = box.size.x - 1.f;
    else if (textAlign & NVG_ALIGN_LEFT)
      textX = 1.f;
    nvgText(args.vg, textX, box.size.y * 0.58f, label.c_str(), nullptr);
  }
};

struct SetAllComplexControlViewModeItem : MenuItem {
  Module* module = nullptr;
  std::vector<int> modeParamIds;
  int mode = 0;

  void onAction(const event::Action& e) override {
    if (!module) return;
    mode = std::max(0, std::min(2, mode));
    for (int modeParamId : modeParamIds) {
      if (modeParamId >= 0 && modeParamId < (int)module->params.size()) {
        module->params[modeParamId].setValue(mode);
      }
    }
  }
};

inline void addSetAllComplexControlViewModeMenu(
    Menu* menu, Module* module, const std::vector<int>& modeParamIds) {
  menu->addChild(new MenuSeparator);
  menu->addChild(construct<MenuLabel>(&MenuLabel::text, "View"));
  menu->addChild(construct<SetAllComplexControlViewModeItem>(
      &MenuItem::text, "set all to arrow",
      &SetAllComplexControlViewModeItem::module, module,
      &SetAllComplexControlViewModeItem::modeParamIds, modeParamIds,
      &SetAllComplexControlViewModeItem::mode, 0));
  menu->addChild(construct<SetAllComplexControlViewModeItem>(
      &MenuItem::text, "set all to xy",
      &SetAllComplexControlViewModeItem::module, module,
      &SetAllComplexControlViewModeItem::modeParamIds, modeParamIds,
      &SetAllComplexControlViewModeItem::mode, 1));
  menu->addChild(construct<SetAllComplexControlViewModeItem>(
      &MenuItem::text, "set all to rθ",
      &SetAllComplexControlViewModeItem::module, module,
      &SetAllComplexControlViewModeItem::modeParamIds, modeParamIds,
      &SetAllComplexControlViewModeItem::mode, 2));
}

struct SwitchableComplexControl : Widget {
  ComputerscareComplexBase* module = nullptr;
  int paramIndex = 0;
  int polarParamIndex = -1;
  int modeParamId = -1;
  int laneIndex = -1;
  int lastMode = -1;
  std::string controlName;
  bool lastFaded = false;
  bool showDisabledOverlay = false;
  ComplexXYMaxMode arrowMaxMode = ComplexXYMaxMode::Rectangular;
  float arrowMaxVoltage = 10.f;
  float arrowDrawingScale = 1.f;
  float arrowYOffset = 0.f;
  NVGcolor disabledOverlayColor = nvgRGBA(120, 120, 120, 135);
  ComplexControl* control = nullptr;

  SwitchableComplexControl(ComputerscareComplexBase* module, int paramIndex,
                           int polarParamIndex, int modeParamId,
                           ComplexXYMaxMode arrowMaxMode, float arrowMaxVoltage,
                           int laneIndex = -1, bool showDisabledOverlay = false,
                           std::string controlName = "") {
    this->module = module;
    this->paramIndex = paramIndex;
    this->polarParamIndex = polarParamIndex;
    this->modeParamId = modeParamId;
    this->arrowMaxMode = arrowMaxMode;
    this->arrowMaxVoltage = arrowMaxVoltage;
    this->laneIndex = laneIndex;
    this->showDisabledOverlay = showDisabledOverlay;
    this->controlName = controlName;
  }

  void setControlName(const std::string& name) {
    controlName = name;
    if (control) control->setControlName(controlName);
  }

  void setArrowDrawingScale(float scale) {
    arrowDrawingScale = std::max(0.f, scale);
    if (control) control->setArrowDrawingScale(arrowDrawingScale);
  }

  void setArrowYOffset(float offset) {
    arrowYOffset = offset;
    if (control) control->setArrowYOffset(arrowYOffset);
  }

  int mode() const {
    if (module && modeParamId >= 0)
      return module->params[modeParamId].getValue();
    return 0;
  }

  bool isFaded() const {
    return module && laneIndex >= 0 && laneIndex >= module->polyChannels;
  }

  void rebuildControl(int mode) {
    if (control) {
      removeChild(control);
      delete control;
      control = nullptr;
    }

    ComplexControlPreset preset = ComplexControlPreset::Arrow;
    if (mode == 1)
      preset = ComplexControlPreset::XYKnobs;
    else if (mode == 2)
      preset = ComplexControlPreset::RThetaKnobs;

    int controlParamIndex = mode == 2 ? polarParamIndex : paramIndex;
    control = new ComplexControl(module, controlParamIndex, preset);
    control->setControlName(controlName);
    if (arrowMaxMode == ComplexXYMaxMode::Radial)
      control->setArrowRadialMax(arrowMaxVoltage);
    else
      control->setArrowRectangularMax(arrowMaxVoltage);
    control->setArrowDrawingScale(arrowDrawingScale);
    control->setArrowYOffset(arrowYOffset);
    control->box = Rect(Vec(0.f, 0.f), box.size);
    control->setStyle(isFaded() ? ComplexControlStyle::Faded
                                : ComplexControlStyle::Normal);
    control->layoutChildren();
    addChildBottom(control);
  }

  void clampPolarRadius() {
    if (!module || polarParamIndex < 0) return;
    if (arrowMaxMode == ComplexXYMaxMode::Radial) {
      module->params[polarParamIndex].setValue(
          std::max(0.f, std::min(arrowMaxVoltage,
                                 module->params[polarParamIndex].getValue())));
    }
  }

  void step() override {
    int currentMode = mode();
    if (currentMode != lastMode) {
      if (lastMode == 2) {
        ComplexControl::syncPolarParamsToRectParams(module, polarParamIndex,
                                                    paramIndex);
      }
      if (currentMode == 2) {
        ComplexControl::syncRectParamsToPolarParams(module, paramIndex,
                                                    polarParamIndex);
        clampPolarRadius();
      }
      lastMode = currentMode;
      rebuildControl(currentMode);
    } else if (currentMode == 2) {
      clampPolarRadius();
      ComplexControl::syncPolarParamsToRectParams(module, polarParamIndex,
                                                  paramIndex);
    }

    bool currentFaded = isFaded();
    if (control && currentFaded != lastFaded) {
      lastFaded = currentFaded;
      control->setStyle(currentFaded ? ComplexControlStyle::Faded
                                     : ComplexControlStyle::Normal);
    }
    Widget::step();
  }

  void draw(const DrawArgs& args) override {
    Widget::draw(args);
    if (showDisabledOverlay && isFaded()) {
      nvgBeginPath(args.vg);
      nvgEllipse(args.vg, box.size.x * 0.5f, box.size.y * 0.5f,
                 box.size.x * 0.5f, box.size.y * 0.5f);
      nvgFillColor(args.vg, disabledOverlayColor);
      nvgFill(args.vg);
    }
  }
};

struct LabeledSwitchableComplexControl : Widget {
  SwitchableComplexControl* control = nullptr;
  ComplexControlViewModeSwitch* modeSwitch = nullptr;

  LabeledSwitchableComplexControl(
      ComputerscareComplexBase* module, int paramIndex, int polarParamIndex,
      int modeParamId, ComplexXYMaxMode arrowMaxMode, float arrowMaxVoltage,
      const std::string& label, Vec controlSize, Vec labelPos, Vec labelSize,
      int laneIndex = -1, bool showDisabledOverlay = false,
      const std::string& controlName = "",
      int labelTextAlign = NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE,
      Vec controlPos = Vec(0.f, 0.f)) {
    control = new SwitchableComplexControl(
        module, paramIndex, polarParamIndex, modeParamId, arrowMaxMode,
        arrowMaxVoltage, laneIndex, showDisabledOverlay, controlName);
    control->box = Rect(controlPos, controlSize);
    addChild(control);

    modeSwitch = createParam<ComplexControlViewModeSwitch>(labelPos, module,
                                                           modeParamId);
    modeSwitch->box = Rect(labelPos, labelSize);
    modeSwitch->label = label;
    modeSwitch->textAlign = labelTextAlign;
    addChild(modeSwitch);
  }

  void setArrowDrawingScale(float scale) {
    if (control) control->setArrowDrawingScale(scale);
  }

  void setArrowYOffset(float offset) {
    if (control) control->setArrowYOffset(offset);
  }
};

}  // namespace cpx
