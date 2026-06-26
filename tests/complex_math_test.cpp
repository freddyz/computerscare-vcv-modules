#include "complex/math/ComplexMath.hpp"
#include "complex/math/ComplexSimd.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {

bool almostEqual(float a, float b, float epsilon = 1e-5f) {
  return std::fabs(a - b) <= epsilon;
}

void require(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
}

void requireNear(float actual, float expected, const char* message,
                 float epsilon = 1e-5f) {
  if (!almostEqual(actual, expected, epsilon)) {
    std::fprintf(stderr, "FAIL: %s: got %.9f expected %.9f\n", message, actual,
                 expected);
    std::exit(1);
  }
}

}  // namespace

int main() {
  namespace cm = cpx::complex_math;
  constexpr float pi = 3.14159265358979323846f;

  cm::Polar oneAt45 = cm::rectToPolar(cm::Rect(1.f, 1.f));
  requireNear(oneAt45.r, std::sqrt(2.f), "rectToPolar radius");
  requireNear(oneAt45.theta, pi / 4.f, "rectToPolar theta");

  cm::Rect fromPolar = cm::polarToRect(cm::Polar(2.f, pi / 6.f));
  requireNear(fromPolar.x, std::sqrt(3.f), "polarToRect x");
  requireNear(fromPolar.y, 1.f, "polarToRect y");
  requireNear(cm::thetaRadiansToCableVoltage(pi), 5.f,
              "theta pi radians to 5 volts");
  requireNear(cm::thetaRadiansToCableVoltage(2.f * pi), 10.f,
              "theta tau radians to 10 volts");
  requireNear(cm::thetaCableVoltageToRadians(5.f), pi,
              "theta 5 volts to pi radians");

  cm::Rect product = cm::multiply(cm::Rect(1.f, 2.f), cm::Rect(3.f, 4.f));
  requireNear(product.x, -5.f, "complex multiply x");
  requireNear(product.y, 10.f, "complex multiply y");

  cm::Rect scaled = cm::scaleAndOffset(cm::Rect(2.f, 3.f),
                                       cm::Rect(4.f, -1.f),
                                       cm::Rect(0.5f, 2.f));
  requireNear(scaled.x, 11.5f, "scaleAndOffset x");
  requireNear(scaled.y, 12.f, "scaleAndOffset y");

  cm::RectChannels z = {};
  cm::RectChannels w = {};
  for (int c = 0; c < cm::maxChannels; ++c) {
    z.x[c] = static_cast<float>(c + 1);
    z.y[c] = static_cast<float>(c + 2);
    w.x[c] = static_cast<float>(2 * c + 1);
    w.y[c] = static_cast<float>(3 - c);
  }

  cm::RectChannels scalarProduct = cm::multiplyChannels(z, w);
  cm::RectChannels simdProduct = cm::simd::multiplyChannels(z, w);
  cm::RectChannels scalarSum = cm::addChannels(z, w);
  cm::RectChannels simdSum = cm::simd::addChannels(z, w);

  for (int c = 0; c < cm::maxChannels; ++c) {
    requireNear(simdProduct.x[c], scalarProduct.x[c], "SIMD product x");
    requireNear(simdProduct.y[c], scalarProduct.y[c], "SIMD product y");
    requireNear(simdSum.x[c], scalarSum.x[c], "SIMD sum x");
    requireNear(simdSum.y[c], scalarSum.y[c], "SIMD sum y");
  }

  std::puts("complex math tests passed");
  return 0;
}
