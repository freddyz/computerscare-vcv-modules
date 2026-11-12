// Development-only renderer. Uses the real module/widget, never a panel mockup.
#define modelComputerscarePhlooper modelPhlooperPreview
#include "../../src/ComputerscarePhlooper.cpp"
#undef modelComputerscarePhlooper
#define modelComputerscareBlunch modelBlunchPreview
#include "../../src/Blunch/ComputerscareBlunch.cpp"
#undef modelComputerscareBlunch


#define modelComputerscareVolyPector modelVolyMultiPreview
#include "../../src/ComputerscareVolyPector.cpp"
#undef modelComputerscareVolyPector

#include "../../src/DevWatcherToolbar.hpp"
#include "../../src/PatchSaveNaming.hpp"
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
#include "blunch-layers-checks.hpp"
#include "blunch-thread-checks.hpp"
#include "blunch-seed-checks.hpp"
#include "blunch-sync-checks.hpp"
#include "blunch-gate-checks.hpp"
#include "blunch-row-selection-checks.hpp"
#include "voly-keyboard-checks.hpp"
#include "../../src/Portaloof/PortaloofModule.hpp"

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
  if(std::getenv("BLUNCH_EDITOR_BENCHMARK") && path.find("/01-editor-selection.png")!=std::string::npos) {
    std::vector<double> samples;
    for(int batch=0;batch<5;++batch){
      glFinish();auto start=std::chrono::steady_clock::now();
      for(int frame=0;frame<20;++frame){widget->step();framebuffer->render(math::Vec(scale,scale));}
      glFinish();samples.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/20.);
    }
    std::sort(samples.begin(),samples.end());
    std::printf("Blunch editor benchmark: %.3f ms/frame median (step + native render, 16 rows)\n",samples[2]);
  }

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

#include "groups.hpp"

void checkMaxVisual(app::ModuleWidget* widget,bool& compact,const std::string& path,float scale) {
  float width=widget->box.size.x;
  auto ports=widget->getPorts();std::vector<widget::Widget*> parents;std::vector<Rect> boxes;
  for(auto* p:ports){parents.push_back(p->parent);boxes.push_back(p->box);}
  compact=true;widget->step();
  if(widget->getPorts().size()!=ports.size())throw std::runtime_error("Max visual must preserve every jack");
  auto* data=widget->module->dataToJson();bool saved=json_is_true(json_object_get(data,"compactUi"));json_decref(data);
  if(!saved)throw std::runtime_error("Max visual must save with patch");
  for(auto* p:ports){
    if(p->parent!=widget || p->box.pos.x<0 || p->box.pos.x+p->box.size.x>widget->box.size.x || p->box.pos.y<0 || p->box.pos.y+p->box.size.y>380)throw std::runtime_error("Max visual ports must fit side rail");
    if(p->box.size.x>15.f || p->box.size.y>15.f)throw std::runtime_error("Max visual must use tiny jacks");
    widget::EventContext hit;widget::Widget::ButtonEvent click;click.context=&hit;click.button=GLFW_MOUSE_BUTTON_LEFT;click.action=GLFW_PRESS;click.pos=p->box.getCenter();widget->onButton(click);
    if(!dynamic_cast<app::PortWidget*>(hit.target))throw std::runtime_error("Max visual ports must receive clicks");
  }
  capture(widget,path,scale);
  if (dynamic_cast<ComputerscarePhlooperWidget*>(widget)) {
    if (widget->box.size.x != width)
      throw std::runtime_error("Phlooper Big must preserve module width");
    for (auto* port : ports)
      if (port->box.getCenter().x < width - 20)
        throw std::runtime_error("Phlooper Big jacks must stay on the right");
    widget::EventContext keys;
    widget::Widget::HoverKeyEvent escape;
    escape.context = &keys;
    escape.key = GLFW_KEY_ESCAPE;
    escape.action = GLFW_PRESS;
    escape.mods = 0;
    widget->onHoverKey(escape);
    widget->step();
    if (compact || widget->box.size.x != width)
      throw std::runtime_error("Escape must restore Phlooper normal view");
    compact = true;
    widget->step();
  }
  computerscare::ReturnToNormalButton* normal=nullptr;
  for(auto* child:widget->children)if(auto* b=dynamic_cast<computerscare::ReturnToNormalButton*>(child))if(b->visible)normal=b;
  if(!normal || !normal->visible)throw std::runtime_error("Max visual must expose return button");
  widget::EventContext hit;widget::Widget::ButtonEvent click;click.context=&hit;click.button=GLFW_MOUSE_BUTTON_LEFT;click.action=GLFW_PRESS;click.pos=normal->box.getCenter();widget->onButton(click);widget->step();
  if(compact || widget->box.size.x!=width)throw std::runtime_error("Return button must restore normal width");
  for(size_t i=0;i<ports.size();++i)if(ports[i]->parent!=parents[i] || ports[i]->box.pos!=boxes[i].pos || ports[i]->box.size!=boxes[i].size)throw std::runtime_error("Return must restore normal jack placement");
  std::puts("Shared Max visual port hit-testing, persistence, return button and layout restoration checks passed");
}

#include "phlooper-benchmark.hpp"
#include "phlooper-storage-checks.hpp"
#include "patch-save-checks.hpp"
#include "blunch-line-menu-checks.hpp"
#include "blunch-settings-checks.hpp"
#include "blunch-scan-checks.hpp"
#include "blunch-bake-checks.hpp"
#include "blunch-repeat-sequence-checks.hpp"
#include "blunch-transport-save-checks.hpp"
#include "voly-multi-checks.hpp"

void renderBlunchControls(plugin::Plugin& plugin, const std::string& output,
                          float scale) {
  auto require = [](bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
  };
  modelBlunchPreview->plugin = &plugin;
  checkBlunchRepeatSequences(modelBlunchPreview);
  checkBlunchTransportSave(modelBlunchPreview);
  checkBlunchSettings(modelBlunchPreview, output, scale);
  checkBlunchScanning(modelBlunchPreview);
  checkBlunchTimeBake(modelBlunchPreview,output,scale);
  checkBlunchGates(modelBlunchPreview);
  checkBlunchRowSelection(modelBlunchPreview,[&](app::ModuleWidget* widget){capture(widget,output+"/all-line-advance-off.png",scale);});
  checkBlunchLineMenus(modelBlunchPreview, output, scale);
  checkBlunchFormulaLayers(modelBlunchPreview);
  checkBlunchThreadHandoff(modelBlunchPreview);
  checkBlunchSeedBoundaries(modelBlunchPreview);
  checkBlunchSkipProbability(modelBlunchPreview);
  checkBlunchRandomModes(modelBlunchPreview,[&](app::ModuleWidget* widget){capture(widget,output+"/11-random-modes.png",scale);});
  checkBlunchChannelSync(modelBlunchPreview);
  auto* module =
      static_cast<ComputerscareBlunch*>(modelBlunchPreview->createModule());
  module->params[ComputerscareBlunch::LIVE_APPLY_PARAM].setValue(0);
  module->params[ComputerscareBlunch::WAIT_MODE_PARAM].setValue(ComputerscareBlunch::WAIT_IMMEDIATE);
  require(module->playbackDisplayStyle == 1, "New modules default to Minimal blinkers");
  require(module->params[ComputerscareBlunch::POLY_CHANNELS_PARAM].getValue() == 0.f,
          "New modules default to automatic polyphony");
  {
    auto draft = std::unique_ptr<ComputerscareBlunch>(
        static_cast<ComputerscareBlunch*>(modelBlunchPreview->createModule()));
    draft->params[ComputerscareBlunch::LIVE_APPLY_PARAM].setValue(0);
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
  module->returnToChannelsView();
  widget->step();
  auto* editor = static_cast<BlunchTextEditor*>(widget->editor);
  if (std::getenv("VCVMON_PREVIEW_CHECKS")) {
    const char* path = std::getenv("VCVMON_CONTROL");
    require(path, "Watcher checks need a control path");
    std::string control(path);
    widget::Widget* toolbar = nullptr;
    for (auto* child : APP->scene->children)
      for(auto* control:child->children)
        if(auto* button=dynamic_cast<ui::Button*>(control))
          if(button->text=="Save version")toolbar=child;
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
    pollState("watching");
    checkPatchSaves(toolbar,[&](widget::Widget* widget,const char* name){if(widget==toolbar)renderToolbar(name);else capture(widget,output+"/"+name,scale,false);});
    std::puts("Watcher toolbar state and command checks passed");
  }
  // The preview scene can retain Rack's startup menus; dismiss them before
  // testing panel focus rather than intentionally deferring focus to a menu.
  for (auto* child : APP->scene->children)
    if (auto* overlay = dynamic_cast<ui::MenuOverlay*>(child))
      overlay->requestDelete();
  APP->event->setSelectedWidget(editor);
  auto key = [&](int code, int mods = 0) {
    widget::EventContext context;
    widget::Widget::SelectKeyEvent event;
    event.context=&context;event.key=code;event.mods=mods;event.action=GLFW_PRESS;
    editor->onSelectKey(event);widget->step();
  };
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
    module->controlSelection.selectOnly(14);
    module->focusControlRow();widget->step();
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
    require(module->sequencers[14].running,
            "Play button keeps an already playing selection running");
    pressTransport(widget->channelPlay);
    module->params[ComputerscareBlunch::POLY_CHANNELS_PARAM].setValue(8.f);
  }
  module->controlSelection.selectOnly(1);module->focusControlRow();widget->step();
  auto allText=module->channelsEditorState.text;
  key(GLFW_KEY_ENTER);key(GLFW_KEY_KP_ENTER);
  require(module->showingChannelsView()&&module->channelsEditorState.text==allText,"Enter in All edit view neither navigates nor inserts rows");
  key(GLFW_KEY_TAB);require(module->showingSequenceView(),"Tab opens channel in edit mode");
  key(GLFW_KEY_TAB);require(module->showingChannelsView(),"Tab returns to All in edit mode");
  key(GLFW_KEY_TAB);
  key(GLFW_KEY_ESCAPE);require(module->showingChannelsView(),"Escape returns to All in edit mode");
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
  unsigned selected=module->controlSelection.targets();
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
  capture(widget.get(), output + "/01-editor-selection.png", scale);
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
  require(widget->channelPlay->tooltipText() == "Apply / start channel 1 [Mod+Enter]" &&
              widget->channelCancel->tooltipText() == "Discard Draft for channel 1 (Shift: discard Next too)",
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
  require(widget->channelCancel->tooltipText() == "Channel 1: no changes",
          "Cancel tooltip updates after reverting");
  require(widget->channelCancel->visible && !widget->channelPlay->pending(),
          "Cancel remains visible when no changes are pending");

  module->focusedChannel = 1;
  module->editorViewMode = BlunchEditorViewMode::Sequence;
  module->selectedLine = 0;
  module->selectedLineDirty = true;
  module->params[ComputerscareBlunch::WAIT_MODE_PARAM].setValue(ComputerscareBlunch::WAIT_PAGE);
  module->sequenceEditorStates[1].text = "4hz\n5hz";
  require(module->executePage(), "UI page staging compiles");
  module->sequenceEditorStates[1].text = "6hz\n5hz";
  module->sequenceEditorStates[1].dirty = true;
  module->editorViewDirty = true;
  widget->step();
  capture(widget.get(), output + "/04-draft-channel.png", scale);
  auto layerKey = [&](int key, int action = GLFW_PRESS, int mods = 0) {
    widget::Widget::SelectKeyEvent e;
    e.key = key; e.action = action; e.mods = mods;
    editor->onSelectKey(e);
    widget->step();
  };
  module->setFormulaLayer(ComputerscareBlunch::FormulaLayer((module->formulaLayer + 1) % 3));
  widget->step();
  require(module->visibleLayer() == ComputerscareBlunch::NEXT && editor->readOnly &&
              editor->text == "4hz\n5hz" && module->sequenceEditorStates[1].text == "6hz\n5hz",
          "Next view is read-only and preserves newer Draft text");
  require(widget->channelLabel->viewIndicator()=="Next", "Header identifies the read-only Next view");
  capture(widget.get(), output + "/05-next-channel.png", scale);
  layerKey(GLFW_KEY_F7);
  require(module->visibleLayer() == ComputerscareBlunch::NOW && editor->text != "4hz\n5hz",
          "F7 cycles Next to Now");
  layerKey(GLFW_KEY_F7, GLFW_RELEASE);
  require(module->visibleLayer() == ComputerscareBlunch::NOW,
          "F7 release leaves the selected layer unchanged");
  layerKey(GLFW_KEY_F7);
  require(module->visibleLayer() == ComputerscareBlunch::DRAFT, "F7 cycles Now back to Draft");
  layerKey(GLFW_KEY_F7);
  require(module->visibleLayer() == ComputerscareBlunch::NEXT && editor->text == "4hz\n5hz", "F7 cycles Draft to Next without losing edits");
  std::string nextBeforeUndo = editor->text;
  layerKey(GLFW_KEY_Z, GLFW_PRESS, RACK_MOD_CTRL);
  require(editor->text == nextBeforeUndo, "Undo cannot modify read-only Next");
  module->setFormulaLayer(ComputerscareBlunch::FormulaLayer((module->formulaLayer + 1) % 3));
  widget->step();
  require(widget->channelLabel->viewIndicator()=="Now", "Header identifies the read-only Now view");
  capture(widget.get(), output + "/06-now-channel.png", scale);
  module->setFormulaLayer(ComputerscareBlunch::FormulaLayer((module->formulaLayer + 1) % 3));
  widget->step();
  require(widget->channelLabel->viewIndicator().empty(), "Draft has no header indicator");
  require(module->visibleLayer() == ComputerscareBlunch::DRAFT && !editor->readOnly &&
              editor->text == "6hz\n5hz", "Cycling views restores editable Draft");
  module->setFormulaLayer(ComputerscareBlunch::FormulaLayer((module->formulaLayer + 1) % 3));
  widget->step();
  widget::Widget::SelectTextEvent draftTyping;
  draftTyping.codepoint = '7';
  editor->onSelectText(draftTyping);
  widget->step();
  require(module->visibleLayer() == ComputerscareBlunch::DRAFT &&
              module->pendingFormulas[1].lines.at(0).text == "4hz" &&
              module->sequenceEditorStates[1].text != "6hz\n5hz",
          "Typing in Next returns to Draft without overwriting the queue");
  editor->undo();
  widget->step();
  require(module->sequenceEditorStates[1].text == "6hz\n5hz",
          "Draft undo survives switching between layer views");
  layerKey(GLFW_KEY_ENTER, GLFW_PRESS, RACK_MOD_CTRL | GLFW_MOD_SHIFT);
  require(module->pendingFormulas[1].page && module->pendingFormulas[1].lines.at(0).text == "6hz",
          "Ctrl/Cmd+Shift+Enter stages a whole page through the real editor");
  module->sequenceEditorStates[1].text = "7hz\n5hz";
  module->sequenceEditorStates[1].dirty = true;
  widget->step();
  layerKey(GLFW_KEY_ESCAPE);
  require(module->showingSequenceView() && editor->state == &module->sequenceEditorStates[1] &&
              module->sequenceEditorStates[1].text == "6hz\n5hz" && module->channelHasNext(1),
          "First Escape discards only Draft and keeps the channel editor open");
  layerKey(GLFW_KEY_ESCAPE);
  require(module->showingSequenceView() && !module->pendingFormulas[1].lines.count(0) &&
              module->pendingFormulas[1].lines.count(1),
          "Second Escape unstages only the focused line");
  layerKey(GLFW_KEY_ESCAPE);
  require(module->showingChannelsView() && module->pendingFormulas[1].lines.count(1),
          "Escape from an unchanged line returns to All without discarding other lines");
  // A readable mixed-state page for visual review: both, queued, draft,
  // unchanged/running, and another draft.
  module->focusedChannel = 1;
  module->editorViewMode = BlunchEditorViewMode::Sequence;
  module->setFormulaLayer(ComputerscareBlunch::DRAFT);
  module->runningLineTexts[1].clear();
  module->pendingFormulas[1] = ComputerscareBlunch::PendingFormula{};
  module->sequenceEditorStates[1].text = "120bpm\n250ms\n2hz\n[300 400]ms\n90bpm@4";
  require(module->commitLineForChannel(1, 3, true), "mixed-state preview baseline compiles");
  module->sequencers[1].running = true;
  module->sequenceEditorStates[1].text = "130bpm\n300ms\n2hz\n[300 400]ms\n90bpm@4";
  module->executePage();
  module->sequenceEditorStates[1].text = "140bpm\n300ms\n2.5hz\n[300 400]ms\n100bpm@4";
  module->selectedLine = 3;
  module->selectedLineDirty = true;
  module->sequenceEditorStates[1].dirty = true;
  module->editorViewDirty = true;
  widget->step();
  require(module->lineHasDraft(1, 0) && module->pendingFormulas[1].lines.count(0) &&
              !module->lineHasDraft(1, 1) && module->pendingFormulas[1].lines.count(1) &&
              module->lineHasDraft(1, 2) && !module->pendingFormulas[1].lines.count(2),
          "mixed-state Channel 2 preview contains both, queued-only, and draft-only rows");
  capture(widget.get(), output + "/07-channel-2-mixed-states.png", scale);
  module->focusedChannel = 0;
  module->channelsCursorChannel = 0;
  module->runningLineTexts[0].clear();
  module->pendingFormulas[0] = ComputerscareBlunch::PendingFormula{};
  module->sequenceEditorStates[0].text = "120bpm\n240bpm";
  module->commitLine(1,true);
  module->params[ComputerscareBlunch::WAIT_MODE_PARAM].setValue(ComputerscareBlunch::WAIT_IMMEDIATE);
  module->sequenceEditorStates[0].text = "90bpm\n[300 400]ms";
  module->executePage();
  module->openFocusedChannelEditPage();
  module->setFormulaLayer(ComputerscareBlunch::DRAFT);
  widget->step();
  capture(widget.get(), output + "/08-channel-draft-applied.png", scale);

  module->params[ComputerscareBlunch::AUTO_ADVANCE_PARAM].setValue(0);
  module->commitLine(0,true);
  widget->step();editor->setCursorLine(1);
  key(GLFW_KEY_ENTER,RACK_MOD_CTRL);
  require(module->sequencers[0].activeLine==1 && module->sequencers[0].running,
          "Ctrl/Cmd+Enter starts the focused channel line with Line advance off");
  module->params[ComputerscareBlunch::WAIT_MODE_PARAM].setValue(ComputerscareBlunch::WAIT_LINE);
  editor->setCursorLine(0);key(GLFW_KEY_ENTER,RACK_MOD_CTRL);
  require(!module->channelHasNext(0) && module->sequencers[0].activeLine==0,
          "Focused-line Play bypasses Wait and starts immediately with Line advance off");
  module->params[ComputerscareBlunch::WAIT_MODE_PARAM].setValue(ComputerscareBlunch::WAIT_IMMEDIATE);


  module->params[ComputerscareBlunch::POLY_CHANNELS_PARAM].setValue(1);
  module->pendingFormulas[2] = ComputerscareBlunch::PendingFormula{};
  module->runningLineTexts[2].clear();
  module->sequencers[2] = BlunchSequencerRuntime{};
  module->sequenceEditorStates[2].text = "109.03bpm";
  module->sequenceEditorStates[2].dirty = true;
  module->returnToChannelsView();module->channelsCursorChannel=2;
  module->refreshChannelsEditorState(true);module->editorViewDirty=true;widget->step();
  auto row = getLineInfo(module->channelsEditorState.text, 2);
  auto appearance = computerscareTextRowAppearance(module->channelsEditorState.highlights,row.begin,row.end);
  require(appearance.italic && appearance.opacity == .65f,
          "Inactive draft foreground and base text inherit identical row styling");
  bool draftForeground=false;
  for(const auto& h:module->channelsEditorState.highlights)
    draftForeground |= h.hasForeground && h.begin>=row.begin && h.end<=row.end;
  require(draftForeground,"New inactive-channel text receives a draft foreground");
  capture(widget.get(),output+"/09-inactive-draft.png",scale);

  checkMaxVisual(widget.get(),module->compactUi,output+"/10-max-visual.png",scale);
  {
    auto menu=std::unique_ptr<ui::Menu>(new ui::Menu);
    widget->appendContextMenu(menu.get());
    ui::MenuItem* pulseMenu=nullptr;
    for(auto* child:menu->children)if(auto* item=dynamic_cast<ui::MenuItem*>(child))if(item->text=="Clock output")pulseMenu=item;
    require(!pulseMenu,"Clock output mode is controlled by the panel knob, not the context menu");
  }
  // Theme changes must not move text, alter cursor hit tests, or touch transport.
  module->returnToChannelsView();widget->step();
  auto themeLayout=layout();
  for(int theme=0;theme<4;++theme) {
    module->editorTheme=theme;widget->step();auto themed=layout();
    require(themed.rows.size()==themeLayout.rows.size(),"Themes preserve visible rows");
    for(size_t row=0;row<themed.rows.size();++row)
      require(themed.rows[row].x==themeLayout.rows[row].x&&themed.rows[row].y==themeLayout.rows[row].y&&themed.rows[row].carets==themeLayout.rows[row].carets,"Themes preserve caret and viewport geometry");
    auto* saved=module->dataToJson();
    auto restored=std::unique_ptr<ComputerscareBlunch>(static_cast<ComputerscareBlunch*>(modelBlunchPreview->createModule()));
    restored->dataFromJson(saved);json_decref(saved);
    require(restored->editorTheme==theme,"Editor theme survives patch reload");
  }
  for(int theme : {1,2,0,3}) {
    module->editorTheme=theme;
    module->params[ComputerscareBlunch::WAIT_MODE_PARAM].setValue(ComputerscareBlunch::WAIT_IMMEDIATE);
    module->focusedChannel=1;
    module->sequenceEditorStates[1].text="80bpm@3\n{100-120}bpm#4s\n[300 450]ms\n(2 (3 1))hz@9";
    module->commitLineForChannel(1,theme%3,true);module->startSequencer(1);
    module->sequencers[1].activeClockDisplayPulse=.65f;
    module->playbackDisplayStyle=theme==2?2:1;
    module->params[ComputerscareBlunch::POLY_CHANNELS_PARAM].setValue(theme==0?16:6);
    if(theme==1||theme==0) {
      module->returnToChannelsView();
      module->controlSelection.selectOnly(theme==0?3:1);module->controlSelection.moveTo(theme==0?5:2,true);module->focusControlRow();
    } else {
      module->channelsCursorChannel=1;module->openFocusedChannelEditPage();
      module->params[ComputerscareBlunch::WAIT_MODE_PARAM].setValue(ComputerscareBlunch::WAIT_LINE);
      module->sequenceEditorStates[1].text="80bpm@3\n{100-140}bpm#4s\n[300 450]ms\n(2 (3 1))hz@9";
    }
    widget->step();
    capture(widget.get(),output+"/theme-"+computerscare::text_editor::editorTheme(theme).name+".png",scale);
  }
  std::puts("Editor theme persistence and identical viewport/caret geometry checks passed");
  std::puts(
      "Blunch editor UI, transport, mouse, and layout checks passed.");
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 5 || argc > 7) {
    std::fprintf(stderr,
                 "Usage: phlooper-preview REPO RACK_ASSETS OUTPUT_DIR SCALE "
                 "[browser|scenarios] [phlooper|blunch]\n");
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
    if(argc==7 && std::string(argv[6])=="groups") {
      renderModuleGroups(plugin,argv[3],scale);
      return 0;
    }
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
    checkPhlooperStorage(modelPhlooperPreview);
    if(std::getenv("PHLOOPER_DSP_BENCHMARK")){benchmarkPhlooper(modelPhlooperPreview);return 0;}
    {
      auto m = std::unique_ptr<ComputerscarePhlooper>(static_cast<ComputerscarePhlooper*>(modelPhlooperPreview->createModule()));
      m->loop.size=32; m->createdLoops=2;
      std::fill(m->loop.audio[0].get(),m->loop.audio[0].get()+64,1.f);
      std::fill(m->loop.audio[1].get(),m->loop.audio[1].get()+64,2.f);
      m->params[ComputerscarePhlooper::EXPANSION].setValue(1);
      m->expandLoops(6);
      for(int i=0;i<6;++i) if(m->loop.audio[i][0] != float(i%2+1)) throw std::runtime_error("Cycle expansion copies existing recordings");
      m->loop.audio[2][0]=9.f;
      if(m->loop.audio[0][0]!=1.f) throw std::runtime_error("Expanded recordings must be independent");
      auto* saved=m->dataToJson();
      if(json_integer_value(json_object_get(saved,"createdLoops"))!=6) throw std::runtime_error("Expansion count persists");
      json_decref(saved);
      std::puts("Phlooper expansion copies, independence and persistence passed");
      using P=ComputerscarePhlooper;
      m->params[P::COUNT].setValue(6);
      m->params[P::OFFSET].setValue(0);
      m->params[P::OUTPUT_MODE].setValue(1);
      m->params[P::MIX].setValue(1);
      m->outputs[P::OUT_L].channels=6;
      m->outputs[P::OUT_R].channels=6;
      Module::ProcessArgs gainArgs;gainArgs.sampleRate=48000;gainArgs.sampleTime=1.f/48000;gainArgs.frame=0;
      std::vector<float> reference;
      for(float gain : {1.f,.25f}) {
        m->params[P::OUTPUT_GAIN].setValue(gain);m->outputGain=gain;
        m->loop.resetHeads();
        for(int frame=0;frame<3000;++frame){
          ++gainArgs.frame;m->process(gainArgs);
          for(int ch=0;ch<6;++ch){float value=m->outputs[P::OUT_L].getVoltage(ch);
            if(gain==1.f) reference.push_back(value);
            else if(std::fabs(value-reference[frame*6+ch]*gain)>1e-5f) throw std::runtime_error("Polyphonic output gain changed waveform instead of amplitude");
          }
        }
      }
      m->params[P::MIX].setValue(0);m->params[P::OUTPUT_GAIN].setValue(1);m->outputGain=1;
      m->inputs[P::LEFT].channels=6;
      for(int ch=0;ch<6;++ch)m->inputs[P::LEFT].setVoltage(float(ch+1),ch);
      for(float gain : {1.f,.25f,2.f}) {
        m->params[P::INPUT_GAIN].setValue(gain);m->inputGain=gain;
        ++gainArgs.frame;m->process(gainArgs);
        for(int ch=0;ch<6;++ch) if(std::fabs(m->outputs[P::OUT_L].getVoltage(ch)-float(ch+1)*gain)>1e-5f) throw std::runtime_error("Polyphonic input gain is not linear");
      }
      std::puts("Phlooper six-channel input/output gain linearity checks passed");
      m->loop.size=1024;m->loop.resetHeads();
      for(int voice=0;voice<16;++voice)std::fill(m->loop.audio[voice].get(),m->loop.audio[voice].get()+2048,0.f);
      m->params[P::MODE].setValue(0);m->params[P::SPEED].setValue(0);m->params[P::OFFSET].setValue(0);m->params[P::COUNT].setValue(4);m->params[P::HOLD].setValue(0);
      m->inputs[P::SPEED_CV].channels=2;m->inputs[P::SPEED_CV].setVoltage(0,0);m->inputs[P::SPEED_CV].setVoltage(1,1);
      for(int frame=0;frame<10;++frame){++gainArgs.frame;m->process(gainArgs);}
      if(std::fabs(m->loop.head[1]-2*m->loop.head[0])>1e-5 || std::fabs(m->loop.head[2]-m->loop.head[0])>1e-5)throw std::runtime_error("Polyphonic speed CV must address matching loops, missing channels use knob");
      m->loop.resetHeads();m->inputs[P::SPEED_CV].channels=1;m->inputs[P::SPEED_CV].setVoltage(1);
      for(int frame=0;frame<10;++frame){++gainArgs.frame;m->process(gainArgs);}
      for(int voice=1;voice<4;++voice)if(m->loop.head[voice]!=m->loop.head[0])throw std::runtime_error("Mono speed CV must broadcast");
      m->loop.resetHeads();m->inputs[P::SPEED_CV].channels=2;m->inputs[P::SPEED_CV].setVoltage(0,0);m->inputs[P::SPEED_CV].setVoltage(1,1);
      m->params[P::MODE].setValue(2);++gainArgs.frame;m->process(gainArgs);
      m->params[P::HOLD].setValue(1);++gainArgs.frame;m->process(gainArgs);
      if(!m->loop.speedHold.active || std::fabs(m->loop.periods[1]-2*m->loop.periods[0])>1e-5)throw std::runtime_error("Speed Hold fits independent polyphonic pitches");
      m->inputs[P::SPEED_CV].setVoltage(-1,1);++gainArgs.frame;m->process(gainArgs);
      if(std::fabs(m->loop.periods[1]-.5*m->loop.periods[0])>1e-5 || std::fabs(phlooper::Engine::wrapPhase(m->loop.head[1]/m->loop.periods[1]-m->loop.speedHold.phases[1])-phlooper::Engine::wrapPhase(m->loop.head[0]/m->loop.periods[0]-m->loop.speedHold.phases[0]))>1e-5)throw std::runtime_error("Poly speed CV responds while held without losing shared repeat timing");
      auto* heldJson=m->dataToJson();auto restored=std::unique_ptr<P>(static_cast<P*>(modelPhlooperPreview->createModule()));restored->dataFromJson(heldJson);json_decref(heldJson);
      if(!restored->loop.speedHold.active || restored->loop.speedHold.controlScales[1]!=2.)throw std::runtime_error("Held polyphonic speed state persists");
      std::puts("Polyphonic speed, mono broadcast, missing channels, live Speed Hold and persistence checks passed");


    }


    if (argc >= 6 && std::string(argv[5]) == "browser") {
      plugin::Model* browserModel = modelPhlooperPreview;
      if (argc == 7 && std::string(argv[6]) == "blunch")
        browserModel = modelBlunchPreview;
      if (argc == 7 && std::string(argv[6]) == "mely-porge")
        browserModel = modelComputerscareMelyPorge;
      if (argc == 7 && std::string(argv[6]) == "voly-pector")
        browserModel = modelComputerscareVolyPector;
      if (argc == 7 && std::string(argv[6]) == "laundry-soup")
        browserModel = modelComputerscareLaundrySoup;
      browserModel->plugin = &plugin;
      if(browserModel==modelComputerscareVolyPector && std::getenv("VOLY_PREVIEW_CHECKS"))checkVolyKeyboard(browserModel,[&](app::ModuleWidget* widget){capture(widget,std::string(argv[3])+"/01-shortcuts.png",scale);});
      if(browserModel==modelComputerscareVolyPector && std::getenv("VOLY_PREVIEW_CHECKS"))checkVolyMulti(browserModel,argv[3],scale);
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
    // Momentary monitoring restores the exact mix, including repeated key events
    // and release after a modifier was pressed while W was held.
    {
      float original = module->params[ComputerscarePhlooper::MIX].getValue();
      module->params[ComputerscarePhlooper::MIX].setValue(.37f);
      auto key = [&](int action, int mods = 0) {
        event::HoverKey e;
        e.key = GLFW_KEY_W; e.action = action; e.mods = mods;
        widget->onHoverKey(e);
      };
      key(GLFW_PRESS, RACK_MOD_CTRL);
      if (module->params[ComputerscarePhlooper::MIX].getValue() != .37f)
        throw std::runtime_error("Modified W must leave Out mix unchanged");
      key(GLFW_PRESS);
      key(GLFW_REPEAT);
      if (module->params[ComputerscarePhlooper::MIX].getValue() != 0.f)
        throw std::runtime_error("Held W must monitor at zero Out mix");
      key(GLFW_RELEASE, RACK_MOD_CTRL);
      if (module->params[ComputerscarePhlooper::MIX].getValue() != .37f || widget->monitorKeyHeld)
        throw std::runtime_error("W release must restore the exact previous Out mix");
      key(GLFW_PRESS);
      widget->releaseMonitorKey();
      if (module->params[ComputerscarePhlooper::MIX].getValue() != .37f)
        throw std::runtime_error("W release outside the module must restore Out mix");
      module->params[ComputerscarePhlooper::MIX].setValue(original);
      std::puts("Phlooper momentary W monitoring and mix restoration checks passed");
    }
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
    {
      using P=ComputerscarePhlooper;
      int size=module->loop.size;int created=module->createdLoops;
      std::array<std::vector<float>,16> before;
      for(int v=0;v<16;++v) before[v].assign(module->loop.audio[v].get(),module->loop.audio[v].get()+size*2);
      module->clockMarkCount.store(2);module->clockMarks[0].store(0);module->clockMarks[1].store(size/4.f);module->recordedBeatFrames.store(size/4.f);
      module->params[P::START].setValue(.2f);module->params[P::LENGTH].setValue(.6f);
      module->params[P::LOOP_MUTE].setValue(1);module->params[P::HOLD].setValue(1);
      module->onReset({});
      if(module->params[P::HOLD].getValue()!=0)
        throw std::runtime_error("Initialize did not reset Hold: default="+std::to_string(module->getParamQuantity(P::HOLD)->getDefaultValue()));
      for(auto* quantity:module->paramQuantities)quantity->setImmediateValue(quantity->getValue());
      simulate(*module,.001f,frame);
      if(module->loop.size!=size || module->frames.load()!=size || module->createdLoops!=created)
        throw std::runtime_error("Initialize must preserve recording length and created loops");
      for(int v=0;v<16;++v)if(!std::equal(before[v].begin(),before[v].end(),module->loop.audio[v].get()))
        throw std::runtime_error("Initialize changed stored loop audio");
      if(module->clockMarkCount.load()!=2 || module->recordedBeatFrames.load()!=size/4.f || module->params[P::START].getValue()!=0 || module->params[P::LENGTH].getValue()!=1 || module->params[P::LOOP_MUTE].getValue()!=0 || module->recording.load())
        throw std::runtime_error("Initialize must reset controls but preserve beat markers");
      module->params[P::COUNT].setValue(4);
      save("22-initialized-audio-preserved");
      module->clockMarkCount.store(0);module->recordedBeatFrames.store(0);
      auto initial=std::unique_ptr<P>(static_cast<P*>(modelPhlooperPreview->createModule()));
      initial->inputs[P::LEFT].channels=1;initial->params[P::RECORD].setValue(1);
      simulate(*initial,.02f,frame);
      int captured=initial->loop.captured;
      std::vector<float> capture(initial->loop.audio[0].get(),initial->loop.audio[0].get()+captured*2);
      initial->onReset({});simulate(*initial,.001f,frame);
      if(initial->loop.initial || initial->loop.size!=captured || !std::equal(capture.begin(),capture.end(),initial->loop.audio[0].get()))
        throw std::runtime_error("Initialize must finalize and preserve an in-progress first recording");
      std::puts("Phlooper Initialize preserves all sixteen audio loops and beat markers while resetting controls");
    }

    {
      using P=ComputerscarePhlooper;
      float mode=module->params[P::MODE].getValue(), offset=module->params[P::OFFSET].getValue();
      module->params[P::MODE].setValue(2);
      auto* quantity=module->getParamQuantity(P::OFFSET);
      quantity->setDisplayValue(25);
      quantity->setImmediateValue(quantity->getValue());
      if (quantity->getDisplayValue()!=25 || module->offsetFor(1,2,false)!=25)
        throw std::runtime_error("Speed offset must reach +25 percent in tooltip and DSP");
      quantity->setDisplayValue(-25);
      quantity->setImmediateValue(quantity->getValue());
      if (quantity->getDisplayValue()!=-25 || module->offsetFor(1,2,false)!=-25)
        throw std::runtime_error("Speed offset must reach -25 percent");
      quantity->setDisplayValue(25);
      quantity->setImmediateValue(quantity->getValue());
      widget->step(); simulate(*module,.05f,frame);
      bool tips=settings::tooltips;settings::tooltips=true;
      auto* knob=widget->getParam(P::OFFSET);knob->createTooltip();
      auto* tip=APP->scene->children.back();APP->scene->removeChild(tip);widget->addChild(tip);tip->step();tip->box.pos=Vec(94,202);
      capture(widget.get(),std::string(argv[3])+"/19-speed-offset-25.png",scale,false);
      widget->removeChild(tip);APP->scene->addChild(tip);knob->destroyTooltip();settings::tooltips=tips;
      module->params[P::SPEED_OFFSET_CURVE].setValue(1);
      module->params[P::OFFSET].setValue(-8); // -10% per preceding loop
      module->params[P::COUNT].setValue(8);
      widget->step();simulate(*module,.2f,frame);
      for(int voice=0;voice<8;++voice)
        if(std::abs(1.f+module->offsetFor(voice,2,false)/100.f-std::pow(.9f,voice))>2e-6f)
          throw std::runtime_error("Multiplicative per-loop speed ratio is incorrect");
      auto* saved=module->toJson();
      bool curveSaved=false;size_t paramIndex;json_t* parameter;
      json_array_foreach(json_object_get(saved,"params"),paramIndex,parameter)
        if(json_integer_value(json_object_get(parameter,"id"))==P::SPEED_OFFSET_CURVE)
          curveSaved=json_number_value(json_object_get(parameter,"value"))==1;
      json_decref(saved);
      if(!curveSaved)throw std::runtime_error("Speed offset curve must persist in patch state");
      save("21-multiplicative-speed-offset");
      if (std::getenv("PHLOOPER_MENU_PREVIEW")) {
        PhlooperChoice* choice=nullptr;
        for(auto* child:widget->children)if(auto* candidate=dynamic_cast<PhlooperChoice*>(child))
          if(candidate->param==P::MODE)choice=candidate;
        if(!choice)throw std::runtime_error("Missing phase-mode dropdown");
        event::Button click;click.button=GLFW_MOUSE_BUTTON_LEFT;click.action=GLFW_PRESS;click.mods=0;
        choice->onButton(click);
        auto* overlay=choice->activeMenuOverlay.get();
        if(!overlay)throw std::runtime_error("Phase-mode dropdown did not open");
        overlay->parent->removeChild(overlay);
        widget::Widget shell;shell.box.size=widget->box.size;shell.addChild(widget.get());shell.addChild(overlay);
        overlay->box=Rect(Vec(),shell.box.size);
        for(auto* child:overlay->children)if(auto* menu=dynamic_cast<ui::Menu*>(child)) {
          menu->step();menu->box.pos=Vec(10,58);shell.box.size.x=std::max(shell.box.size.x,menu->box.size.x+20);
        }
        overlay->box.size=shell.box.size;
        capture(&shell,std::string(argv[3])+"/23-mode-menu.png",scale,false);
        shell.removeChild(widget.get());shell.removeChild(overlay);APP->scene->addChild(overlay);overlay->requestDelete();
        choice->activeMenuOverlay=nullptr;
      }

      module->params[P::SPEED_OFFSET_CURVE].setValue(0);
      module->params[P::COUNT].setValue(4);
      module->params[P::MODE].setValue(mode);module->params[P::OFFSET].setValue(offset);
    }

    if (std::getenv("PHLOOPER_UNCLOCKED_BEATS")) {
      float oldOffset = module->params[ComputerscarePhlooper::OFFSET].getValue();
      module->params[ComputerscarePhlooper::MODE].setValue(3.f);
      module->params[ComputerscarePhlooper::OFFSET].setValue(-4.5f);
      simulate(*module, .1f, frame);
      if (std::abs(module->loop.periods[1] - module->loop.size * .975) > 1.f)
        throw std::runtime_error("Unclocked -1/10 offset must shorten loop 2 by 1/40 of the recording");
      save("18-unclocked-four-beats");
      module->params[ComputerscarePhlooper::LENGTH].setValue(.523f);
      simulate(*module, .01f, frame);
      double base = module->loop.periods[0];
      module->params[ComputerscarePhlooper::OFFSET].setValue(-5.f);
      simulate(*module, .01f, frame);
      if (module->loop.periods[0] != base ||
          std::abs(module->snappedMainValue(ComputerscarePhlooper::LENGTH) * module->loop.size - base) > 1.f)
        throw std::runtime_error("Offset moved the main Length or its tooltip");
      auto* state = module->dataToJson();
      auto restored = std::unique_ptr<ComputerscarePhlooper>(static_cast<ComputerscarePhlooper*>(modelPhlooperPreview->createModule()));
      for (int i=0;i<ComputerscarePhlooper::PARAMS;++i) restored->params[i].setValue(module->params[i].getValue());
      restored->dataFromJson(state); json_decref(state);
      restored->loop.size = module->loop.size;
      for (int i=0;i<16;++i) std::copy(module->loop.audio[i].get(), module->loop.audio[i].get()+module->loop.size*2, restored->loop.audio[i].get());
      simulate(*restored, .01f, frame);
      if (restored->loop.periods[0] != base)
        throw std::runtime_error("Stable beat length did not survive patch serialization");
      save("20-offset-eighth-stable-length");
      module->params[ComputerscarePhlooper::LENGTH].setValue(1.f);

      module->params[ComputerscarePhlooper::MODE].setValue(0.f);
      module->params[ComputerscarePhlooper::OFFSET].setValue(oldOffset);
      save("19-unclocked-time-no-marks");
    }

    module->params[ComputerscarePhlooper::COUNT].setValue(16.f);
    module->params[ComputerscarePhlooper::START].setValue(.2f);
    module->params[ComputerscarePhlooper::LENGTH].setValue(.45f);
    simulate(*module, .2f, frame);
    save("03-sixteen-loops");
    if (std::getenv("PHLOOPER_SELECTOR_CHECKS")) {
      auto overlay = std::unique_ptr<ui::MenuOverlay>(new ui::MenuOverlay);
      PhlooperChoice* count = nullptr;
      PhlooperChoice* expansion = nullptr;
      for (auto* parent : widget->children)
        for (auto* child : parent->children)
          if (auto* choice = dynamic_cast<PhlooperChoice*>(child)) {
            if (choice->param == ComputerscarePhlooper::COUNT) count = choice;
            if (choice->param == ComputerscarePhlooper::EXPANSION) expansion = choice;
          }
      if (!count || !expansion) throw std::runtime_error("Missing separate loop selector buttons");
      count->activeMenuOverlay = overlay.get();
      save("16-count-pressed");
      count->activeMenuOverlay = nullptr;
      expansion->activeMenuOverlay = overlay.get();
      save("17-expansion-pressed");
      expansion->activeMenuOverlay = nullptr;
    }

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
    module->params[ComputerscarePhlooper::LOOP_SOLO + 1].setValue(0.f);
    module->params[ComputerscarePhlooper::LOOP_MUTE + 2].setValue(0.f);
    module->params[ComputerscarePhlooper::START].setValue(.08f);
    module->params[ComputerscarePhlooper::LENGTH].setValue(.8f);
    module->params[ComputerscarePhlooper::COUNT].setValue(3.f);
    module->params[ComputerscarePhlooper::CLOCK_RATIO].setValue(3.f);
    module->inputs[ComputerscarePhlooper::WRITE_CLOCK].channels = 1;
    module->inputs[ComputerscarePhlooper::WRITE_CLOCK].setVoltage(0.f);
    simulate(*module,.01f,frame);
    module->params[ComputerscarePhlooper::RECORD].setValue(1.f);
    for(int pulse=0;pulse<6;++pulse) {
      module->inputs[ComputerscarePhlooper::WRITE_CLOCK].setVoltage(10.f);
      simulate(*module,.001f,frame);
      module->inputs[ComputerscarePhlooper::WRITE_CLOCK].setVoltage(0.f);
      simulate(*module,.249f,frame);
    }
    module->params[ComputerscarePhlooper::RECORD].setValue(0.f);
    simulate(*module,.001f,frame);
    capture(widget.get(),std::string(argv[3])+"/08-clock-queued-stop.png",scale);
    if (!module->queuedRecord.load() || module->clockMarkCount.load()<10)
      throw std::runtime_error("Clock recording must publish queued stop and recorded pulse markers");
    module->inputs[ComputerscarePhlooper::WRITE_CLOCK].setVoltage(10.f);
    simulate(*module,.001f,frame);
    if (module->recording.load() || module->queuedRecord.load())
      throw std::runtime_error("Clock edge must finish the queued overdub");
    module->resetRequest = true;
    module->inputs[ComputerscarePhlooper::WRITE_CLOCK].setVoltage(0.f);
    module->params[ComputerscarePhlooper::CLOCK_RATIO].setValue(-1.f);
    simulate(*module,.01f,frame);
    module->params[ComputerscarePhlooper::RECORD].setValue(1.f);
    simulate(*module,.01f,frame);
    if (module->loop.initial) throw std::runtime_error("Initial recording started before its clock");
    module->inputs[ComputerscarePhlooper::WRITE_CLOCK].setVoltage(10.f);
    simulate(*module,.001f,frame);
    if (!module->loop.initial) throw std::runtime_error("First divided clock must start initial recording");
    module->params[ComputerscarePhlooper::RECORD].setValue(0.f);
    module->inputs[ComputerscarePhlooper::WRITE_CLOCK].setVoltage(0.f);
    simulate(*module,.249f,frame);
    module->inputs[ComputerscarePhlooper::WRITE_CLOCK].setVoltage(10.f);
    simulate(*module,.001f,frame);
    if (!module->loop.initial) throw std::runtime_error("Divided clock stopped initial recording too early");
    module->inputs[ComputerscarePhlooper::WRITE_CLOCK].setVoltage(0.f);
    simulate(*module,.249f,frame);
    module->inputs[ComputerscarePhlooper::WRITE_CLOCK].setVoltage(10.f);
    simulate(*module,.001f,frame);
    if (module->loop.initial || std::abs(module->loop.size-24000)>2)
      throw std::runtime_error("Divided clock must finish a precisely half-second initial recording");
    if (std::abs(module->recordedBeatFrames.load()-24000.f)>2.f)
      throw std::runtime_error("Recorded beats must retain the divided clock interval");
    module->params[ComputerscarePhlooper::MODE].setValue(3.f);
    module->params[ComputerscarePhlooper::OFFSET].setValue(11.f);
    module->params[ComputerscarePhlooper::COUNT].setValue(3.f);
    module->params[ComputerscarePhlooper::LENGTH].setValue(.625f);
    module->inputs[ComputerscarePhlooper::WRITE_CLOCK].channels=0;
    simulate(*module,.05f,frame);
    if (std::abs(module->loop.periods[1]-22500.)>2.)
      throw std::runtime_error("Beat mode must offset playback by 5/16 of the recorded beat: " + std::to_string(module->loop.periods[1]) + " size=" + std::to_string(module->loop.size) + " beat=" + std::to_string(module->recordedBeatFrames.load()) + " hold=" + std::to_string(module->params[ComputerscarePhlooper::HOLD].getValue()) + " offset=" + std::to_string(module->offsetFor(1,3,false)));
    save("09-recorded-beats");
    double retainedBeatLength = module->loop.periods[0];
    module->params[ComputerscarePhlooper::OFFSET].setValue(-4.f);
    simulate(*module,.05f,frame);
    if (std::abs(module->loop.periods[0] - retainedBeatLength) > 1. ||
        std::abs(module->loop.periods[1] - (retainedBeatLength - module->recordedBeatFrames.load()/12.)) > 2.)
      throw std::runtime_error("Minus 1/12 must shorten loop two by one twelfth of a clock-mark division");
    save("10-minus-twelfth-beat");
    module->params[ComputerscarePhlooper::OFFSET].setValue(6.5f);
    module->params[ComputerscarePhlooper::START].setValue(.17f);
    module->params[ComputerscarePhlooper::LENGTH].setValue(.57f);
    simulate(*module,.05f,frame);
    save("11-odd-beat-grid");
    if (std::abs(module->loop.sourceStart(0)-4080.)>1.)
      throw std::runtime_error("Beat quantization must leave Start freely positioned at the knob value");

    if (std::abs(module->getParamQuantity(ComputerscarePhlooper::LENGTH)->getDisplayValue() -
                 module->loop.periods[0] / module->recordedBeatFrames.load()) > .01f)
      throw std::runtime_error("Length tooltip must report the same snapped length used by playback");



    auto* lengthKnob = widget->getParam(ComputerscarePhlooper::LENGTH);
    auto* lengthQuantity = module->getParamQuantity(ComputerscarePhlooper::LENGTH);
    std::string formattedLength = lengthQuantity->getDisplayValueString();
    lengthQuantity->setDisplayValueString(formattedLength);
    if (lengthQuantity->getDisplayValueString() != formattedLength)
      throw std::runtime_error("Beat Length fractions must round-trip through the value editor");
    bool showTooltips = settings::tooltips;
    settings::tooltips = true;
    lengthKnob->createTooltip();
    auto* tooltip = APP->scene->children.back();
    APP->scene->removeChild(tooltip);
    widget->addChild(tooltip);
    tooltip->step();
    tooltip->box.pos = Vec(98, 205);
    capture(widget.get(), std::string(argv[3])+"/12-beat-length-tooltip.png", scale, false);
    widget->removeChild(tooltip);
    APP->scene->addChild(tooltip);
    lengthKnob->destroyTooltip();
    settings::tooltips = showTooltips;
    PhlooperView* loopView = nullptr;
    for (auto* child : widget->children)
      if (auto* candidate = dynamic_cast<PhlooperView*>(child)) loopView = candidate;
    if (!loopView) throw std::runtime_error("Phlooper loop-point view is missing");
    loopView->rememberDrag(1, false);
    auto dragBase = loopView->dragRegion;
    float grid = std::abs(phlooper::beatValue(module->params[ComputerscarePhlooper::OFFSET].getValue())) *
                 module->recordedBeatFrames.load() / module->frames.load();
    auto snapped = phlooper::dragLoopPointOnGrid({dragBase.x,dragBase.y},false,
                    grid*.7f,48.f/module->frames.load(),
                    float(phlooper::capacity)/module->frames.load(),grid,
                    loopView->requestedRegion(1,false,1).x);
    loopView->applyRegion({snapped.start,snapped.end});
    simulate(*module,.01f,frame);
    auto actual = loopView->requestedRegion(1);
    if (std::abs(actual.y-snapped.end)>1e-5 || std::abs(actual.x-dragBase.x)>1e-5)
      throw std::runtime_error("Snapped handle edits must preserve the other point and reach playback");
    loopView->dragVoice=-1;
    save("13-snapped-custom-handle");
    std::puts("Phlooper clocked initial recording, overdub, queued indicators and waveform marker checks passed");
    module->params[ComputerscarePhlooper::STOP].setValue(1);
    module->inputs[ComputerscarePhlooper::RESTART_TRIG].channels=3;
    for(int ch=0;ch<3;++ch)module->inputs[ComputerscarePhlooper::RESTART_TRIG].setVoltage(0,ch);
    simulate(*module,.001f,frame);
    unsigned blink=module->startBlink[1].load();
    module->inputs[ComputerscarePhlooper::RESTART_TRIG].setVoltage(10,1);
    simulate(*module,.001f,frame);widget->step();
    if(module->startBlink[1].load()!=blink+1 || loopView->startFlash(1)<.8f)throw std::runtime_error("Restart trigger must flash its loop's start bracket");
    simulate(*module,.001f,frame);
    if(module->startBlink[1].load()!=blink+1)throw std::runtime_error("Held restart gate must not repeatedly flash");
    module->compactUi=true;widget->step();capture(widget.get(),std::string(argv[3])+"/15-start-bracket-blink.png",scale);
    module->compactUi=false;widget->step();
    loopView->startBlinkTime[1]-=1.;
    if(loopView->startFlash(1)!=0)throw std::runtime_error("Bracket flash must fade back to its normal color");
    checkMaxVisual(widget.get(),module->compactUi,std::string(argv[3])+"/14-max-visual.png",scale);
    widget.reset();

    // Context destroys graphics before terminating GLFW. No settings are saved.
    return 0;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "Preview failed: %s\n", e.what());
    return 1;
  }
}
