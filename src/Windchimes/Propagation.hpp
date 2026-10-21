#pragma once
#include <memory>

#include "Types.hpp"
namespace windchimes {
// One mono travel path per source, before speaker routing and room sends.
class Propagation {
  static constexpr unsigned capacity = 262144;
  std::unique_ptr<float[]> buffer{new float[capacity]{}};
  unsigned position = 0, valid = 0;
  bool forestReflections = false;
  std::array<float, 3> reflectionTaps{}, wantedReflectionTaps{},
      reflectionGains{};
  float reflectionValue = 0.f, reflectionState = 0.f, reflectionCoeff = .2f,
        reflectionSlew = .002f;
  float target = 0.f, tap = 0.f, previous = 0.f, blend = 1.f, blendStep = .001f;
  float read(float lag) const {
    if (valid <= std::ceil(lag)) return 0.f;
    double index = double(position) - lag;
    if (index < 0.) index += capacity;
    unsigned a = static_cast<unsigned>(index);
    float fraction = index - a;
    return buffer[a] * (1.f - fraction) +
           buffer[(a + 1) & (capacity - 1)] * fraction;
  }

 public:
  void reset() {
    reflectionValue = reflectionState = 0.f;
    valid = 0;
    blend = 1.f;
  }
  void configure(float rate, float distance) {
    target =
        std::min(float(capacity - 2), .08f * rate * clamp(distance, 0.f, 1.f));
    blendStep = 1.f / std::max(1.f, .02f * rate);
  }
  void configureReflections(float rate, float expanse, float thicket,
                            float undergrowth, float canopy, float frequency,
                            float distance, int material) {
    forestReflections = true;
    float pitch = std::log2(std::max(30.f, frequency) / 55.f);
    pitch -= std::floor(pitch);
    float period = rate / std::max(30.f, frequency);
    for (int i = 0; i < 3; ++i) {
      float seconds =
          (.025f + .65f * expanse * expanse) *
          (1.f + .18f * i + .18f * pitch + .2f * distance + .03f * material);
      // Nearby multiples of the source's period color reflections with its
      // tuning.
      float samples = seconds * rate;
      float tuned = std::max(period, std::round(samples / period) * period);
      wantedReflectionTaps[i] = clamp(samples + (tuned - samples) * undergrowth,
                                      2.f, float(capacity - 2));
      reflectionGains[i] =
          i == 0 ? 1.f : (i == 1 ? .7f * thicket : .5f * thicket * thicket);
      if (!valid) reflectionTaps[i] = wantedReflectionTaps[i];
    }
    reflectionCoeff =
        1.f - std::exp(-2.f * pi * (16000.f * std::pow(.008f, canopy)) / rate);
    reflectionSlew = 1.f - std::exp(-1.f / (rate * .08f));
  }
  float reflection() const { return reflectionValue; }
  float process(float input) {
    if (!valid) {
      tap = previous = target;
      blend = 1.f;
    }
    if (blend >= 1.f && std::fabs(target - tap) > .1f) {
      previous = tap;
      tap = target;
      blend = 0.f;
    }
    buffer[position] = input;
    valid = std::min(valid + 1, capacity - 1);
    float out = read(tap);
    if (blend < 1.f) out = read(previous) * (1.f - blend) + out * blend;
    blend = std::min(1.f, blend + blendStep);
    if (forestReflections) {
      float echoes = 0.f;
      for (int i = 0; i < 3; ++i) {
        reflectionTaps[i] +=
            (wantedReflectionTaps[i] - reflectionTaps[i]) * reflectionSlew;
        echoes += read(reflectionTaps[i]) * reflectionGains[i];
      }
      reflectionState += (echoes - reflectionState) * reflectionCoeff;
      if (std::fabs(reflectionState) < 1e-20f) reflectionState = 0.f;
      reflectionValue = reflectionState;
    }
    position = (position + 1) & (capacity - 1);
    return out;
  }
};
}  // namespace windchimes
