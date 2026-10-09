#pragma once

#include <algorithm>
#include <cmath>

namespace phlooper {
constexpr int waveformBins = 256;
// Log amplitude lifts quiet details and leaves headroom for large peaks.
// Display mapping only; stored audio and playback gain remain unchanged.
inline float waveformAmplitude(float volts) {
  float magnitude = std::min(20.f, std::fabs(volts));
  float compressed = std::log1p(magnitude / .1f) / std::log(201.f);
  return std::copysign(std::pow(compressed, 1.15f), volts);
}
struct WaveformBin {
  float low = 0.f, high = 0.f;
};
// Bounded overview sampling keeps visualization work independent of WAV length.
// Keep stereo extrema separately instead of cancelling opposite-phase channels.
inline WaveformBin sampleWaveformBin(const float* audio, int frames,
                                     bool stereo, int bin) {
  WaveformBin result;
  if (!audio || frames <= 0 || bin < 0 || bin >= waveformBins) return result;
  int first = int(static_cast<long long>(bin) * frames / waveformBins);
  int last = std::max(
      first, int(static_cast<long long>(bin + 1) * frames / waveformBins) - 1);
  int samples = std::min(64, last - first + 1);
  result.low = result.high = audio[first * 2];
  for (int point = 0; point < samples; ++point) {
    int frame = first + (samples > 1 ? int(static_cast<long long>(point) *
                                           (last - first) / (samples - 1))
                                     : 0);
    float left = audio[frame * 2];
    float right = stereo ? audio[frame * 2 + 1] : left;
    result.low = std::min(result.low, std::min(left, right));
    result.high = std::max(result.high, std::max(left, right));
  }
  return result;
}
}  // namespace phlooper
