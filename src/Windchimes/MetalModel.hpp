#pragma once
#include "Types.hpp"
namespace windchimes {
// Research-informed reduced tube model, not a measured alloy calibration.
// Lukkari/Valimaki (NORSIG 2004): bent-tube ratios compress relative to ideal
// Euler-Bernoulli modes, and upper-mode T60 falls steeply (40/7/2/1/.5 s).
// Six two-axis bending pairs use the existing twelve resonator slots.
struct MetalModel {
  struct Modal {
    float frequency, seconds, gain;
  };
  static Modal mode(int slot, float hz, const SetConfig& c) {
    int n = slot % 6;
    bool partner = slot >= 6;
    static const float beam[] = {1.f, 2.756f, 5.404f, 8.933f, 13.344f, 18.64f};
    // A bounded shear/rotary-inertia approximation. Keep a bending spectrum at
    // both knob extremes, rather than turning tubes into strings or bells.
    float correction =
        (.03f + .035f * (1.f - c.shape)) * std::pow(4.f, .5f - c.inharmonicity);
    float ratio =
        beam[n] * std::sqrt((1.f + correction) / (1.f + correction * beam[n]));
    float f = hz * ratio;
    float split = hz * (.00025f + .002f * c.inharmonicity * c.inharmonicity) *
                  (.35f + .65f * c.shape) * std::sqrt(ratio);
    if (partner) f += split;
    float t60 = .12f * std::pow(40.f / .12f, c.decay);
    // Separate support loss from frequency-dependent internal loss. Multiplying
    // all loss by ratio^1.8 made moderate Decay settings sound wooden.
    // Upper modes retain audible ring; Body adds gentle distributed muting.
    float modalLoss = 6.907755f / t60 * (1.f + .12f * (ratio - 1.f));
    float intrinsic =
        .18f * std::pow(ratio - 1.f, 1.4f) * std::sqrt(hz / 220.f);
    float mute =
        7.f * std::pow(1.f - c.body, 2.f) * (1.f + .08f * (ratio - 1.f));
    float seconds = 6.907755f / (modalLoss + intrinsic + mute);
    static const float weights[] = {.65f, .8f, .75f, .5f, .33f, .22f};
    float gain =
        weights[n] * std::pow(ratio, -(.3f + .6f * (1.f - c.brightness)));
    // Brightness alters relative radiation, without boosting noise at low Body.
    float pair = .18f + .2f * c.shape;
    gain *= partner ? pair / (1.f + pair) : 1.f / (1.f + pair);
    return {f, seconds * (partner ? .94f : 1.f), gain};
  }
  static float contactDuration(float hz, float hardness, float velocity,
                               bool tubeContact) {
    float duration =
        tubeContact ? .00008f
                    : .00008f + .0012f * (1.f - hardness) * (1.f - hardness);
    return duration * clamp(std::sqrt(220.f / hz), .55f, 1.8f) *
           std::pow(clamp(velocity, .03f, 1.f), -.2f);
  }
};
}  // namespace windchimes
