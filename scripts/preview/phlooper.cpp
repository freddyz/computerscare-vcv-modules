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

void capture(app::ModuleWidget* widget, const std::string& path, float scale) {
  // Step while unattached so the Rack-only keyboard overlay isn't installed.
  widget->step();
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
  APP->engine->addModule(module);
  auto widget = std::unique_ptr<ComputerscareBlunchWidget>(
      static_cast<ComputerscareBlunchWidget*>(
          modelBlunchPreview->createModuleWidget(module)));
  const char* formulas[] = {"120bpm",  "[300 400]ms",   "(.5 .7 (.3 .4))s",
                            "90bpm@4", "{100|140}bpm",  "250ms ~250ms",
                            "2hz",     "[110 130]bpm@2"};
  for (int ch = 0; ch < 16; ++ch) {
    module->sequenceEditorStates[ch].text = formulas[ch % 8];
    if (ch == 1) module->sequenceEditorStates[ch].text += "\n180bpm";
    require(module->commitLineForChannel(ch, 0, true),
            "preview formula must compile");
    module->sequencers[ch].running = true;
  }
  module->params[ComputerscareBlunch::POLY_CHANNELS_PARAM].setValue(8.f);
  module->setControlMode(true);
  widget->step();
  auto* editor = static_cast<BlunchTextEditor*>(widget->editor);
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
    modelPhlooperPreview->plugin = &plugin;
    if (argc >= 6 && std::string(argv[5]) == "browser") {
      plugin::Model* browserModel = modelPhlooperPreview;
      if (argc == 7 && std::string(argv[6]) == "blunch")
        browserModel = modelBlunchPreview;
      if (argc == 7 && std::string(argv[6]) == "windchimes")
        browserModel = modelComputerscareWindchimes;
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
    widget.reset();

    // Context destroys graphics before terminating GLFW. No settings are saved.
    return 0;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "Preview failed: %s\n", e.what());
    return 1;
  }
}
