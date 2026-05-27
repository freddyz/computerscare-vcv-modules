#include "Computerscare.hpp"
#include "ComputerscarePolyModule.hpp"

struct ComputerscareKnolyPobs;

const int numKnobs = 16;
const int numToggles = 16;

struct ComputerscareKnolyPobs : ComputerscarePolyModule {
  ComputerscareSVGPanel* panelRef;
  bool bipolarMainKnobs = false;
  int mainKnobRangeRevision = 0;

  enum ParamIds {
    KNOB,
    TOGGLES = KNOB + numKnobs,
    POLY_CHANNELS = TOGGLES + numToggles,
    GLOBAL_SCALE,
    GLOBAL_OFFSET,
    NUM_PARAMS

  };
  enum InputIds { CHANNEL_INPUT, NUM_INPUTS };
  enum OutputIds { POLY_OUTPUT, NUM_OUTPUTS };
  enum LightIds { NUM_LIGHTS };

  ComputerscareKnolyPobs() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    for (int i = 0; i < numKnobs; i++) {
      configParam(KNOB + i, 0.f, 10.f, 0.f, "Channel " + std::to_string(i + 1));
    }
    configSwitch(POLY_CHANNELS, 1.f, 16.f, 16.f, "Poly Channels",
                 polyChannelLabels(false));
    configParam(GLOBAL_SCALE, -2.f, 2.f, 1.f, "Scale");
    configParam(GLOBAL_OFFSET, -10.f, 10.f, 0.f, "Offset", " volts");

    getParamQuantity(POLY_CHANNELS)->randomizeEnabled = false;
    getParamQuantity(POLY_CHANNELS)->resetEnabled = false;
    getParamQuantity(GLOBAL_SCALE)->randomizeEnabled = false;
    getParamQuantity(GLOBAL_OFFSET)->randomizeEnabled = false;

    configOutput(POLY_OUTPUT, "Main");
  }

  void setMainKnobRange(bool bipolar) {
    bool changed = bipolarMainKnobs != bipolar;
    bipolarMainKnobs = bipolar;
    for (int i = 0; i < numKnobs; i++) {
      engine::ParamQuantity* pq = getParamQuantity(KNOB + i);
      if (!pq) continue;
      pq->minValue = bipolar ? -10.f : 0.f;
      pq->maxValue = 10.f;
      pq->defaultValue = 0.f;
      pq->setValue(math::clamp(pq->getValue(), pq->minValue, pq->maxValue));
    }
    if (changed) {
      mainKnobRangeRevision++;
    }
  }

  json_t* dataToJson() override {
    json_t* rootJ = json_object();
    json_object_set_new(rootJ, "bipolarMainKnobs",
                        json_boolean(bipolarMainKnobs));
    return rootJ;
  }

  void dataFromJson(json_t* rootJ) override {
    json_t* bipolarMainKnobsJ = json_object_get(rootJ, "bipolarMainKnobs");
    setMainKnobRange(bipolarMainKnobsJ &&
                     json_boolean_value(bipolarMainKnobsJ));
  }

  void process(const ProcessArgs& args) override {
    ComputerscarePolyModule::checkCounter();
    float trim = params[GLOBAL_SCALE].getValue();
    float offset = params[GLOBAL_OFFSET].getValue();
    for (int i = 0; i < polyChannels; i++) {
      outputs[POLY_OUTPUT].setVoltage(
          params[KNOB + i].getValue() * trim + offset, i);
    }
  }
  void checkPoly() override {
    polyChannels = params[POLY_CHANNELS].getValue();
    if (polyChannels == 0) {
      polyChannels = 16;
      params[POLY_CHANNELS].setValue(16);
    }
    outputs[POLY_OUTPUT].setChannels(polyChannels);
  }
};

struct NoRandomSmallKnob : SmallKnob {
  NoRandomSmallKnob() { SmallKnob(); };
};
struct NoRandomMediumSmallKnob : ComputerscareRoundKnob {
  std::shared_ptr<Svg> enabledSvg = APP->window->loadSvg(asset::plugin(
      pluginInstance, "res/components/computerscare-medium-small-knob.svg"));

  NoRandomMediumSmallKnob() {
    setSvg(enabledSvg);
    ComputerscareRoundKnob();
  };
};

struct DisableableSmoothKnob : ComputerscareRoundKnob {
  std::shared_ptr<Svg> enabledSvg = APP->window->loadSvg(asset::plugin(
      pluginInstance, "res/components/computerscare-medium-small-knob.svg"));
  std::shared_ptr<Svg> disabledSvg = APP->window->loadSvg(asset::plugin(
      pluginInstance,
      "res/components/computerscare-medium-small-knob-disabled.svg"));

  int channel = 0;
  bool disabled = false;
  int mainKnobRangeRevision = -1;
  ComputerscarePolyModule* module;

  DisableableSmoothKnob() {
    setSvg(enabledSvg);
    shadow->box.size = math::Vec(0, 0);
    shadow->opacity = 0.f;
  }
  void step() override {
    if (module) {
      ComputerscareKnolyPobs* pobs =
          dynamic_cast<ComputerscareKnolyPobs*>(module);
      if (pobs && mainKnobRangeRevision != pobs->mainKnobRangeRevision) {
        event::Change eChange;
        onChange(eChange);
        mainKnobRangeRevision = pobs->mainKnobRangeRevision;
      }

      bool candidate = channel > module->polyChannels - 1;
      if (disabled != candidate) {
        setSvg(candidate ? disabledSvg : enabledSvg);
        onChange(*(new event::Change()));
        fb->dirty = true;
        disabled = candidate;
      }
    } else {
    }
    ComputerscareRoundKnob::step();
  }
};

struct ComputerscareKnolyPobsWidget : ModuleWidget {
  ComputerscareKnolyPobsWidget(ComputerscareKnolyPobs* module) {
    setModule(module);
    // setPanel(APP->window->loadSvg(asset::plugin(pluginInstance,
    // "res/panels/ComputerscareKnolyPobsPanel.svg")));
    box.size = Vec(4 * 15, 380);
    {
      ComputerscareSVGPanel* panel = new ComputerscareSVGPanel();
      panel->box.size = box.size;
      panel->setBackground(APP->window->loadSvg(asset::plugin(
          pluginInstance, "res/panels/ComputerscareKnolyPobsPanel.svg")));
      addChild(panel);
    }
    channelWidget = new PolyOutputChannelsWidget(
        Vec(1, 24), module, ComputerscareKnolyPobs::POLY_CHANNELS);
    addOutput(createOutput<PointingUpPentagonPort>(
        Vec(30, 22), module, ComputerscareKnolyPobs::POLY_OUTPUT));

    addChild(channelWidget);

    addParam(createParam<NoRandomSmallKnob>(
        Vec(11, 54), module, ComputerscareKnolyPobs::GLOBAL_SCALE));
    addParam(createParam<NoRandomMediumSmallKnob>(
        Vec(32, 57), module, ComputerscareKnolyPobs::GLOBAL_OFFSET));

    float xx;
    float yy;
    float yInitial = 86;
    float ySpacing = 34;
    float yRightColumnOffset = 14.3 / 8;
    for (int i = 0; i < numKnobs; i++) {
      xx = 1.4f + 24.3 * (i - i % 8) / 8;
      yy = yInitial + ySpacing * (i % 8) + yRightColumnOffset * (i - i % 8);
      addLabeledKnob(std::to_string(i + 1), xx, yy, module, i,
                     (i - i % 8) * 1.2 - 3 + (i == 8 ? 5 : 0), 2);
    }
  }
  void appendContextMenu(Menu* menu) override {
    ComputerscareKnolyPobs* module =
        dynamic_cast<ComputerscareKnolyPobs*>(this->module);
    if (!module) return;

    struct MainKnobRangeItem : MenuItem {
      ComputerscareKnolyPobs* module;
      bool bipolar;
      void onAction(const event::Action& e) override {
        module->setMainKnobRange(bipolar);
      }
      void step() override {
        rightText = CHECKMARK(module->bipolarMainKnobs == bipolar);
        MenuItem::step();
      }
    };

    menu->addChild(new MenuSeparator());
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Main Knob Range"));
    menu->addChild(construct<MainKnobRangeItem>(
        &MenuItem::text, "Unipolar", &MainKnobRangeItem::module, module,
        &MainKnobRangeItem::bipolar, false));
    menu->addChild(construct<MainKnobRangeItem>(
        &MenuItem::text, "Bipolar", &MainKnobRangeItem::module, module,
        &MainKnobRangeItem::bipolar, true));
  }
  void addLabeledKnob(std::string label, int x, int y,
                      ComputerscareKnolyPobs* module, int index, float labelDx,
                      float labelDy) {
    smallLetterDisplay = new SmallLetterDisplay();
    smallLetterDisplay->box.size = Vec(5, 10);
    smallLetterDisplay->fontSize = 18;
    smallLetterDisplay->value = label;
    smallLetterDisplay->textAlign = 1;

    ParamWidget* pob = createParam<DisableableSmoothKnob>(
        Vec(x, y), module, ComputerscareKnolyPobs::KNOB + index);

    DisableableSmoothKnob* fader = dynamic_cast<DisableableSmoothKnob*>(pob);

    fader->module = module;
    fader->channel = index;

    addParam(fader);

    smallLetterDisplay->box.pos = Vec(x + labelDx, y - 12 + labelDy);

    addChild(smallLetterDisplay);
  }
  PolyOutputChannelsWidget* channelWidget;
  PolyChannelsDisplay* channelDisplay;
  DisableableSmoothKnob* fader;
  SmallLetterDisplay* smallLetterDisplay;
};

Model* modelComputerscareKnolyPobs =
    createModel<ComputerscareKnolyPobs, ComputerscareKnolyPobsWidget>(
        "computerscare-knolypobs");
