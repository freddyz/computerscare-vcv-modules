#include <osdialog.h>

#include <atomic>
#include <mutex>

#include "Computerscare.hpp"
#include "Phlooper/Block.hpp"
#include "Phlooper/Dropdown.hpp"
#include "Phlooper/Engine.hpp"
#include "Phlooper/Wav.hpp"

struct PhlooperLengthQuantity : ParamQuantity {
  std::string getDescription() override;
};

struct PhlooperOffsetQuantity : ParamQuantity {
  float getDisplayValue() override;
  void setDisplayValue(float value) override;
  std::string getDisplayValueString() override {
    return string::f("%+.3f", getDisplayValue());
  }
  std::string getUnit() override;
};

struct ComputerscarePhlooper : Module {
  enum Param {
    START,
    LENGTH,
    OFFSET,
    REC_MIX,
    MIX,
    COUNT,
    MODE,
    OVERDUB,
    RECORD,
    ERASE,
    RESTART,
    SPEED,
    STOP,
    HOLD,
    PARAMS
  };
  enum Input {
    LEFT,
    RIGHT,
    REC_GATE,
    ERASE_GATE,
    RESTART_TRIG,
    MUTE_GATE,
    STOP_GATE,
    START_CV,
    LENGTH_CV,
    OFFSET_CV,
    HOLD_GATE,
    INPUTS
  };
  enum Output { OUT_L, OUT_R, EOC, OUTPUTS };
  enum Light { RECORD_LIGHT, LIGHTS };
  phlooper::Engine loop;
  std::mutex storage;
  std::atomic<int> frames{0};
  std::array<std::atomic<float>, 16> positions{}, starts{}, ends{};
  std::atomic<bool> recording{false};
  std::atomic<bool> resetRequest{false};
  bool firstGate = false;
  dsp::SchmittTrigger restartButton;
  std::array<dsp::SchmittTrigger, 16> retrigger;
  std::array<dsp::PulseGenerator, 16> endPulse;
  std::string error;
  ComputerscarePhlooper() {
    config(PARAMS, INPUTS, OUTPUTS, LIGHTS);
    configParam(START, 0, 1, 0, "Start", "%", 0, 100);
    configParam<PhlooperLengthQuantity>(LENGTH, .001, 1, 1, "Length", "%", 0,
                                        100);
    configParam(SPEED, -2.f, 2.f, 0.f, "Overall playback speed", "×", 2.f);
    configParam<PhlooperOffsetQuantity>(OFFSET, -20, 20, 1, "Phase offset");
    configParam(REC_MIX, 0, 1, .5, "Record mix", "%", 0, 100);
    configParam(MIX, 0, 1, 1, "Output mix", "%", 0, 100);
    configParam(COUNT, 1, 16, 2, "Active loops")->snapEnabled = true;
    configSwitch(MODE, 0, 2, 0, "Phase mode",
                 {"Time (ms)", "Length (%)", "Speed (%)"});
    configSwitch(OVERDUB, 0, 1, 0, "Overdub", {"Blend", "Add"});
    configSwitch(RECORD, 0.f, 1.f, 0.f, "Record all", {"Off", "Recording"});
    configLight(RECORD_LIGHT, "Recording");
    configButton(ERASE, "Erase all while held");
    configButton(RESTART, "Restart all");
    configSwitch(HOLD, 0.f, 1.f, 0.f, "Hold phase", {"Drifting", "Held"});
    configSwitch(STOP, 0.f, 1.f, 0.f, "Stop all loops", {"Playing", "Stopped"});
    const char* names[] = {
        "Audio left / mono", "Audio right", "Record gates",   "Erase gates",
        "Restart triggers",  "Mute gates",  "Stop gates",     "Start CV",
        "Length CV",         "Offset CV",   "Hold phase gate"};
    for (int i = 0; i < INPUTS; ++i) configInput(i, names[i]);
    getInputInfo(OFFSET_CV)->description =
        "Each channel sets its loop offset: -10 to +10 V maps to -100 to +100 "
        "ms "
        "(Time), or -20 to +20% (Length/Speed). Missing channels use the knob "
        "spacing.";
    for (int port : {START_CV, LENGTH_CV})
      getInputInfo(port)->description =
          "Polyphonic offsets from the knob: +/-10 V adds/subtracts 100 "
          "percentage points. Mono applies to all loops; missing poly channels "
          "use the knob.";
    configOutput(EOC, "End of loop triggers (polyphonic)");
    configOutput(OUT_L, "Left");
    configOutput(OUT_R, "Right");
    configBypass(LEFT, OUT_L);
    configBypass(RIGHT, OUT_R);
  }
  unsigned gates(int port) {
    unsigned mask = 0;
    for (int i = 0; i < 16; ++i) {
      // A one-channel control addresses all loops; poly channels address
      // individually.
      float v = inputs[port].getChannels() == 1 ? inputs[port].getVoltage()
                                                : inputs[port].getVoltage(i);
      if (v >= 1.f) mask |= 1u << i;
    }
    return mask;
  }
  void process(const ProcessArgs& args) override {
    phlooper::Frame input;
    input.l = inputs[LEFT].getVoltage();
    input.r =
        inputs[RIGHT].isConnected() ? inputs[RIGHT].getVoltage() : input.l;
    std::unique_lock<std::mutex> lock(storage, std::try_to_lock);
    if (!lock.owns_lock()) {
      outputs[OUT_L].setVoltage(input.l);
      outputs[OUT_R].setVoltage(input.r);
      return;
    }
    if (resetRequest.exchange(false)) {
      loop.size = 0;
      loop.initial = false;
      params[RECORD].setValue(0.f);
      frames = 0;
    }
    unsigned rec =
        gates(REC_GATE) | (params[RECORD].getValue() > .5f ? 65535u : 0u);
    if (!loop.size && !loop.initial && rec) {
      loop.begin(inputs[RIGHT].isConnected());
      firstGate = true;
    }
    if (loop.initial && !rec && firstGate) {
      loop.finish();
      firstGate = false;
    }
    unsigned restart = 0;
    bool all = restartButton.process(params[RESTART].getValue());
    for (int i = 0; i < 16; ++i) {
      float v = inputs[RESTART_TRIG].getChannels() == 1
                    ? inputs[RESTART_TRIG].getVoltage()
                    : inputs[RESTART_TRIG].getVoltage(i);
      if (retrigger[i].process(v) || all) restart |= 1u << i;
    }
    phlooper::Settings s;
    s.count = int(params[COUNT].getValue());
    s.mode = int(params[MODE].getValue());
    s.overdub = int(params[OVERDUB].getValue());
    s.start = params[START].getValue();
    s.length = params[LENGTH].getValue();
    for (int i = 0; i < phlooper::voices; ++i) {
      auto cv = [&](int port) {
        return inputs[port].getChannels() == 1 ? inputs[port].getVoltage()
                                               : inputs[port].getVoltage(i);
      };
      if (inputs[START_CV].getChannels() == 1 ||
          i < inputs[START_CV].getChannels()) {
        s.startMask |= 1u << i;
        s.starts[i] = clamp(s.start + cv(START_CV) / 10.f, 0.f, 1.f);
      }
      if (inputs[LENGTH_CV].getChannels() == 1 ||
          i < inputs[LENGTH_CV].getChannels()) {
        s.lengthMask |= 1u << i;
        s.lengths[i] = clamp(s.length + cv(LENGTH_CV) / 10.f, .001f, 1.f);
      }
    }
    s.offset = params[OFFSET].getValue() * (s.mode == 0 ? 5.f : 1.f);
    for (int i = 0; i < std::min(16, inputs[OFFSET_CV].getChannels()); ++i) {
      s.offsetMask |= 1u << i;
      float range = s.mode == 0 ? 100.f : 20.f;
      s.offsets[i] =
          clamp(inputs[OFFSET_CV].getVoltage(i) / 10.f, -1.f, 1.f) * range;
    }
    s.recordMix = params[REC_MIX].getValue();
    s.mix = params[MIX].getValue();
    s.speed = std::pow(2.f, params[SPEED].getValue());
    s.hold =
        params[HOLD].getValue() > .5f || inputs[HOLD_GATE].getVoltage() >= 1.f;
    auto out = loop.process(
        input, args.sampleRate, s, rec,
        gates(ERASE_GATE) | (params[ERASE].getValue() > .5f ? 65535u : 0u),
        restart, gates(MUTE_GATE),
        gates(STOP_GATE) | (params[STOP].getValue() > .5f ? 65535u : 0u),
        inputs[RIGHT].isConnected());
    outputs[OUT_L].setVoltage(out.l);
    outputs[OUT_R].setVoltage(out.r);
    outputs[EOC].setChannels(s.count);
    for (int i = 0; i < 16; ++i) {
      if (loop.completed & (1u << i)) endPulse[i].trigger(.001f);
      bool high = endPulse[i].process(args.sampleTime);
      if (i < s.count) outputs[EOC].setVoltage(high ? 10.f : 0.f, i);
    }
    frames.store(loop.initial ? loop.captured : loop.size,
                 std::memory_order_relaxed);
    recording.store(loop.initial || rec, std::memory_order_relaxed);
    lights[RECORD_LIGHT].setBrightness(loop.initial || rec ? 1.f : 0.f);
    for (int i = 0; i < 16; ++i) {
      float start = float(std::max(0, loop.activeStart[i]));
      positions[i].store(
          loop.size ? float((start + loop.head[i]) / loop.size) : 0.f,
          std::memory_order_relaxed);
      starts[i].store(loop.size ? start / loop.size : 0.f,
                      std::memory_order_relaxed);
      ends[i].store(
          loop.size ? float((start + loop.periods[i]) / loop.size) : 1.f,
          std::memory_order_relaxed);
    }
  }
  void load(const std::string& path) {
    try {
      auto wav = phlooper::loadWav(path);
      int available = int(wav.samples.size() / wav.channels);
      int n = int(std::min(double(phlooper::capacity),
                           double(available) * phlooper::rate / wav.rate));
      if (n < 1) throw std::runtime_error("Empty WAV");
      std::vector<float> converted(n * 2);
      for (int i = 0; i < n; ++i) {
        double p = double(i) * wav.rate / phlooper::rate;
        int a = std::min(available - 1, int(p)),
            b = std::min(available - 1, a + 1);
        float f = float(p - a);
        for (int ch = 0; ch < 2; ++ch) {
          int c = std::min(ch, wav.channels - 1);
          converted[2 * i + ch] =
              5.f * (wav.samples[a * wav.channels + c] * (1 - f) +
                     wav.samples[b * wav.channels + c] * f);
        }
      }
      std::lock_guard<std::mutex> lock(storage);
      for (int i = 0; i < 16; ++i) {
        std::copy(converted.begin(), converted.end(), loop.audio[i].get());
        loop.stereo[i] = wav.channels > 1;
      }
      loop.size = n;
      loop.initial = false;
      loop.resetHeads();
      params[RECORD].setValue(0.f);
      frames = n;
      error.clear();
    } catch (const std::exception& e) {
      error = e.what();
    }
  }
  json_t* dataToJson() override {
    std::lock_guard<std::mutex> lock(storage);
    auto* root = json_object();
    auto* heads = json_array();
    auto* starts = json_array();
    for (int i = 0; i < 16; ++i) {
      json_array_append_new(heads, json_real(loop.head[i]));
      json_array_append_new(starts, json_integer(loop.activeStart[i]));
    }
    json_object_set_new(root, "heads", heads);
    json_object_set_new(root, "starts", starts);
    return root;
  }
  void dataFromJson(json_t* root) override {
    std::lock_guard<std::mutex> lock(storage);
    auto* heads = json_object_get(root, "heads");
    auto* starts = json_object_get(root, "starts");
    for (int i = 0; i < 16; ++i) {
      auto* h = json_array_get(heads, i);
      double value = json_is_number(h) ? json_number_value(h) : 0.;
      loop.head[i] = std::isfinite(value)
                         ? std::max(0., std::min(double(loop.limit), value))
                         : 0.;
      auto* start = json_array_get(starts, i);
      loop.activeStart[i] =
          json_is_integer(start)
              ? int(std::max(json_int_t(-1),
                             std::min(json_int_t(loop.limit - 1),
                                      json_integer_value(start))))
              : -1;
    }
  }
  void onSave(const SaveEvent&) override {
    int n;
    {
      std::lock_guard<std::mutex> lock(storage);
      n = loop.initial ? loop.captured : loop.size;
    }
    try {
      std::string dir = createPatchStorageDirectory();
      if (!n) {
        for (int i = 0; i < 16; ++i)
          std::remove(
              (dir + "/loop-" + std::to_string(i + 1) + ".wav").c_str());
        return;
      }
      std::vector<float> snapshot(n * 2);
      for (int i = 0; i < 16; ++i) {
        bool stereo = false;
        for (int start = 0; start < n * 2; start += 4096) {
          std::lock_guard<std::mutex> lock(storage);
          int end = std::min(n * 2, start + 4096);
          std::copy(loop.audio[i].get() + start, loop.audio[i].get() + end,
                    snapshot.begin() + start);
          stereo = loop.stereo[i];
        }
        phlooper::saveWav(dir + "/loop-" + std::to_string(i + 1) + ".wav",
                          snapshot.data(), n, stereo);
      }
    } catch (const std::exception& e) {
      WARN("Phlooper save: %s", e.what());
    }
  }
  void onAdd(const AddEvent&) override {
    std::lock_guard<std::mutex> lock(storage);
    for (int i = 0; i < 16; ++i) {
      try {
        auto wav = phlooper::loadWav(getPatchStorageDirectory() + "/loop-" +
                                     std::to_string(i + 1) + ".wav");
        int n = std::min(loop.limit, int(wav.samples.size() / wav.channels));
        for (int j = 0; j < n; ++j)
          for (int ch = 0; ch < 2; ++ch)
            loop.audio[i][2 * j + ch] =
                wav.samples[j * wav.channels + std::min(ch, wav.channels - 1)] *
                5.f;
        if (i == 0) loop.size = n;
        if (n != loop.size) {
          loop.size = 0;
          break;
        }
        loop.stereo[i] = wav.channels > 1;
      } catch (const std::exception&) {
        if (loop.size) loop.size = 0;
        break;
      }
    }
    for (int i = 0; i < 16; ++i)
      if (loop.activeStart[i] >= loop.size) loop.activeStart[i] = -1;
    frames = loop.size;
  }
  void onReset(const ResetEvent& e) override {
    Module::onReset(e);
    resetRequest = true;
  }
};

std::string PhlooperLengthQuantity::getDescription() {
  auto* m = dynamic_cast<ComputerscarePhlooper*>(module);
  return string::f("%.3f seconds",
                   (m ? m->frames.load() : 0) / 48000.f * getValue());
}

float PhlooperOffsetQuantity::getDisplayValue() {
  auto* m = dynamic_cast<ComputerscarePhlooper*>(module);
  bool time = !m || m->params[ComputerscarePhlooper::MODE].getValue() == 0.f;
  return getValue() * (time ? 5.f : 1.f);
}
void PhlooperOffsetQuantity::setDisplayValue(float value) {
  auto* m = dynamic_cast<ComputerscarePhlooper*>(module);
  bool time = !m || m->params[ComputerscarePhlooper::MODE].getValue() == 0.f;
  setValue(value / (time ? 5.f : 1.f));
}

std::string PhlooperOffsetQuantity::getUnit() {
  auto* m = dynamic_cast<ComputerscarePhlooper*>(module);
  int mode = m ? int(m->params[ComputerscarePhlooper::MODE].getValue()) : 0;
  return mode == 0 ? " ms per loop" : "% per loop";
}

struct PhlooperButton : SvgSwitch {
  PhlooperButton() {
    momentary = true;
    shadow->opacity = 0.f;
    addFrame(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/components/computerscare-iso-button-up.svg")));
    addFrame(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/components/computerscare-iso-button-down.svg")));
  }
  void step() override {
    SvgSwitch::step();
    if (paramId != ComputerscarePhlooper::HOLD) return;
    auto* m = dynamic_cast<ComputerscarePhlooper*>(module);
    bool held =
        m && (m->params[ComputerscarePhlooper::HOLD].getValue() > .5f ||
              m->inputs[ComputerscarePhlooper::HOLD_GATE].getVoltage() >= 1.f);
    auto frame = frames[held ? 1 : 0];
    if (sw->svg != frame) {
      sw->setSvg(frame);
      fb->setDirty();
    }
  }
};

struct PhlooperChoice : phlooper::WarpedDropdown {
  ComputerscarePhlooper* module = nullptr;
  int param = 0;
  WeakPtr<ui::MenuOverlay> activeMenuOverlay;
  std::string label() override {
    int value = module ? int(module->params[param].getValue()) : 0;
    if (param == ComputerscarePhlooper::COUNT)
      return std::to_string(module ? value : 2) + " loops";
    if (param == ComputerscarePhlooper::MODE)
      return std::string(value == 0   ? "Time ms"
                         : value == 1 ? "Length %"
                                      : "Speed %");
    return std::string(value ? "Add" : "Blend");
  }
  bool isPressed() override {
    auto* overlay = activeMenuOverlay.get();
    return overlay && !overlay->requestedDelete;
  }
  void onButton(const event::Button& e) override {
    if (module && e.action == GLFW_PRESS &&
        e.button == GLFW_MOUSE_BUTTON_LEFT) {
      auto* menu = createMenu();
      activeMenuOverlay = menu->getAncestorOfType<ui::MenuOverlay>();
      int count = param == ComputerscarePhlooper::COUNT  ? 16
                  : param == ComputerscarePhlooper::MODE ? 3
                                                         : 2;
      for (int i = 0; i < count; ++i) {
        int value = param == ComputerscarePhlooper::COUNT ? i + 1 : i;
        std::string name = param == ComputerscarePhlooper::COUNT
                               ? std::to_string(value) + " loops"
                           : param == ComputerscarePhlooper::MODE
                               ? (i == 0   ? "Time (ms)"
                                  : i == 1 ? "Length (%)"
                                           : "Speed (%)")
                               : (i == 0 ? "Blend" : "Add");
        menu->addChild(createCheckMenuItem(
            name, "",
            [=]() { return module->params[param].getValue() == value; },
            [=]() {
              auto* h = new history::ParamChange;
              h->name = "Phlooper mode";
              h->moduleId = module->id;
              h->paramId = param;
              h->oldValue = module->params[param].getValue();
              h->newValue = float(value);
              module->params[param].setValue(float(value));
              APP->history->push(h);
            }));
      }
      e.consume(this);
    }
  }
};
struct PhlooperView : widget::TransparentWidget {
  ComputerscarePhlooper* module = nullptr;
  void draw(const DrawArgs& args) override {
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
    nvgFillColor(args.vg, nvgRGB(13, 29, 28));
    nvgFill(args.vg);
    int count =
        module
            ? clamp(
                  int(module->params[ComputerscarePhlooper::COUNT].getValue()),
                  1, 16)
            : 2;
    float rowHeight = (box.size.y - 12.f) / count;
    float dotScaleY = std::min(1.f, rowHeight * .36f / 7.f);
    float dotScaleX = std::max(.65f, dotScaleY);
    float tickHeight = std::min(5.f, rowHeight * .32f);
    float lineWidth = std::min(2.5f, rowHeight * .25f);
    // Never zoom into shorter loops; include any duration beyond the source
    // buffer.
    float horizon = 1.f;
    for (int i = 0; i < count; ++i)
      if (module) horizon = std::max(horizon, module->ends[i].load());
    auto xAt = [&](float position) {
      return 10.f + (box.size.x - 20.f) * clamp(position / horizon, 0.f, 1.f);
    };
    for (int i = 0; i < count; ++i) {
      float requestedStart = .2f;
      if (module) {
        auto& input = module->inputs[ComputerscarePhlooper::START_CV];
        float voltage = input.getChannels() == 1  ? input.getVoltage()
                        : i < input.getChannels() ? input.getVoltage(i)
                                                  : 0.f;
        requestedStart =
            clamp(module->params[ComputerscarePhlooper::START].getValue() +
                      voltage / 10.f,
                  0.f, 1.f);
      }
      float y = 6.f + rowHeight * (i + .5f);
      float start = module ? module->starts[i].load() : .2f;
      float end = module ? module->ends[i].load() : 1.f;
      float shade = float(i) / 15.f;
      NVGcolor color = nvgRGB(67 + int(123 * shade), 126 + int(104 * shade),
                              119 + int(38 * shade));
      // Buffer guides span the full source buffer for every lane.
      nvgBeginPath(args.vg);
      nvgMoveTo(args.vg, 10.f, y);
      nvgLineTo(args.vg, box.size.x - 10.f, y);
      nvgStrokeColor(args.vg, nvgRGBA(193, 219, 187, 85));
      nvgStrokeWidth(args.vg, .75f);
      nvgStroke(args.vg);
      nvgBeginPath(args.vg);
      nvgMoveTo(args.vg, xAt(start), y);
      nvgLineTo(args.vg, xAt(end), y);
      nvgStrokeColor(args.vg, color);
      nvgStrokeWidth(args.vg, lineWidth);
      nvgStroke(args.vg);
      for (float marker : {requestedStart, end}) {
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, xAt(marker), y - tickHeight);
        nvgLineTo(args.vg, xAt(marker), y + tickHeight);
        nvgStroke(args.vg);
      }
      float position = module ? module->positions[i].load() : start + i * .1f;
      float x = xAt(position);
      // Stable, independent corner offsets give every loop its own silhouette.
      auto warp = [i](unsigned corner) {
        unsigned bits = unsigned(i + 1) * 0x9e3779b9u + corner * 0x85ebca6bu;
        bits ^= bits >> 16;
        bits *= 0x7feb352du;
        bits ^= bits >> 15;
        return float(bits & 1023u) / 1023.f;
      };
      float blobX[8], blobY[8];
      for (int corner = 0; corner < 8; ++corner) {
        float angle = corner * 0.78539816f + (warp(corner + 8) - .5f) * .3f;
        float radius = .55f + .45f * warp(corner);
        blobX[corner] = x + std::cos(angle) * radius * 8.2f * dotScaleX;
        blobY[corner] = y + std::sin(angle) * radius * 7.f * dotScaleY;
      }
      nvgBeginPath(args.vg);
      nvgMoveTo(args.vg, blobX[0], blobY[0]);
      for (int corner = 0; corner < 8; ++corner) {
        int prev = (corner + 7) % 8, next = (corner + 1) % 8;
        int after = (corner + 2) % 8;
        nvgBezierTo(args.vg, blobX[corner] + (blobX[next] - blobX[prev]) * .18f,
                    blobY[corner] + (blobY[next] - blobY[prev]) * .18f,
                    blobX[next] - (blobX[after] - blobX[corner]) * .18f,
                    blobY[next] - (blobY[after] - blobY[corner]) * .18f,
                    blobX[next], blobY[next]);
      }
      nvgClosePath(args.vg);
      nvgFillColor(args.vg, nvgRGB(220, 222, 220));
      nvgFill(args.vg);
      nvgStrokeColor(args.vg, nvgRGB(0, 0, 0));
      nvgStrokeWidth(args.vg, .65f);
      nvgStroke(args.vg);
    }
  }
};
struct PhlooperTiming : widget::TransparentWidget {
  ComputerscarePhlooper* module = nullptr;
  void draw(const DrawArgs& args) override {
    auto font =
        APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 8.5f);
    nvgFillColor(args.vg, nvgRGB(15, 42, 36));
    float base = module ? module->frames.load() / 48000.f : 0.f;
    float length =
        module ? module->params[ComputerscarePhlooper::LENGTH].getValue() : 1.f;
    nvgText(args.vg, 0, 9, string::f("Base: %.3fs", base).c_str(), nullptr);
    nvgText(args.vg, 0, 21, string::f("Loop: %.2fs", base * length).c_str(),
            nullptr);
  }
};
struct PhlooperBackplate : widget::TransparentWidget {
  void draw(const DrawArgs& args) override {
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
    nvgFillColor(args.vg, nvgRGB(222, 222, 222));
    nvgFill(args.vg);
    widget::TransparentWidget::draw(args);
  }
};
struct ComputerscarePhlooperWidget : ModuleWidget {
  ComputerscarePhlooperWidget(ComputerscarePhlooper* module) {
    setModule(module);
    box.size = Vec(330, 380);
    auto* backplate = new PhlooperBackplate;
    backplate->box.size = box.size;
    addChild(backplate);
    auto block = [&](Rect bounds, float depth, float distortion, unsigned seed,
                     int grey) {
      auto* plaque = new phlooper::WarpedBlock;
      plaque->configure(bounds, depth, distortion, seed,
                        nvgRGB(grey, grey, grey));
      backplate->addChild(plaque);
    };
    block(Rect(Vec(9, 3), Vec(156, 31)), 4.f, .9f, 11, 205);
    block(Rect(Vec(174, 3), Vec(147, 31)), 4.f, 1.3f, 12, 210);
    block(Rect(Vec(9, 150), Vec(312, 83)), 4.f, 2.5f, 13, 236);
    block(Rect(Vec(9, 237), Vec(312, 42)), 4.f, 1.7f, 14, 210);
    block(Rect(Vec(9, 282), Vec(312, 83)), 4.f, 3.f, 15, 232);
    auto* panel = new ComputerscareSVGPanel;
    panel->setBackground(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/panels/ComputerscarePhlooperPanel.svg")));
    addChild(panel);
    auto* view = new PhlooperView;
    view->module = module;
    view->box = Rect(Vec(10, 38), Vec(310, 108));
    addChild(view);
    auto* timing = new PhlooperTiming;
    timing->module = module;
    timing->box = Rect(Vec(181, 7), Vec(72, 24));
    addChild(timing);
    for (int i = 0; i < 3; ++i) {
      auto* choice = new PhlooperChoice;
      choice->module = module;
      choice->param = i == 0   ? ComputerscarePhlooper::COUNT
                      : i == 1 ? ComputerscarePhlooper::MODE
                               : ComputerscarePhlooper::OVERDUB;
      choice->box = i == 0   ? Rect(Vec(255, 9), Vec(57, 20))
                    : i == 1 ? Rect(Vec(20, 155), Vec(65, 20))
                             : Rect(Vec(128, 155), Vec(46, 20));
      choice->configure(choice->box.size.x, choice->box.size.y, 4.f, 4.f, 1.2f,
                        unsigned(i + 1));
      addChild(choice);
    }
    const int knobs[] = {
        ComputerscarePhlooper::START,   ComputerscarePhlooper::LENGTH,
        ComputerscarePhlooper::OFFSET,  ComputerscarePhlooper::SPEED,
        ComputerscarePhlooper::REC_MIX, ComputerscarePhlooper::MIX};
    for (int i = 0; i < 6; ++i)
      addParam(createParamCentered<SmoothKnob>(Vec(35 + i * 52, 199), module,
                                               knobs[i]));
    const int buttons[] = {
        ComputerscarePhlooper::RECORD, ComputerscarePhlooper::ERASE,
        ComputerscarePhlooper::RESTART, ComputerscarePhlooper::STOP,
        ComputerscarePhlooper::HOLD};
    for (int i = 0; i < 5; ++i) {
      auto* b = createParamCentered<PhlooperButton>(Vec(34 + i * 49, 255),
                                                    module, buttons[i]);
      b->momentary = i == 1 || i == 2;
      addParam(b);
    }
    addChild(createLightCentered<SmallLight<RedLight>>(
        Vec(55, 247), module, ComputerscarePhlooper::RECORD_LIGHT));
    for (int i = 0; i < 5; ++i)
      addInput(createInputCentered<InPort>(Vec(28 + i * 51, 300), module, i));
    for (int i = 0; i < 5; ++i)
      addInput(
          createInputCentered<InPort>(Vec(28 + i * 51, 347), module, i + 5));
    addInput(createInputCentered<InPort>(Vec(270, 255), module,
                                         ComputerscarePhlooper::HOLD_GATE));
    addOutput(createOutputCentered<PointingUpPentagonPort>(
        Vec(298, 255), module, ComputerscarePhlooper::EOC));
    addOutput(
        createOutputCentered<PointingUpPentagonPort>(Vec(298, 300), module, 0));
    addOutput(
        createOutputCentered<PointingUpPentagonPort>(Vec(298, 347), module, 1));
  }
  void onPathDrop(const event::PathDrop& e) override {
    auto* m = dynamic_cast<ComputerscarePhlooper*>(module);
    if (m && !e.paths.empty()) {
      m->load(e.paths.front());
      e.consume(this);
    }
  }
  void appendContextMenu(Menu* menu) override {
    auto* m = dynamic_cast<ComputerscarePhlooper*>(module);
    if (!m) return;
    menu->addChild(new MenuSeparator);
    menu->addChild(createMenuItem("Load WAV…", "", [m]() {
      char* path = osdialog_file(OSDIALOG_OPEN, nullptr, nullptr, nullptr);
      if (path) {
        m->load(path);
        std::free(path);
        if (!m->error.empty())
          osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, m->error.c_str());
      }
    }));
    menu->addChild(createMenuItem("Clear recording", "",
                                  [m]() { m->resetRequest = true; }));
  }
};
Model* modelComputerscarePhlooper =
    createModel<ComputerscarePhlooper, ComputerscarePhlooperWidget>(
        "computerscare-phlooper");
