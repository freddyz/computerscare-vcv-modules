#pragma once

#include "../Computerscare.hpp"
#include "../complex/ComplexWidgets.hpp"
#include "NudiebugState.hpp"

namespace nudiebug {

template <typename ModuleT>
inline void processDebugger(ModuleT* module) {
  const int zInputMode = module->params[ModuleT::Z_INPUT_MODE].getValue();
  const int zOutputMode = module->params[ModuleT::Z_OUTPUT_MODE].getValue();

  std::vector<std::vector<int>> inputCompolyphony =
      module->getInputCompolyphony({ModuleT::Z_INPUT_MODE}, {ModuleT::Z_INPUT});
  const int compolyChannels =
      module->calcOutputCompolyphony(0, inputCompolyphony);
  module->snapshot.compolyChannels = compolyChannels;

  module->setOutputChannels(ModuleT::Z_OUTPUT, zOutputMode, compolyChannels);

  for (int c = 0; c < compolyChannels; c++) {
    std::vector<float> z =
        module->getComplexVoltage(c, ModuleT::Z_INPUT, zInputMode,
                                  ModuleT::WRAP_NORMAL, inputCompolyphony);
    module->snapshot.rectX[c] = z[0];
    module->snapshot.rectY[c] = z[1];
    module->snapshot.polarR[c] = z[2];
    module->snapshot.polarTheta[c] = z[3];
    module->setOutputVoltages(ModuleT::Z_OUTPUT, zOutputMode, c, z[0], z[1],
                              z[2], z[3]);
  }

  module->snapshot.leftChannels =
      module->inputs[ModuleT::Z_INPUT].getChannels();
  module->snapshot.rightChannels =
      module->inputs[ModuleT::Z_INPUT + 1].getChannels();

  for (int c = 0; c < kMaxChannels; c++) {
    module->snapshot.leftVoltages[c] =
        module->inputs[ModuleT::Z_INPUT].getVoltage(c);
    module->snapshot.rightVoltages[c] =
        module->inputs[ModuleT::Z_INPUT + 1].getVoltage(c);
    if (c >= compolyChannels) {
      module->snapshot.rectX[c] = 0.f;
      module->snapshot.rectY[c] = 0.f;
      module->snapshot.polarR[c] = 0.f;
      module->snapshot.polarTheta[c] = 0.f;
    }
  }
}

}  // namespace nudiebug
