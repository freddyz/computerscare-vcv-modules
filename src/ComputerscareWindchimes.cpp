#include <atomic>
#include <functional>

#include "Computerscare.hpp"
#include "Windchimes/Engine.hpp"
#include "Windchimes/Presets.hpp"

namespace wc = windchimes;

struct ComputerscareWindchimes : Module {
  enum GlobalParam {
    WIND,
    GUSTINESS,
    TURBULENCE,
    WIND_MIX,
    MASTER,
    GUST,
    SET_BASE
  };
  enum SetParam {
    ENABLED,
    X,
    Y,
    TUBES,
    MATERIAL,
    SCALE,
    ROOT,
    OCTAVE,
    FINE,
    DIVISIONS,
    DECAY,
    BRIGHTNESS,
    HARDNESS,
    LEVEL,
    LEGACY_SPREAD,  // Reserved so saved patches retain their parameter IDs.
    SWING,
    SET_PARAMS,
    SHAPE = SET_PARAMS,
    BODY,
    INHARMONICITY,
    SPREAD
  };
  // Append controls after the existing set parameters to preserve saved
  // patches.
  enum EffectParam {
    REVERB_MIX = SET_BASE + wc::maxSets * SET_PARAMS,
    REVERB_SIZE,
    WIND_TONE,
    WIND_TEXTURE,
    TIMBRE_BASE,
    INHARM_BASE = TIMBRE_BASE + wc::maxSets * 2,
    SPREAD_BASE = INHARM_BASE + wc::maxSets,
    NUM_PARAMS = SPREAD_BASE + wc::maxSets
  };
  enum Input { WIND_INPUT, TRANSPOSE_INPUT, GUST_INPUT, INPUTS };
  enum Output { LEFT_OUTPUT, RIGHT_OUTPUT, WIND_OUTPUT, OUTPUTS };
  static constexpr int param(int set, int field) {
    return field == SPREAD          ? SPREAD_BASE + set
           : field == INHARMONICITY ? INHARM_BASE + set
           : field >= SET_PARAMS    ? TIMBRE_BASE + set * 2 + field - SHAPE
                                    : SET_BASE + set * SET_PARAMS + field;
  }
  wc::Engine engine;
  dsp::SchmittTrigger gustTrigger, gustButton;
  std::atomic<uint32_t> strikeRequests{0}, stopRequests{0};
  std::array<std::atomic<float>, wc::maxSets> visualStrikerAngle{},
      visualStrikerFlash{}, visualStrikerDepth{}, visualSailX{}, visualSailY{},
      visualSailZ{};
  std::array<std::atomic<float>, wc::maxSets * wc::maxTubes> visualTubeAngle{},
      visualTubeFlash{}, visualTubeDepth{};
  std::atomic<float> visualWind{0.f};
  int refresh = 0;
  float sampleRate = 0.f, amount = 0.f, gustiness = 0.f, turbulence = 0.f,
        windMix = 0.f;
  float master = 0.8f, smoothMaster = 0.8f;

  ComputerscareWindchimes() {
    config(NUM_PARAMS, INPUTS, OUTPUTS, 0);
    configParam(WIND, 0.f, 1.f, 0.5f, "Wind strength", "%", 0.f, 100.f);
    configParam(GUSTINESS, 0.f, 1.f, 0.55f, "Gustiness", "%", 0.f, 100.f);
    configParam(TURBULENCE, 0.f, 1.f, 0.35f, "Turbulence", "%", 0.f, 100.f);
    configParam(WIND_MIX, 0.f, 1.f, 0.12f, "Audible wind", "%", 0.f, 100.f);
    configParam(MASTER, 0.f, 1.f, 0.8f, "Output level", "%", 0.f, 100.f);
    configButton(GUST, "Make a gust");
    configParam(REVERB_MIX, 0.f, 1.f, 0.2f, "Reverb dry/wet", "%", 0.f, 100.f);
    configParam(REVERB_SIZE, 0.f, 1.f, 0.55f, "Reverb size", "%", 0.f, 100.f);
    configParam(WIND_TONE, 0.f, 1.f, 0.45f, "Wind tone", "%", 0.f, 100.f);
    getParamQuantity(WIND_TONE)->description =
        "Deep rumble to bright, airy wind";
    configParam(WIND_TEXTURE, 0.f, 1.f, 0.4f, "Wind texture", "%", 0.f, 100.f);
    getParamQuantity(WIND_TEXTURE)->description =
        "Smooth air to rustling, breathy howls";
    for (int i = 0; i < wc::maxSets; ++i) {
      std::string name = "Set " + std::to_string(i + 1) + " ";
      configSwitch(param(i, ENABLED), 0.f, 1.f, i == 0 ? 1.f : 0.f,
                   name + "enabled", {"Off", "On"});
      configParam(param(i, X), 0.f, 1.f, 0.25f + (i % 3) * 0.25f, name + "pan");
      configParam(param(i, Y), 0.f, 1.f, 0.35f + (i % 2) * 0.3f,
                  name + "nearness");
      configParam(param(i, TUBES), 1.f, 12.f, 6.f, name + "tubes")
          ->snapEnabled = true;
      configSwitch(param(i, MATERIAL), 0.f, 2.f, 0.f, name + "material",
                   {"Metal", "Wood / bamboo", "Plastic"});
      std::vector<std::string> scaleLabels;
      for (const char* label : wc::scaleNames) scaleLabels.push_back(label);
      configSwitch(param(i, SCALE), 0.f, wc::scaleCount - 1.f, 0.f,
                   name + "scale", scaleLabels);
      configParam(param(i, SPREAD), 1.f, 4.f, 1.f, name + "scale-degree spread")
          ->snapEnabled = true;
      getParamQuantity(param(i, SPREAD))->description =
          "1: consecutive notes; 2: skip one; 3: skip two; 4: skip three";
      configSwitch(
          param(i, ROOT), 0.f, 11.f, 0.f, name + "root",
          {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"});
      configParam(param(i, OCTAVE), -2.f, 2.f, 0.f, name + "register", " oct")
          ->snapEnabled = true;
      configParam(param(i, FINE), -100.f, 100.f, 0.f, name + "fine tuning",
                  " cents");
      configParam(param(i, DIVISIONS), 5.f, 24.f, 12.f,
                  name + "equal divisions per octave")
          ->snapEnabled = true;
      configParam(param(i, DECAY), 0.f, 1.f, 0.55f, name + "decay");
      configParam(param(i, BRIGHTNESS), 0.f, 1.f, 0.55f, name + "brightness");
      configParam(param(i, HARDNESS), 0.f, 1.f, 0.5f,
                  name + "striker hardness");
      configParam(param(i, LEVEL), 0.f, 1.f, 0.7f, name + "level");
      configParam(param(i, LEGACY_SPREAD), 0.f, 1.f, 0.6f,
                  "Unused legacy spread")
          ->randomizeEnabled = false;
      configParam(param(i, SWING), 0.f, 1.f, 0.5f, name + "swing");
      configParam(param(i, SHAPE), 0.f, 1.f, 0.35f, name + "shape", "%", 0.f,
                  100.f);
      getParamQuantity(param(i, SHAPE))->description =
          "Continuous morph: solid bar, block, open shell, hollow tube";
      configParam(param(i, BODY), 0.f, 1.f, 0.65f, name + "body", "%", 0.f,
                  100.f);
      getParamQuantity(param(i, BODY))->description =
          "Dry contact and clack to full resonant body";
      getParamQuantity(param(i, DECAY))->description =
          "Wood: natural sustain through 85%, extended decay to 12 s above; "
          "metal 0.12–24 s, "
          "plastic 0.07–16 s";
      configParam(param(i, INHARMONICITY), 0.f, 1.f, 0.5f,
                  name + "inharmonicity", "%", 0.f, 100.f);
      getParamQuantity(param(i, INHARMONICITY))->description =
          "Harmonic spacing through natural body modes (50%) to exaggerated "
          "spacing";
      // Scene placement should survive Rack's global Randomize action.
      for (int field : {ENABLED, X, Y})
        getParamQuantity(param(i, field))->randomizeEnabled = false;
    }
    configInput(WIND_INPUT, "Wind strength (0–10 V, added to breeze)");
    configInput(TRANSPOSE_INPUT, "Transpose (1 V/oct)");
    configInput(GUST_INPUT, "Gust trigger");
    configOutput(LEFT_OUTPUT, "Left / mono");
    configOutput(RIGHT_OUTPUT, "Right");
    configOutput(WIND_OUTPUT, "Wind strength (0–10 V)");
  }
  wc::SetConfig readSet(int i) {
    auto value = [this, i](int f) { return params[param(i, f)].getValue(); };
    wc::SetConfig c;
    c.enabled = value(ENABLED) > 0.5f;
    c.x = value(X);
    c.y = value(Y);
    c.tubes =
        std::max(1, std::min(12, static_cast<int>(std::round(value(TUBES)))));
    c.material = static_cast<int>(std::round(value(MATERIAL)));
    c.scale = static_cast<int>(std::round(value(SCALE)));
    c.spread = static_cast<int>(std::round(value(SPREAD)));
    c.root = static_cast<int>(std::round(value(ROOT)));
    c.octave = value(OCTAVE);
    c.fine = value(FINE);
    c.divisions = static_cast<int>(std::round(value(DIVISIONS)));
    c.decay = value(DECAY);
    c.brightness = value(BRIGHTNESS);
    c.hardness = value(HARDNESS);
    c.level = value(LEVEL);
    c.swing = value(SWING);
    c.shape = value(SHAPE);
    c.body = value(BODY);
    c.inharmonicity = value(INHARMONICITY);
    return c;
  }
  void process(const ProcessArgs& args) override {
    if (sampleRate != args.sampleRate) {
      sampleRate = args.sampleRate;
      engine.setSampleRate(sampleRate);
      refresh = 0;
    }
    bool publishVisual = refresh <= 0;
    if (refresh-- <= 0) {
      refresh = std::max(1, static_cast<int>(sampleRate / 200.f)) - 1;
      float transpose =
          wc::clamp(inputs[TRANSPOSE_INPUT].getVoltage(), -5.f, 5.f);
      for (int i = 0; i < wc::maxSets; ++i)
        engine.configure(i, readSet(i), transpose);
      amount = wc::clamp(
          params[WIND].getValue() + inputs[WIND_INPUT].getVoltage() / 10.f, 0.f,
          1.f);
      gustiness = params[GUSTINESS].getValue();
      turbulence = params[TURBULENCE].getValue();
      engine.configureEffects(
          params[REVERB_MIX].getValue(), params[REVERB_SIZE].getValue(),
          params[WIND_TONE].getValue(), params[WIND_TEXTURE].getValue());
      windMix = params[WIND_MIX].getValue();
      master = params[MASTER].getValue();
    }
    bool externalGust = gustTrigger.process(inputs[GUST_INPUT].getVoltage());
    bool manualGust = gustButton.process(params[GUST].getValue());
    if (externalGust || manualGust) engine.gust();
    uint32_t stops = stopRequests.exchange(0, std::memory_order_relaxed);
    for (int i = 0; stops && i < wc::maxSets; ++i)
      if (stops & (1u << i)) engine.stop(i);
    uint32_t requests =
        strikeRequests.exchange(0, std::memory_order_relaxed) & ~stops;
    for (int i = 0; requests && i < wc::maxSets; ++i)
      if (requests & (1u << i)) engine.strike(i);
    wc::Stereo out = engine.process(amount, gustiness, turbulence, windMix);
    // Publish after advancing physics and exciting audio from the same
    // contacts.
    if (publishVisual) {
      for (int i = 0; i < wc::maxSets; ++i) {
        wc::Point sail = engine.sailAxis(i);
        visualSailX[i].store(sail.x, std::memory_order_relaxed);
        visualSailY[i].store(sail.y, std::memory_order_relaxed);
        visualSailZ[i].store(sail.z, std::memory_order_relaxed);
        visualStrikerDepth[i].store(engine.strikerDepth(i),
                                    std::memory_order_relaxed);
        visualStrikerAngle[i].store(engine.strikerAngle(i),
                                    std::memory_order_relaxed);
        visualStrikerFlash[i].store(engine.strikerFlash(i),
                                    std::memory_order_relaxed);
        for (int t = 0; t < wc::maxTubes; ++t) {
          visualTubeDepth[i * wc::maxTubes + t].store(
              engine.tubeDepth(i, t), std::memory_order_relaxed);
          visualTubeAngle[i * wc::maxTubes + t].store(
              engine.tubeAngle(i, t), std::memory_order_relaxed);
          visualTubeFlash[i * wc::maxTubes + t].store(
              engine.tubeFlash(i, t), std::memory_order_relaxed);
        }
      }
      visualWind.store(engine.windStrength(), std::memory_order_relaxed);
    }
    smoothMaster +=
        (master - smoothMaster) * std::min(1.f, args.sampleTime * 100.f);
    // A bounded soft limiter leaves headroom for dense, simultaneous impacts.
    auto voltage = [this](float v) {
      float x = v * smoothMaster;
      return 5.f * x / (1.f + std::fabs(x));
    };
    if (!outputs[RIGHT_OUTPUT].isConnected())
      outputs[LEFT_OUTPUT].setVoltage(
          voltage((out.left + out.right) * 0.70710678f));
    else
      outputs[LEFT_OUTPUT].setVoltage(voltage(out.left));
    outputs[RIGHT_OUTPUT].setVoltage(voltage(out.right));
    outputs[WIND_OUTPUT].setVoltage(
        wc::clamp(engine.windStrength() * 10.f, 0.f, 10.f));
  }
};

namespace {
using W = ComputerscareWindchimes;
const char* materials[] = {"Metal", "Wood / bamboo", "Plastic"};
const char* materialCaptions[] = {"Metal", "Wood", "Plastic"};
constexpr float controlColumnOffset = 180.f;
constexpr float panelWidth = 780.f;
struct Editor {
  bool perspective = true;
  int selected = 0;
};
void text(NVGcontext* vg, float x, float y, const std::string& label,
          float size = 11.f, NVGcolor color = nvgRGB(186, 216, 209)) {
  auto font = APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
  if (!font) return;
  nvgFontFaceId(vg, font->handle);
  nvgFontSize(vg, size);
  nvgFillColor(vg, color);
  nvgText(vg, x, y, label.c_str(), nullptr);
}
void setWithHistory(W* module, int id, float value,
                    history::ComplexAction* action) {
  auto* quantity = module->getParamQuantity(id);
  float old = quantity->getValue();
  quantity->setValue(value);
  auto* change = new history::ParamChange;
  change->moduleId = module->id;
  change->paramId = id;
  change->oldValue = old;
  change->newValue = quantity->getValue();
  action->push(change);
}
bool setEnabled(W* module, int selected) {
  return module &&
         module->params[W::param(selected, W::ENABLED)].getValue() > 0.5f;
}
int vacantSet(W* module) {
  if (module)
    for (int i = 0; i < wc::maxSets; ++i)
      if (!setEnabled(module, i)) return i;
  return -1;
}
void editSound(W* module, int selected, bool wiggle,
               history::ComplexAction* action) {
  if (wiggle) {
    int fields[] = {W::DECAY, W::BRIGHTNESS, W::HARDNESS,
                    W::SHAPE, W::BODY,       W::INHARMONICITY};
    // Shuffle without replacement so each chosen control changes once.
    for (int i = 5; i > 0; --i) {
      int j = std::min(static_cast<int>(random::uniform() * (i + 1)), i);
      std::swap(fields[i], fields[j]);
    }
    int count = 1 + std::min(static_cast<int>(random::uniform() * 6.f), 5);
    for (int i = 0; i < count; ++i) {
      int id = W::param(selected, fields[i]);
      auto* quantity = module->getParamQuantity(id);
      float low = quantity->getMinValue(), high = quantity->getMaxValue();
      float delta = (0.01f + random::uniform() * 0.04f) * (high - low);
      float value =
          quantity->getValue() + (random::uniform() < 0.5f ? -delta : delta);
      // Reflect at the limits so an outward nudge still changes the knob.
      if (value < low) value = low + (low - value);
      if (value > high) value = high - (value - high);
      setWithHistory(module, id, value, action);
    }
  } else {
    for (int field : {W::DECAY, W::BRIGHTNESS, W::HARDNESS, W::SHAPE, W::BODY,
                      W::INHARMONICITY})
      setWithHistory(module, W::param(selected, field), random::uniform(),
                     action);
  }
}
void changeSound(W* module, int selected, bool wiggle) {
  if (!setEnabled(module, selected)) return;
  auto* action = new history::ComplexAction;
  action->name =
      wiggle ? "Wiggle windchime sound" : "Randomize windchime sound";
  editSound(module, selected, wiggle, action);
  APP->history->push(action);
}
void removeSet(W* module, Editor* editor, int selected) {
  if (!setEnabled(module, selected)) return;
  auto* action = new history::ComplexAction;
  action->name = "Remove windchime set";
  setWithHistory(module, W::param(selected, W::ENABLED), 0.f, action);
  APP->history->push(action);
  if (editor->selected == selected)
    for (int i = 0; i < wc::maxSets; ++i)
      if (setEnabled(module, i)) {
        editor->selected = i;
        break;
      }
}
void copySet(W* module, Editor* editor, int source, bool divide) {
  if (!setEnabled(module, source)) return;
  int target = vacantSet(module);
  if (target < 0) return;
  auto* action = new history::ComplexAction;
  action->name = divide ? "Divide windchime set" : "Duplicate windchime set";
  // Include appended timbre controls and reserved fields; enable only after
  // configuring the complete copy. No motion state or global settings are
  // copied.
  for (int field = W::X; field <= W::SPREAD; ++field)
    setWithHistory(module, W::param(target, field),
                   module->params[W::param(source, field)].getValue(), action);
  // Place the copy nearby so both sets can immediately be selected and moved.
  for (int field : {W::X, W::Y}) {
    float old = module->params[W::param(source, field)].getValue();
    float offset = field == W::X ? 0.13f : 0.05f;
    setWithHistory(module, W::param(target, field),
                   old + (old + offset <= 1.f ? offset : -offset), action);
  }
  if (divide) editSound(module, target, true, action);
  setWithHistory(module, W::param(target, W::ENABLED), 1.f, action);
  APP->history->push(action);
  editor->selected = target;
}
struct ActionButton : ComputerscareBlankButton {
  std::function<std::string()> label;
  std::function<void()> action;
  Vec nativeSize = box.size;
  bool pressed = false;
  bool selector = false;
  void setPressed(bool down) {
    if (pressed == down) return;
    pressed = down;
    sw->setSvg(frames[pressed ? 1 : 0]);
    fb->setDirty();
  }
  void draw(const DrawArgs& args) override {
    nvgSave(args.vg);
    nvgScale(args.vg, box.size.x / nativeSize.x, box.size.y / nativeSize.y);
    ComputerscareBlankButton::draw(args);
    nvgRestore(args.vg);
    auto font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/Oswald-Regular.ttf"));
    if (!font) return;
    nvgFontFaceId(args.vg, font->handle);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    std::string caption = label();
    float fontSize = selector ? 15.f : 11.f;
    nvgFontSize(args.vg, fontSize);
    float width =
        nvgTextBounds(args.vg, 0, 0, caption.c_str(), nullptr, nullptr);
    float available = box.size.x - 16.f;
    if (selector && width > available) {
      while (!caption.empty() &&
             nvgTextBounds(args.vg, 0, 0, (caption + "..").c_str(), nullptr,
                           nullptr) > available)
        caption.pop_back();
      caption += "..";
    } else if (!selector && width > available) {
      nvgFontSize(args.vg, fontSize * available / width);
    }
    nvgFillColor(args.vg, nvgRGB(20, 39, 35));
    float dx =
        pressed ? (selector ? 1.5f : 3.6f * box.size.x / nativeSize.x) : 0.f;
    float dy =
        pressed ? (selector ? 1.5f : 2.9f * box.size.y / nativeSize.y) : 0.f;
    nvgText(args.vg, box.size.x * 0.5f + dx, box.size.y * 0.47f + dy,
            caption.c_str(), nullptr);
  }
  void onButton(const event::Button& e) override {
    if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS) {
      setPressed(true);
      action();
      e.consume(this);
    } else if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_RELEASE) {
      setPressed(false);
      e.consume(this);
    } else
      ComputerscareBlankButton::onButton(e);
  }
  void onDragEnd(const event::DragEnd& e) override { setPressed(false); }
  void onLeave(const event::Leave& e) override {
    setPressed(false);
    ComputerscareBlankButton::onLeave(e);
  }
};
// Follow the selector behavior used by VolyPector: menu lifetime, rather
// than mouse release, determines the depressed frame and text position.
struct SelectorButton : ActionButton {
  WeakPtr<ui::MenuOverlay> activeMenuOverlay;
  std::function<Menu*()> openMenu;
  SelectorButton() { selector = true; }
  bool menuOpen() {
    auto* overlay = activeMenuOverlay.get();
    return overlay && !overlay->requestedDelete;
  }
  void step() override {
    ComputerscareBlankButton::step();
    setPressed(menuOpen());
  }
  void draw(const DrawArgs& args) override {
    setPressed(menuOpen());
    ActionButton::draw(args);
  }
  void onButton(const event::Button& e) override {
    if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS) {
      e.consume(this);
      Menu* menu = openMenu();
      if (menu) activeMenuOverlay = menu->getAncestorOfType<ui::MenuOverlay>();
      setPressed(menuOpen());
    }
  }
  void onDragEnd(const event::DragEnd&) override { setPressed(menuOpen()); }
  void onLeave(const event::Leave&) override { setPressed(menuOpen()); }
};
// Keep the larger hit area and knob size using the plugin's existing artwork.
struct ChimeKnob : SmoothKnob {
  static constexpr float scale = 1.14f;
  ChimeKnob() { box.size = box.size.mult(scale); }
  void draw(const DrawArgs& args) override {
    nvgSave(args.vg);
    nvgScale(args.vg, scale, scale);
    SmoothKnob::draw(args);
    nvgRestore(args.vg);
  }
};
struct SetKnob : ChimeKnob {
  Editor* editor = nullptr;
  int field = 0;
  void step() override {
    if (module && editor) paramId = W::param(editor->selected, field);
    ChimeKnob::step();
  }
};
struct ChimeScene : widget::OpaqueWidget {
  W* module = nullptr;
  Editor* editor = nullptr;
  int dragging = -1;
  float oldX = 0.f, oldY = 0.f;
  float nearness(int i) const {
    return module ? module->params[W::param(i, W::Y)].getValue()
                  : 0.2f + i * 0.25f;
  }
  float visualScale(int i) const { return 0.65f + 0.55f * nearness(i); }
  Vec position(int i) {
    float pan =
        module ? module->params[W::param(i, W::X)].getValue() : 0.2f + i * 0.3f;
    return Vec(pan * box.size.x, 35.f + nearness(i) * (box.size.y - 90.f));
  }
  bool enabled(int i) const {
    return module ? module->params[W::param(i, W::ENABLED)].getValue() > 0.5f
                  : i < 3;
  }
  void draw(const DrawArgs& args) override {
    auto* vg = args.vg;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, 0, 0, box.size.x, box.size.y, 8);
    nvgFillColor(vg, nvgRGB(13, 29, 28));
    nvgFill(vg);
    for (int line = 1; line < 5; ++line) {
      nvgBeginPath(vg);
      nvgMoveTo(vg, line * box.size.x / 5, 18);
      nvgLineTo(vg, line * box.size.x / 5, box.size.y - 18);
      nvgStrokeColor(vg, nvgRGBA(100, 160, 140, 20));
      nvgStrokeWidth(vg, 1);
      nvgStroke(vg);
    }
    text(vg, 10, 16, "FAR", 9);
    text(vg, 10, box.size.y - 8, "NEAR", 9);
    nvgSave(vg);
    nvgIntersectScissor(vg, 0.f, 0.f, box.size.x, box.size.y);
    std::array<int, wc::maxSets> setOrder{};
    for (int i = 0; i < wc::maxSets; ++i) setOrder[i] = i;
    std::sort(setOrder.begin(), setOrder.end(),
              [this](int a, int b) { return nearness(a) < nearness(b); });
    for (int i : setOrder) {
      if (!enabled(i)) continue;
      Vec p = position(i);
      int tubes = module
                      ? static_cast<int>(std::round(
                            module->params[W::param(i, W::TUBES)].getValue()))
                      : 6;
      int material =
          module ? static_cast<int>(std::round(
                       module->params[W::param(i, W::MATERIAL)].getValue()))
                 : i;
      NVGcolor color = material == 0   ? nvgRGB(120, 207, 212)
                       : material == 1 ? nvgRGB(218, 168, 107)
                                       : nvgRGB(191, 153, 220);
      bool selected = editor && editor->selected == i;
      float scale = 48.f * visualScale(i);
      bool perspective = !editor || editor->perspective;
      auto point = [p, perspective, scale](wc::Point v) {
        return Vec(p.x + v.x * scale,
                   perspective ? p.y + v.y * scale + v.z * scale * 0.45f
                               : p.y - 8.f + v.z * scale);
      };
      auto line = [vg](Vec a, Vec b, NVGcolor c, float width) {
        nvgBeginPath(vg);
        nvgMoveTo(vg, a.x, a.y);
        nvgLineTo(vg, b.x, b.y);
        nvgStrokeColor(vg, c);
        nvgStrokeWidth(vg, width);
        nvgStroke(vg);
      };
      nvgBeginPath(vg);
      nvgRoundedRect(vg, p.x - 62 * visualScale(i), p.y - 46 * visualScale(i),
                     124 * visualScale(i), 84 * visualScale(i), 8);
      nvgFillColor(vg, selected ? nvgRGBA(218, 239, 162, 12)
                                : nvgRGBA(120, 170, 160, 5));
      nvgFill(vg);
      nvgStrokeColor(vg, selected ? nvgRGBA(218, 239, 162, 160)
                                  : nvgRGBA(120, 170, 160, 35));
      nvgStrokeWidth(vg, 1);
      nvgStroke(vg);
      Vec support = point({0.f, wc::pivotY, 0.f});
      nvgBeginPath(vg);
      for (int segment = 0; segment <= 48; ++segment) {
        float phase = segment * 2.f * wc::pi / 48.f;
        Vec v = point(
            {0.22f * std::cos(phase), wc::pivotY, 0.22f * std::sin(phase)});
        if (segment == 0)
          nvgMoveTo(vg, v.x, v.y);
        else
          nvgLineTo(vg, v.x, v.y);
      }
      nvgStrokeColor(vg, color);
      nvgStrokeWidth(vg, 2.f);
      nvgStroke(vg);
      float angle =
          module ? module->visualStrikerAngle[i].load(std::memory_order_relaxed)
                 : 0.f;
      float depth =
          module ? module->visualStrikerDepth[i].load(std::memory_order_relaxed)
                 : 0.f;
      wc::Point axis = wc::pendulumAxis(angle, depth);
      wc::Point center =
          wc::Point(0.f, wc::pivotY, 0.f) + axis * wc::strikerLength;
      std::array<int, wc::maxTubes + 1> order{};
      std::array<float, wc::maxTubes + 1> depths{};
      for (int t = 0; t < tubes; ++t) {
        order[t] = t;
        float a = module ? module->visualTubeAngle[i * wc::maxTubes + t].load(
                               std::memory_order_relaxed)
                         : 0.f;
        float d = module ? module->visualTubeDepth[i * wc::maxTubes + t].load(
                               std::memory_order_relaxed)
                         : 0.f;
        depths[t] = wc::tubeCapsule(t, tubes, a, d).center.z;
      }
      order[tubes] = tubes;
      depths[tubes] = center.z;
      std::sort(order.begin(), order.begin() + tubes + 1,
                [&](int a, int b) { return depths[a] < depths[b]; });
      for (int index = 0; index <= tubes; ++index) {
        int t = order[index];
        if (t == tubes) {
          float flash = module ? module->visualStrikerFlash[i].load(
                                     std::memory_order_relaxed)
                               : 0.f;
          Vec striker = point(center);
          line(support, striker, nvgRGBA(218, 227, 213, 180), 0.8f);
          nvgBeginPath(vg);
          nvgCircle(vg, striker.x, striker.y, wc::strikerRadius * scale);
          nvgFillColor(vg, nvgLerpRGBA(nvgRGB(218, 195, 144),
                                       nvgRGB(255, 250, 205), flash));
          nvgFill(vg);
          {
            wc::Point sailAxis = module
                                     ? wc::Point(module->visualSailX[i].load(
                                                     std::memory_order_relaxed),
                                                 module->visualSailY[i].load(
                                                     std::memory_order_relaxed),
                                                 module->visualSailZ[i].load(
                                                     std::memory_order_relaxed))
                                     : wc::Point(0.f, 1.f, 0.f);
            Vec sail = point(center + sailAxis * wc::sailLength);
            line(striker, sail, nvgRGBA(218, 227, 213, 160), 0.8f);
            nvgBeginPath(vg);
            nvgRoundedRect(vg, sail.x - 3, sail.y - 4, 6, 9, 1);
            nvgFillColor(vg, color);
            nvgFill(vg);
          }
        } else {
          float a = module ? module->visualTubeAngle[i * wc::maxTubes + t].load(
                                 std::memory_order_relaxed)
                           : 0.f;
          float d = module ? module->visualTubeDepth[i * wc::maxTubes + t].load(
                                 std::memory_order_relaxed)
                           : 0.f;
          float flash =
              module ? module->visualTubeFlash[i * wc::maxTubes + t].load(
                           std::memory_order_relaxed)
                     : 0.f;
          wc::Capsule body = wc::tubeCapsule(t, tubes, a, d);
          Vec top = point(body.top()), bottom = point(body.bottom());
          line(point(wc::tubeAnchorPoint(t, tubes)), top,
               nvgRGBA(205, 224, 217, 155), 0.8f);
          NVGcolor tubeColor = nvgLerpRGBA(color, nvgRGB(255, 250, 205), flash);
          nvgLineCap(vg, NVG_ROUND);
          line(top, bottom, tubeColor, body.radius * scale * 2.f);
          nvgLineCap(vg, NVG_BUTT);
          if (!perspective) {
            Vec v = point(body.center);
            nvgBeginPath(vg);
            nvgCircle(vg, v.x, v.y, body.radius * scale);
            nvgFillColor(vg, tubeColor);
            nvgFill(vg);
          }
        }
      }
      float numberX = wc::clamp(p.x - 62.f * visualScale(i) + 12.f, 10.f,
                                box.size.x - 10.f);
      float numberY = wc::clamp(p.y - 46.f * visualScale(i) + 11.f, 10.f,
                                box.size.y - 10.f);
      nvgBeginPath(vg);
      nvgRoundedRect(vg, numberX - 8.f, numberY - 7.f, 16.f, 14.f, 3.f);
      nvgFillColor(vg, nvgRGB(13, 29, 28));
      nvgFill(vg);
      text(vg, numberX - 3.f, numberY + 3.f, std::to_string(i + 1), 10, color);
    }
    nvgRestore(vg);
  }
  void onButton(const event::Button& e) override {
    if (!module || !editor) return;
    if (e.action == GLFW_PRESS && (e.button == GLFW_MOUSE_BUTTON_LEFT ||
                                   e.button == GLFW_MOUSE_BUTTON_RIGHT)) {
      std::array<int, wc::maxSets> order{};
      for (int i = 0; i < wc::maxSets; ++i) order[i] = i;
      std::sort(order.begin(), order.end(),
                [this](int a, int b) { return nearness(a) > nearness(b); });
      for (int i : order) {
        if (enabled(i) &&
            std::fabs(position(i).x - e.pos.x) < 62.f * visualScale(i) &&
            std::fabs(position(i).y - e.pos.y) < 46.f * visualScale(i)) {
          editor->selected = i;
          if (e.button == GLFW_MOUSE_BUTTON_RIGHT) {
            dragging = -1;
            auto* menu = createMenu();
            menu->addChild(
                createMenuLabel("Chime set " + std::to_string(i + 1)));
            menu->addChild(createMenuItem("Strike", "", [this, i]() {
              module->strikeRequests.fetch_or(1u << i,
                                              std::memory_order_relaxed);
            }));
            menu->addChild(createMenuItem("Stop", "", [this, i]() {
              module->stopRequests.fetch_or(1u << i, std::memory_order_relaxed);
            }));
            menu->addChild(createMenuItem("Randomize", "", [this, i]() {
              changeSound(module, i, false);
            }));
            menu->addChild(createMenuItem(
                "Wiggle", "", [this, i]() { changeSound(module, i, true); }));
            menu->addChild(new ui::MenuSeparator);
            bool full = vacantSet(module) < 0;
            menu->addChild(createMenuItem(
                "Duplicate", full ? "8/8 sets" : "",
                [this, i]() { copySet(module, editor, i, false); }, full));
            menu->addChild(createMenuItem(
                "Divide", full ? "8/8 sets" : "",
                [this, i]() { copySet(module, editor, i, true); }, full));
            menu->addChild(createMenuItem(
                "Remove", "", [this, i]() { removeSet(module, editor, i); }));
            e.consume(this);
            return;
          }
          dragging = i;
          oldX = module->params[W::param(i, W::X)].getValue();
          oldY = module->params[W::param(i, W::Y)].getValue();
          e.consume(this);
          return;
        }
      }
    }
    OpaqueWidget::onButton(e);
  }
  void removeActive() {
    if (module && editor) removeSet(module, editor, editor->selected);
  }
  void onSelect(const event::Select& e) override { e.consume(this); }
  void onSelectKey(const event::SelectKey& e) override {
    if ((e.mods & RACK_MOD_MASK) == 0 &&
        (e.key == GLFW_KEY_DELETE || e.key == GLFW_KEY_BACKSPACE)) {
      // Consume repeats/releases too: holding Delete removes only one set.
      if (e.action == GLFW_PRESS) removeActive();
      e.consume(this);
      return;
    }
    OpaqueWidget::onSelectKey(e);
  }
  void onDragMove(const event::DragMove& e) override {
    if (!module || dragging < 0 || e.button != GLFW_MOUSE_BUTTON_LEFT) return;
    Vec delta = e.mouseDelta.div(APP->scene->rackScroll->getZoom());
    for (int axis = 0; axis < 2; ++axis) {
      int id = W::param(dragging, axis == 0 ? W::X : W::Y);
      auto* q = module->getParamQuantity(id);
      q->setValue(q->getValue() + (axis == 0 ? delta.x / box.size.x
                                             : delta.y / (box.size.y - 90)));
    }
  }
  void onDragEnd(const event::DragEnd& e) override {
    if (!module || dragging < 0) return;
    auto* action = new history::ComplexAction;
    action->name = "Move windchime set";
    for (int axis = 0; axis < 2; ++axis) {
      auto* change = new history::ParamChange;
      change->moduleId = module->id;
      change->paramId = W::param(dragging, axis == 0 ? W::X : W::Y);
      change->oldValue = axis == 0 ? oldX : oldY;
      change->newValue = module->params[change->paramId].getValue();
      action->push(change);
    }
    APP->history->push(action);
    dragging = -1;
  }
  void onDoubleClick(const event::DoubleClick& e) override {
    if (module && editor) {
      module->strikeRequests.fetch_or(1u << editor->selected,
                                      std::memory_order_relaxed);
      e.consume(this);
    }
  }
};
struct Labels : widget::TransparentWidget {
  W* module = nullptr;
  Editor* editor = nullptr;
  void draw(const DrawArgs& args) override {
    auto* vg = args.vg;
    nvgSave(vg);
    nvgTranslate(vg, controlColumnOffset, 0.f);
    int selected = editor ? editor->selected : 0;
    bool enabled =
        module
            ? module->params[W::param(selected, W::ENABLED)].getValue() > 0.5f
            : true;
    const NVGcolor selectedColor = nvgRGB(218, 239, 162);
    const NVGcolor globalColor = nvgRGB(151, 208, 232);
    auto group = [vg](float x, float y, float w, float h, NVGcolor fill,
                      NVGcolor border) {
      nvgBeginPath(vg);
      nvgRoundedRect(vg, x, y, w, h, 5);
      nvgFillColor(vg, fill);
      nvgFill(vg);
      nvgStrokeColor(vg, border);
      nvgStrokeWidth(vg, 1);
      nvgStroke(vg);
    };
    group(328, 40, 267, 93, nvgRGB(44, 53, 35), nvgRGBA(218, 239, 162, 95));
    group(328, 138, 267, 94, nvgRGB(44, 53, 35), nvgRGBA(218, 239, 162, 95));
    group(328, 238, 267, 132, nvgRGB(27, 46, 61), nvgRGBA(151, 208, 232, 110));
    text(vg, 338, 50,
         enabled ? "SET " + std::to_string(selected + 1) + " — TUNING / MOTION"
                 : "SELECT OR ADD A SET",
         9, selectedColor);
    text(vg, 338, 148, "SET " + std::to_string(selected + 1) + " — SOUND", 9,
         selectedColor);
    const char* tuning[] = {"Tubes", "Root", "Register",
                            "Fine",  "EDO",  "Spread"};
    for (int i = 0; i < 6; ++i)
      text(vg, 335.f + i * 45.f, 128, tuning[i], 8, selectedColor);
    text(vg, 475, 93, "Set level", 8, selectedColor);
    text(vg, 539, 93, "Swing", 8, selectedColor);
    const char* sound[] = {"Decay", "Bright", "Hard",
                           "Shape", "Body",   "Inharm"};
    for (int i = 0; i < 6; ++i)
      text(vg, 335.f + i * 45.f, 228, sound[i], 8, selectedColor);
    text(vg, 338, 247, "GLOBAL — ALL SETS", 9, globalColor);
    const char* wind[] = {"Wind",     "Gusts", "Turb",
                          "Wind mix", "Tone",  "Texture"};
    for (int i = 0; i < 6; ++i)
      text(vg, 335.f + i * 45.f, 283, wind[i], 8, globalColor);
    const char* effects[] = {"Output", "Rev wet", "Size"};
    for (int i = 0; i < 3; ++i)
      text(vg, 335.f + i * 45.f, 325, effects[i], 8, globalColor);
    text(vg, 514, 325, "Make gust", 8, globalColor);
    const char* ports[] = {"WIND CV",  "1V/OCT",   "GUST",
                           "WIND OUT", "L / MONO", "R"};
    const float positions[] = {347, 392, 437, 482, 530, 568};
    for (int i = 0; i < 6; ++i)
      text(vg, positions[i] - (i == 5 ? 3.f : 18.f), 365, ports[i], 7);
    nvgRestore(vg);
  }
};
}  // namespace

struct ComputerscareWindchimesWidget : ModuleWidget {
  Editor editor;
  ChimeScene* scene = nullptr;
  void onHoverKey(const event::HoverKey& e) override {
    // Rack processes module deletion before passing keys to children. Handle
    // the scene first so an active/hovered chime cannot delete the whole
    // module.
    if (module && scene && scene->box.contains(e.pos) &&
        ((e.mods & RACK_MOD_MASK) == 0 &&
         (e.key == GLFW_KEY_DELETE || e.key == GLFW_KEY_BACKSPACE))) {
      if (e.action == GLFW_PRESS) scene->removeActive();
      e.consume(this);
      return;
    }
    ModuleWidget::onHoverKey(e);
  }
  ComputerscareWindchimesWidget(W* module) {
    setModule(module);
    box.size = Vec(panelWidth, RACK_GRID_HEIGHT);
    addChild(new BGPanel(nvgRGB(22, 42, 39)));
    children.back()->box.size = box.size;
    auto* labels = new Labels;
    labels->box.size = box.size;
    labels->module = module;
    labels->editor = &editor;
    addChild(labels);
    scene = new ChimeScene;
    scene->module = module;
    scene->editor = &editor;
    scene->box = Rect(Vec(5, 5), Vec(315.f + controlColumnOffset, 370));
    addChild(scene);
    auto button = [this](Vec pos, float width,
                         std::function<std::string()> label,
                         std::function<void()> action) {
      auto* b = new ActionButton;
      b->box = Rect(pos, Vec(width, 23));
      b->label = std::move(label);
      b->action = std::move(action);
      addChild(b);
    };
    auto selector = [this](Vec pos, float width,
                           std::function<std::string()> label,
                           std::function<Menu*()> openMenu) {
      auto* b = new SelectorButton;
      b->box = Rect(pos, Vec(width, 23));
      b->label = std::move(label);
      b->openMenu = std::move(openMenu);
      addChild(b);
    };
    button(
        Vec(328, 10), 88,
        [module]() {
          int count = 0;
          if (module)
            for (int i = 0; i < wc::maxSets; ++i)
              count +=
                  module->params[W::param(i, W::ENABLED)].getValue() > 0.5f;
          return "+ Add (" + std::to_string(count) + "/8)";
        },
        [this, module]() {
          if (!module) return;
          for (int i = 0; i < wc::maxSets; ++i)
            if (module->params[W::param(i, W::ENABLED)].getValue() < 0.5f) {
              auto* action = new history::ComplexAction;
              action->name = "Add windchime set";
              for (int f = 0; f < W::SET_PARAMS; ++f)
                setWithHistory(
                    module, W::param(i, f),
                    module->getParamQuantity(W::param(i, f))->getDefaultValue(),
                    action);
              for (int f : {W::SHAPE, W::BODY, W::INHARMONICITY, W::SPREAD})
                setWithHistory(
                    module, W::param(i, f),
                    module->getParamQuantity(W::param(i, f))->getDefaultValue(),
                    action);
              setWithHistory(module, W::param(i, W::ENABLED), 1.f, action);
              APP->history->push(action);
              editor.selected = i;
              break;
            }
        });
    button(
        Vec(420, 10), 60, []() { return "Remove"; },
        [this, module]() { removeSet(module, &editor, editor.selected); });
    button(
        Vec(428, 139), 47, []() { return "Strike"; },
        [this, module]() {
          if (module)
            module->strikeRequests.fetch_or(1u << editor.selected,
                                            std::memory_order_relaxed);
        });
    button(
        Vec(551, 10), 38, [this]() { return editor.perspective ? "3D" : "2D"; },
        [this]() { editor.perspective = !editor.perspective; });
    for (int field : {W::MATERIAL, W::SCALE}) {
      selector(
          Vec(field == W::MATERIAL ? 338 : 338,
              field == W::MATERIAL ? 163 : 57),
          field == W::MATERIAL ? 78 : 110,
          [this, module, field]() {
            int value =
                module ? static_cast<int>(std::round(
                             module->params[W::param(editor.selected, field)]
                                 .getValue()))
                       : 0;
            return std::string(
                       field == W::MATERIAL
                           ? materialCaptions[std::max(0, std::min(value, 2))]
                           : wc::scaleCaptions[std::max(
                                 0, std::min(value, wc::scaleCount - 1))]) +
                   "  v";
          },
          [this, module, field]() -> Menu* {
            if (!module) return nullptr;
            auto* menu = createMenu();
            int selected = editor.selected;
            for (int i = 0; i < (field == W::MATERIAL ? 3 : wc::scaleCount);
                 ++i)
              menu->addChild(createCheckMenuItem(
                  field == W::MATERIAL ? materials[i] : wc::scaleNames[i], "",
                  [module, selected, field, i]() {
                    return std::round(module->params[W::param(selected, field)]
                                          .getValue()) == i;
                  },
                  [module, selected, field, i]() {
                    auto* action = new history::ComplexAction;
                    action->name = "Edit windchime set";
                    setWithHistory(module, W::param(selected, field), i,
                                   action);
                    APP->history->push(action);
                  }));
            return menu;
          });
    }
    button(
        Vec(480, 139), 52, []() { return "Wiggle"; },
        [this, module]() { changeSound(module, editor.selected, true); });
    button(
        Vec(538, 139), 47, []() { return "Random"; },
        [this, module]() { changeSound(module, editor.selected, false); });
    selector(
        Vec(426, 163), 130,
        [this, module]() {
          if (!module) return std::string("Preset  v");
          int selected = editor.selected;
          int material = static_cast<int>(std::round(
              module->params[W::param(selected, W::MATERIAL)].getValue()));
          auto presets = wc::soundPresets(material);
          for (int i = 0; i < presets.count; ++i) {
            const auto& p = presets.items[i];
            const float values[] = {p.decay, p.brightness, p.hardness,
                                    p.shape, p.body,       p.inharmonicity};
            const int fields[] = {W::DECAY, W::BRIGHTNESS, W::HARDNESS,
                                  W::SHAPE, W::BODY,       W::INHARMONICITY};
            bool matches = true;
            for (int f = 0; f < 6; ++f)
              matches =
                  matches &&
                  std::fabs(
                      module->params[W::param(selected, fields[f])].getValue() -
                      values[f]) < 0.0001f;
            if (matches) {
              std::string caption = p.name;
              if (caption == "Resonant wood block") caption = "Wood block";
              if (caption == "Split bamboo clack") caption = "Bamboo clack";
              if (caption == "Hard lumber knock") caption = "Lumber knock";
              if (caption == "Dry bamboo rattle") caption = "Bamboo rattle";
              if (caption == "Warm suspended bar") caption = "Warm bar";
              if (caption == "Solid plastic knock") caption = "Plastic knock";
              if (caption == "Soft plastic block") caption = "Soft block";
              return caption + "  v";
            }
          }
          return std::string("Custom  v");
        },
        [this, module]() -> Menu* {
          if (!module ||
              module->params[W::param(editor.selected, W::ENABLED)].getValue() <
                  0.5f)
            return nullptr;
          int selected = editor.selected;
          auto presets = wc::soundPresets(static_cast<int>(std::round(
              module->params[W::param(selected, W::MATERIAL)].getValue())));
          auto* menu = createMenu();
          menu->addChild(createMenuLabel("Sound presets — selected material"));
          for (int i = 0; i < presets.count; ++i) {
            wc::SoundPreset preset = presets.items[i];
            menu->addChild(
                createMenuItem(preset.name, "", [module, selected, preset]() {
                  auto* action = new history::ComplexAction;
                  action->name = "Apply windchime sound preset";
                  const int fields[] = {W::DECAY,    W::BRIGHTNESS,
                                        W::HARDNESS, W::SHAPE,
                                        W::BODY,     W::INHARMONICITY};
                  const float values[] = {
                      preset.decay, preset.brightness, preset.hardness,
                      preset.shape, preset.body,       preset.inharmonicity};
                  for (int f = 0; f < 6; ++f)
                    setWithHistory(module, W::param(selected, fields[f]),
                                   values[f], action);
                  APP->history->push(action);
                }));
          }
          return menu;
        });
    const int fields[] = {W::TUBES,     W::ROOT,   W::OCTAVE, W::FINE,
                          W::DIVISIONS, W::SPREAD, W::DECAY,  W::BRIGHTNESS,
                          W::HARDNESS,  W::SHAPE,  W::BODY,   W::INHARMONICITY,
                          W::LEVEL,     W::SWING};
    for (int i = 0; i < 14; ++i) {
      auto* knob =
          createParamCentered<SetKnob>(Vec(i < 6    ? 349.f + i * 45.f
                                           : i < 12 ? 349.f + (i - 6) * 45.f
                                                    : 493.f + (i - 12) * 62.f,
                                           i < 6    ? 107.f
                                           : i < 12 ? 205.f
                                                    : 72.f),
                                       module, W::param(0, fields[i]));
      knob->editor = &editor;
      knob->field = fields[i];
      addParam(knob);
    }
    const int globals[] = {W::WIND,     W::GUSTINESS,  W::TURBULENCE,
                           W::WIND_MIX, W::WIND_TONE,  W::WIND_TEXTURE,
                           W::MASTER,   W::REVERB_MIX, W::REVERB_SIZE};
    for (int i = 0; i < 9; ++i)
      addParam(createParamCentered<ChimeKnob>(
          Vec(i < 6 ? 349.f + i * 45.f : 349.f + (i - 6) * 45.f,
              i < 6 ? 262.f : 304.f),
          module, globals[i]));
    addParam(createParamCentered<ComputerscareBlankButton>(Vec(539, 304),
                                                           module, W::GUST));
    for (int i = 0; i < 3; ++i)
      addInput(
          createInputCentered<InPort>(Vec(347.f + i * 45.f, 345.f), module, i));
    addOutput(
        createOutputCentered<OutPort>(Vec(482, 345), module, W::WIND_OUTPUT));
    addOutput(
        createOutputCentered<OutPort>(Vec(530, 345), module, W::LEFT_OUTPUT));
    addOutput(
        createOutputCentered<OutPort>(Vec(568, 345), module, W::RIGHT_OUTPUT));
    // Reserve the extra module width entirely for the scene. Keep the existing
    // compact control-column layout and translate its widgets together.
    for (auto* child : children)
      if (child != children.front() && child != labels && child != scene)
        child->box.pos.x += controlColumnOffset;
  }
};
Model* modelComputerscareWindchimes =
    createModel<ComputerscareWindchimes, ComputerscareWindchimesWidget>(
        "computerscare-windchimes");
