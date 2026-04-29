/*
        ComputerscareComplexBase

        Compluterxare

        Extends Module with a polyChannels member variable,
        and a polyphony channels widget that is "bound" to the
        the module instance's value

*/

#pragma once

#include "math/ComplexMath.hpp"

using namespace rack;

struct ComputerscareComplexBase;

struct ComputerscareComplexBase : ComputerscareMenuParamModule {
  static constexpr int WRAP_NORMAL =
      static_cast<int>(cpx::compoly::WrapMode::Normal);
  static constexpr int WRAP_CYCLE =
      static_cast<int>(cpx::compoly::WrapMode::Cycle);
  static constexpr int WRAP_MINIMAL =
      static_cast<int>(cpx::compoly::WrapMode::Minimal);
  static constexpr int WRAP_STALL =
      static_cast<int>(cpx::compoly::WrapMode::Stall);
  static constexpr int RECT_INTERLEAVED =
      static_cast<int>(cpx::complex_math::CoordinateMode::RectInterleaved);
  static constexpr int POLAR_INTERLEAVED =
      static_cast<int>(cpx::complex_math::CoordinateMode::PolarInterleaved);
  static constexpr int RECT_SEPARATED =
      static_cast<int>(cpx::complex_math::CoordinateMode::RectSeparated);
  static constexpr int POLAR_SEPARATED =
      static_cast<int>(cpx::complex_math::CoordinateMode::PolarSeparated);

  static cpx::complex_math::CoordinateMode coordinateModeFromParam(int mode) {
    return static_cast<cpx::complex_math::CoordinateMode>(mode);
  }

  static bool outputModeIsPolar(int mode) {
    return cpx::complex_math::isPolar(coordinateModeFromParam(mode));
  }

  static bool outputModeIsInterleaved(int mode) {
    return cpx::complex_math::isInterleaved(coordinateModeFromParam(mode));
  }

  struct ComplexSample {
    float x;
    float y;
    float r;
    float theta;

    ComplexSample() : x(0.f), y(0.f), r(0.f), theta(0.f) {}

    ComplexSample(float x, float y, float r, float theta)
        : x(x), y(y), r(r), theta(theta) {}
  };

  struct CompolyInputInfo {
    int compolyChannels;
    int portAChannels;
    int portBChannels;

    CompolyInputInfo()
        : compolyChannels(0), portAChannels(0), portBChannels(0) {}

    CompolyInputInfo(int compolyChannels, int portAChannels, int portBChannels)
        : compolyChannels(compolyChannels),
          portAChannels(portAChannels),
          portBChannels(portBChannels) {}
  };

  static bool coordinateFamiliesMatch(int firstMode, int secondMode) {
    return cpx::complex_math::isRect(coordinateModeFromParam(firstMode)) ==
           cpx::complex_math::isRect(coordinateModeFromParam(secondMode));
  }

  bool outputPairConnected(int firstPortIndex) {
    return outputs[firstPortIndex].isConnected() ||
           outputs[firstPortIndex + 1].isConnected();
  }

  std::vector<std::vector<int>> getInputCompolyphony(
      std::vector<int> inputModeIndices,
      std::vector<int> inputFirstPortIndices) {
    std::vector<std::vector<int>> output;

    int numInputsToConsider = inputFirstPortIndices.size();

    for (int i = 0; i < numInputsToConsider; i++) {
      std::vector<int> myStuff;
      int inputMode = params[inputModeIndices[i]].getValue();

      int portOnePolyphony = inputs[inputFirstPortIndices[i]].getChannels();
      int portTwoPolyphony = inputs[inputFirstPortIndices[i] + 1].getChannels();
      myStuff.push_back(cpx::complex_math::compolyphonyForInput(
          coordinateModeFromParam(inputMode), portOnePolyphony,
          portTwoPolyphony));

      myStuff.push_back(portOnePolyphony);
      myStuff.push_back(portTwoPolyphony);
      output.push_back(myStuff);
    }

    return output;
  }

  CompolyInputInfo getInputCompolyInfo(int inputMode, int inputFirstPortIndex) {
    int portAChannels = inputs[inputFirstPortIndex].getChannels();
    int portBChannels = inputs[inputFirstPortIndex + 1].getChannels();
    int compolyChannels = cpx::complex_math::compolyphonyForInput(
        coordinateModeFromParam(inputMode), portAChannels, portBChannels);
    return CompolyInputInfo(compolyChannels, portAChannels, portBChannels);
  }

  void setOutputVoltages(int outIndex, int outMode, int compChannelIndex,
                         float x, float y, float r, float theta) {
    int outputBlock = compChannelIndex > 7 ? 1 : 0;
    if (outMode == RECT_INTERLEAVED) {
      // interleaved rectangular
      outputs[outIndex + outputBlock].setVoltage(x,
                                                 (compChannelIndex * 2) % 16);
      outputs[outIndex + outputBlock].setVoltage(
          y, (compChannelIndex * 2 + 1) % 16);
    } else if (outMode == POLAR_INTERLEAVED) {
      // interleaved polar
      outputs[outIndex + outputBlock].setVoltage(r,
                                                 (compChannelIndex * 2) % 16);
      outputs[outIndex + outputBlock].setVoltage(
          cpx::complex_math::thetaRadiansToCableVoltage(theta),
          (compChannelIndex * 2 + 1) % 16);
    } else if (outMode == RECT_SEPARATED) {
      // separated rectangular
      outputs[outIndex].setVoltage(x, compChannelIndex);
      outputs[outIndex + 1].setVoltage(y, compChannelIndex);
    } else if (outMode == POLAR_SEPARATED) {
      // separated polar
      outputs[outIndex].setVoltage(r, compChannelIndex);
      outputs[outIndex + 1].setVoltage(
          cpx::complex_math::thetaRadiansToCableVoltage(theta),
          compChannelIndex);
    }
  }

  void setOutputChannels(int outIndex, int outMode, int compolyChannels) {
    cpx::complex_math::PortChannelCounts counts =
        cpx::complex_math::outputPortChannelCounts(
            coordinateModeFromParam(outMode), compolyChannels);
    outputs[outIndex + 0].setChannels(counts.a);
    outputs[outIndex + 1].setChannels(counts.b);
  }

  void setOutputCoordinatePair(int outIndex, int outMode, int compChannelIndex,
                               float a, float b) {
    int outputBlock = compChannelIndex > 7 ? 1 : 0;
    if (outMode == RECT_INTERLEAVED || outMode == POLAR_INTERLEAVED) {
      outputs[outIndex + outputBlock].setVoltage(a,
                                                 (compChannelIndex * 2) % 16);
      outputs[outIndex + outputBlock].setVoltage(
          b, (compChannelIndex * 2 + 1) % 16);
    } else {
      outputs[outIndex].setVoltage(a, compChannelIndex);
      outputs[outIndex + 1].setVoltage(b, compChannelIndex);
    }
  }

  int calcOutputCompolyphony(
      int knobSetting,
      std::vector<std::vector<int>> inputCompolyphonyChannels) {
    int numInputsToConsider = inputCompolyphonyChannels.size();

    int outputCompolyphony;

    int maxOfInputsCompolyphony = 0;

    for (int i = 0; i < numInputsToConsider; i++) {
      maxOfInputsCompolyphony =
          std::max(maxOfInputsCompolyphony, inputCompolyphonyChannels[i][0]);
    }

    outputCompolyphony = cpx::complex_math::outputCompolyphony(
        knobSetting, maxOfInputsCompolyphony);

    return outputCompolyphony;
  }

  void readComplexInputPairToRect(int firstPortIndex, int inputMode, float* x,
                                  float* y) {
    cpx::complex_math::PortChannels ports = {};
    inputs[firstPortIndex].readVoltages(ports.a.data());
    inputs[firstPortIndex + 1].readVoltages(ports.b.data());

    cpx::complex_math::RectChannels rect = cpx::complex_math::readRectFromPorts(
        ports, coordinateModeFromParam(inputMode));
    for (int c = 0; c < cpx::complex_math::maxChannels; c++) {
      x[c] = rect.x[c];
      y[c] = rect.y[c];
    }
  }

  void writeComplexOutputPairFromRect(int firstPortIndex, int outputMode,
                                      const float* x, const float* y) {
    cpx::complex_math::RectChannels rect = {};
    for (int c = 0; c < cpx::complex_math::maxChannels; c++) {
      rect.x[c] = x[c];
      rect.y[c] = y[c];
    }

    cpx::complex_math::PortChannels ports =
        cpx::complex_math::writePortsFromRect(
            rect, coordinateModeFromParam(outputMode));
    outputs[firstPortIndex].writeVoltages(ports.a.data());
    outputs[firstPortIndex + 1].writeVoltages(ports.b.data());
  }

  /*
          S

          Separated:
                  Complex 0:
                          - port0, ch0
                          - port1, ch0

                  Complex 1:
                          - port0, ch1
                          - port1, ch1


          Interleaved:
                  Complex 0:
                          -port0, ch0
                          -port0, ch1

                  Complex 1:
                          -port0, ch2
                          -port0, ch3
  */

  std::vector<float> getComplexVoltageFromSeparatedInput(
      int outputIndex, int firstPortID, int inputMode, int wrapMode,
      std::vector<int>& inputCompolyphony) {
    std::vector<float> output = {};

    int firstPortIndex = outputIndex;
    int secondPortIndex = outputIndex;
    if (wrapMode == WRAP_NORMAL) {
      /*
If monophonic, copy ch1 to all
Otherwise use the poly channels
*/
      if (inputCompolyphony[1] == 1) {
        firstPortIndex = 0;
      } else {
        firstPortIndex = outputIndex;
      }
      if (inputCompolyphony[2] == 1) {
        secondPortIndex = 0;
      } else {
        secondPortIndex = outputIndex;
      }
    }
    output.push_back(inputs[firstPortID].getVoltage(firstPortIndex));
    output.push_back(inputs[firstPortID + 1].getVoltage(secondPortIndex));

    return output;
  }
  std::vector<float> getComplexVoltageFromInterleavedInput(
      int outputIndex, int firstPortID, int inputMode, int wrapMode,
      std::vector<int>& inputCompolyphony) {
    std::vector<float> output = {};
    int portIndex = outputIndex >= 8 ? firstPortID + 1 : firstPortID;
    int relativeOutputChannelIndex = outputIndex % 8;
    int firstChannelIndex = 0;

    if (wrapMode == WRAP_NORMAL) {
      /*
If monophonic, copy ch1 to all
Otherwise use the poly channels
*/
      if (inputCompolyphony[0] == 2) {
        firstChannelIndex = 0;
      } else {
        firstChannelIndex = relativeOutputChannelIndex * 2;
      }
    }
    output.push_back(inputs[portIndex].getVoltage(firstChannelIndex));
    output.push_back(inputs[portIndex].getVoltage(firstChannelIndex + 1));
    return output;
  }

  void getCoordinatePairFromSeparatedInput(int outputIndex, int firstPortID,
                                           int wrapMode,
                                           std::vector<int>& inputCompolyphony,
                                           float& a, float& b) {
    int firstPortIndex = outputIndex;
    int secondPortIndex = outputIndex;
    if (wrapMode == WRAP_NORMAL) {
      firstPortIndex = inputCompolyphony[1] == 1 ? 0 : outputIndex;
      secondPortIndex = inputCompolyphony[2] == 1 ? 0 : outputIndex;
    }
    a = inputs[firstPortID].getVoltage(firstPortIndex);
    b = inputs[firstPortID + 1].getVoltage(secondPortIndex);
  }

  void getCoordinatePairFromSeparatedInput(int outputIndex, int firstPortID,
                                           int wrapMode,
                                           const CompolyInputInfo& inputInfo,
                                           float& a, float& b) {
    int firstPortIndex = outputIndex;
    int secondPortIndex = outputIndex;
    if (wrapMode == WRAP_NORMAL) {
      firstPortIndex = inputInfo.portAChannels == 1 ? 0 : outputIndex;
      secondPortIndex = inputInfo.portBChannels == 1 ? 0 : outputIndex;
    }
    a = inputs[firstPortID].getVoltage(firstPortIndex);
    b = inputs[firstPortID + 1].getVoltage(secondPortIndex);
  }

  void getCoordinatePairFromInterleavedInput(int outputIndex, int firstPortID,
                                             int wrapMode,
                                             std::vector<int>& inputCompolyphony,
                                             float& a, float& b) {
    int portIndex = outputIndex >= 8 ? firstPortID + 1 : firstPortID;
    int relativeOutputChannelIndex = outputIndex % 8;
    int firstChannelIndex = 0;

    if (wrapMode == WRAP_NORMAL) {
      firstChannelIndex =
          inputCompolyphony[0] == 2 ? 0 : relativeOutputChannelIndex * 2;
    }
    a = inputs[portIndex].getVoltage(firstChannelIndex);
    b = inputs[portIndex].getVoltage(firstChannelIndex + 1);
  }

  void getCoordinatePairFromInterleavedInput(int outputIndex, int firstPortID,
                                             int wrapMode,
                                             const CompolyInputInfo& inputInfo,
                                             float& a, float& b) {
    int portIndex = outputIndex >= 8 ? firstPortID + 1 : firstPortID;
    int relativeOutputChannelIndex = outputIndex % 8;
    int firstChannelIndex = 0;

    if (wrapMode == WRAP_NORMAL) {
      firstChannelIndex =
          inputInfo.compolyChannels == 2 ? 0 : relativeOutputChannelIndex * 2;
    }
    a = inputs[portIndex].getVoltage(firstChannelIndex);
    b = inputs[portIndex].getVoltage(firstChannelIndex + 1);
  }

  void getCoordinatePair(int outputIndex, int firstPortID, int inputMode,
                         int wrapMode, std::vector<int>& inputCompolyphony,
                         float& a, float& b) {
    if (inputMode == RECT_INTERLEAVED || inputMode == POLAR_INTERLEAVED) {
      getCoordinatePairFromInterleavedInput(outputIndex, firstPortID, wrapMode,
                                            inputCompolyphony, a, b);
    } else {
      getCoordinatePairFromSeparatedInput(outputIndex, firstPortID, wrapMode,
                                          inputCompolyphony, a, b);
    }
  }

  void getCoordinatePair(int outputIndex, int firstPortID, int inputMode,
                         int wrapMode, const CompolyInputInfo& inputInfo,
                         float& a, float& b) {
    if (inputMode == RECT_INTERLEAVED || inputMode == POLAR_INTERLEAVED) {
      getCoordinatePairFromInterleavedInput(outputIndex, firstPortID, wrapMode,
                                            inputInfo, a, b);
    } else {
      getCoordinatePairFromSeparatedInput(outputIndex, firstPortID, wrapMode,
                                          inputInfo, a, b);
    }
  }

  ComplexSample getComplexSample(int outputIndex, int firstPortID,
                                 int inputMode, int wrapMode,
                                 std::vector<int>& inputCompolyphony) {
    float a = 0.f;
    float b = 0.f;
    getCoordinatePair(outputIndex, firstPortID, inputMode, wrapMode,
                      inputCompolyphony, a, b);

    cpx::complex_math::Quad z = cpx::complex_math::quadFromPair(
        a, b, static_cast<cpx::complex_math::CoordinateMode>(inputMode));
    return {z.x, z.y, z.r, z.theta};
  }

  ComplexSample getComplexSample(int outputIndex, int firstPortID,
                                 int inputMode, int wrapMode,
                                 const CompolyInputInfo& inputInfo) {
    float a = 0.f;
    float b = 0.f;
    getCoordinatePair(outputIndex, firstPortID, inputMode, wrapMode, inputInfo,
                      a, b);

    cpx::complex_math::Quad z = cpx::complex_math::quadFromPair(
        a, b, static_cast<cpx::complex_math::CoordinateMode>(inputMode));
    return ComplexSample(z.x, z.y, z.r, z.theta);
  }

  bool routeComplexPairExactLayout(int inputFirstPortIndex,
                                   int outputFirstPortId, int inputMode,
                                   int outputMode, int wrapMode,
                                   int compolyChannels,
                                   const CompolyInputInfo& inputInfo) {
    if (inputMode != outputMode || wrapMode != WRAP_NORMAL) return false;

    cpx::complex_math::PortChannelCounts outputCounts =
        cpx::complex_math::outputPortChannelCounts(
            coordinateModeFromParam(outputMode), compolyChannels);
    if (inputInfo.portAChannels != outputCounts.a ||
        inputInfo.portBChannels != outputCounts.b) {
      return false;
    }

    float a[16] = {};
    float b[16] = {};
    inputs[inputFirstPortIndex].readVoltages(a);
    inputs[inputFirstPortIndex + 1].readVoltages(b);
    outputs[outputFirstPortId].writeVoltages(a);
    outputs[outputFirstPortId + 1].writeVoltages(b);
    return true;
  }

  void routeComplexPairSameFamily(int inputFirstPortIndex, int outputFirstPortId,
                                  int inputMode, int outputMode, int wrapMode,
                                  int compolyChannels,
                                  std::vector<int>& inputCompolyphony) {
    for (int c = 0; c < compolyChannels; c++) {
      float a = 0.f;
      float b = 0.f;
      getCoordinatePair(c, inputFirstPortIndex, inputMode, wrapMode,
                        inputCompolyphony, a, b);
      setOutputCoordinatePair(outputFirstPortId, outputMode, c, a, b);
    }
  }

  void routeComplexPairSameFamily(int inputFirstPortIndex, int outputFirstPortId,
                                  int inputMode, int outputMode, int wrapMode,
                                  int compolyChannels,
                                  const CompolyInputInfo& inputInfo) {
    if (routeComplexPairExactLayout(inputFirstPortIndex, outputFirstPortId,
                                    inputMode, outputMode, wrapMode,
                                    compolyChannels, inputInfo)) {
      return;
    }

    for (int c = 0; c < compolyChannels; c++) {
      float a = 0.f;
      float b = 0.f;
      getCoordinatePair(c, inputFirstPortIndex, inputMode, wrapMode, inputInfo,
                        a, b);
      setOutputCoordinatePair(outputFirstPortId, outputMode, c, a, b);
    }
  }

  void routeComplexPairConverted(int inputFirstPortIndex, int outputFirstPortId,
                                 int inputMode, int outputMode, int wrapMode,
                                 int compolyChannels,
                                 std::vector<int>& inputCompolyphony) {
    for (int c = 0; c < compolyChannels; c++) {
      ComplexSample z = getComplexSample(c, inputFirstPortIndex, inputMode,
                                         wrapMode, inputCompolyphony);
      setOutputVoltages(outputFirstPortId, outputMode, c, z.x, z.y, z.r,
                        z.theta);
    }
  }

  void routeComplexPairConverted(int inputFirstPortIndex, int outputFirstPortId,
                                 int inputMode, int outputMode, int wrapMode,
                                 int compolyChannels,
                                 const CompolyInputInfo& inputInfo) {
    for (int c = 0; c < compolyChannels; c++) {
      ComplexSample z = getComplexSample(c, inputFirstPortIndex, inputMode,
                                         wrapMode, inputInfo);
      setOutputVoltages(outputFirstPortId, outputMode, c, z.x, z.y, z.r,
                        z.theta);
    }
  }

  std::vector<float> getQuad(std::vector<float>& ab, int type) {
    cpx::complex_math::Quad z = cpx::complex_math::quadFromPair(
        ab[0], ab[1], static_cast<cpx::complex_math::CoordinateMode>(type));
    return {z.x, z.y, z.r, z.theta};
  }

  std::vector<float> getComplexVoltage(
      int outputIndex, int firstPortID, int inputMode, int wrapMode,
      std::vector<std::vector<int>>& inputCompolyphony) {
    // 7% -> 3%
    // return {0.f,1.f,2.f,3.f};
    std::vector<float> mainInputVoltages;
    if (inputMode == RECT_INTERLEAVED || inputMode == POLAR_INTERLEAVED) {
      mainInputVoltages = getComplexVoltageFromInterleavedInput(
          outputIndex, firstPortID, inputMode, wrapMode, inputCompolyphony[0]);
    } else {
      mainInputVoltages = getComplexVoltageFromSeparatedInput(
          outputIndex, firstPortID, inputMode, wrapMode, inputCompolyphony[0]);
    }
    return getQuad(mainInputVoltages, inputMode);
  }

  std::vector<int> getChannelIndicesFromSeparatedInput(
      int outputIndex, int wrapMode, std::vector<int> channelCounts) {
    std::vector<int> output;
    for (int i = 0; i < (int)channelCounts.size(); i++) {
      int myInputChannelIndex = 0;
      ;
      int myNumChannels = channelCounts[i];
      if (wrapMode == WRAP_NORMAL) {
        /*
      If monophonic, copy ch1 to all
      Otherwise use the poly channels
    */
        if (myNumChannels == 1) {
          myInputChannelIndex = 0;
        } else {
          myInputChannelIndex = outputIndex;
        }
      } else if (wrapMode == WRAP_CYCLE) {
        // Cycle through the poly channels
        myInputChannelIndex = outputIndex % myNumChannels;
      } else if (wrapMode == WRAP_MINIMAL) {
        // Do not copy channel 1 if monophonic
        myInputChannelIndex = outputIndex;
      } else if (wrapMode == WRAP_STALL) {
        // Go up to the maximum channel, and then use the final value for the
        // rest
        myInputChannelIndex =
            outputIndex > myNumChannels - 1 ? myNumChannels - 1 : outputIndex;
      }
      // reverse
      // pingpong
      // random shuffled

      output.push_back(myInputChannelIndex);
    }
    return output;
  }
};
