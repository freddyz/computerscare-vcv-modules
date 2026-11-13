#pragma once
#include "../Phlooper/Block.hpp"
#include "../Phlooper/Dropdown.hpp"
#include "Module.hpp"

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
  cc::Profile family = cc::DualSense;
  std::vector<std::shared_ptr<cc::Connection>> available;
  WeakPtr<ui::MenuOverlay> active;
  std::string label() override {
    if (!module) return family == cc::Xbox ? "Xbox" : "PS5";
    return module->profile == cc::Xbox         ? "Xbox"
           : module->profile == cc::DualShock4 ? "PS4"
                                               : "PS5";
  }
  bool isPressed() override { return active.get() != nullptr; }
  void step() override {
    if (module && module->service) {
      available = module->service->connections();
      available.erase(
          std::remove_if(available.begin(), available.end(),
                         [this](const std::shared_ptr<cc::Connection>& c) {
                           return !module->accepts(c->profile);
                         }),
          available.end());
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
          "Auto: first matching controller", "",
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
static Vec controllerCompactCenter(int i, cc::Profile family = cc::DualSense) {
  if (i < 21) {
    for (int ch = 0; ch < 16; ++ch)
      if (cc::buttonBankControl(0, ch) == i)
        return Vec(16 + (ch / 8) * controllerCompactPitch, 112 + (ch % 8) * 18);
    if (family == cc::Xbox) {
      const int other[] = {16, 12, 13, 14};
      for (int n = 0; n < 4; ++n)
        if (other[n] == i)
          return Vec(16 + (n % 2) * controllerCompactPitch, 313 + (n / 2) * 18);
    }
    for (int ch = 0; ch < 5; ++ch)
      if (cc::buttonBankControl(1, ch) == i) {
        int col = ch == 4 ? 0 : ch % 2;
        int row = ch == 4 ? 2 : ch / 2;
        return Vec(16 + col * controllerCompactPitch,
                   row == 2 ? 350 : 313 + row * 18);
      }
  }
  if (i < 29) {
    int row = i - 21;
    if (row == 6)
      row = 7;
    else if (row == 7)
      row = 6;
    return Vec(16 + 2 * controllerCompactPitch, 112 + row * 18);
  }
  if (i < 33) return Vec(16 + 3 * controllerCompactPitch, 127 + (i - 29) * 21);
  if (i < 39) return Vec(16 + 3 * controllerCompactPitch, 213 + (i - 33) * 17);
  if (i == 39) return Vec(68, 265);
  if (i == 40) return Vec(68, 360);
  if (i == 41) return Vec(141.5f, 280);
  if (i == 42) return Vec(197, 343);
  return Vec(16 + 2 * controllerCompactPitch, 330 + (i - 43) * 20);
}
struct ControllerCompactLabels : widget::TransparentWidget {
  ComputerscareController* module = nullptr;
  std::array<std::shared_ptr<window::Svg>, 8> icons;
  cc::Profile family;
  explicit ControllerCompactLabels(cc::Profile f = cc::DualSense) : family(f) {
    const char* paths[] = {"res/components/controller-icons/cross.svg",
                           "res/components/controller-icons/circle.svg",
                           "res/components/controller-icons/square.svg",
                           "res/components/controller-icons/triangle.svg",
                           "res/components/controller-icons/arrow-up.svg",
                           "res/components/controller-icons/arrow-down.svg",
                           "res/components/controller-icons/arrow-left.svg",
                           "res/components/controller-icons/arrow-right.svg"};
    const char* xboxPaths[] = {"res/components/controller-icons/button-a.svg",
                               "res/components/controller-icons/button-b.svg",
                               "res/components/controller-icons/button-x.svg",
                               "res/components/controller-icons/button-y.svg"};
    for (int i = 0; i < 4; ++i)
      if (family == cc::Xbox) paths[i] = xboxPaths[i];
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
    const Heading headings[] = {
        {{8, 97}, "Buttons 1"},  {{8, 300}, "Buttons 2"},
        {{118, 97}, "Controls"}, {{174, 97}, "Touch &"},
        {{174, 108}, "Motion"},  {{118, 317}, "Rumble in"}};
    for (const auto& heading : headings) {
      if (family == cc::Xbox && heading.position.x == 174) continue;
      nvgFontSize(args.vg, heading.position.x == 174   ? 9.5f
                           : heading.position.y == 317 ? 8.5f
                                                       : 10.5f);
      nvgText(args.vg, heading.position.x, heading.position.y, heading.text,
              nullptr);
    }
    nvgFontSize(args.vg, 10);
    uint32_t active = module ? module->activeButtons.load() : 0;
    for (int i = 0; i < 45; ++i) {
      if (family == cc::Xbox &&
          ((i == 15 || i == 17 || i == 18) || (i >= 29 && i <= 38) || i == 42))
        continue;
      Vec p = controllerCompactCenter(i, family);
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
      if (i >= 39 && i <= 42) {
        if (i == 39 || i == 40) {
          nvgFontSize(args.vg, 9);
          nvgText(args.vg, p.x + 14, p.y + 3, labels[i], nullptr);
        } else {
          nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
          nvgText(args.vg, p.x, p.y - 15, labels[i], nullptr);
          nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
        }
        continue;
      }
      const char* text = labels[i];
      if (family == cc::Xbox) {
        switch (i) {
          case 8:
            text = "LB";
            break;
          case 9:
            text = "RB";
            break;
          case 10:
            text = "LS";
            break;
          case 11:
            text = "RS";
            break;
          case 12:
            text = "View";
            break;
          case 13:
            text = "Menu";
            break;
          case 14:
            text = "Xbox";
            break;
          case 16:
            text = "Share";
            break;
          case 19:
            text = "LTg";
            break;
          case 20:
            text = "RTg";
            break;
          case 25:
            text = "LT";
            break;
          case 26:
            text = "RT";
            break;
        }
      }
      nvgFontSize(args.vg, family == cc::Xbox &&
                                   (i == 12 || i == 13 || i == 14 || i == 16)
                               ? 9
                               : 10);
      if (i < 8 && icons[i]) {
        nvgSave(args.vg);
        nvgTranslate(args.vg, p.x + 15.5f, p.y - 7);
        if (!supported) nvgGlobalAlpha(args.vg, .35f);
        window::svgDraw(args.vg, icons[i]->handle);
        nvgRestore(args.vg);
      } else
        nvgText(args.vg, p.x + 15.5f, p.y + 3.5f, text, nullptr);
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
  const cc::Profile family;
  explicit ComputerscareControllerWidget(ComputerscareController* m,
                                         cc::Profile f = cc::DualSense)
      : family(m ? m->deviceFamily : f) {
    setModule(m);
    box.size = Vec(family == cc::Xbox ? 180 : controllerCompactWidth, 380);
    auto* base = new BGPanel(nvgRGB(190, 190, 190));
    base->box.size = box.size;
    addChild(base);
    auto section = [&](Rect rect, int grey, unsigned seed) {
      auto* block = new phlooper::WarpedBlock;
      block->configure(rect, (seed >= 1200 ? 1.6f : 2.2f) + .18f * (seed % 4),
                       1.05f + .12f * (seed % 3), seed,
                       nvgRGB(grey, grey, grey));
      addChild(block);
    };
    section(Rect(Vec(3, 84), Vec(109, 200)), 215, 1101);
    section(Rect(Vec(3, 288), Vec(109, 91)), 215, 1102);

    section(Rect(Vec(116, 84), Vec(51, 215)), 202, 1103);
    if (family != cc::Xbox)
      section(Rect(Vec(172, 84), Vec(50, 280)), 196, 1104);
    section(Rect(Vec(116, 305), Vec(51, 59)), 224, 1105);
    section(Rect(Vec(55, 251), Vec(54, 29)), 187, 1201);
    section(Rect(Vec(55, 346), Vec(54, 29)), 187, 1202);
    section(Rect(Vec(120, 252), Vec(43, 42)), 181, 1203);
    if (family != cc::Xbox)
      section(Rect(Vec(176, 315), Vec(42, 44)), 177, 1204);
    auto* header = new phlooper::WarpedBlock;
    header->configure(Rect(Vec(4, 3), Vec(box.size.x - 8, 21)), 2, .8f, 981,
                      nvgRGB(205, 205, 205));
    addChild(header);
    auto* art = new ComputerscareSVGPanel;
    art->setBackground(APP->window->loadSvg(asset::plugin(
        pluginInstance, family == cc::Xbox
                            ? "res/panels/ComputerscareControllerXboxPanel.svg"
                            : "res/panels/ComputerscareControllerPanel.svg")));
    addChild(art);
    auto* device = new ControllerDeviceButton;
    device->module = m;
    device->family = family;
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
    if (family != cc::Xbox)
      addChild(center);
    else
      delete center;
    for (int i = 0; i < 3; ++i)
      addParam(createParamCentered<SmallKnob>(
          Vec(32 + i * (family == cc::Xbox ? 58 : 73), 72), m,
          ComputerscareController::DEADZONE + i));
    for (int i = 0; i < ComputerscareController::OUTPUTS; ++i) {
      if (family == cc::Xbox &&
          ((i == 15 || i == 17 || i == 18) || (i >= 29 && i <= 38) || i == 42))
        continue;
      Vec p = controllerCompactCenter(i, family);
      if (i < cc::buttons)
        addOutput(createOutputCentered<ControllerButtonJack>(p, m, i));
      else if (i < cc::buttons + cc::analogs)
        addOutput(createOutputCentered<ControllerAnalogJack>(p, m, i));
      else
        addOutput(createOutputCentered<JackSmallPointingDown>(p, m, i));
    }
    for (int i = 0; i < ComputerscareController::INPUTS; ++i)
      addInput(createInputCentered<JackSmallPointingUp>(
          controllerCompactCenter(ComputerscareController::OUTPUTS + i, family),
          m, i));
    auto* labels = new ControllerCompactLabels(family);
    labels->module = m;
    labels->box.size = box.size;
    addChild(labels);
    if (m && !std::getenv("CONTROLLER_PREVIEW"))
      m->service = cc::service(pluginInstance->path);
  }
  void step() override {
    auto* m = dynamic_cast<ComputerscareController*>(module);
    cc::Profile profile = m ? cc::Profile(m->profile.load()) : family;
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
    if (m) {
      bool known = (m->visibleAnalog.load() & (1u << 6)) != 0;
      m->outputInfos[ComputerscareController::ANALOG + 6]->description =
          known ? "Estimated battery level: 0–10 V"
                : "Battery level unavailable from controller/driver";
    }
    ModuleWidget::step();
  }
  void appendContextMenu(Menu* menu) override {
    auto* m = dynamic_cast<ComputerscareController*>(module);
    if (!m) return;
    menu->addChild(new MenuSeparator);
    menu->addChild(createMenuItem("Reset all offsets and scales", "",
                                  [m]() { m->resetTransforms(); }));
    if (family != cc::Xbox)
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
