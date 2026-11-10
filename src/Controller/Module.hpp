#pragma once
#include "../Computerscare.hpp"
#include "Backend.hpp"
namespace cc = computerscare::controller;

struct ComputerscareController : Module {
  enum ParamIds {
    ENUMS(MODE, cc::buttons),
    DEADZONE,
    THRESHOLD,
    SMOOTHING,
    PULSE_MS,
    CENTER,
    ENUMS(OFFSET, cc::buttons + cc::analogs),
    ENUMS(SCALE, cc::buttons + cc::analogs),
    PARAMS
  };
  enum InputIds { RUMBLE_LOW, RUMBLE_HIGH, INPUTS };
  enum OutputIds {
    ENUMS(BUTTON, cc::buttons),
    ENUMS(ANALOG, cc::analogs),
    ENUMS(BANK, 4),
    OUTPUTS
  };
  const uint32_t rumbleOwner = cc::nextRumbleOwner();
  unsigned rumbleDivider = 0;
  std::shared_ptr<cc::Service> service;
  std::vector<std::shared_ptr<cc::Connection>> keepAlive;
  std::atomic<cc::Connection*> connection{nullptr};
  std::string deviceKey;
  std::atomic<bool> centerRequested{false};
  const cc::Profile deviceFamily;
  std::atomic<int> profile;
  std::array<cc::ButtonSignal, cc::buttons> signals;
  std::array<float, cc::analogs> filtered{};
  std::array<float, 6> baseline{};
  std::array<uint32_t, 2> triggerEdges{};
  std::array<bool, 2> triggerDown{};
  cc::Connection* previous = nullptr;
  bool initialized = false, centerHeld = false;
  std::atomic<uint32_t> visibleAnalog{0}, visibleButtons{0};
  std::atomic<bool> online{false};
  std::atomic<uint32_t> activeButtons{0};
  explicit ComputerscareController(cc::Profile family = cc::DualSense)
      : deviceFamily(family), profile(family) {
    config(PARAMS, INPUTS, OUTPUTS, 0);
    configInput(RUMBLE_LOW, "Low-frequency rumble, 0–10 V");
    configInput(RUMBLE_HIGH, "High-frequency rumble, 0–10 V");
    for (int i = 0; i < cc::buttons; ++i) {
      configSwitch(MODE + i, 0, 2, 0,
                   std::string(cc::buttonName(i, deviceFamily)) + " output",
                   {"Gate", "Trigger", "Toggle"});
      configOutput(BUTTON + i, cc::buttonName(i, deviceFamily));
    }
    for (int i = 0; i < cc::analogs; ++i)
      configOutput(ANALOG + i, cc::analogName(i, deviceFamily));
    const char* banks[] = {"Buttons 1", "Buttons 2", "Controls",
                           "Touch and motion"};
    for (int i = 0; i < 4; ++i) configOutput(BANK + i, banks[i]);
    configParam(DEADZONE, 0, .4f, .08f, "Stick dead zone", "%", 0, 100);
    configParam(THRESHOLD, 0, 1, .5f, "Trigger threshold", "%", 0, 100);
    configParam(SMOOTHING, 0, 100, 5, "CV smoothing", " ms");
    configParam(PULSE_MS, 1, 1000, 1, "Trigger duration", " ms");
    configButton(CENTER, "Center motion");
    for (int i = 0; i < cc::buttons + cc::analogs; ++i) {
      std::string name = i < cc::buttons
                             ? cc::buttonName(i, deviceFamily)
                             : cc::analogName(i - cc::buttons, deviceFamily);
      configParam(OFFSET + i, -10, 10, 0, name + " offset", " V");
      configParam(SCALE + i, 0, 2, 1, name + " scale", "×");
    }
    for (auto* q : paramQuantities) q->randomizeEnabled = false;
  }
  ~ComputerscareController() override {
    if (previous) previous->rumble.stop(rumbleOwner);
  }
  bool accepts(cc::Profile p) const {
    return deviceFamily == cc::Xbox
               ? p == cc::Xbox
               : (p == cc::DualSense || p == cc::DualShock4);
  }
  void bind(std::shared_ptr<cc::Connection> c) {
    if (c && !accepts(c->profile)) c.reset();
    if (c &&
        std::find(keepAlive.begin(), keepAlive.end(), c) == keepAlive.end())
      keepAlive.push_back(c);
    connection.store(c.get(), std::memory_order_release);
    if (c) profile = c->profile;
  }
  void process(const ProcessArgs& args) override {
    cc::Connection* c = connection.load(std::memory_order_acquire);
    bool connected = c && c->connected.load(std::memory_order_acquire);
    if (!connected) {
      if (previous) previous->rumble.stop(rumbleOwner);
      for (auto& signal : signals) signal.reset();
      for (auto& output : outputs)
        for (int ch = 0; ch < output.getChannels(); ++ch)
          output.setVoltage(0, ch);
      filtered.fill(0);
      initialized = false;
      previous = nullptr;
      triggerDown.fill(false);
      online = false;
      activeButtons = 0;
      visibleAnalog = 0;
      visibleButtons = 0;
      return;
    }
    if (c != previous) {
      if (previous) previous->rumble.stop(rumbleOwner);
      rumbleDivider = 0;
      initialized = false;
      for (auto& signal : signals) signal.reset();
      baseline.fill(0);
      triggerDown.fill(false);
      previous = c;
    }
    if (rumbleDivider++ % 64 == 0) {
      if (inputs[RUMBLE_LOW].isConnected() || inputs[RUMBLE_HIGH].isConnected())
        c->rumble.request(rumbleOwner, inputs[RUMBLE_LOW].getNormalVoltage(0.f),
                          inputs[RUMBLE_HIGH].getNormalVoltage(0.f));
      else
        c->rumble.stop(rumbleOwner);
    }
    std::array<float, cc::analogs> cv{};
    uint32_t av = c->supportedAnalog.load(), bv = c->supportedButtons.load();
    if (deviceFamily == cc::Xbox) {
      av &= 255u;
      bv &= ~((1u << 15) | (1u << 17) | (1u << 18));
    }
    for (int i = 0; i < cc::analogs; ++i) cv[i] = c->values[i].load();
    cc::deadzone(cv[0], cv[1], params[DEADZONE].getValue());
    cc::deadzone(cv[2], cv[3], params[DEADZONE].getValue());
    for (int i = 0; i < 4; ++i) cv[i] *= 5;
    for (int i = 4; i < 12; ++i) cv[i] *= 10;
    bool center =
        params[CENTER].getValue() > .5f || centerRequested.exchange(false);
    if (center && !centerHeld)
      for (int i = 0; i < 6; ++i) baseline[i] = cv[12 + i];
    centerHeld = center;
    for (int i = 12; i < 15; ++i)
      cv[i] = (cv[i] - baseline[i - 12]) * 5.f / 3.14159265f;
    for (int i = 15; i < 18; ++i)
      cv[i] = (cv[i] - baseline[i - 12]) * 5.f / 19.6133f;
    float tau = params[SMOOTHING].getValue() * .001f;
    float amount = tau > 0 ? std::min(1.f, args.sampleTime / tau) : 1.f;
    for (int i = 0; i < cc::analogs; ++i) {
      if (i == 7)
        filtered[i] = 10;
      else
        filtered[i] =
            initialized ? filtered[i] + amount * (cv[i] - filtered[i]) : cv[i];
      outputs[ANALOG + i].setVoltage(
          av & (1u << i)
              ? cc::clamp(filtered[i], i < 4 || i >= 12 ? -10.f : 0.f, 10.f)
              : 0);
    }
    for (int t = 0; t < 2; ++t) {
      float value = c->values[4 + t].load();
      bool next =
          triggerDown[t]
              ? value > std::max(0.f, params[THRESHOLD].getValue() - .02f)
              : value > params[THRESHOLD].getValue();
      if (next && !triggerDown[t]) ++triggerEdges[t];
      triggerDown[t] = next;
    }
    uint32_t active = 0;
    for (int i = 0; i < cc::buttons; ++i) {
      bool down = i < 19 ? c->down[i].load() : triggerDown[i - 19];
      uint32_t edges = i < 19 ? c->presses[i].load() : triggerEdges[i - 19];
      float voltage = signals[i].process(
          (bv & (1u << i)) != 0, down, edges,
          int(std::round(params[MODE + i].getValue())), args.sampleTime,
          params[PULSE_MS].getValue() * .001f);
      outputs[BUTTON + i].setVoltage(voltage);
      if (voltage > 0 || (down && (bv & (1u << i)))) active |= 1u << i;
    }
    activeButtons = active;
    // Transform named outputs once; banks reuse the same final voltages.
    for (int i = 0; i < cc::buttons + cc::analogs; ++i) {
      bool supported =
          i < cc::buttons ? (bv & (1u << i)) : (av & (1u << (i - cc::buttons)));
      if (supported)
        outputs[i].setVoltage(outputs[i].getVoltage() *
                                  params[SCALE + i].getValue() +
                              params[OFFSET + i].getValue());
    }
    for (int bank = 0; bank < 4; ++bank) {
      int count = bank == 0 ? 16 : bank == 1 ? 5 : bank == 2 ? 8 : 10;
      outputs[BANK + bank].setChannels(count);
      for (int ch = 0; ch < count; ++ch)
        outputs[BANK + bank].setVoltage(
            outputs[bank < 2 ? BUTTON + cc::buttonBankControl(bank, ch)
                             : ANALOG + (bank == 2 ? ch : 8 + ch)]
                .getVoltage(),
            ch);
    }
    initialized = true;
    online = true;
    visibleAnalog = av;
    visibleButtons = bv;
  }
  void editParam(history::ComplexAction* group, int index, float value) {
    float old = params[index].getValue();
    if (old == value) return;
    auto* change = new history::ParamChange;
    change->moduleId = id;
    change->paramId = index;
    change->oldValue = old;
    change->newValue = value;
    params[index].setValue(value);
    group->push(change);
  }
  void setButtonModes(int mode) {
    auto* group = new history::ComplexAction;
    group->name = "Set all controller button modes";
    for (int i = 0; i < cc::buttons; ++i) editParam(group, MODE + i, mode);
    if (group->isEmpty())
      delete group;
    else
      APP->history->push(group);
  }
  void resetTransforms(int control = -1) {
    auto* group = new history::ComplexAction;
    group->name = "Reset controller offsets and scales";
    for (int i = 0; i < cc::buttons + cc::analogs; ++i)
      if (control < 0 || control == i) {
        editParam(group, OFFSET + i, 0);
        editParam(group, SCALE + i, 1);
      }
    if (group->isEmpty())
      delete group;
    else
      APP->history->push(group);
  }
  json_t* dataToJson() override {
    auto* j = json_object();
    json_object_set_new(j, "device", json_string(deviceKey.c_str()));
    json_object_set_new(j, "profile", json_integer(profile.load()));
    return j;
  }
  void dataFromJson(json_t* j) override {
    auto* key = json_object_get(j, "device");
    if (json_is_string(key)) deviceKey = json_string_value(key);
    int p = json_integer_value(json_object_get(j, "profile"));
    profile = accepts(cc::Profile(p)) ? p : int(deviceFamily);
  }
};
