#include <array>

#include "Computerscare.hpp"
#include "complex/ComplexControl.hpp"
#include "complex/PerspectivePentagonWidget.hpp"
#include "complex/SwitchableComplexControl.hpp"

struct ComputerscareComplexGenerator;

const int numComplexGeneratorKnobs = 16;

struct ComputerscareComplexGenerator : ComputerscareComplexBase {
  ComputerscareSVGPanel* panelRef;
  cpx::PerspectivePentagonSettings pentagonSettings;

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

    configSwitch(COMPOLY_CHANNELS, 1.f, 16.f, 16.f, "Compoly Lanes",
                 polyChannelLabels(false));
    configParam(GLOBAL_SCALE, -2.f, 2.f, 1.f, "Scale");
    configParam(GLOBAL_OFFSET, -10.f, 10.f, 0.f, "Offset", " volts");
    configParam<cpx::CompolyModeParam>(
        MAIN_OUTPUT_MODE, cpx::complex_math::firstCoordinateModeValue,
        cpx::complex_math::lastCoordinateModeValue,
        cpx::complex_math::defaultCoordinateModeValue, "Main Output Mode");
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
    json_object_set_new(rootJ, "pentagonDisplaySize",
                        json_integer(pentagonSettings.size));
    json_object_set_new(rootJ, "pentagonPerspective",
                        json_integer(pentagonSettings.preset));
    json_object_set_new(rootJ, "pentagonColorVariation",
                        json_integer(pentagonSettings.colorVariation));
    json_object_set_new(rootJ, "pentagonShape",
                        json_integer(pentagonSettings.shape));
    return rootJ;
  }

  void dataFromJson(json_t* rootJ) override {
    json_t* pentagonDisplaySizeJ =
        json_object_get(rootJ, "pentagonDisplaySize");
    if (pentagonDisplaySizeJ) {
      pentagonSettings.size = cpx::clampPerspectivePentagonSize(
          json_integer_value(pentagonDisplaySizeJ));
    }
    json_t* pentagonPerspectiveJ =
        json_object_get(rootJ, "pentagonPerspective");
    if (pentagonPerspectiveJ) {
      pentagonSettings.preset = cpx::clampPerspectivePentagonPreset(
          json_integer_value(pentagonPerspectiveJ));
    }
    json_t* pentagonColorVariationJ =
        json_object_get(rootJ, "pentagonColorVariation");
    if (pentagonColorVariationJ) {
      pentagonSettings.colorVariation =
          cpx::clampPerspectivePentagonColorVariation(
              json_integer_value(pentagonColorVariationJ));
    }
    json_t* pentagonShapeJ = json_object_get(rootJ, "pentagonShape");
    if (pentagonShapeJ) {
      pentagonSettings.shape = cpx::clampPerspectivePentagonShape(
          json_integer_value(pentagonShapeJ));
    }

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

struct ComputerscareComplexGeneratorViewMenu : MenuItem {
  ComputerscareComplexGenerator* generator = nullptr;

  Menu* createChildMenu() override {
    Menu* menu = new Menu;
    cpx::addPerspectivePentagonMenuItems(
        menu, generator ? &generator->pentagonSettings : nullptr);
    return menu;
  }

  void step() override {
    rightText = RIGHT_ARROW;
    MenuItem::step();
  }
};

struct ComputerscareComplexGeneratorControlsMenu : MenuItem {
  ComputerscareComplexGenerator* generator = nullptr;

  Menu* createChildMenu() override {
    Menu* menu = new Menu;
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Controls"));

    std::vector<int> modeParamIds;
    for (int i = 0; i < numComplexGeneratorKnobs; i++) {
      modeParamIds.push_back(ComputerscareComplexGenerator::LANE_VIEW_MODE + i);
    }
    modeParamIds.push_back(ComputerscareComplexGenerator::SCALE_VIEW_MODE);
    modeParamIds.push_back(ComputerscareComplexGenerator::OFFSET_VIEW_MODE);
    menu->addChild(construct<cpx::SetAllComplexControlViewModeItem>(
        &MenuItem::text, "set all to arrow",
        &cpx::SetAllComplexControlViewModeItem::module, generator,
        &cpx::SetAllComplexControlViewModeItem::modeParamIds, modeParamIds,
        &cpx::SetAllComplexControlViewModeItem::mode, 0));
    menu->addChild(construct<cpx::SetAllComplexControlViewModeItem>(
        &MenuItem::text, "set all to xy",
        &cpx::SetAllComplexControlViewModeItem::module, generator,
        &cpx::SetAllComplexControlViewModeItem::modeParamIds, modeParamIds,
        &cpx::SetAllComplexControlViewModeItem::mode, 1));
    menu->addChild(construct<cpx::SetAllComplexControlViewModeItem>(
        &MenuItem::text, "set all to rtheta",
        &cpx::SetAllComplexControlViewModeItem::module, generator,
        &cpx::SetAllComplexControlViewModeItem::modeParamIds, modeParamIds,
        &cpx::SetAllComplexControlViewModeItem::mode, 2));
    return menu;
  }

  void step() override {
    rightText = RIGHT_ARROW;
    MenuItem::step();
  }
};

struct ComputerscareComplexGeneratorWidget : ModuleWidget {
  ComputerscareComplexGeneratorWidget(ComputerscareComplexGenerator* module) {
    setModule(module);
    box.size = Vec(8 * 15, 380);

    constexpr float pentLeftX = 2.f;
    constexpr float pentRightX = 62.f;
    constexpr float pentTopY = 80.f;
    constexpr float pentRowSpacing = 33.f;
    Vec titlePos = Vec(2.f, -1.f);
    Vec titleSize = Vec(58.f, 31.f);
    Vec channelPos = Vec(64.f, -1.f);
    Vec channelSize = Vec(54.f, 31.f);
    Vec outputPos = Vec(2.f, 31.f);
    Vec outputSize = Vec(116.f, 48.f);
    Vec modeSize = Vec(56.f, 43.f);
    Vec laneSize = Vec(56.f, 36.f);

    addPerspectiveDisplay(titlePos, titleSize, module, 100, true, false);
    addPerspectiveDisplay(channelPos, channelSize, module, 101, true, false);
    addPerspectiveDisplay(outputPos, outputSize, module, 102, true, false);
    addPerspectiveDisplay(pentLeftX, pentTopY, modeSize, module,
                          ComputerscareComplexGenerator::SCALE_VAL_AB, true,
                          false, 0x74, 0x78, 0x0a);
    addPerspectiveDisplay(pentRightX, pentTopY, modeSize, module,
                          ComputerscareComplexGenerator::OFFSET_VAL_AB, true,
                          false, 0x74, 0x78, 0x0a);
    for (int i = 0; i < numComplexGeneratorKnobs; i++) {
      bool isRightColumn = i % 2 == 1;
      float xx = isRightColumn ? pentRightX : pentLeftX;
      float yy = pentTopY + pentRowSpacing * (1 + i / 2);
      addPerspectiveDisplay(xx, yy, laneSize, module, i + 2, true, false);
    }

    addPerspectiveDisplay(titlePos, titleSize, module, 100, false, true);
    addPerspectiveDisplay(channelPos, channelSize, module, 101, false, true);
    addPerspectiveDisplay(outputPos, outputSize, module, 102, false, true);
    addPerspectiveDisplay(pentLeftX, pentTopY, modeSize, module,
                          ComputerscareComplexGenerator::SCALE_VAL_AB, false,
                          true, 0x74, 0x78, 0x0a);
    addPerspectiveDisplay(pentRightX, pentTopY, modeSize, module,
                          ComputerscareComplexGenerator::OFFSET_VAL_AB, false,
                          true, 0x74, 0x78, 0x0a);
    for (int i = 0; i < numComplexGeneratorKnobs; i++) {
      bool isRightColumn = i % 2 == 1;
      float xx = isRightColumn ? pentRightX : pentLeftX;
      float yy = pentTopY + pentRowSpacing * (1 + i / 2);
      addPerspectiveDisplay(xx, yy, laneSize, module, i + 2, false, true);
    }

    SmallLetterDisplay* titleDisplay = new SmallLetterDisplay();
    titleDisplay->box = Rect(titlePos.plus(Vec(5.f, 2.f)), Vec(50.f, 19.f));
    titleDisplay->value = "complex\ngenerator";
    titleDisplay->fontSize = 10;
    titleDisplay->letterSpacing = 0.4f;
    titleDisplay->textAlign = NVG_ALIGN_LEFT;
    titleDisplay->breakRowWidth = 58.f;
    addChild(titleDisplay);

    channelWidget = new CompolyLaneCountWidget(
        Vec(74, 0), module, ComputerscareComplexGenerator::COMPOLY_CHANNELS,
        module ? &module->polyChannels : nullptr, false, 1.44f, 16.f);

    // addOutput(createOutput<PointingUpPentagonPort>(Vec(30, 22), module,
    // ComputerscareComplexGenerator::POLY_OUTPUT));

    addChild(channelWidget);

    cpx::CompolyPortsWidget* mainOutput = new cpx::CompolyPortsWidget(
        Vec(45, 43), module, ComputerscareComplexGenerator::COMPOLY_MAIN_OUT_A,
        ComputerscareComplexGenerator::MAIN_OUTPUT_MODE, 0.6);
    mainOutput->compolyLabelTransform->box.pos = Vec(21, 44);
    addChild(mainOutput);

    addModeControl("scale", pentLeftX, pentTopY, module,
                   ComputerscareComplexGenerator::SCALE_VAL_AB,
                   ComputerscareComplexGenerator::SCALE_POLAR,
                   ComputerscareComplexGenerator::SCALE_VIEW_MODE,
                   cpx::ComplexXYMaxMode::Radial, 3.f);
    addModeControl("offset", pentRightX, pentTopY, module,
                   ComputerscareComplexGenerator::OFFSET_VAL_AB,
                   ComputerscareComplexGenerator::OFFSET_POLAR,
                   ComputerscareComplexGenerator::OFFSET_VIEW_MODE,
                   cpx::ComplexXYMaxMode::Rectangular, 10.f);

    // addParam(createParam<NoRandomSmallKnob>(Vec(11, 54), module,
    // ComputerscareComplexGenerator::GLOBAL_SCALE));
    // addParam(createParam<NoRandomMediumSmallKnob>(Vec(32, 57), module,
    // ComputerscareComplexGenerator::GLOBAL_OFFSET));

    for (int i = 0; i < numComplexGeneratorKnobs; i++) {
      bool isRightColumn = i % 2 == 1;
      float xx = isRightColumn ? pentRightX : pentLeftX;
      float yy = pentTopY + pentRowSpacing * (1 + i / 2);
      addLabeledKnob(std::to_string(i + 1), xx, yy, module, i * 2);
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

  void addPerspectiveDisplay(float x, float y, Vec size,
                             ComputerscareComplexGenerator* module, int seed,
                             bool drawSides, bool drawFace, int faceShade = -1,
                             int sideShade = -1, int sideStep = 0x0c) {
    addPerspectiveDisplay(Vec(x, y), size, module, seed, drawSides, drawFace,
                          faceShade, sideShade, sideStep);
  }

  void addPerspectiveDisplay(Vec pos, Vec size,
                             ComputerscareComplexGenerator* module, int seed,
                             bool drawSides, bool drawFace, int faceShade = -1,
                             int sideShade = -1, int sideStep = 0x0c) {
    cpx::PerspectivePentagonWidget* pentagon =
        new cpx::PerspectivePentagonWidget(
            module ? &module->pentagonSettings : nullptr, seed);
    pentagon->box = Rect(pos, size);
    pentagon->setDrawParts(drawSides, drawFace);
    if (faceShade >= 0 && sideShade >= 0)
      pentagon->setBaseShades(faceShade, sideShade, sideStep);
    pentagon->appendContextMenuHandler = [module](Menu* menu) {
      menu->addChild(construct<ComputerscareComplexGeneratorViewMenu>(
          &MenuItem::text, "View",
          &ComputerscareComplexGeneratorViewMenu::generator, module));
      menu->addChild(construct<ComputerscareComplexGeneratorControlsMenu>(
          &MenuItem::text, "Controls",
          &ComputerscareComplexGeneratorControlsMenu::generator, module));
    };
    addChild(pentagon);
  }

  void addModeControl(std::string label, int x, int y,
                      ComputerscareComplexGenerator* module, int paramIndex,
                      int polarParamIndex, int modeParamId,
                      cpx::ComplexXYMaxMode arrowMaxMode,
                      float arrowMaxVoltage) {
    std::string controlLabel = label;
    if (!controlLabel.empty() && controlLabel[0] >= 'a' &&
        controlLabel[0] <= 'z')
      controlLabel[0] += 'A' - 'a';
    cpx::PerspectiveLabeledSwitchableComplexControl* control =
        new cpx::PerspectiveLabeledSwitchableComplexControl(
            module ? &module->pentagonSettings : nullptr, paramIndex, module,
            paramIndex, polarParamIndex, modeParamId, arrowMaxMode,
            arrowMaxVoltage, label, Vec(34.f, 26.f), Vec(17.f, 2.f),
            Vec(34.f, 10.f), -1, false, controlLabel,
            NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE, Vec(11.f, 7.f));
    control->box = Rect(Vec(x, y), Vec(56.f, 43.f));
    control->setDrawParts(false, false);
    control->setContextMenuEnabled(false);
    control->setContentFillsBox(true);
    control->setHoverHighlightEnabled(true);
    control->setBaseShades(0x74, 0x78, 0x0a);
    control->setArrowDrawingScale(0.76f);
    control->setArrowYOffset(-5.f);
    addChild(control);
  }

  void addLabeledKnob(std::string label, float x, float y,
                      ComputerscareComplexGenerator* module, int index) {
    /*	ParamWidget* pob =  createParam<DisableableSmoothKnob>(Vec(x, y),
       module, ComputerscareComplexGenerator::KNOB + index);

            DisableableSmoothKnob* fader =
       dynamic_cast<DisableableSmoothKnob*>(pob);

            fader->module = module;
            fader->channel = index;

            addParam(fader);*/

    float labelWidth = label.size() > 1 ? 16.f : 12.f;
    Vec labelRel = Vec(56.f - labelWidth - 5.f, 2.f);
    Vec controlRel = Vec(11.f, 5.f);
    Vec wrapperSize = Vec(56.f, 36.f);

    cpx::PerspectiveLabeledSwitchableComplexControl* control =
        new cpx::PerspectiveLabeledSwitchableComplexControl(
            module ? &module->pentagonSettings : nullptr, index / 2 + 2, module,
            ComputerscareComplexGenerator::COMPLEX_XY + index,
            ComputerscareComplexGenerator::LANE_POLAR + index,
            ComputerscareComplexGenerator::LANE_VIEW_MODE + index / 2,
            cpx::ComplexXYMaxMode::Rectangular, 10.f, label, Vec(34.f, 26.f),
            labelRel, Vec(labelWidth, 10.f), index / 2, false, "Lane " + label,
            NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE, controlRel);
    control->box = Rect(Vec(x, y), wrapperSize);
    control->setDrawParts(false, false);
    control->setContextMenuEnabled(false);
    control->setContentFillsBox(true);
    control->setHoverHighlightEnabled(true);
    control->setArrowDrawingScale(0.76f);
    control->setArrowYOffset(-5.f);
    control->complexControl->modeSwitch->fontSize = 8.f;
    addChild(control);
  }

  void appendContextMenu(Menu* menu) override {
    ComputerscareComplexGenerator* generator =
        dynamic_cast<ComputerscareComplexGenerator*>(module);
    menu->addChild(new MenuSeparator);
    menu->addChild(construct<ComputerscareComplexGeneratorViewMenu>(
        &MenuItem::text, "View",
        &ComputerscareComplexGeneratorViewMenu::generator, generator));
    menu->addChild(construct<ComputerscareComplexGeneratorControlsMenu>(
        &MenuItem::text, "Controls",
        &ComputerscareComplexGeneratorControlsMenu::generator, generator));
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
