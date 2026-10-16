#pragma once

// Optional integration checks against the plugin's actual Rack module.
inline void checkWindchimesCv(rack::plugin::Model* model) {
  using namespace rack;
  std::unique_ptr<engine::Module> moduleOwner(model->createModule());
  auto* module = moduleOwner.get();
  auto require = [](bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
  };
  auto input = [&](const std::string& name) -> engine::Input& {
    for (size_t i = 0; i < module->inputs.size(); ++i)
      if (module->getInputInfo(i)->name == name) return module->inputs[i];
    throw std::runtime_error("Missing input: " + name);
  };
  auto parameter = [&](const std::string& name) -> engine::Param& {
    for (size_t i = 0; i < module->params.size(); ++i)
      if (module->getParamQuantity(i)->name == name) return module->params[i];
    throw std::runtime_error("Missing parameter: " + name);
  };
  engine::Module::ProcessArgs args;
  args.sampleRate = 48000.f;
  args.sampleTime = 1.f / args.sampleRate;
  args.frame = 0;
  auto step = [&](int samples) {
    for (int i = 0; i < samples; ++i) {
      module->process(args);
      ++args.frame;
    }
  };
  parameter("Chimes 2 enabled").setValue(1.f);
  // Simulate Rack cable connection; setChannels cannot connect a disconnected
  // port.
  input("Random trigger").channels = 2;
  step(1);
  input("Random trigger").setVoltage(10.f, 1);
  step(500);
  float first = parameter("Chimes 1 decay").getValue();
  float second = parameter("Chimes 2 decay").getValue();
  require(first == .55f && second != .55f,
          "Poly random must target only the matching chime");
  step(500);
  require(second == parameter("Chimes 2 decay").getValue(),
          "Sustained random gate must not retrigger");
  input("Random trigger").setVoltage(0.f, 1);
  step(1);
  input("Random trigger").setChannels(1);
  input("Random trigger").setVoltage(10.f);
  step(500);
  require(first != parameter("Chimes 1 decay").getValue() &&
              second != parameter("Chimes 2 decay").getValue(),
          "Mono random must broadcast to all enabled chimes");
  input("Wiggle trigger").channels = 2;
  input("Wiggle trigger").setVoltage(10.f, 1);
  const char* names[] = {"decay",          "brightness", "striker hardness",
                         "striker weight", "shape",      "body",
                         "inharmonicity"};
  std::array<float, 7> before{};
  for (int i = 0; i < 7; ++i)
    before[i] = parameter(std::string("Chimes 2 ") + names[i]).getValue();
  first = parameter("Chimes 1 decay").getValue();
  step(500);
  int changed = 0;
  for (int i = 0; i < 7; ++i) {
    float difference = std::fabs(
        parameter(std::string("Chimes 2 ") + names[i]).getValue() - before[i]);
    require(difference <= .050001f, "Wiggle must stay within five percent");
    if (difference > 0.f) ++changed;
  }
  require(changed >= 1 && changed <= 6 &&
              first == parameter("Chimes 1 decay").getValue(),
          "Poly wiggle must nudge one to six controls of its own chime");
  APP->engine->addModule(module);
  auto widget =
      std::unique_ptr<app::ModuleWidget>(model->createModuleWidget(module));
  // ModuleWidget owns and unregisters its module on destruction.
  moduleOwner.release();
  widget->step();
  auto pressTop = [&](float x, int modifiers = 0) {
    for (auto* child : widget->children) {
      if (std::fabs(child->box.pos.x - x) < 1.f &&
          std::fabs(child->box.pos.y - 22.f) < .1f) {
        event::Button event;
        event.pos = Vec(5, 5);
        event.button = GLFW_MOUSE_BUTTON_LEFT;
        event.action = GLFW_PRESS;
        event.mods = modifiers;
        child->onButton(event);
        event.action = GLFW_RELEASE;
        child->onButton(event);
        return;
      }
    }
    throw std::runtime_error("Missing top-level randomization button");
  };
  std::vector<float> original;
  for (auto& p : module->params) original.push_back(p.getValue());
  for (int trial = 0; trial < 16; ++trial) {
    pressTop(705.9f);
    int enabled = 0;
    for (int ch = 1; ch <= 16; ++ch) {
      auto prefix = std::string("Chimes ") + std::to_string(ch) + " ";
      if (parameter(prefix + "enabled").getValue() > .5f) ++enabled;
      require(parameter(prefix + "mute").getValue() == 0 &&
                  parameter(prefix + "solo").getValue() == 0,
              "New scenes must start unmuted and unsoloed");
    }
    require(enabled >= 2 && enabled <= 8,
            "Randomize all must create two to eight chimes");
    for (size_t i = 0; i < module->params.size(); ++i) {
      auto* q = module->getParamQuantity(i);
      require(module->params[i].getValue() >= q->getMinValue() &&
                  module->params[i].getValue() <= q->getMaxValue(),
              "Scene settings must stay within control ranges");
    }
    step(500);
    APP->history->undo();
    for (size_t i = 0; i < original.size(); ++i)
      require(module->params[i].getValue() == original[i],
              "One undo must restore all previous scene settings");
    widget->step();
  }
  float position = parameter("Chimes 1 stage horizontal").getValue();
  float tuning = parameter("Chimes 1 root").getValue();
  pressTop(837.85f, GLFW_MOD_SHIFT);
  require(position == parameter("Chimes 1 stage horizontal").getValue() &&
              tuning == parameter("Chimes 1 root").getValue(),
          "Top Wiggle must preserve scene position and tuning");
  APP->history->undo();
  std::puts(
      "Windchimes native CV, scene randomization, and undo checks passed");
}
