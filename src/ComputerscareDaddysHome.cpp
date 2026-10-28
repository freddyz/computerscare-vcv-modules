#include <atomic>

#include "Computerscare.hpp"
#include "DaddysHome/DisplayLayout.hpp"
#include "DaddysHome/Edit.hpp"
#include "DaddysHome/Randomize.hpp"
#include "DaddysHome/RouteKnob.hpp"
#include "DaddysHome/Routing.hpp"
#include "Phlooper/Block.hpp"
#include "Phlooper/Dropdown.hpp"
#include "components/NumberDotKnob.hpp"
#include "components/RandomBlock.hpp"

// One source per output channel in a fixed 16-by-16 display.
struct ComputerscareDaddysHome : ComputerscarePolyModule {
  // Reserve the old focus slots so existing prototype patches keep their
  // counts.
  enum ParamIds {
    LEGACY_FOCUS,
    OUTPUT_POLY = LEGACY_FOCUS + 4,
    POLY_EDIT = OUTPUT_POLY + 16,
    ROUTE,
    RANDOM_PROBABILITY = ROUTE + 256,
    WIGGLE_PROBABILITY,
    EDIT_SCENE,
    STEPS,
    NUM_PARAMS
  };
  enum InputIds {
    POLY_INPUT,
    RANDOM_ALL = POLY_INPUT + 16,
    RANDOM_OUTPUT,
    RANDOM_CHANNEL,
    WIGGLE_ALL,
    WIGGLE_OUTPUT,
    WIGGLE_CHANNEL,
    RANDOM_PROB_CV,
    WIGGLE_PROB_CV,
    CLOCK,
    RESET,
    SCAN,
    NUM_INPUTS
  };
  enum OutputIds { POLY_OUTPUT, EOC = POLY_OUTPUT + 16, NUM_OUTPUTS };
  struct RouteSlot {
    engine::Param* parameter = nullptr;
    int load() const {
      return clamp(int(std::round(parameter->getValue())), -1, 255);
    }
    void store(int value) { parameter->setValue(clamp(value, -1, 255)); }
  };
  std::array<RouteSlot, daddys_home::routes> routing;
  struct Scene {
    std::array<int16_t, 256> routes{};
    std::array<uint8_t, 16> poly{};
    Scene() {
      for (int i = 0; i < 256; ++i) routes[i] = i;
    }
  };
  std::array<Scene, 16> scenes;
  int editingScene = 0;
  int activeScene = 0;
  dsp::SchmittTrigger sceneClock, sceneReset;
  dsp::PulseGenerator cyclePulse;
  Scene captureScene() {
    Scene result;
    for (int i = 0; i < 256; ++i) result.routes[i] = routing[i].load();
    for (int i = 0; i < 16; ++i)
      result.poly[i] =
          clamp(int(std::round(params[OUTPUT_POLY + i].getValue())), 0, 16);
    return result;
  }
  void selectEditingScene(int index) {
    index = clamp(index, 0, 15);
    if (index == editingScene) return;
    scenes[editingScene] = captureScene();
    editingScene = index;
    for (int i = 0; i < 256; ++i) routing[i].store(scenes[index].routes[i]);
    for (int i = 0; i < 16; ++i)
      params[OUTPUT_POLY + i].setValue(scenes[index].poly[i]);
  }
  void processScenes(float dt) {
    // Both controls are bounded positive integers; avoid two round() calls
    // on every audio sample. MIDI/automation changes still apply immediately.
    selectEditingScene(int(params[EDIT_SCENE].getValue() + .5f) - 1);
    int steps = clamp(int(params[STEPS].getValue() + .5f), 1, 16);
    bool clock = sceneClock.process(inputs[CLOCK].getVoltage() / 2.f);
    bool reset = sceneReset.process(inputs[RESET].getVoltage() / 2.f);
    if (inputs[SCAN].isConnected()) {
      activeScene =
          clamp(int(std::floor(clamp(inputs[SCAN].getVoltage(), 0.f, 10.f) *
                               steps / 10.f)),
                0, steps - 1);
      cyclePulse.reset();
    } else if (reset) {
      activeScene = 0;
      cyclePulse.reset();
    } else {
      if (activeScene >= steps) activeScene = 0;
      if (clock) {
        activeScene = (activeScene + 1) % steps;
        if (activeScene == 0) cyclePulse.trigger(.001f);
      }
    }
    outputs[EOC].setVoltage(cyclePulse.process(dt) ? 10.f : 0.f);
  }
  dsp::SchmittTrigger actionTriggers[2][3][16];
  struct SourceQuantity : ParamQuantity {
    std::string getDisplayValueString() override {
      return daddys_home::address(clamp(int(std::round(getValue())), -1, 255));
    }
    std::string getString() override {
      return getDisplayValueString() + " → " +
             daddys_home::address(paramId - ROUTE, true);
    }
    void setDisplayValueString(std::string text) override {
      int source;
      if (daddys_home::parseSource(text, source)) setValue(source);
    }
  };
  daddys_home::RandomOptions randomOptions;
  bool knobMode = false;
  int lastManualDestination = -1;
  double lastManualEditTime = -1.;
  void markManualEdit(int destination) {
    lastManualDestination = destination;
    lastManualEditTime = system::getTime();
  }
  unsigned previousActionMask = 0;
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
  ComputerscareDaddysHome() {
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
    for (int i = 0; i < 256; ++i) {
      configParam<SourceQuantity>(ROUTE + i, -1, 255, i,
                                  "Source → " + daddys_home::address(i, true));
      getParamQuantity(ROUTE + i)->snapEnabled = true;
      getParamQuantity(ROUTE + i)->randomizeEnabled = false;
      routing[i].parameter = &params[ROUTE + i];
    }
    configParam(RANDOM_PROBABILITY, 0, 1, 1, "Randomize probability", "%", 0,
                100);
    configParam(WIGGLE_PROBABILITY, 0, 1, 1, "Wiggle probability", "%", 0, 100);
    getParamQuantity(RANDOM_PROBABILITY)->randomizeEnabled = false;
    getParamQuantity(WIGGLE_PROBABILITY)->randomizeEnabled = false;
    const char* controls[] = {"Randomize all",
                              "Randomize by output",
                              "Randomize by output channel",
                              "Wiggle all",
                              "Wiggle by output",
                              "Wiggle by output channel",
                              "Randomize probability",
                              "Wiggle probability"};
    for (int i = 0; i < 8; ++i) configInput(RANDOM_ALL + i, controls[i]);
    configSwitch(EDIT_SCENE, 1, 16, 1, "Editing scene");
    configSwitch(STEPS, 1, 16, 16, "Number of steps");
    getParamQuantity(EDIT_SCENE)->randomizeEnabled = false;
    getParamQuantity(STEPS)->randomizeEnabled = false;
    configInput(CLOCK, "Clock");
    configInput(RESET, "Reset");
    configInput(SCAN, "Scan (0–10 V)");
    configOutput(EOC, "End of cycle");
    sceneClock.process(0.f);
    sceneReset.process(0.f);
    resetRouting();
    for (auto& kind : actionTriggers)
      for (auto& scope : kind)
        for (auto& trigger : scope) trigger.process(0.f);
  }
  daddys_home::Routing snapshotRouting() const {
    daddys_home::Routing result{};
    for (int i = 0; i < daddys_home::routes; ++i) result[i] = routing[i].load();
    return result;
  }
  void setRouting(const daddys_home::Routing& values) {
    for (int i = 0; i < daddys_home::routes; ++i) routing[i].store(values[i]);
  }
  daddys_home::Availability availability(const daddys_home::Routing& values) {
    daddys_home::Availability result;
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
  float actionProbability(bool wiggle) {
    return clamp(
        params[wiggle ? WIGGLE_PROBABILITY : RANDOM_PROBABILITY].getValue() +
            inputs[wiggle ? WIGGLE_PROB_CV : RANDOM_PROB_CV].getVoltage() /
                10.f,
        0.f, 1.f);
  }
  void randomizeRouting(bool wiggle = false, int scope = 0, int index = 0) {
    auto before = snapshotRouting();
    daddys_home::Filter filter;
    if (scope == 1) filter.outputPort = index;
    if (scope == 2) filter.outputChannel = index;
    auto after = daddys_home::randomize(
        before, filter, availability(before), randomOptions,
        [](int count) { return int(random::u32() % unsigned(count)); },
        actionProbability(wiggle), wiggle);
    setRouting(after);
  }
  void processActionTriggers() {
    for (int kind = 0; kind < 2; ++kind)
      for (int scope = 0; scope < 3; ++scope) {
        auto& input = inputs[RANDOM_ALL + kind * 3 + scope];
        unsigned bit = 1u << (kind * 3 + scope);
        if (!input.isConnected()) {
          if (previousActionMask & bit)
            for (auto& trigger : actionTriggers[kind][scope]) {
              trigger.reset();
              trigger.process(0.f);
            }
          previousActionMask &= ~bit;
          continue;
        }
        previousActionMask |= bit;
        if (scope == 0) {
          float voltage = 0;
          for (int ch = 0; ch < input.getChannels(); ++ch)
            voltage = std::max(voltage, input.getVoltage(ch));
          if (actionTriggers[kind][scope][0].process(voltage / 2.f))
            randomizeRouting(kind == 1);
        } else {
          for (int ch = 0; ch < 16; ++ch)
            if (actionTriggers[kind][scope][ch].process(
                    (ch < input.getChannels() ? input.getVoltage(ch) : 0.f) /
                    2.f))
              randomizeRouting(kind == 1, scope, ch);
        }
      }
  }
  void onRandomize() override { randomizeRouting(); }
  void resetRouting() {
    for (int i = 0; i < daddys_home::routes; ++i) routing[i].store(i);
  }
  void onReset() override {
    resetRouting();
    scenes.fill(Scene{});
    editingScene = activeScene = 0;
    params[EDIT_SCENE].setValue(1);
    sceneClock.reset();
    sceneClock.process(0.f);
    sceneReset.reset();
    sceneReset.process(0.f);
    cyclePulse.reset();
  }
  json_t* dataToJson() override {
    json_t* root = json_object();
    json_t* sceneList = json_array();
    int selected =
        clamp(int(std::round(params[EDIT_SCENE].getValue())) - 1, 0, 15);
    // The parameter buffer belongs to editingScene until the engine switches
    // it.
    for (int index = 0; index < 16; ++index) {
      Scene scene = index == editingScene ? captureScene() : scenes[index];
      json_t* item = json_object();
      json_t* routes = json_array();
      json_t* poly = json_array();
      for (auto route : scene.routes)
        json_array_append_new(routes, json_integer(route));
      for (auto count : scene.poly)
        json_array_append_new(poly, json_integer(count));
      json_object_set_new(item, "routes", routes);
      json_object_set_new(item, "poly", poly);
      json_array_append_new(sceneList, item);
    }
    json_object_set_new(root, "scenes", sceneList);
    json_object_set_new(root, "editingScene", json_integer(selected));
    json_object_set_new(root, "activeScene", json_integer(activeScene));
    json_t* values = json_array();
    Scene selectedScene =
        selected == editingScene ? captureScene() : scenes[selected];
    for (auto source : selectedScene.routes)
      json_array_append_new(values, json_integer(source));
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
    randomOptions.mode =
        static_cast<daddys_home::RandomMode>(clamp(mode, 0, 3));
    randomOptions.connectedSources =
        json_is_true(json_object_get(root, "connectedSources"));
    randomOptions.unique = json_is_true(json_object_get(root, "uniqueSources"));
    randomOptions.activeDestinations =
        json_is_true(json_object_get(root, "activeDestinations"));
    randomOptions.allowBlank =
        json_is_true(json_object_get(root, "allowBlank"));
    json_t* values = json_object_get(root, "routing");
    for (int i = 0; i < daddys_home::routes; ++i) {
      json_t* value = json_array_get(values, i);
      if (json_is_integer(value)) {
        auto source = json_integer_value(value);
        routing[i].store(
            source >= -1 && source < daddys_home::routes ? int(source) : -1);
      }
    }
    scenes.fill(Scene{});
    editingScene =
        clamp(int(std::round(params[EDIT_SCENE].getValue())) - 1, 0, 15);
    activeScene = 0;
    json_t* sceneList = json_object_get(root, "scenes");
    if (json_is_array(sceneList)) {
      editingScene =
          clamp(int(json_integer_value(json_object_get(root, "editingScene"))),
                0, 15);
      activeScene = clamp(
          int(json_integer_value(json_object_get(root, "activeScene"))), 0, 15);
      for (int index = 0; index < 16; ++index) {
        json_t* item = json_array_get(sceneList, index);
        for (int i = 0; i < 256; ++i) {
          json_t* value = json_array_get(json_object_get(item, "routes"), i);
          if (json_is_integer(value))
            scenes[index].routes[i] = std::max<json_int_t>(
                -1, std::min<json_int_t>(255, json_integer_value(value)));
        }
        for (int i = 0; i < 16; ++i) {
          json_t* value = json_array_get(json_object_get(item, "poly"), i);
          if (json_is_integer(value))
            scenes[index].poly[i] = std::max<json_int_t>(
                0, std::min<json_int_t>(16, json_integer_value(value)));
        }
      }
      for (int i = 0; i < 256; ++i) {
        json_t* value = json_array_get(values, i);
        if (json_is_integer(value))
          scenes[editingScene].routes[i] = std::max<json_int_t>(
              -1, std::min<json_int_t>(255, json_integer_value(value)));
      }
      params[EDIT_SCENE].setValue(editingScene + 1);
      for (int i = 0; i < 256; ++i)
        routing[i].store(scenes[editingScene].routes[i]);
      for (int i = 0; i < 16; ++i)
        params[OUTPUT_POLY + i].setValue(scenes[editingScene].poly[i]);
    } else
      scenes[editingScene] = captureScene();
  }
  template <bool editing>
  void processRouting() {
    auto* routeParams = params.data() + ROUTE;
    for (int port = 0; port < 16; ++port) {
      auto& output = outputs[port];
      if (!output.isConnected()) continue;
      int count = 1;
      for (int ch = 0; ch < 16; ++ch) {
        int source =
            editing
                ? clamp(int(std::round(routeParams[port * 16 + ch].getValue())),
                        -1, 255)
                : scenes[activeScene].routes[port * 16 + ch];
        float voltage = 0.f;
        if (source >= 0 && (source & 15) < inputs[source >> 4].getChannels()) {
          voltage = inputs[source >> 4].getVoltage(source & 15);
          count = ch + 1;
        }
        output.setVoltage(voltage, ch);
      }
      int setting = editing ? int(params[OUTPUT_POLY + port].getValue())
                            : scenes[activeScene].poly[port];
      output.setChannels(setting > 0 ? setting : count);
    }
  }
  void process(const ProcessArgs& args) override {
    processScenes(args.sampleTime);
    processActionTriggers();
    // Choose once for the whole matrix, so the live-param loop stays as lean
    // as the original router. Inactive editing uses compact stored routes.
    if (activeScene == editingScene)
      processRouting<true>();
    else
      processRouting<false>();
  }
};

float ComputerscareDaddysHome::PolyEditQuantity::getValue() {
  auto* router = static_cast<ComputerscareDaddysHome*>(module);
  return router->params[OUTPUT_POLY].getValue();
}
void ComputerscareDaddysHome::PolyEditQuantity::setValue(float value) {
  auto* router = static_cast<ComputerscareDaddysHome*>(module);
  value = std::round(clamp(value, 0.f, 16.f));
  for (int port = 0; port < 16; ++port)
    router->params[OUTPUT_POLY + port].setValue(value);
}
std::string ComputerscareDaddysHome::PolyEditQuantity::getDisplayValueString() {
  auto* router = static_cast<ComputerscareDaddysHome*>(module);
  if (router->mixedPolyphony()) return "Mixed";
  int setting = int(getValue());
  return setting == 0 ? "Auto" : std::to_string(setting);
}
std::string ComputerscareDaddysHome::PolyEditQuantity::getLabel() {
  return "All output channels";
}

struct DaddysHomeSceneDisplay : widget::Widget {
  ComputerscareDaddysHome* router = nullptr;
  void draw(const DrawArgs& args) override {
    auto font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/DejaVuSansMono.ttf"));
    if (!font) return;
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, 18, 20);
    nvgFillColor(args.vg, nvgRGB(17, 29, 26));
    nvgFill(args.vg);
    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 13);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(args.vg, COLOR_COMPUTERSCARE_GREEN);
    std::string text = std::to_string(router ? router->activeScene + 1 : 1);
    nvgText(args.vg, 9, 10, text.c_str(), nullptr);
  }
};

struct DaddysHomePolyKnob : TinyChannelsSnapKnob {
  ComputerscareDaddysHome* router = nullptr;
  std::array<float, 16> before{};
  std::array<float, 16> snapshot() const {
    std::array<float, 16> result{};
    if (router)
      for (int port = 0; port < 16; ++port)
        result[port] =
            router->params[ComputerscareDaddysHome::OUTPUT_POLY + port]
                .getValue();
    return result;
  }
  void pushChange(const std::array<float, 16>& old) {
    if (!router) return;
    auto after = snapshot();
    if (old == after) return;
    auto* change = new history::ComplexAction;
    change->name = "Daddy's Home output polyphony";
    for (int port = 0; port < 16; ++port) {
      if (old[port] == after[port]) continue;
      auto* item = new history::ParamChange;
      item->moduleId = router->id;
      item->paramId = ComputerscareDaddysHome::OUTPUT_POLY + port;
      item->oldValue = old[port];
      item->newValue = after[port];
      change->push(item);
    }
    APP->history->push(change);
  }
  void draw(const DrawArgs& args) override {
    // Use the standard polyphony artwork for the global setting.
    TinyChannelsSnapKnob::module = router;
    TinyChannelsSnapKnob::paramId = ComputerscareDaddysHome::OUTPUT_POLY;
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
      router->getParamQuantity(ComputerscareDaddysHome::POLY_EDIT)->setValue(0);
    pushChange(old);
    e.consume(this);
  }
  void onButton(const event::Button& e) override {
    if (e.button == GLFW_MOUSE_BUTTON_RIGHT && e.action == GLFW_PRESS &&
        router) {
      auto* menu = createMenu();
      menu->addChild(createMenuLabel(
          router->getParamQuantity(ComputerscareDaddysHome::POLY_EDIT)
              ->getLabel()));
      for (int value = 0; value <= 16; ++value)
        menu->addChild(createMenuItem(
            value == 0 ? "Auto" : std::to_string(value), "", [this, value]() {
              auto old = snapshot();
              router->getParamQuantity(ComputerscareDaddysHome::POLY_EDIT)
                  ->setValue(value);
              pushChange(old);
            }));
      e.consume(this);
      return;
    }
    TinyChannelsSnapKnob::onButton(e);
  }
};
struct DaddysHomePolyDisplay : SmallLetterDisplay {
  ComputerscareDaddysHome* router = nullptr;
  DaddysHomePolyDisplay(Vec pos) {
    box.pos = pos;
    fontSize = 14;
    letterSpacing = 1.f;
    textAlign = 18;
    textColor = BLACK;
    breakRowWidth = 20;
  }
  void draw(const DrawArgs& args) override {
    value = router
                ? router->getParamQuantity(ComputerscareDaddysHome::POLY_EDIT)
                      ->getDisplayValueString()
                : "16";
    if (value == "Mixed") value = "~";
    if (value == "Auto") value = "A";
    SmallLetterDisplay::draw(args);
  }
};

struct DaddysHomeCellChange : history::ModuleAction {
  int destination = 0, before = -1, after = -1;
  void apply(int value) {
    auto* router = dynamic_cast<ComputerscareDaddysHome*>(
        APP->engine->getModule(moduleId));
    if (router) router->routing[destination].store(value);
  }
  void undo() override { apply(before); }
  void redo() override { apply(after); }
};
static void daddysHomeEdit(ComputerscareDaddysHome* router, int destination,
                           int before, int after) {
  if (!router || destination < 0 || destination >= 256 || after < -1 ||
      after > 255 || before == after)
    return;
  router->routing[destination].store(after);
  router->markManualEdit(destination);
  auto* change = new DaddysHomeCellChange;
  change->name = "Set Daddy's Home source";
  change->moduleId = router->id;
  change->destination = destination;
  change->before = before;
  change->after = after;
  APP->history->push(change);
}
struct DaddysHomeHoverTip {
  WeakPtr<ui::Tooltip> hoverTooltip;
  ~DaddysHomeHoverTip() { clearHoverTip(); }
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
struct DaddysHomeSourcePicker : widget::OpaqueWidget {
  int64_t moduleId = -1;
  int destination = 0, selected = -1;
  int hovered = -2;
  float pickerScale = 1.5f;
  DaddysHomeSourcePicker() { box.size = Vec(356, 344).mult(pickerScale); }
  void onHover(const event::Hover& e) override {
    Vec point = e.pos.div(pickerScale);
    hovered = daddys_home::pickerCell(point.x, point.y);
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
    auto* router = dynamic_cast<ComputerscareDaddysHome*>(
        APP->engine->getModule(moduleId));
    if (router)
      daddysHomeEdit(router, destination, router->routing[destination].load(),
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
    auto title = "Source → " + daddys_home::address(destination, true);
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
    auto* router = dynamic_cast<ComputerscareDaddysHome*>(
        APP->engine->getModule(moduleId));
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
        auto code = daddys_home::address(source);
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
        int cell = daddys_home::pickerCell(point.x, point.y);
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
struct DaddysHomeSourceField : ui::TextField {
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
      invalid = !daddys_home::parseSource(text, source);
      if (!invalid) {
        auto* router = dynamic_cast<ComputerscareDaddysHome*>(
            APP->engine->getModule(moduleId));
        if (router)
          daddysHomeEdit(router, destination,
                         router->routing[destination].load(), source);
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

struct DaddysHomeDisplay : widget::OpaqueWidget, DaddysHomeHoverTip {
  ComputerscareDaddysHome* module = nullptr;
  daddys_home::Routing routing = daddys_home::identity();
  daddys_home::Availability available;
  int dragDestination = -1, dragBefore = -1;
  float dragSteps = 0.f, distance = 0.f;
  bool locked = false;
  int hoverDestination = -1;
  void updateHover(Vec pos) {
    hoverDestination =
        daddys_home::matrixCell(pos.x, pos.y, box.size.x, box.size.y);
    if (hoverDestination >= 0)
      showHoverTip(
          daddys_home::address(module ? module->routing[hoverDestination].load()
                                      : routing[hoverDestination]) +
          " → " + daddys_home::address(hoverDestination, true));
    else
      clearHoverTip();
  }
  void onHover(const event::Hover& e) override {
    updateHover(e.pos);
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
    auto* picker = new DaddysHomeSourcePicker;
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
        createMenuLabel("Source → " + daddys_home::address(destination, true)));
    auto* field = new DaddysHomeSourceField;
    field->moduleId = module->id;
    field->destination = destination;
    field->box.size.x = 170;
    field->text = daddys_home::address(module->routing[destination].load());
    field->selectAll();
    menu->addChild(field);
    menu->addChild(createMenuLabel("a1-p16 / X = blank"));
    APP->event->setSelectedWidget(field);
  }
  void onButton(const event::Button& e) override {
    widget::OpaqueWidget::onButton(e);
  }
  void handleCellButton(const event::Button& e) {
    int cell =
        daddys_home::matrixCell(e.pos.x, e.pos.y, box.size.x, box.size.y);
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
    int changed = daddys_home::draggedSource(dragBefore, dragSteps);
    if (module->routing[dragDestination].load() != changed)
      module->markManualEdit(dragDestination);
    module->routing[dragDestination].store(changed);
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
        daddysHomeEdit(module, dragDestination, dragBefore, after);
    }
    dragDestination = -1;
  }
  DaddysHomeDisplay() {
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
      hoverTooltip->text = daddys_home::address(routing[hoverDestination]) +
                           " → " + daddys_home::address(hoverDestination, true);
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
    float textSize = daddys_home::sourceFontSize(
        box.size.x, nvgTextBounds(args.vg, 0, 0, "m16", nullptr, nullptr));
    nvgFontSize(args.vg, textSize);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    for (int port = 0; port < 16; ++port)
      for (int ch = 0; ch < 16; ++ch) {
        int dest = port * 16 + ch;
        bool knobs = module && module->knobMode;
        float editFade = module && module->lastManualDestination == dest
                             ? clamp(1.f - float(system::getTime() -
                                                 module->lastManualEditTime) /
                                               2.5f,
                                     0.f, 1.f)
                             : 0.f;
        if ((!knobs && hoverDestination == dest) || editFade > 0.f) {
          nvgBeginPath(args.vg);
          nvgRect(
              args.vg, 5.f + ch * daddys_home::sourceColumnWidth(box.size.x),
              port * box.size.y / 16.f,
              daddys_home::sourceColumnWidth(box.size.x), box.size.y / 16.f);
          NVGcolor color = editFade > 0.f ? COLOR_COMPUTERSCARE_YELLOW
                                          : COLOR_COMPUTERSCARE_GREEN;
          color.a = editFade > 0.f ? .2f * editFade : .16f;
          nvgFillColor(args.vg, color);
          nvgFill(args.vg);
        }
        Vec center(daddys_home::sourceColumnX(box.size.x, ch),
                   (port + .5f) * box.size.y / 16.f);
        if (knobs)
          daddys_home::drawRouteKnob(args.vg, center, 21.f, routing[dest],
                                     available.route(routing, dest), dest + 1);
        nvgFontSize(args.vg, textSize);
        auto label = daddys_home::address(routing[dest]);
        nvgFillColor(args.vg, available.route(routing, dest)
                                  ? COLOR_COMPUTERSCARE_LIGHT_GREEN
                              : knobs ? nvgRGB(154, 180, 169)
                                      : nvgRGB(79, 93, 89));
        nvgText(args.vg, daddys_home::sourceColumnX(box.size.x, ch),
                (port + .5f) * box.size.y / 16.f, label.c_str(), nullptr);
      }
    nvgRestore(args.vg);
    widget::Widget::draw(args);
  }
};
struct DaddysHomeMappedCell : app::ParamWidget {
  DaddysHomeDisplay* display = nullptr;
  unsigned pendingPickerFrames = 0;
  void step() override {
    app::ParamWidget::step();
    // MIDI learn consumes touchedParam after ButtonEvent, on deselect or its
    // next UI step. Allow a complete UI pass before opening a custom editor.
    if (pendingPickerFrames && --pendingPickerFrames == 0 && module &&
        !display->module->knobMode &&
        APP->scene->rack->getTouchedParam() == this)
      display->openPicker(paramId - ComputerscareDaddysHome::ROUTE);
  }
  void onDoubleClick(const event::DoubleClick& e) override {
    if (display->module && display->module->knobMode)
      app::ParamWidget::onDoubleClick(e);
    else
      e.consume(this);
  }
  void onEnter(const event::Enter&) override {}
  void onHover(const event::Hover& e) override {
    display->updateHover(e.pos.plus(box.pos));
    e.consume(this);
  }
  void onLeave(const event::Leave&) override {
    display->hoverDestination = -1;
    display->clearHoverTip();
  }
  void onButton(const event::Button& e) override {
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT &&
        module) {
      APP->scene->rack->setTouchedParam(this);
      if (!display->module->knobMode) {
        pendingPickerFrames = 2;
        e.consume(this);
        return;
      }
    }
    auto forwarded = e;
    forwarded.pos = e.pos.plus(box.pos);
    display->handleCellButton(forwarded);
    e.consume(this);
  }
  void onDragStart(const event::DragStart& e) override {
    display->onDragStart(e);
  }
  void onDragMove(const event::DragMove& e) override { display->onDragMove(e); }
  void onDragEnd(const event::DragEnd& e) override { display->onDragEnd(e); }
};

struct DaddysHomeJackLabels : widget::TransparentWidget {
  void draw(const DrawArgs& args) override {
    auto font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/DejaVuSansMono.ttf"));
    if (!font) return;
    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 13.f);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(args.vg, BLACK);
    for (int port = 0; port < 16; ++port) {
      float y = 55.f + (port + .5f) * 320.f / 16.f;
      auto input = std::string(1, 'a' + port),
           output = std::string(1, 'A' + port);
      nvgText(args.vg, 44.f, y, input.c_str(), nullptr);
      nvgText(args.vg, 473.f, y, output.c_str(), nullptr);
    }
  }
};

struct DaddysHomeBackground : widget::TransparentWidget {
  void draw(const DrawArgs& args) override {
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
    nvgFillColor(args.vg, nvgRGB(190, 190, 190));
    nvgFill(args.vg);
  }
};

struct DaddysHomeRouteChange : history::ModuleAction {
  daddys_home::Routing before{}, after{};
  void apply(const daddys_home::Routing& values) {
    auto* router = dynamic_cast<ComputerscareDaddysHome*>(
        APP->engine->getModule(moduleId));
    if (router)
      for (int i = 0; i < 256; ++i)
        if (before[i] != after[i]) router->routing[i].store(values[i]);
  }
  void undo() override { apply(before); }
  void redo() override { apply(after); }
};

struct DaddysHomeModeButton : phlooper::WarpedDropdown {
  ComputerscareDaddysHome* module = nullptr;
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

struct DaddysHomeActionButton : computerscare::RandomBlock::Button {
  float factor = 1.f;
  DaddysHomeActionButton(bool wiggle, std::string scope, float size = 1.f)
      : Button(wiggle, scope), factor(size) {
    box.size = box.size.mult(factor);
  }
  void draw(const DrawArgs& args) override {
    nvgSave(args.vg);
    nvgScale(args.vg, factor, factor);
    auto footprint = box.size;
    box.size = Vec(14, 14);
    Button::draw(args);
    box.size = footprint;
    nvgRestore(args.vg);
  }
};
struct ComputerscareDaddysHomeWidget : ModuleWidget {
  void randomizeRouting(bool wiggle = false, int scope = 0, int index = 0) {
    auto* router = dynamic_cast<ComputerscareDaddysHome*>(module);
    if (!router) return;
    auto before = router->snapshotRouting();
    router->randomizeRouting(wiggle, scope, index);
    auto after = router->snapshotRouting();
    if (before == after) return;
    auto* change = new DaddysHomeRouteChange;
    change->name =
        wiggle ? "Wiggle Daddy's Home routes" : "Randomize Daddy's Home routes";
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
    auto* router = dynamic_cast<ComputerscareDaddysHome*>(module);
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
                      static_cast<daddys_home::RandomMode>(i);
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
                                         [ComputerscareDaddysHome::OUTPUT_POLY +
                                          port]
                                     .getValue() == count;
                        },
                        [router, port, count]() {
                          int id = ComputerscareDaddysHome::OUTPUT_POLY + port;
                          auto* h = new history::ParamChange;
                          h->name = "Daddy's Home output polyphony";
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
  explicit ComputerscareDaddysHomeWidget(ComputerscareDaddysHome* module) {
    setModule(module);
    box.size = Vec(525, 380);
    auto* bg = new DaddysHomeBackground;
    bg->box.size = box.size;
    addChild(bg);
    auto* titleBlock = new phlooper::WarpedBlock;
    titleBlock->configure(Rect(Vec(4, 0), Vec(194, 27)), 3, 1.2f, 701,
                          nvgRGB(212, 212, 212));
    addChild(titleBlock);
    for (int side = 0; side < 2; ++side) {
      auto* block = new phlooper::WarpedBlock;
      block->configure(
          Rect(Vec(side ? 431 : 5, 53), Vec(side ? 76 : 54, 326)), 3, 1.5f,
          702 + side,
          nvgRGB(side ? 170 : 220, side ? 170 : 220, side ? 170 : 220));
      addChild(block);
    }
    auto* panel = new ComputerscareSVGPanel;
    panel->setBackground(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/panels/ComputerscareDaddysHomePanel.svg")));
    addChild(panel);
    auto* mode = new DaddysHomeModeButton;
    mode->module = module;
    mode->configure(54, 21, 3, 2, .8f, 718);
    mode->textSize = 12;
    mode->box.pos = Vec(206, 4);
    addChild(mode);
    auto* display = new DaddysHomeDisplay;
    display->module = module;
    for (int dest = 0; dest < 256; ++dest) {
      auto* cell = createParam<DaddysHomeMappedCell>(
          Vec(5.f + (dest % 16) * daddys_home::sourceColumnWidth(356.f),
              (dest / 16) * 20.f),
          module, ComputerscareDaddysHome::ROUTE + dest);
      cell->display = display;
      cell->box.size = Vec(daddys_home::sourceColumnWidth(356.f), 20.f);
      display->addChild(cell);
    }
    display->box = Rect(Vec(69, 55), Vec(356, 320));
    addChild(display);
    for (int i = 0; i < 3; ++i)
      addInput(createInputCentered<JackSmallPointingDown>(
          Vec(269 + i * 17, 17), module, ComputerscareDaddysHome::CLOCK + i));
    addOutput(createOutputCentered<JackSmallPointingUp>(
        Vec(320, 17), module, ComputerscareDaddysHome::EOC));
    addParam(createParamCentered<NumberDotKnob>(
        Vec(342, 17), module, ComputerscareDaddysHome::EDIT_SCENE));
    auto* stepsKnob = createParamCentered<NumberDotKnob>(
        Vec(365, 17), module, ComputerscareDaddysHome::STEPS);
    stepsKnob->previewValue = 16;
    addParam(stepsKnob);
    auto* active = new DaddysHomeSceneDisplay;
    active->router = module;
    active->box = Rect(Vec(378, 7), Vec(18, 20));
    addChild(active);
    auto* poly = createParam<DaddysHomePolyKnob>(
        Vec(405, 4), module, ComputerscareDaddysHome::POLY_EDIT);
    poly->router = module;
    poly->previewChannels = 16;
    addParam(poly);
    auto* count = new DaddysHomePolyDisplay(Vec(398, 1));
    count->router = module;
    addChild(count);
    auto actionButton = [&](Vec pos, bool wiggle, int scope, int index,
                            float size = 1.f) {
      std::string scopeName = scope == 0 ? "all routes"
                              : scope == 1
                                  ? "output " + std::string(1, 'A' + index)
                                  : "channel " + std::to_string(index + 1);
      auto* button = new DaddysHomeActionButton(wiggle, scopeName, size);
      button->box.pos = pos;
      button->action = [this, wiggle, scope, index]() {
        randomizeRouting(wiggle, scope, index);
      };
      addChild(button);
    };
    for (int i = 0; i < 16; ++i) {
      float x = 69.f + daddys_home::sourceColumnX(356.f, i);
      actionButton(Vec(x - 10.5f, 35), false, 2, i, .7f);
      actionButton(Vec(x + .5f, 35), true, 2, i, .7f);
      float y = 55.f + (i + .5f) * 20.f;
      actionButton(Vec(434, y - 7), false, 1, i);
      actionButton(Vec(449, y - 7), true, 1, i);
    }
    for (int kind = 0; kind < 2; ++kind) {
      float y = 15.f + kind * 24.f;
      actionButton(Vec(424, y - 7), kind == 1, 0, 0);
      for (int scope = 0; scope < 3; ++scope)
        addInput(createInputCentered<JackSmallPointingDown>(
            Vec(445 + scope * 16.f, y), module,
            ComputerscareDaddysHome::RANDOM_ALL + kind * 3 + scope));
      addParam(createParamCentered<SmallKnob>(
          Vec(498, y), module,
          kind ? ComputerscareDaddysHome::WIGGLE_PROBABILITY
               : ComputerscareDaddysHome::RANDOM_PROBABILITY));
      addInput(createInputCentered<JackSmallPointingDown>(
          Vec(517, y), module,
          kind ? ComputerscareDaddysHome::WIGGLE_PROB_CV
               : ComputerscareDaddysHome::RANDOM_PROB_CV));
    }
    auto* labels = new DaddysHomeJackLabels;
    labels->box.size = box.size;
    addChild(labels);
    for (int i = 0; i < 16; ++i) {
      float y = 55.f + (i + .5f) * 320.f / 16.f;
      addInput(
          createInputCentered<JackSmallPointingDown>(Vec(23, y), module, i));
      addOutput(
          createOutputCentered<JackSmallPointingUp>(Vec(493, y), module, i));
    }
  }
};

Model* modelComputerscareDaddysHome =
    createModel<ComputerscareDaddysHome, ComputerscareDaddysHomeWidget>(
        "computerscare-daddys-home");

// Keep existing development patches loadable; new instances use the new slug.
Model* modelComputerscareDaddysHomeLegacy =
    createModel<ComputerscareDaddysHome, ComputerscareDaddysHomeWidget>(
        "computerscare-xrouter");
