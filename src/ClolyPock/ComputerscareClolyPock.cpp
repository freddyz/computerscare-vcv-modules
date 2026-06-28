#include <cmath>
#include <string>

#include "../ClolyPockLanguage/ClolyPockLanguage.hpp"
#include "../ComputerscareTextEditor.hpp"

struct ComputerscareClolyPock : Module {
  static constexpr float CLOCK_BPM = 120.f;
  static constexpr float CLOCK_HZ = CLOCK_BPM / 60.f;

  enum ParamIds { NUM_PARAMS };
  enum InputIds { NUM_INPUTS };
  enum OutputIds { CLOCK_OUTPUT, EOC1_OUTPUT, EOC2_OUTPUT, NUM_OUTPUTS };
  enum LightIds { SYNTAX_ERROR_LIGHT, NUM_LIGHTS };

  ComputerscareTextEditorState editorState;
  float clockPhase = 0.f;
  bool clockHigh = false;
  bool syntaxError = false;
  int selectedLine = 0;
  int checkedLine = -1;
  std::string checkedLineText;
  int activeHighlightBegin = 0;
  int activeHighlightEnd = 6;
  cloly::language::ClockSpec activeClockSpec;

  ComputerscareClolyPock() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    configOutput(CLOCK_OUTPUT, "Clock");
    configOutput(EOC1_OUTPUT, "EOC 1");
    configOutput(EOC2_OUTPUT, "EOC 2");
    editorState.text = "120bpm\n33hz\n40ms\n";
    activeClockSpec.bpm = CLOCK_BPM;
    activeClockSpec.hz = CLOCK_HZ;
    activeClockSpec.periodSeconds = 1.f / CLOCK_HZ;
  }

  void process(const ProcessArgs& args) override {
    clockPhase += args.sampleTime * activeClockSpec.hz;
    if (clockPhase >= 1.f) {
      clockPhase -= std::floor(clockPhase);
    }
    clockHigh = clockPhase < 0.5f;
    outputs[CLOCK_OUTPUT].setVoltage(clockHigh ? 10.f : 0.f);
    outputs[EOC1_OUTPUT].setVoltage(0.f);
    outputs[EOC2_OUTPUT].setVoltage(0.f);
    lights[SYNTAX_ERROR_LIGHT].setBrightness(syntaxError ? 1.f : 0.f);
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

  void setSelectedLineProgram(int line, int lineBegin,
                              const std::string& lineText) {
    if (line == checkedLine && lineText == checkedLineText) {
      return;
    }

    checkedLine = line;
    checkedLineText = lineText;
    cloly::language::ParseResult parse =
        cloly::language::parseClockLiteral(lineText);
    if (!parse.ok()) {
      syntaxError = true;
      return;
    }

    cloly::language::EvaluationResult eval =
        cloly::language::evaluateClockLiteral(parse.ast);
    if (!eval.ok()) {
      syntaxError = true;
      return;
    }

    syntaxError = false;
    activeClockSpec = eval.spec;
    activeHighlightBegin = lineBegin + parse.ast.range.begin;
    activeHighlightEnd = lineBegin + parse.ast.range.end;
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
    nvgFillColor(args.vg, nvgRGB(0x18, 0x18, 0x18));
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    nvgText(args.vg, box.size.x / 2.f, 0.f, "Cloly Pock", NULL);
  }
};

struct ClolyPockPanel : Widget {
  void draw(const DrawArgs& args) override {
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
    nvgFillColor(args.vg, nvgRGB(0xf4, 0xf2, 0xec));
    nvgFill(args.vg);

    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0.f, 0.f, box.size.x, 42.f);
    nvgFillColor(args.vg, nvgRGB(0xff, 0xff, 0xfa));
    nvgFill(args.vg);

    nvgBeginPath(args.vg);
    nvgMoveTo(args.vg, 0.f, 42.f);
    nvgLineTo(args.vg, box.size.x, 42.f);
    nvgStrokeColor(args.vg, nvgRGB(0xd6, 0xd2, 0xc8));
    nvgStrokeWidth(args.vg, 1.f);
    nvgStroke(args.vg);
  }
};

struct ClolyPockLineInfo {
  int begin = 0;
  int end = 0;
  std::string text;
};

static ClolyPockLineInfo getLineInfo(const std::string& text, int line) {
  ClolyPockLineInfo info;
  int currentLine = 0;
  int length = text.size();
  info.begin = 0;

  for (int i = 0; i < length; i++) {
    if (currentLine == line) {
      info.begin = i;
      break;
    }
    if (text[i] == '\n') {
      currentLine++;
      info.begin = i + 1;
    }
  }

  if (line > currentLine) {
    info.begin = length;
  }

  info.end = length;
  for (int i = info.begin; i < length; i++) {
    if (text[i] == '\n') {
      info.end = i;
      break;
    }
  }
  info.text = text.substr(info.begin, info.end - info.begin);
  return info;
}

struct ComputerscareClolyPockWidget : ModuleWidget {
  ComputerscareTextEditor* editor = nullptr;
  ComputerscareTextEditorState browserEditorState;

  ComputerscareClolyPockWidget(ComputerscareClolyPock* module) {
    setModule(module);
    box.size = Vec(10 * 15.f, 380.f);

    ClolyPockPanel* panel = new ClolyPockPanel();
    panel->box.size = box.size;
    addChild(panel);

    ClolyPockTitle* title = new ClolyPockTitle();
    title->box.pos = Vec(0.f, 11.f);
    title->box.size = Vec(box.size.x, 24.f);
    addChild(title);

    editor = createWidget<ComputerscareTextEditor>(Vec(8.f, 52.f));
    editor->box.size = Vec(box.size.x - 16.f, 278.f);
    editor->placeholder = "type a sequence...";
    editor->style.backgroundColor = nvgRGB(0x05, 0x06, 0x08);
    editor->style.borderColor = nvgRGB(0x55, 0x5b, 0x64);
    editor->style.textColor = nvgRGB(0xee, 0xee, 0xea);
    editor->style.selectionColor = nvgRGBA(0xe4, 0xc4, 0x21, 0x66);
    editor->style.placeholderColor = nvgRGBA(0xee, 0xee, 0xea, 0x66);
    if (module) {
      editor->setState(&module->editorState);
    } else {
      browserEditorState.text = "120bpm\n33hz\n40ms\n";
      editor->setState(&browserEditorState);
    }
    addChild(editor);

    addChild(createLight<ComputerscareSmallLight<ComputerscareRedLight>>(
        Vec(127.f, 18.f), module, ComputerscareClolyPock::SYNTAX_ERROR_LIGHT));

    addOutput(createOutput<OutPort>(Vec(33.f, 340.f), module,
                                    ComputerscareClolyPock::CLOCK_OUTPUT));
    addOutput(createOutput<OutPort>(Vec(65.f, 340.f), module,
                                    ComputerscareClolyPock::EOC1_OUTPUT));
    addOutput(createOutput<OutPort>(Vec(97.f, 340.f), module,
                                    ComputerscareClolyPock::EOC2_OUTPUT));
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
        clolyPock->selectedLine = editor->getCursorLine();
        ClolyPockLineInfo lineInfo =
            getLineInfo(state->text, clolyPock->selectedLine);
        clolyPock->setSelectedLineProgram(clolyPock->selectedLine,
                                          lineInfo.begin, lineInfo.text);
        blinkHigh = clolyPock->clockHigh;
      } else {
        state = &browserEditorState;
        float browserPhase = std::fmod(
            (float)rack::system::getTime() * ComputerscareClolyPock::CLOCK_HZ,
            1.f);
        blinkHigh = browserPhase < 0.5f;
      }

      state->highlights.clear();
      int line = editor->getCursorLine();
      ClolyPockLineInfo lineInfo = getLineInfo(state->text, line);
      if (clolyPock && clolyPock->syntaxError) {
        ComputerscareTextHighlight errorHighlight;
        errorHighlight.begin = lineInfo.begin;
        errorHighlight.end = std::max(lineInfo.end, lineInfo.begin + 1);
        errorHighlight.background = nvgRGBA(0xc4, 0x34, 0x21, 0x35);
        state->highlights.push_back(errorHighlight);
      }
      if (blinkHigh) {
        ComputerscareTextHighlight blinkHighlight;
        if (clolyPock) {
          blinkHighlight.begin = clolyPock->activeHighlightBegin;
          blinkHighlight.end = clolyPock->activeHighlightEnd;
        } else {
          blinkHighlight.begin = lineInfo.begin;
          blinkHighlight.end = lineInfo.end;
        }
        blinkHighlight.background = nvgRGBA(0xe4, 0xc4, 0x21, 0xd8);
        if (blinkHighlight.begin < blinkHighlight.end) {
          state->highlights.push_back(blinkHighlight);
        }
      }
    }
  }
};

Model* modelComputerscareClolyPock =
    createModel<ComputerscareClolyPock, ComputerscareClolyPockWidget>(
        "computerscare-cloly-pock");
