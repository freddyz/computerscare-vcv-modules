#include "Computerscare.hpp"
#include "Phlooper/Block.hpp"

namespace beekapoo_audio {
// No device, buffering, gain or filtering. Each used pair is a native Rack
// connection point, preserving input fan-out and stacked output cables.
struct Module : engine::Module {
  static constexpr int capacity = 128;
  std::vector<std::string> names;
  Module() { config(0, capacity, capacity, 0); }
  void process(const ProcessArgs&) override {
    for (int i = 0; i < int(names.size()); ++i) {
      int channels = inputs[i].getChannels();
      outputs[i].setChannels(std::max(1, channels));
      if (!channels) {
        outputs[i].setVoltage(0.f);
        continue;
      }
      for (int channel = 0; channel < std::max(1, channels); channel += 4)
        outputs[i].setVoltageSimd(
            inputs[i].getVoltageSimd<simd::float_4>(channel), channel);
    }
  }
  json_t* dataToJson() override {
    auto* list = json_array();
    for (const auto& name : names)
      json_array_append_new(list, json_string(name.c_str()));
    return json_pack("{s:o}", "names", list);
  }
  void dataFromJson(json_t* root) override {
    names.clear();
    bypassRoutes.clear();
    auto* list = json_object_get(root, "names");
    for (size_t i = 0; i < std::min(size_t(capacity), json_array_size(list));
         ++i) {
      const char* label = json_string_value(json_array_get(list, i));
      names.emplace_back(label ? label : "Audio");
      configInput(int(i), names.back());
      configOutput(int(i), names.back());
      configBypass(int(i), int(i));
    }
    for (int i = int(names.size()); i < capacity; ++i) {
      outputs[i].setChannels(1);
      outputs[i].setVoltage(0.f);
    }
  }
};
struct Widget : app::ModuleWidget {
  int lastCount = -1;
  phlooper::WarpedBlock *header, *body, *footer;
  widget::SvgWidget title, logo;
  Widget(Module* module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(
        pluginInstance, "res/panels/ComputerscareBeekapooAudioPorts.svg")));
    getPanel()->hide();
    title.setSvg(APP->window->loadSvg(
        asset::plugin(pluginInstance, "res/Beekapoo/audio-routing.svg")));
    logo.setSvg(APP->window->loadSvg(
        asset::plugin(pluginInstance, "res/Beekapoo/computerscare-logo.svg")));
    header = new phlooper::WarpedBlock;
    body = new phlooper::WarpedBlock;
    footer = new phlooper::WarpedBlock;
    header->configure(Rect(Vec(), Vec(120, 41)), 3, 1.f, 341,
                      nvgRGB(190, 203, 196));
    body->configure(Rect(Vec(0, 43), Vec(120, 311)), 3, 1.f, 347,
                    nvgRGB(108, 128, 119));
    footer->configure(Rect(Vec(0, 357), Vec(120, 23)), 2, .7f, 353,
                      nvgRGB(190, 203, 196));
    addChild(header);
    addChild(body);
    addChild(footer);
    rebuild();
  }
  void rebuild() {
    auto* m = getModule<Module>();
    int count = m ? int(m->names.size()) : 2;
    if (count == lastCount) return;
    lastCount = count;
    // Preserve native endpoint identities if module data is pasted later.
    for (auto* port : getPorts()) port->visible = port->portId < count;
    int columns = std::max(1, (count + 15) / 16);
    box.size = Vec(std::max(75.f, std::ceil((columns * 48 + 16) / 15.f) * 15),
                   RACK_GRID_HEIGHT);
    for (auto* frame : {header, body, footer}) frame->box.size.x = box.size.x;
    for (int i = 0; i < count; ++i) {
      Vec center(24 + (i / 16) * 48, 65 + (i % 16) * 18);
      if (!getInput(i))
        addInput(createInputCentered<JackSmallPointingDown>(center, m, i));
      if (!getOutput(i))
        addOutput(createOutputCentered<JackSmallPointingUp>(center + Vec(21, 0),
                                                            m, i));
      getInput(i)->box.pos = center - getInput(i)->box.size * .5f;
      getOutput(i)->box.pos =
          center + Vec(21, 0) - getOutput(i)->box.size * .5f;
    }
  }
  void step() override {
    rebuild();
    app::ModuleWidget::step();
  }
  void draw(const DrawArgs& args) override {
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
    nvgFillColor(args.vg, nvgRGB(190, 203, 196));
    nvgFill(args.vg);
    app::ModuleWidget::draw(args);
    auto art = [&](widget::SvgWidget& svg, Rect area) {
      if (!svg.svg || svg.box.size.x <= 0 || svg.box.size.y <= 0) return;
      float scale =
          std::min(area.size.x / svg.box.size.x, area.size.y / svg.box.size.y);
      nvgSave(args.vg);
      nvgTranslate(args.vg, area.pos.x, area.pos.y);
      nvgScale(args.vg, scale, scale);
      svg.draw(args);
      nvgRestore(args.vg);
    };
    art(title, Rect(Vec(8, 8), Vec(std::min(170.f, box.size.x - 16), 25)));
    art(logo, Rect(Vec(box.size.x - 27, 360), Vec(19, 16)));
  }
};
}  // namespace beekapoo_audio
Model* modelComputerscareBeekapooAudioPorts = [] {
  auto* model = createModel<beekapoo_audio::Module, beekapoo_audio::Widget>(
      "computerscare-beekapoo-audio-ports");
  model->hidden = true;
  model->name = "Beekapoo audio routing";
  return model;
}();
