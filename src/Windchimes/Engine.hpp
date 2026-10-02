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
    float distanceState = 0.f, distanceState2 = 0.f, distanceCoeff = 1.f;
    float roomLeft = 0.f, roomRight = 0.f;
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
        s.distanceState = s.distanceState2 = 0.f;
      }
      s.config = config;
      return;
    }
    if (!s.config.enabled) s.motion.reset();
    s.motion.configure(config.tubes);
    for (int t = 0; t < config.tubes; ++t) {
      s.tubes[t].configure(tubeFrequency(config, t, transpose), rate, config);
      float pan = clamp(config.x, 0.f, 1.f);
      float near = clamp(config.y, 0.f, 1.f);
      // Direct sound falls faster with distance than the diffuse room send.
      float gain = config.level * 0.48f / (1.f + 7.f * (1.f - near));
      float roomGain = config.level * 0.48f * (0.35f + 0.65f * near);
      s.roomLeft = std::cos(pan * pi * 0.5f) * roomGain;
      s.roomRight = std::sin(pan * pi * 0.5f) * roomGain;
      s.panLeft[t] = std::cos(pan * pi * 0.5f) * gain;
      s.panRight[t] = std::sin(pan * pi * 0.5f) * gain;
    }
    for (int t = config.tubes; t < maxTubes; ++t) s.tubes[t].reset();
    s.distanceCoeff =
        1.f -
        std::exp(-2.f * pi *
                 std::min(800.f * std::pow(22.5f, clamp(config.y, 0.f, 1.f)),
                          rate * 0.4f) /
                 rate);
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
            [&s](int t, float velocity) {
              auto shape = s.motion.tubeShape(t);
              auto contact = closestPoint(s.motion.strikerPosition(),
                                          shape.top(), shape.bottom());
              float location = clamp((contact - shape.top()).dot(shape.axis) /
                                         (2.f * shape.halfLength),
                                     0.f, 1.f);
              s.tubes[t].strike(velocity, ContactKind::Striker, location);
            },
            [&s](int a, int b, float velocity) {
              auto ca = s.motion.tubeShape(a), cb = s.motion.tubeShape(b);
              auto contact = capsuleContact(ca, cb);
              float first = clamp(
                  (contact.a - ca.top()).dot(ca.axis) / (2.f * ca.halfLength),
                  0.f, 1.f);
              float second = clamp(
                  (contact.b - cb.top()).dot(cb.axis) / (2.f * cb.halfLength),
                  0.f, 1.f);
              s.tubes[a].strike(velocity, ContactKind::Tube, first);
              s.tubes[b].strike(velocity, ContactKind::Tube, second);
            });
      }
    }
    Stereo out = wind.process(rate);
    float smoothing = std::min(1.f, 100.f / rate);
    smoothWindMix += (windMix - smoothWindMix) * smoothing;
    smoothWet += (wet - smoothWet) * smoothing;
    out.left *= smoothWindMix;
    out.right *= smoothWindMix;
    Stereo roomSend = out;
    for (auto& s : sets) {
      if (!s.config.enabled) continue;
      float mono = 0.f;
      for (int t = 0; t < s.config.tubes; ++t) mono += s.tubes[t].process();
      s.distanceState += (mono - s.distanceState) * s.distanceCoeff;
      if (std::fabs(s.distanceState) < 1e-12f) s.distanceState = 0.f;
      s.distanceState2 +=
          (s.distanceState - s.distanceState2) * s.distanceCoeff;
      if (std::fabs(s.distanceState2) < 1e-12f) s.distanceState2 = 0.f;
      out.left += s.distanceState2 * s.panLeft[0];
      out.right += s.distanceState2 * s.panRight[0];
      roomSend.left += s.distanceState * s.roomLeft;
      roomSend.right += s.distanceState * s.roomRight;
    }
    Stereo reflections = reverb.processWet(roomSend);
    return {out.left * (1.f - smoothWet) + reflections.left * smoothWet,
            out.right * (1.f - smoothWet) + reflections.right * smoothWet};
  }
};
}  // namespace windchimes
