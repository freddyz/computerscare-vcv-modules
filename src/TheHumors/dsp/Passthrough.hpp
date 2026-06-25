#pragma once

namespace the_humors {
namespace dsp {

template <typename Inputs, typename Outputs>
void processPassthrough(Inputs& inputs, Outputs& outputs, int inputOffset,
                        int outputOffset, int count) {
  for (int i = 0; i < count; i++) {
    const int channels = inputs[inputOffset + i].getChannels();
    outputs[outputOffset + i].setChannels(channels);
    for (int c = 0; c < channels; c++) {
      outputs[outputOffset + i].setVoltage(
          inputs[inputOffset + i].getVoltage(c), c);
    }
  }
}

}  // namespace dsp
}  // namespace the_humors
