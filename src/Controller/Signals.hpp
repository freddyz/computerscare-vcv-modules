#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace computerscare {
namespace controller {
constexpr int buttons = 21, analogs = 18;
enum Profile { Unknown = 0, DualSense = 1, Xbox = 2, DualShock4 = 3 };
// Bank channel order is also the compact panel's button order.
inline int buttonBankControl(int bank, int channel) {
  static const int first[] = {0, 1,  2,  3, 4,  5,  6,  7,
                              8, 19, 10, 9, 20, 11, 17, 18};
  static const int second[] = {15, 16, 12, 13, 14};
  return bank == 0 ? first[channel] : second[channel];
}
inline const char* buttonName(int i, Profile p) {
  static const char* sony[] = {
      "Cross",   "Circle",  "Square", "Triangle",    "Up",   "Down",
      "Left",    "Right",   "L1",     "R1",          "L3",   "R3",
      "Create",  "Options", "PS",     "Touch click", "Mute", "Touch 1",
      "Touch 2", "L2 gate", "R2 gate"};
  static const char* xbox[] = {
      "A",       "B",       "X",      "Y",           "Up",    "Down",
      "Left",    "Right",   "LB",     "RB",          "LS",    "RS",
      "View",    "Menu",    "Xbox",   "Touch click", "Share", "Touch 1",
      "Touch 2", "LT gate", "RT gate"};
  if (p == DualShock4 && i == 12) return "Share";
  return (p == Xbox ? xbox : sony)[i];
}
inline const char* analogName(int i, Profile p) {
  static const char* names[] = {
      "Left X",    "Left Y",    "Right X",   "Right Y",   "L2",
      "R2",        "Battery",   "Connected", "Touch 1 X", "Touch 1 Y",
      "Touch 2 X", "Touch 2 Y", "Gyro X",    "Gyro Y",    "Gyro Z",
      "Accel X",   "Accel Y",   "Accel Z"};
  if (p == Xbox && i == 4) return "LT";
  if (p == Xbox && i == 5) return "RT";
  return names[i];
}
inline float clamp(float x, float low, float high) {
  return std::max(low, std::min(high, x));
}
inline void deadzone(float& x, float& y, float amount) {
  float r = std::sqrt(x * x + y * y);
  amount = clamp(amount, 0, .95f);
  float scale =
      r > amount ? std::min(1.f, (r - amount) / (1 - amount)) / r : 0.f;
  x *= scale;
  y *= scale;
}
inline uint16_t rumbleLevel(float voltage) {
  return std::isfinite(voltage) ? uint16_t(clamp(voltage, 0, 10) * 6553.5f) : 0;
}
struct ButtonSignal {
  uint32_t seen = 0;
  int mode = -1;
  bool primed = false, latched = false;
  float remaining = 0;
  void reset() {
    primed = false;
    latched = false;
    remaining = 0;
    mode = -1;
  }
  float process(bool connected, bool down, uint32_t presses, int nextMode,
                float dt, float pulse) {
    if (!connected) {
      reset();
      return 0;
    }
    if (!primed || mode != nextMode) {
      seen = presses;
      primed = true;
      latched = false;
      remaining = 0;
      mode = nextMode;
    }
    uint32_t edges = presses - seen;
    seen = presses;
    if (edges) {
      if (edges & 1u) latched = !latched;
      remaining = std::max(.001f, pulse);
    }
    bool high = nextMode == 0 ? down : nextMode == 1 ? remaining > 0 : latched;
    remaining = std::max(0.f, remaining - dt);
    return high ? 10.f : 0.f;
  }
};
}  // namespace controller
}  // namespace computerscare
