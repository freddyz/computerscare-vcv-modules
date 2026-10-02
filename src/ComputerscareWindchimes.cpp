#include <atomic>
#include <functional>

#include "Computerscare.hpp"
#include "Windchimes/Engine.hpp"

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
    SET_PARAMS
  };
  // Append controls after the existing set parameters to preserve saved
  // patches.
  enum EffectParam {
    REVERB_MIX = SET_BASE + wc::maxSets * SET_PARAMS,
    REVERB_SIZE,
    WIND_TONE,
    WIND_TEXTURE,
    NUM_PARAMS
  };
  enum Input { WIND_INPUT, TRANSPOSE_INPUT, GUST_INPUT, INPUTS };
  enum Output { LEFT_OUTPUT, RIGHT_OUTPUT, WIND_OUTPUT, OUTPUTS };
  static constexpr int param(int set, int field) {
    return SET_BASE + set * SET_PARAMS + field;
  }
  wc::Engine engine;
  dsp::SchmittTrigger gustTrigger, gustButton;
  std::atomic<uint32_t> strikeRequests{0};
  std::array<std::atomic<float>, wc::maxSets> visualStrikerAngle{},
      visualStrikerFlash{}, visualStrikerDepth{};
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
                  name + "distance");
      configParam(param(i, TUBES), 1.f, 12.f, 6.f, name + "tubes")
          ->snapEnabled = true;
      configSwitch(param(i, MATERIAL), 0.f, 2.f, 0.f, name + "material",
                   {"Metal", "Wood / bamboo", "Plastic"});
      configSwitch(param(i, SCALE), 0.f, 4.f, 0.f, name + "scale",
                   {"Major pentatonic", "Minor pentatonic", "Major", "Minor",
                    "Whole tone"});
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
    c.root = static_cast<int>(std::round(value(ROOT)));
    c.octave = value(OCTAVE);
    c.fine = value(FINE);
    c.divisions = static_cast<int>(std::round(value(DIVISIONS)));
    c.decay = value(DECAY);
    c.brightness = value(BRIGHTNESS);
    c.hardness = value(HARDNESS);
    c.level = value(LEVEL);
    c.swing = value(SWING);
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
    uint32_t requests = strikeRequests.exchange(0, std::memory_order_relaxed);
    for (int i = 0; requests && i < wc::maxSets; ++i)
      if (requests & (1u << i)) engine.strike(i);
    wc::Stereo out = engine.process(amount, gustiness, turbulence, windMix);
    // Publish after advancing physics and exciting audio from the same
    // contacts.
    if (publishVisual) {
      for (int i = 0; i < wc::maxSets; ++i) {
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
const char* scales[] = {"Major pentatonic", "Minor pentatonic", "Major",
                        "Minor", "Whole tone"};
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
struct ActionButton : ComputerscareBlankButton {
  std::function<std::string()> label;
  std::function<void()> action;
  Vec nativeSize = box.size;
  bool pressed = false;
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
    text(args.vg, pressed ? 10.f : 8.f,
         box.size.y * 0.5f + (pressed ? 5.f : 3.f), label(), 11.f,
         nvgRGB(20, 39, 35));
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
  Vec position(int i) {
    if (!module) return Vec(125.f + i * 155.f, 76.f);
    return Vec(65.f + module->params[W::param(i, W::X)].getValue() *
                          (box.size.x - 130.f),
               54.f + module->params[W::param(i, W::Y)].getValue() *
                          (box.size.y - 100.f));
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
    text(vg, 10, 16,
         editor && !editor->perspective ? "2D TOP — click for 3D"
                                        : "3D — click for 2D",
         8);
    text(vg, 10, box.size.y - 8, "NEAR", 8);
    text(vg, box.size.x - 112, 16, "DRAG TO POSITION", 8);
    for (int i = 0; i < wc::maxSets; ++i) {
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
      constexpr float scale = 48.f;
      bool perspective = !editor || editor->perspective;
      auto point = [p, perspective](wc::Point v) {
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
      nvgRoundedRect(vg, p.x - 62, p.y - 46, 124, 84, 8);
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
          if (perspective) {
            Vec sail = point(center + axis * 0.46f);
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
      text(vg, p.x - 3, p.y + 33, std::to_string(i + 1), 10, color);
    }
  }
  void onButton(const event::Button& e) override {
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT &&
        e.pos.y < 22.f && e.pos.x < 145.f && editor) {
      editor->perspective = !editor->perspective;
      e.consume(this);
      return;
    }
    if (!module) return;
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
      for (int i = wc::maxSets - 1; i >= 0; --i) {
        if (enabled(i) && std::fabs(position(i).x - e.pos.x) < 62.f &&
            std::fabs(position(i).y - e.pos.y) < 45.f) {
          editor->selected = i;
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
  void onDragMove(const event::DragMove& e) override {
    if (!module || dragging < 0 || e.button != GLFW_MOUSE_BUTTON_LEFT) return;
    Vec delta = e.mouseDelta.div(APP->scene->rackScroll->getZoom());
    for (int axis = 0; axis < 2; ++axis) {
      int id = W::param(dragging, axis == 0 ? W::X : W::Y);
      auto* q = module->getParamQuantity(id);
      q->setValue(q->getValue() + (axis == 0 ? delta.x / (box.size.x - 130)
                                             : delta.y / (box.size.y - 100)));
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
    text(vg, 18, 23, "WINDCHIMES", 17, nvgRGB(218, 239, 162));
    text(vg, 464, 23, "computerscare", 12);
    int selected = editor ? editor->selected : 0;
    bool enabled =
        module
            ? module->params[W::param(selected, W::ENABLED)].getValue() > 0.5f
            : true;
    // The selected-set area and global area have separate, continuous outlines.
    const NVGcolor selectedColor = nvgRGB(218, 239, 162);
    const NVGcolor globalColor = nvgRGB(151, 208, 232);
    nvgBeginPath(vg);
    nvgMoveTo(vg, 15, 213);
    nvgLineTo(vg, 585, 213);
    nvgLineTo(vg, 585, 263);
    nvgLineTo(vg, 223, 263);
    nvgLineTo(vg, 223, 321);
    nvgLineTo(vg, 15, 321);
    nvgClosePath(vg);
    nvgFillColor(vg, nvgRGB(44, 53, 35));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGBA(218, 239, 162, 95));
    nvgStrokeWidth(vg, 1);
    nvgStroke(vg);
    nvgBeginPath(vg);
    nvgMoveTo(vg, 230, 265);
    nvgLineTo(vg, 585, 265);
    nvgLineTo(vg, 585, 366);
    nvgLineTo(vg, 15, 366);
    nvgLineTo(vg, 15, 325);
    nvgLineTo(vg, 230, 325);
    nvgClosePath(vg);
    nvgFillColor(vg, nvgRGB(27, 46, 61));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGBA(151, 208, 232, 110));
    nvgStrokeWidth(vg, 1);
    nvgStroke(vg);
    text(vg, 237, 274, "GLOBAL — ALL CHIME SETS", 9, globalColor);
    text(vg, 23, 274, "SELECTED SET MIX / MOTION", 8, selectedColor);
    text(vg, 17, 359, "GUST", 8, globalColor);
    text(vg, 23, 222,
         enabled ? "SELECTED CHIME SET " + std::to_string(selected + 1) +
                       " — SOUND / TUNING"
                 : "SELECT A SET OR ADD ONE",
         9, selectedColor);
    const char* top[] = {"Tubes", "Root",  "Register",   "Fine",
                         "EDO",   "Decay", "Brightness", "Hardness"};
    const char* bottom[] = {"Set level", "Swing",      "",         "Wind",
                            "Gustiness", "Turbulence", "Wind mix", "Output"};
    for (int i = 0; i < 8; ++i) {
      text(vg, 23.f + i * 73.f, 260, top[i], 9, selectedColor);
      text(vg, 23.f + i * 73.f, 315, bottom[i], 9,
           i < 3 ? selectedColor : globalColor);
    }
    text(vg, 152, 292, "PAN / DISTANCE", 7, selectedColor);
    text(vg, 152, 306, "Drag in scene", 8, selectedColor);
    const char* ports[] = {"WIND CV",  "1V/OCT",   "GUST",
                           "WIND OUT", "L / MONO", "R"};
    const float positions[] = {176, 222, 268, 436, 526, 564};
    for (int i = 0; i < 6; ++i)
      text(vg, positions[i] - (i == 5 ? 3.f : 20.f), 359, ports[i], 8);
    text(vg, 63, 359, "REV WET", 8);
    text(vg, 115, 359, "SIZE", 8);
    text(vg, 305, 359, "WIND TONE", 8);
    text(vg, 356, 359, "TEXTURE", 8);
  }
};
}  // namespace

struct ComputerscareWindchimesWidget : ModuleWidget {
  Editor editor;
  ComputerscareWindchimesWidget(W* module) {
    setModule(module);
    box.size = Vec(600, RACK_GRID_HEIGHT);
    addChild(new BGPanel(nvgRGB(22, 42, 39)));
    children.back()->box.size = box.size;
    auto* labels = new Labels;
    labels->box.size = box.size;
    labels->module = module;
    labels->editor = &editor;
    addChild(labels);
    auto* scene = new ChimeScene;
    scene->module = module;
    scene->editor = &editor;
    scene->box = Rect(Vec(15, 35), Vec(570, 148));
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
    button(
        Vec(15, 188), 98,
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
              setWithHistory(module, W::param(i, W::ENABLED), 1.f, action);
              APP->history->push(action);
              editor.selected = i;
              break;
            }
        });
    button(
        Vec(119, 188), 74, []() { return "Remove"; },
        [this, module]() {
          if (!module) return;
          auto* action = new history::ComplexAction;
          action->name = "Remove windchime set";
          setWithHistory(module, W::param(editor.selected, W::ENABLED), 0.f,
                         action);
          APP->history->push(action);
          for (int i = 0; i < wc::maxSets; ++i)
            if (module->params[W::param(i, W::ENABLED)].getValue() > 0.5f) {
              editor.selected = i;
              break;
            }
        });
    button(
        Vec(199, 188), 75, []() { return "Strike"; },
        [this, module]() {
          if (module)
            module->strikeRequests.fetch_or(1u << editor.selected,
                                            std::memory_order_relaxed);
        });
    for (int field : {W::MATERIAL, W::SCALE}) {
      button(
          Vec(field == W::MATERIAL ? 280 : 410, 188),
          field == W::MATERIAL ? 124 : 175,
          [this, module, field]() {
            int value =
                module ? static_cast<int>(std::round(
                             module->params[W::param(editor.selected, field)]
                                 .getValue()))
                       : 0;
            return std::string(field == W::MATERIAL
                                   ? materials[std::max(0, std::min(value, 2))]
                                   : scales[std::max(0, std::min(value, 4))]) +
                   "  v";
          },
          [this, module, field]() {
            if (!module) return;
            auto* menu = createMenu();
            int selected = editor.selected;
            for (int i = 0; i < (field == W::MATERIAL ? 3 : 5); ++i)
              menu->addChild(createCheckMenuItem(
                  field == W::MATERIAL ? materials[i] : scales[i], "",
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
          });
    }
    const int fields[] = {W::TUBES,     W::ROOT,  W::OCTAVE,     W::FINE,
                          W::DIVISIONS, W::DECAY, W::BRIGHTNESS, W::HARDNESS,
                          W::LEVEL,     W::SWING};
    for (int i = 0; i < 10; ++i) {
      auto* knob = createParamCentered<SetKnob>(
          Vec(42.f + (i % 8) * 73.f, i < 8 ? 237.f : 289.f), module,
          W::param(0, fields[i]));
      knob->editor = &editor;
      knob->field = fields[i];
      addParam(knob);
    }
    const int globals[] = {W::WIND, W::GUSTINESS, W::TURBULENCE, W::WIND_MIX,
                           W::MASTER};
    for (int i = 0; i < 5; ++i)
      addParam(createParamCentered<ChimeKnob>(Vec(261.f + i * 73.f, 289.f),
                                              module, globals[i]));
    addParam(createParamCentered<ComputerscareBlankButton>(Vec(45, 333), module,
                                                           W::GUST));
    const int effects[] = {W::REVERB_MIX, W::REVERB_SIZE, W::WIND_TONE,
                           W::WIND_TEXTURE};
    const float effectsX[] = {84, 128, 326, 374};
    for (int i = 0; i < 4; ++i)
      addParam(createParamCentered<ChimeKnob>(Vec(effectsX[i], 333), module,
                                              effects[i]));
    for (int i = 0; i < 3; ++i)
      addInput(
          createInputCentered<InPort>(Vec(176.f + i * 46.f, 333.f), module, i));
    addOutput(
        createOutputCentered<OutPort>(Vec(436, 333), module, W::WIND_OUTPUT));
    addOutput(
        createOutputCentered<OutPort>(Vec(526, 333), module, W::LEFT_OUTPUT));
    addOutput(
        createOutputCentered<OutPort>(Vec(564, 333), module, W::RIGHT_OUTPUT));
  }
};
Model* modelComputerscareWindchimes =
    createModel<ComputerscareWindchimes, ComputerscareWindchimesWidget>(
        "computerscare-windchimes");
