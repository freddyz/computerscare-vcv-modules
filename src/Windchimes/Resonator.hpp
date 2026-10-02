#pragma once
#include "Types.hpp"
namespace windchimes {
// A bank of damped complex rotations. Coefficients change at control rate;
// the sample loop contains only multiplies/adds and no allocation or trig.
class Resonator {
  struct Mode {
    float real = 0.f, imag = 0.f, a = 0.f, b = 0.f, gain = 0.f;
  };
  std::array<Mode, modesPerTube> modes{};
  int remaining = 0;
  float frequency = -1.f, sampleRate = 0.f, decay = -1.f, brightness = -1.f,
        hardness = -1.f;
  int material = -1, lifetime = 0;

 public:
  void reset() {
    for (auto& m : modes) {
      m.real = m.imag = 0.f;
    }
    remaining = 0;
  }
  bool active() const { return remaining > 0; }
  void configure(float hz, float rate, const SetConfig& c) {
    if (frequency == hz && sampleRate == rate && material == c.material &&
        decay == c.decay && brightness == c.brightness &&
        hardness == c.hardness)
      return;
    frequency = hz;
    sampleRate = rate;
    material = c.material;
    decay = c.decay;
    brightness = c.brightness;
    hardness = c.hardness;
    const float ratios[modesPerTube] = {1.f,     2.756f, 5.404f, 8.933f,
                                        13.344f, 18.64f, 24.82f, 31.88f};
    float baseDecay =
        c.material == 0 ? 1.2f : (c.material == 1 ? 0.14f : 0.32f);
    float t60 = baseDecay * std::exp2(c.decay * 4.f);
    lifetime = static_cast<int>(std::min(t60 * 1.25f, 30.f) * rate);
    float normalization = 0.f;
    for (int i = 0; i < modesPerTube; ++i) {
      auto& m = modes[i];
      float ratio =
          c.material == 2 ? ratios[i] * (1.f + 0.014f * i) : ratios[i];
      float f = hz * ratio;
      float seconds = t60 / (1.f + i * (c.material == 0 ? 0.18f : 0.65f));
      float radius = std::exp(-6.907755f / (seconds * rate));
      float angle = 2.f * pi * std::min(f, rate * 0.45f) / rate;
      m.a = radius * std::cos(angle);
      m.b = radius * std::sin(angle);
      m.gain = f < rate * 0.45f
                   ? std::exp(-i * (1.6f - c.brightness * 1.25f)) *
                         (i == 0 ? 1.f : 0.35f + c.hardness * 0.65f)
                   : 0.f;
      if (m.gain == 0.f) m.real = m.imag = 0.f;
      normalization += m.gain;
    }
    for (auto& m : modes) m.gain /= std::max(normalization, 1.f);
  }
  void strike(float velocity) {
    for (auto& m : modes) m.real = clamp(m.real + velocity * m.gain, -2.f, 2.f);
    remaining = lifetime;
  }
  float process() {
    if (remaining <= 0) return 0.f;
    --remaining;
    float out = 0.f;
    for (auto& m : modes) {
      if (m.gain == 0.f) continue;
      float real = m.a * m.real - m.b * m.imag;
      m.imag = m.b * m.real + m.a * m.imag;
      m.real = real;
      out += m.imag;
    }
    if (remaining == 0) reset();
    return out;
  }
};
}  // namespace windchimes
