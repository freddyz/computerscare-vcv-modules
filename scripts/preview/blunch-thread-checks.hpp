#pragma once

#include <atomic>
#include <exception>
#include <thread>

inline void checkBlunchThreadHandoff(rack::plugin::Model* model) {
  using B = ComputerscareBlunch;
  auto require = [](bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
  };
  auto m = std::unique_ptr<B>(static_cast<B*>(model->createModule()));
  m->params[B::POLY_CHANNELS_PARAM].setValue(16.f);
  for (int ch = 0; ch < 16; ++ch) {
    m->runningLineTexts[ch].clear();
    m->sequenceEditorStates[ch].text = "2hz\n[300 400]ms\n120bpm";
    require(m->commitLineForChannel(ch, 0, true), "thread-test baseline compiles");
    m->sequencers[ch].running = true;
  }
  rack::engine::Module::ProcessArgs args;
  args.sampleRate = 48000.f;
  args.sampleTime = 1.f / 48000.f;
  args.frame = 0;
  for (int i = 0; i < 100; ++i) { m->process(args); ++args.frame; }
  float before = m->sequencers[0].clockPhase;
  auto* context = APP;
  {
    // A GUI critical section must not block the audio callback. Keep the lock
    // until this thread has completed all callbacks; a blocking lock deadlocks.
    B::StateGuard gui(m->stateMutex);
    std::thread audio([&]() {
      rack::contextSet(context);
      for (int i = 0; i < 100; ++i) { m->process(args); ++args.frame; }
      rack::contextSet(nullptr);
    });
    audio.join();
  }
  m->process(args);
  float elapsed = 2.f * 101.f / 48000.f;
  require(std::fabs(m->sequencers[0].clockPhase - before - elapsed) < .0001f,
          "Audio callback never blocks on GUI work and retains elapsed clock time");

  m->focusedChannel = 0;
  m->params[B::WAIT_MODE_PARAM].setValue(B::WAIT_CLOCK);
  m->sequenceEditorStates[0].text = "4hz\n[300 400]ms\n120bpm";
  require(m->executeLine(0, true), "Queue protection fixture stages");
  {
    B::StateGuard gui(m->stateMutex);
    m->params[B::WAIT_MODE_PARAM].setValue(B::WAIT_IMMEDIATE);
    m->startAllRequested = true;
    std::thread audio([&]() {
      rack::contextSet(context);
      for (int i = 0; i < 100; ++i) { m->process(args); ++args.frame; }
      rack::contextSet(nullptr);
    });
    audio.join();
    require(m->pendingFormulas[0].lines.count(0) &&
                m->sequencers[0].activeLineText == "2hz",
            "Start All cannot consume or invalidate a queue while the GUI owns it");
  }
  m->process(args);
  require(!m->channelHasNext(0) && m->sequencers[0].activeLineText == "4hz",
          "Deferred Start All safely applies the queued formula after handoff");

  std::atomic<bool> ready{false}, done{false};
  std::atomic<unsigned> callbacks{0};
  std::exception_ptr audioError;
  std::thread audio([&]() {
    rack::contextSet(context);
    ready.store(true);
    try {
      while (!done.load() || callbacks.load() < 96000) {
        m->process(args);
        ++args.frame;
        ++callbacks;
      }
    } catch (...) { audioError = std::current_exception(); }
    rack::contextSet(nullptr);
  });
  while (!ready.load()) std::this_thread::yield();
  try {
    for (int edit = 0; edit < 1500; ++edit) {
      B::StateGuard gui(m->stateMutex);
      int ch = edit % 16;
      m->focusedChannel = ch;
      m->params[B::WAIT_MODE_PARAM].setValue(edit % 2 ? B::WAIT_CLOCK : B::WAIT_IMMEDIATE);
      m->sequenceEditorStates[ch].text = std::string(edit % 3 ? "4hz" : "3hz") +
                                        "\n[300 400]ms\n120bpm";
      if (edit % 3) m->executeLine(0, true);
      else m->executePage();
      if (edit % 5 == 0) m->discardChannelPending(ch, true);
      // Exercise the same command posted by the real Ctrl+Space handler.
      ComputerscareTextEditorCommands command;
      computerscare::blunch::KeyboardShortcutOptions options;
      computerscare::blunch::handleKeyboardShortcut(
          GLFW_KEY_SPACE, " ", GLFW_MOD_CONTROL, GLFW_PRESS, options, command);
      require(command.startAllCount == 1, "Ctrl+Space posts Start All");
      m->startAllRequested = true;
      m->refreshChannelsEditorState();
      (void)m->layerPage(ch, B::NEXT);
      if (edit % 50 == 0) json_decref(m->dataToJson());
    }
  } catch (...) {
    done.store(true);
    audio.join();
    throw;
  }
  done.store(true);
  audio.join();
  if (audioError) std::rethrow_exception(audioError);
  require(callbacks.load() >= 96000, "Concurrent stress executes real audio callbacks");
  {
    B::StateGuard gui(m->stateMutex);
    for (int ch = 0; ch < 16; ++ch) {
      const auto& seq = m->sequencers[ch];
      require(std::isfinite(seq.clockPhase) && !seq.activeProgram.empty() &&
                  seq.activeProgramIndex >= 0 &&
                  seq.activeProgramIndex < int(seq.activeProgram.size()),
              "Concurrent staging and Start All retain valid sequencer programs");
    }
  }
  std::puts("Blunch concurrent edit/stage/cancel/serialize/Start All stress checks passed.");
}
