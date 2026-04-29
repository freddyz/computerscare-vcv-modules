#pragma once

using namespace rack;

struct AutoParamQuantity : ParamQuantity {
  std::string getDisplayValueString() override {
    std::string disp = Quantity::getDisplayValueString();
    return disp == "0" ? "Auto" : disp;
  }
};

struct ComputerscarePolyModule : Module {
  int polyChannels = 16;
  int polyChannelsKnobSetting = 0;
  int counterPeriod = 64;
  int counter = counterPeriod + 1;

  virtual void checkCounter() {
    counter++;
    if (counter > counterPeriod) {
      checkPoly();
      counter = 0;
    }
  }

  virtual void checkPoly() {};
};
struct TinyChannelsSnapKnob : ComputerscareRoundKnob {
  std::shared_ptr<Svg> manualChannelsSetSvg = APP->window->loadSvg(
      asset::plugin(pluginInstance,
                    "res/components/computerscare-channels-empty-knob.svg"));
  std::shared_ptr<Svg> autoChannelsSvg = APP->window->loadSvg(asset::plugin(
      pluginInstance,
      "res/components/computerscare-channels-empty-knob-auto-mode.svg"));
  int prevSetting = -1;
  int paramId = -1;

  ComputerscarePolyModule* module;

  TinyChannelsSnapKnob() {
    setSvg(APP->window->loadSvg(
        asset::plugin(pluginInstance,
                      "res/components/computerscare-channels-empty-knob.svg")));
    shadow->opacity = 0.f;
    snap = true;
    ComputerscareRoundKnob();
  }
  void draw(const DrawArgs& args) override {
    if (module) {
      int currentSetting = module->params[paramId].getValue();
      ;
      if (currentSetting != prevSetting) {
        setSvg(currentSetting == 0 ? autoChannelsSvg : manualChannelsSetSvg);
        prevSetting = currentSetting;
      }
    } else {
    }
    ComputerscareRoundKnob::draw(args);
  }
};

struct TinyCompolyLanesSnapKnob : ComputerscareRoundKnob {
  std::shared_ptr<Svg> manualChannelsSetSvg =
      APP->window->loadSvg(asset::plugin(
          pluginInstance, "res/components/compoly-lane-count-empty.svg"));
  std::shared_ptr<Svg> autoChannelsSvg = APP->window->loadSvg(asset::plugin(
      pluginInstance, "res/components/compoly-lane-count-auto.svg"));
  int prevSetting = -1;
  int paramId = -1;
  bool allowAuto = true;
  bool hasDoubleClickResetValue = false;
  float doubleClickResetValue = 0.f;

  ComputerscarePolyModule* module;

  TinyCompolyLanesSnapKnob() {
    setSvg(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/components/compoly-lane-count-empty.svg")));
    shadow->opacity = 0.f;
    snap = true;
    ComputerscareRoundKnob();
  }
  void draw(const DrawArgs& args) override {
    if (module) {
      int currentSetting = module->params[paramId].getValue();
      if (currentSetting != prevSetting) {
        setSvg(allowAuto && currentSetting == 0 ? autoChannelsSvg
                                                : manualChannelsSetSvg);
        prevSetting = currentSetting;
      }
    }
    ComputerscareRoundKnob::draw(args);
  }

  void onDoubleClick(const DoubleClickEvent& e) override {
    if (!hasDoubleClickResetValue) {
      ComputerscareRoundKnob::onDoubleClick(e);
      return;
    }

    engine::ParamQuantity* pq = getParamQuantity();
    if (!pq || !pq->isBounded()) return;

    float oldValue = pq->getValue();
    float newValue = std::max(
        pq->getMinValue(), std::min(pq->getMaxValue(), doubleClickResetValue));
    pq->setValue(newValue);

    if (module && oldValue != newValue) {
      history::ParamChange* h = new history::ParamChange;
      h->name = "Reset parameter";
      h->moduleId = module->id;
      h->paramId = paramId;
      h->oldValue = oldValue;
      h->newValue = newValue;
      APP->history->push(h);
    }
  }
};

struct PolyChannelsDisplay : SmallLetterDisplay {
  ComputerscarePolyModule* module;
  int* channelCount = NULL;
  bool controlled = false;
  int prevChannels = -1;
  int paramId = -1;

  PolyChannelsDisplay(math::Vec pos) {
    box.pos = pos;
    fontSize = 14;
    letterSpacing = 1.f;
    textAlign = 18;
    textColor = BLACK;
    breakRowWidth = 20;
    SmallLetterDisplay();
  };
  void draw(const DrawArgs& args) {
    if (module) {
      int newChannels = channelCount ? *channelCount : module->polyChannels;
      if (newChannels != prevChannels) {
        std::string str = std::to_string(newChannels);
        value = str;
        prevChannels = newChannels;
      }

    } else {
      value = std::to_string((random::u32() % 16) + 1);
    }
    SmallLetterDisplay::draw(args);
  }
};
struct PolyOutputChannelsWidget : Widget {
  ComputerscarePolyModule* module;
  PolyChannelsDisplay* channelCountDisplay;
  TinyChannelsSnapKnob* channelsKnob;
  PolyOutputChannelsWidget(math::Vec pos, ComputerscarePolyModule* mod,
                           int paramId, int* channelCount = NULL) {
    module = mod;

    channelsKnob =
        createParam<TinyChannelsSnapKnob>(pos.plus(Vec(7, 3)), module, paramId);
    channelsKnob->module = module;
    channelsKnob->paramId = paramId;

    channelCountDisplay = new PolyChannelsDisplay(pos);

    channelCountDisplay->module = module;
    channelCountDisplay->channelCount = channelCount;

    addChild(channelsKnob);
    addChild(channelCountDisplay);
  }
};

struct CompolyLaneCountWidget : Widget {
  ComputerscarePolyModule* module;
  PolyChannelsDisplay* laneCountDisplay;
  TinyCompolyLanesSnapKnob* lanesKnob;
  TransformWidget* lanesKnobTransform;
  CompolyLaneCountWidget(math::Vec pos, ComputerscarePolyModule* mod,
                         int paramId, int* laneCount = NULL,
                         bool allowAuto = true, float knobScale = 1.2f,
                         float doubleClickResetValue = -1.f) {
    module = mod;

    lanesKnobTransform = new TransformWidget();
    lanesKnobTransform->box.pos = pos.plus(Vec(5, 1));
    lanesKnobTransform->scale(knobScale);

    lanesKnob =
        createParam<TinyCompolyLanesSnapKnob>(Vec(0, 0), module, paramId);
    lanesKnob->module = module;
    lanesKnob->paramId = paramId;
    lanesKnob->allowAuto = allowAuto;
    lanesKnob->hasDoubleClickResetValue = doubleClickResetValue >= 0.f;
    lanesKnob->doubleClickResetValue = doubleClickResetValue;
    lanesKnobTransform->addChild(lanesKnob);

    constexpr float knobSize = 18.f;
    Vec knobCenter = lanesKnobTransform->box.pos.plus(
        Vec(knobSize, knobSize).mult(0.5f * knobScale));
    Vec displayTextCenter = Vec(16.f, 12.f);
    laneCountDisplay =
        new PolyChannelsDisplay(knobCenter.minus(displayTextCenter));
    laneCountDisplay->module = module;
    laneCountDisplay->channelCount = laneCount;

    addChild(lanesKnobTransform);
    addChild(laneCountDisplay);
  }
};
