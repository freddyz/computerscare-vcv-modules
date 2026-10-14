#pragma once
#include "Types.hpp"
#include "WindSound.hpp"
namespace windchimes {
class Wind {
  WindSound sound, dedicatedSound;
  bool dedicatedConnected = false;
  float presence = .5f;
  Random weather{0x1948a3};
  float elapsed = 0.f, targetX = 0.f, targetY = 0.f;
  float breezeX = 0.f, breezeY = 0.f, gustEnergy = 0.f;
  float direction = 0.f, eddyX = 0.f, eddyY = 0.f;
  float flutter = 0.f, flutterTarget = 0.f;
  // Smooth spatial modes share weather between neighbors; the smaller local
  // component prevents overlapping identical sets from locking together.
  Random fieldWeather{0x53e17b};
  std::array<Stereo, 4> field{};
  std::array<Stereo, maxSets> local{};
  float fieldForce = 0.f, fieldTurbulence = 0.f;
  float rate = 0.f, tone = .45f, texture = .4f;

 public:
  float x = 0.f, y = 0.f, strength = 0.f;
  void configure(float sampleRate, float newTone, float newTexture,
                 float mix = .5f) {
    if (sampleRate == rate && newTone == tone && newTexture == texture &&
        mix == presence)
      return;
    rate = sampleRate;
    tone = clamp(newTone, 0.f, 1.f);
    texture = clamp(newTexture, 0.f, 1.f);
    presence = clamp(mix, 0.f, 1.f);
    sound.configure(rate, tone, texture, presence);
    dedicatedSound.configure(rate, tone, texture, 1.f);
  }
  void gust() {
    gustEnergy = 1.f;
    elapsed = 0.f;
  }
  void step(float dt, float amount, float gustiness, float turbulence) {
    elapsed -= dt;
    if (elapsed <= 0.f) {
      elapsed = 4.f + weather.uniform() * (10.f - gustiness * 6.f);
      direction += weather.bipolar() * (0.3f + turbulence * 1.2f);
      float angle = direction;
      float magnitude = 0.15f + weather.uniform() * (0.35f + gustiness * 0.8f);
      targetX = std::cos(angle) * magnitude;
      targetY = std::sin(angle) * magnitude;
      flutterTarget = weather.bipolar();
    }
    breezeX +=
        (targetX - breezeX) * std::min(dt * (0.65f + gustiness * 0.6f), 1.f);
    breezeY +=
        (targetY - breezeY) * std::min(dt * (0.65f + gustiness * 0.6f), 1.f);
    flutter += (flutterTarget - flutter) * std::min(dt * 6.f, 1.f);
    gustEnergy *= std::exp(-dt * 0.8f);
    // Correlated gusts retain energy near pendulum time scales without a clock
    // or phase-aware drive. Even a smooth breeze has some speed fluctuation.
    float eddyRate = 1.2f + turbulence * 4.f;
    float coefficient = 1.f - std::exp(-dt * eddyRate);
    float variance = std::sqrt(3.f * (2.f - coefficient) / coefficient);
    eddyX += (weather.bipolar() * variance - eddyX) * coefficient;
    eddyY += (weather.bipolar() * variance - eddyY) * coefficient;
    fieldForce = amount + gustEnergy;
    fieldTurbulence = clamp(turbulence, 0.f, 1.f);
    // Correlated random flow, never an oscillator or a phase-aware drive.
    auto drift = [this, dt](Stereo& state, float speed) {
      float k = 1.f - std::exp(-dt * speed);
      float variance = std::sqrt(3.f * (2.f - k) / k);
      state.left += (fieldWeather.bipolar() * variance - state.left) * k;
      state.right += (fieldWeather.bipolar() * variance - state.right) * k;
    };
    for (int mode = 0; mode < 4; ++mode)
      drift(field[mode], 0.3f + mode * 0.18f + fieldTurbulence * 1.4f);
    for (auto& state : local) drift(state, 0.45f + fieldTurbulence * 2.5f);
    float gustScale = 0.18f + gustiness * 0.4f;
    float force = amount + gustEnergy;
    float along = eddyX * gustScale;
    float across = eddyY * gustScale * (0.12f + turbulence * 0.75f);
    x = force *
        (breezeX + std::cos(direction) * along - std::sin(direction) * across);
    y = force *
        (breezeY + std::sin(direction) * along + std::cos(direction) * across);
    strength = clamp(std::sqrt(x * x + y * y), 0.f, 1.5f);
    if (rate > 0.f) {
      sound.step(dt, strength, x, y, fieldTurbulence);
      if (dedicatedConnected)
        dedicatedSound.step(dt, strength, x, y, fieldTurbulence);
    }
  }
  Stereo flowAt(float stageX, float stageY, int slot) const {
    float px = clamp(stageX, 0.f, 1.f) - 0.5f;
    float py = clamp(stageY, 0.f, 1.f) - 0.5f;
    // Stage position supplies a stable exposure/direction bias. All spatial
    // weights are continuous, including while dragging across the stage.
    float angle = 0.65f * px - 0.45f * py;
    float exposure = 1.f + 0.22f * px + 0.18f * py;
    float ca = std::cos(angle), sa = std::sin(angle);
    Stereo out((x * ca - y * sa) * exposure, (x * sa + y * ca) * exposure);
    Stereo eddy;
    for (int mode = 0; mode < 4; ++mode) {
      float weight = std::sin((mode + 1) * px * 2.4f +
                              (mode % 2 ? -1.f : 1.f) * py * 3.f + mode * 1.7f);
      eddy.left += field[mode].left * weight * 0.5f;
      eddy.right += field[mode].right * weight * 0.5f;
    }
    float shared = 0.09f + fieldTurbulence * 0.36f;
    float independent = 0.045f + fieldTurbulence * 0.22f;
    int index = std::max(0, std::min(slot, maxSets - 1));
    out.left +=
        fieldForce * (eddy.left * shared + local[index].left * independent);
    out.right +=
        fieldForce * (eddy.right * shared + local[index].right * independent);
    return out;
  }
  void setDedicatedConnected(bool connected) { dedicatedConnected = connected; }
  float dedicatedAudio() {
    return dedicatedConnected ? dedicatedSound.process(1u).channel[0] : 0.f;
  }
  Quad processQuad(float sampleRate, unsigned mask) {
    if (rate != sampleRate) configure(sampleRate, tone, texture, presence);
    return sound.process(mask);
  }
  Stereo process(float sampleRate) {
    Quad out = processQuad(sampleRate, 3u);
    return {out.channel[0], out.channel[1]};
  }
};
}  // namespace windchimes
