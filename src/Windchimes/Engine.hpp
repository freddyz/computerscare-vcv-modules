#pragma once
#include "Motion.hpp"
#include "Resonator.hpp"
#include "Reverb.hpp"
#include "Tuning.hpp"
#include "Wind.hpp"
namespace windchimes {
class Engine {
  struct Set {
    SetConfig config;
    Motion motion;
    std::array<Resonator, maxTubes> tubes;
    std::array<float, maxTubes> panLeft{}, panRight{};
  };
  std::array<Set, maxSets> sets;
  Wind wind;
  Reverb reverb;
  float wet = 0.f, smoothWet = 0.f, smoothWindMix = 0.f;
  Random strikes{0x985fe};
  float rate = 48000.f, dt = 0.f;
  int counter = 0, interval = 120;

 public:
  void setSampleRate(float sampleRate) {
    rate = std::max(sampleRate, 8000.f);
    interval = std::max(1, static_cast<int>(rate / 400.f));
    dt = interval / rate;
    counter = 0;
    reverb.configure(rate, 0.5f);
  }
  void configureEffects(float mix, float size, float tone, float texture) {
    wet = clamp(mix, 0.f, 1.f);
    reverb.configure(rate, size);
    wind.configure(rate, tone, texture);
  }
  void configure(int index, const SetConfig& config, float transpose) {
    auto& s = sets[index];
    if (!config.enabled) {
      if (s.config.enabled) {
        for (auto& t : s.tubes) t.reset();
        s.motion.reset();
      }
      s.config = config;
      return;
    }
    if (!s.config.enabled) s.motion.reset();
    s.motion.configure(config.tubes);
    for (int t = 0; t < config.tubes; ++t) {
      s.tubes[t].configure(tubeFrequency(config, t, transpose), rate, config);
      float pan = clamp(config.x, 0.f, 1.f);
      float gain = config.level * (0.45f + config.y * 0.55f) * 0.48f;
      s.panLeft[t] = std::cos(pan * pi * 0.5f) * gain;
      s.panRight[t] = std::sin(pan * pi * 0.5f) * gain;
    }
    for (int t = config.tubes; t < maxTubes; ++t) s.tubes[t].reset();
    s.config = config;
  }
  void gust() {
    wind.gust();
    for (auto& s : sets)
      if (s.config.enabled) s.motion.kick(strikes.uniform() * 2.f * pi);
  }
  void strike(int index) {
    auto& s = sets[index];
    if (!s.config.enabled) return;
    int tube = std::min(static_cast<int>(strikes.uniform() * s.config.tubes),
                        s.config.tubes - 1);
    s.motion.strike(tube);
  }
  float tubeAngle(int i, int tube) const {
    return sets[i].motion.tubeAngle(tube);
  }
  float tubeFlash(int i, int tube) const {
    return sets[i].motion.tubeFlash(tube);
  }
  float strikerAngle(int i) const { return sets[i].motion.strikerAngle(); }
  float tubeDepth(int i, int t) const { return sets[i].motion.tubeDepth(t); }
  float strikerDepth(int i) const { return sets[i].motion.strikerDepth(); }
  float strikerFlash(int i) const { return sets[i].motion.strikerFlash(); }
  float tubeX(int i, int tube) const { return sets[i].motion.tubeX(tube); }
  float tubeY(int i, int tube) const { return sets[i].motion.tubeY(tube); }
  float motionX(int i) const { return sets[i].motion.x; }
  float motionY(int i) const { return sets[i].motion.y; }
  float windStrength() const { return wind.strength; }
  Stereo process(float amount, float gustiness, float turbulence,
                 float windMix) {
    if (++counter >= interval) {
      counter = 0;
      wind.step(dt, amount, gustiness, turbulence);
      for (int i = 0; i < maxSets; ++i) {
        auto& s = sets[i];
        if (!s.config.enabled) continue;
        s.motion.step(
            dt, wind.x, wind.y, s.config, i,
            [&s](int t, float velocity) { s.tubes[t].strike(velocity); },
            [&s](int a, int b, float velocity) {
              s.tubes[a].strike(velocity);
              s.tubes[b].strike(velocity);
            });
      }
    }
    Stereo out = wind.process(rate);
    float smoothing = std::min(1.f, 100.f / rate);
    smoothWindMix += (windMix - smoothWindMix) * smoothing;
    smoothWet += (wet - smoothWet) * smoothing;
    out.left *= smoothWindMix;
    out.right *= smoothWindMix;
    for (auto& s : sets) {
      if (!s.config.enabled) continue;
      for (int t = 0; t < s.config.tubes; ++t) {
        float audio = s.tubes[t].process();
        out.left += audio * s.panLeft[t];
        out.right += audio * s.panRight[t];
      }
    }
    return reverb.process(out, smoothWet);
  }
};
}  // namespace windchimes
