#pragma once

#include "../Computerscare.hpp"
#include "../complex/ComplexWidgets.hpp"
#include "../complex/CompolyInputFormation.hpp"
#include "NudiebugState.hpp"

namespace nudiebug {

template <typename ModuleT>
inline void updateDisplaySnapshot(
    ModuleT* module, int zInputMode, int compolyChannels,
    const cpx::compoly::PortChannels& ports,
    cpx::compoly::PortChannelCounts portChannels,
    cpx::compoly::input_formation::Options inputFormationOptions) {
  const bool needsComplexValues =
      module->displayOptions.displayType == DISPLAY_TYPE_COMPOLY ||
      module->displayOptions.plotEnabled;

  module->snapshot.compolyChannels = compolyChannels;
  module->snapshot.leftChannels =
      module->inputs[ModuleT::Z_INPUT].getChannels();
  module->snapshot.rightChannels =
      module->inputs[ModuleT::Z_INPUT + 1].getChannels();
  module->snapshot.leftOutputChannels =
      module->outputs[ModuleT::Z_OUTPUT].getChannels();
  module->snapshot.rightOutputChannels =
      module->outputs[ModuleT::Z_OUTPUT + 1].getChannels();

  for (int c = 0; c < kMaxChannels; c++) {
    module->snapshot.leftVoltages[c] =
        module->inputs[ModuleT::Z_INPUT].getVoltage(c);
    module->snapshot.rightVoltages[c] =
        module->inputs[ModuleT::Z_INPUT + 1].getVoltage(c);
    module->snapshot.leftOutputVoltages[c] =
        module->outputs[ModuleT::Z_OUTPUT].getVoltage(c);
    module->snapshot.rightOutputVoltages[c] =
        module->outputs[ModuleT::Z_OUTPUT + 1].getVoltage(c);

    if (needsComplexValues && c < compolyChannels) {
      std::array<float, 2> pair = cpx::compoly::input_formation::pairForLane(
          ports, portChannels, ModuleT::coordinateModeFromParam(zInputMode), c,
          inputFormationOptions);
      cpx::complex_math::Quad z = cpx::complex_math::quadFromPair(
          pair[0], pair[1], ModuleT::coordinateModeFromParam(zInputMode));
      module->snapshot.compolyA[c] = pair[0];
      module->snapshot.compolyB[c] = pair[1];
      module->snapshot.rectX[c] = z.x;
      module->snapshot.rectY[c] = z.y;
      module->snapshot.polarR[c] = z.r;
      module->snapshot.polarTheta[c] = z.theta;
    } else {
      module->snapshot.rectX[c] = 0.f;
      module->snapshot.rectY[c] = 0.f;
      module->snapshot.polarR[c] = 0.f;
      module->snapshot.polarTheta[c] = 0.f;
      module->snapshot.compolyA[c] = 0.f;
      module->snapshot.compolyB[c] = 0.f;
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

  cpx::compoly::input_formation::Options inputFormationOptions(
      cpx::compoly::input_formation::partialPairPolicyFromInt(
          module->params[ModuleT::INTERLEAVED_PARTIAL_PAIR_MODE].getValue()),
      cpx::compoly::input_formation::interleavedBankPolicyFromInt(
          module->params[ModuleT::INTERLEAVED_BANK_MODE].getValue()),
      cpx::compoly::input_formation::separatedLanePolicyFromInt(
          module->params[ModuleT::SEPARATED_LANE_MODE].getValue()));

  cpx::compoly::PortChannels ports = {};
  module->inputs[ModuleT::Z_INPUT].readVoltages(ports.a.data());
  module->inputs[ModuleT::Z_INPUT + 1].readVoltages(ports.b.data());
  cpx::compoly::PortChannelCounts portChannels(
      module->inputs[ModuleT::Z_INPUT].getChannels(),
      module->inputs[ModuleT::Z_INPUT + 1].getChannels());
  const int compolyChannels = cpx::compoly::outputCompolyLanes(
      0, cpx::compoly::input_formation::lanesForInput(
             ModuleT::coordinateModeFromParam(zInputMode), portChannels,
             inputFormationOptions));

  const bool computeOutputs =
      outputConnected || (displayActive && updateDisplay);
  if (computeOutputs) {
    module->setOutputChannels(ModuleT::Z_OUTPUT, zOutputMode, compolyChannels);
    for (int c = 0; c < compolyChannels; c++) {
      std::array<float, 2> pair = cpx::compoly::input_formation::pairForLane(
          ports, portChannels, ModuleT::coordinateModeFromParam(zInputMode), c,
          inputFormationOptions);
      if (ModuleT::coordinateFamiliesMatch(zInputMode, zOutputMode)) {
        module->setOutputCoordinatePair(ModuleT::Z_OUTPUT, zOutputMode, c,
                                        pair[0], pair[1]);
      } else {
        cpx::complex_math::Quad z = cpx::complex_math::quadFromPair(
            pair[0], pair[1], ModuleT::coordinateModeFromParam(zInputMode));
        module->setOutputVoltages(ModuleT::Z_OUTPUT, zOutputMode, c, z.x, z.y,
                                  z.r, z.theta);
      }
    }
  } else {
    module->outputs[ModuleT::Z_OUTPUT].setChannels(0);
    module->outputs[ModuleT::Z_OUTPUT + 1].setChannels(0);
  }

  if (displayActive && updateDisplay) {
    updateDisplaySnapshot(module, zInputMode, compolyChannels, ports,
                          portChannels, inputFormationOptions);
  }
}

}  // namespace nudiebug
