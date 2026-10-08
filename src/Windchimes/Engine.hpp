#pragma once
#include "Motion.hpp"
#include "Propagation.hpp"
#include "Resonator.hpp"
#include "Reverb.hpp"
#include "Tuning.hpp"
#include "Wind.hpp"
namespace windchimes {
class Engine {
  struct Set {
    SetConfig config;
    int stopSamples = 0, stopLength = 1;
    bool paused = false;
    Motion motion;
    Propagation propagation;
    float distanceState = 0.f, distanceState2 = 0.f, distanceCoeff = 1.f;
    float roomLeft = 0.f, roomRight = 0.f;
    std::array<Resonator, maxTubes> tubes;
    std::array<float, 4> targetGain{}, currentGain{};
    float directGain = 0.f, audibility = 1.f;
    float configuredRate = 0.f, transpose = 0.f;
  };
  std::array<Set, maxSets> sets;
  Wind wind;
  Reverb reverb;
  std::unique_ptr<Reverb> windReverb{new Reverb};
  Quad windBypass;
  float wet = 0.f, smoothWet = 0.f, smoothWindMix = 0.f;
  Random strikes{0x985fe};
  float rate = 48000.f, dt = 0.f;
  int counter = 0, interval = 120;
  unsigned outputMask = 3u;
  unsigned audibleMask = (1u << maxSets) - 1u;
  bool excludeWind = false;
  float dedicatedWind = 0.f;
  std::array<std::array<float, 4>, 4> roomDecode{};
  std::array<std::array<float, 4>, 4> currentRoomDecode{};
  void updateRouting() {
    for (auto& s : sets)
      s.targetGain = spatialGains(s.config.x, s.config.y, outputMask);
    const float px[4] = {0.f, 1.f, 0.f, 1.f};
    const float py[4] = {0.f, 0.f, 1.f, 1.f};
    for (int src = 0; src < 4; ++src)
      roomDecode[src] = spatialGains(px[src], py[src], outputMask);
    // Each return's decoder column has unit power, preserving diffuse
    // room energy across mono, stereo and quad rather than losing channels.
  }

  static bool sameConfig(const SetConfig& a, const SetConfig& b) {
    return a.enabled == b.enabled && a.tubes == b.tubes &&
           a.material == b.material && a.scale == b.scale && a.root == b.root &&
           a.divisions == b.divisions && a.spread == b.spread &&
           a.octave == b.octave && a.fine == b.fine && a.decay == b.decay &&
           a.brightness == b.brightness && a.hardness == b.hardness &&
           a.strikerWeight == b.strikerWeight && a.sailSize == b.sailSize &&
           a.level == b.level && a.swing == b.swing && a.x == b.x &&
           a.y == b.y && a.shape == b.shape && a.body == b.body &&
           a.inharmonicity == b.inharmonicity;
  }

 public:
  Engine() { updateRouting(); }
  void setOutputMask(unsigned mask) {
    mask &= 15u;
    if (mask == outputMask) return;
    unsigned newlyConnected = mask & ~outputMask;
    for (int ch = 0; ch < 4; ++ch)
      if (newlyConnected & (1u << ch)) {
        for (int src = 0; src < 4; ++src) currentRoomDecode[src][ch] = 0.f;
        for (auto& s : sets) s.currentGain[ch] = 0.f;
      }
    outputMask = mask;
    updateRouting();
  }
  void setSampleRate(float sampleRate) {
    rate = std::max(sampleRate, 8000.f);
    interval = std::max(1, static_cast<int>(rate / 400.f));
    dt = interval / rate;
    counter = 0;
    for (auto& s : sets) s.propagation.reset();
    reverb.configure(rate, 0.5f);
    windReverb->configure(rate, .5f);
  }
  void setAudibleMask(unsigned mask) {
    audibleMask = mask & ((1u << maxSets) - 1u);
  }
  void configureWindRouting(bool excluded, bool dedicatedConnected) {
    excludeWind = excluded;
    wind.setDedicatedConnected(dedicatedConnected);
  }
  float windAudio() const { return dedicatedWind; }
  const Quad& windWithoutDelay() const { return windBypass; }
  void configureEffects(float mix, float size, float tone, float texture,
                        float presence = .5f) {
    wet = clamp(mix, 0.f, 1.f);
    reverb.configure(rate, size);
    windReverb->configure(rate, size);
    wind.configure(rate, tone, texture, presence);
  }
  void configure(int index, const SetConfig& config, float transpose) {
    auto& s = sets[index];
    if (s.configuredRate == rate && s.transpose == transpose &&
        sameConfig(s.config, config))
      return;
    s.configuredRate = rate;
    s.transpose = transpose;
    if (!config.enabled) {
      if (s.config.enabled) {
        for (auto& t : s.tubes) t.reset();
        s.motion.reset();
        s.propagation.reset();
        s.distanceState = s.distanceState2 = 0.f;
        s.stopSamples = 0;
        s.paused = false;
      }
      s.config = config;
      return;
    }
    if (!s.config.enabled) {
      s.motion.reset();
      s.paused = false;
      s.stopSamples = 0;
    }
    s.motion.setWeight(config.strikerWeight);
    s.motion.setSailSize(config.sailSize);
    s.motion.configure(config.tubes);
    for (int t = 0; t < config.tubes; ++t)
      s.tubes[t].configure(tubeFrequency(config, t, transpose), rate, config);
    float pan = clamp(config.x, 0.f, 1.f);
    float near = 1.f - stageDistance(config.x, config.y);
    s.propagation.configure(rate, 1.f - near);
    // Every tube in a set shares its scene position and output gains.
    float gain = config.level * 0.48f / (1.f + 7.f * (1.f - near));
    float roomGain = config.level * 0.48f * (0.35f + 0.65f * near);
    float left = std::cos(pan * pi * .5f), right = std::sin(pan * pi * .5f);
    s.roomLeft = left * roomGain;
    s.roomRight = right * roomGain;
    s.directGain = gain;
    s.targetGain = spatialGains(config.x, config.y, outputMask);
    for (int t = config.tubes; t < maxTubes; ++t) s.tubes[t].reset();
    s.distanceCoeff =
        1.f -
        std::exp(-2.f * pi *
                 std::min(800.f * std::pow(22.5f, near), rate * 0.4f) / rate);
    s.config = config;
  }
  void gust() {
    wind.gust();
    for (auto& s : sets)
      if (s.config.enabled && !s.paused)
        s.motion.kick(strikes.uniform() * 2.f * pi);
  }
  bool stopped(int index) const { return sets[index].paused; }
  void stop(int index) {
    auto& s = sets[index];
    if (!s.config.enabled) return;
    s.paused = true;
    s.motion.reset();
    s.motion.configure(s.config.tubes);
    // Fade this set's existing sound before clearing its voices. Room tails
    // belong to the shared scene and are allowed to decay normally.
    s.stopLength = std::max(1, static_cast<int>(rate * 0.005f));
    s.stopSamples = s.stopLength;
  }
  void strike(int index) {
    auto& s = sets[index];
    if (!s.config.enabled) return;
    s.paused = false;
    s.stopSamples = 0;
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
  Point sailAxis(int i) const { return sets[i].motion.sailAxis(); }
  float strikerFlash(int i) const { return sets[i].motion.strikerFlash(); }
  float tubeX(int i, int tube) const { return sets[i].motion.tubeX(tube); }
  float tubeY(int i, int tube) const { return sets[i].motion.tubeY(tube); }
  float motionX(int i) const { return sets[i].motion.x; }
  float motionY(int i) const { return sets[i].motion.y; }
  float windStrength() const { return wind.strength; }
  Stereo windFlow(float x, float y) const { return wind.flowAt(x, y, 0); }
  Quad processQuad(float amount, float gustiness, float turbulence,
                   float windMix, bool separateWind = false) {
    if (++counter >= interval) {
      counter = 0;
      wind.step(dt, amount, gustiness, turbulence);
      for (int i = 0; i < maxSets; ++i) {
        auto& s = sets[i];
        if (!s.config.enabled || s.paused) continue;
        Stereo air = wind.flowAt(s.config.x, s.config.y, i);
        s.motion.step(
            dt, air.left, air.right, s.config, i,
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
    Quad windDirect = wind.processQuad(rate, excludeWind ? 0u : outputMask);
    Quad quad;
    windBypass = {};
    dedicatedWind = wind.dedicatedAudio();
    float smoothing = std::min(1.f, 100.f / rate);
    smoothWindMix +=
        ((excludeWind ? 0.f : windMix) - smoothWindMix) * smoothing;
    smoothWet += (wet - smoothWet) * smoothing;
    for (float& channel : windDirect.channel) channel *= smoothWindMix;
    Stereo windSend(
        (windDirect.channel[0] + windDirect.channel[2]) * .70710678f,
        (windDirect.channel[1] + windDirect.channel[3]) * .70710678f);
    Stereo roomSend;
    for (int source = 0; source < maxSets; ++source) {
      auto& s = sets[source];
      s.audibility +=
          ((audibleMask & (1u << source) ? 1.f : 0.f) - s.audibility) *
          smoothing;
      if (s.audibility < 1e-6f) s.audibility = 0.f;
      if (!s.config.enabled || (s.paused && s.stopSamples == 0)) continue;
      float mono = 0.f;
      for (int t = 0; t < s.config.tubes; ++t) mono += s.tubes[t].process();
      mono = s.propagation.process(mono);
      s.distanceState += (mono - s.distanceState) * s.distanceCoeff;
      if (std::fabs(s.distanceState) < 1e-12f) s.distanceState = 0.f;
      s.distanceState2 +=
          (s.distanceState - s.distanceState2) * s.distanceCoeff;
      if (std::fabs(s.distanceState2) < 1e-12f) s.distanceState2 = 0.f;
      float fade = s.stopSamples > 0
                       ? s.stopSamples / static_cast<float>(s.stopLength)
                       : 1.f;
      float direct = s.distanceState2 * s.directGain * fade * s.audibility;
      for (int ch = 0; ch < 4; ++ch) {
        s.currentGain[ch] += (s.targetGain[ch] - s.currentGain[ch]) * smoothing;
        if (outputMask & (1u << ch))
          quad.channel[ch] += direct * s.currentGain[ch];
      }
      roomSend.left += s.distanceState * s.roomLeft * fade * s.audibility;
      roomSend.right += s.distanceState * s.roomRight * fade * s.audibility;
      if (s.stopSamples > 0 && --s.stopSamples == 0) {
        for (auto& tube : s.tubes) tube.reset();
        s.distanceState = s.distanceState2 = 0.f;
        s.propagation.reset();
      }
    }
    // A single eight-line room supports every layout. No duplicated voices
    // or reverbs; only connected output rows are accumulated.
    Quad reflections = reverb.processWetQuad(roomSend);
    Quad windReflections = windReverb->processWetQuad(windSend);
    for (int ch = 0; ch < 4; ++ch)
      if (outputMask & (1u << ch)) {
        float room = 0.f, windRoom = 0.f;
        for (int src = 0; src < 4; ++src) {
          currentRoomDecode[src][ch] +=
              (roomDecode[src][ch] - currentRoomDecode[src][ch]) * smoothing;
          room += reflections.channel[src] * currentRoomDecode[src][ch];
          windRoom += windReflections.channel[src] * currentRoomDecode[src][ch];
        }
        quad.channel[ch] =
            quad.channel[ch] * (1.f - smoothWet) + room * smoothWet;
        windBypass.channel[ch] =
            windDirect.channel[ch] * (1.f - smoothWet) + windRoom * smoothWet;
        if (!separateWind) quad.channel[ch] += windBypass.channel[ch];
      }
    return quad;
  }
  Stereo process(float amount, float gustiness, float turbulence,
                 float windMix) {
    Quad out = processQuad(amount, gustiness, turbulence, windMix);
    return {out.channel[0], out.channel[1]};
  }
};
}  // namespace windchimes
