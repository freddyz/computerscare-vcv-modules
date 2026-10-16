#pragma once

// Optional integration checks against the plugin's actual Rack module.
inline void checkWindchimesCv(rack::plugin::Model* model) {
  using namespace rack;
  std::unique_ptr<engine::Module> module(model->createModule());
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
  std::puts("Windchimes native CV checks passed");
}
