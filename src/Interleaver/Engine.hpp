#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace interleaver {
enum Operation { Zero, Half, Equal, Weave, Repeat, Reverse, Direct };
enum Routing { Cycles, Gate, Trigger, Timer };
struct Settings {
  int timing = 0, a = 1, b = 1, operation = Zero, routing = Cycles;
  float buffer = 40, join = .15f, balance = .5f, period = 100;
};
// CV is additive. +10 V at full depth spans the parameter's entire range.
inline float cv(float base, float voltage, float depth, float low, float high) {
  if (!std::isfinite(base)) base = low;
  if (!std::isfinite(voltage)) voltage = 0;
  if (!std::isfinite(depth)) depth = 0;
  voltage = std::max(-10.f, std::min(voltage, 10.f));
  depth = std::max(-1.f, std::min(depth, 1.f));
  return std::max(low,
                  std::min(base + voltage * depth * .1f * (high - low), high));
}
struct Segment {
  double start, end;
  bool positive;
  Segment(double a = 0, double b = 0, bool p = true)
      : start(a), end(b), positive(p) {}
};
class Engine {
  static constexpr uint64_t sampleCapacity = 262144;
  static constexpr uint64_t segmentCapacity = 8192;
  static constexpr uint64_t invalid = std::numeric_limits<uint64_t>::max();
  struct Source {
    std::array<float, sampleCapacity> samples{};
    std::array<Segment, segmentCapacity> cycles{}, halves{};
    uint64_t next = 0, halfNext = 0;
    double crossing = -1, halfCrossing = -1;
    float previous = 0, dcInput = 0, dcOutput = 0;
    bool armed = false, positiveArmed = false;
    void reset() {
      next = halfNext = 0;
      crossing = halfCrossing = -1;
      previous = dcInput = dcOutput = 0;
      armed = positiveArmed = false;
    }
    uint64_t count(bool half) const { return half ? halfNext : next; }
    uint64_t oldest(bool half) const {
      uint64_t n = count(half);
      return n > segmentCapacity ? n - segmentCapacity : 0;
    }
    bool get(uint64_t seq, Segment& s, bool half) const {
      if (seq < oldest(half) || seq >= count(half)) return false;
      s = (half ? halves : cycles)[seq % segmentCapacity];
      return true;
    }
    uint64_t locate(double time, bool half) const {
      uint64_t lo = oldest(half), hi = count(half);
      while (lo < hi) {
        uint64_t mid = lo + (hi - lo) / 2;
        if ((half ? halves : cycles)[mid % segmentCapacity].end <= time)
          lo = mid + 1;
        else
          hi = mid;
      }
      return lo;
    }
    float delayed(double position, uint64_t now) const {
      if (position < 0 || position > now ||
          now - position >= sampleCapacity - 2)
        return 0;
      uint64_t n = static_cast<uint64_t>(position);
      float t = static_cast<float>(position - n);
      float a = samples[n % sampleCapacity],
            b = samples[std::min(n + 1, now) % sampleCapacity];
      return a + (b - a) * t;
    }
    void capture(float input, uint64_t now, float pole, float rate) {
      if (!std::isfinite(input)) input = 0;
      input = std::max(-1000.f, std::min(input, 1000.f));
      float v = input - dcInput + pole * dcOutput;
      dcInput = input;
      dcOutput = v;
      samples[now % sampleCapacity] = v;
      if (v < -.0001f) armed = true;
      if (v > .0001f) positiveArmed = true;
      bool rising = armed && previous < 0 && v >= 0;
      bool falling = positiveArmed && previous > 0 && v <= 0;
      if (rising || falling) {
        double at = double(now) - 1 - previous / (v - previous);
        double minimum = std::max(2.f, rate / 20000.f);
        if (halfCrossing >= 0 && at - halfCrossing >= minimum &&
            at - halfCrossing <= rate * .2f)
          halves[halfNext++ % segmentCapacity] =
              Segment(halfCrossing, at, falling);
        halfCrossing = at;
        if (rising) {
          if (crossing >= 0 && at - crossing >= minimum &&
              at - crossing <= rate * .2f)
            cycles[next++ % segmentCapacity] = Segment(crossing, at);
          crossing = at;
          armed = false;
        } else
          positiveArmed = false;
      }
      previous = v;
    }
  };
  std::array<Source, 2> inputs;
  Settings desired;
  uint64_t now = 0, anchorSequence = invalid;
  std::array<uint64_t, 2> freeCursor{};
  float rate = 48000, pole = .9997f, join = .15f, balance = .5f, period = 100;
  double delay = 1920, targetDelay = 1920, played = 0, timerElapsed = 0,
         weaveError = 0;
  int timing = 0, operation = Zero, routing = Cycles, lane = 0, run = 0,
      playing = -1;
  int requestedSource = 0, heldSource = -1, previousClockSource = -1;
  Segment playback, anchorSlot, held;
  bool haveFree = false, gateHigh = false, resetHigh = false;
  float last = 0, transitionFrom = 0, outputInput = 0, outputDC = 0;
  float previousEqualDifference = 0, previousClockValue = 0, equalMix = 0;
  int transition = 0, transitionLength = 1, equalWait = 0, pendingAge = 0;
  bool half() const { return operation == Half; }
  void fade() {
    transitionFrom = last;
    transitionLength = std::max(1, int(rate * .002f));
    transition = transitionLength;
  }
  void restart() {
    anchorSequence = invalid;
    lane = run = 0;
    playing = -1;
    haveFree = false;
    played = 0;
    freeCursor = {{0, 0}};
    heldSource = -1;
    timerElapsed = 0;
    weaveError = 0;
    equalWait = 0;
    previousClockSource = -1;
    previousEqualDifference = previousClockValue = equalMix = 0;
    fade();
  }
  bool selectorsPending() const {
    return timing != desired.timing || operation != desired.operation ||
           routing != desired.routing;
  }
  void commit() {
    timing = desired.timing;
    operation = desired.operation;
    routing = desired.routing;
    requestedSource = routing == Gate && gateHigh ? 1 : 0;
    restart();
    pendingAge = 0;
  }
  bool usable(const Segment& s) const {
    return s.start >= 0 && s.end > s.start && s.end <= now &&
           now - s.start < sampleCapacity - 2;
  }
  bool canRepeat(const Segment& s, double duration) const {
    // Refresh a held cycle before its samples can wrap during the next
    // playback.
    return usable(s) && now - s.start + duration * 2 < sampleCapacity - 2;
  }
  void advance() {
    if (routing != Cycles) {
      lane = requestedSource;
      run = 0;
      return;
    }
    if (operation == Weave) {
      weaveError += balance;
      lane = weaveError >= 1 ? 1 : 0;
      if (lane) weaveError -= 1;
    } else if (++run >= (lane ? desired.b : desired.a)) {
      lane = 1 - lane;
      run = 0;
    }
  }
  bool otherCycle(int source, const Segment& slot, Segment& best) const {
    const auto& in = inputs[source];
    double duration = slot.end - slot.start, error = 1e30;
    bool found = false;
    // Bounded metadata search, independent of the audio buffer's size.
    for (uint64_t n = in.count(half()), count = 0;
         n > in.oldest(half()) && count < 16; ++count) {
      Segment s;
      in.get(--n, s, half());
      double length = s.end - s.start;
      if (!usable(s) || now - s.end > delay || length < duration * .125 ||
          length > duration * 8 || (half() && s.positive != slot.positive))
        continue;
      double e = length > duration ? length / duration : duration / length;
      if (e < error) {
        error = e;
        best = s;
        found = true;
      }
    }
    return found;
  }
  float render(int source, const Segment& s, double fraction) const {
    if (fraction <= 0 || fraction >= 1) return 0;
    double position = operation == Reverse ? 1 - fraction : fraction;
    float v =
        inputs[source].delayed(s.start + (s.end - s.start) * position, now);
    double edge =
        std::min(double(join) * rate * .001, (s.end - s.start) * .125);
    if (edge > 0) {
      float t = static_cast<float>(std::min(
          std::min(fraction, 1 - fraction) * (s.end - s.start) / edge, 1.0));
      v *= t * t * (3 - 2 * t);
    }
    return v;
  }
  bool slotAt(int source, double clock, Segment& slot, uint64_t& seq) const {
    slot = anchorSlot;
    seq = anchorSequence;
    if (seq == invalid || clock < slot.start || clock >= slot.end) {
      seq = inputs[source].locate(clock, half());
      if (!inputs[source].get(seq, slot, half())) return false;
    }
    return usable(slot) && slot.start <= clock && clock < slot.end;
  }
  void remember() {
    if (operation == Repeat) {
      held = playback;
      heldSource = playing;
    }
  }
  float anchored(double clock) {
    Segment slot;
    uint64_t seq;
    if (!slotAt(timing, clock, slot, seq)) {
      int fallback = routing == Cycles ? timing : requestedSource;
      if (playing != fallback) {
        playing = fallback;
        fade();
      }
      return inputs[fallback].delayed(clock, now);
    }
    if (seq != anchorSequence) {
      if (selectorsPending()) {
        commit();
        return 0;
      }
      if (anchorSequence != invalid)
        advance();
      else if (routing != Cycles)
        lane = requestedSource;
      else if (operation == Weave)
        advance();
      anchorSequence = seq;
      anchorSlot = slot;
      playing = lane;
      if (operation == Repeat && heldSource == playing &&
          canRepeat(held, slot.end - slot.start))
        playback = held;
      else {
        if (playing == timing)
          playback = slot;
        else if (!otherCycle(playing, slot, playback)) {
          playing = timing;
          playback = slot;
        }
        remember();
      }
    }
    if (!usable(playback)) return inputs[timing].delayed(clock, now);
    return render(playing, playback,
                  (clock - slot.start) / (slot.end - slot.start));
  }
  bool takeFree(int source, double clock) {
    if (operation == Repeat && heldSource == source &&
        canRepeat(held, held.end - held.start)) {
      playback = held;
      playing = source;
      played = 0;
      haveFree = true;
      return true;
    }
    auto& in = inputs[source];
    uint64_t earliest = in.locate(clock, half());
    uint64_t seq = std::max(freeCursor[source], earliest);
    if (seq >= in.count(half()) && in.count(half()) > in.oldest(half()))
      seq = in.count(half()) - 1;
    Segment s;
    if (!in.get(seq, s, half()) || !usable(s) || now - s.end > delay)
      return false;
    freeCursor[source] = seq + 1;
    playing = source;
    playback = s;
    played = 0;
    haveFree = true;
    remember();
    return true;
  }
  float free(double clock) {
    if (haveFree && played >= playback.end - playback.start) {
      if (selectorsPending()) {
        commit();
        return 0;
      }
      haveFree = false;
      advance();
    }
    if (!haveFree) {
      if (routing != Cycles)
        lane = requestedSource;
      else if (operation == Weave && playing < 0)
        advance();
      if (!takeFree(lane, clock) && !takeFree(1 - lane, clock)) {
        playing = -1;
        return 0;
      }
    }
    if (!usable(playback)) {
      haveFree = false;
      playing = -1;
      return 0;
    }
    float v =
        render(playing, playback, played / (playback.end - playback.start));
    played += 1;
    return v;
  }
  float continuous(double clock) {
    // Continuous delayed signals; request changes on the chosen clock's rising
    // crossings.
    int clockSource = timing < 2 ? timing : std::max(0, playing);
    float clockValue = inputs[clockSource].delayed(clock, now);
    if (previousClockSource != clockSource) {
      previousClockValue = inputs[clockSource].delayed(clock - 1, now);
      previousClockSource = clockSource;
    }
    bool boundary = previousClockValue < 0 && clockValue >= 0;
    previousClockValue = clockValue;
    if (boundary) {
      if (selectorsPending()) {
        commit();
        return 0;
      }
      advance();
    }
    if (routing != Cycles) lane = requestedSource;
    if (playing < 0) {
      playing = lane;
      equalMix = float(lane);
    }
    float a = inputs[0].delayed(clock, now), b = inputs[1].delayed(clock, now);
    float difference = a - b;
    if (lane != playing) {
      if (operation == Direct)
        playing = lane;
      else {
        ++equalWait;
        bool intersection = std::fabs(difference) < .001f ||
                            difference * previousEqualDifference < 0;
        float slopeA = a - inputs[0].delayed(clock - 1, now);
        float slopeB = b - inputs[1].delayed(clock - 1, now);
        bool compatible = std::fabs(slopeA - slopeB) < .05f;
        if ((intersection && compatible) || equalWait >= int(rate * .02f)) {
          playing = lane;
          equalWait = 0;
        }
      }
    } else
      equalWait = 0;
    // A continuous mix ramp also protects a gate that reverses during a fade.
    float step = 1.f / (rate * .002f);
    equalMix += std::max(-step, std::min(float(playing) - equalMix, step));
    previousEqualDifference = difference;
    return a + (b - a) * equalMix;
  }
  static float finiteClamp(float v, float lo, float hi) {
    return std::isfinite(v) ? std::max(lo, std::min(v, hi)) : lo;
  }

 public:
  void reset() {
    for (auto& in : inputs) in.reset();
    now = 0;
    last = outputInput = outputDC = 0;
    gateHigh = resetHigh = false;
    requestedSource = 0;
    restart();
  }
  void setSampleRate(float value) {
    value = finiteClamp(value, 8000, 768000);
    if (value == rate && now > 0) return;
    rate = value;
    pole = std::exp(-2.f * 3.14159265358979323846f * 2.f / rate);
    delay = targetDelay = desired.buffer * rate * .001;
    reset();
  }
  void configure(const Settings& s) {
    desired = s;
    desired.timing = std::max(0, std::min(s.timing, 2));
    desired.operation = std::max(0, std::min(s.operation, 6));
    desired.routing = std::max(0, std::min(s.routing, 3));
    desired.a = std::max(1, std::min(s.a, 8));
    desired.b = std::max(1, std::min(s.b, 8));
    desired.buffer = finiteClamp(s.buffer, 20, 200);
    desired.join = finiteClamp(s.join, 0, 1);
    desired.balance = finiteClamp(s.balance, 0, 1);
    desired.period = finiteClamp(s.period, 1, 2000);
    targetDelay = std::min(double(desired.buffer) * rate * .001,
                           double(sampleCapacity - 4));
    if (now == 0) {
      delay = targetDelay;
      join = desired.join;
      balance = desired.balance;
      period = desired.period;
      commit();
    }
  }
  void configure(int mode, int a, int b, float milliseconds, float smoothing) {
    Settings s;
    s.timing = mode;
    s.a = a;
    s.b = b;
    s.buffer = milliseconds;
    s.join = smoothing;
    configure(s);
  }
  float process(float a, float b, float switchVoltage = 0,
                float resetVoltage = 0) {
    inputs[0].capture(a, now, pole, rate);
    inputs[1].capture(b, now, pole, rate);
    bool rising = false;
    if (!gateHigh && std::isfinite(switchVoltage) && switchVoltage >= 1) {
      gateHigh = true;
      rising = true;
    } else if (gateHigh &&
               (!std::isfinite(switchVoltage) || switchVoltage <= .1f))
      gateHigh = false;
    bool resetEdge = false;
    if (!resetHigh && std::isfinite(resetVoltage) && resetVoltage >= 1) {
      resetHigh = true;
      resetEdge = true;
    } else if (resetHigh &&
               (!std::isfinite(resetVoltage) || resetVoltage <= .1f))
      resetHigh = false;
    if (resetEdge) {
      requestedSource = 0;
      restart();
    }
    if (routing == Gate) requestedSource = gateHigh ? 1 : 0;
    if (routing == Trigger && rising) requestedSource = 1 - requestedSource;
    // Monotonic read clock: buffer CV cannot reverse playback or jump across
    // history.
    double step = (targetDelay - delay) / (rate * .02);
    delay += std::max(-.125, std::min(step, .125));
    float smoothing = 1.f / (rate * .01f);
    join += (desired.join - join) * smoothing;
    balance += (desired.balance - balance) * smoothing;
    period += (desired.period - period) / (rate * .02f);
    double clock = double(now) - delay;
    if (clock >= 0 && routing == Timer) {
      double duration =
          period * rate * .001 * (requestedSource ? desired.b : desired.a);
      timerElapsed += 1 / duration;
      if (timerElapsed >= 1) {
        timerElapsed -= 1;
        requestedSource = 1 - requestedSource;
      }
    }
    if (selectorsPending()) {
      // Silence/very slow cycles cannot strand a pending selector change.
      if (++pendingAge >= int(rate * .02f)) commit();
    } else
      pendingAge = 0;
    float out = clock >= 0 ? ((operation == Equal || operation == Direct)
                                  ? continuous(clock)
                              : timing < 2 ? anchored(clock)
                                           : free(clock))
                           : 0;
    if (transition > 0 && clock >= 0) {
      float t = 1.f - float(--transition) / transitionLength;
      out = transitionFrom * (1 - t) + out * t;
    }
    if (!std::isfinite(out)) out = 0;
    last = out;
    // Half-cycle selection and unequal cycle areas can produce DC.
    float filtered = out - outputInput + pole * outputDC;
    outputInput = out;
    outputDC = filtered;
    ++now;
    return std::isfinite(filtered) ? filtered : 0;
  }
  int activeSource() const { return playing; }
  int activeTiming() const { return timing; }
  int activeOperation() const { return operation; }
  int activeRouting() const { return routing; }
  double bufferSamples() const { return delay; }
  uint64_t capturedCycles(int source) const { return inputs[source].next; }
  uint64_t capturedHalves(int source) const { return inputs[source].halfNext; }
};
}  // namespace interleaver
