
#include "Computerscare.hpp"
#include "Phlooper/Block.hpp"
#include "Phlooper/Dropdown.hpp"
#include "Wind/Generator.hpp"
#include "Wind/Overdrive.hpp"

struct ComputerscareWindyWally : Module {
  enum ParamId {
    STRENGTH,
    GUSTINESS,
    TURBULENCE,
    TONE,
    TEXTURE,
    PRESENCE,
    WIDTH,
    OVERDRIVE,
    GUST,
    PARAMS
  };
  enum InputId {
    STRENGTH_CV,
    GUSTINESS_CV,
    TURBULENCE_CV,
    TONE_CV,
    TEXTURE_CV,
    PRESENCE_CV,
    WIDTH_CV,
    OVERDRIVE_CV,
    GUST_INPUT,
    INPUTS
  };
  enum OutputId { LEFT, RIGHT, WIND, OUTPUTS };
  computerscare::wind::Generator wind;
  dsp::SchmittTrigger externalGust, manualGust;
  float sampleRate = 0.f, strength = .5f, gustiness = .55f, turbulence = .35f;
  float targetDrive = 0.f, drive = 0.f, targetWidth = 1.f, width = 1.f;
  computerscare::wind::Overdrive saturation[2];
  int refresh = 0;

  ComputerscareWindyWally() {
    config(PARAMS, INPUTS, OUTPUTS, 0);
    const char* names[] = {"Strength",     "Gustiness", "Turbulence",
                           "Tone",         "Texture",   "Presence",
                           "Stereo width", "Overdrive"};
    const float defaults[] = {.5f, .55f, .35f, .45f, .4f, .5f, 1.f, 0.f};
    for (int i = 0; i < 8; ++i) {
      configParam(i, 0.f, 1.f, defaults[i], names[i], "%", 0.f, 100.f);
      configInput(i, std::string(names[i]) + " (0–10 V, added to knob)");
    }
    configButton(GUST, "Gust");
    configInput(GUST_INPUT, "Gust (trigger, 1 V threshold)");
    configOutput(LEFT, "Left / mono audio");
    configOutput(RIGHT, "Right audio");
    configOutput(WIND, "Wind strength (0–10 V)");
  }
  float control(int param) {
    return clamp(params[param].getValue() + inputs[param].getVoltage() / 10.f,
                 0.f, 1.f);
  }
  void process(const ProcessArgs& args) override {
    if (sampleRate != args.sampleRate) {
      sampleRate = args.sampleRate;
      refresh = 0;
    }
    if (refresh-- <= 0) {
      refresh = std::max(1, int(sampleRate / 200.f)) - 1;
      strength = control(STRENGTH);
      gustiness = control(GUSTINESS);
      turbulence = control(TURBULENCE);
      wind.configure(sampleRate, control(TONE), control(TEXTURE),
                     control(PRESENCE));
      targetWidth = control(WIDTH);
      targetDrive = control(OVERDRIVE);
    }
    bool button = manualGust.process(params[GUST].getValue());
    bool trigger = externalGust.process(inputs[GUST_INPUT].getVoltage());
    if (button || trigger) wind.gust();
    unsigned mask = (outputs[LEFT].isConnected() ? 1u : 0u) |
                    (outputs[RIGHT].isConnected() ? 2u : 0u);
    auto out = wind.process(strength, gustiness, turbulence, mask);
    float slew = std::min(1.f, args.sampleTime / .01f);
    drive += (targetDrive - drive) * slew;
    width += (targetWidth - width) * slew;
    float left = out.channel[0], right = out.channel[1];
    if (mask == 3) {
      float mid = (left + right) * .5f, side = (left - right) * .5f * width;
      left = mid + side;
      right = mid - side;
    }
    outputs[LEFT].setVoltage(
        5.f * saturation[0].process(left * 3.f, drive, sampleRate));
    outputs[RIGHT].setVoltage(
        5.f * saturation[1].process(right * 3.f, drive, sampleRate));
    outputs[WIND].setVoltage(wind.strength());
  }
};

struct WindyWallyGustButton : app::Switch {
  struct Face : phlooper::WarpedDropdown {
    bool pressed = false;
    std::string label() override { return "Gust"; }
    bool isPressed() override { return pressed; }
  } face;
  WindyWallyGustButton() {
    momentary = true;
    box.size = Vec(51, 22);
    face.configure(58, 22, 4.f, 3.f, 1.2f, 483);
    face.faceColor = nvgRGB(36, 201, 166);
  }
  void draw(const DrawArgs& args) override {
    auto* quantity = getParamQuantity();
    face.pressed = quantity && quantity->getValue() > .5f;
    face.draw(args);
  }
};
struct WindyWallyBackplate : widget::TransparentWidget {
  void draw(const DrawArgs& args) override {
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
    nvgFillColor(args.vg, nvgRGB(222, 222, 222));
    nvgFill(args.vg);
    widget::TransparentWidget::draw(args);
  }
};
struct ComputerscareWindyWallyWidget : ModuleWidget {
  ComputerscareWindyWallyWidget(ComputerscareWindyWally* module) {
    setModule(module);
    box.size = Vec(120, 380);
    auto* backplate = new WindyWallyBackplate;
    backplate->box.size = box.size;
    addChild(backplate);
    auto block = [&](Rect bounds, float distortion, unsigned seed, int grey) {
      auto* face = new phlooper::WarpedBlock;
      face->configure(bounds, 4.f, distortion, seed, nvgRGB(grey, grey, grey));
      backplate->addChild(face);
    };
    block(Rect(Vec(7, 4), Vec(106, 31)), 1.2f, 475, 205);
    block(Rect(Vec(7, 39), Vec(106, 228)), 1.9f, 476, 232);
    block(Rect(Vec(7, 273), Vec(106, 44)), 1.95f, 477, 210);
    block(Rect(Vec(7, 322), Vec(106, 45)), 2.f, 478, 175);
    auto* panel = new ComputerscareSVGPanel;
    panel->setBackground(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/panels/ComputerscareWindyWallyPanel.svg")));
    addChild(panel);
    for (int i = 0; i < 8; ++i) {
      Vec center(i % 2 ? 86.f : 33.f, 65.f + (i / 2) * 56.f);
      addParam(createParamCentered<SmoothKnob>(center, module, i));
      addInput(createInputCentered<JackSmallPointingDown>(center + Vec(0, 24),
                                                          module, i));
    }
    addParam(createParam<WindyWallyGustButton>(Vec(11, 282), module,
                                               ComputerscareWindyWally::GUST));
    addInput(createInputCentered<JackLargePointingDown>(
        Vec(91, 297), module, ComputerscareWindyWally::GUST_INPUT));
    const float outputs[] = {25.f, 60.f, 95.f};
    for (int i = 0; i < 3; ++i)
      addOutput(createOutputCentered<JackLargePointingUp>(Vec(outputs[i], 342),
                                                          module, i));
  }
};
Model* modelComputerscareWindyWally =
    createModel<ComputerscareWindyWally, ComputerscareWindyWallyWidget>(
        "computerscare-windy-wally");
