#pragma once
#include <algorithm>
#include <cmath>
namespace phlooper {
// One shared clock grid, with independently latched per-voice write gates.
struct RecordClock {
  static int factor(int setting) {
    constexpr int values[] = {1, 2, 3, 4, 6, 8, 12, 16};
    return values[std::min(7, std::abs(setting))];
  }
  unsigned record = 0, erase = 0;
  double now = 0, last = -1, period = 0, next = 0;
  int division = 0, subdivision = 1, previousSetting = 0;
  bool high = false, connected = false;
  bool process(bool patched, float voltage, double dt, int setting,
               unsigned wantedRecord, unsigned wantedErase) {
    now += dt;
    if (!patched) {
      *this = {};
      record = wantedRecord;
      erase = wantedErase;
      return false;
    }
    if (!connected) {
      connected = true;
      high = voltage >= 1.f;
      last = -1;
      period = 0;
      division = 0;
    }
    if (setting != previousSetting) {
      division = 0;
      subdivision = 1;
    }
    previousSetting = setting;
    bool edge = !high && voltage >= 1.f;
    if (voltage <= .1f)
      high = false;
    else if (edge)
      high = true;
    bool tick = false;
    int amount = factor(setting);
    if (edge) {
      if (last >= 0) period = now - last;
      last = now;
      subdivision = 1;
      next = now + period / amount;
      tick = setting >= 0 || division == 0;
      division = (division + 1) % amount;
    } else if (setting > 0 && period > 0 && subdivision < amount &&
               now >= next) {
      tick = true;
      ++subdivision;
      next = last + period * subdivision / amount;
    }
    if (tick) {
      record = wantedRecord;
      erase = wantedErase;
    }
    return tick;
  }
};
}  // namespace phlooper
