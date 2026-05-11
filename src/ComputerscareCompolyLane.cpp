#include "Computerscare.hpp"
#include "CvControlGroup.hpp"
#include "complex/ComplexWidgets.hpp"
#include "complex/CompolyPortMapping.hpp"
#include "complex/math/ComplexMath.hpp"

struct ComputerscareCompolyLane : ComputerscareComplexBase {
  enum ParamIds {
    WRAP_MODE,
    COMPOLY_CHANNELS,
    Z_INPUT_MODE,
    W_INPUT_MODE,
    Z_OUTPUT_MODE,
    W_OUTPUT_MODE,
    A_SCALE_VAL,
    A_SCALE_TRIM,
    A_OFFSET_VAL,
    A_OFFSET_TRIM,
    B_SCALE_VAL,
    B_SCALE_TRIM,
    B_OFFSET_VAL,
    B_OFFSET_TRIM,
    C_SCALE_VAL,
    C_SCALE_TRIM,
    C_OFFSET_VAL,
    C_OFFSET_TRIM,
    D_SCALE_VAL,
    D_SCALE_TRIM,
    D_OFFSET_VAL,
    D_OFFSET_TRIM,
    NUM_PARAMS
  };
  enum InputIds {
    A_INPUT,
    A_SCALE_CV,
    A_OFFSET_CV,
    B_INPUT,
    B_SCALE_CV,
    B_OFFSET_CV,
    C_INPUT,
    C_SCALE_CV,
    C_OFFSET_CV,
    D_INPUT,
    D_SCALE_CV,
    D_OFFSET_CV,
    NUM_INPUTS
  };
  enum OutputIds {
    Z_OUTPUT_A,
    Z_OUTPUT_B,
    W_OUTPUT_A,
    W_OUTPUT_B,
    NUM_OUTPUTS
  };
  enum LightIds { NUM_LIGHTS };

  enum InputMode {
    INPUT_XY,
    INPUT_RTHETA,
  };

  int compolyChannels = 1;

  struct InputModeParam : SwitchQuantity {
    InputModeParam() {
      snapEnabled = true;
      randomizeEnabled = false;
      resetEnabled = false;
      labels = {"x / y", "r / theta"};
    }

    std::string getDisplayValueString() override {
      return getValue() < 0.5f ? "x / y" : "r / theta";
    }
  };

  template <int TModeParamId, bool TSecond, int TRole>
  struct LaneParamQuantity : ParamQuantity {
    std::string getLabel() override {
      ComputerscareCompolyLane* m =
          dynamic_cast<ComputerscareCompolyLane*>(module);
      std::string coordinate = m ? m->inputCoordinateName(TModeParamId, TSecond)
                                 : (TSecond ? "Y / Theta" : "X / Radius");

      std::string role;
      if (TRole == 0) {
        role = "Scale";
      } else if (TRole == 1) {
        role = "Scale CV Amount";
      } else if (TRole == 2) {
        role = "Offset";
      } else {
        role = "Offset CV Amount";
      }
      return name + " " + coordinate + " " + role;
    }
  };

  template <int TModeParamId, bool TSecond, int TRole>
  struct LaneInputInfo : PortInfo {
    std::string getName() override {
      ComputerscareCompolyLane* m =
          dynamic_cast<ComputerscareCompolyLane*>(module);
      std::string coordinate = m ? m->inputCoordinateName(TModeParamId, TSecond)
                                 : (TSecond ? "Y / Theta" : "X / Radius");

      std::string role;
      if (TRole == 1) {
        role = " Scale CV";
      } else if (TRole == 2) {
        role = " Offset CV";
      }
      return name + " " + coordinate + role;
    }
  };

  ComputerscareCompolyLane() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    configMenuParam(WRAP_MODE, cpx::polyphonic::defaultMappingModeValue,
                    "Polyphonic Mapping Mode",
                    cpx::compoly::wrapModeDescriptions());
    getParamQuantity(WRAP_MODE)->randomizeEnabled = false;
    getParamQuantity(WRAP_MODE)->resetEnabled = false;

    configParam<AutoParamQuantity>(COMPOLY_CHANNELS, 0.f, 16.f, 0.f,
                                   "Compoly Lanes");
    getParamQuantity(COMPOLY_CHANNELS)->randomizeEnabled = false;

    configParam<InputModeParam>(Z_INPUT_MODE, 0.f, 1.f, 0.f, "Z Input Mode");
    configParam<InputModeParam>(W_INPUT_MODE, 0.f, 1.f, 0.f, "W Input Mode");

    configParam<cpx::CompolyModeParam>(
        Z_OUTPUT_MODE, cpx::complex_math::firstCoordinateModeValue,
        cpx::complex_math::lastCoordinateModeValue,
        cpx::complex_math::defaultCoordinateModeValue, "Z Output Mode");
    configParam<cpx::CompolyModeParam>(
        W_OUTPUT_MODE, cpx::complex_math::firstCoordinateModeValue,
        cpx::complex_math::lastCoordinateModeValue,
        cpx::complex_math::defaultCoordinateModeValue, "W Output Mode");

    configLane<Z_INPUT_MODE, false>(A_INPUT, A_SCALE_CV, A_OFFSET_CV,
                                    A_SCALE_VAL, A_SCALE_TRIM, A_OFFSET_VAL,
                                    A_OFFSET_TRIM, "Z");
    configLane<Z_INPUT_MODE, true>(B_INPUT, B_SCALE_CV, B_OFFSET_CV,
                                   B_SCALE_VAL, B_SCALE_TRIM, B_OFFSET_VAL,
                                   B_OFFSET_TRIM, "Z");
    configLane<W_INPUT_MODE, false>(C_INPUT, C_SCALE_CV, C_OFFSET_CV,
                                    C_SCALE_VAL, C_SCALE_TRIM, C_OFFSET_VAL,
                                    C_OFFSET_TRIM, "W");
    configLane<W_INPUT_MODE, true>(D_INPUT, D_SCALE_CV, D_OFFSET_CV,
                                   D_SCALE_VAL, D_SCALE_TRIM, D_OFFSET_VAL,
                                   D_OFFSET_TRIM, "W");

    configOutput<cpx::CompolyPortInfo<Z_OUTPUT_MODE, 0>>(Z_OUTPUT_A, "z");
    configOutput<cpx::CompolyPortInfo<Z_OUTPUT_MODE, 1>>(Z_OUTPUT_B, "z");
    configOutput<cpx::CompolyPortInfo<W_OUTPUT_MODE, 0>>(W_OUTPUT_A, "w");
    configOutput<cpx::CompolyPortInfo<W_OUTPUT_MODE, 1>>(W_OUTPUT_B, "w");
  }

  template <int TModeParamId, bool TSecond>
  void configLane(int valueInputId, int scaleCvInputId, int offsetCvInputId,
                  int scaleParamId, int scaleTrimParamId, int offsetParamId,
                  int offsetTrimParamId, std::string name) {
    configInput<LaneInputInfo<TModeParamId, TSecond, 0>>(valueInputId, name);
    configInput<LaneInputInfo<TModeParamId, TSecond, 1>>(scaleCvInputId, name);
    configInput<LaneInputInfo<TModeParamId, TSecond, 2>>(offsetCvInputId, name);
    configParam<LaneParamQuantity<TModeParamId, TSecond, 0>>(scaleParamId, -2.f,
                                                             2.f, 1.f, name);
    configParam<LaneParamQuantity<TModeParamId, TSecond, 1>>(
        scaleTrimParamId, -1.f, 1.f, 0.f, name);
    configParam<LaneParamQuantity<TModeParamId, TSecond, 2>>(
        offsetParamId, -10.f, 10.f, 0.f, name);
    configParam<LaneParamQuantity<TModeParamId, TSecond, 3>>(
        offsetTrimParamId, -1.f, 1.f, 0.f, name);
  }

  std::string inputCoordinateName(int modeParamId, bool second) {
    bool polar = params[modeParamId].getValue() >= 0.5f;
    if (polar) return second ? "Theta" : "Radius";
    return second ? "Y" : "X";
  }

  float readWrappedInput(int inputId, int channel, int inputChannels,
                         int wrapMode) {
    int inputChannel = cpx::compoly::cableChannelForCompolyLane(
        cpx::compoly::CompolyLane(channel),
        static_cast<cpx::polyphonic::MappingMode>(wrapMode),
        cpx::compoly::CablePolyChannels(inputChannels));
    return inputs[inputId].getVoltage(inputChannel);
  }

  float readWrappedCv(int inputId, int channel, int wrapMode) {
    int inputChannels = inputs[inputId].getChannels();
    if (inputChannels == 0) return 0.f;
    return readWrappedInput(inputId, channel, inputChannels, wrapMode);
  }

  float laneValue(int valueInputId, int scaleCvInputId, int offsetCvInputId,
                  int scaleParamId, int scaleTrimParamId, int offsetParamId,
                  int offsetTrimParamId, int channel, int valueInputChannels,
                  int wrapMode) {
    float value =
        readWrappedInput(valueInputId, channel, valueInputChannels, wrapMode);
    float scale = params[scaleParamId].getValue() +
                  readWrappedCv(scaleCvInputId, channel, wrapMode) *
                      params[scaleTrimParamId].getValue();
    float offset = params[offsetParamId].getValue() +
                   readWrappedCv(offsetCvInputId, channel, wrapMode) *
                       params[offsetTrimParamId].getValue();
    return value * scale + offset;
  }

  void checkPoly() override {
    int requestedChannels = params[COMPOLY_CHANNELS].getValue();
    int zDetected = detectedPairCompolyChannels(A_INPUT, B_INPUT);
    int wDetected = detectedPairCompolyChannels(C_INPUT, D_INPUT);
    compolyChannels = cpx::compoly::outputCompolyLanes(
        requestedChannels, std::max(zDetected, wDetected));
    setOutputChannels(Z_OUTPUT_A, params[Z_OUTPUT_MODE].getValue(),
                      compolyChannels);
    setOutputChannels(W_OUTPUT_A, params[W_OUTPUT_MODE].getValue(),
                      compolyChannels);
  }

  void process(const ProcessArgs& args) override {
    ComputerscarePolyModule::checkCounter();

    int wrapMode = params[WRAP_MODE].getValue();
    int zOutputMode = params[Z_OUTPUT_MODE].getValue();
    int wOutputMode = params[W_OUTPUT_MODE].getValue();
    bool zPolarInput = params[Z_INPUT_MODE].getValue() >= 0.5f;
    bool wPolarInput = params[W_INPUT_MODE].getValue() >= 0.5f;

    for (int c = 0; c < compolyChannels; ++c) {
      processPair(Z_OUTPUT_A, A_INPUT, A_SCALE_CV, A_OFFSET_CV, A_SCALE_VAL,
                  A_SCALE_TRIM, A_OFFSET_VAL, A_OFFSET_TRIM, B_INPUT,
                  B_SCALE_CV, B_OFFSET_CV, B_SCALE_VAL, B_SCALE_TRIM,
                  B_OFFSET_VAL, B_OFFSET_TRIM, c, wrapMode, zOutputMode,
                  zPolarInput);
      processPair(W_OUTPUT_A, C_INPUT, C_SCALE_CV, C_OFFSET_CV, C_SCALE_VAL,
                  C_SCALE_TRIM, C_OFFSET_VAL, C_OFFSET_TRIM, D_INPUT,
                  D_SCALE_CV, D_OFFSET_CV, D_SCALE_VAL, D_SCALE_TRIM,
                  D_OFFSET_VAL, D_OFFSET_TRIM, c, wrapMode, wOutputMode,
                  wPolarInput);
    }
  }

  int detectedPairCompolyChannels(int firstInputId, int secondInputId) {
    return cpx::compoly::compolyLanesForSeparatedCables(
        cpx::compoly::SeparatedCablePolyChannels(
            cpx::compoly::CablePolyChannels(inputs[firstInputId].getChannels()),
            cpx::compoly::CablePolyChannels(
                inputs[secondInputId].getChannels())));
  }

  void processPair(int outputId, int firstInputId, int firstScaleCvInputId,
                   int firstOffsetCvInputId, int firstScaleParamId,
                   int firstScaleTrimParamId, int firstOffsetParamId,
                   int firstOffsetTrimParamId, int secondInputId,
                   int secondScaleCvInputId, int secondOffsetCvInputId,
                   int secondScaleParamId, int secondScaleTrimParamId,
                   int secondOffsetParamId, int secondOffsetTrimParamId,
                   int channel, int wrapMode, int outputMode, bool polarInput) {
    float a = laneValue(firstInputId, firstScaleCvInputId, firstOffsetCvInputId,
                        firstScaleParamId, firstScaleTrimParamId,
                        firstOffsetParamId, firstOffsetTrimParamId, channel,
                        inputs[firstInputId].getChannels(), wrapMode);
    float b =
        laneValue(secondInputId, secondScaleCvInputId, secondOffsetCvInputId,
                  secondScaleParamId, secondScaleTrimParamId,
                  secondOffsetParamId, secondOffsetTrimParamId, channel,
                  inputs[secondInputId].getChannels(), wrapMode);

    float x = a;
    float y = b;
    if (polarInput) {
      float theta = cpx::complex_math::thetaCableVoltageToRadians(b);
      cpx::complex_math::Rect rect =
          cpx::complex_math::polarToRect(cpx::complex_math::Polar(a, theta));
      x = rect.x;
      y = rect.y;
    }

    float r = std::hypot(x, y);
    float theta = std::atan2(y, x);
    setOutputVoltages(outputId, outputMode, channel, x, y, r, theta);
  }
};

struct CompolyLaneCoordinateLabel : cpx::ScaledSvgWidget {
  ComputerscareCompolyLane* module = nullptr;
  bool second = false;
  int modeParamId = -1;
  std::string lastFilename;

  CompolyLaneCoordinateLabel() : cpx::ScaledSvgWidget(0.34f) {}

  std::string labelFilename() {
    bool polar = module && modeParamId >= 0 &&
                 module->params[modeParamId].getValue() >= 0.5f;
    if (polar) return second ? "theta.svg" : "r.svg";
    return second ? "y.svg" : "x.svg";
  }

  void step() override {
    std::string filename = labelFilename();
    if (filename != lastFilename) {
      setSVG("res/complex-labels/" + filename);
      lastFilename = filename;
    }
    cpx::ScaledSvgWidget::step();
  }
};

struct ComputerscareCompolyLaneWidget : ModuleWidget {
  ComputerscareCompolyLaneWidget(ComputerscareCompolyLane* module) {
    setModule(module);
    box.size = Vec(12 * 15, 380);
    {
      ComputerscareSVGPanel* panel = new ComputerscareSVGPanel();
      panel->box.size = box.size;
      panel->setBackground(APP->window->loadSvg(asset::plugin(
          pluginInstance, "res/panels/ComputerscareCompolyLanePanel.svg")));
      addChild(panel);
    }

    auto* laneWidget = new CompolyLaneCountWidget(
        Vec(142.f, 1.f), module, ComputerscareCompolyLane::COMPOLY_CHANNELS,
        module ? &module->compolyChannels : nullptr);
    addChild(laneWidget);

    addParam(createParam<cpx::CompolyTypeLabelSwitch>(
        Vec(12.f, 8.f), module, ComputerscareCompolyLane::Z_INPUT_MODE));
    addParam(createParam<cpx::CompolyTypeLabelSwitch>(
        Vec(94.f, 8.f), module, ComputerscareCompolyLane::W_INPUT_MODE));

    const Vec lanePos[4] = {Vec(16.f, 48.f), Vec(53.f, 48.f), Vec(98.f, 48.f),
                            Vec(135.f, 48.f)};
    const int inputModes[4] = {ComputerscareCompolyLane::Z_INPUT_MODE,
                               ComputerscareCompolyLane::Z_INPUT_MODE,
                               ComputerscareCompolyLane::W_INPUT_MODE,
                               ComputerscareCompolyLane::W_INPUT_MODE};
    const int valueInputs[4] = {
        ComputerscareCompolyLane::A_INPUT, ComputerscareCompolyLane::B_INPUT,
        ComputerscareCompolyLane::C_INPUT, ComputerscareCompolyLane::D_INPUT};
    const int scaleCvs[4] = {ComputerscareCompolyLane::A_SCALE_CV,
                             ComputerscareCompolyLane::B_SCALE_CV,
                             ComputerscareCompolyLane::C_SCALE_CV,
                             ComputerscareCompolyLane::D_SCALE_CV};
    const int scaleTrims[4] = {ComputerscareCompolyLane::A_SCALE_TRIM,
                               ComputerscareCompolyLane::B_SCALE_TRIM,
                               ComputerscareCompolyLane::C_SCALE_TRIM,
                               ComputerscareCompolyLane::D_SCALE_TRIM};
    const int scaleVals[4] = {ComputerscareCompolyLane::A_SCALE_VAL,
                              ComputerscareCompolyLane::B_SCALE_VAL,
                              ComputerscareCompolyLane::C_SCALE_VAL,
                              ComputerscareCompolyLane::D_SCALE_VAL};
    const int offsetCvs[4] = {ComputerscareCompolyLane::A_OFFSET_CV,
                              ComputerscareCompolyLane::B_OFFSET_CV,
                              ComputerscareCompolyLane::C_OFFSET_CV,
                              ComputerscareCompolyLane::D_OFFSET_CV};
    const int offsetTrims[4] = {ComputerscareCompolyLane::A_OFFSET_TRIM,
                                ComputerscareCompolyLane::B_OFFSET_TRIM,
                                ComputerscareCompolyLane::C_OFFSET_TRIM,
                                ComputerscareCompolyLane::D_OFFSET_TRIM};
    const int offsetVals[4] = {ComputerscareCompolyLane::A_OFFSET_VAL,
                               ComputerscareCompolyLane::B_OFFSET_VAL,
                               ComputerscareCompolyLane::C_OFFSET_VAL,
                               ComputerscareCompolyLane::D_OFFSET_VAL};

    for (int i = 0; i < 4; ++i) {
      addCoordinateLabel(lanePos[i].plus(Vec(2.f, -14.f)), module,
                         inputModes[i], i % 2 == 1);

      addInput(createInput<InPort>(lanePos[i], module, valueInputs[i]));

      computerscare::addCvControlGroup<SmoothKnob, SmallKnob, InPort>(
          this, module, lanePos[i].plus(Vec(0.f, 38.f)),
          computerscare::CvControlGroupIds(scaleVals[i], scaleTrims[i],
                                           scaleCvs[i]),
          computerscare::CvControlGroupMode::Column);

      addCoordinateLabel(lanePos[i].plus(Vec(2.f, 24.f)), module, inputModes[i],
                         i % 2 == 1);

      computerscare::addCvControlGroup<SmoothKnob, ComputerscareDotKnob,
                                       InPort>(
          this, module, lanePos[i].plus(Vec(0.f, 146.f)),
          computerscare::CvControlGroupIds(offsetVals[i], offsetTrims[i],
                                           offsetCvs[i]),
          computerscare::CvControlGroupMode::Column);

      addCoordinateLabel(lanePos[i].plus(Vec(2.f, 132.f)), module,
                         inputModes[i], i % 2 == 1);
    }

    cpx::CompolyPortsWidget* zOutput = new cpx::CompolyPortsWidget(
        Vec(70.f, 288.f), module, ComputerscareCompolyLane::Z_OUTPUT_A,
        ComputerscareCompolyLane::Z_OUTPUT_MODE, 0.6, true, "z");
    zOutput->compolyLabelTransform->box.pos = Vec(53.f, 294.f);
    addChild(zOutput);

    cpx::CompolyPortsWidget* wOutput = new cpx::CompolyPortsWidget(
        Vec(70.f, 334.f), module, ComputerscareCompolyLane::W_OUTPUT_A,
        ComputerscareCompolyLane::W_OUTPUT_MODE, 0.6, true, "w");
    wOutput->compolyLabelTransform->box.pos = Vec(53.f, 340.f);
    addChild(wOutput);
  }

  void addCoordinateLabel(Vec pos, ComputerscareCompolyLane* module,
                          int modeParamId, bool second) {
    auto* label = new CompolyLaneCoordinateLabel();
    label->module = module;
    label->modeParamId = modeParamId;
    label->second = second;
    label->box.pos = pos;
    addChild(label);
  }
};

Model* modelComputerscareCompolyLane =
    createModel<ComputerscareCompolyLane, ComputerscareCompolyLaneWidget>(
        "computerscare-compoly-lane");
