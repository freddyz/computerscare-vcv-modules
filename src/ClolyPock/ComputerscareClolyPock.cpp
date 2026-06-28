#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "../ClolyPockLanguage/ClolyPockLanguage.hpp"
#include "../ComputerscareTextEditor.hpp"

struct ClolyPockProgramStep {
  cloly::language::ClockSpec spec;
  int repeat = 1;
  int probability = 100;
  int highlightBegin = 0;
  int highlightEnd = 0;
};

struct ComputerscareClolyPock : Module {
  static constexpr float CLOCK_BPM = 120.f;
  static constexpr float CLOCK_HZ = CLOCK_BPM / 60.f;

  enum ParamIds { NUM_PARAMS };
  enum InputIds { NUM_INPUTS };
  enum OutputIds { CLOCK_OUTPUT, EOC1_OUTPUT, EOC2_OUTPUT, NUM_OUTPUTS };
  enum LightIds { SYNTAX_ERROR_LIGHT, NUM_LIGHTS };

  ComputerscareTextEditorState editorState;
  float clockPhase = 0.f;
  float activeClockRamp = 0.f;
  bool clockHigh = false;
  bool syntaxError = false;
  int selectedLine = 0;
  bool selectedLineDirty = false;
  int checkedLine = -1;
  std::string checkedLineText;
  int activeHighlightBegin = 0;
  int activeHighlightEnd = 6;
  cloly::language::ClockSpec activeClockSpec;
  std::vector<ClolyPockProgramStep> activeProgram;
  int activeProgramIndex = 0;
  int activeProgramBeat = 0;
  bool activeStepPlays = true;

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
    while (clockPhase >= 1.f) {
      clockPhase -= 1.f;
      advanceActiveProgram();
    }
    activeClockRamp = clockPhase;
    clockHigh = clockPhase < 0.5f;
    outputs[CLOCK_OUTPUT].setVoltage(clockHigh && activeStepPlays ? 10.f : 0.f);
    outputs[EOC1_OUTPUT].setVoltage(0.f);
    outputs[EOC2_OUTPUT].setVoltage(0.f);
    lights[SYNTAX_ERROR_LIGHT].setBrightness(syntaxError ? 1.f : 0.f);
  }

  json_t* dataToJson() override {
    json_t* rootJ = json_object();
    json_object_set_new(
        rootJ, "text",
        json_stringn(editorState.text.c_str(), editorState.text.size()));
    json_object_set_new(rootJ, "focusedLine", json_integer(selectedLine));
    return rootJ;
  }

  void dataFromJson(json_t* rootJ) override {
    json_t* textJ = json_object_get(rootJ, "text");
    if (textJ) {
      editorState.text = json_string_value(textJ);
      editorState.dirty = true;
    }
    json_t* focusedLineJ = json_object_get(rootJ, "focusedLine");
    if (focusedLineJ) {
      selectedLine = json_integer_value(focusedLineJ);
      selectedLineDirty = true;
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

    std::vector<ClolyPockProgramStep> program;
    for (size_t i = 0; i < parse.program.blocks.size(); i++) {
      const cloly::language::ClockBlockAst& block = parse.program.blocks[i];
      cloly::language::EvaluationResult eval =
          cloly::language::evaluateClockLiteral(block.literal);
      if (!eval.ok()) {
        syntaxError = true;
        return;
      }

      ClolyPockProgramStep step;
      step.spec = eval.spec;
      step.repeat = std::max(1, block.repeat);
      step.probability = std::max(0, std::min(100, block.probability));
      step.highlightBegin = lineBegin + block.literal.range.begin;
      step.highlightEnd = lineBegin + block.literal.range.end;
      program.push_back(step);
    }

    if (program.empty()) {
      syntaxError = true;
      return;
    }

    syntaxError = false;
    activeProgram = program;
    activeProgramIndex = 0;
    activeProgramBeat = 0;
    activeStepPlays = true;
    clockPhase = 0.f;
    activeClockRamp = 0.f;
    applyActiveProgramStep();
  }

  void applyActiveProgramStep() {
    if (activeProgram.empty()) {
      return;
    }

    activeProgramIndex = std::max(
        0, std::min(activeProgramIndex, (int)activeProgram.size() - 1));
    const ClolyPockProgramStep& step = activeProgram[activeProgramIndex];
    activeClockSpec = step.spec;
    activeHighlightBegin = step.highlightBegin;
    activeHighlightEnd = step.highlightEnd;
    chooseActiveStepPlayback();
  }

  void advanceActiveProgram() {
    if (activeProgram.empty()) {
      return;
    }

    activeProgramBeat++;
    if (activeProgramBeat >= activeProgram[activeProgramIndex].repeat) {
      activeProgramBeat = 0;
      activeProgramIndex++;
      if (activeProgramIndex >= (int)activeProgram.size()) {
        activeProgramIndex = 0;
      }
      applyActiveProgramStep();
    } else {
      chooseActiveStepPlayback();
    }
  }

  void chooseActiveStepPlayback() {
    if (activeProgram.empty()) {
      activeStepPlays = true;
      return;
    }

    int probability = activeProgram[activeProgramIndex].probability;
    if (probability >= 100) {
      activeStepPlays = true;
    } else if (probability <= 0) {
      activeStepPlays = false;
    } else {
      activeStepPlays = random::uniform() * 100.f < probability;
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
      float activeProgress = 0.f;

      if (clolyPock) {
        state = &clolyPock->editorState;
        if (clolyPock->selectedLineDirty) {
          editor->setCursorLine(clolyPock->selectedLine);
          clolyPock->selectedLineDirty = false;
        }
        clolyPock->selectedLine = editor->getCursorLine();
        ClolyPockLineInfo lineInfo =
            getLineInfo(state->text, clolyPock->selectedLine);
        clolyPock->setSelectedLineProgram(clolyPock->selectedLine,
                                          lineInfo.begin, lineInfo.text);
        blinkHigh = clolyPock->clockHigh && clolyPock->activeStepPlays;
        activeProgress = clolyPock->activeClockRamp;
      } else {
        state = &browserEditorState;
        float browserPhase = std::fmod(
            (float)rack::system::getTime() * ComputerscareClolyPock::CLOCK_HZ,
            1.f);
        blinkHigh = browserPhase < 0.5f;
        activeProgress = browserPhase;
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
      ComputerscareTextHighlight activeHighlight;
      if (clolyPock) {
        activeHighlight.begin = clolyPock->activeHighlightBegin;
        activeHighlight.end = clolyPock->activeHighlightEnd;
      } else {
        activeHighlight.begin = lineInfo.begin;
        activeHighlight.end = lineInfo.end;
      }
      activeHighlight.hasBackground = blinkHigh;
      activeHighlight.background = nvgRGBA(0xc8, 0x9f, 0x16, 0x66);
      activeHighlight.hasBorder = true;
      activeHighlight.border = nvgRGBA(0xff, 0xee, 0x9a, 0xdd);
      activeHighlight.hasProgress = true;
      activeHighlight.progress = activeProgress;
      activeHighlight.progressColor = COLOR_COMPUTERSCARE_GREEN;
      if (activeHighlight.begin < activeHighlight.end) {
        state->highlights.push_back(activeHighlight);
      }
    }
  }
};

Model* modelComputerscareClolyPock =
    createModel<ComputerscareClolyPock, ComputerscareClolyPockWidget>(
        "computerscare-cloly-pock");
