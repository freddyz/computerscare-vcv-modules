#include "Computerscare.hpp"
#include "CvControlGroup.hpp"
#include "complex/ComplexWidgets.hpp"
#include "complex/CompolyPortMapping.hpp"
#include "complex/math/ComplexMath.hpp"
#include "complex/math/ComplexSimd.hpp"

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
    W_WRAP_MODE,
    SUM_OUTPUT_MODE,
    PRODUCT_OUTPUT_MODE,
    Z_OUTPUT_OPERATION,
    W_OUTPUT_OPERATION,
    SUM_OUTPUT_OPERATION,
    PRODUCT_OUTPUT_OPERATION,
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
    SUM_OUTPUT_A,
    SUM_OUTPUT_B,
    PRODUCT_OUTPUT_A,
    PRODUCT_OUTPUT_B,
    NUM_OUTPUTS
  };
  enum LightIds { NUM_LIGHTS };

  enum InputMode {
    INPUT_XY,
    INPUT_RTHETA,
  };

  enum OutputOperation {
    OP_Z,
    OP_W,
    OP_Z_PLUS_W,
    OP_Z_MINUS_W,
    OP_W_MINUS_Z,
    OP_Z_TIMES_W,
    OP_W_DIVIDE_Z,
    OP_Z_DIVIDE_W,
    NUM_OUTPUT_OPERATIONS
  };

  int compolyChannels = 1;

  static const std::vector<std::string>& outputOperationLabels() {
    static const std::vector<std::string> labels = {
        "z", "w", "z + w", "z - w", "w - z", "w * z", "w / z", "z / w",
    };
    return labels;
  }

  static std::string outputOperationLabel(int operation) {
    const std::vector<std::string>& labels = outputOperationLabels();
    if (operation >= 0 && operation < (int)labels.size()) {
      return labels[operation];
    }
    return "";
  }

  static std::string outputOperationSvg(int operation) {
    switch (operation) {
      case OP_Z:
        return "z.svg";
      case OP_W:
        return "w.svg";
      case OP_Z_PLUS_W:
        return "zplusw.svg";
      case OP_Z_MINUS_W:
        return "zminusw.svg";
      case OP_W_MINUS_Z:
        return "wminusz.svg";
      case OP_Z_TIMES_W:
        return "wtimesz.svg";
      case OP_W_DIVIDE_Z:
        return "wdividez.svg";
      case OP_Z_DIVIDE_W:
        return "zdividew.svg";
    }
    return "z.svg";
  }

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

  template <int TModeParamId, int TOperationParamId, int TBlockNum>
  struct OutputPortInfo : cpx::CompolyPortInfo<TModeParamId, TBlockNum> {
    std::string getName() override {
      ComputerscareCompolyLane* m =
          dynamic_cast<ComputerscareCompolyLane*>(this->module);
      int operation = m ? m->params[TOperationParamId].getValue() : OP_Z;
      return outputOperationLabel(operation) + ", " + this->getModeName();
    }
  };

  ComputerscareCompolyLane() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    configMenuParam(WRAP_MODE, cpx::polyphonic::defaultMappingModeValue,
                    "Z Polyphonic Mapping Mode",
                    cpx::compoly::wrapModeDescriptions());
    getParamQuantity(WRAP_MODE)->randomizeEnabled = false;
    getParamQuantity(WRAP_MODE)->resetEnabled = false;

    configMenuParam(W_WRAP_MODE, cpx::polyphonic::defaultMappingModeValue,
                    "W Polyphonic Mapping Mode",
                    cpx::compoly::wrapModeDescriptions());
    getParamQuantity(W_WRAP_MODE)->randomizeEnabled = false;
    getParamQuantity(W_WRAP_MODE)->resetEnabled = false;

    configSwitch(COMPOLY_CHANNELS, 0.f, 16.f, 0.f, "Compoly Lanes",
                 polyChannelLabels(true));
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
    configParam<cpx::CompolyModeParam>(
        SUM_OUTPUT_MODE, cpx::complex_math::firstCoordinateModeValue,
        cpx::complex_math::lastCoordinateModeValue,
        cpx::complex_math::defaultCoordinateModeValue, "Output 3 Mode");
    configParam<cpx::CompolyModeParam>(
        PRODUCT_OUTPUT_MODE, cpx::complex_math::firstCoordinateModeValue,
        cpx::complex_math::lastCoordinateModeValue,
        cpx::complex_math::defaultCoordinateModeValue, "Output 4 Mode");

    configSwitch(Z_OUTPUT_OPERATION, 0.f, NUM_OUTPUT_OPERATIONS - 1, OP_Z,
                 "Z Output Operation", outputOperationLabels());
    configSwitch(W_OUTPUT_OPERATION, 0.f, NUM_OUTPUT_OPERATIONS - 1, OP_W,
                 "W Output Operation", outputOperationLabels());
    configSwitch(SUM_OUTPUT_OPERATION, 0.f, NUM_OUTPUT_OPERATIONS - 1,
                 OP_Z_PLUS_W, "Output 3 Operation", outputOperationLabels());
    configSwitch(PRODUCT_OUTPUT_OPERATION, 0.f, NUM_OUTPUT_OPERATIONS - 1,
                 OP_Z_TIMES_W, "Output 4 Operation", outputOperationLabels());
    getParamQuantity(Z_OUTPUT_OPERATION)->randomizeEnabled = false;
    getParamQuantity(W_OUTPUT_OPERATION)->randomizeEnabled = false;
    getParamQuantity(SUM_OUTPUT_OPERATION)->randomizeEnabled = false;
    getParamQuantity(PRODUCT_OUTPUT_OPERATION)->randomizeEnabled = false;
    getParamQuantity(Z_OUTPUT_OPERATION)->resetEnabled = false;
    getParamQuantity(W_OUTPUT_OPERATION)->resetEnabled = false;
    getParamQuantity(SUM_OUTPUT_OPERATION)->resetEnabled = false;
    getParamQuantity(PRODUCT_OUTPUT_OPERATION)->resetEnabled = false;

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

    configOutput<OutputPortInfo<Z_OUTPUT_MODE, Z_OUTPUT_OPERATION, 0>>(
        Z_OUTPUT_A, "z");
    configOutput<OutputPortInfo<Z_OUTPUT_MODE, Z_OUTPUT_OPERATION, 1>>(
        Z_OUTPUT_B, "z");
    configOutput<OutputPortInfo<W_OUTPUT_MODE, W_OUTPUT_OPERATION, 0>>(
        W_OUTPUT_A, "w");
    configOutput<OutputPortInfo<W_OUTPUT_MODE, W_OUTPUT_OPERATION, 1>>(
        W_OUTPUT_B, "w");
    configOutput<OutputPortInfo<SUM_OUTPUT_MODE, SUM_OUTPUT_OPERATION, 0>>(
        SUM_OUTPUT_A, "z + w");
    configOutput<OutputPortInfo<SUM_OUTPUT_MODE, SUM_OUTPUT_OPERATION, 1>>(
        SUM_OUTPUT_B, "z + w");
    configOutput<
        OutputPortInfo<PRODUCT_OUTPUT_MODE, PRODUCT_OUTPUT_OPERATION, 0>>(
        PRODUCT_OUTPUT_A, "z * w");
    configOutput<
        OutputPortInfo<PRODUCT_OUTPUT_MODE, PRODUCT_OUTPUT_OPERATION, 1>>(
        PRODUCT_OUTPUT_B, "z * w");
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
    setOutputChannels(SUM_OUTPUT_A, params[SUM_OUTPUT_MODE].getValue(),
                      compolyChannels);
    setOutputChannels(PRODUCT_OUTPUT_A, params[PRODUCT_OUTPUT_MODE].getValue(),
                      compolyChannels);
  }

  void process(const ProcessArgs& args) override {
    ComputerscarePolyModule::checkCounter();

    int zWrapMode = params[WRAP_MODE].getValue();
    int wWrapMode = params[W_WRAP_MODE].getValue();
    int zOutputMode = params[Z_OUTPUT_MODE].getValue();
    int wOutputMode = params[W_OUTPUT_MODE].getValue();
    int sumOutputMode = params[SUM_OUTPUT_MODE].getValue();
    int productOutputMode = params[PRODUCT_OUTPUT_MODE].getValue();
    bool zPolarInput = params[Z_INPUT_MODE].getValue() >= 0.5f;
    bool wPolarInput = params[W_INPUT_MODE].getValue() >= 0.5f;

    cpx::complex_math::RectChannels z = {};
    cpx::complex_math::RectChannels w = {};

    for (int c = 0; c < compolyChannels; ++c) {
      readLanePairToRect(A_INPUT, A_SCALE_CV, A_OFFSET_CV, A_SCALE_VAL,
                         A_SCALE_TRIM, A_OFFSET_VAL, A_OFFSET_TRIM, B_INPUT,
                         B_SCALE_CV, B_OFFSET_CV, B_SCALE_VAL, B_SCALE_TRIM,
                         B_OFFSET_VAL, B_OFFSET_TRIM, c, zWrapMode, zPolarInput,
                         z);
      readLanePairToRect(C_INPUT, C_SCALE_CV, C_OFFSET_CV, C_SCALE_VAL,
                         C_SCALE_TRIM, C_OFFSET_VAL, C_OFFSET_TRIM, D_INPUT,
                         D_SCALE_CV, D_OFFSET_CV, D_SCALE_VAL, D_SCALE_TRIM,
                         D_OFFSET_VAL, D_OFFSET_TRIM, c, wWrapMode, wPolarInput,
                         w);
    }

    writeOperationOutput(Z_OUTPUT_A, zOutputMode,
                         params[Z_OUTPUT_OPERATION].getValue(), z, w);
    writeOperationOutput(W_OUTPUT_A, wOutputMode,
                         params[W_OUTPUT_OPERATION].getValue(), z, w);
    writeOperationOutput(SUM_OUTPUT_A, sumOutputMode,
                         params[SUM_OUTPUT_OPERATION].getValue(), z, w);
    writeOperationOutput(PRODUCT_OUTPUT_A, productOutputMode,
                         params[PRODUCT_OUTPUT_OPERATION].getValue(), z, w);
  }

  int detectedPairCompolyChannels(int firstInputId, int secondInputId) {
    return cpx::compoly::compolyLanesForSeparatedCables(
        cpx::compoly::SeparatedCablePolyChannels(
            cpx::compoly::CablePolyChannels(inputs[firstInputId].getChannels()),
            cpx::compoly::CablePolyChannels(
                inputs[secondInputId].getChannels())));
  }

  void readLanePairToRect(int firstInputId, int firstScaleCvInputId,
                          int firstOffsetCvInputId, int firstScaleParamId,
                          int firstScaleTrimParamId, int firstOffsetParamId,
                          int firstOffsetTrimParamId, int secondInputId,
                          int secondScaleCvInputId, int secondOffsetCvInputId,
                          int secondScaleParamId, int secondScaleTrimParamId,
                          int secondOffsetParamId, int secondOffsetTrimParamId,
                          int channel, int wrapMode, bool polarInput,
                          cpx::complex_math::RectChannels& rect) {
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

    rect.x[channel] = x;
    rect.y[channel] = y;
  }

  cpx::complex_math::RectChannels operationChannels(
      int operation, const cpx::complex_math::RectChannels& z,
      const cpx::complex_math::RectChannels& w) {
    switch (operation) {
      case OP_Z:
        return z;
      case OP_W:
        return w;
      case OP_Z_PLUS_W:
        return cpx::complex_math::simd::addChannels(z, w);
      case OP_Z_MINUS_W:
        return subtractChannels(z, w);
      case OP_W_MINUS_Z:
        return subtractChannels(w, z);
      case OP_Z_TIMES_W:
        return cpx::complex_math::simd::multiplyChannels(z, w);
      case OP_W_DIVIDE_Z:
        return divideChannels(w, z);
      case OP_Z_DIVIDE_W:
        return divideChannels(z, w);
    }
    return z;
  }

  cpx::complex_math::RectChannels subtractChannels(
      const cpx::complex_math::RectChannels& a,
      const cpx::complex_math::RectChannels& b) {
    using namespace cpx::complex_math;
    RectChannels out = {};
    for (int c = 0; c < maxChannels; c += 4) {
      cpx::complex_math::simd::storeRect4(
          &out.x[c], &out.y[c],
          cpx::complex_math::simd::Rect4(
              cpx::complex_math::simd::load4(&a.x[c]) -
                  cpx::complex_math::simd::load4(&b.x[c]),
              cpx::complex_math::simd::load4(&a.y[c]) -
                  cpx::complex_math::simd::load4(&b.y[c])));
    }
    return out;
  }

  cpx::complex_math::RectChannels divideChannels(
      const cpx::complex_math::RectChannels& numerator,
      const cpx::complex_math::RectChannels& denominator) {
    using namespace cpx::complex_math;
    RectChannels out = {};
    for (int c = 0; c < maxChannels; c += 4) {
      cpx::complex_math::simd::Float4 ax =
          cpx::complex_math::simd::load4(&numerator.x[c]);
      cpx::complex_math::simd::Float4 ay =
          cpx::complex_math::simd::load4(&numerator.y[c]);
      cpx::complex_math::simd::Float4 bx =
          cpx::complex_math::simd::load4(&denominator.x[c]);
      cpx::complex_math::simd::Float4 by =
          cpx::complex_math::simd::load4(&denominator.y[c]);
      cpx::complex_math::simd::Float4 divisor = bx * bx + by * by;
      cpx::complex_math::simd::Float4 x = (ax * bx + ay * by) / divisor;
      cpx::complex_math::simd::Float4 y = (ay * bx - ax * by) / divisor;
      cpx::complex_math::simd::storeRect4(&out.x[c], &out.y[c],
                                          cpx::complex_math::simd::Rect4(x, y));
    }

    for (int c = 0; c < maxChannels; ++c) {
      if (!std::isfinite(out.x[c]) || !std::isfinite(out.y[c])) {
        out.x[c] = 0.f;
        out.y[c] = 0.f;
      }
    }
    return out;
  }

  void writeOperationOutput(int outputId, int outputMode, int operation,
                            const cpx::complex_math::RectChannels& z,
                            const cpx::complex_math::RectChannels& w) {
    cpx::complex_math::RectChannels out = operationChannels(operation, z, w);
    writeComplexOutputPairFromRect(outputId, outputMode, out.x.data(),
                                   out.y.data());
  }
};

struct CompolyLaneCoordinateLabel : cpx::ScaledSvgWidget {
  ComputerscareCompolyLane* module = nullptr;
  bool second = false;
  int modeParamId = -1;
  std::string lastFilename;

  CompolyLaneCoordinateLabel() : cpx::ScaledSvgWidget(0.46f) {}

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

struct CompolyLaneOutputOperationLabel : app::ParamWidget {
  ComputerscareCompolyLane* laneModule = nullptr;
  cpx::ScaledSvgWidget* label = nullptr;
  std::string lastFilename;
  bool hovering = false;
  float labelScale = 0.44f;
  float rightEdgeX = 0.f;
  float minWidth = 38.f;
  float paddingX = 4.f;
  float paddingY = 2.f;

  CompolyLaneOutputOperationLabel(ComputerscareCompolyLane* module,
                                  int paramId) {
    laneModule = module;
    app::ParamWidget::module = module;
    app::ParamWidget::paramId = paramId;
    box.size = Vec(28.f, 18.f);
    initParamQuantity();

    label = new cpx::ScaledSvgWidget(labelScale);
    label->box.pos = Vec(paddingX, paddingY);
    addChild(label);
  }

  int operation() const {
    if (!laneModule || paramId < 0) return ComputerscareCompolyLane::OP_Z;
    return laneModule->params[paramId].getValue();
  }

  void setOperation(int operation) {
    if (!laneModule || paramId < 0) return;
    if (ParamQuantity* pq = laneModule->paramQuantities[paramId]) {
      pq->setValue(operation);
    } else {
      laneModule->params[paramId].setValue(operation);
    }
  }

  void cycleOperation() {
    int next = operation() + 1;
    if (next >= ComputerscareCompolyLane::NUM_OUTPUT_OPERATIONS) next = 0;
    setOperation(next);
  }

  void step() override {
    std::string filename =
        ComputerscareCompolyLane::outputOperationSvg(operation());
    if (filename != lastFilename) {
      label->setSVG("res/complex-labels/" + filename);
      lastFilename = filename;
    }
    if (label && label->svg && label->svg->svg) {
      Vec labelSize = label->svg->box.size.mult(labelScale);
      box.size = Vec(std::max(minWidth, labelSize.x + paddingX * 2.f),
                     std::max(16.f, labelSize.y + paddingY * 2.f));
      if (rightEdgeX > 0.f) {
        box.pos.x = rightEdgeX - box.size.x;
      }
      label->box.pos = Vec(box.size.x - paddingX - labelSize.x,
                           (box.size.y - labelSize.y) * 0.5f);
    }
    app::ParamWidget::step();
  }

  void draw(const DrawArgs& args) override {
    if (hovering) {
      nvgBeginPath(args.vg);
      nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, 3.f);
      nvgFillColor(args.vg, nvgRGBA(0x24, 0xc9, 0xa6, 0x28));
      nvgFill(args.vg);
      nvgStrokeWidth(args.vg, 1.f);
      nvgStrokeColor(args.vg, COLOR_COMPUTERSCARE_DARK_GREEN);
      nvgStroke(args.vg);
    }
    app::ParamWidget::draw(args);
  }

  void onEnter(const event::Enter& e) override {
    hovering = true;
    app::ParamWidget::onEnter(e);
  }

  void onLeave(const event::Leave& e) override {
    hovering = false;
    app::ParamWidget::onLeave(e);
  }

  void onButton(const event::Button& e) override {
    if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS) {
      cycleOperation();
      e.consume(this);
      return;
    }
    if (e.button == GLFW_MOUSE_BUTTON_RIGHT && e.action == GLFW_PRESS) {
      createOperationMenu();
      e.consume(this);
      return;
    }
    app::ParamWidget::onButton(e);
  }

  void onDoubleClick(const event::DoubleClick& e) override { e.consume(this); }

  void createOperationMenu() {
    if (!laneModule || paramId < 0) return;
    Menu* menu = createMenu();
    MenuLabel* paramLabel = new MenuLabel;
    ParamQuantity* pq = getParamQuantity();
    paramLabel->text = pq ? pq->getLabel() : "Output Operation";
    menu->addChild(paramLabel);

    const std::vector<std::string>& labels =
        ComputerscareCompolyLane::outputOperationLabels();
    int current = operation();
    for (int i = 0; i < (int)labels.size(); ++i) {
      ParamSettingItem* item =
          new ParamSettingItem(i, &laneModule->params[paramId]);
      item->text = labels[i];
      item->rightText = CHECKMARK(i == current);
      menu->addChild(item);
    }
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
    const computerscare::CvControlGroupLayout laneControlLayout = {
        Vec(28.f, 76.f),
        Vec(0.f, 0.f),
        Vec(4.f, 28.f),
        Vec(0.f, 56.f),
    };
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
      addCoordinateLabel(lanePos[i].plus(Vec(7.f, -10.f)), module,
                         inputModes[i], i % 2 == 1);

      addInput(createInput<InPort>(lanePos[i], module, valueInputs[i]));

      computerscare::addCvControlGroup<SmoothKnob, SmallKnob,
                                       PointingUpPentagonPort>(
          this, module, lanePos[i].plus(Vec(0.f, 38.f)),
          computerscare::CvControlGroupIds(scaleVals[i], scaleTrims[i],
                                           scaleCvs[i]),
          laneControlLayout);

      computerscare::addCvControlGroup<SmoothKnob, ComputerscareDotKnob,
                                       PointingUpPentagonPort>(
          this, module, lanePos[i].plus(Vec(0.f, 146.f)),
          computerscare::CvControlGroupIds(offsetVals[i], offsetTrims[i],
                                           offsetCvs[i]),
          laneControlLayout);
    }

    constexpr float outputBlockXLeft = 27.f;
    constexpr float outputBlockXRight = 117.f;

    addOutputPair(Vec(outputBlockXLeft, 288.f), module,
                  ComputerscareCompolyLane::Z_OUTPUT_A,
                  ComputerscareCompolyLane::Z_OUTPUT_MODE,
                  ComputerscareCompolyLane::Z_OUTPUT_OPERATION, "z");
    addOutputPair(Vec(outputBlockXRight, 288.f), module,
                  ComputerscareCompolyLane::W_OUTPUT_A,
                  ComputerscareCompolyLane::W_OUTPUT_MODE,
                  ComputerscareCompolyLane::W_OUTPUT_OPERATION, "w");
    addOutputPair(Vec(outputBlockXLeft, 334.f), module,
                  ComputerscareCompolyLane::SUM_OUTPUT_A,
                  ComputerscareCompolyLane::SUM_OUTPUT_MODE,
                  ComputerscareCompolyLane::SUM_OUTPUT_OPERATION, "zplusw");
    addOutputPair(Vec(outputBlockXRight, 334.f), module,
                  ComputerscareCompolyLane::PRODUCT_OUTPUT_A,
                  ComputerscareCompolyLane::PRODUCT_OUTPUT_MODE,
                  ComputerscareCompolyLane::PRODUCT_OUTPUT_OPERATION,
                  "ztimesw");
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

  void addOutputPair(Vec pos, ComputerscareCompolyLane* module, int outputId,
                     int modeParamId, int operationParamId,
                     std::string fallbackLabelSvg) {
    cpx::CompolyPortsWidget* output = new cpx::CompolyPortsWidget(
        pos, module, outputId, modeParamId, 0.6, true, fallbackLabelSvg);
    output->compolyLabelTransform->visible = false;
    addChild(output);

    CompolyLaneOutputOperationLabel* operationLabel =
        new CompolyLaneOutputOperationLabel(module, operationParamId);
    operationLabel->rightEdgeX = pos.x + 6.f;
    operationLabel->box.pos = Vec(
        operationLabel->rightEdgeX - operationLabel->box.size.x, pos.y + 6.f);
    addChild(operationLabel);
  }

  void appendContextMenu(Menu* menu) override {
    ComputerscareCompolyLane* laneModule =
        dynamic_cast<ComputerscareCompolyLane*>(this->module);
    if (!laneModule) return;

    menu->addChild(new MenuEntry);
    addWrapModeMenu(menu, laneModule, ComputerscareCompolyLane::WRAP_MODE,
                    "Z Polyphonic Mapping Mode");
    addWrapModeMenu(menu, laneModule, ComputerscareCompolyLane::W_WRAP_MODE,
                    "W Polyphonic Mapping Mode");
  }

  void addWrapModeMenu(Menu* menu, ComputerscareCompolyLane* module,
                       int paramId, std::string text) {
    menu->addChild(
        cpx::createCompolyWrapModeMenu(module->paramQuantities[paramId], text));
  }
};

Model* modelComputerscareCompolyLane =
    createModel<ComputerscareCompolyLane, ComputerscareCompolyLaneWidget>(
        "computerscare-compoly-lane");
