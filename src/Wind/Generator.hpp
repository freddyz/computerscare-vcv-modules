#pragma once
#include "Wind.hpp"
namespace computerscare {
namespace wind {
// Standalone clocking for the same field/sound used by the chime engine.
class Generator {
  Wind weather;
  float rate = 0.f, dt = 0.f;
  int counter = 0, interval = 120;

 public:
  void configure(float sampleRate, float tone, float texture, float presence) {
    if (rate != sampleRate) {
      rate = sampleRate;
      interval = std::max(1, int(rate / 400.f));
      dt = interval / rate;
      counter = 0;
    }
    weather.configure(rate, tone, texture, presence);
  }
  void gust() { weather.gust(); }
  Quad process(float strength, float gustiness, float turbulence,
               unsigned mask) {
    if (++counter >= interval) {
      counter = 0;
      weather.step(dt, strength, gustiness, turbulence);
    }
    return weather.processQuad(rate, mask);
  }
  float strength() const { return clamp(weather.strength * 10.f, 0.f, 10.f); }
};
}  // namespace wind
}  // namespace computerscare
