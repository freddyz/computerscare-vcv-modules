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
                                     bool stereo, int bin, float from = 0.f,
                                     float to = 1.f) {
  WaveformBin result;
  if (!audio || frames <= 0 || bin < 0 || bin >= waveformBins) return result;
  int first =
      int(std::floor((from + double(to - from) * bin / waveformBins) * frames));
  int last = std::max(
      first,
      int(std::ceil((from + double(to - from) * (bin + 1) / waveformBins) *
                    frames)) -
          1);
  auto wrap = [&](int frame) { return (frame % frames + frames) % frames; };
  int samples = std::min(64, last - first + 1);
  result.low = result.high = audio[wrap(first) * 2];
  for (int point = 0; point < samples; ++point) {
    int frame = first + (samples > 1 ? int(static_cast<long long>(point) *
                                           (last - first) / (samples - 1))
                                     : 0);
    frame = wrap(frame);
    float left = audio[frame * 2];
    float right = stereo ? audio[frame * 2 + 1] : left;
    result.low = std::min(result.low, std::min(left, right));
    result.high = std::max(result.high, std::max(left, right));
  }
  return result;
}
}  // namespace phlooper
