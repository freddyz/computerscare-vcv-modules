#pragma once
inline void checkController(plugin::Model* model, const std::string& dir,
                            float scale) {
  setenv("CONTROLLER_PREVIEW", "1", 1);
  using M = ComputerscareController;
  auto* m = static_cast<M*>(model->createModule());
  APP->engine->addModule(m);
  auto widget =
      std::unique_ptr<app::ModuleWidget>(model->createModuleWidget(m));
  for (auto& output : m->outputs)
    output.channels = 1;  // Cables normally set this in Rack.
  auto c = std::make_shared<cc::Connection>();
  c->profile = cc::DualSense;
  c->name = "DualSense — preview fixture";
  c->connected = true;
  c->supportedAnalog = (1u << cc::analogs) - 1;
  c->supportedButtons = (1u << cc::buttons) - 1;
  c->values[0] = 1;
  c->values[1] = -.5;
  c->values[4] = .75;
  c->values[6] = .8;
  c->values[7] = 1;
  c->values[8] = .32;
  c->values[9] = .76;
  c->values[12] = 3.14159265f;
  c->values[15] = 19.6133;
  m->params[M::SMOOTHING].setValue(0);
  m->params[M::MODE].setValue(1);
  m->bind(c);
  engine::Module::ProcessArgs args;
  args.sampleRate = 48000;
  args.sampleTime = 1.f / 48000;
  args.frame = 0;
  m->process(args);
  auto require = [](bool okay, const char* message) {
    if (!okay) throw std::runtime_error(message);
  };
  require(m->outputs[M::ANALOG + 4].getVoltage() == 7.5f,
          "Controller trigger range");
  require(std::fabs(m->outputs[M::ANALOG + 12].getVoltage() - 5) < 1e-4,
          "Controller gyro range");
  require(std::fabs(m->outputs[M::ANALOG + 15].getVoltage() - 5) < 1e-4,
          "Controller accelerometer range");
  c->setButton(0, true);
  c->setButton(0, false);
  ++args.frame;
  m->process(args);
  require(m->outputs[0].getVoltage() == 10,
          "Controller short button press must survive polling");
  require(m->outputs[M::BANK].getVoltage(0) == 10 &&
              m->outputs[M::BANK].getChannels() == 16,
          "Controller button bank mirrors named outputs");
  m->params[M::SCALE + M::ANALOG + 4].setValue(2);
  m->params[M::OFFSET + M::ANALOG + 4].setValue(-2);
  ++args.frame;
  m->process(args);
  require(m->outputs[M::ANALOG + 4].getVoltage() == 13,
          "Per-control scale and offset apply in volts");
  require(m->outputs[M::BANK + 2].getVoltage(4) == 13,
          "Banks carry transformed CV");
  m->params[M::SCALE + M::ANALOG + 4].setValue(1);
  m->params[M::OFFSET + M::ANALOG + 4].setValue(0);
  m->inputs[M::RUMBLE_LOW].channels = 1;
  m->inputs[M::RUMBLE_HIGH].channels = 1;
  m->inputs[M::RUMBLE_LOW].setVoltage(5);
  m->inputs[M::RUMBLE_HIGH].setVoltage(12);
  for (int i = 0; i < 64; ++i) {
    ++args.frame;
    m->process(args);
  }
  uint64_t command = c->rumble.command.load();
  require(uint16_t(command) == 32767 && uint16_t(command >> 16) == 65535,
          "Rumble CV maps and clamps both motor intensities");
  c->rumble.stop(m->rumbleOwner + 1);
  require(c->rumble.command.load() == command,
          "Unrelated modules cannot cancel rumble");
  m->inputs[M::RUMBLE_LOW].channels = 0;
  m->inputs[M::RUMBLE_HIGH].channels = 0;
  for (int i = 0; i < 64; ++i) {
    ++args.frame;
    m->process(args);
  }
  require(c->rumble.command.load() == 0,
          "Unpatching rumble clears the request");
  m->params[M::CENTER].setValue(1);
  ++args.frame;
  m->process(args);
  require(m->outputs[M::ANALOG + 12].getVoltage() == 0,
          "Controller motion center");
  m->params[M::CENTER].setValue(0);
  capture(widget.get(), dir + "/01-dualsense.png", scale);
  auto* originalPort = widget->getOutput(0);
  c->setButton(1, true);
  c->setButton(8, true);
  ++args.frame;
  m->process(args);
  widget->step();
  require(widget->box.size.x == 225, "Compact controller fits fifteen HP");
  require(widget->getOutput(0) == originalPort,
          "Layout changes preserve cable endpoint widgets");
  std::vector<PortWidget*> ports;
  for (int i = 0; i < M::OUTPUTS; ++i) ports.push_back(widget->getOutput(i));
  for (int i = 0; i < M::INPUTS; ++i) ports.push_back(widget->getInput(i));
  for (auto* port : ports) {
    require(port && port->visible && port->box.size.x <= 16.1f,
            "Every compact connection remains visible at tiny size");
    require(port->box.pos.x >= 0 && port->box.pos.x + port->box.size.x <= 225 &&
                port->box.pos.y >= 24 &&
                port->box.pos.y + port->box.size.y <= 380,
            "Compact jacks stay within the panel");
  }
  for (size_t a = 0; a < ports.size(); ++a)
    for (size_t b = a + 1; b < ports.size(); ++b) {
      auto ra = ports[a]->box;
      auto rb = ports[b]->box;
      require(
          !(ra.pos.x < rb.pos.x + rb.size.x &&
            ra.pos.x + ra.size.x > rb.pos.x &&
            ra.pos.y < rb.pos.y + rb.size.y && ra.pos.y + ra.size.y > rb.pos.y),
          "Compact jack hit areas never overlap");
    }
  m->setButtonModes(2);
  for (int i = 0; i < cc::buttons; ++i)
    require(m->params[M::MODE + i].getValue() == 2,
            "Set-all updates every button");
  APP->history->undo();
  require(m->params[M::MODE].getValue() == 1 &&
              m->params[M::MODE + 1].getValue() == 0,
          "Set-all undo restores mixed button modes");
  APP->history->redo();
  require(m->params[M::MODE + 1].getValue() == 2,
          "Set-all redo restores the requested mode");
  m->setButtonModes(0);
  m->params[M::OFFSET].setValue(2);
  m->params[M::SCALE].setValue(.5);
  m->params[M::OFFSET + 1].setValue(3);
  m->params[M::SCALE + 1].setValue(1.5);
  m->resetTransforms(0);
  require(m->params[M::OFFSET].getValue() == 0 &&
              m->params[M::SCALE].getValue() == 1 &&
              m->params[M::OFFSET + 1].getValue() == 3,
          "Per-control reset leaves neighbouring controls alone");
  APP->history->undo();
  require(m->params[M::OFFSET].getValue() == 2 &&
              m->params[M::SCALE].getValue() == .5,
          "Reset undo restores both offset and scale");
  APP->history->redo();
  m->resetTransforms();
  for (int i = 0; i < cc::buttons + cc::analogs; ++i)
    require(m->params[M::OFFSET + i].getValue() == 0 &&
                m->params[M::SCALE + i].getValue() == 1,
            "Global reset restores all transforms");
  require(m->params[M::MODE].getValue() == 0,
          "Transform resets preserve button mode");
  ++args.frame;
  m->process(args);
  capture(widget.get(), dir + "/04-compact.png", scale);
  {
    widget::Widget composition;
    composition.box.size = widget->box.size;
    composition.addChild(widget.get());
    std::vector<GroupCableWidget*> cables;
    for (int i = 0; i < 2; ++i) {
      auto* cable = new GroupCableWidget;
      cable->outputPort = widget->getOutput(i == 0 ? 1 : M::ANALOG);
      cable->inputPort = widget->getInput(i);
      cable->color = i == 0 ? nvgRGB(55, 155, 192) : nvgRGB(200, 120, 55);
      cable->box.size = composition.box.size;
      composition.addChild(cable);
      composition.addChild(cable->outputPlug);
      composition.addChild(cable->inputPlug);
      cable->updateCable();
      cables.push_back(cable);
    }
    capture(&composition, dir + "/05-compact-patched.png", scale);
    for (auto* cable : cables) {
      composition.removeChild(cable->inputPlug);
      composition.removeChild(cable->outputPlug);
      composition.removeChild(cable);
      delete cable;
    }
    composition.removeChild(widget.get());
  }

  widget->step();
  require(widget->box.size.x == 225 && widget->getOutput(0) == originalPort,
          "Controller stays compact with stable cable endpoints");
  c->profile = cc::DualShock4;
  c->name = "DualShock 4 — preview fixture";
  m->bind(c);
  c->supportedButtons.fetch_and(~(1u << 16));
  ++args.frame;
  m->process(args);
  capture(widget.get(), dir + "/03-dualshock4.png", scale);
  auto* saved = m->dataToJson();
  auto restored = std::unique_ptr<M>(static_cast<M*>(model->createModule()));
  restored->dataFromJson(saved);
  json_decref(saved);
  require(restored->profile == cc::DualShock4, "Controller profile persists");
  c->connected = false;
  ++args.frame;
  m->process(args);
  for (auto& output : m->outputs)
    for (int ch = 0; ch < output.getChannels(); ++ch)
      require(output.getVoltage(ch) == 0, "Controller unplug clears outputs");
  auto xbox = std::make_shared<cc::Connection>();
  xbox->profile = cc::Xbox;
  xbox->name = "Xbox Series — preview fixture";
  xbox->connected = true;
  xbox->supportedAnalog = 255;
  xbox->supportedButtons = ((1u << 15) - 1) | (1u << 16) | (3u << 19);
  m->params[M::OFFSET + M::ANALOG + 12].setValue(6);
  m->bind(xbox);
  ++args.frame;
  m->process(args);
  require(m->outputs[M::ANALOG + 12].getVoltage() == 0,
          "Xbox has no motion output");
  require(!m->connection.load() && !m->online,
          "PlayStation refuses to silently bind an Xbox controller");
  // Exercise the same UI/audio ownership split as hot-unplug and rebind.
  std::atomic<bool> startStress{false};
  std::thread audio([&]() {
    auto tick = args;
    while (!startStress.load()) std::this_thread::yield();
    for (int i = 0; i < 50000; ++i) {
      ++tick.frame;
      m->process(tick);
    }
  });
  startStress = true;
  for (int i = 0; i < 1000; ++i) {
    auto state = std::make_shared<cc::Connection>();
    state->profile = cc::DualSense;
    state->supportedAnalog = (1u << cc::analogs) - 1;
    state->supportedButtons = (1u << cc::buttons) - 1;
    state->values[0] = .5f;
    state->connected = true;
    m->bind(state);
    state->setButton(0, true);
    state->connected = false;
  }
  m->bind(nullptr);
  audio.join();
  ++args.frame;
  m->process(args);
  for (auto& output : m->outputs)
    for (int ch = 0; ch < output.getChannels(); ++ch)
      require(output.getVoltage(ch) == 0,
              "Concurrent disconnects and rebinding leave all outputs at zero");
  std::puts(
      "Controller concurrent hot-unplug/rebind stress passed (1,000 "
      "connections, 50,000 audio ticks)");
  std::puts(
      "Controller ranges, scale/offset, short presses, banks, centering, "
      "persistence and "
      "disconnect checks passed");
}

inline void checkXboxController(plugin::Model* model, const std::string& dir,
                                float scale) {
  setenv("CONTROLLER_PREVIEW", "1", 1);
  using M = ComputerscareController;
  auto* m = static_cast<M*>(model->createModule());
  APP->engine->addModule(m);
  auto w = std::unique_ptr<app::ModuleWidget>(model->createModuleWidget(m));
  auto require = [](bool okay, const char* why) {
    if (!okay) throw std::runtime_error(why);
  };
  require(m->deviceFamily == cc::Xbox && w->box.size.x == 180,
          "Xbox has a fixed twelve HP panel");
  for (auto& out : m->outputs) out.channels = 1;
  for (int i : {15, 17, 18, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 42})
    require(w->getOutput(i) == nullptr, "Xbox omits PlayStation-only jacks");
  for (int i : {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14,
                16, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 39, 40, 41})
    require(w->getOutput(i) != nullptr,
            "Xbox retains every supported individual output and bank");
  auto c = std::make_shared<cc::Connection>();
  c->profile = cc::Xbox;
  c->name = "Xbox Series — preview fixture";
  c->connected = true;
  c->supportedAnalog = 255;
  c->supportedButtons = ((1u << cc::buttons) - 1);
  c->values[0] = .5;
  c->values[4] = .75;
  c->values[6] = .65;
  c->values[7] = 1;
  c->setButton(16, true);
  c->setButton(13, true);
  c->setButton(2, true);
  c->setButton(9, true);
  m->bind(c);
  m->params[M::DEADZONE].setValue(0);
  m->params[M::SMOOTHING].setValue(0);
  m->params[M::SCALE + M::ANALOG].setValue(1.5);
  m->params[M::OFFSET + M::ANALOG].setValue(-1);
  engine::Module::ProcessArgs args;
  args.sampleRate = 48000;
  args.sampleTime = 1.f / 48000;
  args.frame = 0;
  m->process(args);
  require(m->outputs[M::BANK].getChannels() == 16 &&
              m->outputs[M::BANK].getVoltage(14) == 0 &&
              m->outputs[M::BANK].getVoltage(15) == 0,
          "Xbox keeps missing touches as zero in Poly1 slots 15–16");
  const float expected[] = {0, 10, 0, 10, 0};
  for (int i = 0; i < 5; ++i)
    require(m->outputs[M::BANK + 1].getVoltage(i) == expected[i],
            "Xbox Poly2 preserves Pad/Share/View/Menu/Home slot compatibility");
  require(m->outputs[M::ANALOG].getVoltage() == 2.75f &&
              m->outputs[M::BANK + 2].getVoltage() == 2.75f,
          "Xbox shares per-control scale/offset processing");
  for (int i = 0; i < M::OUTPUTS; ++i)
    if (auto* port = w->getOutput(i))
      require(port->box.pos.x >= 0 &&
                  port->box.pos.x + port->box.size.x <= 180 &&
                  port->box.pos.y + port->box.size.y <= 380,
              "Xbox jacks fit the fixed panel");
  require(w->getOutput(16)->box.pos.x == w->getOutput(13)->box.pos.x &&
              w->getOutput(12)->box.pos.x == w->getOutput(14)->box.pos.x,
          "Xbox auxiliary buttons form a filled two-by-two group");
  require(w->getOutput(M::ANALOG + 6)->box.pos.y >
              w->getOutput(M::ANALOG + 7)->box.pos.y,
          "Battery is the last individual control");
  capture(w.get(), dir + "/01-xbox.png", scale);
  auto ps = std::make_shared<cc::Connection>();
  ps->profile = cc::DualSense;
  ps->connected = true;
  m->bind(ps);
  ++args.frame;
  m->process(args);
  require(!m->connection.load() && !m->online,
          "Xbox refuses to silently switch to a PlayStation controller");
  auto* saved = m->dataToJson();
  json_object_set_new(saved, "profile", json_integer(cc::DualSense));
  auto clone = std::unique_ptr<M>(static_cast<M*>(model->createModule()));
  clone->dataFromJson(saved);
  json_decref(saved);
  require(clone->profile == cc::Xbox,
          "Xbox panel identity survives incompatible saved profile data");
  std::puts(
      "Xbox fixed layout, supported jacks, compatible bank slots, transforms, "
      "and controller-family switching checks passed");
}
