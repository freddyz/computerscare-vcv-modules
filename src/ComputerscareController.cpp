#include "Computerscare.hpp"
#include "Controller/Backend.hpp"
#include "Phlooper/Block.hpp"
#include "Phlooper/Dropdown.hpp"
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
  std::atomic<int> profile{cc::DualSense};
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
  ComputerscareController() {
    config(PARAMS, INPUTS, OUTPUTS, 0);
    configInput(RUMBLE_LOW, "Low-frequency rumble, 0–10 V");
    configInput(RUMBLE_HIGH, "High-frequency rumble, 0–10 V");
    for (int i = 0; i < cc::buttons; ++i) {
      configSwitch(MODE + i, 0, 2, 0,
                   std::string(cc::buttonName(i, cc::DualSense)) + " output",
                   {"Gate", "Trigger", "Toggle"});
      configOutput(BUTTON + i, cc::buttonName(i, cc::DualSense));
    }
    for (int i = 0; i < cc::analogs; ++i)
      configOutput(ANALOG + i, cc::analogName(i, cc::DualSense));
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
                             ? cc::buttonName(i, cc::DualSense)
                             : cc::analogName(i - cc::buttons, cc::DualSense);
      configParam(OFFSET + i, -10, 10, 0, name + " offset", " V");
      configParam(SCALE + i, 0, 2, 1, name + " scale", "×");
    }
    for (auto* q : paramQuantities) q->randomizeEnabled = false;
  }
  ~ComputerscareController() override {
    if (previous) previous->rumble.stop(rumbleOwner);
  }
  void bind(const std::shared_ptr<cc::Connection>& c) {
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
    profile = p == cc::DualShock4 ? cc::DualShock4
              : p == cc::Xbox     ? cc::Xbox
                                  : cc::DualSense;
  }
};
static bool controllerPortSupported(PortWidget* port) {
  auto* m = dynamic_cast<ComputerscareController*>(port->module);
  if (!m) return true;
  return port->portId < ComputerscareController::ANALOG
             ? (m->visibleButtons.load() & (1u << port->portId)) != 0
             : (m->visibleAnalog.load() &
                (1u << (port->portId - ComputerscareController::ANALOG))) != 0;
}
struct ControllerAnalogJack : JackSmallPointingDown {
  void appendContextMenu(Menu* menu) override {
    auto* m = dynamic_cast<ComputerscareController*>(module);
    if (!m) return;
    menu->addChild(new MenuSeparator);
    menu->addChild(createMenuLabel(m->outputInfos[portId]->name));
    menu->addChild(createMenuItem("Reset offset and scale", "",
                                  [m, this]() { m->resetTransforms(portId); }));
    menu->addChild(new MenuParamSlider(
        m->paramQuantities[ComputerscareController::SCALE + portId]));
    menu->addChild(new MenuParamSlider(
        m->paramQuantities[ComputerscareController::OFFSET + portId]));
  }
  void draw(const DrawArgs& args) override {
    nvgSave(args.vg);
    if (!controllerPortSupported(this)) nvgGlobalAlpha(args.vg, .35f);
    JackSmallPointingDown::draw(args);
    nvgRestore(args.vg);
  }
};
struct ControllerButtonJack : ControllerAnalogJack {
  void appendContextMenu(Menu* menu) override {
    ControllerAnalogJack::appendContextMenu(menu);
    auto* m = dynamic_cast<ComputerscareController*>(module);
    if (!m) return;
    menu->addChild(new MenuSeparator);
    menu->addChild(createMenuLabel("Button output mode"));
    for (int mode = 0; mode < 3; ++mode)
      menu->addChild(createCheckMenuItem(
          mode == 0   ? "Gate"
          : mode == 1 ? "Trigger"
                      : "Toggle",
          "",
          [=]() {
            return int(m->params[ComputerscareController::MODE + portId]
                           .getValue()) == mode;
          },
          [=]() {
            auto* change = new history::ParamChange;
            change->moduleId = m->id;
            change->paramId = ComputerscareController::MODE + portId;
            change->name = "Controller button mode";
            change->oldValue = m->params[change->paramId].getValue();
            change->newValue = mode;
            m->params[change->paramId].setValue(mode);
            APP->history->push(change);
          }));
  }
};
struct ControllerDeviceButton : phlooper::WarpedDropdown {
  ComputerscareController* module = nullptr;
  std::vector<std::shared_ptr<cc::Connection>> available;
  WeakPtr<ui::MenuOverlay> active;
  std::string label() override {
    if (!module) return "PS5";
    return module->profile == cc::Xbox         ? "Xbox"
           : module->profile == cc::DualShock4 ? "PS4"
                                               : "PS5";
  }
  bool isPressed() override { return active.get() != nullptr; }
  void step() override {
    if (module && module->service) {
      available = module->service->connections();
      std::shared_ptr<cc::Connection> selected;
      for (auto& c : available)
        if (module->deviceKey.empty() || c->key == module->deviceKey) {
          selected = c;
          break;
        }
      module->bind(selected);
    }
    phlooper::WarpedDropdown::step();
  }
  void onButton(const event::Button& e) override {
    if (module && e.button == GLFW_MOUSE_BUTTON_LEFT &&
        e.action == GLFW_PRESS) {
      auto* menu = createMenu();
      active = menu->getAncestorOfType<ui::MenuOverlay>();
      menu->addChild(createCheckMenuItem(
          "Auto: first supported controller", "",
          [=]() { return module->deviceKey.empty(); },
          [=]() { module->deviceKey.clear(); }));
      for (auto& c : available)
        menu->addChild(createCheckMenuItem(
            c->name, "", [=]() { return module->deviceKey == c->key; },
            [=]() { module->deviceKey = c->key; }));
      if (module->service && module->service->status() != "Ready")
        menu->addChild(createMenuLabel(module->service->status()));
      if (available.empty())
        menu->addChild(
            createMenuLabel("Pair in your computer's Bluetooth settings."));
      e.consume(this);
    }
  }
};
struct ControllerCenterButton : phlooper::WarpedDropdown {
  ComputerscareController* module = nullptr;
  std::string label() override { return "Center"; }
  bool isPressed() override {
    return module &&
           module->params[ComputerscareController::CENTER].getValue() > .5f;
  }
  void onButton(const event::Button& e) override {
    if (module && e.button == GLFW_MOUSE_BUTTON_LEFT) {
      module->params[ComputerscareController::CENTER].setValue(
          e.action == GLFW_PRESS ? 1 : 0);
      e.consume(this);
    }
  }
  void onDragEnd(const event::DragEnd& e) override {
    if (module) module->params[ComputerscareController::CENTER].setValue(0);
  }
};
constexpr float controllerCompactWidth = 225;
constexpr float controllerCompactPitch = controllerCompactWidth / 4;
static Vec controllerCompactCenter(int i) {
  if (i < 21) {
    for (int ch = 0; ch < 16; ++ch)
      if (cc::buttonBankControl(0, ch) == i)
        return Vec(16 + (ch / 8) * controllerCompactPitch, 112 + (ch % 8) * 20);
    for (int ch = 0; ch < 5; ++ch)
      if (cc::buttonBankControl(1, ch) == i) {
        int col = ch == 4 ? 1 : ch % 2;
        int row = ch == 4 ? 2 : ch / 2;
        return Vec(16 + col * controllerCompactPitch, 323 + row * 19);
      }
  }
  if (i < 29) return Vec(16 + 2 * controllerCompactPitch, 112 + (i - 21) * 22);
  if (i < 33) return Vec(16 + 3 * controllerCompactPitch, 127 + (i - 29) * 21);
  if (i < 39) return Vec(16 + 3 * controllerCompactPitch, 213 + (i - 33) * 21);
  if (i == 39) return Vec(16, 277);
  if (i == 40) return Vec(16, 361);
  if (i == 41) return Vec(16 + 2 * controllerCompactPitch, 284);
  if (i == 42) return Vec(16 + 3 * controllerCompactPitch, 346);
  return Vec(16 + 2 * controllerCompactPitch, 330 + (i - 43) * 20);
}
struct ControllerCompactLabels : widget::TransparentWidget {
  ComputerscareController* module = nullptr;
  std::array<std::shared_ptr<window::Svg>, 8> icons;
  ControllerCompactLabels() {
    const char* paths[] = {"res/components/controller-icons/cross.svg",
                           "res/components/controller-icons/circle.svg",
                           "res/components/controller-icons/square.svg",
                           "res/components/controller-icons/triangle.svg",
                           "res/components/controller-icons/arrow-up.svg",
                           "res/components/controller-icons/arrow-down.svg",
                           "res/components/controller-icons/arrow-left.svg",
                           "res/components/controller-icons/arrow-right.svg"};
    for (int i = 0; i < 8; ++i)
      icons[i] = APP->window->loadSvg(asset::plugin(pluginInstance, paths[i]));
  }
  void drawLayer(const DrawArgs& args, int layer) override {
    if (layer != 1) return;
    auto font =
        APP->window->loadFont(asset::system("res/fonts/Nunito-Bold.ttf"));
    if (!font) return;
    static const char* labels[] = {
        "×",    "○",    "□",   "△",     "↑",     "↓",    "←",    "→",   "L1",
        "R1",   "L3",   "R3",  "Cr",    "Opt",   "PS",   "Pad",  "Mic", "T1",
        "T2",   "L2g",  "R2g", "LX",    "LY",    "RX",   "RY",   "L2",  "R2",
        "Batt", "Link", "T1X", "T1Y",   "T2X",   "T2Y",  "Gx",   "Gy",  "Gz",
        "Ax",   "Ay",   "Az",  "Poly1", "Poly2", "Poly", "Poly", "Low", "Hi"};
    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 10);
    nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
    nvgFillColor(args.vg, nvgRGB(45, 62, 52));
    nvgFontSize(args.vg, 10.5f);
    struct Heading {
      Vec position;
      const char* text;
    };
    static const Heading headings[] = {
        {{8, 97}, "Buttons 1"},  {{8, 311}, "Buttons 2"},
        {{118, 97}, "Controls"}, {{174, 97}, "Touch &"},
        {{174, 108}, "Motion"},  {{118, 317}, "Rumble in"}};
    for (const auto& heading : headings) {
      nvgFontSize(args.vg, heading.position.x == 174   ? 9.5f
                           : heading.position.y == 317 ? 8.5f
                                                       : 10.5f);
      nvgText(args.vg, heading.position.x, heading.position.y, heading.text,
              nullptr);
    }
    nvgFontSize(args.vg, 10);
    uint32_t active = module ? module->activeButtons.load() : 0;
    for (int i = 0; i < 45; ++i) {
      Vec p = controllerCompactCenter(i);
      bool on = i < cc::buttons && (active & (1u << i));
      bool supported =
          !module || i >= cc::buttons + cc::analogs ||
          (i < cc::buttons
               ? (module->visibleButtons.load() & (1u << i))
               : (module->visibleAnalog.load() & (1u << (i - cc::buttons))));
      if (on) {
        nvgBeginPath(args.vg);
        nvgRect(args.vg, p.x + 13.5f, p.y - 8, 23, 16);
        nvgFillColor(args.vg, COLOR_COMPUTERSCARE_GREEN);
        nvgFill(args.vg);
      }
      nvgFillColor(args.vg,
                   supported ? nvgRGB(20, 35, 28) : nvgRGBA(20, 35, 28, 90));
      if (i < 8 && icons[i]) {
        nvgSave(args.vg);
        nvgTranslate(args.vg, p.x + 15.5f, p.y - 7);
        if (!supported) nvgGlobalAlpha(args.vg, .35f);
        window::svgDraw(args.vg, icons[i]->handle);
        nvgRestore(args.vg);
      } else
        nvgText(args.vg, p.x + 15.5f, p.y + 3.5f, labels[i], nullptr);
    }
  }
};
struct ControllerAllModesButton : phlooper::WarpedDropdown {
  ComputerscareController* module = nullptr;
  WeakPtr<ui::MenuOverlay> active;
  std::string label() override {
    int mode =
        module ? int(module->params[ComputerscareController::MODE].getValue())
               : 0;
    if (module)
      for (int i = 1; i < cc::buttons; ++i)
        if (int(module->params[ComputerscareController::MODE + i].getValue()) !=
            mode)
          return "All: Mixed";
    return std::string("All: ") + (mode == 0   ? "Gate"
                                   : mode == 1 ? "Trigger"
                                               : "Toggle");
  }
  bool isPressed() override { return active.get() != nullptr; }
  void onButton(const event::Button& e) override {
    if (!module || e.button != GLFW_MOUSE_BUTTON_LEFT || e.action != GLFW_PRESS)
      return;
    auto* menu = createMenu();
    active = menu->getAncestorOfType<ui::MenuOverlay>();
    for (int mode = 0; mode < 3; ++mode)
      menu->addChild(
          createMenuItem(mode == 0   ? "Gate"
                         : mode == 1 ? "Trigger"
                                     : "Toggle",
                         "", [this, mode]() { module->setButtonModes(mode); }));
    e.consume(this);
  }
};
struct ComputerscareControllerWidget : ModuleWidget {
  int displayedProfile = -1;
  ComputerscareControllerWidget(ComputerscareController* m) {
    setModule(m);
    box.size = Vec(controllerCompactWidth, 380);
    auto* base = new BGPanel(nvgRGB(190, 190, 190));
    base->box.size = box.size;
    addChild(base);
    auto section = [&](Rect rect, int grey, unsigned seed) {
      auto* block = new phlooper::WarpedBlock;
      block->configure(rect, 1.5f, .8f, seed, nvgRGB(grey, grey, grey));
      addChild(block);
    };
    section(Rect(Vec(3, 84), Vec(109, 210)), 215, 1101);
    section(Rect(Vec(3, 299), Vec(109, 77)), 215, 1102);
    section(Rect(Vec(116, 84), Vec(51, 215)), 202, 1103);
    section(Rect(Vec(172, 84), Vec(50, 280)), 196, 1104);
    section(Rect(Vec(116, 305), Vec(51, 59)), 224, 1105);
    auto* header = new phlooper::WarpedBlock;
    header->configure(Rect(Vec(4, 3), Vec(controllerCompactWidth - 8, 21)), 2,
                      .8f, 981, nvgRGB(205, 205, 205));
    addChild(header);
    auto* art = new ComputerscareSVGPanel;
    art->setBackground(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/panels/ComputerscareControllerPanel.svg")));
    addChild(art);
    auto* device = new ControllerDeviceButton;
    device->module = m;
    device->box.pos = Vec(6, 29);
    device->configure(52, 18, 2, 1, .6f, 986);
    device->textSize = 11;
    addChild(device);
    auto* all = new ControllerAllModesButton;
    all->module = m;
    all->box.pos = Vec(68, 29);
    all->configure(82, 18, 2, 1, .6f, 992);
    all->textSize = 10;
    addChild(all);
    auto* center = new ControllerCenterButton;
    center->module = m;
    center->box.pos = Vec(159, 29);
    center->configure(45, 18, 2, 1, .6f, 987);
    center->textSize = 10;
    center->faceColor = COLOR_COMPUTERSCARE_GREEN;
    addChild(center);
    for (int i = 0; i < 3; ++i)
      addParam(createParamCentered<SmallKnob>(
          Vec(32 + i * 73, 72), m, ComputerscareController::DEADZONE + i));
    for (int i = 0; i < ComputerscareController::OUTPUTS; ++i) {
      Vec p = controllerCompactCenter(i);
      if (i < cc::buttons)
        addOutput(createOutputCentered<ControllerButtonJack>(p, m, i));
      else if (i < cc::buttons + cc::analogs)
        addOutput(createOutputCentered<ControllerAnalogJack>(p, m, i));
      else
        addOutput(createOutputCentered<JackSmallPointingDown>(p, m, i));
    }
    for (int i = 0; i < ComputerscareController::INPUTS; ++i)
      addInput(createInputCentered<JackSmallPointingUp>(
          controllerCompactCenter(ComputerscareController::OUTPUTS + i), m, i));
    auto* labels = new ControllerCompactLabels;
    labels->module = m;
    labels->box.size = box.size;
    addChild(labels);
    if (m && !std::getenv("CONTROLLER_PREVIEW"))
      m->service = cc::service(pluginInstance->path);
  }
  void step() override {
    auto* m = dynamic_cast<ComputerscareController*>(module);
    cc::Profile profile = m ? cc::Profile(m->profile.load()) : cc::DualSense;
    if (m && displayedProfile != profile) {
      displayedProfile = profile;
      for (int i = 0; i < cc::buttons; ++i) {
        m->outputInfos[i]->name = cc::buttonName(i, profile);
        m->paramQuantities[ComputerscareController::MODE + i]->name =
            std::string(cc::buttonName(i, profile)) + " output";
      }
      for (int i = 0; i < cc::analogs; ++i)
        m->outputInfos[ComputerscareController::ANALOG + i]->name =
            cc::analogName(i, profile);
      for (int i = 0; i < cc::buttons + cc::analogs; ++i) {
        std::string name = m->outputInfos[i]->name;
        m->paramQuantities[ComputerscareController::SCALE + i]->name =
            name + " scale";
        m->paramQuantities[ComputerscareController::OFFSET + i]->name =
            name + " offset";
      }
    }
    ModuleWidget::step();
  }
  void appendContextMenu(Menu* menu) override {
    auto* m = dynamic_cast<ComputerscareController*>(module);
    if (!m) return;
    menu->addChild(new MenuSeparator);
    menu->addChild(createMenuItem("Reset all offsets and scales", "",
                                  [m]() { m->resetTransforms(); }));
    menu->addChild(createMenuItem("Center motion", "",
                                  [m]() { m->centerRequested = true; }));
    for (int i = 0; i < 3; ++i)
      menu->addChild(new MenuParamSlider(
          m->paramQuantities[ComputerscareController::DEADZONE + i]));
    menu->addChild(createMenuLabel(
        "Right-click a button jack for Gate / Trigger / Toggle."));
    menu->addChild(new MenuParamSlider(
        m->paramQuantities[ComputerscareController::PULSE_MS]));
  }
};
Model* modelComputerscareController =
    createModel<ComputerscareController, ComputerscareControllerWidget>(
        "computerscare-controller");
