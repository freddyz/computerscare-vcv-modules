#include <array>

#include "Computerscare.hpp"
#include "complex/ComplexControl.hpp"
#include "complex/SwitchableComplexControl.hpp"

struct ComputerscareComplexGenerator;

const int numComplexGeneratorKnobs = 16;

struct ComputerscareComplexGenerator : ComputerscareComplexBase {
  ComputerscareSVGPanel* panelRef;
  void setAllControlModes(int mode) {
    mode = std::max(0, std::min(2, mode));
    for (int i = 0; i < numComplexGeneratorKnobs; i++) {
      params[LANE_VIEW_MODE + i].setValue(mode);
    }
    params[SCALE_VIEW_MODE].setValue(mode);
    params[OFFSET_VIEW_MODE].setValue(mode);
  }
  void syncPolarViewsFromRect() {
    for (int i = 0; i < numComplexGeneratorKnobs; i++) {
      if ((int)params[LANE_VIEW_MODE + i].getValue() == 2) {
        cpx::ComplexControl::syncRectParamsToPolarParams(
            this, COMPLEX_XY + 2 * i, LANE_POLAR + 2 * i);
      }
    }
    if ((int)params[SCALE_VIEW_MODE].getValue() == 2) {
      cpx::ComplexControl::syncRectParamsToPolarParams(this, SCALE_VAL_AB,
                                                       SCALE_POLAR);
    }
    if ((int)params[OFFSET_VIEW_MODE].getValue() == 2) {
      cpx::ComplexControl::syncRectParamsToPolarParams(this, OFFSET_VAL_AB,
                                                       OFFSET_POLAR);
    }
  }
  enum ParamIds {
    COMPLEX_XY,

    COMPOLY_CHANNELS = COMPLEX_XY + 2 * numComplexGeneratorKnobs,

    SCALE_VAL_AB,
    SCALE_TRIM_AB = SCALE_VAL_AB + 2,

    OFFSET_VAL_AB = SCALE_TRIM_AB + 2,
    OFFSET_TRIM_AB = OFFSET_VAL_AB + 2,

    DELTA_SCALE_AB = OFFSET_TRIM_AB + 2,
    DELTA_OFFSET_AB = DELTA_SCALE_AB + 2,

    MAIN_OUTPUT_MODE,

    NEXT,
    GLOBAL_SCALE,
    GLOBAL_OFFSET,
    LANE_VIEW_MODE,
    SCALE_VIEW_MODE = LANE_VIEW_MODE + numComplexGeneratorKnobs,
    OFFSET_VIEW_MODE,
    LANE_POLAR = OFFSET_VIEW_MODE + 1,
    SCALE_POLAR = LANE_POLAR + 2 * numComplexGeneratorKnobs,
    OFFSET_POLAR = SCALE_POLAR + 2,
    NUM_PARAMS = OFFSET_POLAR + 2

  };
  enum InputIds { CHANNEL_INPUT, NUM_INPUTS };
  enum OutputIds { COMPOLY_MAIN_OUT_A, COMPOLY_MAIN_OUT_B, NUM_OUTPUTS };
  enum LightIds { NUM_LIGHTS };

  ComputerscareComplexGenerator() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    for (int i = 0; i < numComplexGeneratorKnobs; i++) {
      configParam(COMPLEX_XY + 2 * i, -10.f, 10.f, 0.f,
                  "Lane " + std::to_string(i + 1) + " Real");
      configParam(COMPLEX_XY + 2 * i + 1, -10.f, 10.f, 0.f,
                  "Lane " + std::to_string(i + 1) + " Imaginary");
      cpx::ComplexControl::configParams(this, LANE_POLAR + 2 * i,
                                        cpx::ComplexControlPreset::RThetaKnobs);
      getParamQuantity(LANE_POLAR + 2 * i)->name =
          "Lane " + std::to_string(i + 1) + " Radius";
      getParamQuantity(LANE_POLAR + 2 * i + 1)->name =
          "Lane " + std::to_string(i + 1) + " Theta";
      getParamQuantity(LANE_POLAR + 2 * i)->randomizeEnabled = false;
      getParamQuantity(LANE_POLAR + 2 * i + 1)->randomizeEnabled = false;
    }

    configParam(SCALE_VAL_AB, -3.f, 3.f, 1.f, "Scale Real");
    configParam(SCALE_VAL_AB + 1, -3.f, 3.f, 0.f, "Scale Imaginary");
    getParamQuantity(SCALE_VAL_AB)->randomizeEnabled = false;
    getParamQuantity(SCALE_VAL_AB + 1)->randomizeEnabled = false;

    configParam(OFFSET_VAL_AB, -10.f, 10.f, 0.f, "Offset Real");
    configParam(OFFSET_VAL_AB + 1, -10.f, 10.f, 0.f, "Offset Imaginary");
    getParamQuantity(OFFSET_VAL_AB)->randomizeEnabled = false;
    getParamQuantity(OFFSET_VAL_AB + 1)->randomizeEnabled = false;

    cpx::ComplexControl::configParams(this, SCALE_POLAR,
                                      cpx::ComplexControlPreset::RThetaKnobs);
    cpx::ComplexControl::configParams(this, OFFSET_POLAR,
                                      cpx::ComplexControlPreset::RThetaKnobs);
    getParamQuantity(SCALE_POLAR)->name = "Scale Radius";
    getParamQuantity(SCALE_POLAR + 1)->name = "Scale Theta";
    getParamQuantity(OFFSET_POLAR)->name = "Offset Radius";
    getParamQuantity(OFFSET_POLAR + 1)->name = "Offset Theta";
    getParamQuantity(SCALE_POLAR)->maxValue = 3.f;
    getParamQuantity(SCALE_POLAR)->randomizeEnabled = false;
    getParamQuantity(SCALE_POLAR + 1)->randomizeEnabled = false;
    getParamQuantity(OFFSET_POLAR)->randomizeEnabled = false;
    getParamQuantity(OFFSET_POLAR + 1)->randomizeEnabled = false;

    configParam(DELTA_OFFSET_AB, -10.f, 10.f, 0.f, "Channel ");
    configParam(DELTA_OFFSET_AB + 1, -10.f, 10.f, 0.f, "Channel ");

    configParam(COMPOLY_CHANNELS, 1.f, 16.f, 16.f, "Compoly Lanes");
    configParam(GLOBAL_SCALE, -2.f, 2.f, 1.f, "Scale");
    configParam(GLOBAL_OFFSET, -10.f, 10.f, 0.f, "Offset", " volts");
    configParam<cpx::CompolyModeParam>(MAIN_OUTPUT_MODE, 0.f, 3.f, 0.f,
                                       "Main Output Mode");
    for (int i = 0; i < numComplexGeneratorKnobs; i++) {
      configParam<cpx::ComplexControlViewModeParam>(
          LANE_VIEW_MODE + i, 0.f, 2.f, 0.f,
          "Lane " + std::to_string(i + 1) + " View");
    }
    configParam<cpx::ComplexControlViewModeParam>(SCALE_VIEW_MODE, 0.f, 2.f,
                                                  0.f, "Scale View");
    configParam<cpx::ComplexControlViewModeParam>(OFFSET_VIEW_MODE, 0.f, 2.f,
                                                  0.f, "Offset View");

    getParamQuantity(COMPOLY_CHANNELS)->randomizeEnabled = false;
    getParamQuantity(COMPOLY_CHANNELS)->resetEnabled = false;
    getParamQuantity(GLOBAL_SCALE)->randomizeEnabled = false;
    getParamQuantity(GLOBAL_OFFSET)->randomizeEnabled = false;

    configOutput<cpx::CompolyPortInfo<MAIN_OUTPUT_MODE, 0>>(COMPOLY_MAIN_OUT_A,
                                                            "Main");
    configOutput<cpx::CompolyPortInfo<MAIN_OUTPUT_MODE, 1>>(COMPOLY_MAIN_OUT_B,
                                                            "Main");
  }
  void process(const ProcessArgs& args) override {
    ComputerscarePolyModule::checkCounter();

    float offsetX = params[OFFSET_VAL_AB].getValue();
    float offsetY = params[OFFSET_VAL_AB + 1].getValue();

    float scaleX = params[SCALE_VAL_AB].getValue();
    float scaleY = params[SCALE_VAL_AB + 1].getValue();

    math::Vec scaleRect = Vec(scaleX, scaleY);

    int mainOutputMode = params[MAIN_OUTPUT_MODE].getValue();
    for (int i = 0; i < polyChannels; i++) {
      if (i < numComplexGeneratorKnobs) {
        float x0 = params[COMPLEX_XY + 2 * i].getValue();
        float y0 = params[COMPLEX_XY + 2 * i + 1].getValue();

        float x1 = x0 * scaleRect.x - y0 * scaleRect.y;
        float y1 = x0 * scaleRect.y + y0 * scaleRect.x;

        float outX = x1 + offsetX;
        float outY = y1 + offsetY;
        float outR = std::hypot(outX, outY);
        float outTheta = std::atan2(outY, outX);

        setOutputVoltages(COMPOLY_MAIN_OUT_A, mainOutputMode, i, outX, outY,
                          outR, outTheta);
      }

      // outputs[POLY_OUTPUT].setVoltage(params[KNOB + i].getValue()*trim +
      // offset, i);
    }
  }
  void onRandomize() override { syncPolarViewsFromRect(); }
  void onReset() override { syncPolarViewsFromRect(); }
  void checkPoly() override {
    polyChannels = params[COMPOLY_CHANNELS].getValue();
    if (polyChannels == 0) {
      polyChannels = 16;
      params[COMPOLY_CHANNELS].setValue(16);
    }
    int mainOutputMode = params[MAIN_OUTPUT_MODE].getValue();
    setOutputChannels(COMPOLY_MAIN_OUT_A, mainOutputMode, polyChannels);
  }
  json_t* dataToJson() override {
    json_t* rootJ = json_object();
    return rootJ;
  }

  void dataFromJson(json_t* rootJ) override {
    json_t* laneControlModesJ = json_object_get(rootJ, "laneControlModes");
    if (laneControlModesJ && json_is_array(laneControlModesJ)) {
      for (int i = 0; i < numComplexGeneratorKnobs; i++) {
        json_t* modeJ = json_array_get(laneControlModesJ, i);
        if (modeJ) {
          int mode = json_integer_value(modeJ);
          params[LANE_VIEW_MODE + i].setValue(std::max(0, std::min(2, mode)));
        }
      }
    }
    json_t* scaleControlModeJ = json_object_get(rootJ, "scaleControlMode");
    if (scaleControlModeJ)
      params[SCALE_VIEW_MODE].setValue(
          std::max(0, std::min(2, (int)json_integer_value(scaleControlModeJ))));
    json_t* offsetControlModeJ = json_object_get(rootJ, "offsetControlMode");
    if (offsetControlModeJ)
      params[OFFSET_VIEW_MODE].setValue(std::max(
          0, std::min(2, (int)json_integer_value(offsetControlModeJ))));
  }
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

struct ComplexGeneratorViewModeSwitch : app::Switch {
  std::string label;
  int textAlign = NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE;

  void draw(const DrawArgs& args) override;
};

void ComplexGeneratorViewModeSwitch::draw(const DrawArgs& args) {
  nvgBeginPath(args.vg);
  nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, 1.5f);
  nvgFillColor(args.vg, nvgRGBA(8, 28, 24, 42));
  nvgFill(args.vg);

  nvgFontSize(args.vg, 9.f);
  nvgTextAlign(args.vg, textAlign);
  nvgFillColor(args.vg, nvgRGB(28, 34, 28));
  float textX = box.size.x * 0.5f;
  if (textAlign & NVG_ALIGN_RIGHT)
    textX = box.size.x - 1.f;
  else if (textAlign & NVG_ALIGN_LEFT)
    textX = 1.f;
  nvgText(args.vg, textX, box.size.y * 0.58f, label.c_str(), nullptr);
}

struct ComplexGeneratorSetAllViewModeItem : MenuItem {
  ComputerscareComplexGenerator* module = nullptr;
  int mode = 0;

  void onAction(const event::Action& e) override {
    if (module) module->setAllControlModes(mode);
  }
};

struct ComputerscareComplexGeneratorWidget : ModuleWidget {
  ComputerscareComplexGeneratorWidget(ComputerscareComplexGenerator* module) {
    setModule(module);
    // setPanel(APP->window->loadSvg(asset::plugin(pluginInstance,
    // "res/panels/ComputerscareComplexGeneratorPanel.svg")));
    box.size = Vec(8 * 15, 380);
    {
      ComputerscareSVGPanel* panel = new ComputerscareSVGPanel();
      panel->box.size = box.size;
      panel->setBackground(APP->window->loadSvg(
          asset::plugin(pluginInstance,
                        "res/panels/ComputerscareComplexGeneratorPanel.svg")));
      addChild(panel);
    }
    channelWidget = new CompolyLaneCountWidget(
        Vec(72, 0), module, ComputerscareComplexGenerator::COMPOLY_CHANNELS,
        &module->polyChannels, false, 1.44f, 16.f);

    // addOutput(createOutput<PointingUpPentagonPort>(Vec(30, 22), module,
    // ComputerscareComplexGenerator::POLY_OUTPUT));

    addChild(channelWidget);

    cpx::CompolyPortsWidget* mainOutput = new cpx::CompolyPortsWidget(
        Vec(61, 28), module, ComputerscareComplexGenerator::COMPOLY_MAIN_OUT_A,
        ComputerscareComplexGenerator::MAIN_OUTPUT_MODE, 0.6);
    mainOutput->compolyLabelTransform->box.pos = Vec(44, 34);
    addChild(mainOutput);

    addModeControl("scale", 5, 22, module,
                   ComputerscareComplexGenerator::SCALE_VAL_AB,
                   ComputerscareComplexGenerator::SCALE_POLAR,
                   ComputerscareComplexGenerator::SCALE_VIEW_MODE,
                   cpx::ComplexXYMaxMode::Radial, 3.f);
    addModeControl("offset", 5, 53, module,
                   ComputerscareComplexGenerator::OFFSET_VAL_AB,
                   ComputerscareComplexGenerator::OFFSET_POLAR,
                   ComputerscareComplexGenerator::OFFSET_VIEW_MODE,
                   cpx::ComplexXYMaxMode::Rectangular, 10.f);

    // addParam(createParam<NoRandomSmallKnob>(Vec(11, 54), module,
    // ComputerscareComplexGenerator::GLOBAL_SCALE));
    // addParam(createParam<NoRandomMediumSmallKnob>(Vec(32, 57), module,
    // ComputerscareComplexGenerator::GLOBAL_OFFSET));

    constexpr float yInitial = 86.f;
    constexpr float ySpacing = 36.f;
    constexpr float leftColumnX = 5.4f;
    constexpr float rightColumnX = 45.4f;
    constexpr float rightColumnY = yInitial + 6.f;
    for (int i = 0; i < numComplexGeneratorKnobs; i++) {
      bool isRightColumn = i >= 8;
      float xx = isRightColumn ? rightColumnX : leftColumnX;
      float yy = (isRightColumn ? rightColumnY : yInitial) + ySpacing * (i % 8);
      addLabeledKnob(std::to_string(i + 1), xx, yy, module, i * 2,
                     isRightColumn ? 0.f : -3.f, 2.f);
    }
  }
  void addSmallLabel(std::string label, int x, int y, float fontSize) {
    SmallLetterDisplay* labelDisplay = new SmallLetterDisplay();
    labelDisplay->box.size = Vec(25, 10);
    labelDisplay->box.pos = Vec(x, y);
    labelDisplay->fontSize = fontSize;
    labelDisplay->value = label;
    labelDisplay->letterSpacing = 0.2f;
    labelDisplay->textAlign = 1;
    addChild(labelDisplay);
  }

  void addModeControl(std::string label, int x, int y,
                      ComputerscareComplexGenerator* module, int paramIndex,
                      int polarParamIndex, int modeParamId,
                      cpx::ComplexXYMaxMode arrowMaxMode,
                      float arrowMaxVoltage) {
    cpx::SwitchableComplexControl* control = new cpx::SwitchableComplexControl(
        module, paramIndex, polarParamIndex, modeParamId, arrowMaxMode,
        arrowMaxVoltage);
    control->box = Rect(Vec(x, y), Vec(32, 25));
    control->setArrowDrawingScale(0.78f);
    addChild(control);

    ComplexGeneratorViewModeSwitch* modeButton =
        createParam<ComplexGeneratorViewModeSwitch>(Vec(x - 2, y - 12), module,
                                                    modeParamId);
    modeButton->box = Rect(Vec(x - 2, y - 12), Vec(32.f, 12.f));
    modeButton->label = label;
    addParam(modeButton);
  }

  void addLabeledKnob(std::string label, float x, float y,
                      ComputerscareComplexGenerator* module, int index,
                      float labelDx, float labelDy) {
    /*	ParamWidget* pob =  createParam<DisableableSmoothKnob>(Vec(x, y),
       module, ComputerscareComplexGenerator::KNOB + index);

            DisableableSmoothKnob* fader =
       dynamic_cast<DisableableSmoothKnob*>(pob);

            fader->module = module;
            fader->channel = index;

            addParam(fader);*/

    cpx::SwitchableComplexControl* control = new cpx::SwitchableComplexControl(
        module, ComputerscareComplexGenerator::COMPLEX_XY + index,
        ComputerscareComplexGenerator::LANE_POLAR + index,
        ComputerscareComplexGenerator::LANE_VIEW_MODE + index / 2,
        cpx::ComplexXYMaxMode::Rectangular, 10.f, index / 2);
    control->box = Rect(Vec(x, y), Vec(32, 25));
    control->setArrowDrawingScale(0.78f);
    control->setArrowYOffset(-2.f);
    addChild(control);

    float labelWidth = label.size() > 1 ? 17.f : 12.f;
    bool isSecondColumn = index / 2 >= 8;
    Vec labelPos = isSecondColumn ? Vec(x + 32.f - labelWidth, y - 12.f)
                                  : Vec(x + labelDx, y - 12 + labelDy);

    ComplexGeneratorViewModeSwitch* modeButton =
        createParam<ComplexGeneratorViewModeSwitch>(
            labelPos, module,
            ComputerscareComplexGenerator::LANE_VIEW_MODE + index / 2);
    modeButton->box = Rect(labelPos, Vec(labelWidth, 12.f));
    modeButton->label = label;
    if (isSecondColumn)
      modeButton->textAlign = NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE;
    addParam(modeButton);
  }

  void appendContextMenu(Menu* menu) override {
    ComputerscareComplexGenerator* generator =
        dynamic_cast<ComputerscareComplexGenerator*>(module);

    menu->addChild(new MenuSeparator);
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "View"));
    menu->addChild(construct<ComplexGeneratorSetAllViewModeItem>(
        &MenuItem::text, "set all to arrow",
        &ComplexGeneratorSetAllViewModeItem::module, generator,
        &ComplexGeneratorSetAllViewModeItem::mode, 0));
    menu->addChild(construct<ComplexGeneratorSetAllViewModeItem>(
        &MenuItem::text, "set all to xy",
        &ComplexGeneratorSetAllViewModeItem::module, generator,
        &ComplexGeneratorSetAllViewModeItem::mode, 1));
    menu->addChild(construct<ComplexGeneratorSetAllViewModeItem>(
        &MenuItem::text, "set all to rθ",
        &ComplexGeneratorSetAllViewModeItem::module, generator,
        &ComplexGeneratorSetAllViewModeItem::mode, 2));
  }

  CompolyLaneCountWidget* channelWidget;
  PolyChannelsDisplay* channelDisplay;
  DisableableSmoothKnob* fader;
  SmallLetterDisplay* smallLetterDisplay;
};

Model* modelComputerscareComplexGenerator =
    createModel<ComputerscareComplexGenerator,
                ComputerscareComplexGeneratorWidget>(
        "computerscare-complex-generator");
