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
  std::array<int, voices> directions{};
  float start = 0, length = 1, offset = 1, recordMix = .5f, mix = 1, speed = 1;
};
class Engine {
 public:
  // Reserve storage outside process(). Unwritten storage is never read.
  std::array<std::unique_ptr<float[]>, voices> audio;
  std::array<double, voices> head{};
  struct WriteSpan {
    double from = 0, extent = 0, period = 1;
    int base = 0, kind = 0;
  };
  std::array<WriteSpan, voices> writeSpans{};
  std::array<int, voices> previousWriteKind{};
  std::array<int, voices> activeStart{};
  std::array<double, voices> periods{};
  std::array<bool, voices> returning{};
  std::array<int, voices> previousDirection{};
  std::array<float, voices> fade{};
  std::array<bool, voices> stereo{};
  struct SpeedHoldState {
    bool active = false;
    int count = 0, size = 0;
    double clock = 0, duration = 1, globalSpeed = 1;
    std::array<double, voices> starts{}, periods{}, speeds{}, phases{};
  } speedHold;
  std::array<Frame, voices> outputs{};
  unsigned completed = 0;
  int size = 0, captured = 0;
  bool initial = false;
  double writeClock = 0;
  std::array<Frame, voices> previousInputs{};
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
    previousInputs.fill({});
    for (int i = 0; i < voices; ++i) stereo[i] = isStereo;
  }
  void finish() {
    initial = false;
    size = captured;
    resetHeads();
  }
  void resetHeads() {
    speedHold.active = false;
    activeStart.fill(-1);
    head.fill(0);
    fade.fill(0);
    returning.fill(false);
    previousWriteKind.fill(0);
  }
  double playbackPosition(int voice, int direction) const {
    bool reverse = direction == 1 || (direction == 2 && returning[voice]);
    return reverse ? std::max(0., periods[voice] - 1. - head[voice])
                   : head[voice];
  }
  static double wrapPhase(double phase) { return phase - std::floor(phase); }
  double sourceStart(int voice) const {
    return speedHold.active && voice < speedHold.count
               ? speedHold.starts[voice]
               : std::max(0, activeStart[voice]);
  }
  int nominalLength(const Settings& s, int voice) const {
    float value = s.lengthMask & (1u << voice) ? s.lengths[voice] : s.length;
    return std::min(
        std::max(1, size),
        std::max(1, int(size * std::max(.001f, std::min(1.f, value)))));
  }
  void captureSpeedHold(const Settings& s) {
    std::array<double, voices> positions{}, phases{}, speeds{};
    double global = std::max(.25f, std::min(4.f, s.speed));
    double duration = double(limit) * 80.;
    for (int v = 0; v < s.count; ++v) {
      double oldPeriod =
          periods[v] > 0 ? periods[v] : std::max(48, nominalLength(s, v));
      phases[v] = wrapPhase(head[v] / oldPeriod);
      float requested = s.startMask & (1u << v) ? s.starts[v] : s.start;
      double oldStart =
          activeStart[v] >= 0 || (speedHold.active && v < speedHold.count)
              ? sourceStart(v)
              : std::min(size - 1,
                         int(std::max(0.f, std::min(1.f, requested)) * size));
      bool reverse =
          s.directions[v] == 1 || (s.directions[v] == 2 && returning[v]);
      positions[v] =
          oldStart +
          (reverse ? std::max(0., oldPeriod - 1. - head[v]) : head[v]);
      float offset = s.offsetMask & (1u << v) ? s.offsets[v] : v * s.offset;
      speeds[v] = std::max(.05, std::min(8., 1. + offset / 100.)) *
                  std::max(.25f, std::min(4.f, s.speed));
      duration =
          std::min(duration, std::max(48, nominalLength(s, v)) / speeds[v]);
    }
    speedHold = {};
    speedHold.active = true;
    speedHold.count = s.count;
    speedHold.size = size;
    speedHold.duration = duration;
    speedHold.globalSpeed = global;
    for (int v = 0; v < s.count; ++v) {
      speedHold.speeds[v] = speeds[v];
      speedHold.periods[v] = duration * speeds[v];
      speedHold.phases[v] = phases[v];
      double position = phases[v] * speedHold.periods[v];
      bool reverse =
          s.directions[v] == 1 || (s.directions[v] == 2 && returning[v]);
      if (reverse)
        position = std::max(0., speedHold.periods[v] - 1. - position);
      double start = positions[v] - position;
      speedHold.starts[v] = start - std::floor(start / size) * size;
      previousWriteKind[v] = 0;
    }
  }
  Frame sourceFrame(int voice, double position) const {
    int base = int(std::floor(position));
    int a = base;
    if (a < 0 || a >= size) {
      a %= size;
      if (a < 0) a += size;
    }
    int b = a + 1 == size ? 0 : a + 1;
    float f = float(position - base);
    Frame out;
    if (f == 0.f) {
      out.l = audio[voice][2 * a];
      out.r = stereo[voice] ? audio[voice][2 * a + 1] : out.l;
      return out;
    }
    out.l = audio[voice][2 * a] * (1 - f) + audio[voice][2 * b] * f;
    out.r = stereo[voice] ? audio[voice][2 * a + 1] * (1 - f) +
                                audio[voice][2 * b + 1] * f
                          : out.l;
    return out;
  }
  Frame read(int voice, double pos, double regionStart,
             int regionLength) const {
    if (!size || pos >= regionLength) return {};
    int i = int(pos), j = (i + 1) % regionLength;
    Frame a = sourceFrame(voice, regionStart + i),
          b = sourceFrame(voice, regionStart + j);
    float f = float(pos - i);
    Frame out;
    out.l = a.l * (1 - f) + b.l * f;
    out.r = a.r * (1 - f) + b.r * f;
    return out;
  }
  Frame filteredRead(int voice, double pos, double start, int length,
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
      Frame sample = sourceFrame(voice, start + p);
      out.l += sample.l * kernel[tap];
      out.r += sample.r * kernel[tap];
    }
    return out;
  }
  Frame process(Frame input, float sampleRate, const Settings& s,
                unsigned record, unsigned erase, unsigned restart,
                unsigned mute, unsigned stop, bool isStereo,
                const std::array<Frame, voices>* channelInputs = nullptr) {
    outputs.fill({});
    completed = 0;
    const double step = double(rate) / sampleRate;
    if (initial) {
      writeClock += step;
      while (writeClock >= 1 && captured < limit) {
        writeClock -= 1;
        float f = float(std::max(0., std::min(1., 1. - writeClock / step)));
        for (int v = 0; v < voices; ++v) {
          Frame incoming = channelInputs ? (*channelInputs)[v] : input;
          float l =
              previousInputs[v].l + (incoming.l - previousInputs[v].l) * f;
          float r =
              previousInputs[v].r + (incoming.r - previousInputs[v].r) * f;
          if (isStereo) stereo[v] = true;
          audio[v][captured * 2] = l;
          audio[v][captured * 2 + 1] = isStereo ? r : l;
        }
        ++captured;
      }
      for (int v = 0; v < voices; ++v) {
        writeSpans[v].from = 0;
        writeSpans[v].extent = captured;
        writeSpans[v].period = std::max(1, captured);
        writeSpans[v].base = 0;
        writeSpans[v].kind = 1;
        previousWriteKind[v] = 0;
      }
      if (captured == limit) finish();
      for (int v = 0; v < voices; ++v)
        previousInputs[v] = channelInputs ? (*channelInputs)[v] : input;
      for (int v = 0; v < s.count; ++v)
        outputs[v] = channelInputs ? (*channelInputs)[v] : input;
      return input;
    }
    if (!size) {
      speedHold.active = false;
      fade.fill(0);
      for (int v = 0; v < s.count; ++v) {
        Frame incoming = channelInputs ? (*channelInputs)[v] : input;
        outputs[v].l = incoming.l * (1 - s.mix);
        outputs[v].r = incoming.r * (1 - s.mix);
      }
      Frame result;
      result.l = input.l * (1 - s.mix);
      result.r = input.r * (1 - s.mix);
      return result;
    }
    bool holdingSpeed = s.hold && s.mode == 2 && size > 0;
    if (holdingSpeed && (!speedHold.active || speedHold.count != s.count ||
                         speedHold.size != size))
      captureSpeedHold(s);
    if (!holdingSpeed && speedHold.active) {
      for (int v = 0; v < speedHold.count; ++v) {
        head[v] = wrapPhase(speedHold.clock + speedHold.phases[v]) *
                  std::max(48, nominalLength(s, v));
        activeStart[v] = -1;
        previousWriteKind[v] = 0;
      }
      speedHold.active = false;
    }
    double globalScale =
        speedHold.active
            ? std::max(.25f, std::min(4.f, s.speed)) / speedHold.globalSpeed
            : 1.;
    double holdStep =
        speedHold.active ? step * globalScale / speedHold.duration : 0.;
    Frame sum;
    for (int v = 0; v < s.count; ++v) {
      Frame incoming = channelInputs ? (*channelInputs)[v] : input;
      unsigned bit = 1u << v;
      float startValue = s.startMask & bit ? s.starts[v] : s.start;
      int requestedStart =
          size ? std::min(size - 1,
                          int(std::max(0.f, std::min(1.f, startValue)) * size))
               : 0;
      int lengthVoice = s.hold && !speedHold.active ? 0 : v;
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
        period += offset * rate / 1000.;
      else if (s.mode == 1)
        period *= 1. + offset / 100.;
      else
        speed = std::max(.05, std::min(8., 1. + offset / 100.));
      period = std::max(48., std::min(double(limit), period));
      speed *= std::max(.25f, std::min(4.f, s.speed));
      if (speedHold.active) {
        period = speedHold.periods[v];
        speed = speedHold.speeds[v] * globalScale;
        length = std::max(1, int(std::ceil(period)));
        head[v] = wrapPhase(speedHold.clock + speedHold.phases[v]) * period;
      }
      periods[v] = period;
      int direction = std::max(0, std::min(2, s.directions[v]));
      if (direction != previousDirection[v] || (restart & bit)) {
        returning[v] = false;
        previousWriteKind[v] = 0;
      }
      previousDirection[v] = direction;
      bool reverse = direction == 1 || (direction == 2 && returning[v]);
      if (size && (activeStart[v] < 0 || (restart & bit)))
        activeStart[v] = requestedStart;
      double start = sourceStart(v);
      if (restart & bit) {
        head[v] = 0;
        if (speedHold.active) speedHold.phases[v] = wrapPhase(-speedHold.clock);
        fade[v] = 0;
      }
      head[v] = std::fmod(head[v], period);
      int writeKind = stop & bit ? 0 : erase & bit ? 2 : record & bit ? 1 : 0;
      auto& span = writeSpans[v];
      if (writeKind && (writeKind != previousWriteKind[v] || (restart & bit) ||
                        span.base != int(start) || span.period != period)) {
        span.from = playbackPosition(v, direction);
        span.extent = 0;
        span.base = int(start);
        span.period = period;
        span.kind = writeKind;
      }
      if (writeKind) {
        span.extent = std::min(period, span.extent + step * speed);
        if (reverse) span.from = std::max(0., playbackPosition(v, direction));
      }
      previousWriteKind[v] = writeKind;

      if (size) {
        // Fully silent voices need no audio interpolation. Heads and write
        // operations still advance below, so mute never changes timing.
        Frame out;
        bool silent = ((mute | stop) & bit) && fade[v] < 1e-7f;
        if (!silent && writeKind != 2)
          out = filteredRead(
              v, playbackPosition(v, direction), start,
              speedHold.active ? length : std::min(length, int(period)),
              step * speed);
        float edge = float(std::min(head[v], period - head[v]) /
                           std::min(48., period * .25));
        edge = std::max(0.f, std::min(1.f, edge));
        float target = (mute | stop) & bit ? 0.f : 1.f;
        fade[v] += (target - fade[v]) * std::min(1.f, 200.f / sampleRate);
        if (silent) fade[v] = 0.f;
        out.l *= edge;
        out.r *= edge;
        // Monitor the operation at the moving head immediately, rather than
        // playing its stale contents until the next pass.
        if (writeKind == 2) {
          out = {};
        } else if (writeKind == 1) {
          float retained = s.overdub == 0 ? 1.f - s.recordMix : 1.f;
          out.l = std::max(-20.f, std::min(20.f, out.l * retained +
                                                     incoming.l * s.recordMix));
          out.r = std::max(-20.f, std::min(20.f, out.r * retained +
                                                     incoming.r * s.recordMix));
        }
        outputs[v].l = incoming.l * (1 - s.mix) + out.l * fade[v] * s.mix;
        outputs[v].r = incoming.r * (1 - s.mix) + out.r * fade[v] * s.mix;
        sum.l += out.l * fade[v];
        sum.r += out.r * fade[v];
        // Visit every crossed storage frame, even when playback is faster than
        // 1x.
        double end = head[v] + step * speed;
        int steps = int(std::ceil(end)) - int(head[v]);
        for (int k = 0; k < steps && !(stop & bit) && ((record | erase) & bit);
             ++k) {
          double p = int(head[v]) + k;
          if (p >= period) continue;
          double source = reverse ? std::max(0., period - 1. - p) : p;
          if (source >= length) continue;
          int index = int(start + source) % size;
          if (erase & bit) {
            audio[v][index * 2] = 0;
            audio[v][index * 2 + 1] = 0;
          } else {
            float weight = float(std::min(end, p + 1.) - std::max(head[v], p));
            float retained =
                s.overdub == 0 ? std::pow(1 - s.recordMix, weight) : 1;
            float incomingWeight =
                s.overdub == 0 ? 1 - retained : s.recordMix * weight;
            float l =
                audio[v][index * 2] * retained + incoming.l * incomingWeight;
            float r = audio[v][index * 2 + 1] * retained +
                      incoming.r * incomingWeight;
            audio[v][index * 2] = std::max(-20.f, std::min(20.f, l));
            audio[v][index * 2 + 1] = std::max(-20.f, std::min(20.f, r));
            if (isStereo) stereo[v] = true;
          }
        }
        if (!(stop & bit)) {
          double next = head[v] + step * speed;
          if (next >= period) {
            completed |= bit;
            if (!speedHold.active) activeStart[v] = requestedStart;
            if (direction == 2 && (int(std::floor(next / period)) & 1))
              returning[v] = !returning[v];
            if (direction == 2) previousWriteKind[v] = 0;
          }
          head[v] = speedHold.active
                        ? wrapPhase(speedHold.clock + speedHold.phases[v] +
                                    holdStep) *
                              period
                        : std::fmod(next, period);
        } else if (speedHold.active) {
          speedHold.phases[v] = wrapPhase(speedHold.phases[v] - holdStep);
        }
      } else {
        fade[v] = 0;
        outputs[v].l = incoming.l * (1 - s.mix);
        outputs[v].r = incoming.r * (1 - s.mix);
      }
    }
    if (speedHold.active)
      speedHold.clock = wrapPhase(speedHold.clock + holdStep);
    float gain = 1.f / std::max(1, s.count);
    Frame result;
    result.l = input.l * (1 - s.mix) + sum.l * gain * s.mix;
    result.r = input.r * (1 - s.mix) + sum.r * gain * s.mix;
    return result;
  }
};
}  // namespace phlooper
