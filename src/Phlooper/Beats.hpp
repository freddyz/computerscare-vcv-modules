#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
namespace phlooper {
// Clock marks from initial capture are chronological. Stop at the first
// wrap into an overdub, and ignore the initially unsplit multiplication gap.
template <typename Marks>
float recordedBeatFromMarks(const Marks& marks, unsigned count) {
  float shortest = 0.f;
  for (unsigned i = 1; i < count; ++i) {
    float gap = float(marks[i]) - float(marks[i - 1]);
    if (gap <= 1.f) break;
    if (shortest == 0.f || gap < shortest) shortest = gap;
  }
  return shortest;
}
struct BeatFraction {
  int numerator, denominator;
};
inline BeatFraction beatFraction(int index) {
  static const BeatFraction values[] = {
      {0, 1},  {1, 128}, {1, 64}, {1, 48}, {1, 32}, {1, 24}, {1, 16},
      {1, 14}, {1, 12},  {1, 10}, {1, 8},  {2, 15}, {1, 7},  {2, 13},
      {1, 6},  {2, 11},  {3, 16}, {4, 21}, {1, 5},  {2, 9},  {1, 4},
      {2, 7},  {5, 16},  {7, 22}, {1, 3},  {4, 11}, {3, 8},  {3, 7},
      {1, 2},  {5, 9},   {4, 7},  {3, 5},  {5, 8},  {5, 7},  {3, 4},
      {7, 8},  {1, 1},   {3, 2},  {2, 1},  {3, 1},  {4, 1}};
  auto value = values[std::min(40, std::abs(index))];
  if (index < 0) value.numerator = -value.numerator;
  return value;
}
inline float beatValue(float knob) {
  auto value = beatFraction(
      int(std::round(std::max(-20.f, std::min(20.f, knob)) * 2.f)));
  return float(value.numerator) / value.denominator;
}
inline float snapBeatRegion(float value, int frames, double beatFrames,
                            float quantum, float minimum = 0.f,
                            float origin = 0.f) {
  if (frames <= 0 || quantum <= 0.f) return value;
  double step = quantum * (beatFrames > 0 ? beatFrames : frames);
  double lower = std::ceil((double(minimum) * frames - origin) / step - 1e-6);
  double upper = std::floor((frames - origin) / step + 1e-6);
  if (upper < lower) return 1.f;  // The selected step exceeds the recording.
  double index = std::floor((double(value) * frames - origin) / step + .500001);
  double snapped = std::max(lower, std::min(upper, index)) * step + origin;
  return float(std::max(double(minimum), std::min(1., snapped / frames)));
}
inline std::string beatLengthLabel(double beats) {
  beats = std::max(0., beats);
  long long whole = static_cast<long long>(std::floor(beats));
  double fraction = beats - whole;
  for (int denominator = 1; denominator <= 1024; ++denominator) {
    int numerator = int(std::round(fraction * denominator));
    if (std::abs(fraction - double(numerator) / denominator) > 2e-5) continue;
    if (numerator == denominator) {
      ++whole;
      numerator = 0;
    }
    if (!numerator) return std::to_string(whole);
    return (whole ? std::to_string(whole) + " " : "") +
           std::to_string(numerator) + "/" + std::to_string(denominator);
  }
  char text[48];
  std::snprintf(text, sizeof(text), "%.3f", beats);
  return text;
}
inline std::string beatLabel(float knob) {
  auto value = beatFraction(
      int(std::round(std::max(-20.f, std::min(20.f, knob)) * 2.f)));
  return (value.numerator > 0 ? "+" : "") + std::to_string(value.numerator) +
         (value.denominator == 1 ? ""
                                 : "/" + std::to_string(value.denominator));
}
}  // namespace phlooper
