#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace computerscare {
namespace contrast {
// sRGB relative luminance, not an average of encoded RGB components.
inline float linear(float value) {
  value = std::max(0.f, std::min(1.f, value));
  return value <= .04045f ? value / 12.92f
                          : std::pow((value + .055f) / 1.055f, 2.4f);
}
inline float luminance(float r, float g, float b) {
  return .2126f * linear(r) + .7152f * linear(g) + .0722f * linear(b);
}
inline float ratio(float a, float b) {
  return (std::max(a, b) + .05f) / (std::min(a, b) + .05f);
}
inline float srgb(float value) {
  return value <= .0031308f ? value * 12.92f
                            : 1.055f * std::pow(value, 1.f / 2.4f) - .055f;
}
// Black or white always supplies at least 4.58:1 against an opaque surface.
inline float textInk(float surfaceLuminance) {
  return ratio(0, surfaceLuminance) >= ratio(1, surfaceLuminance) ? 0.f : 1.f;
}
template <size_t N>
inline float border(std::array<float, N> surfaces, float brightest = 1.f) {
  static_assert(N > 0, "A border needs at least one adjacent surface");
  // One ink for the entire control, including its bevels and both states.
  // Maximize the weakest contrast against all of those fills and the panel.
  // The optimum is black, the allowed ceiling, or the equal-contrast point
  // between two adjacent luminances. Some combinations cannot all reach 3:1
  // with one ink.
  std::sort(surfaces.begin(), surfaces.end());
  float best = 0, score = 0, ceiling = linear(brightest);
  auto consider = [&](float candidate) {
    if (candidate > ceiling) return;
    float minimum = 21;
    for (float surface : surfaces)
      minimum = std::min(minimum, ratio(candidate, surface));
    if (minimum > score) {
      best = candidate;
      score = minimum;
    }
  };
  consider(0);
  consider(ceiling);
  for (size_t i = 1; i < N; ++i)
    consider(std::sqrt((surfaces[i - 1] + .05f) * (surfaces[i] + .05f)) - .05f);
  return srgb(best);
}
inline float border(float faceLuminance, float backgroundLuminance) {
  return border(std::array<float, 2>{{faceLuminance, backgroundLuminance}});
}
inline float controlBorder(float raised, float recessed, float background,
                           float brightest = .8f) {
  // Prefer the familiar dark outline when it reads against the button face.
  // On a dark panel, the filled face can supply the exterior silhouette.
  // Dark bevel shading must not force the whole outline to become light.
  float faceContrast = std::min(ratio(0, raised), ratio(0, recessed));
  float silhouette =
      std::min(ratio(raised, background), ratio(recessed, background));
  if (faceContrast >= 3.f && (ratio(0, background) >= 2.f || silhouette >= 3.f))
    return 0.f;
  return border(std::array<float, 3>{{raised, recessed, background}},
                brightest);
}
}  // namespace contrast
}  // namespace computerscare
