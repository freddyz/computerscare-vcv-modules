#pragma once
#include "Types.hpp"
namespace windchimes {
// Original reduced viscoelastic object model. Complex-modulus loss gives
// amplitude decay rate pi*f*eta; geometry controls the mode families
// separately. Ratios are sound-design approximations, not measured polymer
// identities.
struct PlasticModel {
  struct Modal {
    float frequency, seconds, gain;
  };
  static float profile(const float* values, float shape) {
    float p = clamp(shape, 0.f, 1.f) * 3.f;
    int index = std::min(static_cast<int>(p), 2);
    return values[index] + (values[index + 1] - values[index]) * (p - index);
  }
  static Modal mode(int n, float hz, const SetConfig& c) {
    static const float ratios[6][4] = {{1.f, 1.f, 1.f, 1.f},
                                       {2.756f, 1.59f, 2.05f, 2.58f},
                                       {5.404f, 2.47f, 3.42f, 4.79f},
                                       {8.933f, 3.66f, 5.11f, 7.52f},
                                       {13.344f, 5.12f, 7.31f, 10.66f},
                                       {18.64f, 6.93f, 9.76f, 14.15f}};
    float sustain = c.decay <= .85f
                        ? .045f * std::pow(2.f / .045f, c.decay / .85f)
                        : 2.f * std::pow(8.f / 2.f, (c.decay - .85f) / .15f);
    float ratio = 1.f, gain = 0.f, loss = 0.f;
    if (n < 6) {
      float natural = profile(ratios[n], c.shape);
      float morph = c.inharmonicity <= .5f
                        ? 2.f * c.inharmonicity
                        : 1.f + .35f * (c.inharmonicity - .5f);
      ratio = (n + 1.f) * std::pow(natural / (n + 1.f), morph);
      static const float weights[] = {.8f, .55f, .4f, .28f, .17f, .1f};
      gain = weights[n] * std::pow(ratio, -.95f * (1.f - c.brightness));
      // Distributed loss with mild dispersion: unlike wood's steep upper loss,
      // rigid plastic retains a round body and a short, bright wall response.
      float eta = 6.907755f / (pi * 220.f * sustain);
      loss = pi * hz * ratio * eta * std::pow(ratio, .12f);
    } else if (n < 10) {
      static const float wall[] = {1.34f, 1.92f, 3.17f, 4.63f};
      ratio = wall[n - 6] * (1.f + .06f * (c.inharmonicity - .5f) * (n - 6));
      float shell = std::sin(pi * c.shape * .85f);
      gain =
          shell * shell * (.25f - .045f * (n - 6)) * (.4f + .6f * c.brightness);
      loss = 6.907755f / (.022f + .14f * c.body + .07f * c.decay) *
             (1.f + .4f * (n - 6));
    } else {
      ratio = n == 10 ? 1.18f : 2.36f;
      float hollow = clamp((c.shape - .45f) / .55f, 0.f, 1.f);
      gain = hollow * (n == 10 ? .22f : .07f);
      loss = 6.907755f / (.025f + .09f * c.body);
    }
    // Even a dry tap is a damped object, never independently mixed noise.
    loss += 28.f * std::pow(1.f - c.body, 2.f) * (1.f + .12f * (ratio - 1.f));
    return {hz * ratio, 6.907755f / loss, gain};
  }
  static float contactDuration(float hz, float hardness, float shape,
                               float velocity, bool tubeContact) {
    float firmness = tubeContact ? .7f - .2f * shape : hardness;
    float duration = .00022f + .0032f * std::pow(1.f - firmness, 2.f);
    return duration * clamp(std::sqrt(220.f / hz), .55f, 1.8f) *
           std::pow(clamp(velocity, .03f, 1.f), -.2f);
  }
  static float localShape(int n, float location) {
    float x = clamp(location, 0.f, 1.f);
    return n < 10 ? std::cos(pi * (n - 5.f) * x) : .6f + .4f * std::sin(pi * x);
  }
};
}  // namespace windchimes
