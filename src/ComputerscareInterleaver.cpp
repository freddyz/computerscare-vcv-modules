#include <atomic>
#include <memory>

#include "Computerscare.hpp"
#include "Interleaver/Engine.hpp"

struct ComputerscareInterleaver : Module {
  // Preserve the original parameter and audio port IDs for saved patches.
  enum ParamIds {
    TIMING,
    A_LENGTH,
    B_LENGTH,
    BUFFER,
    JOIN,
    OPERATION,
    BALANCE,
    ROUTING,
    PERIOD,
    TIMING_AMOUNT,
    A_AMOUNT,
    B_AMOUNT,
    BUFFER_AMOUNT,
    JOIN_AMOUNT,
    OPERATION_AMOUNT,
    BALANCE_AMOUNT,
    ROUTING_AMOUNT,
    PERIOD_AMOUNT,
    NUM_PARAMS
  };
  enum InputIds {
    A_INPUT,
    B_INPUT,
    TIMING_CV,
    A_CV,
    B_CV,
    BUFFER_CV,
    JOIN_CV,
    OPERATION_CV,
    BALANCE_CV,
    ROUTING_CV,
    PERIOD_CV,
    SWITCH_INPUT,
    RESET_INPUT,
    NUM_INPUTS
  };
  enum OutputIds { AUDIO_OUTPUT, NUM_OUTPUTS };
  std::unique_ptr<interleaver::Engine> engine{new interleaver::Engine};
  std::array<std::atomic<float>, 128> trace{};
  std::atomic<int> traceHead{0}, activeSource{-1}, activeTiming{0},
      activeOperation{0}, activeRouting{0};
  unsigned refresh = 0, traceClock = 0, head = 0;
  float rate = 0;
  ComputerscareInterleaver() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, 0);
    for (auto& value : trace) value.store(0, std::memory_order_relaxed);
    configSwitch(TIMING, 0, 2, 0, "Timing", {"Follow A", "Follow B", "Free"});
    configParam(A_LENGTH, 1, 8, 1, "A count")->snapEnabled = true;
    configParam(B_LENGTH, 1, 8, 1, "B count")->snapEnabled = true;
    configParam(BUFFER, 20, 200, 40, "Buffer lookback", " ms");
    configParam(JOIN, 0, 1, .15f, "Join softening", " ms");
    configSwitch(
        OPERATION, 0, 6, 0, "Operation",
        {"Zero splice", "Half-cycle splice", "Equal-value splice",
         "Cycle weave", "Cycle repeat", "Reverse splice", "Direct switch"});
    configParam(BALANCE, 0, 1, .5f, "Weave balance", "%", 0, 100);
    configSwitch(ROUTING, 0, 3, 0, "Routing",
                 {"Cycle pattern", "Gate: low A / high B", "Trigger toggle",
                  "Timer pattern"});
    configParam(PERIOD, 1, 2000, 100, "Timer interval", " ms");
    getParamQuantity(BUFFER)->description =
        "Slewed lookback/latency; modulation never reverses the read clock";
    getParamQuantity(JOIN)->description =
        "Softens cycle edges; capped at one eighth of each captured cycle";
    getParamQuantity(A_LENGTH)->description =
        "A cycles per run, or A timer intervals";
    getParamQuantity(B_LENGTH)->description =
        "B cycles per run, or B timer intervals";
    getParamQuantity(BALANCE)->description =
        "B's share of cycles in Weave with Cycle routing (0% A, 100% B)";
    const char* names[] = {"Timing",  "A count", "B count",
                           "Buffer",  "Join",    "Operation",
                           "Balance", "Routing", "Timer interval"};
    for (int i = 0; i < 9; ++i) {
      configParam(TIMING_AMOUNT + i, -1, 1, 1,
                  std::string(names[i]) + " CV amount", "%", 0, 100);
      configInput(TIMING_CV + i,
                  std::string(names[i]) + " CV (10 V spans range)");
    }
    configInput(A_INPUT, "Audio A (mono)");
    configInput(B_INPUT, "Audio B (mono)");
    configInput(SWITCH_INPUT,
                "Switch: gate or rising-edge toggle, selected by Routing");
    configInput(RESET_INPUT, "Reset A/B pattern (rising edge)");
    configOutput(AUDIO_OUTPUT, "Interleaved audio");
    configBypass(A_INPUT, AUDIO_OUTPUT);
  }
  float value(int parameter, int input, int amount, float low, float high) {
    return interleaver::cv(params[parameter].getValue(),
                           inputs[input].getVoltage(),
                           params[amount].getValue(), low, high);
  }
  void onReset(const ResetEvent& e) override {
    Module::onReset(e);
    engine->reset();
    refresh = 0;
  }
  void process(const ProcessArgs& args) override {
    if (rate != args.sampleRate) {
      rate = args.sampleRate;
      engine->setSampleRate(rate);
      refresh = 0;
    }
    if (refresh++ % 32 == 0) {
      interleaver::Settings c;
      c.timing = int(std::round(value(TIMING, TIMING_CV, TIMING_AMOUNT, 0, 2)));
      c.a = int(std::round(value(A_LENGTH, A_CV, A_AMOUNT, 1, 8)));
      c.b = int(std::round(value(B_LENGTH, B_CV, B_AMOUNT, 1, 8)));
      c.buffer = value(BUFFER, BUFFER_CV, BUFFER_AMOUNT, 20, 200);
      c.join = value(JOIN, JOIN_CV, JOIN_AMOUNT, 0, 1);
      c.operation = int(
          std::round(value(OPERATION, OPERATION_CV, OPERATION_AMOUNT, 0, 6)));
      c.balance = value(BALANCE, BALANCE_CV, BALANCE_AMOUNT, 0, 1);
      c.routing =
          int(std::round(value(ROUTING, ROUTING_CV, ROUTING_AMOUNT, 0, 3)));
      c.period = value(PERIOD, PERIOD_CV, PERIOD_AMOUNT, 1, 2000);
      engine->configure(c);
    }
    float out = engine->process(
        inputs[A_INPUT].getVoltage(), inputs[B_INPUT].getVoltage(),
        inputs[SWITCH_INPUT].getVoltage(), inputs[RESET_INPUT].getVoltage());
    outputs[AUDIO_OUTPUT].setVoltage(out);
    if (++traceClock >= 32) {
      traceClock = 0;
      trace[head].store(out, std::memory_order_relaxed);
      head = (head + 1) % 128;
      traceHead.store(head, std::memory_order_relaxed);
      activeSource.store(engine->activeSource(), std::memory_order_relaxed);
      activeTiming.store(engine->activeTiming(), std::memory_order_relaxed);
      activeOperation.store(engine->activeOperation(),
                            std::memory_order_relaxed);
      activeRouting.store(engine->activeRouting(), std::memory_order_relaxed);
    }
  }
};
namespace {
struct InterleaverDisplay : widget::OpaqueWidget {
  ComputerscareInterleaver* module = nullptr;
  void draw(const DrawArgs& args) override {
    auto* vg = args.vg;
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, box.size.x, box.size.y);
    nvgFillColor(vg, nvgRGB(15, 28, 27));
    nvgFill(vg);
    nvgBeginPath(vg);
    nvgMoveTo(vg, 0, box.size.y * .5f);
    nvgLineTo(vg, box.size.x, box.size.y * .5f);
    nvgStrokeColor(vg, nvgRGBA(183, 220, 207, 35));
    nvgStrokeWidth(vg, 1);
    nvgStroke(vg);
    int head = module ? module->traceHead.load(std::memory_order_relaxed) : 0;
    int source =
        module ? module->activeSource.load(std::memory_order_relaxed) : 0;
    NVGcolor color = source == 1 ? nvgRGB(238, 184, 112) : nvgRGB(92, 212, 193);
    nvgBeginPath(vg);
    for (int i = 0; i < 128; ++i) {
      float v =
          module
              ? module->trace[(head + i) % 128].load(std::memory_order_relaxed)
              : 3.f * std::sin(i * .23f) * (i < 64 ? 1.f : .65f);
      float y = box.size.y * .5f - displayClamp(v) * box.size.y * .075f;
      if (i == 0)
        nvgMoveTo(vg, 1, y);
      else
        nvgLineTo(vg, 1 + i * (box.size.x - 2) / 127, y);
    }
    nvgStrokeColor(vg, color);
    nvgStrokeWidth(vg, 1.1f);
    nvgStroke(vg);
    auto font =
        APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
    if (font) {
      nvgFontFaceId(vg, font->handle);
      nvgFontSize(vg, 9);
      nvgFillColor(vg, color);
      nvgText(vg, 5, 12,
              source == 1   ? "B"
              : source == 0 ? "A"
                            : "WAIT",
              nullptr);
      int mode =
          module ? module->activeTiming.load(std::memory_order_relaxed) : 0;
      nvgTextAlign(vg, NVG_ALIGN_RIGHT);
      nvgText(vg, box.size.x - 5, 12,
              mode == 2   ? "FREE"
              : mode == 1 ? "FOLLOW B"
                          : "FOLLOW A",
              nullptr);
      const char* operations[] = {"ZERO SPLICE",  "HALF CYCLE",   "EQUAL VALUE",
                                  "CYCLE WEAVE",  "CYCLE REPEAT", "REVERSE",
                                  "DIRECT SWITCH"};
      const char* routes[] = {"CYCLES", "GATE", "TRIGGER", "TIMER"};
      int op =
          module ? module->activeOperation.load(std::memory_order_relaxed) : 0;
      int route =
          module ? module->activeRouting.load(std::memory_order_relaxed) : 0;
      nvgText(vg, box.size.x - 5, box.size.y - 4, routes[route], nullptr);
      nvgTextAlign(vg, NVG_ALIGN_LEFT);
      nvgText(vg, 5, box.size.y - 4, operations[op], nullptr);
    }
  }
  static float displayClamp(float v) {
    return std::max(-5.5f, std::min(v, 5.5f));
  }
};
}  // namespace
struct ComputerscareInterleaverWidget : ModuleWidget {
  ComputerscareInterleaverWidget(ComputerscareInterleaver* module) {
    setModule(module);
    box.size = Vec(360, RACK_GRID_HEIGHT);
    auto* panel = new ComputerscareSVGPanel;
    panel->setBackground(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/panels/ComputerscareInterleaverPanel.svg")));
    addChild(panel);
    auto* display = new InterleaverDisplay;
    display->module = module;
    display->box = Rect(Vec(8, 40), Vec(344, 48));
    addChild(display);
    using M = ComputerscareInterleaver;
    const int params[] = {M::OPERATION, M::TIMING,   M::ROUTING,
                          M::A_LENGTH,  M::B_LENGTH, M::BALANCE,
                          M::BUFFER,    M::JOIN,     M::PERIOD};
    const int cvs[] = {M::OPERATION_CV, M::TIMING_CV, M::ROUTING_CV,
                       M::A_CV,         M::B_CV,      M::BALANCE_CV,
                       M::BUFFER_CV,    M::JOIN_CV,   M::PERIOD_CV};
    const int amounts[] = {
        M::OPERATION_AMOUNT, M::TIMING_AMOUNT, M::ROUTING_AMOUNT,
        M::A_AMOUNT,         M::B_AMOUNT,      M::BALANCE_AMOUNT,
        M::BUFFER_AMOUNT,    M::JOIN_AMOUNT,   M::PERIOD_AMOUNT};
    for (int i = 0; i < 9; ++i) {
      float x = 60 + 120 * (i % 3), y = 116 + 80 * (i / 3);
      if (i < 5)
        addParam(createParamCentered<MediumSnapKnob>(Vec(x - 28, y), module,
                                                     params[i]));
      else
        addParam(
            createParamCentered<SmoothKnob>(Vec(x - 28, y), module, params[i]));
      addInput(createInputCentered<InPort>(Vec(x + 25, y + 3), module, cvs[i]));
      addParam(createParamCentered<SmallKnob>(Vec(x + 25, y + 30), module,
                                              amounts[i]));
    }
    addInput(createInputCentered<InPort>(Vec(40, 341), module, M::A_INPUT));
    addInput(createInputCentered<InPort>(Vec(105, 341), module, M::B_INPUT));
    addInput(
        createInputCentered<InPort>(Vec(185, 341), module, M::SWITCH_INPUT));
    addInput(
        createInputCentered<InPort>(Vec(260, 341), module, M::RESET_INPUT));
    addOutput(createOutputCentered<PointingUpPentagonPort>(
        Vec(320, 341), module, M::AUDIO_OUTPUT));
  }
};
Model* modelComputerscareInterleaver =
    createModel<ComputerscareInterleaver, ComputerscareInterleaverWidget>(
        "computerscare-interleaver");
