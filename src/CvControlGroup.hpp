#pragma once

#include "Computerscare.hpp"

namespace computerscare {

enum class CvControlGroupMode {
  Column,
  Row,
  Patrix,
};

struct CvControlGroupLayout {
  Vec size;
  Vec cvInputPos;
  Vec attenuverterPos;
  Vec knobPos;
};

inline CvControlGroupLayout cvControlGroupLayout(CvControlGroupMode mode) {
  switch (mode) {
    case CvControlGroupMode::Column:
      return {
          Vec(28.f, 76.f),
          Vec(0.f, 0.f),
          Vec(4.f, 32.f),
          Vec(0.f, 56.f),
      };
    case CvControlGroupMode::Row:
      return {
          Vec(76.f, 24.f),
          Vec(0.f, 0.f),
          Vec(30.f, 5.f),
          Vec(52.f, 1.f),
      };
    case CvControlGroupMode::Patrix:
      return {
          Vec(46.f, 28.f),
          Vec(0.f, 13.f),
          Vec(13.f, 0.f),
          Vec(28.f, 3.f),
      };
  }

  return cvControlGroupLayout(CvControlGroupMode::Column);
}

struct CvControlGroupIds {
  int knobParamId = -1;
  int attenuverterParamId = -1;
  int cvInputId = -1;

  CvControlGroupIds() {}
  CvControlGroupIds(int knobParamId, int attenuverterParamId, int cvInputId)
      : knobParamId(knobParamId),
        attenuverterParamId(attenuverterParamId),
        cvInputId(cvInputId) {}
};

struct CvControlGroupWidgets {
  ParamWidget* knob = nullptr;
  ParamWidget* attenuverter = nullptr;
  PortWidget* cvInput = nullptr;
  Rect box;
};

template <typename KnobWidget = SmoothKnob,
          typename AttenuverterWidget = SmallKnob,
          typename CvInputWidget = InPort>
CvControlGroupWidgets addCvControlGroup(ModuleWidget* parent, Module* module,
                                        Vec pos, CvControlGroupIds ids,
                                        CvControlGroupLayout layout) {
  CvControlGroupWidgets widgets;
  widgets.box = Rect(pos, layout.size);

  if (ids.cvInputId >= 0) {
    widgets.cvInput = createInput<CvInputWidget>(pos.plus(layout.cvInputPos),
                                                 module, ids.cvInputId);
    parent->addInput(widgets.cvInput);
  }

  if (ids.attenuverterParamId >= 0) {
    widgets.attenuverter = createParam<AttenuverterWidget>(
        pos.plus(layout.attenuverterPos), module, ids.attenuverterParamId);
    parent->addParam(widgets.attenuverter);
  }

  if (ids.knobParamId >= 0) {
    widgets.knob = createParam<KnobWidget>(pos.plus(layout.knobPos), module,
                                           ids.knobParamId);
    parent->addParam(widgets.knob);
  }

  return widgets;
}

template <typename KnobWidget = SmoothKnob,
          typename AttenuverterWidget = SmallKnob,
          typename CvInputWidget = InPort>
CvControlGroupWidgets addCvControlGroup(ModuleWidget* parent, Module* module,
                                        Vec pos, CvControlGroupIds ids,
                                        CvControlGroupMode mode) {
  return addCvControlGroup<KnobWidget, AttenuverterWidget, CvInputWidget>(
      parent, module, pos, ids, cvControlGroupLayout(mode));
}

}  // namespace computerscare
