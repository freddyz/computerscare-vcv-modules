#pragma once
#include "../components/TerminalSettings.hpp"
#include "BlunchHelp.hpp"

// Included after the module definition: both interfaces use these bindings.
inline std::vector<computerscare::TerminalSetting> blunchSettings(
    ComputerscareBlunch* m) {
  using Entry = computerscare::TerminalSetting;
  auto choice = [](const std::string& name, const std::string& help,
                   std::function<bool()> checked, std::function<void()> set) {
    Entry e;
    e.name = name;
    e.help = help;
    e.checked = checked;
    e.action = set;
    return e;
  };
  auto paramChoice = [=](const char* title, int param, const std::string* names,
                         const std::string* descriptions, int count,
                         bool advance) {
    Entry group;
    group.name = title;
    const std::string category = title;
    group.help = category == "Line advance"    ? "When to change lines."
                 : category == "Block advance" ? "When to change blocks."
                 : category == "Wait"       ? "When staged edits take effect."
                 : category == "Line scan"  ? "Select lines using CV."
                 : category == "Block scan" ? "Select blocks using CV."
                                            : "";
    group.value = [=]() {
      BlunchStateLock lock(m);
      return names[rack::math::clamp(int(m->params[param].getValue()), 0,
                                     count - 1)];
    };
    for (int i = 0; i < count; ++i) {
      int mode = advance ? (i + 1) % count : i;
      group.children.push_back(choice(
          names[mode], descriptions[mode],
          [=]() {
            BlunchStateLock lock(m);
            return int(m->params[param].getValue()) == mode;
          },
          [=]() {
            BlunchStateLock lock(m);
            m->params[param].setValue(mode);
          }));
    }
    return group;
  };
  Entry playback;
  playback.name = "Playback";
  playback.help = "Timing and transport.";
  playback.children.push_back(paramChoice(
      "Line advance", ComputerscareBlunch::AUTO_ADVANCE_PARAM,
      BLUNCH_ADVANCE_MODE_NAMES, BLUNCH_LINE_ADVANCE_MODE_DESCRIPTIONS,
      BLUNCH_ADVANCE_MODE_COUNT, true));
  playback.children.push_back(paramChoice(
      "Block advance", ComputerscareBlunch::AUTO_BLOCK_ADVANCE_PARAM,
      BLUNCH_ADVANCE_MODE_NAMES, BLUNCH_BLOCK_ADVANCE_MODE_DESCRIPTIONS,
      BLUNCH_ADVANCE_MODE_COUNT, true));
  playback.children.push_back(paramChoice(
      "Wait", ComputerscareBlunch::WAIT_MODE_PARAM, BLUNCH_WAIT_MODE_NAMES,
      BLUNCH_WAIT_MODE_DESCRIPTIONS, BLUNCH_WAIT_MODE_COUNT, false));
  Entry run;
  run.name = "Run input";
  run.help = "Gate or toggle playback.";
  run.value = [=]() {
    BlunchStateLock lock(m);
    return BLUNCH_RUN_INPUT_MODE_NAMES[rack::math::clamp(m->runInputMode, 0,
                                                         1)];
  };
  for (int i = 0; i < 2; ++i)
    run.children.push_back(choice(
        BLUNCH_RUN_INPUT_MODE_NAMES[i],
        i ? "A trigger toggles playback." : "A high gate enables playback.",
        [=]() {
          BlunchStateLock lock(m);
          return m->runInputMode == i;
        },
        [=]() {
          BlunchStateLock lock(m);
          m->runInputMode = i;
        }));
  playback.children.push_back(run);
  playback.children.push_back(choice(
      "Auto-stage valid edits",
      "Stage valid edits for each line’s next visit; playback is "
      "uninterrupted.",
      [=]() {
        BlunchStateLock lock(m);
        return m->params[ComputerscareBlunch::LIVE_APPLY_PARAM].getValue() >
               .5f;
      },
      [=]() {
        BlunchStateLock lock(m);
        auto& p = m->params[ComputerscareBlunch::LIVE_APPLY_PARAM];
        p.setValue(p.getValue() > .5f ? 0.f : 1.f);
      }));
  playback.children.back().shortName = "Auto-stage edits";
  Entry sync;
  sync.name = "Channel sync";
  sync.help = "Keep channels together.";
  sync.value = [=]() {
    BlunchStateLock lock(m);
    return m->syncLabel();
  };
  sync.children.push_back(choice(
      "Free", "Channels run independently.",
      [=]() {
        BlunchStateLock lock(m);
        return m->syncSource < 0;
      },
      [=]() {
        BlunchStateLock lock(m);
        m->syncSource = -1;
      }));
  Entry source;
  source.name = "Source channel";
  source.help = "Choose the leading channel.";
  source.value = [=]() {
    BlunchStateLock lock(m);
    return m->syncSource < 0 ? std::string("Free")
                             : std::to_string(m->syncSource + 1);
  };
  for (int i = 0; i < 16; ++i)
    source.children.push_back(choice(
        "Channel " + std::to_string(i + 1),
        "Source boundaries restart other playing channels.",
        [=]() {
          BlunchStateLock lock(m);
          return m->syncSource == i;
        },
        [=]() {
          BlunchStateLock lock(m);
          m->syncSource = i;
        }));
  sync.children.push_back(source);
  const char* events[] = {"End of Block", "End of Line", "End of Page"};
  const char* restarts[] = {"Current block", "Current line", "Page beginning"};
  Entry boundary;
  boundary.name = "Source boundary";
  boundary.help = "Choose the sync event.";
  Entry restart;
  restart.name = "Restart followers at";
  restart.help = "Where followers restart.";
  for (int i = 0; i < 3; ++i) {
    std::string event = events[i], location = restarts[i];
    boundary.children.push_back(choice(
        event, "Restart followers at this source boundary.",
        [=]() {
          BlunchStateLock lock(m);
          return m->syncEvent == i;
        },
        [=]() {
          BlunchStateLock lock(m);
          m->syncEvent = i;
        }));
    restart.children.push_back(choice(
        location, "Stopped channels stay stopped.",
        [=]() {
          BlunchStateLock lock(m);
          return m->syncRestart == i;
        },
        [=]() {
          BlunchStateLock lock(m);
          m->syncRestart = i;
        }));
  }
  boundary.value = [=]() {
    BlunchStateLock lock(m);
    const char* n[] = {"End of Block", "End of Line", "End of Page"};
    return std::string(n[rack::math::clamp(m->syncEvent, 0, 2)]);
  };
  restart.value = [=]() {
    BlunchStateLock lock(m);
    const char* n[] = {"Current block", "Current line", "Page beginning"};
    return std::string(n[rack::math::clamp(m->syncRestart, 0, 2)]);
  };
  sync.children.push_back(boundary);
  sync.children.push_back(restart);
  Entry units;
  units.name = "Default number unit";
  units.help = "Units for bare numbers.";
  units.value = [=]() {
    BlunchStateLock lock(m);
    return std::string(blunch::language::clockUnitName(m->defaultNumberUnit));
  };
  for (const char* name : {"bpm", "s", "ms", "hz", "min", "mhz", "khz"}) {
    auto unit = blunch::language::parseClockUnit(name);
    units.children.push_back(choice(
        name, "Used for numbers with no written unit.",
        [=]() {
          BlunchStateLock lock(m);
          return m->defaultNumberUnit == unit;
        },
        [=]() {
          BlunchStateLock lock(m);
          m->defaultNumberUnit = unit;
        }));
  }
  Entry stage;
  stage.name = "Stage page";
  stage.help = "Submit edits using the current Wait setting.";
  stage.action = [=]() {
    BlunchStateLock lock(m);
    ++m->pageSubmitCount;
  };
  static const std::string scanNames[] = {"Off", "W", "X", "Y", "Z"};
  static const std::string scanHelp[] = {
      "Advance sequentially.", "Sample W at each advance boundary (0–10 V).",
      "Sample X at each advance boundary (0–10 V).",
      "Sample Y at each advance boundary (0–10 V).",
      "Sample Z at each advance boundary (0–10 V)."};
  playback.children.push_back(paramChoice("Line scan",
                                          ComputerscareBlunch::LINE_SCAN_PARAM,
                                          scanNames, scanHelp, 5, false));
  playback.children.push_back(paramChoice("Block scan",
                                          ComputerscareBlunch::BLOCK_SCAN_PARAM,
                                          scanNames, scanHelp, 5, false));
  Entry bake;
  bake.name = "Bake Time into formulas";
  bake.help = "Remap timing, then reset Time. Ignores CV.";
  bake.action = [=]() {
    BlunchStateLock lock(m);
    if (m->params[ComputerscareBlunch::SPEED_OF_TIME_PARAM].getValue() == 0.f)
      return;
    auto* change = new rack::history::ModuleChange;
    change->name = "Bake Time into formulas";
    change->moduleId = m->id;
    change->oldModuleJ = m->toJson();
    try {
      m->bakeTime();
      change->newModuleJ = m->toJson();
      APP->history->push(change);
    } catch (const std::exception& error) {
      delete change;
      osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, error.what());
    }
  };
  playback.children.push_back(bake);
  return {playback, sync, units, stage, blunchHelp()};
}
