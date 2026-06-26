#include <array>

#include "Computerscare.hpp"
#include "complex/PerspectivePentagonWidget.hpp"
#include "complex/SwitchableComplexControl.hpp"
#include "complex/math/ComplexMath.hpp"

struct ComputerscareComplexTransformer;

struct ComputerscareComplexTransformer : ComputerscareComplexBase {
  ComputerscareSVGPanel* panelRef;
  int compolyChannelsMainOutput = 0;

  enum ParamIds {

    COMPOLY_CHANNELS,

    Z_SCALE_VAL_AB,
    SCALE_VAL_AB = Z_SCALE_VAL_AB,
    SCALE_TRIM_AB = Z_SCALE_VAL_AB + 2,

    OFFSET_VAL_AB = SCALE_TRIM_AB + 2,
    Z_OFFSET_VAL_AB = OFFSET_VAL_AB,
    OFFSET_TRIM_AB = OFFSET_VAL_AB + 2,

    Z_INPUT_MODE,
    W_INPUT_MODE,
    A_INPUT_MODE,
    B_INPUT_MODE,
    C_INPUT_MODE,
    MAIN_OUTPUT_MODE,
    PRODUCT_OUTPUT_MODE,
    W_SCALE_VAL_AB,
    W_OFFSET_VAL_AB = W_SCALE_VAL_AB + 2,
    Z_SCALE_VIEW_MODE = W_OFFSET_VAL_AB + 2,
    Z_OFFSET_VIEW_MODE,
    W_SCALE_VIEW_MODE,
    W_OFFSET_VIEW_MODE,
    Z_SCALE_POLAR,
    Z_OFFSET_POLAR = Z_SCALE_POLAR + 2,
    W_SCALE_POLAR = Z_OFFSET_POLAR + 2,
    W_OFFSET_POLAR = W_SCALE_POLAR + 2,
    COMPOLY_WRAP_MODE = W_OFFSET_POLAR + 2,
    NUM_PARAMS
  };
  enum InputIds {
    Z_INPUT,
    W_INPUT = Z_INPUT + 2,
    A_INPUT = W_INPUT + 2,
    B_INPUT = A_INPUT + 2,
    C_INPUT = B_INPUT + 2,
    NUM_INPUTS = C_INPUT + 2,
  };
  enum OutputIds {
    COMPOLY_MAIN_OUT_A,
    COMPOLY_MAIN_OUT_B,
    COMPOLY_PRODUCT_OUT_A,
    COMPOLY_PRODUCT_OUT_B,
    NUM_OUTPUTS
  };
  enum LightIds { NUM_LIGHTS };

  ComputerscareComplexTransformer() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    configComplexAffineParams(Z_SCALE_VAL_AB, Z_OFFSET_VAL_AB, Z_SCALE_POLAR,
                              Z_OFFSET_POLAR, "z");
    configComplexAffineParams(W_SCALE_VAL_AB, W_OFFSET_VAL_AB, W_SCALE_POLAR,
                              W_OFFSET_POLAR, "w");

    configSwitch(COMPOLY_CHANNELS, 0.f, 16.f, 0.f, "Compoly Lanes",
                 polyChannelLabels(true));
    getParamQuantity(COMPOLY_CHANNELS)->randomizeEnabled = false;
    getParamQuantity(COMPOLY_CHANNELS)->resetEnabled = false;

    configParam<cpx::CompolyModeParam>(
        MAIN_OUTPUT_MODE, cpx::complex_math::firstCoordinateModeValue,
        cpx::complex_math::lastCoordinateModeValue,
        cpx::complex_math::defaultCoordinateModeValue, "Main Output Mode");
    configParam<cpx::CompolyModeParam>(
        PRODUCT_OUTPUT_MODE, cpx::complex_math::firstCoordinateModeValue,
        cpx::complex_math::lastCoordinateModeValue,
        cpx::complex_math::defaultCoordinateModeValue, "Product Output Mode");

    configParam<cpx::CompolyModeParam>(
        Z_INPUT_MODE, cpx::complex_math::firstCoordinateModeValue,
        cpx::complex_math::lastCoordinateModeValue,
        cpx::complex_math::defaultCoordinateModeValue, "z Input Mode");
    configParam<cpx::CompolyModeParam>(
        W_INPUT_MODE, cpx::complex_math::firstCoordinateModeValue,
        cpx::complex_math::lastCoordinateModeValue,
        cpx::complex_math::defaultCoordinateModeValue, "w Input Mode");
    configParam<cpx::CompolyModeParam>(
        A_INPUT_MODE, cpx::complex_math::firstCoordinateModeValue,
        cpx::complex_math::lastCoordinateModeValue,
        cpx::complex_math::defaultCoordinateModeValue, "a Input Mode");
    configParam<cpx::CompolyModeParam>(
        B_INPUT_MODE, cpx::complex_math::firstCoordinateModeValue,
        cpx::complex_math::lastCoordinateModeValue,
        cpx::complex_math::defaultCoordinateModeValue, "b Input Mode");
    configParam<cpx::CompolyModeParam>(
        C_INPUT_MODE, cpx::complex_math::firstCoordinateModeValue,
        cpx::complex_math::lastCoordinateModeValue,
        cpx::complex_math::defaultCoordinateModeValue, "c Input Mode");
    configParam<cpx::ComplexControlViewModeParam>(Z_SCALE_VIEW_MODE, 0.f, 2.f,
                                                  0.f, "z Scale View");
    configParam<cpx::ComplexControlViewModeParam>(Z_OFFSET_VIEW_MODE, 0.f, 2.f,
                                                  0.f, "z Offset View");
    configParam<cpx::ComplexControlViewModeParam>(W_SCALE_VIEW_MODE, 0.f, 2.f,
                                                  0.f, "w Scale View");
    configParam<cpx::ComplexControlViewModeParam>(W_OFFSET_VIEW_MODE, 0.f, 2.f,
                                                  0.f, "w Offset View");

    configMenuParam(COMPOLY_WRAP_MODE, cpx::polyphonic::defaultMappingModeValue,
                    "Compoly Wrapping Mode",
                    cpx::compoly::wrapModeDescriptions());

    getParamQuantity(MAIN_OUTPUT_MODE)->randomizeEnabled = false;
    getParamQuantity(PRODUCT_OUTPUT_MODE)->randomizeEnabled = false;
    getParamQuantity(Z_INPUT_MODE)->randomizeEnabled = false;
    getParamQuantity(W_INPUT_MODE)->randomizeEnabled = false;
    getParamQuantity(A_INPUT_MODE)->randomizeEnabled = false;
    getParamQuantity(B_INPUT_MODE)->randomizeEnabled = false;
    getParamQuantity(C_INPUT_MODE)->randomizeEnabled = false;
    getParamQuantity(COMPOLY_WRAP_MODE)->randomizeEnabled = false;
    getParamQuantity(COMPOLY_WRAP_MODE)->resetEnabled = false;

    configInput<cpx::CompolyPortInfo<Z_INPUT_MODE, 0>>(Z_INPUT, "z");
    configInput<cpx::CompolyPortInfo<Z_INPUT_MODE, 1>>(Z_INPUT + 1, "z");

    configInput<cpx::CompolyPortInfo<W_INPUT_MODE, 0>>(W_INPUT, "w");
    configInput<cpx::CompolyPortInfo<W_INPUT_MODE, 1>>(W_INPUT + 1, "w");

    configInput<cpx::CompolyPortInfo<A_INPUT_MODE, 0>>(A_INPUT, "a");
    configInput<cpx::CompolyPortInfo<A_INPUT_MODE, 1>>(A_INPUT + 1, "a");

    configOutput<cpx::CompolyPortInfo<MAIN_OUTPUT_MODE, 0>>(COMPOLY_MAIN_OUT_A,
                                                            "z+w");
    configOutput<cpx::CompolyPortInfo<MAIN_OUTPUT_MODE, 1>>(COMPOLY_MAIN_OUT_B,
                                                            "z+w");

    configOutput<cpx::CompolyPortInfo<PRODUCT_OUTPUT_MODE, 0>>(
        COMPOLY_PRODUCT_OUT_A, "z*w");
    configOutput<cpx::CompolyPortInfo<PRODUCT_OUTPUT_MODE, 1>>(
        COMPOLY_PRODUCT_OUT_B, "z*w");
  }

  void configComplexAffineParams(int scaleParamId, int offsetParamId,
                                 int scalePolarParamId, int offsetPolarParamId,
                                 const std::string& inputName) {
    configParam(scaleParamId, -3.f, 3.f, 1.f, inputName + " Scale Real");
    configParam(scaleParamId + 1, -3.f, 3.f, 0.f,
                inputName + " Scale Imaginary");
    configParam(offsetParamId, -10.f, 10.f, 0.f, inputName + " Offset Real");
    configParam(offsetParamId + 1, -10.f, 10.f, 0.f,
                inputName + " Offset Imaginary");

    cpx::ComplexControl::configParams(this, scalePolarParamId,
                                      cpx::ComplexControlPreset::RThetaKnobs);
    cpx::ComplexControl::configParams(this, offsetPolarParamId,
                                      cpx::ComplexControlPreset::RThetaKnobs);
    getParamQuantity(scalePolarParamId)->name = inputName + " Scale Radius";
    getParamQuantity(scalePolarParamId + 1)->name = inputName + " Scale Theta";
    getParamQuantity(offsetPolarParamId)->name = inputName + " Offset Radius";
    getParamQuantity(offsetPolarParamId + 1)->name =
        inputName + " Offset Theta";
    getParamQuantity(scalePolarParamId)->maxValue = 3.f;

    for (int id : {scalePolarParamId, scalePolarParamId + 1, offsetPolarParamId,
                   offsetPolarParamId + 1}) {
      getParamQuantity(id)->randomizeEnabled = false;
    }
  }

  void setAllControlModes(int mode) {
    mode = std::max(0, std::min(2, mode));
    for (int id : {Z_SCALE_VIEW_MODE, Z_OFFSET_VIEW_MODE, W_SCALE_VIEW_MODE,
                   W_OFFSET_VIEW_MODE}) {
      params[id].setValue(mode);
    }
  }

  void syncPolarViewsFromRect() {
    syncPolarViewFromRect(Z_SCALE_VIEW_MODE, Z_SCALE_VAL_AB, Z_SCALE_POLAR);
    syncPolarViewFromRect(Z_OFFSET_VIEW_MODE, Z_OFFSET_VAL_AB, Z_OFFSET_POLAR);
    syncPolarViewFromRect(W_SCALE_VIEW_MODE, W_SCALE_VAL_AB, W_SCALE_POLAR);
    syncPolarViewFromRect(W_OFFSET_VIEW_MODE, W_OFFSET_VAL_AB, W_OFFSET_POLAR);
  }

  void syncPolarViewFromRect(int viewModeParamId, int rectParamId,
                             int polarParamId) {
    if ((int)params[viewModeParamId].getValue() == 2) {
      cpx::ComplexControl::syncRectParamsToPolarParams(this, rectParamId,
                                                       polarParamId);
    }
  }

  void applyComplexAffine(float* x, float* y, int scaleParamId,
                          int offsetParamId) {
    float scaleX = params[scaleParamId].getValue();
    float scaleY = params[scaleParamId + 1].getValue();
    float offsetX = params[offsetParamId].getValue();
    float offsetY = params[offsetParamId + 1].getValue();

    for (uint8_t c = 0; c < 16; c += 4) {
      simd::float_4 xv = simd::float_4::load(x + c);
      simd::float_4 yv = simd::float_4::load(y + c);
      (xv * scaleX - yv * scaleY + offsetX).store(x + c);
      (xv * scaleY + yv * scaleX + offsetY).store(y + c);
    }
  }

  int inputCompolyChannels(int inputMode, int firstPortId) {
    return cpx::compoly::compolyLanesForInput(
        coordinateModeFromParam(inputMode), inputs[firstPortId].getChannels(),
        inputs[firstPortId + 1].getChannels());
  }

  void process(const ProcessArgs& args) override {
    ComputerscarePolyModule::checkCounter();

    int compolyphonyKnobSetting = params[COMPOLY_CHANNELS].getValue();
    int zInputMode = params[Z_INPUT_MODE].getValue();
    int wInputMode = params[W_INPUT_MODE].getValue();
    int wrapMode = params[COMPOLY_WRAP_MODE].getValue();

    int zInputCompolyChannels = inputCompolyChannels(zInputMode, Z_INPUT);
    int wInputCompolyChannels = inputCompolyChannels(wInputMode, W_INPUT);
    compolyChannelsMainOutput = cpx::compoly::outputCompolyLanes(
        compolyphonyKnobSetting,
        std::max(zInputCompolyChannels, wInputCompolyChannels));
    int mainOutputMode = params[MAIN_OUTPUT_MODE].getValue();
    setOutputChannels(COMPOLY_MAIN_OUT_A, mainOutputMode,
                      compolyChannelsMainOutput);

    int productOutputMode = params[PRODUCT_OUTPUT_MODE].getValue();
    setOutputChannels(COMPOLY_PRODUCT_OUT_A, productOutputMode,
                      compolyChannelsMainOutput);

    bool mainOutputConnected = outputPairConnected(COMPOLY_MAIN_OUT_A);
    bool productOutputConnected = outputPairConnected(COMPOLY_PRODUCT_OUT_A);
    if (!mainOutputConnected && !productOutputConnected) return;

    float zx[16] = {};
    float zy[16] = {};

    float wx[16] = {};
    float wy[16] = {};

    float sumx[16] = {};
    float sumy[16] = {};

    float prodx[16] = {};
    float prody[16] = {};

    readComplexInputPairToRect(Z_INPUT, zInputMode, zx, zy, wrapMode,
                               compolyChannelsMainOutput);
    readComplexInputPairToRect(W_INPUT, wInputMode, wx, wy, wrapMode,
                               compolyChannelsMainOutput);
    applyComplexAffine(zx, zy, Z_SCALE_VAL_AB, Z_OFFSET_VAL_AB);
    applyComplexAffine(wx, wy, W_SCALE_VAL_AB, W_OFFSET_VAL_AB);

    if (mainOutputConnected) {
      for (uint8_t c = 0; c < 16; c += 4) {
        simd::float_4 zxv = simd::float_4::load(zx + c);
        simd::float_4 zyv = simd::float_4::load(zy + c);
        simd::float_4 wxv = simd::float_4::load(wx + c);
        simd::float_4 wyv = simd::float_4::load(wy + c);

        (zxv + wxv).store(sumx + c);
        (zyv + wyv).store(sumy + c);
      }
    }

    if (productOutputConnected) {
      for (uint8_t c = 0; c < 16; c += 4) {
        simd::float_4 zxv = simd::float_4::load(zx + c);
        simd::float_4 zyv = simd::float_4::load(zy + c);
        simd::float_4 wxv = simd::float_4::load(wx + c);
        simd::float_4 wyv = simd::float_4::load(wy + c);

        (zxv * wxv - zyv * wyv).store(prodx + c);
        (zxv * wyv + zyv * wxv).store(prody + c);
      }
    }

    if (mainOutputConnected) {
      writeComplexOutputPairFromRect(COMPOLY_MAIN_OUT_A, mainOutputMode, sumx,
                                     sumy);
    }
    if (productOutputConnected) {
      writeComplexOutputPairFromRect(COMPOLY_PRODUCT_OUT_A, productOutputMode,
                                     prodx, prody);
    }
  }

  json_t* dataToJson() override {
    json_t* rootJ = json_object();
    return rootJ;
  }

  void dataFromJson(json_t* rootJ) override {}
  void onRandomize() override { syncPolarViewsFromRect(); }
  void onReset() override { syncPolarViewsFromRect(); }
};

struct NoRandomSmallKnob : SmallKnob {
  NoRandomSmallKnob() { SmallKnob(); };
};
struct NoRandomMediumSmallKnob : RoundKnob {
  std::shared_ptr<Svg> enabledSvg = APP->window->loadSvg(asset::plugin(
      pluginInstance, "res/components/computerscare-medium-small-knob.svg"));

  NoRandomMediumSmallKnob() {
    setSvg(enabledSvg);
    RoundKnob();
  };
};

struct DisableableSmoothKnob : RoundKnob {
  std::shared_ptr<Svg> enabledSvg = APP->window->loadSvg(asset::plugin(
      pluginInstance, "res/components/computerscare-medium-small-knob.svg"));
  std::shared_ptr<Svg> disabledSvg = APP->window->loadSvg(asset::plugin(
      pluginInstance,
      "res/components/computerscare-medium-small-knob-disabled.svg"));

  int channel = 0;
  bool disabled = false;
  ComputerscarePolyModule* module;

  DisableableSmoothKnob() {
    setSvg(enabledSvg);
    shadow->box.size = math::Vec(0, 0);
    shadow->opacity = 0.f;
  }
  void step() override {
    if (module) {
      bool candidate = channel > module->polyChannels - 1;
      if (disabled != candidate) {
        setSvg(candidate ? disabledSvg : enabledSvg);
        onChange(*(new event::Change()));
        fb->dirty = true;
        disabled = candidate;
      }
    } else {
    }
    RoundKnob::step();
  }
};

struct ComputerscareComplexTransformerWidget : ModuleWidget {
  ComputerscareComplexTransformerWidget(
      ComputerscareComplexTransformer* module) {
    setModule(module);
    box.size = Vec(8 * 15, 380);

    addPerspectiveDisplay(Vec(64.f, -1.f), Vec(54.f, 42.f), 201, true, false);
    addPerspectiveDisplay(Vec(2.f, -1.f), Vec(58.f, 42.f), 200, true, false);
    addPerspectiveDisplay(Vec(64.f, -1.f), Vec(54.f, 42.f), 201, false, true);
    addPerspectiveDisplay(Vec(2.f, -1.f), Vec(58.f, 42.f), 200, false, true);

    addTextDisplay(Vec(7.f, 5.f), Vec(58.f, 24.f), "complex\ntransformer", 10.f,
                   NVG_ALIGN_LEFT, 64.f);

    channelWidget = new CompolyLaneCountWidget(
        Vec(74, 4), module, ComputerscareComplexTransformer::COMPOLY_CHANNELS,
        module ? &module->compolyChannelsMainOutput : nullptr, true, 1.2f, 0.f);

    // addOutput(createOutput<PointingUpPentagonPort>(Vec(30, 22), module,
    // ComputerscareComplexTransformer::POLY_OUTPUT));

    addChild(channelWidget);

    addPerspectiveBlock(Vec(2.f, 43.f), Vec(116.f, 47.f), 202);

    cpx::CompolyPortsWidget* zInput = new cpx::CompolyPortsWidget(
        Vec(50, 60), module, ComputerscareComplexTransformer::Z_INPUT,
        ComputerscareComplexTransformer::Z_INPUT_MODE, 0.8, false, "z");
    zInput->compolyLabel->box.pos = Vec(18, 6);
    addChild(zInput);
    addAffineControls("scale", "offset", Vec(6, 103), module,
                      ComputerscareComplexTransformer::Z_SCALE_VAL_AB,
                      ComputerscareComplexTransformer::Z_SCALE_POLAR,
                      ComputerscareComplexTransformer::Z_SCALE_VIEW_MODE,
                      ComputerscareComplexTransformer::Z_OFFSET_VAL_AB,
                      ComputerscareComplexTransformer::Z_OFFSET_POLAR,
                      ComputerscareComplexTransformer::Z_OFFSET_VIEW_MODE, "z");

    addPerspectiveBlock(Vec(2.f, 151.f), Vec(116.f, 47.f), 203);

    cpx::CompolyPortsWidget* wInput = new cpx::CompolyPortsWidget(
        Vec(50, 168), module, ComputerscareComplexTransformer::W_INPUT,
        ComputerscareComplexTransformer::W_INPUT_MODE, 1.f, false, "w");
    wInput->compolyLabel->box.pos = Vec(15, 6);
    addChild(wInput);
    addAffineControls("scale", "offset", Vec(6, 211), module,
                      ComputerscareComplexTransformer::W_SCALE_VAL_AB,
                      ComputerscareComplexTransformer::W_SCALE_POLAR,
                      ComputerscareComplexTransformer::W_SCALE_VIEW_MODE,
                      ComputerscareComplexTransformer::W_OFFSET_VAL_AB,
                      ComputerscareComplexTransformer::W_OFFSET_POLAR,
                      ComputerscareComplexTransformer::W_OFFSET_VIEW_MODE, "w");

    // addParam(createParam<NoRandomSmallKnob>(Vec(11, 54), module,
    // ComputerscareComplexTransformer::GLOBAL_SCALE));
    // addParam(createParam<NoRandomMediumSmallKnob>(Vec(32, 57), module,
    // ComputerscareComplexTransformer::GLOBAL_OFFSET));

    addPerspectiveBlock(Vec(2.f, 259.f), Vec(116.f, 47.f), 204);

    cpx::CompolyPortsWidget* mainOutput = new cpx::CompolyPortsWidget(
        Vec(50, 274), module,
        ComputerscareComplexTransformer::COMPOLY_MAIN_OUT_A,
        ComputerscareComplexTransformer::MAIN_OUTPUT_MODE, 0.7f, true,
        "zplusw");
    mainOutput->compolyLabel->box.pos = Vec(0, 10);
    addChild(mainOutput);

    addPerspectiveBlock(Vec(2.f, 309.f), Vec(116.f, 47.f), 205);

    cpx::CompolyPortsWidget* productOutput = new cpx::CompolyPortsWidget(
        Vec(50, 324), module,
        ComputerscareComplexTransformer::COMPOLY_PRODUCT_OUT_A,
        ComputerscareComplexTransformer::PRODUCT_OUTPUT_MODE, 0.7f, true,
        "ztimesw");
    productOutput->compolyLabel->box.pos = Vec(0, 10);
    addChild(productOutput);

    addPerspectiveBlock(Vec(2.f, 359.f), Vec(116.f, 22.f), 206);

    SvgWidget* logo = new SvgWidget();
    logo->setSvg(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/components/computerscare-logo-light.svg")));
    logo->box.pos = Vec(27.f, 360.f);
    logo->box.size = Vec(66.f, 20.f);
    addChild(logo);
  }

  void addPerspectiveDisplay(Vec pos, Vec size, int seed, bool drawSides,
                             bool drawFace) {
    cpx::PerspectivePentagonWidget* block =
        new cpx::PerspectivePentagonWidget(nullptr, seed);
    block->box = Rect(pos, size);
    block->setDrawParts(drawSides, drawFace);
    addChild(block);
  }

  void addPerspectiveBlock(Vec pos, Vec size, int seed) {
    addPerspectiveDisplay(pos, size, seed, true, false);
    addPerspectiveDisplay(pos, size, seed, false, true);
  }

  void addTextDisplay(Vec pos, Vec size, std::string text, float fontSize,
                      int textAlign, float breakRowWidth) {
    SmallLetterDisplay* display = new SmallLetterDisplay();
    display->box = Rect(pos, size);
    display->value = text;
    display->fontSize = fontSize;
    display->letterSpacing = 0.4f;
    display->textAlign = textAlign;
    display->breakRowWidth = breakRowWidth;
    addChild(display);
  }

  void addAffineControls(std::string scaleLabel, std::string offsetLabel,
                         Vec pos, ComputerscareComplexTransformer* module,
                         int scaleParamId, int scalePolarParamId,
                         int scaleViewModeParamId, int offsetParamId,
                         int offsetPolarParamId, int offsetViewModeParamId,
                         const std::string& inputName) {
    Vec controlPos = pos.plus(Vec(-2.f, -12.f));
    addModeControl(offsetLabel, controlPos.x + 56.f, controlPos.y, module,
                   offsetParamId, offsetPolarParamId, offsetViewModeParamId,
                   cpx::ComplexXYMaxMode::Rectangular, 10.f,
                   inputName + " Offset");
    addModeControl(scaleLabel, controlPos.x, controlPos.y, module, scaleParamId,
                   scalePolarParamId, scaleViewModeParamId,
                   cpx::ComplexXYMaxMode::Radial, 3.f, inputName + " Scale");
  }

  void addModeControl(std::string label, float x, float y,
                      ComputerscareComplexTransformer* module, int paramIndex,
                      int polarParamIndex, int modeParamId,
                      cpx::ComplexXYMaxMode arrowMaxMode, float arrowMaxVoltage,
                      const std::string& controlName) {
    cpx::PerspectiveLabeledSwitchableComplexControl* control =
        new cpx::PerspectiveLabeledSwitchableComplexControl(
            nullptr, paramIndex, module, paramIndex, polarParamIndex,
            modeParamId, arrowMaxMode, arrowMaxVoltage, label, Vec(34.f, 26.f),
            Vec(17.f, 2.f), Vec(34.f, 10.f), -1, false, controlName,
            NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE, Vec(11.f, 7.f));
    control->box = Rect(Vec(x, y), Vec(56.f, 43.f));
    control->setDrawParts(true, true);
    control->setContentFillsBox(true);
    control->setHoverHighlightEnabled(true);
    control->setBaseShades(0xa4, 0xa8, 0x0a);
    control->setArrowDrawingScale(0.76f);
    control->setArrowYOffset(-5.f);
    addChild(control);
  }

  void appendContextMenu(Menu* menu) override {
    ComputerscareComplexTransformer* transformerModule =
        dynamic_cast<ComputerscareComplexTransformer*>(this->module);
    if (transformerModule) {
      menu->addChild(new MenuEntry);
      menu->addChild(cpx::createCompolyWrapModeMenu(
          transformerModule->paramQuantities
              [ComputerscareComplexTransformer::COMPOLY_WRAP_MODE],
          "Compoly Wrapping Mode"));
    }

    cpx::addSetAllComplexControlViewModeMenu(
        menu, module,
        {ComputerscareComplexTransformer::Z_SCALE_VIEW_MODE,
         ComputerscareComplexTransformer::Z_OFFSET_VIEW_MODE,
         ComputerscareComplexTransformer::W_SCALE_VIEW_MODE,
         ComputerscareComplexTransformer::W_OFFSET_VIEW_MODE});
  }

  CompolyLaneCountWidget* channelWidget;
  PolyChannelsDisplay* channelDisplay;
  DisableableSmoothKnob* fader;
  SmallLetterDisplay* smallLetterDisplay;
};

Model* modelComputerscareComplexTransformer =
    createModel<ComputerscareComplexTransformer,
                ComputerscareComplexTransformerWidget>(
        "computerscare-complex-transformer");
