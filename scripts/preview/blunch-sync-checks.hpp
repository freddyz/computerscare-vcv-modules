#pragma once

inline void checkBlunchChannelSync(rack::plugin::Model* model) {
  using B = ComputerscareBlunch;
  auto require = [](bool ok, const char* why) {
    if (!ok) throw std::runtime_error(why);
  };
  auto make = [&](int leader, bool seed) {
    auto m = std::unique_ptr<B>(static_cast<B*>(model->createModule()));
    m->params[B::POLY_CHANNELS_PARAM].setValue(3.f);
    m->params[B::RUN_PARAM].setValue(1.f);
    m->lastRunHigh = true;
    for (int ch = 0; ch < 3; ++ch) {
      m->runningLineTexts[ch].clear();
      m->sequenceEditorStates[ch].text = ch == leader ? "100hz@2" : "1hz@100\n2hz@100";
      m->commitLineForChannel(ch, 0, true);
      m->sequencers[ch].running = true;
    }
    m->syncSource = leader;
    if (seed) m->inputs[B::SEED_CV_INPUT].channels = 1;
    return m;
  };
  rack::engine::Module::ProcessArgs args;
  args.sampleRate = 48000.f;
  args.sampleTime = 1.f / args.sampleRate;
  for (int leader : {0, 2}) {
    for (bool seed : {false, true}) {
      auto m = make(leader, seed);
      int follower = leader == 0 ? 2 : 0;
      int stopped = 1;
      m->sequencers[stopped].running = false;
      m->sequencers[stopped].clockPhase = .37f;
      m->sequenceEditorStates[follower].text = "3hz@100\n2hz@100";
      for (int sample = 0; sample < 4800; ++sample) m->process(args);
      require(m->sequencers[follower].clockPhase < .025f,
              "Leader EOL restarts follower lines regardless of channel order and Seed CV");
      require(!m->sequencers[stopped].running && m->sequencers[stopped].clockPhase == .37f,
              "Sync does not restart or reset stopped channels");
      require(m->sequenceEditorStates[follower].text == "3hz@100\n2hz@100" &&
                  m->runningLineTexts[follower][0] == "1hz@100",
              "Sync preserves unsubmitted Draft and running Now");
      m->syncSource = -1;
      for (int sample = 0; sample < 4800; ++sample) m->process(args);
      require(m->sequencers[follower].clockPhase > .09f,
              "Free lets channels continue independently");
      m->syncSource = leader;
      m->syncEvent = 2;
      m->syncRestart = 2;
      json_t* saved = m->dataToJson();
      auto loaded = std::unique_ptr<B>(static_cast<B*>(model->createModule()));
      loaded->dataFromJson(saved);
      json_decref(saved);
      require(loaded->syncSource == leader && loaded->syncEvent == 2 && loaded->syncRestart == 2,
              "Channel sync settings survive patch reload");
    }
  }
  for (int event = 0; event < 3; ++event) {
    for (int restart = 0; restart < 3; ++restart) {
      auto m = make(2, false);
      m->sequenceEditorStates[2].text = "100hz 100hz\n100hz 100hz";
      m->commitLineForChannel(2, 0, true);
      m->sequenceEditorStates[0].text = "1hz 2hz\n3hz 4hz";
      m->commitLineForChannel(0, 1, true);
      blunchProcessingChannel = 0;
      m->sequencers[0].activeProgramIndex = 1;
      m->applyActiveProgramStep(m->sequencers[0]);
      blunchProcessingChannel = -1;
      m->syncEvent = event;
      m->syncRestart = restart;
      bool fired = false;
      for (int sample = 0; sample < 4800 && !fired; ++sample) {
        m->process(args);
        fired = (m->boundaryEvents[2] & (1u << event)) != 0;
      }
      require(fired, "Each source boundary kind is available to sync");
      require(m->sequencers[0].activeLine == (restart == 2 ? 0 : 1) &&
                  m->sequencers[0].activeProgramIndex == (restart == 0 ? 1 : 0),
              "Block, line, and page sync restart the requested follower scope");
    }
  }
  for (bool missing : {false, true}) {
    auto m = make(2, false);
    if (missing) m->syncSource = 15;
    else m->sequencers[2].running = false;
    for (int sample = 0; sample < 4800; ++sample) m->process(args);
    require(m->sequencers[0].clockPhase > .09f,
            "Stopped or unavailable source leaves followers running freely");
  }
  auto free = std::unique_ptr<B>(static_cast<B*>(model->createModule()));
  require(free->syncSource == -1 && free->syncLabel() == "Free", "Sync defaults to Free");
  std::puts("Blunch channel sync, channel order, Seed CV, stopped channels, Draft, and persistence checks passed.");
}
