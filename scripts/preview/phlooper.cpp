// Development-only renderer. Uses the real module/widget, never a panel mockup.
#define modelComputerscarePhlooper modelPhlooperPreview
#include "../../src/ComputerscarePhlooper.cpp"
#undef modelComputerscarePhlooper
#define modelComputerscareBlunch modelBlunchPreview
#include "../../src/Blunch/ComputerscareBlunch.cpp"
#undef modelComputerscareBlunch

#include <stb_image_write.h>

#include <algorithm>
#include <app/Scene.hpp>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <thread>
#include <chrono>
#include <engine/Engine.hpp>
#include <history.hpp>
#include <memory>
#include <patch.hpp>
#include <random.hpp>
#include <settings.hpp>
#include <system.hpp>
#include <ui/common.hpp>

using namespace rack;
#include "windchimes-checks.hpp"

namespace {
struct PreviewContainer : widget::Widget {
  void draw(const DrawArgs& args) override {
    Widget::draw(args);
    Widget::drawLayer(args, 1);
  }
};

void simulate(ComputerscarePhlooper& module, float seconds, int64_t& frame) {
  engine::Module::ProcessArgs args;
  args.sampleRate = 48000.f;
  args.sampleTime = 1.f / args.sampleRate;
  for (int i = 0; i < int(seconds * args.sampleRate); ++i) {
    args.frame = frame++;
    float t = args.frame * args.sampleTime;
    // A pulsing stereo tone with changing dynamics makes waveform detail clear.
    float envelope =
        .08f + .7f * std::pow(.5f + .5f * std::sin(2.f * M_PI * 2.f * t), 4.f);
    module.inputs[ComputerscarePhlooper::LEFT].setVoltage(
        4.f * envelope *
        (std::sin(2.f * M_PI * 110.f * t) +
         .25f * std::sin(2.f * M_PI * 331.f * t)));
    module.inputs[ComputerscarePhlooper::RIGHT].setVoltage(
        4.f * envelope * std::sin(2.f * M_PI * 165.f * t));
    module.process(args);
  }
}

void capture(widget::Widget* widget, const std::string& path, float scale,
             bool advance = true) {
  // Step while unattached so the Rack-only keyboard overlay isn't installed.
  if (advance) widget->step();
  auto framebuffer =
      std::unique_ptr<widget::FramebufferWidget>(new widget::FramebufferWidget);
  framebuffer->oversample = 2.f;
  auto* container = new PreviewContainer;
  framebuffer->addChild(container);
  container->box.size = widget->box.size;
  framebuffer->box.size = widget->box.size;
  container->addChild(widget);
  framebuffer->render(math::Vec(scale, scale));
  if (!framebuffer->getFramebuffer())
    throw std::runtime_error("Could not render framebuffer");
  nvgluBindFramebuffer(framebuffer->getFramebuffer());
  int width = 0, height = 0;
  nvgImageSize(APP->window->vg, framebuffer->getImageHandle(), &width, &height);
  std::vector<unsigned char> pixels(width * height * 4);
  glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
  for (int y = 0; y < height / 2; ++y)
    for (int x = 0; x < width * 4; ++x)
      std::swap(pixels[y * width * 4 + x],
                pixels[(height - 1 - y) * width * 4 + x]);
  if (!stbi_write_png(path.c_str(), width, height, 4, pixels.data(), width * 4))
    throw std::runtime_error("Could not write " + path);
  nvgluBindFramebuffer(nullptr);
  container->removeChild(widget);
  std::printf("%s (%d x %d)\n", path.c_str(), width, height);
}
void renderBlunchControls(plugin::Plugin& plugin, const std::string& output,
                          float scale) {
  auto require = [](bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
  };
  modelBlunchPreview->plugin = &plugin;
  auto* module =
      static_cast<ComputerscareBlunch*>(modelBlunchPreview->createModule());
  require(module->playbackDisplayStyle == 1, "New modules default to Minimal blinkers");
  require(module->params[ComputerscareBlunch::POLY_CHANNELS_PARAM].getValue() == 0.f,
          "New modules default to automatic polyphony");
  {
    auto draft = std::unique_ptr<ComputerscareBlunch>(
        static_cast<ComputerscareBlunch*>(modelBlunchPreview->createModule()));
    for (int ch = 0; ch < 2; ++ch) {
      draft->runningLineTexts[ch].clear();
      draft->sequenceEditorStates[ch].text = "120bpm";
      require(draft->commitLineForChannel(ch, 0, true), "pending test baseline compiles");
      draft->focusedChannel = ch;
      draft->sequenceEditorStates[ch].text = "140bpm";
      draft->inspectFocusedLine(0, false);
      require(draft->channelHasPendingFormula(ch), "edited formula marks channel pending");
      for (int loop = 0; loop < 3; ++loop) draft->moveToNextLine(false);
      require(draft->sequencers[ch].activeLineText == "120bpm",
              "looping never submits edited text automatically");
    }
    draft->sequenceEditorStates[0].text = "120bpm";
    draft->focusedChannel = 0;
    draft->inspectFocusedLine(0, false);
    require(!draft->channelHasPendingFormula(0), "reverting an edit clears pending");
    draft->sequenceEditorStates[0].text = "140bpm";
    draft->inspectFocusedLine(0, false);
    draft->params[ComputerscareBlunch::WAIT_MODE_PARAM].setValue(ComputerscareBlunch::WAIT_CLOCK);
    for (int ch = 0; ch < 2; ++ch) {
      draft->focusedChannel = ch;
      require(draft->executeLine(0, true), "submitted drafts queue independently");
      require(draft->sequencers[ch].activeLineText == "120bpm", "Wait keeps old formula until boundary");
    }
    draft->focusedChannel = 1;
    blunchProcessingChannel = 0;
    require(draft->activatePendingLineAtBoundary(ComputerscareBlunch::WAIT_BOUNDARY_CLOCK, true),
            "queued channel activates even while another channel is focused");
    require(!draft->channelHasPendingFormula(0) && draft->channelHasPendingFormula(1),
            "activation clears only its own pending indicator");
    blunchProcessingChannel = 1;
    require(draft->activatePendingLineAtBoundary(ComputerscareBlunch::WAIT_BOUNDARY_CLOCK, true),
            "second channel retains its queued formula");
    blunchProcessingChannel = -1;
    draft->focusedChannel = 0;
    draft->runningLineTexts[0].clear();
    draft->sequenceEditorStates[0].text = "120bpm\n90bpm";
    draft->commitLine(0, true);
    draft->sequenceEditorStates[0].text = "120bpm\n150bpm";
    draft->moveToNextLine(false);
    require(draft->sequencers[0].activeLineText == "90bpm" && draft->channelHasPendingFormula(0),
            "page advance retains the submitted version of edited lines");
    draft->params[ComputerscareBlunch::WAIT_MODE_PARAM].setValue(ComputerscareBlunch::WAIT_IMMEDIATE);
    draft->executeLine(1, true);
    require(draft->sequencers[0].activeLineText == "150bpm" && !draft->channelHasPendingFormula(0),
            "explicit submit applies edits and clears pending");
  }
  APP->engine->addModule(module);
  auto widget = std::unique_ptr<ComputerscareBlunchWidget>(
      static_cast<ComputerscareBlunchWidget*>(
          modelBlunchPreview->createModuleWidget(module)));
  const char* formulas[] = {"120bpm",  "[300 400]ms",   "(.5 .7 (.3 .4))s",
                            "90bpm@4", "{100|140}bpm",  "250ms ~250ms",
                            "2hz",     "[110 130]bpm@2"};
  for (int ch = 0; ch < 16; ++ch) {
    module->runningLineTexts[ch].clear();
    module->sequenceEditorStates[ch].text = formulas[ch % 8];
    if (ch == 1) module->sequenceEditorStates[ch].text += "\n180bpm";
    require(module->commitLineForChannel(ch, 0, true),
            "preview formula must compile");
    module->sequencers[ch].running = true;
  }
  {
    auto line = getLineInfo(module->sequenceEditorStates[0].text, 0);
    auto displaySequence = module->sequencers[0];
    displaySequence.activeClockDisplayPulse = 1.f;
    for (int style = 0; style < 5; ++style) for (bool bars : {false, true}) {
      std::vector<ComputerscareTextHighlight> highlights;
      addBlunchSequencerHighlights(highlights, displaySequence, line, line, true, style, bars);
      require(!highlights.empty(), "Blinker style retains the active token");
      if (style == 3)
        for (const auto& highlight : highlights)
          require(!highlight.hasBorder && !highlight.hasForeground && !highlight.hasBackground,
                  "Off removes blinkers while leaving Bars independent");
      require((highlights.front().backgroundDistortion > 0.f) == (style <= 1 || style == 4),
              "Full and Minimal share the polygon geometry");
      require(highlights.front().hasBorder == (style == 0 || style == 4) &&
                  highlights.front().hasForeground == (style == 2),
              "Bars do not change the chosen blinker style");
      bool hasBars = false;
      for (const auto& highlight : highlights) hasBars |= highlight.hasProgress;
      require(hasBars == bars, "Bars switch works with every blinker style");
    }
  }
  {
    auto sequence = module->sequencers[0];
    sequence.activeClockDisplayPulse = 0.f;
    auto line = getLineInfo(module->sequenceEditorStates[0].text, 0);
    std::vector<ComputerscareTextHighlight> highlights;
    addBlunchSequencerHighlights(highlights, sequence, line, line, true, 4, false);
    for (const auto& highlight : highlights)
      require(!highlight.hasBorder && !highlight.hasBackground && !highlight.hasForeground,
              "Border blinker disappears between triggers");
  }
  module->params[ComputerscareBlunch::POLY_CHANNELS_PARAM].setValue(8.f);
  module->setControlMode(true);
  widget->step();
  auto* editor = static_cast<BlunchTextEditor*>(widget->editor);
  if (std::getenv("VCVMON_PREVIEW_CHECKS")) {
    const char* path = std::getenv("VCVMON_CONTROL");
    require(path, "Watcher checks need a control path");
    std::string control(path);
    widget::Widget* toolbar = nullptr;
    for (auto* child : APP->scene->children)
      if (child->box.size == Vec(370, 28)) toolbar = child;
    require(toolbar, "Watcher toolbar attaches to the Rack scene");
    auto pollState = [&](const char* state) {
      { std::ofstream status(control + ".status"); status << state << " " << std::time(nullptr); }
      std::this_thread::sleep_for(std::chrono::milliseconds(300));
      toolbar->step();
    };
    auto click = [&](const char* name, const char* expected) {
      std::remove((control + ".command").c_str());
      bool found = false;
      for (auto* child : toolbar->children) {
        auto* button = dynamic_cast<ui::Button*>(child);
        if (!button || button->text != name) continue;
        widget::Widget::ActionEvent event;
        button->onAction(event);
        found = true;
      }
      require(found, "Watcher toolbar button has the correct state label");
      std::ifstream command(control + ".command");
      std::string value; command >> value;
      require(value == expected, "Watcher toolbar sends the requested command");
    };
    auto renderToolbar = [&](const char* name) {
      Vec position = toolbar->box.pos;
      APP->scene->removeChild(toolbar);
      toolbar->box.pos = Vec();
      capture(toolbar, output + "/" + name, scale, false);
      toolbar->box.pos = position;
      APP->scene->addChild(toolbar);
    };
    pollState("watching"); renderToolbar("02-watcher-watching.png"); click("Pause", "pause");
    pollState("queued"); renderToolbar("03-watcher-paused.png"); click("Resume", "resume");
    pollState("watching"); click("Rebuild", "rebuild");
    pollState("building"); click("Rebuild", "");
    pollState("failed"); click("Rebuild", "rebuild");
    std::puts("Watcher toolbar state and command checks passed");
  }
  // The preview scene can retain Rack's startup menus; dismiss them before
  // testing panel focus rather than intentionally deferring focus to a menu.
  for (auto* child : APP->scene->children)
    if (auto* overlay = dynamic_cast<ui::MenuOverlay*>(child))
      overlay->requestDelete();
  APP->event->setSelectedWidget(nullptr);
  {
    widget::EventContext context;
    widget::Widget::HoverKeyEvent event;
    event.context = &context;
    event.key = GLFW_KEY_SPACE;
    event.action = GLFW_PRESS;
    event.pos = Vec(3, 180);
    widget->onHoverKey(event);
    require(module->controlToggleRequests.load() == 0 && !widget->controlFocused(),
            "Hover alone does not activate Control shortcuts");
  }
  {
    widget::EventContext context;
    widget::Widget::ButtonEvent event;
    event.context = &context;
    event.button = GLFW_MOUSE_BUTTON_LEFT;
    event.action = GLFW_PRESS;
    event.pos = Vec(3, 180);
    widget->onButton(event);
    widget->step();
    require(APP->event->getSelectedWidget() == editor && widget->controlFocused(),
            "Clicking the panel in Control mode focuses its editor");
    APP->event->setSelectedWidget(nullptr);
    require(!widget->controlFocused(), "Focus indicator clears on focus loss");
  }
  APP->event->setSelectedWidget(editor);
  auto key = [&](int code, int mods = 0) {
    widget::EventContext context;
    widget::Widget::SelectKeyEvent event;
    event.context = &context;
    event.key = code;
    event.mods = mods;
    event.action = GLFW_PRESS;
    editor->onSelectKey(event);
    widget->step();
  };
  key(GLFW_KEY_DOWN);
  key(GLFW_KEY_DOWN, GLFW_MOD_SHIFT);
  key(GLFW_KEY_DOWN, GLFW_MOD_SHIFT);
  for (int i = 0; i < 11; ++i) key(GLFW_KEY_DOWN);
  key(GLFW_KEY_ENTER, GLFW_MOD_CONTROL);
  unsigned selected = 14u | (1u << 14);
  require(module->controlSelection.targets() == selected,
          "UI keyboard selection selects channels 2, 3, 4, 15");
  key(GLFW_KEY_SPACE);
  module->processControlRequests();
  require(!module->sequencers[1].running && !module->sequencers[14].running &&
              module->sequencers[0].running,
          "space stops only selected channels");
  key(GLFW_KEY_SPACE);
  module->processControlRequests();
  require(module->sequencers[1].running && module->sequencers[14].running,
          "space restarts selected channels");
  {
    auto pressTransport = [&](BlunchChannelTransport* button) {
      widget::EventContext context;
      widget::Widget::ButtonEvent event;
      event.context = &context;
      event.button = GLFW_MOUSE_BUTTON_LEFT;
      event.action = GLFW_PRESS;
      button->onButton(event);
      event.action = GLFW_RELEASE;
      button->onButton(event);
      module->processControlRequests();
      widget->step();
    };
    module->params[ComputerscareBlunch::POLY_CHANNELS_PARAM].setValue(16.f);
    pressTransport(widget->channelStop);
    require(widget->channelPlay->tooltipText() == "Channel 15: stopped" &&
                widget->channelStop->tooltipText() == "Stop channel 15",
            "Transport tooltips identify the focused channel and playback state");
    require(!module->sequencers[14].running && module->sequencers[1].running &&
                !widget->channelPlay->isPressed(),
            "Stop button affects only the focused channel and clears Play state");
    pressTransport(widget->channelPlay);
    require(module->sequencers[14].running && widget->channelPlay->isPressed(),
            "Play button starts the focused channel and reflects playback");
    require(widget->channelPlay->tooltipText() == "Channel 15: playing",
            "Play tooltip follows the channel playback state");
    capture(widget.get(), output + "/02-playing-transport.png", scale);
    require(module->sequencers[14].activeHighlightEnd >
                module->sequencers[14].activeHighlightBegin &&
                module->sequencers[14].clockStartHighPending,
            "Play immediately restores the first highlight and schedules its trigger");
    engine::Module::ProcessArgs startArgs;
    startArgs.sampleRate = 48000.f;
    startArgs.sampleTime = 1.f / 48000.f;
    bool firstTrigger = false;
    for (int i = 0; i < 8; ++i) {
      startArgs.frame = i;
      module->process(startArgs);
      firstTrigger |= module->sequencers[14].activeClockOutputHigh;
    }
    require(firstTrigger && module->sequencers[14].activeClockDisplayPulse > .9f,
            "Play emits its initial trigger and display blink without waiting a beat");
    pressTransport(widget->channelPlay);
    require(!module->sequencers[14].running,
            "Play button toggles like the transport keyboard shortcut");
    pressTransport(widget->channelPlay);
    module->params[ComputerscareBlunch::POLY_CHANNELS_PARAM].setValue(8.f);
  }
  require(module->commitLineForChannel(1, 1, true), "second line must compile");
  module->stopSequencer(1, true);
  key(GLFW_KEY_R);
  module->processControlRequests();
  require(
      module->sequencers[1].activeLine == 0 && !module->sequencers[1].running,
      "reset returns stopped channels to first line without starting them");
  key(GLFW_KEY_ENTER);
  require(module->showingSequenceView(),
          "Enter opens focused channel in Control mode");
  key(GLFW_KEY_TAB);
  require(module->showingChannelsView(), "Tab returns to All in Control mode");
  key(GLFW_KEY_TAB);
  key(GLFW_KEY_ESCAPE);
  require(module->showingChannelsView() &&
              module->controlSelection.targets() == selected,
          "Escape returns to All without clearing selection");
  auto before = module->channelsEditorState.text;
  widget::Widget::SelectTextEvent typing;
  typing.codepoint = 'x';
  editor->onSelectText(typing);
  require(module->channelsEditorState.text == before,
          "Control mode blocks typing");
  {
    auto restored = std::unique_ptr<ComputerscareBlunch>(
        static_cast<ComputerscareBlunch*>(modelBlunchPreview->createModule()));
    auto data =
        std::unique_ptr<json_t, void (*)(json_t*)>(json_object(), json_decref);
    json_t* texts = json_array();
    for (int ch = 0; ch < 16; ++ch)
      json_array_append_new(texts, json_string(""));
    json_object_set_new(data.get(), "channelTexts", texts);
    restored->dataFromJson(data.get());
    for (const auto& seq : restored->sequencers)
      require(seq.activeProgram.empty(),
              "restored blank channels do not play startup examples");
    json_array_set_new(texts, 0, json_string("2"));
    json_object_set_new(data.get(), "defaultNumberUnit", json_string("s"));
    restored->dataFromJson(data.get());
    require(std::fabs(restored->sequencers[0].activeClockSpec.periodSeconds -
                      2.f) < .001f,
            "saved default unit applies before restored programs compile");
    json_array_set_new(texts, 0, json_string("not-a-formula"));
    restored->dataFromJson(data.get());
    require(restored->sequencers[0].activeProgram.empty(),
            "invalid restored formula does not retain a previous program");
  }
  // Physical row spacing stays fixed while the glyph height changes.
  // Compare mouse/caret boundaries with actual rendered glyph advances, not
  // another unscaled layout. Long lines expose cumulative spacing errors.
  {
    auto savedStyle = editor->style;
    auto savedText = editor->text;
    editor->style.lineWrapping = false;
    editor->text = "120bpm 300ms 4hz 120bpm 300ms 4hz 120bpm 300ms";
    auto font = APP->window->loadFont(asset::system(editor->style.fontPath));
    NVGcontext* vg = APP->window->vg;
    for (float width : {0.f, .23f, -.19f}) {
      for (float height : {0.f, .069f, .3f}) {
        editor->style.fontWidthOffset = width;
        editor->style.fontHeightOffset = height;
        float sx = 1.f + width, sy = 1.f + height;
        nvgSave(vg);
        nvgResetTransform(vg);
        float zoom = editor->getAbsoluteZoom();
        nvgScale(vg, zoom * sx, zoom * sy);
        nvgFontFaceId(vg, font->handle);
        nvgFontSize(vg, editor->style.fontSize);
        nvgTextLetterSpacing(vg, editor->style.letterSpacing);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
        auto rendered = ComputerscareTextEditorLayout::build(
            vg, editor->text, Vec(10000.f, editor->box.size.y / sy), editor->style);
        const auto& row = rendered.rows.front();
        NVGglyphPosition glyphs[128];
        int count = nvgTextGlyphPositions(vg, row.x, row.y,
                                        editor->text.c_str(), nullptr, glyphs, 128);
        nvgRestore(vg);
        for (int g = 1; g < count; ++g) {
          int offset = glyphs[g].str - editor->text.c_str();
          Vec mouse(glyphs[g].x * sx, (row.top + row.bottom) * .5f * sy);
          require(editor->getTextPosition(mouse) == offset,
                  "mouse caret matches rendered glyphs across font scales and long lines");
        }
      }
    }
    editor->style = savedStyle;
    editor->text = savedText;
  }
  auto layout = [&]() {
    auto font = APP->window->loadFont(asset::system(editor->style.fontPath));
    nvgFontFaceId(APP->window->vg, font->handle);
    nvgFontSize(APP->window->vg, editor->style.fontSize);
    float sy = std::max(.5f, 1.f + editor->style.fontHeightOffset);
    return ComputerscareTextEditorLayout::build(
        APP->window->vg, editor->text,
        Vec(editor->box.size.x, editor->box.size.y / sy), editor->style);
  };
  auto normal = layout();
  float normalHeight = normal.metrics.lineHeight *
                       std::max(.5f, 1.f + editor->style.fontHeightOffset);
  require(normal.rows.size() == 16 &&
              normal.rows.back().bottom *
                      std::max(.5f, 1.f + editor->style.fontHeightOffset) <=
                  editor->box.size.y,
          "all sixteen rows fit at the default spacing");
  module->params[ComputerscareBlunch::EDITOR_FONT_HEIGHT_PARAM].setValue(.3f);
  widget->step();
  auto tall = layout();
  require(std::fabs(tall.metrics.lineHeight *
                        std::max(.5f, 1.f + editor->style.fontHeightOffset) -
                    normalHeight) < .001f,
          "text height does not change row spacing");
  module->params[ComputerscareBlunch::EDITOR_FONT_HEIGHT_PARAM].setValue(0.f);
  module->params[ComputerscareBlunch::EDITOR_LINE_HEIGHT_PARAM].setValue(.75f);
  widget->step();
  auto spacedRows = layout();
  float scaleY = std::max(.5f, 1.f + editor->style.fontHeightOffset);
  require(std::fabs(spacedRows.metrics.lineHeight * scaleY - normalHeight) < .001f,
          "Steady line height ignores the spacing slider");
  module->params[ComputerscareBlunch::EDITOR_LINE_HEIGHT_PARAM].setValue(0.f);
  module->editorFitLines = false;
  widget->step();
  auto natural = layout();
  module->params[ComputerscareBlunch::EDITOR_LINE_HEIGHT_PARAM].setValue(.5f);
  widget->step();
  require(std::fabs(layout().metrics.lineHeight - natural.metrics.lineHeight * 1.5f) < .001f,
          "Line height adjusts text-sized rows when steady height is off");
  module->params[ComputerscareBlunch::EDITOR_LINE_HEIGHT_PARAM].setValue(0.f);
  widget->step();
  {
    auto saved = std::unique_ptr<json_t, void(*)(json_t*)>(module->dataToJson(), json_decref);
    auto restored = std::unique_ptr<ComputerscareBlunch>(
        static_cast<ComputerscareBlunch*>(modelBlunchPreview->createModule()));
    restored->dataFromJson(saved.get());
    require(!restored->editorFitLines, "Fit lines preference survives patch saving");
  }
  module->params[ComputerscareBlunch::EDITOR_FONT_SIZE_PARAM].setValue(20.f);
  widget->step();
  require(layout().metrics.lineHeight > natural.metrics.lineHeight,
          "Unfitted rows follow font size");
  module->params[ComputerscareBlunch::EDITOR_FONT_SIZE_PARAM].setValue(14.f);
  module->params[ComputerscareBlunch::EDITOR_FONT_HEIGHT_PARAM].setValue(.3f);
  widget->step();
  auto naturalTall = layout();
  require(naturalTall.metrics.lineHeight * (1.f + editor->style.fontHeightOffset) >
              natural.metrics.lineHeight * (1.f + BLUNCH_FONT_HEIGHT_BASE),
          "Unfitted rows follow text height");
  module->params[ComputerscareBlunch::EDITOR_FONT_HEIGHT_PARAM].setValue(0.f);
  module->editorFitLines = true;
  widget->step();
  // Hit testing must agree with the displayed, independently spaced rows.
  auto click = [&](int row, int mods) {
    auto rows = layout();
    float sy = std::max(.5f, 1.f + editor->style.fontHeightOffset);
    widget::Widget::ButtonEvent event;
    event.action = GLFW_PRESS;
    event.button = GLFW_MOUSE_BUTTON_LEFT;
    event.mods = mods;
    event.pos =
        Vec(10.f, (rows.rows[row].top + rows.metrics.lineHeight * .5f) * sy);
    editor->onButton(event);
    widget->step();
  };
  click(6, GLFW_MOD_CONTROL);
  require(module->controlSelection.targets() == (selected | (1u << 6)),
          "modifier click adds clicked channel");
  click(6, GLFW_MOD_CONTROL);
  require(module->controlSelection.targets() == selected,
          "modifier click removes clicked channel");
  click(4, 0);
  require(module->controlSelection.targets() == (1u << 4),
          "plain click replaces selection");
  module->controlSelection.selectOnly(1);
  module->controlSelection.moveTo(3, true);
  module->controlSelection.moveTo(14, false);
  module->controlSelection.toggleFocused();
  module->focusControlRow();
  widget->step();
  require(editor->getCursorLine() == module->controlSelection.focus,
          "focus border follows the control cursor after a programmatic move");
  capture(widget.get(), output + "/01-control-mode.png", scale);
  module->sequenceEditorStates[0].text = "140bpm";
  module->sequenceEditorStates[2].text = "100bpm";
  module->refreshChannelsEditorState();
  widget->step();
  require(module->channelHasPendingFormula(0) && module->channelHasPendingFormula(2),
          "All view marks pending edits on multiple channels");
  require(!widget->channelPlay->pending(),
          "other channels' pending edits do not affect the focused Play button");
  module->controlSelection.selectOnly(0);
  module->focusControlRow();
  widget->step();
  require(widget->channelPlay->tooltipText() == "Apply pending changes to channel 1" &&
              widget->channelCancel->tooltipText() == "Discard pending changes for channel 1",
          "pending tooltips describe the action and target channel");
  require(widget->channelPlay->pending() && !widget->channelPlay->isPressed(),
          "Play stays raised for its channel's pending formula while playback continues");
  require(widget->channelCancel->visible &&
              widget->channelCancel->box.pos.x > widget->channelStop->box.pos.x,
          "Cancel is always present to the right of Stop");
  capture(widget.get(), output + "/03-pending-formulas.png", scale);
  widget::Widget::ButtonEvent pendingClick;
  pendingClick.button = GLFW_MOUSE_BUTTON_LEFT;
  pendingClick.action = GLFW_PRESS;
  module->params[ComputerscareBlunch::WAIT_MODE_PARAM].setValue(ComputerscareBlunch::WAIT_IMMEDIATE);
  widget->channelPlay->onButton(pendingClick);
  require(module->sequencers[0].activeLineText == "140bpm" &&
              !module->channelHasPendingFormula(0),
          "Play submits pending text instead of stopping the old formula");
  module->sequenceEditorStates[0].text = "150bpm";
  module->refreshChannelsEditorState();
  widget->step();
  widget->channelCancel->onButton(pendingClick);
  pendingClick.action = GLFW_RELEASE;
  widget->channelCancel->onButton(pendingClick);
  widget->step();
  require(module->sequenceEditorStates[0].text == "140bpm" &&
              module->sequencers[0].activeLineText == "140bpm" &&
              !module->channelHasPendingFormula(0) &&
              module->channelHasPendingFormula(2),
          "Cancel restores only its channel's submitted text without interrupting playback");
  require(widget->channelCancel->tooltipText() == "Channel 1: no pending changes",
          "Cancel tooltip updates after reverting");
  require(widget->channelCancel->visible && !widget->channelPlay->pending(),
          "Cancel remains visible when no changes are pending");
  std::puts(
      "Blunch Control-mode UI, transport, mouse, and layout checks passed.");
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 5 || argc > 7) {
    std::fprintf(stderr,
                 "Usage: phlooper-preview REPO RACK_ASSETS OUTPUT_DIR SCALE "
                 "[browser|scenarios] [phlooper|blunch|windchimes]\n");
    return 2;
  }
  try {
    float scale = std::stof(argv[4]);
    if (!std::isfinite(scale) || scale < .25f || scale > 8.f)
      throw std::runtime_error("Scale must be between 0.25 and 8");
    asset::systemDir = argv[2];
    asset::userDir = std::string(argv[3]) + "/runtime";
    system::createDirectories(asset::userDir);
    settings::devMode = true;
    system::init();
    random::init();
    string::init();
    ui::init();
    window::init();
    Context context;
    contextSet(&context);
    context.engine = new engine::Engine;
    context.engine->setSampleRate(48000.f);
    context.event = new widget::EventState;
    context.history = new history::State;
    context.patch = new patch::Manager;
    context.patch->autosavePath = asset::user("preview-patch");
    context.window = new window::Window;
    glfwHideWindow(context.window->win);
    context.scene = new app::Scene;
    plugin::Plugin plugin;
    plugin.path = argv[1];
    plugin.slug = "computerscare";
    pluginInstance = &plugin;
    if (std::getenv("JACK_PREVIEW_CHECKS")) {
      std::unique_ptr<widget::Widget> jacks[] = {
          std::unique_ptr<widget::Widget>(new JackSmallPointingUp),
          std::unique_ptr<widget::Widget>(new JackSmallPointingDown),
          std::unique_ptr<widget::Widget>(new JackLargePointingUp),
          std::unique_ptr<widget::Widget>(new JackLargePointingDown),
          std::unique_ptr<widget::Widget>(new JackLargeOutlinePointingUp),
          std::unique_ptr<widget::Widget>(new JackLargeOutlinePointingDown)};
      const Vec sizes[] = {Vec(16, 16), Vec(16, 16), Vec(27, 29),
                           Vec(27, 29), Vec(32, 34), Vec(32, 34)};
      for (int i = 0; i < 6; ++i)
        if (std::fabs(jacks[i]->box.size.x - sizes[i].x) > .01f ||
            std::fabs(jacks[i]->box.size.y - sizes[i].y) > .01f)
          throw std::runtime_error(
              "Jack artwork did not load at its original size");
      std::puts("All six jack variants load at their expected sizes");
    }
    modelPhlooperPreview->plugin = &plugin;
    if (argc >= 6 && std::string(argv[5]) == "browser") {
      plugin::Model* browserModel = modelPhlooperPreview;
      if (argc == 7 && std::string(argv[6]) == "blunch")
        browserModel = modelBlunchPreview;
      if (argc == 7 && std::string(argv[6]) == "windchimes")
        browserModel = modelComputerscareWindchimes;
      if (argc == 7 && std::string(argv[6]) == "mely-porge")
        browserModel = modelComputerscareMelyPorge;
      if (argc == 7 && std::string(argv[6]) == "voly-pector")
        browserModel = modelComputerscareVolyPector;
      if (argc == 7 && std::string(argv[6]) == "laundry-soup")
        browserModel = modelComputerscareLaundrySoup;
      browserModel->plugin = &plugin;
      if (browserModel == modelComputerscareWindchimes &&
          std::getenv("WINDCHIMES_PREVIEW_CHECKS"))
        checkWindchimesCv(browserModel);
      auto browserWidget = std::unique_ptr<app::ModuleWidget>(
          browserModel->createModuleWidget(nullptr));
      capture(browserWidget.get(), std::string(argv[3]) + "/00-browser.png",
              scale);
      return 0;
    }
    if (argc == 7 && std::string(argv[6]) == "blunch") {
      renderBlunchControls(plugin, argv[3], scale);
      return 0;
    }
    // Keep the module alive until its widget and cached graphics are destroyed.
    auto* module = static_cast<ComputerscarePhlooper*>(
        modelPhlooperPreview->createModule());
    context.engine->addModule(module);
    auto widget = std::unique_ptr<ComputerscarePhlooperWidget>(
        new ComputerscarePhlooperWidget(module));
    int64_t frame = 0;
    auto save = [&](const char* name) {
      // A full sixteen-loop waveform cache takes about 0.69 audio seconds.
      if (module->frames.load() > 0) {
        widget->step();
        simulate(*module, .75f, frame);
      }
      capture(widget.get(), std::string(argv[3]) + "/" + name + ".png", scale);
    };
    save("01-empty");
    // No cables exist in this offline runner: establish connection state
    // directly.
    module->inputs[ComputerscarePhlooper::LEFT].channels = 1;
    module->inputs[ComputerscarePhlooper::RIGHT].channels = 1;
    module->params[ComputerscarePhlooper::RECORD].setValue(1.f);
    simulate(*module, 2.f, frame);
    module->params[ComputerscarePhlooper::RECORD].setValue(0.f);
    module->params[ComputerscarePhlooper::COUNT].setValue(4.f);
    simulate(*module, .7f, frame);
    // Waveform cache is populated over audio ticks for the requested viewport.
    widget->step();
    simulate(*module, .1f, frame);
    save("02-playing-four-loops");
    module->params[ComputerscarePhlooper::COUNT].setValue(16.f);
    module->params[ComputerscarePhlooper::START].setValue(.2f);
    module->params[ComputerscarePhlooper::LENGTH].setValue(.45f);
    simulate(*module, .2f, frame);
    save("03-sixteen-loops");
    module->params[ComputerscarePhlooper::COUNT].setValue(4.f);
    module->params[ComputerscarePhlooper::LENGTH].setValue(.12f);
    module->params[ComputerscarePhlooper::ZOOM].setValue(1.f);
    widget->step();
    simulate(*module, .2f, frame);
    widget->step();
    simulate(*module, .2f, frame);
    save("04-zoomed-region");
    module->params[ComputerscarePhlooper::RECORD].setValue(1.f);
    simulate(*module, .12f, frame);
    save("05-overdubbing");
    module->params[ComputerscarePhlooper::RECORD].setValue(0.f);
    module->params[ComputerscarePhlooper::MUTE].setValue(1.f);
    module->params[ComputerscarePhlooper::SHOW_TRANSPORT].setValue(0.f);
    simulate(*module, .12f, frame);
    save("06-muted-controls-hidden");
    module->params[ComputerscarePhlooper::MUTE].setValue(0.f);
    module->params[ComputerscarePhlooper::ZOOM].setValue(0.f);
    module->params[ComputerscarePhlooper::SHOW_TRANSPORT].setValue(1.f);
    module->params[ComputerscarePhlooper::LOOP_SOLO + 1].setValue(1.f);
    module->params[ComputerscarePhlooper::LOOP_MUTE + 2].setValue(1.f);
    save("07-mute-solo-backgrounds");
    widget.reset();

    // Context destroys graphics before terminating GLFW. No settings are saved.
    return 0;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "Preview failed: %s\n", e.what());
    return 1;
  }
}
