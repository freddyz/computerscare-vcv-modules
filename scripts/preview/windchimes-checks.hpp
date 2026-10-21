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
  {
    std::unique_ptr<engine::Module> owner(model->createModule());
    auto* target=owner.get();
    auto idFor=[&](const std::string& name) {
      for (size_t id=0;id<target->params.size();++id)
        if (target->getParamQuantity(id)->name==name) return int(id);
      throw std::runtime_error("Missing rndblk parameter: "+name);
    };
    auto cvFor=[&](const std::string& name) -> engine::Input& {
      for (size_t id=0;id<target->inputs.size();++id)
        if (target->getInputInfo(id)->name==name) return target->inputs[id];
      throw std::runtime_error("Missing rndblk input: "+name);
    };
    int wind=idFor("Wind strength"), gust=idFor("Gustiness");
    int mix=idFor("Expanse"), size=idFor("Thicket");
    target->params[wind].setValue(.42f); target->params[mix].setValue(.31f);
    json_t* data=json_object(); json_t* locks=json_array();
    json_array_append_new(locks,json_integer(wind)); json_array_append_new(locks,json_integer(mix));
    json_object_set_new(data,"randomLocks",locks); target->dataFromJson(data); json_decref(data);
    engine::Module::ProcessArgs args; args.sampleRate=48000; args.sampleTime=1.f/48000; args.frame=0;
    target->process(args);  // Establish low trigger states before the first edge.
    auto& randomWind=cvFor("Randomize wind"); randomWind.channels=1; randomWind.setVoltage(10.f);
    target->process(args);
    require(target->params[wind].getValue()==.42f, "Wind random CV must respect knob locks");
    float randomized=target->params[gust].getValue();
    require(randomized!=.55f, "Wind random CV changes unlocked controls");
    target->process(args);
    require(target->params[gust].getValue()==randomized, "rndblk triggers once per rising edge");
    auto& wiggle=cvFor("Wiggle wind"); wiggle.channels=1; wiggle.setVoltage(10.f);
    target->process(args);
    require(target->params[wind].getValue()==.42f && target->params[gust].getValue()!=randomized &&
                std::abs(target->params[gust].getValue()-randomized)<=.051f,
            "Wiggle CV makes small changes while leaving locked controls fixed");
    auto& randomReverb=cvFor("Randomize forest"); randomReverb.channels=1; randomReverb.setVoltage(10.f);
    target->process(args);
    require(target->params[mix].getValue()==.31f && target->params[size].getValue()!=.55f,
            "Forest randomization is grouped and respects locks");
    json_t* saved=target->dataToJson();
    std::unique_ptr<engine::Module> loaded(model->createModule()); loaded->dataFromJson(saved);
    json_decref(saved); json_t* roundtrip=loaded->dataToJson();
    require(json_array_size(json_object_get(roundtrip,"randomLocks"))==2, "Knob locks survive patch reload");
    json_decref(roundtrip);
    std::puts("rndblk trigger, group isolation, wiggle amount, knob lock, and persistence checks passed");
  }
  for (size_t i = 0; i < module->params.size(); ++i) {
    auto* gain = module->getParamQuantity(i);
    if (gain->name != "Gain") continue;
    float original = gain->getValue();
    require(gain->getUnit() == " dB" && std::fabs(original - .8f) < .0001f,
            "Gain uses dB while preserving the previous default level");
    gain->setDisplayValue(18.f);
    require(std::fabs(gain->getValue() - 7.94328235f) < .0001f &&
                std::fabs(gain->getDisplayValue() - 18.f) < .001f,
            "Gain reaches +18 dB and accepts typed decibel values");
    gain->setDisplayValue(0.f);
    require(std::fabs(gain->getValue() - 1.f) < .0001f,
            "Zero dB gives unity gain");
    gain->setDisplayValue(-80.f);
    require(gain->getValue() == 0.f && gain->getDisplayValueString() == "−∞",
            "Minimum gain is silence");
    gain->setValue(original);
  }
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
  float previousTubes = parameter("Chimes 2 tubes").getValue();
  float materialBefore = parameter("Chimes 2 material").getValue();
  parameter("Chimes 2 level").setValue(0.f);
  first = parameter("Chimes 1 decay").getValue();
  step(500);
  require(std::fabs(parameter("Chimes 2 tubes").getValue() - previousTubes) == 1.f,
          "Wiggle changes Tubes by one");
  require(parameter("Chimes 2 level").getValue() >= .3f &&
              parameter("Chimes 2 level").getValue() <= 1.f &&
              parameter("Chimes 2 material").getValue() == materialBefore,
          "Wiggle bounds Level and preserves Material");
  int changed = 0;
  for (int i = 0; i < 7; ++i) {
    float difference = std::fabs(
        parameter(std::string("Chimes 2 ") + names[i]).getValue() - before[i]);
    require(difference <= .050001f, "Wiggle must stay within five percent");
    if (difference > 0.f) ++changed;
  }
  require(changed == 7 &&
              first == parameter("Chimes 1 decay").getValue(),
          "Poly wiggle must nudge every sound control of its own chime");
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
  for (int trial = 0; trial < 12; ++trial) {
    float material = parameter("Chimes 1 material").getValue();
    float secondLevel = parameter("Chimes 2 level").getValue();
    pressTop(782.75f);
    require(parameter("Chimes 1 level").getValue() >= .3f &&
                parameter("Chimes 1 level").getValue() <= 1.f,
            "Random keeps Level between thirty percent and full");
    require(parameter("Chimes 1 material").getValue() == material &&
                parameter("Chimes 2 level").getValue() == secondLevel,
            "Random preserves material and unselected chimes");
    float tubes = parameter("Chimes 1 tubes").getValue();
    require(tubes >= 1.f && tubes <= 12.f && tubes == std::floor(tubes),
            "Random chooses whole tube counts");
    APP->history->undo();
    for (size_t i = 0; i < original.size(); ++i)
      require(module->params[i].getValue() == original[i],
              "One undo restores a selected model randomization");
  }
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
