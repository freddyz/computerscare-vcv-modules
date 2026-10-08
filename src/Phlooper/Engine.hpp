#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
namespace phlooper {
constexpr int voices = 16, rate = 48000, capacity = rate * 60;
struct Frame {
  float l = 0, r = 0;
};
struct Settings {
  int count = 2, mode = 0, overdub = 0;
  bool hold = false;
  unsigned offsetMask = 0, startMask = 0, lengthMask = 0;
  std::array<float, voices> offsets{}, starts{}, lengths{};
  float start = 0, length = 1, offset = 1, recordMix = .5f, mix = 1, speed = 1;
};
class Engine {
 public:
  // Reserve storage outside process(). Unwritten storage is never read.
  std::array<std::unique_ptr<float[]>, voices> audio;
  std::array<double, voices> head{};
  std::array<int, voices> activeStart{};
  std::array<double, voices> periods{};
  std::array<float, voices> fade{};
  std::array<bool, voices> stereo{};
  unsigned completed = 0;
  int size = 0, captured = 0;
  bool initial = false;
  double writeClock = 0;
  Frame previous;
  // Windowed-sinc kernels, built once off the audio thread. Faster playback
  // selects a conservative low-pass cutoff before downsampling.
  std::array<float, 10 * 256 * 32> kernels{};
  const std::array<float, 10> downsample{
      {1.f, 1.5f, 2.f, 2.5f, 3.f, 4.f, 6.f, 8.f, 16.f, 32.f}};
  explicit Engine(int limit = capacity) : limit(limit) {
    activeStart.fill(-1);
    for (auto& a : audio) a.reset(new float[limit * 2]);
    const double pi = 3.141592653589793;
    for (int band = 0; band < 10; ++band)
      for (int phase = 0; phase < 256; ++phase) {
        float sum = 0;
        float cutoff = .45f / downsample[band];
        for (int tap = 0; tap < 32; ++tap) {
          double x = tap - 15 - phase / 256.;
          double sinc = std::fabs(x) < 1e-9
                            ? 2 * cutoff
                            : std::sin(2 * pi * cutoff * x) / (pi * x);
          float coefficient = float(sinc * (.5 + .5 * std::cos(pi * x / 17.)));
          kernels[(band * 256 + phase) * 32 + tap] = coefficient;
          sum += coefficient;
        }
        for (int tap = 0; tap < 32; ++tap)
          kernels[(band * 256 + phase) * 32 + tap] /= sum;
      }
  }
  int limit;
  void begin(bool isStereo) {
    initial = true;
    captured = 0;
    writeClock = 0;
    for (int i = 0; i < voices; ++i) stereo[i] = isStereo;
  }
  void finish() {
    initial = false;
    size = captured;
    resetHeads();
  }
  void resetHeads() {
    activeStart.fill(-1);
    head.fill(0);
    fade.fill(0);
  }
  Frame read(int voice, double pos, int regionStart, int regionLength) const {
    if (!size || pos >= regionLength) return {};
    int i = int(pos), j = (i + 1) % regionLength;
    int a = (regionStart + i) % size, b = (regionStart + j) % size;
    float f = float(pos - i);
    Frame out;
    out.l = audio[voice][2 * a] * (1 - f) + audio[voice][2 * b] * f;
    out.r = stereo[voice] ? audio[voice][2 * a + 1] * (1 - f) +
                                audio[voice][2 * b + 1] * f
                          : out.l;
    return out;
  }
  Frame filteredRead(int voice, double pos, int start, int length,
                     double increment) const {
    if (increment <= 1.05 || length < 32 || pos >= length)
      return read(voice, pos, start, length);
    int band = 0;
    while (band < 9 && downsample[band] < increment) ++band;
    int frame = int(pos), phase = std::min(255, int((pos - frame) * 256));
    const float* kernel = kernels.data() + (band * 256 + phase) * 32;
    Frame out;
    for (int tap = 0; tap < 32; ++tap) {
      int p = (frame + tap - 15 + length) % length;
      int index = (start + p) % size;
      out.l += audio[voice][2 * index] * kernel[tap];
      out.r += (stereo[voice] ? audio[voice][2 * index + 1]
                              : audio[voice][2 * index]) *
               kernel[tap];
    }
    return out;
  }
  Frame process(Frame input, float sampleRate, const Settings& s,
                unsigned record, unsigned erase, unsigned restart,
                unsigned mute, unsigned stop, bool isStereo) {
    completed = 0;
    const double step = double(rate) / sampleRate;
    if (initial) {
      writeClock += step;
      while (writeClock >= 1 && captured < limit) {
        writeClock -= 1;
        float f = float(std::max(0., std::min(1., 1. - writeClock / step)));
        float l = previous.l + (input.l - previous.l) * f;
        float r = previous.r + (input.r - previous.r) * f;
        for (int v = 0; v < voices; ++v) {
          audio[v][captured * 2] = l;
          audio[v][captured * 2 + 1] = isStereo ? r : l;
        }
        ++captured;
      }
      if (captured == limit) finish();
      previous = input;
      return input;
    }
    Frame sum;
    for (int v = 0; v < s.count; ++v) {
      unsigned bit = 1u << v;
      float startValue = s.startMask & bit ? s.starts[v] : s.start;
      int requestedStart =
          size ? std::min(size - 1,
                          int(std::max(0.f, std::min(1.f, startValue)) * size))
               : 0;
      int lengthVoice = s.hold ? 0 : v;
      float lengthValue = s.lengthMask & (1u << lengthVoice)
                              ? s.lengths[lengthVoice]
                              : s.length;
      int length =
          std::max(1, int(size * std::max(.001f, std::min(1.f, lengthValue))));
      length = std::min(std::max(1, size), length);
      float offset = s.hold
                         ? ((s.offsetMask & 1u) ? s.offsets[0] : 0.f)
                         : ((s.offsetMask & bit) ? s.offsets[v] : v * s.offset);
      double period = length;
      double speed = 1;
      if (s.mode == 0)
        period -= offset * rate / 1000.;
      else if (s.mode == 1)
        period *= 1. + offset / 100.;
      else
        speed = std::max(.05, std::min(8., 1. + offset / 100.));
      period = std::max(48., std::min(double(limit), period));
      speed *= std::max(.25f, std::min(4.f, s.speed));
      periods[v] = period;
      if (activeStart[v] < 0 || (restart & bit))
        activeStart[v] = requestedStart;
      int start = activeStart[v];
      if (restart & bit) {
        head[v] = 0;
        fade[v] = 0;
      }
      head[v] = std::fmod(head[v], period);
      if (size) {
        Frame out = filteredRead(v, head[v], start,
                                 std::min(length, int(period)), step * speed);
        float edge = float(std::min(head[v], period - head[v]) /
                           std::min(48., period * .25));
        edge = std::max(0.f, std::min(1.f, edge));
        float target = (mute | stop) & bit ? 0.f : 1.f;
        fade[v] += (target - fade[v]) * std::min(1.f, 200.f / sampleRate);
        sum.l += out.l * edge * fade[v];
        sum.r += out.r * edge * fade[v];
        // Visit every crossed storage frame, even when playback is faster than
        // 1x.
        double end = head[v] + step * speed;
        int steps = int(std::ceil(end)) - int(head[v]);
        for (int k = 0; k < steps && !(stop & bit) && ((record | erase) & bit);
             ++k) {
          double p = int(head[v]) + k;
          if (p >= period || p >= length) continue;
          int index = (start + int(p)) % size;
          if (erase & bit) {
            audio[v][index * 2] = 0;
            audio[v][index * 2 + 1] = 0;
          } else {
            float weight = float(std::min(end, p + 1.) - std::max(head[v], p));
            float retained =
                s.overdub == 0 ? std::pow(1 - s.recordMix, weight) : 1;
            float incoming =
                s.overdub == 0 ? 1 - retained : s.recordMix * weight;
            float l = audio[v][index * 2] * retained + input.l * incoming;
            float r = audio[v][index * 2 + 1] * retained + input.r * incoming;
            audio[v][index * 2] = std::max(-20.f, std::min(20.f, l));
            audio[v][index * 2 + 1] = std::max(-20.f, std::min(20.f, r));
            if (isStereo) stereo[v] = true;
          }
        }
        if (!(stop & bit)) {
          double next = head[v] + step * speed;
          if (next >= period) {
            completed |= bit;
            activeStart[v] = requestedStart;
          }
          head[v] = std::fmod(next, period);
        }
      } else
        fade[v] = 0;
    }
    float gain = 1.f / std::max(1, s.count);
    Frame result;
    result.l = input.l * (1 - s.mix) + sum.l * gain * s.mix;
    result.r = input.r * (1 - s.mix) + sum.r * gain * s.mix;
    return result;
  }
};
}  // namespace phlooper
