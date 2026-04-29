#pragma once

#include "../Computerscare.hpp"
#include "../complex/ComplexWidgets.hpp"
#include "NudiebugState.hpp"

namespace nudiebug {

template <typename ModuleT>
inline void updateDisplaySnapshot(ModuleT* module, int zInputMode,
                                  int compolyChannels,
                                  const typename ModuleT::CompolyInputInfo&
                                      inputInfo) {
  const bool needsComplexValues =
      module->displayOptions.textMode == TEXT_COMPOLY_RECT ||
      module->displayOptions.textMode == TEXT_COMPOLY_POLAR ||
      module->displayOptions.plotEnabled;

  module->snapshot.compolyChannels = compolyChannels;
  module->snapshot.leftChannels = module->inputs[ModuleT::Z_INPUT].getChannels();
  module->snapshot.rightChannels =
      module->inputs[ModuleT::Z_INPUT + 1].getChannels();

  for (int c = 0; c < kMaxChannels; c++) {
    module->snapshot.leftVoltages[c] =
        module->inputs[ModuleT::Z_INPUT].getVoltage(c);
    module->snapshot.rightVoltages[c] =
        module->inputs[ModuleT::Z_INPUT + 1].getVoltage(c);

    if (needsComplexValues && c < compolyChannels) {
      typename ModuleT::ComplexSample z = module->getComplexSample(
          c, ModuleT::Z_INPUT, zInputMode, ModuleT::WRAP_NORMAL,
          inputInfo);
      module->snapshot.rectX[c] = z.x;
      module->snapshot.rectY[c] = z.y;
      module->snapshot.polarR[c] = z.r;
      module->snapshot.polarTheta[c] = z.theta;
    } else {
      module->snapshot.rectX[c] = 0.f;
      module->snapshot.rectY[c] = 0.f;
      module->snapshot.polarR[c] = 0.f;
      module->snapshot.polarTheta[c] = 0.f;
    }
  }
}

template <typename ModuleT>
inline void processDebugger(ModuleT* module, bool updateDisplay) {
  const int zInputMode = module->params[ModuleT::Z_INPUT_MODE].getValue();
  const int zOutputMode = module->params[ModuleT::Z_OUTPUT_MODE].getValue();
  const bool outputConnected = module->outputPairConnected(ModuleT::Z_OUTPUT);
  const bool displayActive = module->displayOptions.textEnabled ||
                             module->displayOptions.visualizationEnabled ||
                             module->displayOptions.plotEnabled;

  if (!outputConnected && !(displayActive && updateDisplay)) return;

  typename ModuleT::CompolyInputInfo inputInfo =
      module->getInputCompolyInfo(zInputMode, ModuleT::Z_INPUT);
  const int compolyChannels =
      cpx::complex_math::outputCompolyphony(0, inputInfo.compolyChannels);

  if (outputConnected) {
    module->setOutputChannels(ModuleT::Z_OUTPUT, zOutputMode, compolyChannels);
    if (ModuleT::coordinateFamiliesMatch(zInputMode, zOutputMode)) {
      module->routeComplexPairSameFamily(
          ModuleT::Z_INPUT, ModuleT::Z_OUTPUT, zInputMode, zOutputMode,
          ModuleT::WRAP_NORMAL, compolyChannels, inputInfo);
    } else {
      module->routeComplexPairConverted(
          ModuleT::Z_INPUT, ModuleT::Z_OUTPUT, zInputMode, zOutputMode,
          ModuleT::WRAP_NORMAL, compolyChannels, inputInfo);
    }
  } else {
    module->outputs[ModuleT::Z_OUTPUT].setChannels(0);
    module->outputs[ModuleT::Z_OUTPUT + 1].setChannels(0);
  }

  if (displayActive && updateDisplay) {
    updateDisplaySnapshot(module, zInputMode, compolyChannels, inputInfo);
  }
}

}  // namespace nudiebug
