#pragma once

#include "ComplexControl.hpp"

namespace cpx {

struct SwitchableComplexControl : Widget {
  ComputerscareComplexBase* module = nullptr;
  int paramIndex = 0;
  int polarParamIndex = -1;
  int modeParamId = -1;
  int laneIndex = -1;
  int lastMode = -1;
  bool lastFaded = false;
  bool showDisabledOverlay = false;
  ComplexXYMaxMode arrowMaxMode = ComplexXYMaxMode::Rectangular;
  float arrowMaxVoltage = 10.f;
  NVGcolor disabledOverlayColor = nvgRGBA(120, 120, 120, 135);
  ComplexControl* control = nullptr;

  SwitchableComplexControl(ComputerscareComplexBase* module, int paramIndex,
                           int polarParamIndex, int modeParamId,
                           ComplexXYMaxMode arrowMaxMode, float arrowMaxVoltage,
                           int laneIndex = -1,
                           bool showDisabledOverlay = false) {
    this->module = module;
    this->paramIndex = paramIndex;
    this->polarParamIndex = polarParamIndex;
    this->modeParamId = modeParamId;
    this->arrowMaxMode = arrowMaxMode;
    this->arrowMaxVoltage = arrowMaxVoltage;
    this->laneIndex = laneIndex;
    this->showDisabledOverlay = showDisabledOverlay;
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
    if (arrowMaxMode == ComplexXYMaxMode::Radial)
      control->setArrowRadialMax(arrowMaxVoltage);
    else
      control->setArrowRectangularMax(arrowMaxVoltage);
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

}  // namespace cpx
