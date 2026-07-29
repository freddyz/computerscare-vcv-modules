#pragma once

#include <algorithm>

#include "rack.hpp"

namespace computerscare {
namespace volypector {

enum class RandomizeMode { REPLACE = 0, WIGGLE = 1 };

struct RandomizeSettings {
  float chance = 1.f;
  RandomizeMode mode = RandomizeMode::REPLACE;
  float minValue = 0.f;
  float maxValue = 10.f;
  float wiggleMin = -1.f;
  float wiggleMax = 1.f;
};

inline float sortedMin(float a, float b) { return std::min(a, b); }

inline float sortedMax(float a, float b) { return std::max(a, b); }

inline bool shouldRandomize(float chance) {
  return rack::random::uniform() <= rack::math::clamp(chance, 0.f, 1.f);
}

inline float randomizeValue(float currentValue,
                            const RandomizeSettings& settings) {
  if (!shouldRandomize(settings.chance)) {
    return currentValue;
  }

  float minValue = sortedMin(settings.minValue, settings.maxValue);
  float maxValue = sortedMax(settings.minValue, settings.maxValue);
  if (settings.mode == RandomizeMode::WIGGLE) {
    float wiggleMin = sortedMin(settings.wiggleMin, settings.wiggleMax);
    float wiggleMax = sortedMax(settings.wiggleMin, settings.wiggleMax);
    float delta = wiggleMin + rack::random::uniform() * (wiggleMax - wiggleMin);
    return rack::math::clamp(currentValue + delta, minValue, maxValue);
  }

  return minValue + rack::random::uniform() * (maxValue - minValue);
}

}  // namespace volypector
}  // namespace computerscare
