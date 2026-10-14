#pragma once
#include <memory>

#include "Spatial.hpp"
namespace windchimes {
// Fixed storage allocated at construction; routing/rate resets only invalidate
// history, never clear megabytes or allocate on the audio thread.
class Delay {
  static constexpr unsigned capacity = 524288;
  std::unique_ptr<float[]> data{new float[capacity * 4]{}};
  unsigned position = 0, mask = 0;
  std::array<unsigned, 4> valid{};
  float rate = 48000.f, mix = 0.f, feedback = .35f;
  float wantedMix = 0.f, wantedFeedback = .35f, smoothing = .002f;
  double wanted = 16800., tap = 16800., oldTap = 16800.;
  float fade = 1.f, fadeStep = .001f;
  bool clockHigh = false, haveEdge = false;
  uint64_t samples = 0, lastEdge = 0;
  double period = 0.;
  float read(int ch, double delay) const {
    if (valid[ch] < std::ceil(delay)) return 0.f;
    double index = position - delay;
    if (index < 0.) index += capacity;
    unsigned a = static_cast<unsigned>(index);
    float fraction = index - a;
    return data[ch * capacity + a] * (1.f - fraction) +
           data[ch * capacity + ((a + 1) & (capacity - 1))] * fraction;
  }

 public:
  static float seconds(float knob) {
    return .01f * std::pow(200.f, clamp(knob, 0.f, 1.f));
  }
  static float ratio(float knob) {
    const float ratios[] = {.125f, .1666667f, .25f, .3333333f, .5f,
                            .75f,  1.f,       1.5f, 2.f,       4.f};
    return ratios[static_cast<int>(std::round(clamp(knob, 0.f, 1.f) * 9.f))];
  }
  void setSampleRate(float value) {
    rate = std::max(1.f, value);
    smoothing = 1.f - std::exp(-1.f / (.01f * rate));
    fadeStep = 1.f / (.02f * rate);
    valid.fill(0);
    samples = lastEdge = 0;
    haveEdge = clockHigh = false;
    period = 0.;
    fade = 1.f;
  }
  void clock(float voltage, bool connected) {
    ++samples;
    if (!connected) {
      haveEdge = clockHigh = false;
      period = 0.;
      return;
    }
    if (voltage <= .1f) clockHigh = false;
    if (voltage >= 1.f && !clockHigh) {
      clockHigh = true;
      double elapsed = double(samples - lastEdge) / rate;
      if (haveEdge && elapsed >= .02 && elapsed <= 20.) period = elapsed;
      if (!haveEdge || elapsed >= .02) {
        lastEdge = samples;
        haveEdge = true;
      }
    }
  }
  bool synced() const { return period > 0.; }
  void configure(float knob, float wet, float regeneration) {
    double duration = synced() ? period * ratio(knob) : seconds(knob);
    wanted = std::max(
        2., std::min(double(capacity - 2), std::min(2., duration) * rate));
    wantedMix = clamp(wet, 0.f, 1.f);
    wantedFeedback = clamp(regeneration, 0.f, .95f);
  }
  Quad process(Quad input, unsigned outputs) {
    outputs &= 15u;
    if (outputs != mask) {
      mask = outputs;
      valid.fill(0);
    }
    if (!mask) return {};
    if (!valid[0] && !valid[1] && !valid[2] && !valid[3]) {
      tap = oldTap = wanted;
      fade = 1.f;
      mix = wantedMix;
      feedback = wantedFeedback;
    }
    if (fade >= 1.f && std::fabs(wanted - tap) > .5) {
      oldTap = tap;
      tap = wanted;
      fade = 0.f;
    }
    mix += (wantedMix - mix) * smoothing;
    feedback += (wantedFeedback - feedback) * smoothing;
    Quad result;
    for (int ch = 0; ch < 4; ++ch)
      if (mask & (1u << ch)) {
        float echo = read(ch, tap);
        if (fade < 1.f) echo = read(ch, oldTap) * (1.f - fade) + echo * fade;
        float dry = std::isfinite(input.channel[ch]) ? input.channel[ch] : 0.f;
        float write = dry + feedback * echo;
        // Bound pathological excitation without changing normal-level echoes.
        data[ch * capacity + position] = clamp(write, -8.f, 8.f);
        result.channel[ch] = dry * (1.f - mix) + echo * mix;
        valid[ch] = std::min(valid[ch] + 1, capacity - 2);
      }
    fade = std::min(1.f, fade + fadeStep);
    position = (position + 1) & (capacity - 1);
    return result;
  }
};
}  // namespace windchimes
