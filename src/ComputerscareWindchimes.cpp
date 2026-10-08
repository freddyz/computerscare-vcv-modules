#include <atomic>
#include <functional>

#include "Computerscare.hpp"
#include "Windchimes/Delay.hpp"
#include "Windchimes/Engine.hpp"
#include "Windchimes/Presets.hpp"
#include "Windchimes/Selection.hpp"

namespace wc = windchimes;

struct DelayTimeQuantity : ParamQuantity {
  std::string getDisplayValueString() override;
};

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
    SPREAD,
    STRIKER_WEIGHT,
    SAIL_SIZE,
    MUTE,
    SOLO
  };
  // Keep the original eight slots and global IDs fixed. New chimes use
  // complete parameter blocks appended after every existing control.
  enum EffectParam {
    REVERB_MIX = SET_BASE + 8 * SET_PARAMS,
    REVERB_SIZE,
    WIND_TONE,
    WIND_TEXTURE,
    TIMBRE_BASE,
    INHARM_BASE = TIMBRE_BASE + 8 * 2,
    SPREAD_BASE = INHARM_BASE + 8,
    WEIGHT_BASE = SPREAD_BASE + 8,
    SAIL_BASE = WEIGHT_BASE + 8,
    DELAY_MIX = SAIL_BASE + 8,
    DELAY_TIME,
    DELAY_FEEDBACK,
    MUTE_BASE,
    SOLO_BASE = MUTE_BASE + 8,
    EXTRA_SET_BASE = SOLO_BASE + 8,
    NUM_PARAMS = EXTRA_SET_BASE + (wc::maxSets - 8) * (SOLO + 1)
  };
  enum Input { WIND_INPUT, TRANSPOSE_INPUT, GUST_INPUT, CLOCK_INPUT, INPUTS };
  enum Output {
    LEFT_OUTPUT,
    RIGHT_OUTPUT,
    WIND_OUTPUT,
    REAR_LEFT_OUTPUT,
    REAR_RIGHT_OUTPUT,
    WIND_AUDIO_OUTPUT,
    OUTPUTS
  };
  static constexpr int param(int set, int field) {
    return set >= 8        ? EXTRA_SET_BASE + (set - 8) * (SOLO + 1) + field
           : field == MUTE ? MUTE_BASE + set
           : field == SOLO ? SOLO_BASE + set
           : field == SAIL_SIZE      ? SAIL_BASE + set
           : field == STRIKER_WEIGHT ? WEIGHT_BASE + set
           : field == SPREAD         ? SPREAD_BASE + set
           : field == INHARMONICITY  ? INHARM_BASE + set
           : field >= SET_PARAMS     ? TIMBRE_BASE + set * 2 + field - SHAPE
                                     : SET_BASE + set * SET_PARAMS + field;
  }
  std::atomic<uint32_t> editSelection{1u};
  std::atomic<bool> excludeWindFromQuad{false};
  std::atomic<bool> gustRequest{false};
  wc::Engine engine;
  wc::Delay delay;
  std::atomic<bool> delaySynced{false};
  dsp::SchmittTrigger gustTrigger, gustButton;
  std::atomic<uint32_t> strikeRequests{0}, stopRequests{0};
  std::array<std::atomic<float>, wc::maxSets> visualStrikerAngle{},
      visualStrikerFlash{}, visualStrikerDepth{}, visualSailX{}, visualSailY{},
      visualSailZ{};
  std::array<std::atomic<float>, wc::maxSets * wc::maxTubes> visualTubeAngle{},
      visualTubeFlash{}, visualTubeDepth{};
  std::atomic<float> visualWind{0.f};
  std::array<std::atomic<float>, 20> visualFlowX{}, visualFlowY{};
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
    getParamQuantity(WIND_MIX)->description =
        "Wind level and presence: sparse passing breezes to fuller close air "
        "and buffeting";
    configParam(MASTER, 0.f, 1.f, 0.8f, "Output level", "%", 0.f, 100.f);
    configButton(GUST, "Make a gust");
    configParam(DELAY_MIX, 0.f, 1.f, 0.f, "Delay mix", "%", 0.f, 100.f);
    configParam<DelayTimeQuantity>(DELAY_TIME, 0.f, 1.f, 2.f / 3.f,
                                   "Delay time");
    getParamQuantity(DELAY_TIME)->description =
        "10 ms–2 s; with Clock: 1/8–4 clock periods, capped at 2 s. Tempo "
        "holds when clock stops.";
    configParam(DELAY_FEEDBACK, 0.f, .95f, .35f, "Delay feedback", "%", 0.f,
                100.f);
    configInput(CLOCK_INPUT, "Delay clock");
    configParam(REVERB_MIX, 0.f, 1.f, 0.2f, "Reverb dry/wet", "%", 0.f, 100.f);
    configParam(REVERB_SIZE, 0.f, 1.f, 0.55f, "Reverb size", "%", 0.f, 100.f);
    configParam(WIND_TONE, 0.f, 1.f, 0.45f, "Wind tone", "%", 0.f, 100.f);
    getParamQuantity(WIND_TONE)->description =
        "Deep buffeting through soft moving air to bright surface rustle";
    configParam(WIND_TEXTURE, 0.f, 1.f, 0.4f, "Wind texture", "%", 0.f, 100.f);
    getParamQuantity(WIND_TEXTURE)->description =
        "Surface activity: clustered rustle, fabric flutter and occasional "
        "whistles, continuously blending toward dry leaf rustle";
    for (int i = 0; i < wc::maxSets; ++i) {
      std::string name = "Chimes " + std::to_string(i + 1) + " ";
      configSwitch(param(i, ENABLED), 0.f, 1.f, i == 0 ? 1.f : 0.f,
                   name + "enabled", {"Off", "On"});
      configParam(param(i, X), 0.f, 1.f, 0.25f + (i % 3) * 0.25f,
                  name + "stage horizontal");
      configParam(param(i, Y), 0.f, 1.f, 0.35f + (i % 2) * 0.3f,
                  name + "stage front / rear");
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
      configParam(param(i, STRIKER_WEIGHT), 0.f, 1.f, .5f,
                  name + "striker weight", "%", 0.f, 100.f);
      getParamQuantity(param(i, STRIKER_WEIGHT))->description =
          "Light/reactive to heavy/inertial; 50% is the original striker mass";
      configSwitch(param(i, MUTE), 0.f, 1.f, 0.f, name + "mute", {"Off", "On"});
      configSwitch(param(i, SOLO), 0.f, 1.f, 0.f, name + "solo", {"Off", "On"});
      configParam(param(i, SAIL_SIZE), 0.f, 1.f, .5f, name + "sail size", "%",
                  0.f, 100.f);
      getParamQuantity(param(i, SAIL_SIZE))->description =
          "Wind-catching area; larger sails drive the suspension more readily "
          "without air braking";
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
    }
    configInput(WIND_INPUT, "Wind strength (0–10 V, added to breeze)");
    configInput(TRANSPOSE_INPUT, "Transpose (1 V/oct)");
    configInput(GUST_INPUT, "Gust trigger");
    configOutput(LEFT_OUTPUT, "Front left / automatic mixdown");
    configOutput(RIGHT_OUTPUT, "Front right / automatic mixdown");
    configOutput(REAR_LEFT_OUTPUT, "Rear left / automatic mixdown");
    configOutput(REAR_RIGHT_OUTPUT, "Rear right / automatic mixdown");
    configOutput(WIND_OUTPUT, "Wind strength (0–10 V)");
    configOutput(WIND_AUDIO_OUTPUT,
                 "Wind audio (mono, independent of Wind Mix)");
    // Standard Randomize shares the selected-sound button's sound and striker
    // controls. Global, tuning, material, swing and scene controls stay fixed.
    for (auto* quantity : paramQuantities) quantity->randomizeEnabled = false;
    for (int i = 0; i < wc::maxSets; ++i)
      for (int field : {DECAY, BRIGHTNESS, HARDNESS, STRIKER_WEIGHT, SHAPE,
                        BODY, INHARMONICITY})
        getParamQuantity(param(i, field))->randomizeEnabled = true;
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
    c.strikerWeight = value(STRIKER_WEIGHT);
    c.sailSize = value(SAIL_SIZE);
    c.level = value(LEVEL);
    c.swing = value(SWING);
    c.shape = value(SHAPE);
    c.body = value(BODY);
    c.inharmonicity = value(INHARMONICITY);
    return c;
  }
  void onRandomize(const RandomizeEvent&) override {
    uint32_t mask = editSelection.load(std::memory_order_relaxed);
    for (int i = 0; i < wc::maxSets; ++i)
      if ((mask & (1u << i)) && params[param(i, ENABLED)].getValue() > 0.5f)
        for (int field : {DECAY, BRIGHTNESS, HARDNESS, STRIKER_WEIGHT, SHAPE,
                          BODY, INHARMONICITY})
          getParamQuantity(param(i, field))->setValue(random::uniform());
  }
  json_t* dataToJson() override {
    json_t* data = json_object();
    json_object_set_new(data, "radialStageVersion", json_integer(1));
    json_object_set_new(
        data, "excludeWindFromQuad",
        json_boolean(excludeWindFromQuad.load(std::memory_order_relaxed)));
    return data;
  }
  void fromJson(json_t* root) override {
    Module::fromJson(root);
    json_t* data = json_object_get(root, "data");
    excludeWindFromQuad.store(
        data && json_is_true(json_object_get(data, "excludeWindFromQuad")),
        std::memory_order_relaxed);
    if (!data || !json_object_get(data, "radialStageVersion")) {
      for (int i = 0; i < wc::maxSets; ++i) {
        // Preserve old distance and horizontal direction in the front half.
        auto pos = wc::migrateStagePosition(params[param(i, X)].getValue(),
                                            params[param(i, Y)].getValue());
        params[param(i, X)].setValue(pos.x);
        params[param(i, Y)].setValue(pos.y);
      }
    }
  }
  void process(const ProcessArgs& args) override {
    if (sampleRate != args.sampleRate) {
      sampleRate = args.sampleRate;
      engine.setSampleRate(sampleRate);
      delay.setSampleRate(sampleRate);
      refresh = 0;
    }
    delay.clock(inputs[CLOCK_INPUT].getVoltage(),
                inputs[CLOCK_INPUT].isConnected());
    engine.configureWindRouting(
        excludeWindFromQuad.load(std::memory_order_relaxed),
        outputs[WIND_AUDIO_OUTPUT].isConnected());
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
          params[WIND_TONE].getValue(), params[WIND_TEXTURE].getValue(),
          params[WIND_MIX].getValue());
      windMix = params[WIND_MIX].getValue();
      master = params[MASTER].getValue();
      unsigned enabledMask = 0, soloMask = 0, muteMask = 0;
      for (int i = 0; i < wc::maxSets; ++i) {
        if (params[param(i, ENABLED)].getValue() > .5f) enabledMask |= 1u << i;
        if (params[param(i, SOLO)].getValue() > .5f) soloMask |= 1u << i;
        if (params[param(i, MUTE)].getValue() > .5f) muteMask |= 1u << i;
      }
      soloMask &= enabledMask;
      engine.setAudibleMask((soloMask ? soloMask : enabledMask) & ~muteMask);
      delaySynced.store(delay.synced(), std::memory_order_relaxed);
      delay.configure(params[DELAY_TIME].getValue(),
                      params[DELAY_MIX].getValue(),
                      params[DELAY_FEEDBACK].getValue());
    }
    bool externalGust = gustTrigger.process(inputs[GUST_INPUT].getVoltage());
    bool manualGust = gustButton.process(params[GUST].getValue());
    bool keyboardGust = gustRequest.exchange(false, std::memory_order_relaxed);
    if (externalGust || manualGust || keyboardGust) engine.gust();
    uint32_t stops = stopRequests.exchange(0, std::memory_order_relaxed);
    for (int i = 0; stops && i < wc::maxSets; ++i)
      if (stops & (1u << i)) engine.stop(i);
    uint32_t requests =
        strikeRequests.exchange(0, std::memory_order_relaxed) & ~stops;
    for (int i = 0; requests && i < wc::maxSets; ++i)
      if (requests & (1u << i)) engine.strike(i);
    const int audioIds[4] = {LEFT_OUTPUT, RIGHT_OUTPUT, REAR_LEFT_OUTPUT,
                             REAR_RIGHT_OUTPUT};
    unsigned mask = 0;
    for (int ch = 0; ch < 4; ++ch)
      if (outputs[audioIds[ch]].isConnected()) mask |= 1u << ch;
    engine.setOutputMask(mask);
    wc::Quad out =
        engine.processQuad(amount, gustiness, turbulence, windMix, true);
    out = delay.process(out, mask);
    for (int ch = 0; ch < 4; ++ch)
      out.channel[ch] += engine.windWithoutDelay().channel[ch];
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
      for (int cell = 0; cell < 20; ++cell) {
        auto flow = engine.windFlow((cell % 5) / 4.f, (cell / 5) / 3.f);
        visualFlowX[cell].store(flow.left, std::memory_order_relaxed);
        visualFlowY[cell].store(flow.right, std::memory_order_relaxed);
      }
    }
    smoothMaster +=
        (master - smoothMaster) * std::min(1.f, args.sampleTime * 100.f);
    // A bounded soft limiter leaves headroom for dense, simultaneous impacts.
    auto voltage = [this](float v) {
      // Bring normal scenes up to useful Rack levels before limiting.
      float x = v * smoothMaster * 3.f;
      return 5.f * x / (1.f + std::fabs(x));
    };
    for (int ch = 0; ch < 4; ++ch)
      outputs[audioIds[ch]].setVoltage(voltage(out.channel[ch]));
    outputs[WIND_AUDIO_OUTPUT].setVoltage(voltage(engine.windAudio()));
    outputs[WIND_OUTPUT].setVoltage(
        wc::clamp(engine.windStrength() * 10.f, 0.f, 10.f));
  }
};

std::string DelayTimeQuantity::getDisplayValueString() {
  auto* m = dynamic_cast<ComputerscareWindchimes*>(module);
  if (m && m->delaySynced.load(std::memory_order_relaxed))
    return string::f("%.3g × clock (max 2 s)", wc::Delay::ratio(getValue()));
  return string::f("%.3g ms", wc::Delay::seconds(getValue()) * 1000.f);
}

namespace {
struct SmallGustButton : ComputerscareBlankButton {
  Vec nativeSize = box.size;
  SmallGustButton() { box.size = Vec(12.f, 12.f); }
  void draw(const DrawArgs& args) override {
    nvgSave(args.vg);
    nvgScale(args.vg, box.size.x / nativeSize.x, box.size.y / nativeSize.y);
    ComputerscareBlankButton::draw(args);
    nvgRestore(args.vg);
  }
};
using W = ComputerscareWindchimes;
const char* materials[] = {"Metal", "Wood / bamboo", "Plastic"};
const char* materialCaptions[] = {"Metal", "Wood", "Plastic"};
constexpr float controlColumnOffset = 180.f;
constexpr float panelWidth = 900.f;
constexpr float controlSpacing = 1.45f;
float controlX(float x) { return 320.f + (x - 320.f) * controlSpacing; }
struct Editor : wc::Selection {
  bool perspective = true;
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
uint32_t enabledSelection(W* module) {
  uint32_t enabled = 0;
  for (int i = 0; i < wc::maxSets; ++i)
    if (setEnabled(module, i)) enabled |= 1u << i;
  return enabled;
}
uint32_t activeSelection(W* module, Editor* editor) {
  if (!module || !editor) return 0;
  editor->retain(enabledSelection(module));
  module->editSelection.store(editor->mask, std::memory_order_relaxed);
  return editor->mask;
}
void editSelectionField(W* module, uint32_t mask, int field, float value,
                        history::ComplexAction* action) {
  for (int i = 0; i < wc::maxSets; ++i)
    if ((mask & (1u << i)) && setEnabled(module, i))
      setWithHistory(module, W::param(i, field), value, action);
}
std::string selectionCaption(Editor* editor) {
  if (!editor || !editor->mask) return "Select chimes";
  int count = 0;
  for (int i = 0; i < wc::maxSets; ++i) count += editor->contains(i);
  return count > 1 ? std::to_string(count) + " Chimes"
                   : "Chimes " + std::to_string(editor->selected + 1);
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
    int fields[] = {W::DECAY, W::BRIGHTNESS, W::HARDNESS,     W::STRIKER_WEIGHT,
                    W::SHAPE, W::BODY,       W::INHARMONICITY};
    // Shuffle without replacement so each chosen control changes once.
    for (int i = 6; i > 0; --i) {
      int j = std::min(static_cast<int>(random::uniform() * (i + 1)), i);
      std::swap(fields[i], fields[j]);
    }
    int count = 1 + std::min(static_cast<int>(random::uniform() * 7.f), 6);
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
    for (int field : {W::DECAY, W::BRIGHTNESS, W::HARDNESS, W::STRIKER_WEIGHT,
                      W::SHAPE, W::BODY, W::INHARMONICITY})
      setWithHistory(module, W::param(selected, field), random::uniform(),
                     action);
  }
}
void changeSelectionSound(W* module, Editor* editor, bool wiggle) {
  uint32_t mask = activeSelection(module, editor);
  if (!mask) return;
  auto* action = new history::ComplexAction;
  action->name =
      wiggle ? "Wiggle selected windchimes" : "Randomize selected windchimes";
  for (int i = 0; i < wc::maxSets; ++i)
    if (mask & (1u << i)) editSound(module, i, wiggle, action);
  APP->history->push(action);
}
void removeSet(W* module, Editor* editor, int selected) {
  if (!setEnabled(module, selected)) return;
  auto* action = new history::ComplexAction;
  action->name = "Remove chimes";
  setWithHistory(module, W::param(selected, W::ENABLED), 0.f, action);
  APP->history->push(action);
  if (editor->selected == selected)
    for (int i = 0; i < wc::maxSets; ++i)
      if (setEnabled(module, i)) {
        editor->selectOnly(i);
        break;
      }
}
void copySet(W* module, Editor* editor, int source, bool divide) {
  if (!setEnabled(module, source)) return;
  int target = vacantSet(module);
  if (target < 0) return;
  auto* action = new history::ComplexAction;
  action->name = divide ? "Divide chimes" : "Duplicate chimes";
  // Include appended timbre controls and reserved fields; enable only after
  // configuring the complete copy. No motion state or global settings are
  // copied.
  for (int field = W::X; field <= W::SAIL_SIZE; ++field)
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
  editor->selectOnly(target);
}
void toggleSelectionFlag(W* module, Editor* editor, int field) {
  if (!module || !editor) return;
  uint32_t mask = activeSelection(module, editor);
  if (!mask) return;
  bool all = true;
  for (int i = 0; i < wc::maxSets; ++i)
    if (mask & (1u << i))
      all &= module->params[W::param(i, field)].getValue() > .5f;
  auto* action = new history::ComplexAction;
  action->name = field == W::SOLO ? "Solo chimes" : "Mute chimes";
  editSelectionField(module, mask, field, all ? 0.f : 1.f, action);
  APP->history->push(action);
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
    float fontSize = 10.f;
    nvgFontSize(args.vg, fontSize);
    float width =
        nvgTextBounds(args.vg, 0, 0, caption.c_str(), nullptr, nullptr);
    float available = box.size.x - 6.f;
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
    nvgText(args.vg, box.size.x * 0.5f - 2.f + dx,
            box.size.y * 0.47f - 1.3f + dy, caption.c_str(), nullptr);
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
struct GroupValueField : ui::TextField {
  std::function<void(const std::string&)> apply;
  void onAction(const event::Action& e) override {
    apply(text);
    if (auto* overlay = getAncestorOfType<ui::MenuOverlay>())
      overlay->requestDelete();
    e.consume(this);
  }
};
struct SetKnob : ChimeKnob {
  Editor* editor = nullptr;
  int field = 0;
  uint32_t dragMask = 0;
  std::array<float, wc::maxSets> before{};
  W* owner() { return dynamic_cast<W*>(module); }
  bool multiple() const {
    return editor && editor->mask && (editor->mask & (editor->mask - 1));
  }
  void step() override {
    if (module && editor) paramId = W::param(editor->selected, field);
    ChimeKnob::step();
  }
  void draw(const DrawArgs& args) override {
    ChimeKnob::draw(args);
    if (editor && !editor->mask) {
      nvgBeginPath(args.vg);
      nvgCircle(args.vg, box.size.x * .5f, box.size.y * .5f, box.size.x * .48f);
      nvgFillColor(args.vg, nvgRGBA(22, 42, 39, 160));
      nvgFill(args.vg);
      return;
    }
    if (!multiple() || !module) return;
    bool mixed = false;
    float primary = module->getParamQuantity(paramId)->getValue();
    for (int i = 0; i < wc::maxSets; ++i)
      if (editor->contains(i) &&
          std::fabs(module->getParamQuantity(W::param(i, field))->getValue() -
                    primary) > 0.00001f)
        mixed = true;
    if (!mixed) return;
    // Tint the knob face itself, leaving its indicator readable. Matching
    // selections use the original artwork without any group decoration.
    auto* vg = args.vg;
    float cx = box.size.x * 0.5f, cy = box.size.y * 0.5f;
    float radius = box.size.x * 0.4f;
    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, radius);
    nvgFillPaint(vg, nvgRadialGradient(vg, cx, cy, radius * 0.25f, radius,
                                       nvgRGBA(255, 188, 92, 105),
                                       nvgRGBA(255, 188, 92, 45)));
    nvgFill(vg);
  }
  void applyValue(float value, uint32_t mask) {
    auto* m = owner();
    if (!m || !mask) return;
    auto* action = new history::ComplexAction;
    action->name = "Edit selected windchimes";
    editSelectionField(m, mask, field, value, action);
    APP->history->push(action);
  }
  void onButton(const event::Button& e) override {
    uint32_t mask = activeSelection(owner(), editor);
    if (!mask) {
      e.consume(this);
      return;
    }
    paramId = W::param(editor->selected, field);
    if (e.button == GLFW_MOUSE_BUTTON_RIGHT && e.action == GLFW_PRESS &&
        multiple()) {
      auto* menu = createMenu();
      menu->addChild(createMenuLabel("Value for selected chimes"));
      auto* input = new GroupValueField;
      input->box.size.x = 160;
      input->setText(getParamQuantity()->getDisplayValueString());
      input->selectAll();
      input->apply = [this, mask](const std::string& value) {
        auto* q = getParamQuantity();
        if (!q) return;
        float old = q->getValue();
        q->setDisplayValueString(value);
        float next = q->getValue();
        q->setValue(old);
        applyValue(next, mask);
      };
      menu->addChild(input);
      menu->addChild(
          createMenuItem("Reset selected to default", "", [this, mask]() {
            applyValue(getParamQuantity()->getDefaultValue(), mask);
          }));
      e.consume(this);
      return;
    }
    ChimeKnob::onButton(e);
  }
  void onDoubleClick(const event::DoubleClick& e) override {
    uint32_t mask = activeSelection(owner(), editor);
    if (mask) applyValue(getParamQuantity()->getDefaultValue(), mask);
    e.consume(this);
  }
  void onHoverScroll(const event::HoverScroll& e) override {
    uint32_t mask = activeSelection(owner(), editor);
    if (!mask) return;
    auto* q = getParamQuantity();
    float old = q->getValue();
    ChimeKnob::onHoverScroll(e);
    float next = q->getValue();
    if (next != old) {
      q->setValue(old);
      applyValue(next, mask);
    }
  }
  void onLeave(const event::Leave& e) override {
    // Scroll edits already own their grouped history; suppress Rack's later
    // primary-only scroll entry while preserving tooltip/gesture cleanup.
    auto* m = module;
    module = nullptr;
    ChimeKnob::onLeave(e);
    module = m;
  }
  void onDragStart(const event::DragStart& e) override {
    dragMask = activeSelection(owner(), editor);
    if (module)
      for (int i = 0; i < wc::maxSets; ++i)
        before[i] = module->getParamQuantity(W::param(i, field))->getValue();
    if (dragMask) ChimeKnob::onDragStart(e);
  }
  void onDragMove(const event::DragMove& e) override {
    if (!dragMask || !module) return;
    float old = getParamQuantity()->getValue();
    ChimeKnob::onDragMove(e);
    float next = getParamQuantity()->getValue();
    if (next != old)
      for (int i = 0; i < wc::maxSets; ++i)
        if (dragMask & (1u << i))
          module->getParamQuantity(W::param(i, field))->setValue(next);
  }
  void onDragEnd(const event::DragEnd& e) override {
    if (!dragMask) return;
    // Let Rack release its cursor/gesture state without creating a separate
    // primary-only undo entry. The complete edit has one grouped history item.
    auto* m = module;
    module = nullptr;
    ChimeKnob::onDragEnd(e);
    module = m;
    if (m && dragMask) {
      auto* action = new history::ComplexAction;
      action->name = "Edit selected windchimes";
      bool changed = false;
      for (int i = 0; i < wc::maxSets; ++i)
        if (dragMask & (1u << i)) {
          int id = W::param(i, field);
          float next = m->getParamQuantity(id)->getValue();
          if (next == before[i]) continue;
          auto* change = new history::ParamChange;
          change->moduleId = m->id;
          change->paramId = id;
          change->oldValue = before[i];
          change->newValue = next;
          action->push(change);
          changed = true;
        }
      if (changed)
        APP->history->push(action);
      else
        delete action;
    }
    dragMask = 0;
  }
};
struct ChimeScene : widget::OpaqueWidget {
  W* module = nullptr;
  Editor* editor = nullptr;
  int dragging = -1;
  float oldX = 0.f, oldY = 0.f;
  std::array<Vec, 36> windStripes{};
  struct WindRibbon {
    std::array<Vec, 25> points{};
    float intensity = 0.f, phase = 0.f;
    double time = 0.;
    bool valid = false;
  };
  std::array<WindRibbon, 36> currentRibbons{};
  std::array<std::array<WindRibbon, 36>, 8> ribbonHistory{};
  unsigned historySlot = 0;
  double lastRibbonCapture = 0.;
  double lastWindDraw = 0.;
  float windColorPhase = 0.f;
  bool stripesInitialized = false;
  void drawWind(NVGcontext* vg) {
    if (!module) return;
    const double now = glfwGetTime();
    const float elapsed =
        lastWindDraw > 0. ? std::min(.05f, float(now - lastWindDraw)) : 0.f;
    lastWindDraw = now;
    windColorPhase =
        std::fmod(windColorPhase +
                      elapsed * (.2f + 1.5f * module->visualWind.load(
                                                  std::memory_order_relaxed)),
                  2.f * wc::pi);
    if (!stripesInitialized) {
      for (int i = 0; i < 36; ++i)
        windStripes[i] = Vec(((i % 6) + .5f) * box.size.x / 6.f,
                             ((i / 6) + .5f) * box.size.y / 6.f);
      stripesInitialized = true;
    }
    std::array<Vec, 20> field;
    for (int cell = 0; cell < 20; ++cell)
      field[cell] =
          Vec(module->visualFlowX[cell].load(std::memory_order_relaxed),
              module->visualFlowY[cell].load(std::memory_order_relaxed));
    auto flowAt = [&](Vec p) {
      float gx = wc::clamp(p.x / box.size.x, 0.f, 1.f) * 4.f;
      float gy = wc::clamp(p.y / box.size.y, 0.f, 1.f) * 3.f;
      int x = std::min(int(gx), 3), y = std::min(int(gy), 2);
      float tx = gx - x, ty = gy - y;
      return (field[y * 5 + x] * (1.f - tx) + field[y * 5 + x + 1] * tx) *
                 (1.f - ty) +
             (field[(y + 1) * 5 + x] * (1.f - tx) +
              field[(y + 1) * 5 + x + 1] * tx) *
                 ty;
    };
    nvgSave(vg);
    nvgIntersectScissor(vg, 0.f, 0.f, box.size.x, box.size.y);
    nvgLineCap(vg, NVG_ROUND);
    nvgLineJoin(vg, NVG_ROUND);
    for (int i = 0; i < 36; ++i) {
      Vec& head = windStripes[i];
      Vec flow = flowAt(head);
      head += flow * (elapsed * 85.f);
      head.x = std::fmod(head.x + box.size.x, box.size.x);
      head.y = std::fmod(head.y + box.size.y, box.size.y);
      auto& ribbon = currentRibbons[i];
      ribbon.intensity = wc::clamp(flow.norm(), 0.f, 1.f);
      ribbon.valid = ribbon.intensity >= .005f;
      ribbon.time = now;
      ribbon.phase = std::fmod(windColorPhase + i * .63f, 2.f * wc::pi);
      if (!ribbon.valid) continue;
      float length = std::hypot(box.size.x, box.size.y) * 1.8f;
      ribbon.points[0] =
          head + flow * (length * .5f / std::max(.02f, flow.norm()));
      for (int segment = 0; segment < 24; ++segment) {
        Vec local = flowAt(ribbon.points[segment]);
        ribbon.points[segment + 1] =
            ribbon.points[segment] -
            local * (length / (24.f * std::max(.02f, local.norm())));
      }
    }
    auto drawRibbon = [&](const WindRibbon& ribbon, float opacity) {
      if (!ribbon.valid || opacity < .001f) return;
      // One stroke for the entire curve: NanoVG handles joins once, avoiding
      // the doubled alpha from independently capped/antialiased segments.
      nvgBeginPath(vg);
      nvgMoveTo(vg, ribbon.points[0].x, ribbon.points[0].y);
      for (int segment = 1; segment <= 24; ++segment)
        nvgLineTo(vg, ribbon.points[segment].x, ribbon.points[segment].y);
      float along = (1.f - ribbon.phase / (2.f * wc::pi)) * 24.f;
      int index = std::min(23, int(along));
      Vec crest =
          ribbon.points[index] +
          (ribbon.points[index + 1] - ribbon.points[index]) * (along - index);
      float alpha = (20.f * std::sqrt(ribbon.intensity) +
                     35.f * ribbon.intensity * ribbon.intensity) *
                    opacity;
      nvgStrokePaint(
          vg, nvgRadialGradient(vg, crest.x, crest.y, 20.f, 210.f,
                                nvgRGBAf(130.f / 255.f, 185.f / 255.f,
                                         167.f / 255.f, alpha / 255.f),
                                nvgRGBAf(130.f / 255.f, 185.f / 255.f,
                                         167.f / 255.f, alpha * .12f / 255.f)));
      nvgStrokeWidth(vg, 2.f + 46.f * std::pow(ribbon.intensity, 2.5f));
      nvgStroke(vg);
    };
    // Fixed-size UI history, captured at 5 Hz; no framebuffers or audio work.
    for (unsigned offset = 0; offset < 8; ++offset) {
      const auto& snapshot = ribbonHistory[(historySlot + offset) % 8];
      for (const auto& ribbon : snapshot) {
        float lifetime = .15f + 1.5f * ribbon.intensity * ribbon.intensity;
        float fade =
            wc::clamp(1.f - float(now - ribbon.time) / lifetime, 0.f, 1.f);
        drawRibbon(ribbon, .16f * fade * fade);
      }
    }
    for (const auto& ribbon : currentRibbons) drawRibbon(ribbon, 1.f);
    if (now - lastRibbonCapture >= .2) {
      ribbonHistory[historySlot] = currentRibbons;
      historySlot = (historySlot + 1) % 8;
      lastRibbonCapture = now;
    }
    nvgRestore(vg);
  }

  Rect actionBox() {
    int i = editor ? editor->selected : 0;
    Vec p = position(i);
    float scale = visualScale(i);
    return Rect(
        Vec(wc::clamp(p.x - 62.f * scale + 3.f, 3.f, box.size.x - 60.f),
            wc::clamp(p.y + 46.f * scale - 15.f, 3.f, box.size.y - 15.f)),
        Vec(57.f, 12.f));
  }
  bool actionsVisible() const {
    return module && editor && editor->contains(editor->selected) &&
           enabled(editor->selected);
  }
  void runAction(int action) {
    if (action < 2)
      toggleSelectionFlag(module, editor, action == 0 ? W::SOLO : W::MUTE);
    else
      changeSelectionSound(module, editor, action == 2);
  }
  void drawActions(NVGcontext* vg) {
    if (!actionsVisible()) return;
    Rect area = actionBox();
    const char* labels[] = {"S", "M", "W", "R"};
    for (int k = 0; k < 4; ++k) {
      Vec p = area.pos + Vec(k * 15.f, 0.f);
      bool active =
          k < 2 &&
          module->params[W::param(editor->selected, k == 0 ? W::SOLO : W::MUTE)]
                  .getValue() > .5f;
      nvgBeginPath(vg);
      nvgRoundedRect(vg, p.x, p.y, 12.f, 12.f, 2.f);
      nvgFillColor(vg,
                   active ? nvgRGB(118, 117, 68) : nvgRGBA(28, 51, 45, 235));
      nvgFill(vg);
      nvgSave(vg);
      nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
      text(vg, p.x + 6.f, p.y + 6.f, labels[k], 8.f, nvgRGB(224, 236, 205));
      nvgRestore(vg);
    }
  }
  float stageValue(int i, int field) const {
    return module ? module->params[W::param(i, field)].getValue()
                  : (field == W::X ? 0.2f + i * 0.3f : 0.25f + i * 0.2f);
  }
  float nearness(int i) const {
    return 1.f - wc::stageDistance(stageValue(i, W::X), stageValue(i, W::Y));
  }
  float visualScale(int i) const { return 0.35f + 0.85f * nearness(i); }
  Vec position(int i) {
    return Vec(stageValue(i, W::X) * box.size.x,
               stageValue(i, W::Y) * box.size.y);
  }
  bool enabled(int i) const {
    return module ? module->params[W::param(i, W::ENABLED)].getValue() > 0.5f
                  : i < 3;
  }
  void draw(const DrawArgs& args) override {
    auto* vg = args.vg;
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, box.size.x, box.size.y);
    nvgFillColor(vg, nvgRGB(13, 29, 28));
    nvgFill(vg);
    drawWind(vg);
    for (int ring = 1; ring <= 3; ++ring) {
      nvgBeginPath(vg);
      nvgEllipse(vg, box.size.x * 0.5f, box.size.y * 0.5f,
                 box.size.x * ring / 6.f, box.size.y * ring / 6.f);
      nvgStrokeColor(vg, nvgRGBA(100, 160, 140, 35));
      nvgStrokeWidth(vg, 1.f);
      nvgStroke(vg);
    }
    text(vg, box.size.x * 0.5f - 17.f, 16, "FRONT", 9);
    text(vg, box.size.x * 0.5f - 14.f, box.size.y - 8.f, "REAR", 9);
    text(vg, 8.f, box.size.y * 0.5f, "FAR", 8);
    text(vg, box.size.x * 0.5f - 14.f, box.size.y * 0.5f, "NEAR", 8);
    nvgSave(vg);
    nvgIntersectScissor(vg, 0.f, 0.f, box.size.x, box.size.y);
    std::array<int, wc::maxSets> setOrder{};
    for (int i = 0; i < wc::maxSets; ++i) setOrder[i] = i;
    std::sort(setOrder.begin(), setOrder.end(),
              [this](int a, int b) { return nearness(a) < nearness(b); });
    std::array<std::function<void()>, wc::maxSets> drawSails;
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
      auto appearance = [this, i](int field, float fallback) {
        return module ? wc::clamp(module->params[W::param(i, field)].getValue(),
                                  0.f, 1.f)
                      : fallback;
      };
      const float shape = appearance(W::SHAPE, 0.35f + 0.25f * i);
      const float fullness = appearance(W::BODY, 0.65f);
      const float bright = appearance(W::BRIGHTNESS, 0.55f);
      const float decay = appearance(W::DECAY, 0.55f);
      const float inharm = appearance(W::INHARMONICITY, 0.5f);
      const float hard = appearance(W::HARDNESS, 0.5f);
      const float weight = appearance(W::STRIKER_WEIGHT, 0.5f);
      const float sailSize = appearance(W::SAIL_SIZE, .5f);
      // Material families remain recognizable; the finish mixes several
      // controls.
      NVGcolor accent = material == 0   ? nvgRGB(201, 166, 113)
                        : material == 1 ? nvgRGB(164, 93, 65)
                                        : nvgRGB(94, 189, 191);
      float tint = wc::clamp(
          .34f * shape + .26f * fullness + .22f * decay + .18f * weight, 0.f,
          1.f);
      color = nvgLerpRGBA(color, accent, .15f + .65f * tint);
      color = nvgLerpRGBA(color, nvgRGB(44, 52, 51),
                          .2f * (1.f - fullness) * (1.f - decay));
      bool selected = editor && editor->contains(i);
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
          const float radius = wc::strikerRadius * scale *
                               (1.35f - 0.55f * hard) * (.75f + .5f * weight);
          // A padded disk gradually becomes a firm, faceted puck.
          for (int vertex = 0; vertex < 32; ++vertex) {
            float phase = vertex * 2.f * wc::pi / 32.f;
            float sector =
                std::fmod(phase + wc::pi / 8.f, wc::pi / 4.f) - wc::pi / 8.f;
            float r =
                radius *
                (1.f - hard + hard * std::cos(wc::pi / 8.f) / std::cos(sector));
            Vec v(striker.x + r * std::cos(phase),
                  striker.y + r * std::sin(phase));
            if (vertex == 0)
              nvgMoveTo(vg, v.x, v.y);
            else
              nvgLineTo(vg, v.x, v.y);
          }
          nvgClosePath(vg);
          nvgFillColor(vg, nvgLerpRGBA(nvgRGB(218, 195, 144),
                                       nvgRGB(255, 250, 205), flash));
          nvgFill(vg);
          nvgStrokeColor(vg, nvgRGBA(255, 248, 222, 100));
          nvgStrokeWidth(vg, 0.6f + hard * 0.5f);
          nvgStroke(vg);
          nvgBeginPath(vg);
          nvgCircle(vg, striker.x, striker.y, radius * 0.65f);
          nvgStrokeColor(vg, nvgRGBA(74, 62, 45, int(150.f * (1.f - hard))));
          nvgStrokeWidth(vg, 1.f);
          nvgStroke(vg);
          drawSails[i] = [=]() {
            wc::Point sailAxis = module
                                     ? wc::Point(module->visualSailX[i].load(
                                                     std::memory_order_relaxed),
                                                 module->visualSailY[i].load(
                                                     std::memory_order_relaxed),
                                                 module->visualSailZ[i].load(
                                                     std::memory_order_relaxed))
                                     : wc::Point(0.f, 1.f, 0.f);
            Vec attachment = point(center + sailAxis * wc::sailLength);
            line(striker, attachment, nvgRGBA(218, 227, 213, 160), 0.8f);
            const float sailScale = visualScale(i) * (.5f + sailSize);
            const float wind =
                module ? module->visualWind.load(std::memory_order_relaxed)
                       : 0.f;
            // Each sail follows its own simulated lower link. Wind flutter is
            // decorative and fades with wind/motion; it never drives physics.
            const float lean = std::hypot(sailAxis.x, sailAxis.z);
            const float phase = float(std::fmod(glfwGetTime(), 10000.)) *
                                    (2.1f + .6f * bright + .17f * i) +
                                i * 2.399f + stageValue(i, W::X) * 3.f +
                                stageValue(i, W::Y) * 5.f;
            const float flutter =
                module ? std::min(1.f, wind * .65f + lean * .45f) : 0.f;
            const float yaw =
                perspective
                    ? .35f * std::sin(i * 2.399f) + 1.5f * sailAxis.x +
                          1.8f * sailAxis.z + .55f * flutter * std::sin(phase)
                    : .25f * std::sin(i * 2.399f) + .6f * sailAxis.x +
                          .6f * sailAxis.z + .1f * flutter * std::sin(phase);
            wc::Point side(std::cos(yaw), 0.f, std::sin(yaw));
            side = side - sailAxis * side.dot(sailAxis);
            side = side * (1.f / std::max(.001f, std::sqrt(side.dot(side))));
            Vec across = point(center + side * (8.f * sailScale / scale)) -
                         point(center);
            Vec along = point(center + sailAxis) - point(center);
            // The overhead view projects a vertical cord to a point; keep the
            // small icon readable there while retaining its direction of sway.
            // Always use this continuous orientation in 2D. Switching at a
            // projected-length threshold made the sail abruptly flip direction.
            if (!perspective)
              along = Vec(sailAxis.x * .6f, .65f + sailAxis.z * .25f);
            along = along.normalize() *
                    (12.f * sailScale * (1.f - .25f * std::fabs(sailAxis.z)));
            const Vec sail = attachment + along * .5f;
            // Metal: a faceted kite. Wood: a pointed leaf. Plastic: a ribbon
            // with a forked tail. Shape/body continuously change the profiles.
            const Vec profiles[3][8] = {
                {Vec(0, -.5f), Vec(.28f, -.27f), Vec(.5f, 0), Vec(.27f, .28f),
                 Vec(0, .5f), Vec(-.27f, .28f), Vec(-.5f, 0),
                 Vec(-.28f, -.27f)},
                {Vec(0, -.5f), Vec(.33f, -.3f), Vec(.5f, -.05f),
                 Vec(.32f, .27f), Vec(0, .5f), Vec(-.32f, .27f),
                 Vec(-.5f, -.05f), Vec(-.33f, -.3f)},
                {Vec(0, -.5f), Vec(.45f, -.4f), Vec(.5f, .05f), Vec(.42f, .5f),
                 Vec(0, .26f), Vec(-.42f, .5f), Vec(-.5f, .05f),
                 Vec(-.45f, -.4f)}};
            auto surface = [&](float u, float v) {
              float flex = (v + .5f) * (v + .5f) * flutter *
                           (.08f + .12f * shape) * std::sin(phase + v * 2.f);
              float breadth = .7f + .6f * fullness;
              return sail + across * (u * breadth + flex) + along * v;
            };
            std::array<Vec, 8> outline;
            for (int vertex = 0; vertex < 8; ++vertex) {
              Vec uv = profiles[std::max(0, std::min(2, material))][vertex];
              uv.x *= 1.f + (shape - .5f) * uv.y * .9f;
              outline[vertex] = surface(uv.x, uv.y);
            }
            Vec thickness((.6f + .5f * fullness) * sailScale * std::sin(yaw),
                          (.65f + .35f * fullness) * sailScale);
            auto silhouette = [&](Vec offset) {
              nvgBeginPath(vg);
              if (material == 1) {
                Vec top = outline[0] + offset, bottom = outline[4] + offset;
                Vec a = outline[1] + offset, b = outline[2] + offset;
                Vec c = outline[6] + offset, d = outline[7] + offset;
                nvgMoveTo(vg, top.x, top.y);
                nvgBezierTo(vg, a.x, a.y, b.x, b.y, bottom.x, bottom.y);
                nvgBezierTo(vg, c.x, c.y, d.x, d.y, top.x, top.y);
              } else {
                for (int vertex = 0; vertex < 8; ++vertex) {
                  Vec v = outline[vertex] + offset;
                  if (vertex == 0)
                    nvgMoveTo(vg, v.x, v.y);
                  else
                    nvgLineTo(vg, v.x, v.y);
                }
              }
              nvgClosePath(vg);
            };
            silhouette(thickness);
            nvgFillColor(vg, nvgLerpRGBA(color, nvgRGB(17, 34, 32), .55f));
            nvgFill(vg);
            silhouette(Vec(0, 0));
            float light = .12f + .2f * (.5f + .5f * std::cos(yaw));
            nvgFillColor(vg, nvgLerpRGBA(color, nvgRGB(240, 231, 194), light));
            nvgFill(vg);
            nvgStrokeColor(vg, nvgLerpRGBA(color, nvgRGB(242, 245, 222), .4f));
            nvgStrokeWidth(vg, .45f * sailScale);
            nvgStroke(vg);
            // A ridge/grain/fold makes the face readable as it turns.
            Vec ridge = surface(0.f, 0.f);
            line(outline[0], ridge, nvgRGBA(255, 245, 214, 130),
                 .55f * sailScale);
            line(ridge, outline[4], nvgRGBA(30, 52, 45, 130), .55f * sailScale);
            if (material == 0)
              line(outline[2], outline[6], nvgRGBA(255, 245, 214, 75),
                   .45f * sailScale);
            else if (material == 1)
              line(surface(-.12f, -.2f), surface(-.09f, .25f),
                   nvgRGBA(66, 43, 27, 100), .45f * sailScale);
          };
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
          // These profiles are decorative; contact geometry stays tied to the
          // published physics, so changing timbre never moves a contact point.
          float glow =
              std::pow(wc::clamp(flash, 0.f, 1.f), 1.6f - 1.2f * decay);
          NVGcolor finish = nvgLerpRGBA(
              nvgLerpRGBA(color, nvgRGB(37, 53, 49), 0.42f), color, bright);
          float variation = ((t * 7 + i * 3) % 5) * .25f;
          NVGcolor patina = material == 0   ? nvgRGB(74, 133, 162)
                            : material == 1 ? nvgRGB(237, 190, 110)
                                            : nvgRGB(226, 133, 154);
          finish = nvgLerpRGBA(
              finish, patina,
              (.08f + .24f * inharm) * variation * (.4f + .6f * hard));
          NVGcolor tubeColor = nvgLerpRGBA(finish, nvgRGB(255, 250, 205), glow);
          float block = 1.f - std::abs(shape - 0.33f) / 0.33f;
          block = wc::clamp(block, 0.f, 1.f);
          float width = body.radius * scale *
                        (material == 0   ? 2.f
                         : material == 1 ? 2.8f
                                         : 3.1f) *
                        (0.65f + 1.1f * fullness + 1.2f * block);
          float hollow = wc::clamp((shape - 0.45f) / 0.55f, 0.f, 1.f);
          // Anchor the decorative length at the suspension point. Decay and
          // Shape now change the silhouette even when the chimes are idle.
          float irregular = (t % 3 - 1) * 0.28f * inharm;
          float lengthScale = (0.65f + 0.75f * decay) *
                              (1.f - 0.35f * block + 0.18f * shape + irregular);
          bottom = Vec(top.x + (bottom.x - top.x) * lengthScale,
                       top.y + (bottom.y - top.y) * lengthScale);
          Vec mid = perspective ? Vec((top.x + bottom.x) * 0.5f,
                                      (top.y + bottom.y) * 0.5f)
                                : point(body.center);
          float dx = bottom.x - top.x, dy = bottom.y - top.y;
          float length = std::sqrt(dx * dx + dy * dy);
          // In the overhead view show a cross-section, including its rim.
          float rotation = perspective ? std::atan2(-dx, dy) : 0.f;
          float height = perspective ? std::max(length, width) : width;
          float corner = material == 0   ? 0.3f
                         : material == 1 ? 0.9f
                                         : width * 0.4f;
          corner += hollow * width * 0.15f;
          nvgSave(vg);
          nvgTranslate(vg, mid.x, mid.y);
          nvgRotate(vg, rotation);
          nvgBeginPath(vg);
          float taper = wc::clamp((.12f - shape) / .12f, 0.f, 1.f);
          float flare = wc::clamp((shape - .88f) / .12f, 0.f, 1.f);
          float waist = wc::clamp((.1f - fullness) / .1f, 0.f, 1.f);
          float bulge = wc::clamp((fullness - .92f) / .08f, 0.f, 1.f);
          float skew =
              wc::clamp((inharm - .9f) / .1f, 0.f, 1.f) * (t % 2 ? 1.f : -1.f);
          if (taper + flare + waist + bulge + std::fabs(skew) > .001f) {
            // Hand-cut/barrel/bell profiles appear only near parameter
            // extremes.
            float topWidth = width * .5f * (1.f - .45f * taper);
            float bottomWidth = width * .5f * (1.f + .3f * flare);
            float middleWidth =
                width * .5f * (1.f - .35f * waist + .22f * bulge);
            float offset = width * .12f * skew;
            float rounding = std::min(corner, width * .15f);
            nvgMoveTo(vg, -topWidth + rounding, -height * .5f);
            nvgLineTo(vg, topWidth - rounding, -height * .5f);
            nvgQuadTo(vg, topWidth, -height * .5f, topWidth,
                      -height * .5f + rounding);
            nvgBezierTo(vg, middleWidth + offset, -height * .23f,
                        middleWidth + offset, height * .23f, bottomWidth,
                        height * .5f - rounding);
            nvgQuadTo(vg, bottomWidth, height * .5f, bottomWidth - rounding,
                      height * .5f);
            nvgLineTo(vg, -bottomWidth + rounding, height * .5f);
            nvgQuadTo(vg, -bottomWidth, height * .5f, -bottomWidth,
                      height * .5f - rounding);
            nvgBezierTo(vg, -middleWidth + offset, height * .23f,
                        -middleWidth + offset, -height * .23f, -topWidth,
                        -height * .5f + rounding);
            nvgQuadTo(vg, -topWidth, -height * .5f, -topWidth + rounding,
                      -height * .5f);
            nvgClosePath(vg);
          } else
            nvgRoundedRect(vg, -width * .5f, -height * .5f, width, height,
                           corner);
          nvgFillColor(vg, tubeColor);
          nvgFill(vg);
          // An open shell has a visible recessed channel before becoming a
          // closed tube. Plastic retains a broad molded lip, wood a split.
          float shell =
              wc::clamp(1.f - std::abs(shape - 0.65f) / 0.22f, 0.f, 1.f);
          if (perspective && shell > 0.f) {
            nvgBeginPath(vg);
            nvgRoundedRect(vg, -width * 0.28f * shell, -height * 0.42f,
                           width * 0.56f * shell, height * 0.84f,
                           material == 2 ? width * 0.15f : 0.3f);
            nvgFillColor(vg, nvgRGBA(22, 36, 33, 190));
            nvgFill(vg);
          }
          if (glow > 0.01f) {
            nvgStrokeColor(vg, nvgRGBA(255, 250, 205, int(glow * 85.f)));
            nvgStrokeWidth(vg, 1.f + decay * 2.f);
            nvgStroke(vg);
          }
          NVGcolor detail = nvgLerpRGBA(
              nvgRGBA(28, 40, 37, 100), nvgRGBA(78, 45, 48, 205),
              wc::clamp(.45f * inharm + .35f * hard + .2f * (1.f - fullness),
                        0.f, 1.f));
          NVGcolor highlight =
              nvgRGBA(255, 255, 234, int(35.f + 150.f * bright));
          if (perspective) {
            // Metal highlight, wood grain, or a molded plastic seam.
            float edge = material == 0   ? -0.28f
                         : material == 1 ? -0.16f
                                         : 0.26f;
            line(Vec(width * edge, -height * 0.38f),
                 Vec(width * edge, height * 0.38f),
                 material == 1 ? detail : highlight, 0.65f);
            if (material == 1)
              line(Vec(width * 0.22f, -height * 0.2f),
                   Vec(width * 0.13f, height * 0.3f), detail, 0.55f);
            int bands = 1 + int((inharm * .6f + decay * .4f) * 3.f);
            for (int band = 0; band < bands; ++band) {
              float y = height * (-.32f + .64f * (band + .5f) / bands +
                                  (inharm - .5f) * .12f);
              line(Vec(-width * .4f * (1.f - .25f * waist), y),
                   Vec(width * .4f * (1.f - .25f * waist), y), detail,
                   material == 1 ? .6f + .7f * fullness : .4f + .45f * hard);
            }
            // Fine colored grain/anodizing/mold streaks share the timbre
            // finish.
            for (int stripe = 0; stripe < 3; ++stripe) {
              float x = width * (-.24f + .24f * stripe);
              NVGcolor streak = nvgLerpRGBA(
                  detail, highlight,
                  wc::clamp(bright * .6f + shape * .25f + .15f * variation, 0.f,
                            1.f));
              streak.a *= .35f + .4f * decay + .25f * inharm;
              line(Vec(x, -height * (.22f + .16f * decay)),
                   Vec(x + width * (variation - .5f) * inharm * .12f,
                       height * (.2f + .12f * fullness)),
                   streak, .4f + .35f * hard);
            }
            // A quiet sustain stripe remains readable without a strike.
            line(Vec(width * 0.35f, height * 0.38f),
                 Vec(width * 0.35f, height * (0.3f - 0.6f * decay)), highlight,
                 0.8f);
          }
          if (hollow > 0.f) {
            float wall = width * (0.08f + fullness * 0.2f);
            float opening = std::max(0.3f, width / 2.f - wall) * hollow;
            float y = perspective ? -height / 2.f + wall + opening * 0.4f : 0.f;
            nvgBeginPath(vg);
            nvgEllipse(vg, 0.f, y, opening,
                       perspective ? opening * 0.4f : opening);
            nvgFillColor(vg, detail);
            nvgFill(vg);
            nvgStrokeColor(vg, highlight);
            nvgStrokeWidth(vg, material == 2 ? 1.2f : 0.65f);
            nvgStroke(vg);
          }
          if (!perspective && hollow < 0.5f) {
            line(Vec(-width * 0.25f, -width * 0.15f),
                 Vec(width * 0.25f, -width * 0.15f), highlight, 0.7f);
            line(Vec(-width * 0.25f, width * (inharm * 0.3f)),
                 Vec(width * 0.25f, width * (inharm * 0.3f)), detail, 0.7f);
          }
          nvgRestore(vg);
        }
      }
      float numberX =
          wc::clamp(p.x - 62.f * visualScale(i) + 4.f, 4.f, box.size.x - 46.f);
      float numberY =
          wc::clamp(p.y - 46.f * visualScale(i) + 8.f, 10.f, box.size.y - 4.f);
      text(vg, numberX, numberY, std::to_string(i + 1), 10, color);
      if (module)
        for (int flag = 0; flag < 2; ++flag)
          if (module->params[W::param(i, flag == 0 ? W::SOLO : W::MUTE)]
                  .getValue() > .5f) {
            float ix =
                wc::clamp(numberX + 19.f + flag * 15.f, 8.f, box.size.x - 8.f);
            nvgBeginPath(vg);
            nvgCircle(vg, ix, numberY - 4.f, 6.f);
            nvgFillColor(
                vg, flag == 0 ? nvgRGB(176, 157, 76) : nvgRGB(153, 78, 65));
            nvgFill(vg);
            text(vg, ix - 3.f, numberY - 1.f, flag == 0 ? "S" : "M", 7.f,
                 nvgRGB(248, 245, 216));
          }
    }
    // Sails overlay all physical bodies, including neighboring chime sets.
    for (int i : setOrder)
      if (drawSails[i]) drawSails[i]();
    nvgRestore(vg);
    drawActions(vg);
  }
  void onButton(const event::Button& e) override {
    if (!module || !editor) return;
    if (e.button == GLFW_MOUSE_BUTTON_LEFT && actionsVisible() &&
        actionBox().contains(e.pos)) {
      if (e.action == GLFW_PRESS) {
        Vec p = e.pos - actionBox().pos;
        int action = std::min(3, int(p.x / 15.f));
        if (p.x - action * 15.f < 12.f) runAction(action);
        dragging = -1;
      }
      e.consume(this);
      return;
    }
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
          if (e.button == GLFW_MOUSE_BUTTON_LEFT &&
              (e.mods & (GLFW_MOD_SHIFT | RACK_MOD_CTRL))) {
            editor->toggle(i);
            activeSelection(module, editor);
            dragging = -1;
            e.consume(this);
            return;
          }
          if (!editor->contains(i) || e.button == GLFW_MOUSE_BUTTON_LEFT)
            editor->selectOnly(i);
          activeSelection(module, editor);
          if (e.button == GLFW_MOUSE_BUTTON_RIGHT) {
            dragging = -1;
            auto* menu = createMenu();
            menu->addChild(createMenuLabel(selectionCaption(editor)));
            menu->addChild(createCheckMenuItem(
                "Solo", "",
                [this, i]() {
                  return module->params[W::param(i, W::SOLO)].getValue() > .5f;
                },
                [this]() { toggleSelectionFlag(module, editor, W::SOLO); }));
            menu->addChild(createCheckMenuItem(
                "Mute", "",
                [this, i]() {
                  return module->params[W::param(i, W::MUTE)].getValue() > .5f;
                },
                [this]() { toggleSelectionFlag(module, editor, W::MUTE); }));
            menu->addChild(createMenuItem("Strike", "", [this]() {
              module->strikeRequests.fetch_or(activeSelection(module, editor),
                                              std::memory_order_relaxed);
            }));
            menu->addChild(createMenuItem("Stop", "", [this]() {
              module->stopRequests.fetch_or(activeSelection(module, editor),
                                            std::memory_order_relaxed);
            }));
            menu->addChild(createMenuItem("Randomize", "", [this]() {
              changeSelectionSound(module, editor, false);
            }));
            menu->addChild(createMenuItem("Wiggle", "", [this]() {
              changeSelectionSound(module, editor, true);
            }));
            menu->addChild(new ui::MenuSeparator);
            bool full = vacantSet(module) < 0;
            menu->addChild(createMenuItem(
                "Duplicate", full ? "16/16 chimes" : "",
                [this, i]() { copySet(module, editor, i, false); }, full));
            menu->addChild(createMenuItem(
                "Divide", full ? "16/16 chimes" : "",
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
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
      editor->clear();
      activeSelection(module, editor);
      e.consume(this);
      return;
    }
    OpaqueWidget::onButton(e);
  }
  void removeActive() {
    if (activeSelection(module, editor))
      removeSet(module, editor, editor->selected);
  }
  void onSelect(const event::Select& e) override { e.consume(this); }
  void onSelectKey(const event::SelectKey& e) override {
    int modifiers = e.mods & RACK_MOD_MASK;
    if (module && editor && e.key == GLFW_KEY_A &&
        (modifiers == RACK_MOD_CTRL || modifiers == GLFW_MOD_CONTROL)) {
      if (e.action == GLFW_PRESS) {
        editor->selectAll(enabledSelection(module));
        activeSelection(module, editor);
      }
      // Selected-stage keys run before Rack's global module-select shortcut.
      e.consume(this);
      return;
    }
    if (e.key == GLFW_KEY_R && (e.mods & RACK_MOD_CTRL)) {
      if (e.action == GLFW_PRESS) changeSelectionSound(module, editor, false);
      e.consume(this);
      return;
    }
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
      q->setValue(q->getValue() +
                  (axis == 0 ? delta.x / box.size.x : delta.y / box.size.y));
    }
  }
  void onDragEnd(const event::DragEnd& e) override {
    if (!module || dragging < 0) return;
    auto* action = new history::ComplexAction;
    action->name = "Move chimes";
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
      module->strikeRequests.fetch_or(activeSelection(module, editor),
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
    const NVGcolor selectedColor = nvgRGB(27, 31, 23);
    std::string caption = module ? selectionCaption(editor) : "Chimes 1";
    text(vg, controlX(338), 50, caption + " — Tuning / motion", 10,
         selectedColor);
    text(vg, controlX(338), 148, caption + " — Sound", 10, selectedColor);
    nvgRestore(vg);
  }
};
}  // namespace

struct ComputerscareWindchimesWidget : ModuleWidget {
  Editor editor;
  ChimeScene* scene = nullptr;
  void step() override {
    activeSelection(dynamic_cast<W*>(module), &editor);
    ModuleWidget::step();
  }
  void onHoverKey(const event::HoverKey& e) override {
    if (module && e.key == GLFW_KEY_TAB &&
        (e.mods & RACK_MOD_MASK & ~GLFW_MOD_SHIFT) == 0) {
      if (e.action == GLFW_PRESS) {
        auto* chimes = static_cast<W*>(module);
        uint32_t enabled = enabledSelection(chimes);
        int direction = (e.mods & GLFW_MOD_SHIFT) ? -1 : 1;
        int start = editor.mask ? editor.selected
                                : (direction > 0 ? wc::maxSets - 1 : 0);
        for (int step = 1; step <= wc::maxSets; ++step) {
          int next = (start + direction * step + wc::maxSets) % wc::maxSets;
          if (enabled & (1u << next)) {
            editor.selectOnly(next);
            activeSelection(chimes, &editor);
            break;
          }
        }
      }
      e.consume(this);
      return;
    }
    if (module && scene && scene->box.contains(e.pos) &&
        (e.mods & RACK_MOD_MASK) == 0 && e.key == GLFW_KEY_SPACE) {
      if (e.action == GLFW_PRESS)
        static_cast<W*>(module)->gustRequest.store(true,
                                                   std::memory_order_relaxed);
      e.consume(this);
      return;
    }
    if (module && scene && scene->box.contains(e.pos) &&
        (e.mods & RACK_MOD_MASK) == 0 &&
        (e.key == GLFW_KEY_M || e.key == GLFW_KEY_S || e.key == GLFW_KEY_W ||
         e.key == GLFW_KEY_R || e.key == GLFW_KEY_X)) {
      if (e.action == GLFW_PRESS) {
        auto* chimes = static_cast<W*>(module);
        if (e.key == GLFW_KEY_M || e.key == GLFW_KEY_S)
          toggleSelectionFlag(chimes, &editor,
                              e.key == GLFW_KEY_M ? W::MUTE : W::SOLO);
        else if (e.key == GLFW_KEY_X)
          chimes->strikeRequests.fetch_or(activeSelection(chimes, &editor),
                                          std::memory_order_relaxed);
        else
          changeSelectionSound(chimes, &editor, e.key == GLFW_KEY_W);
      }
      e.consume(this);
      return;
    }
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
    auto* panel = new ComputerscareSVGPanel;
    panel->setBackground(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/panels/ComputerscareWindchimesPanel.svg")));
    addChild(panel);
    auto* labels = new Labels;
    labels->box.size = box.size;
    labels->module = module;
    labels->editor = &editor;
    addChild(labels);
    scene = new ChimeScene;
    scene->module = module;
    scene->editor = &editor;
    scene->box =
        Rect(Vec(0, 0), Vec(320.f + controlColumnOffset, RACK_GRID_HEIGHT));
    addChild(scene);
    auto button = [this](Vec pos, float width,
                         std::function<std::string()> label,
                         std::function<void()> action) {
      auto* b = new ActionButton;
      b->box = Rect(pos + Vec(0.f, 4.f), Vec(width * .82f, 15.f));
      b->label = std::move(label);
      b->action = std::move(action);
      addChild(b);
    };
    auto selector = [this](Vec pos, float width,
                           std::function<std::string()> label,
                           std::function<Menu*()> openMenu) {
      auto* b = new SelectorButton;
      b->box = Rect(pos + Vec(0.f, 4.f), Vec(width * .82f, 15.f));
      b->label = std::move(label);
      b->openMenu = std::move(openMenu);
      addChild(b);
    };
    button(
        Vec(462, 10), 47, []() { return "+ Add"; },
        [this, module]() {
          if (!module) return;
          for (int i = 0; i < wc::maxSets; ++i)
            if (module->params[W::param(i, W::ENABLED)].getValue() < 0.5f) {
              auto* action = new history::ComplexAction;
              action->name = "Add chimes";
              for (int f = 0; f < W::SET_PARAMS; ++f)
                setWithHistory(
                    module, W::param(i, f),
                    module->getParamQuantity(W::param(i, f))->getDefaultValue(),
                    action);
              for (int f : {W::SHAPE, W::BODY, W::INHARMONICITY, W::SPREAD,
                            W::STRIKER_WEIGHT, W::SAIL_SIZE, W::MUTE, W::SOLO})
                setWithHistory(
                    module, W::param(i, f),
                    module->getParamQuantity(W::param(i, f))->getDefaultValue(),
                    action);
              setWithHistory(module, W::param(i, W::ENABLED), 1.f, action);
              APP->history->push(action);
              editor.selectOnly(i);
              break;
            }
        });
    button(
        Vec(513, 10), 48, []() { return "Remove"; },
        [this, module]() {
          if (activeSelection(module, &editor))
            removeSet(module, &editor, editor.selected);
        });
    button(
        Vec(428, 139), 47, []() { return "Strike"; },
        [this, module]() {
          if (module)
            module->strikeRequests.fetch_or(activeSelection(module, &editor),
                                            std::memory_order_relaxed);
        });
    button(
        Vec(565, 10), 28, [this]() { return editor.perspective ? "3D" : "2D"; },
        [this]() { editor.perspective = !editor.perspective; });
    for (int field : {W::MATERIAL, W::SCALE}) {
      selector(
          Vec(field == W::MATERIAL ? 338 : 338,
              field == W::MATERIAL ? 163 : 57),
          field == W::MATERIAL ? 52 : 80,
          [this, module, field]() {
            int value =
                module ? static_cast<int>(std::round(
                             module->params[W::param(editor.selected, field)]
                                 .getValue()))
                       : 0;
            if (module)
              for (int i = 0; i < wc::maxSets; ++i)
                if (editor.contains(i) &&
                    std::round(module->params[W::param(i, field)].getValue()) !=
                        value)
                  return std::string("Mixed  v");
            return std::string(
                       field == W::MATERIAL
                           ? materialCaptions[std::max(0, std::min(value, 2))]
                           : wc::scaleCaptions[std::max(
                                 0, std::min(value, wc::scaleCount - 1))]) +
                   "  v";
          },
          [this, module, field]() -> Menu* {
            if (!activeSelection(module, &editor)) return nullptr;
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
                  [module, field, i, this]() {
                    auto* action = new history::ComplexAction;
                    action->name = "Edit chimes";
                    editSelectionField(module, activeSelection(module, &editor),
                                       field, i, action);
                    APP->history->push(action);
                  }));
            return menu;
          });
    }
    button(
        Vec(480, 139), 52, []() { return "Wiggle"; },
        [this, module]() { changeSelectionSound(module, &editor, true); });
    button(
        Vec(538, 139), 47, []() { return "Random"; },
        [this, module]() { changeSelectionSound(module, &editor, false); });
    button(
        Vec(538, 163), 36,
        [this, module]() {
          return module && module->params[W::param(editor.selected, W::SOLO)]
                                   .getValue() > .5f
                     ? "Solo*"
                     : "Solo";
        },
        [this, module]() { toggleSelectionFlag(module, &editor, W::SOLO); });
    button(
        Vec(569, 163), 36,
        [this, module]() {
          return module && module->params[W::param(editor.selected, W::MUTE)]
                                   .getValue() > .5f
                     ? "Mute*"
                     : "Mute";
        },
        [this, module]() { toggleSelectionFlag(module, &editor, W::MUTE); });
    selector(
        Vec(426, 163), 94,
        [this, module]() {
          if (!module) return std::string("Preset  v");
          int selected = editor.selected;
          int material = static_cast<int>(std::round(
              module->params[W::param(selected, W::MATERIAL)].getValue()));
          for (int i = 0; i < wc::maxSets; ++i)
            if (editor.contains(i)) {
              for (int field :
                   {W::MATERIAL, W::DECAY, W::BRIGHTNESS, W::HARDNESS, W::SHAPE,
                    W::BODY, W::INHARMONICITY})
                if (std::fabs(
                        module->params[W::param(i, field)].getValue() -
                        module->params[W::param(selected, field)].getValue()) >
                    0.0001f)
                  return std::string("Mixed  v");
            }
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
          if (!activeSelection(module, &editor)) return nullptr;
          int selected = editor.selected;
          auto presets = wc::soundPresets(static_cast<int>(std::round(
              module->params[W::param(selected, W::MATERIAL)].getValue())));
          auto* menu = createMenu();
          menu->addChild(createMenuLabel("Sound presets — selected material"));
          for (int i = 0; i < presets.count; ++i) {
            wc::SoundPreset preset = presets.items[i];
            menu->addChild(
                createMenuItem(preset.name, "", [module, preset, this]() {
                  auto* action = new history::ComplexAction;
                  action->name = "Apply windchime sound preset";
                  const int fields[] = {W::DECAY,    W::BRIGHTNESS,
                                        W::HARDNESS, W::SHAPE,
                                        W::BODY,     W::INHARMONICITY};
                  const float values[] = {
                      preset.decay, preset.brightness, preset.hardness,
                      preset.shape, preset.body,       preset.inharmonicity};
                  for (int f = 0; f < 6; ++f)
                    editSelectionField(module, activeSelection(module, &editor),
                                       fields[f], values[f], action);
                  APP->history->push(action);
                }));
          }
          return menu;
        });
    const int fields[] = {
        W::TUBES,         W::ROOT,           W::OCTAVE, W::FINE,
        W::DIVISIONS,     W::SPREAD,         W::DECAY,  W::BRIGHTNESS,
        W::HARDNESS,      W::STRIKER_WEIGHT, W::SHAPE,  W::BODY,
        W::INHARMONICITY, W::LEVEL,          W::SWING,  W::SAIL_SIZE};
    for (int i = 0; i < 16; ++i) {
      auto* knob =
          createParamCentered<SetKnob>(Vec(i < 6    ? 349.f + i * 45.f
                                           : i < 13 ? 346.f + (i - 6) * 38.5f
                                                    : 471.f + (i - 13) * 49.f,
                                           i < 6    ? 107.f
                                           : i < 13 ? 205.f
                                                    : 72.f),
                                       module, W::param(0, fields[i]));
      knob->editor = &editor;
      knob->field = fields[i];
      addParam(knob);
    }
    const int globals[] = {W::WIND,      W::GUSTINESS,  W::TURBULENCE,
                           W::WIND_MIX,  W::WIND_TONE,  W::WIND_TEXTURE,
                           W::MASTER,    W::REVERB_MIX, W::REVERB_SIZE,
                           W::DELAY_MIX, W::DELAY_TIME, W::DELAY_FEEDBACK};
    for (int i = 0; i < 12; ++i)
      addParam(createParamCentered<ChimeKnob>(
          Vec(i < 6 ? 349.f + i * 45.f : 349.f + (i - 6) * 45.f,
              i < 6 ? 253.f : 292.f),
          module, globals[i]));
    addParam(
        createParamCentered<SmallGustButton>(Vec(419, 329), module, W::GUST));
    for (int i = 0; i < 4; ++i)
      addInput(
          createInputCentered<InPort>(Vec(349.f + i * 35.f, 353.f), module, i));
    addOutput(createOutputCentered<PointingUpPentagonPort>(
        Vec(491, 334), module, W::WIND_OUTPUT));
    addOutput(createOutputCentered<PointingUpPentagonPort>(
        Vec(491, 358), module, W::WIND_AUDIO_OUTPUT));
    const int outputIds[] = {W::LEFT_OUTPUT, W::RIGHT_OUTPUT,
                             W::REAR_LEFT_OUTPUT, W::REAR_RIGHT_OUTPUT};
    for (int i = 0; i < 4; ++i)
      addOutput(createOutputCentered<PointingUpPentagonPort>(
          Vec(530.f + (i % 2) * 38.f, 334.f + (i / 2) * 24.f), module,
          outputIds[i]));
    // Reserve the extra module width entirely for the scene. Keep the existing
    // compact control-column layout and translate its widgets together.
    for (auto* child : children)
      if (child != children.front() && child != labels && child != scene) {
        // Buttons anchor at their left edge; knobs and jacks keep their
        // centers.
        float anchor =
            dynamic_cast<ActionButton*>(child) ? 0.f : child->box.size.x * .5f;
        child->box.pos.x =
            controlX(child->box.pos.x + anchor) - anchor + controlColumnOffset;
      }
  }
  void appendContextMenu(Menu* menu) override {
    auto* m = dynamic_cast<W*>(module);
    if (!m) return;
    menu->addChild(new MenuSeparator);
    menu->addChild(createCheckMenuItem(
        "Exclude wind from quad", "",
        [m]() {
          return m->excludeWindFromQuad.load(std::memory_order_relaxed);
        },
        [m]() {
          m->excludeWindFromQuad.store(
              !m->excludeWindFromQuad.load(std::memory_order_relaxed),
              std::memory_order_relaxed);
        }));
  }
};
Model* modelComputerscareWindchimes =
    createModel<ComputerscareWindchimes, ComputerscareWindchimesWidget>(
        "computerscare-windchimes");
