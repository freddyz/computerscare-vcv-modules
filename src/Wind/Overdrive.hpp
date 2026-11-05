#pragma once
#include "Types.hpp"

namespace computerscare {
namespace wind {
// First-order antiderivative antialiasing averages the smooth clipping curve
// over each input segment, reducing folded harmonics without oversampling.
class Overdrive {
  double previous = 0., previousOutput = 0., dc = 0.;
  float rate = 0.f;
  double dcPole = 0.;
  static double primitive(double x) {
    double a = std::abs(x);
    return a < 1.5 ? x * x * .5 - x * x * x * x / 27. : a - .5625;
  }

 public:
  static double curve(double x) {
    if (x <= -1.5) return -1.;
    if (x >= 1.5) return 1.;
    return x * (1. - 4. * x * x / 27.);
  }
  void reset() { previous = previousOutput = dc = 0.; }
  float process(float sample, float amount, float sampleRate) {
    float nextRate = std::max(8000.f, sampleRate);
    if (rate != nextRate) {
      rate = nextRate;
      dcPole = std::exp(-2. * pi * 5. / rate);
      reset();
    }
    if (!std::isfinite(sample)) {
      reset();
      return 0.f;
    }
    double a = clamp(amount, 0.f, 1.f);
    double drive = 1. + 8. * a * a;
    double x = double(sample) * drive;
    double delta = x - previous;
    double shaped = std::abs(delta) > 1e-5
                        ? (primitive(x) - primitive(previous)) / delta
                        : curve((x + previous) * .5);
    previous = x;
    // Static compensation avoids an envelope follower pumping with the wind.
    shaped /= std::sqrt(drive);
    dc = shaped - previousOutput + dcPole * dc;
    previousOutput = shaped;
    if (std::abs(dc) < 1e-15) dc = 0.;
    // Gentle output protection also bounds the short DC-blocker transient.
    return float(std::tanh(dc));
  }
};
}  // namespace wind
}  // namespace computerscare
