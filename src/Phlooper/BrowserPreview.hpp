#pragma once

#include "BrowserWaveform.hpp"
#include "Waveform.hpp"

namespace phlooper {
// Lightweight display-only data for Rack's null-module browser widgets.
// No audio buffers, DSP engine, or changes to new-module defaults.
struct BrowserPreview {
  int count;
  int mode, direction;
  float start, length, offset;
  float seconds = browserWaveformSeconds;
  float waveformShift = 0.f, waveformSpeed = 1.f;

  BrowserPreview(float countRandom, float startRandom, float lengthRandom,
                 float offsetRandom, float waveformRandom = 0.f,
                 float modeRandom = 0.f, float directionRandom = 0.f)
      : count(2 + std::min(3, int(countRandom * 4.f))),
        mode(std::min(3, int(modeRandom * 4.f))),
        direction(std::min(3, int(directionRandom * 4.f))),
        start(.18f + .08f * startRandom),
        length(.36f + .22f * lengthRandom),
        offset((offsetRandom < .5f ? -1.f : 1.f) *
               (.02f + .06f * std::fabs(offsetRandom * 2.f - 1.f)) /
               browserWaveformSeconds),
        waveformShift(waveformRandom),
        waveformSpeed(.85f + .4f * std::fabs(std::sin(waveformRandom * 71.f))) {
  }

  float regionStart(int voice) const { return start + voice * offset; }
  float position(int voice) const {
    return regionStart(voice) + length * (.17f + .16f * voice);
  }

  WaveformBin sampleWaveform(float position) const {
    // A shared time warp and gain contour vary each browser instance while
    // preserving the same recognizable speech detail in every loop row.
    float source = waveformShift + position * waveformSpeed +
                   .018f * std::sin(position * 13.f + waveformShift * 31.f);
    auto bin = waveform(source);
    float gain =
        .65f +
        .55f * std::fabs(std::sin(position * 21.f + waveformShift * 97.f));
    bin.low *= gain;
    bin.high *= gain;
    return bin;
  }

  static WaveformBin waveform(float position) {
    position -= std::floor(position);
    // Sample real speech extrema rather than rounded synthetic envelopes.
    int index = std::min(waveformBins - 1, int(position * waveformBins));
    WaveformBin bin;
    bin.low = browserWaveform[index][0];
    bin.high = browserWaveform[index][1];
    return bin;
  }
};
}  // namespace phlooper
