#include <atomic>

#include "Computerscare.hpp"
#include "Phlooper/Block.hpp"
#include "Phlooper/Dropdown.hpp"
#include "XRouter/DisplayLayout.hpp"
#include "XRouter/Edit.hpp"
#include "XRouter/Randomize.hpp"
#include "XRouter/RouteKnob.hpp"
#include "XRouter/Routing.hpp"

// One source per output channel in a fixed 16-by-16 display.
struct ComputerscareXRouter : ComputerscarePolyModule {
  // Reserve the old focus slots so existing prototype patches keep their
  // counts.
  enum ParamIds {
    LEGACY_FOCUS,
    OUTPUT_POLY = LEGACY_FOCUS + 4,
    POLY_EDIT = OUTPUT_POLY + 16,
    NUM_PARAMS
  };
  enum InputIds { POLY_INPUT, NUM_INPUTS = POLY_INPUT + 16 };
  enum OutputIds { POLY_OUTPUT, NUM_OUTPUTS = POLY_OUTPUT + 16 };
  std::array<std::atomic<int>, xrouter::routes> routing;
  xrouter::RandomOptions randomOptions;
  bool knobMode = false;
  struct PolyEditQuantity : ParamQuantity {
    float getValue() override;
    void setValue(float value) override;
    std::string getDisplayValueString() override;
    std::string getLabel() override;
    json_t* toJson() override { return json_object(); }
    void fromJson(json_t*) override {}
  };
  bool mixedPolyphony() {
    for (int port = 1; port < 16; ++port)
      if (params[OUTPUT_POLY + port].getValue() !=
          params[OUTPUT_POLY].getValue())
        return true;
    return false;
  }
  ComputerscareXRouter() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, 0);
    for (int i = 0; i < OUTPUT_POLY; ++i) {
      configParam(i, -1, 15, -1, "Legacy focus");
      getParamQuantity(i)->randomizeEnabled = false;
    }
    for (int i = 0; i < 16; ++i) {
      configInput(i, std::string(1, 'a' + i));
      configOutput(i, std::string(1, 'A' + i));
      configBypass(i, i);
    }
    for (int port = 0; port < 16; ++port) {
      configSwitch(OUTPUT_POLY + port, 0, 16, 0,
                   std::string(1, 'A' + port) + " output channels",
                   polyChannelLabels(true));
      getParamQuantity(OUTPUT_POLY + port)->randomizeEnabled = false;
    }
    configParam<PolyEditQuantity>(POLY_EDIT, 0, 16, 0, "Output channels");
    getParamQuantity(POLY_EDIT)->snapEnabled = true;
    getParamQuantity(POLY_EDIT)->randomizeEnabled = false;
    getParamQuantity(POLY_EDIT)->resetEnabled = false;
    resetRouting();
  }
  xrouter::Routing snapshotRouting() const {
    xrouter::Routing result{};
    for (int i = 0; i < xrouter::routes; ++i) result[i] = routing[i].load();
    return result;
  }
  void setRouting(const xrouter::Routing& values) {
    for (int i = 0; i < xrouter::routes; ++i) routing[i].store(values[i]);
  }
  xrouter::Availability availability(const xrouter::Routing& values) {
    xrouter::Availability result;
    for (int port = 0; port < 16; ++port)
      result.inputs[port] = inputs[port].getChannels();
    for (int port = 0; port < 16; ++port) {
      int count = int(params[OUTPUT_POLY + port].getValue());
      if (count == 0) {
        count = 1;
        for (int ch = 0; ch < 16; ++ch)
          if (result.source(values[port * 16 + ch])) count = ch + 1;
      }
      result.outputs[port] = outputs[port].isConnected() ? count : 0;
    }
    return result;
  }
  void randomizeRouting() {
    auto before = snapshotRouting();
    auto after = xrouter::randomize(
        before, {}, availability(before), randomOptions,
        [](int count) { return int(random::u32() % unsigned(count)); });
    setRouting(after);
  }
  void onRandomize() override { randomizeRouting(); }
  void resetRouting() {
    for (int i = 0; i < xrouter::routes; ++i) routing[i].store(i);
  }
  void onReset() override { resetRouting(); }
  json_t* dataToJson() override {
    json_t* root = json_object();
    json_t* values = json_array();
    for (const auto& source : routing)
      json_array_append_new(values, json_integer(source.load()));
    json_object_set_new(root, "routing", values);
    json_object_set_new(root, "knobMode", json_boolean(knobMode));
    json_object_set_new(root, "randomMode",
                        json_integer(int(randomOptions.mode)));
    json_object_set_new(root, "connectedSources",
                        json_boolean(randomOptions.connectedSources));
    json_object_set_new(root, "uniqueSources",
                        json_boolean(randomOptions.unique));
    json_object_set_new(root, "activeDestinations",
                        json_boolean(randomOptions.activeDestinations));
    json_object_set_new(root, "allowBlank",
                        json_boolean(randomOptions.allowBlank));
    return root;
  }
  void dataFromJson(json_t* root) override {
    resetRouting();
    knobMode = json_is_true(json_object_get(root, "knobMode"));
    int mode = int(json_integer_value(json_object_get(root, "randomMode")));
    randomOptions.mode = static_cast<xrouter::RandomMode>(clamp(mode, 0, 3));
    randomOptions.connectedSources =
        json_is_true(json_object_get(root, "connectedSources"));
    randomOptions.unique = json_is_true(json_object_get(root, "uniqueSources"));
    randomOptions.activeDestinations =
        json_is_true(json_object_get(root, "activeDestinations"));
    randomOptions.allowBlank =
        json_is_true(json_object_get(root, "allowBlank"));
    json_t* values = json_object_get(root, "routing");
    for (int i = 0; i < xrouter::routes; ++i) {
      json_t* value = json_array_get(values, i);
      if (json_is_integer(value)) {
        auto source = json_integer_value(value);
        routing[i].store(source >= -1 && source < xrouter::routes ? int(source)
                                                                  : -1);
      }
    }
  }
  void process(const ProcessArgs&) override {
    for (int port = 0; port < 16; ++port) {
      if (!outputs[port].isConnected()) continue;
      int count = 1;
      for (int ch = 0; ch < 16; ++ch) {
        int source = routing[port * 16 + ch].load(std::memory_order_relaxed);
        float voltage = 0.f;
        if (source >= 0 && source < xrouter::routes &&
            source % 16 < inputs[source / 16].getChannels()) {
          voltage = inputs[source / 16].getVoltage(source % 16);
          count = ch + 1;
        }
        outputs[port].setVoltage(voltage, ch);
      }
      int setting = int(params[OUTPUT_POLY + port].getValue());
      outputs[port].setChannels(setting > 0 ? setting : count);
    }
  }
};

float ComputerscareXRouter::PolyEditQuantity::getValue() {
  auto* router = static_cast<ComputerscareXRouter*>(module);
  return router->params[OUTPUT_POLY].getValue();
}
void ComputerscareXRouter::PolyEditQuantity::setValue(float value) {
  auto* router = static_cast<ComputerscareXRouter*>(module);
  value = std::round(clamp(value, 0.f, 16.f));
  for (int port = 0; port < 16; ++port)
    router->params[OUTPUT_POLY + port].setValue(value);
}
std::string ComputerscareXRouter::PolyEditQuantity::getDisplayValueString() {
  auto* router = static_cast<ComputerscareXRouter*>(module);
  if (router->mixedPolyphony()) return "Mixed";
  int setting = int(getValue());
  return setting == 0 ? "Auto" : std::to_string(setting);
}
std::string ComputerscareXRouter::PolyEditQuantity::getLabel() {
  return "All output channels";
}

struct XRouterPolyKnob : TinyChannelsSnapKnob {
  ComputerscareXRouter* router = nullptr;
  std::array<float, 16> before{};
  std::array<float, 16> snapshot() const {
    std::array<float, 16> result{};
    if (router)
      for (int port = 0; port < 16; ++port)
        result[port] =
            router->params[ComputerscareXRouter::OUTPUT_POLY + port].getValue();
    return result;
  }
  void pushChange(const std::array<float, 16>& old) {
    if (!router) return;
    auto after = snapshot();
    if (old == after) return;
    auto* change = new history::ComplexAction;
    change->name = "XRouter output polyphony";
    for (int port = 0; port < 16; ++port) {
      if (old[port] == after[port]) continue;
      auto* item = new history::ParamChange;
      item->moduleId = router->id;
      item->paramId = ComputerscareXRouter::OUTPUT_POLY + port;
      item->oldValue = old[port];
      item->newValue = after[port];
      change->push(item);
    }
    APP->history->push(change);
  }
  void draw(const DrawArgs& args) override {
    // Use the standard polyphony artwork for the global setting.
    TinyChannelsSnapKnob::module = router;
    TinyChannelsSnapKnob::paramId = ComputerscareXRouter::OUTPUT_POLY;
    TinyChannelsSnapKnob::draw(args);
  }
  void onDragStart(const event::DragStart& e) override {
    before = snapshot();
    TinyChannelsSnapKnob::onDragStart(e);
  }
  void onDragEnd(const event::DragEnd& e) override {
    // Base performs cursor cleanup; record the real target params instead of
    // the proxy.
    auto* saved = app::ParamWidget::module;
    app::ParamWidget::module = nullptr;
    TinyChannelsSnapKnob::onDragEnd(e);
    app::ParamWidget::module = saved;
    pushChange(before);
  }
  void onDoubleClick(const event::DoubleClick& e) override {
    auto old = snapshot();
    if (router)
      router->getParamQuantity(ComputerscareXRouter::POLY_EDIT)->setValue(0);
    pushChange(old);
    e.consume(this);
  }
  void onButton(const event::Button& e) override {
    if (e.button == GLFW_MOUSE_BUTTON_RIGHT && e.action == GLFW_PRESS &&
        router) {
      auto* menu = createMenu();
      menu->addChild(createMenuLabel(
          router->getParamQuantity(ComputerscareXRouter::POLY_EDIT)
              ->getLabel()));
      for (int value = 0; value <= 16; ++value)
        menu->addChild(createMenuItem(
            value == 0 ? "Auto" : std::to_string(value), "", [this, value]() {
              auto old = snapshot();
              router->getParamQuantity(ComputerscareXRouter::POLY_EDIT)
                  ->setValue(value);
              pushChange(old);
            }));
      e.consume(this);
      return;
    }
    TinyChannelsSnapKnob::onButton(e);
  }
};
struct XRouterPolyDisplay : SmallLetterDisplay {
  ComputerscareXRouter* router = nullptr;
  XRouterPolyDisplay(Vec pos) {
    box.pos = pos;
    fontSize = 14;
    letterSpacing = 1.f;
    textAlign = 18;
    textColor = BLACK;
    breakRowWidth = 20;
  }
  void draw(const DrawArgs& args) override {
    value = router ? router->getParamQuantity(ComputerscareXRouter::POLY_EDIT)
                         ->getDisplayValueString()
                   : "16";
    if (value == "Mixed") value = "~";
    if (value == "Auto") value = "A";
    SmallLetterDisplay::draw(args);
  }
};

struct XRouterCellChange : history::ModuleAction {
  int destination = 0, before = -1, after = -1;
  void apply(int value) {
    auto* router =
        dynamic_cast<ComputerscareXRouter*>(APP->engine->getModule(moduleId));
    if (router) router->routing[destination].store(value);
  }
  void undo() override { apply(before); }
  void redo() override { apply(after); }
};
static void xrouterEdit(ComputerscareXRouter* router, int destination,
                        int before, int after) {
  if (!router || destination < 0 || destination >= 256 || after < -1 ||
      after > 255 || before == after)
    return;
  router->routing[destination].store(after);
  auto* change = new XRouterCellChange;
  change->name = "Set XRouter source";
  change->moduleId = router->id;
  change->destination = destination;
  change->before = before;
  change->after = after;
  APP->history->push(change);
}
struct XRouterHoverTip {
  WeakPtr<ui::Tooltip> hoverTooltip;
  ~XRouterHoverTip() { clearHoverTip(); }
  void clearHoverTip() {
    if (hoverTooltip) hoverTooltip->requestDelete();
    hoverTooltip = nullptr;
  }
  void showHoverTip(const std::string& text) {
    if (!settings::tooltips) {
      clearHoverTip();
      return;
    }
    if (!hoverTooltip) {
      auto* tip = new ui::Tooltip;
      hoverTooltip = tip;
      APP->scene->addChild(tip);
    }
    hoverTooltip->text = text;
  }
};
struct XRouterSourcePicker : widget::OpaqueWidget {
  int64_t moduleId = -1;
  int destination = 0, selected = -1;
  int hovered = -2;
  float pickerScale = 1.5f;
  XRouterSourcePicker() { box.size = Vec(356, 344).mult(pickerScale); }
  void onHover(const event::Hover& e) override {
    Vec point = e.pos.div(pickerScale);
    hovered = xrouter::pickerCell(point.x, point.y);
    if (hovered < 0)
      hovered = point.x >= 318 && point.x < 346 && point.y >= 3 && point.y < 19
                    ? -1
                    : -2;
    widget::OpaqueWidget::onHover(e);
  }
  void onLeave(const event::Leave& e) override {
    hovered = -2;
    widget::OpaqueWidget::onLeave(e);
  }
  void choose(int source) {
    auto* router =
        dynamic_cast<ComputerscareXRouter*>(APP->engine->getModule(moduleId));
    if (router)
      xrouterEdit(router, destination, router->routing[destination].load(),
                  source);
    if (parent) parent->requestDelete();
  }
  void draw(const DrawArgs& args) override {
    auto* vg = args.vg;
    nvgSave(vg);
    nvgScale(vg, pickerScale, pickerScale);
    nvgBeginPath(vg);
    nvgMoveTo(vg, 0, 2);
    nvgLineTo(vg, 354, 0);
    nvgLineTo(vg, 356, 342);
    nvgLineTo(vg, 1, 344);
    nvgClosePath(vg);
    nvgFillColor(vg, nvgRGB(205, 211, 207));
    nvgFill(vg);
    nvgStrokeColor(vg, BLACK);
    nvgStrokeWidth(vg, 1);
    nvgStroke(vg);
    auto font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/DejaVuSansMono.ttf"));
    if (!font) {
      nvgRestore(vg);
      return;
    }
    nvgFontFaceId(vg, font->handle);
    nvgFontSize(vg, 10.f);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, BLACK);
    auto title = "Source -> " + xrouter::address(destination, true);
    nvgText(vg, 7, 11, title.c_str(), nullptr);
    nvgBeginPath(vg);
    nvgRect(vg, 318, 3, 28, 16);
    nvgFillColor(vg, hovered == -1  ? COLOR_COMPUTERSCARE_YELLOW
                     : selected < 0 ? COLOR_COMPUTERSCARE_GREEN
                                    : nvgRGB(237, 239, 235));
    nvgFill(vg);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, BLACK);
    nvgText(vg, 332, 11, "X", nullptr);
    auto* router =
        dynamic_cast<ComputerscareXRouter*>(APP->engine->getModule(moduleId));
    for (int ch = 0; ch < 16; ++ch) {
      auto label = std::to_string(ch + 1);
      nvgFontSize(vg, 8.f);
      nvgFillColor(vg, BLACK);
      nvgText(vg, 35 + ch * 20, 25, label.c_str(), nullptr);
    }
    for (int port = 0; port < 16; ++port) {
      auto label = std::string(1, 'a' + port);
      nvgFontSize(vg, 10.f);
      nvgFillColor(vg, BLACK);
      nvgText(vg, 12, 41.5f + port * 19, label.c_str(), nullptr);
      for (int ch = 0; ch < 16; ++ch) {
        int source = port * 16 + ch;
        float x = 25 + ch * 20.f, y = 32 + port * 19.f;
        nvgBeginPath(vg);
        nvgRect(vg, x + .5f, y + .5f, 19.f, 18.f);
        nvgFillColor(vg, source == hovered    ? COLOR_COMPUTERSCARE_YELLOW
                         : source == selected ? COLOR_COMPUTERSCARE_GREEN
                                              : nvgRGB(port % 2 ? 222 : 231,
                                                       port % 2 ? 226 : 234,
                                                       port % 2 ? 221 : 229));
        nvgFill(vg);
        bool available = !router || ch < router->inputs[port].getChannels();
        nvgFillColor(vg, (available || source == selected || source == hovered)
                             ? BLACK
                             : nvgRGB(111, 119, 114));
        nvgFontSize(vg, 8.5f);
        auto code = xrouter::address(source);
        nvgText(vg, x + 10, y + 9.5f, code.c_str(), nullptr);
      }
    }
    nvgRestore(vg);
  }
  void onButton(const event::Button& e) override {
    Vec point = e.pos.div(pickerScale);
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
      if (point.x >= 318 && point.x < 346 && point.y >= 3 && point.y < 19)
        choose(-1);
      else {
        int cell = xrouter::pickerCell(point.x, point.y);
        if (cell >= 0) choose(cell);
      }
    }
    e.consume(this);
  }
  void onHoverKey(const event::HoverKey& e) override {
    if (e.action == GLFW_PRESS && e.key == GLFW_KEY_X &&
        !(e.mods & RACK_MOD_MASK)) {
      choose(-1);
      e.consume(this);
    } else
      widget::OpaqueWidget::onHoverKey(e);
  }
};
struct XRouterSourceField : ui::TextField {
  int64_t moduleId = -1;
  int destination = 0;
  bool invalid = false;
  void draw(const DrawArgs& args) override {
    ui::TextField::draw(args);
    if (invalid) {
      nvgBeginPath(args.vg);
      nvgRect(args.vg, 1, 1, box.size.x - 2, box.size.y - 2);
      nvgStrokeColor(args.vg, COLOR_COMPUTERSCARE_RED);
      nvgStrokeWidth(args.vg, 1.5f);
      nvgStroke(args.vg);
    }
  }
  void step() override {
    ui::TextField::step();
    APP->event->setSelectedWidget(this);
  }
  void onSelectKey(const event::SelectKey& e) override {
    if (e.action == GLFW_PRESS &&
        (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER)) {
      int source = -1;
      invalid = !xrouter::parseSource(text, source);
      if (!invalid) {
        auto* router = dynamic_cast<ComputerscareXRouter*>(
            APP->engine->getModule(moduleId));
        if (router)
          xrouterEdit(router, destination, router->routing[destination].load(),
                      source);
        if (auto* overlay = getAncestorOfType<ui::MenuOverlay>())
          overlay->requestDelete();
      }
      e.consume(this);
      return;
    }
    if (e.action == GLFW_PRESS && e.key == GLFW_KEY_ESCAPE) {
      if (auto* overlay = getAncestorOfType<ui::MenuOverlay>())
        overlay->requestDelete();
      e.consume(this);
      return;
    }
    ui::TextField::onSelectKey(e);
  }
};

struct XRouterDisplay : widget::OpaqueWidget, XRouterHoverTip {
  ComputerscareXRouter* module = nullptr;
  xrouter::Routing routing = xrouter::identity();
  xrouter::Availability available;
  int dragDestination = -1, dragBefore = -1;
  float dragSteps = 0.f, distance = 0.f;
  bool locked = false;
  int hoverDestination = -1;
  void onHover(const event::Hover& e) override {
    hoverDestination =
        xrouter::matrixCell(e.pos.x, e.pos.y, box.size.x, box.size.y);
    if (hoverDestination >= 0)
      showHoverTip(
          xrouter::address(module ? module->routing[hoverDestination].load()
                                  : routing[hoverDestination]) +
          " -> " + xrouter::address(hoverDestination, true));
    else
      clearHoverTip();
    widget::OpaqueWidget::onHover(e);
  }
  void onLeave(const event::Leave& e) override {
    hoverDestination = -1;
    clearHoverTip();
    widget::OpaqueWidget::onLeave(e);
  }

  void openPicker(int destination) {
    clearHoverTip();
    if (!module) return;
    auto* overlay = new ui::MenuOverlay;
    auto* picker = new XRouterSourcePicker;
    picker->moduleId = module->id;
    picker->destination = destination;
    picker->selected = module->routing[destination].load();
    picker->pickerScale = std::max(
        .5f, std::min(1.5f, std::min((APP->scene->box.size.x - 16) / 356.f,
                                     (APP->scene->box.size.y - 16) / 344.f)));
    picker->box.size = Vec(356, 344).mult(picker->pickerScale);
    Vec anchor = APP->scene->mousePos.minus(picker->box.size.mult(.5f));
    picker->box.pos =
        Vec(clamp(anchor.x, 0.f,
                  std::max(0.f, APP->scene->box.size.x - picker->box.size.x)),
            clamp(anchor.y, 0.f,
                  std::max(0.f, APP->scene->box.size.y - picker->box.size.y)));
    overlay->addChild(picker);
    APP->scene->addChild(overlay);
  }
  void openText(int destination) {
    clearHoverTip();
    if (!module) return;
    auto* menu = createMenu();
    menu->addChild(
        createMenuLabel("Source -> " + xrouter::address(destination, true)));
    auto* field = new XRouterSourceField;
    field->moduleId = module->id;
    field->destination = destination;
    field->box.size.x = 170;
    field->text = xrouter::address(module->routing[destination].load());
    field->selectAll();
    menu->addChild(field);
    menu->addChild(createMenuLabel("a1-p16 / X = blank"));
    APP->event->setSelectedWidget(field);
  }
  void onButton(const event::Button& e) override {
    int cell = xrouter::matrixCell(e.pos.x, e.pos.y, box.size.x, box.size.y);
    if (!module || cell < 0) return;
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT) {
      openText(cell);
      e.consume(this);
    } else if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
      if (module->knobMode)
        dragDestination = cell;
      else
        openPicker(cell);
      e.consume(this);
    }
  }
  void onDragStart(const event::DragStart& e) override {
    if (!module || !module->knobMode || dragDestination < 0 ||
        e.button != GLFW_MOUSE_BUTTON_LEFT)
      return;
    dragBefore = module->routing[dragDestination].load();
    dragSteps = distance = 0.f;
    APP->window->cursorLock();
    locked = true;
  }
  void onDragMove(const event::DragMove& e) override {
    if (!module || !locked || e.button != GLFW_MOUSE_BUTTON_LEFT) return;
    distance += std::abs(e.mouseDelta.y) + std::abs(e.mouseDelta.x);
    int mods = APP->window->getMods();
    float scale = (mods & RACK_MOD_CTRL)    ? .05f
                  : (mods & GLFW_MOD_SHIFT) ? 2.f
                                            : .5f;
    dragSteps -= e.mouseDelta.y * scale;
    module->routing[dragDestination].store(
        xrouter::draggedSource(dragBefore, dragSteps));
  }
  void onDragEnd(const event::DragEnd& e) override {
    if (e.button != GLFW_MOUSE_BUTTON_LEFT || !locked) return;
    APP->window->cursorUnlock();
    locked = false;
    if (module) {
      int after = module->routing[dragDestination].load();
      if (distance < 2.f) {
        module->routing[dragDestination].store(dragBefore);
      } else
        xrouterEdit(module, dragDestination, dragBefore, after);
    }
    dragDestination = -1;
  }
  XRouterDisplay() {
    available.inputs.fill(16);
    available.outputs.fill(16);
  }
  void step() override {
    widget::OpaqueWidget::step();
    if (module) {
      routing = module->snapshotRouting();
      available = module->availability(routing);
    }
    if (hoverTooltip && hoverDestination >= 0)
      hoverTooltip->text = xrouter::address(routing[hoverDestination]) +
                           " -> " + xrouter::address(hoverDestination, true);
  }
  void draw(const DrawArgs& args) override {
    phlooper::drawDisplayFrame(args.vg, box.size, -1.f, -1.f, 1.f, 6.5f);
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
    nvgFillColor(args.vg, nvgRGB(20, 27, 26));
    nvgFill(args.vg);
    auto font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/DejaVuSansMono.ttf"));
    if (!font) return;
    nvgSave(args.vg);
    nvgScissor(args.vg, 0, 0, box.size.x, box.size.y);
    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 8.f);
    float textSize = xrouter::sourceFontSize(
        box.size.x, nvgTextBounds(args.vg, 0, 0, "m16", nullptr, nullptr));
    nvgFontSize(args.vg, textSize);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    for (int port = 0; port < 16; ++port)
      for (int ch = 0; ch < 16; ++ch) {
        int dest = port * 16 + ch;
        bool knobs = module && module->knobMode;
        Vec center(xrouter::sourceColumnX(box.size.x, ch),
                   (port + .5f) * box.size.y / 16.f);
        if (knobs)
          xrouter::drawRouteKnob(args.vg, center, 19.5f, routing[dest],
                                 available.route(routing, dest), dest + 1);
        nvgFontSize(args.vg, knobs ? 7.3f : textSize);
        auto label = xrouter::address(routing[dest]);
        nvgFillColor(args.vg, available.route(routing, dest)
                                  ? COLOR_COMPUTERSCARE_LIGHT_GREEN
                                  : nvgRGB(79, 93, 89));
        nvgText(args.vg, xrouter::sourceColumnX(box.size.x, ch),
                (port + .5f) * box.size.y / 16.f, label.c_str(), nullptr);
      }
    nvgRestore(args.vg);
  }
};
struct XRouterJackLabels : widget::TransparentWidget {
  void draw(const DrawArgs& args) override {
    auto font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/DejaVuSansMono.ttf"));
    if (!font) return;
    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 13.f);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(args.vg, BLACK);
    for (int port = 0; port < 16; ++port) {
      float y = 31.f + (port + .5f) * 344.f / 16.f;
      auto input = std::string(1, 'a' + port),
           output = std::string(1, 'A' + port);
      nvgText(args.vg, 44.f, y, input.c_str(), nullptr);
      nvgText(args.vg, 451.f, y, output.c_str(), nullptr);
    }
  }
};

struct XRouterBackground : widget::TransparentWidget {
  void draw(const DrawArgs& args) override {
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
    nvgFillColor(args.vg, nvgRGB(190, 190, 190));
    nvgFill(args.vg);
  }
};

struct XRouterRouteChange : history::ModuleAction {
  xrouter::Routing before{}, after{};
  void apply(const xrouter::Routing& values) {
    auto* router =
        dynamic_cast<ComputerscareXRouter*>(APP->engine->getModule(moduleId));
    if (router) router->setRouting(values);
  }
  void undo() override { apply(before); }
  void redo() override { apply(after); }
};

struct XRouterModeButton : phlooper::WarpedDropdown {
  ComputerscareXRouter* module = nullptr;
  std::string label() override {
    return module && module->knobMode ? "Knobs" : "Grid";
  }
  bool isPressed() override { return module && module->knobMode; }
  void onButton(const event::Button& e) override {
    if (module && e.action == GLFW_PRESS &&
        e.button == GLFW_MOUSE_BUTTON_LEFT) {
      module->knobMode = !module->knobMode;
      e.consume(this);
    }
  }
};

struct ComputerscareXRouterWidget : ModuleWidget {
  void randomizeRouting() {
    auto* router = dynamic_cast<ComputerscareXRouter*>(module);
    if (!router) return;
    auto before = router->snapshotRouting();
    router->randomizeRouting();
    auto after = router->snapshotRouting();
    if (before == after) return;
    auto* change = new XRouterRouteChange;
    change->name = "Randomize XRouter routes";
    change->moduleId = router->id;
    change->before = before;
    change->after = after;
    APP->history->push(change);
  }
  void onHoverKey(const event::HoverKey& e) override {
    if (module && e.key == GLFW_KEY_R &&
        (e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL &&
        !dynamic_cast<ui::TextField*>(APP->event->getSelectedWidget())) {
      if (e.action == GLFW_PRESS) randomizeRouting();
      e.consume(this);
      return;
    }
    ModuleWidget::onHoverKey(e);
  }
  void appendContextMenu(ui::Menu* menu) override {
    auto* router = dynamic_cast<ComputerscareXRouter*>(module);
    if (!router) return;
    menu->addChild(new ui::MenuSeparator);
    menu->addChild(createMenuItem("Randomize routes", RACK_MOD_CTRL_NAME "+R",
                                  [this]() { randomizeRouting(); }));
    menu->addChild(
        createSubmenuItem("Randomization", "", [router](ui::Menu* sub) {
          const char* names[] = {"New sources", "Shuffle existing sources",
                                 "Keep jacks, randomize channels",
                                 "Keep channels, randomize jacks"};
          for (int i = 0; i < 4; ++i)
            sub->addChild(createCheckMenuItem(
                names[i], "",
                [router, i]() { return int(router->randomOptions.mode) == i; },
                [router, i]() {
                  router->randomOptions.mode =
                      static_cast<xrouter::RandomMode>(i);
                }));
          sub->addChild(new ui::MenuSeparator);
          sub->addChild(
              createBoolPtrMenuItem("Use connected input channels only", "",
                                    &router->randomOptions.connectedSources));
          sub->addChild(createBoolPtrMenuItem("Without replacement", "",
                                              &router->randomOptions.unique));
          sub->addChild(
              createBoolPtrMenuItem("Active output channels only", "",
                                    &router->randomOptions.activeDestinations));
          sub->addChild(createBoolPtrMenuItem(
              "Include blank", "", &router->randomOptions.allowBlank));
        }));
    menu->addChild(
        createSubmenuItem("Output polyphony", "", [router](ui::Menu* sub) {
          for (int port = 0; port < 16; ++port)
            sub->addChild(createSubmenuItem(
                std::string(1, 'A' + port), "",
                [router, port](ui::Menu* counts) {
                  for (int count = 0; count <= 16; ++count)
                    counts->addChild(createCheckMenuItem(
                        count == 0 ? "Auto" : std::to_string(count), "",
                        [router, port, count]() {
                          return router
                                     ->params
                                         [ComputerscareXRouter::OUTPUT_POLY +
                                          port]
                                     .getValue() == count;
                        },
                        [router, port, count]() {
                          int id = ComputerscareXRouter::OUTPUT_POLY + port;
                          auto* h = new history::ParamChange;
                          h->name = "XRouter output polyphony";
                          h->moduleId = router->id;
                          h->paramId = id;
                          h->oldValue = router->params[id].getValue();
                          h->newValue = count;
                          router->params[id].setValue(count);
                          APP->history->push(h);
                        }));
                }));
        }));
  }
  explicit ComputerscareXRouterWidget(ComputerscareXRouter* module) {
    setModule(module);
    box.size = Vec(495, 380);
    auto* bg = new XRouterBackground;
    bg->box.size = box.size;
    addChild(bg);
    for (int side = 0; side < 2; ++side) {
      auto* block = new phlooper::WarpedBlock;
      block->configure(
          Rect(Vec(side ? 436 : 5, 31), Vec(54, 344)), 3, 1.5f, 702 + side,
          nvgRGB(side ? 170 : 220, side ? 170 : 220, side ? 170 : 220));
      addChild(block);
    }
    auto* panel = new ComputerscareSVGPanel;
    panel->setBackground(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/panels/ComputerscareXRouterPanel.svg")));
    addChild(panel);
    auto* mode = new XRouterModeButton;
    mode->module = module;
    mode->configure(54, 21, 3, 2, .8f, 718);
    mode->textSize = 12;
    mode->box.pos = Vec(206, 4);
    addChild(mode);
    auto* display = new XRouterDisplay;
    display->module = module;
    display->box = Rect(Vec(69, 31), Vec(356, 344));
    addChild(display);
    auto* poly = createParam<XRouterPolyKnob>(Vec(468, 4), module,
                                              ComputerscareXRouter::POLY_EDIT);
    poly->router = module;
    poly->previewChannels = 16;
    addParam(poly);
    auto* count = new XRouterPolyDisplay(Vec(461, 1));
    count->router = module;
    addChild(count);
    auto* labels = new XRouterJackLabels;
    labels->box.size = box.size;
    addChild(labels);
    for (int i = 0; i < 16; ++i) {
      float y = 31.f + (i + .5f) * 344.f / 16.f;
      addInput(
          createInputCentered<JackSmallPointingDown>(Vec(23, y), module, i));
      addOutput(
          createOutputCentered<JackSmallPointingUp>(Vec(472, y), module, i));
    }
  }
};

Model* modelComputerscareXRouter =
    createModel<ComputerscareXRouter, ComputerscareXRouterWidget>(
        "computerscare-xrouter");
