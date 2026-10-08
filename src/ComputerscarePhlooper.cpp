#include <osdialog.h>

#include <atomic>
#include <mutex>

#include "Computerscare.hpp"
#include "Phlooper/Engine.hpp"
#include "Phlooper/Wav.hpp"

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
    INPUTS
  };
  enum Output { OUT_L, OUT_R, OUTPUTS };
  phlooper::Engine loop;
  std::mutex storage;
  std::atomic<int> frames{0};
  std::array<std::atomic<float>, 16> positions{};
  std::atomic<bool> recording{false};
  std::atomic<bool> resetRequest{false};
  bool recordLatch = false, firstGate = false;
  dsp::SchmittTrigger recordButton, restartButton;
  std::array<dsp::SchmittTrigger, 16> retrigger;
  std::string error;
  ComputerscarePhlooper() {
    config(PARAMS, INPUTS, OUTPUTS);
    configParam(START, 0, 1, 0, "Start", "%", 0, 100);
    configParam(LENGTH, .001, 1, 1, "Length", "%", 0, 100);
    configParam(OFFSET, -20, 20, 1, "Phase offset");
    configParam(REC_MIX, 0, 1, .5, "Record mix", "%", 0, 100);
    configParam(MIX, 0, 1, 1, "Output mix", "%", 0, 100);
    configParam(COUNT, 1, 16, 2, "Active loops")->snapEnabled = true;
    configSwitch(MODE, 0, 2, 0, "Phase mode",
                 {"Time (ms)", "Length (%)", "Speed (%)"});
    configSwitch(OVERDUB, 0, 1, 0, "Overdub", {"Blend", "Add"});
    configButton(RECORD, "Toggle record all");
    configButton(ERASE, "Erase all while held");
    configButton(RESTART, "Restart all");
    const char* names[] = {
        "Audio left / mono", "Audio right", "Record gates", "Erase gates",
        "Restart triggers",  "Mute gates",  "Stop gates",   "Start CV",
        "Length CV",         "Offset CV"};
    for (int i = 0; i < INPUTS; ++i) configInput(i, names[i]);
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
      recordLatch = false;
      frames = 0;
    }
    if (recordButton.process(params[RECORD].getValue()))
      recordLatch = !recordLatch;
    unsigned rec = gates(REC_GATE) | (recordLatch ? 65535u : 0u);
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
    s.start =
        clamp(params[START].getValue() + inputs[START_CV].getVoltage() / 10.f,
              0.f, 1.f);
    s.length =
        clamp(params[LENGTH].getValue() + inputs[LENGTH_CV].getVoltage() / 10.f,
              .001f, 1.f);
    s.offset =
        clamp(params[OFFSET].getValue() + inputs[OFFSET_CV].getVoltage() * 2.f,
              -20.f, 20.f);
    s.recordMix = params[REC_MIX].getValue();
    s.mix = params[MIX].getValue();
    auto out = loop.process(
        input, args.sampleRate, s, rec,
        gates(ERASE_GATE) | (params[ERASE].getValue() > .5f ? 65535u : 0u),
        restart, gates(MUTE_GATE), gates(STOP_GATE),
        inputs[RIGHT].isConnected());
    outputs[OUT_L].setVoltage(out.l);
    outputs[OUT_R].setVoltage(out.r);
    frames.store(loop.initial ? loop.captured : loop.size,
                 std::memory_order_relaxed);
    recording.store(loop.initial || rec, std::memory_order_relaxed);
    for (int i = 0; i < 16; ++i)
      positions[i].store(loop.size ? float(loop.head[i] / loop.size) : 0.f,
                         std::memory_order_relaxed);
  }
  void load(const std::string& path) {
    try {
      auto wav = phlooper::loadWav(path);
      int available = int(wav.samples.size() / wav.channels);
      int n = std::min(phlooper::capacity,
                       int(double(available) * phlooper::rate / wav.rate));
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
        loop.stereo[i] = wav.channels == 2;
      }
      loop.size = n;
      loop.initial = false;
      loop.resetHeads();
      recordLatch = false;
      frames = n;
      error.clear();
    } catch (const std::exception& e) {
      error = e.what();
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
        loop.stereo[i] = wav.channels == 2;
      } catch (const std::exception&) {
        if (loop.size) loop.size = 0;
        break;
      }
    }
    frames = loop.size;
  }
  void onReset(const ResetEvent& e) override {
    Module::onReset(e);
    resetRequest = true;
  }
};

struct PhlooperChoice : widget::OpaqueWidget {
  ComputerscarePhlooper* module = nullptr;
  int param = 0;
  std::shared_ptr<Svg> svg;
  std::string label() {
    int value = module ? int(module->params[param].getValue()) : 0;
    if (param == ComputerscarePhlooper::COUNT)
      return std::to_string(module ? value : 2) + " loops  v";
    if (param == ComputerscarePhlooper::MODE)
      return std::string(value == 0   ? "Time ms"
                         : value == 1 ? "Length %"
                                      : "Speed %") +
             "  v";
    return std::string(value ? "Add" : "Blend") + "  v";
  }
  void draw(const DrawArgs& args) override {
    if (!svg)
      svg = APP->window->loadSvg(asset::plugin(
          pluginInstance, "res/components/computerscare-blank-button.svg"));
    nvgSave(args.vg);
    nvgScale(args.vg, box.size.x / svg->getSize().x,
             box.size.y / svg->getSize().y);
    svgDraw(args.vg, svg->handle);
    nvgRestore(args.vg);
    auto font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/Oswald-Regular.ttf"));
    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 10);
    nvgFillColor(args.vg, nvgRGB(15, 42, 36));
    nvgText(args.vg, 3, 11, label().c_str(), nullptr);
  }
  void onButton(const event::Button& e) override {
    if (module && e.action == GLFW_PRESS &&
        e.button == GLFW_MOUSE_BUTTON_LEFT) {
      auto* menu = createMenu();
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
    auto font =
        APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 9);
    nvgFillColor(args.vg, nvgRGB(180, 220, 190));
    int n = module ? module->frames.load() : 0;
    std::string caption =
        module ? (module->recording.load() ? "Recording  " : "Phlooper  ") +
                     std::to_string(n / 48000.f).substr(0, 5) + " s"
               : "Phlooper";
    nvgText(args.vg, 7, 14, caption.c_str(), nullptr);
    int count =
        module ? int(module->params[ComputerscarePhlooper::COUNT].getValue())
               : 2;
    for (int i = 0; i < count; ++i) {
      float y = 24 + i * 5.f;
      nvgBeginPath(args.vg);
      nvgMoveTo(args.vg, 7, y);
      nvgLineTo(args.vg, box.size.x - 7, y);
      nvgStrokeColor(args.vg, nvgRGBA(100, 150, 140, 65));
      nvgStrokeWidth(args.vg, 1);
      nvgStroke(args.vg);
      float x = 7 + (box.size.x - 14) *
                        (module ? module->positions[i].load() : i * .13f);
      nvgBeginPath(args.vg);
      nvgCircle(args.vg, x, y, 2);
      nvgFillColor(args.vg, nvgHSLA(i / 16.f, .6, .65, 255));
      nvgFill(args.vg);
    }
  }
};
struct ComputerscarePhlooperWidget : ModuleWidget {
  ComputerscarePhlooperWidget(ComputerscarePhlooper* module) {
    setModule(module);
    box.size = Vec(330, 380);
    auto* panel = new ComputerscareSVGPanel;
    panel->setBackground(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/panels/ComputerscarePhlooperPanel.svg")));
    addChild(panel);
    auto* view = new PhlooperView;
    view->module = module;
    view->box = Rect(Vec(10, 38), Vec(310, 108));
    addChild(view);
    for (int i = 0; i < 3; ++i) {
      auto* choice = new PhlooperChoice;
      choice->module = module;
      choice->param = i == 0   ? ComputerscarePhlooper::COUNT
                      : i == 1 ? ComputerscarePhlooper::MODE
                               : ComputerscarePhlooper::OVERDUB;
      choice->box = Rect(Vec(20 + i * 103, 158), Vec(86, 17));
      addChild(choice);
    }
    for (int i = 0; i < 5; ++i)
      addParam(
          createParamCentered<SmoothKnob>(Vec(35 + i * 64, 207), module, i));
    for (int i = 0; i < 3; ++i) {
      auto* b = createParamCentered<IsoButton>(
          Vec(52 + i * 106, 255), module, ComputerscarePhlooper::RECORD + i);
      b->momentary = true;
      addParam(b);
    }
    for (int i = 0; i < 5; ++i)
      addInput(createInputCentered<InPort>(Vec(28 + i * 51, 300), module, i));
    for (int i = 0; i < 5; ++i)
      addInput(
          createInputCentered<InPort>(Vec(28 + i * 51, 347), module, i + 5));
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
