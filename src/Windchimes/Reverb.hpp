#pragma once
#include "Spatial.hpp"
namespace windchimes {
// Stereo input diffusion followed by an eight-line orthogonal feedback network.
// All buffers are fixed storage; size changes move fractional delay taps
// smoothly.
class Reverb {
  static constexpr int capacity = 32768;
  struct Line {
    std::array<float, capacity> buffer{};
    int write = 0;
    float delay = 1000.f, target = 1000.f, filtered = 0.f, feedback = 0.7f;
    float read() const {
      float position = write - delay;
      if (position < 0.f) position += capacity;
      int index = static_cast<int>(position);
      float fraction = position - index;
      return buffer[index] +
             fraction * (buffer[(index + 1) % capacity] - buffer[index]);
    }
  };
  struct Allpass {
    std::array<float, 4096> buffer{};
    int write = 0, length = 240;
    float process(float in) {
      float delayed = buffer[write];
      float out = delayed - in * 0.6f;
      buffer[write] = in + out * 0.6f;
      if (++write >= length) write = 0;
      return out;
    }
  };
  std::array<Line, 8> lines;
  std::array<Allpass, 4> diffusers;
  float rate = 0.f, lastSize = -1.f, damping = 0.2f, slew = 0.001f;

 public:
  void configure(float sampleRate, float size) {
    size = clamp(size, 0.f, 1.f);
    bool rateChanged = rate != sampleRate;
    if (!rateChanged && size == lastSize) return;
    rate = sampleRate;
    lastSize = size;
    const float times[8] = {0.0297f, 0.0371f, 0.0411f, 0.0437f,
                            0.0531f, 0.0613f, 0.0719f, 0.0797f};
    float scale = 0.45f + size * 1.4f;
    float decaySeconds = 0.55f + size * size * 9.f;
    damping = 1.f - std::exp(-2.f * pi * (6500.f - size * 3000.f) / rate);
    slew = 1.f - std::exp(-1.f / (rate * 0.12f));
    for (int i = 0; i < 8; ++i) {
      auto& line = lines[i];
      line.target = clamp(times[i] * scale * rate, 4.f, capacity - 2.f);
      line.feedback =
          std::exp(-6.907755f * line.target / (rate * decaySeconds));
      if (rateChanged) {
        line.buffer.fill(0.f);
        line.write = 0;
        line.filtered = 0.f;
        line.delay = line.target;
      }
    }
    const float diffusionTimes[4] = {0.0047f, 0.0113f, 0.0053f, 0.0127f};
    if (rateChanged)
      for (int i = 0; i < 4; ++i) {
        diffusers[i].buffer.fill(0.f);
        diffusers[i].write = 0;
        diffusers[i].length = std::max(
            1, std::min(4096, static_cast<int>(diffusionTimes[i] * rate)));
      }
  }
  Quad processWetQuad(Stereo dry) {
    float left = diffusers[1].process(diffusers[0].process(dry.left));
    float right = diffusers[3].process(diffusers[2].process(dry.right));
    float values[8];
    Quad wet;
    for (int i = 0; i < 8; ++i) {
      auto& line = lines[i];
      line.delay += (line.target - line.delay) * slew;
      float delayed = line.read();
      line.filtered += (delayed - line.filtered) * damping;
      if (std::fabs(line.filtered) < 1e-20f) line.filtered = 0.f;
      values[i] = line.filtered * line.feedback;
      for (int ch = 0; ch < 4; ++ch) {
        const int patterns[4] = {1, 2, 4, 7};
        int bits = i & patterns[ch];
        bool odd = ((bits & 1) != 0) ^ ((bits & 2) != 0) ^ ((bits & 4) != 0);
        wet.channel[ch] += delayed * (odd ? -0.25f : 0.25f);
      }
    }
    // Normalized Hadamard transform preserves feedback energy.
    for (int width = 1; width < 8; width *= 2)
      for (int base = 0; base < 8; base += width * 2)
        for (int j = 0; j < width; ++j) {
          float a = values[base + j], b = values[base + j + width];
          values[base + j] = a + b;
          values[base + j + width] = a - b;
        }
    for (int i = 0; i < 8; ++i) {
      auto& line = lines[i];
      float input = (i & 1 ? right : left) * (i & 4 ? -0.22f : 0.22f);
      line.buffer[line.write] = input + values[i] * 0.35355339f;
      if (++line.write == capacity) line.write = 0;
    }
    return wet;
  }
  Stereo processWet(Stereo dry) {
    Quad wet = processWetQuad(dry);
    return {wet.channel[0], wet.channel[1]};
  }
  Stereo process(Stereo dry, float mix) {
    mix = clamp(mix, 0.f, 1.f);
    Stereo wet = processWet(dry);
    return {dry.left * (1.f - mix) + wet.left * mix,
            dry.right * (1.f - mix) + wet.right * mix};
  }
};
}  // namespace windchimes
