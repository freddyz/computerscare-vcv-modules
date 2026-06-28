#include <cmath>

#include "../ComputerscareTextEditor.hpp"

struct ComputerscareClolyPock : Module {
  static constexpr float CLOCK_BPM = 120.f;
  static constexpr float CLOCK_HZ = CLOCK_BPM / 60.f;

  enum ParamIds { NUM_PARAMS };
  enum InputIds { NUM_INPUTS };
  enum OutputIds { CLOCK_OUTPUT, NUM_OUTPUTS };
  enum LightIds { NUM_LIGHTS };

  ComputerscareTextEditorState editorState;
  float clockPhase = 0.f;
  bool clockHigh = false;

  ComputerscareClolyPock() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    configOutput(CLOCK_OUTPUT, "Clock");
    editorState.text = "x\n";
  }

  void process(const ProcessArgs& args) override {
    clockPhase += args.sampleTime * CLOCK_HZ;
    if (clockPhase >= 1.f) {
      clockPhase -= std::floor(clockPhase);
    }
    clockHigh = clockPhase < 0.5f;
    outputs[CLOCK_OUTPUT].setVoltage(clockHigh ? 10.f : 0.f);
  }

  json_t* dataToJson() override {
    json_t* rootJ = json_object();
    json_object_set_new(
        rootJ, "text",
        json_stringn(editorState.text.c_str(), editorState.text.size()));
    return rootJ;
  }

  void dataFromJson(json_t* rootJ) override {
    json_t* textJ = json_object_get(rootJ, "text");
    if (textJ) {
      editorState.text = json_string_value(textJ);
      editorState.dirty = true;
    }
  }
};

struct ClolyPockTitle : TransparentWidget {
  void draw(const DrawArgs& args) override {
    std::shared_ptr<Font> font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/Oswald-Regular.ttf"));
    if (!font || font->handle < 0) {
      return;
    }

    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 18.f);
    nvgFillColor(args.vg, nvgRGB(0x07, 0x09, 0x0a));
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    nvgText(args.vg, box.size.x / 2.f, 0.f, "Cloly Pock", NULL);
  }
};

struct ClolyPockPanel : Widget {
  void draw(const DrawArgs& args) override {
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
    nvgFillColor(args.vg, nvgRGB(0xc0, 0xe7, 0xde));
    nvgFill(args.vg);

    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0.f, 0.f, box.size.x, 42.f);
    nvgFillColor(args.vg, nvgRGB(0x24, 0xc9, 0xa6));
    nvgFill(args.vg);

    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0.f, 334.f, box.size.x, box.size.y - 334.f);
    nvgFillColor(args.vg, nvgRGB(0x24, 0x44, 0xc1));
    nvgFill(args.vg);
  }
};

struct ComputerscareClolyPockWidget : ModuleWidget {
  ComputerscareTextEditor* editor = nullptr;
  ComputerscareTextEditorState browserEditorState;

  ComputerscareClolyPockWidget(ComputerscareClolyPock* module) {
    setModule(module);
    box.size = Vec(8 * 15.f, 380.f);

    ClolyPockPanel* panel = new ClolyPockPanel();
    panel->box.size = box.size;
    addChild(panel);

    ClolyPockTitle* title = new ClolyPockTitle();
    title->box.pos = Vec(0.f, 11.f);
    title->box.size = Vec(box.size.x, 24.f);
    addChild(title);

    editor = createWidget<ComputerscareTextEditor>(Vec(8.f, 52.f));
    editor->box.size = Vec(box.size.x - 16.f, 258.f);
    editor->placeholder = "type a sequence...";
    if (module) {
      editor->setState(&module->editorState);
    } else {
      browserEditorState.text = "x\n";
      editor->setState(&browserEditorState);
    }
    addChild(editor);

    addOutput(createOutput<OutPort>(Vec(48.f, 340.f), module,
                                    ComputerscareClolyPock::CLOCK_OUTPUT));
  }

  void step() override {
    ModuleWidget::step();

    ComputerscareClolyPock* clolyPock =
        dynamic_cast<ComputerscareClolyPock*>(module);
    if (editor) {
      ComputerscareTextEditorState* state = nullptr;
      bool blinkHigh = false;

      if (clolyPock) {
        state = &clolyPock->editorState;
        blinkHigh = clolyPock->clockHigh;
      } else {
        state = &browserEditorState;
        float browserPhase = std::fmod(
            (float)rack::system::getTime() * ComputerscareClolyPock::CLOCK_HZ,
            1.f);
        blinkHigh = browserPhase < 0.5f;
      }

      state->highlights.clear();
      if (blinkHigh && !state->text.empty()) {
        ComputerscareTextHighlight blinkHighlight;
        blinkHighlight.begin = 0;
        blinkHighlight.end = 1;
        blinkHighlight.background = nvgRGBA(0xe4, 0xc4, 0x21, 0xd8);
        state->highlights.push_back(blinkHighlight);
      }
    }
  }
};

Model* modelComputerscareClolyPock =
    createModel<ComputerscareClolyPock, ComputerscareClolyPockWidget>(
        "computerscare-cloly-pock");
