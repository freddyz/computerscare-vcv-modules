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

  cm::Rect product = cm::multiply(cm::Rect(1.f, 2.f), cm::Rect(3.f, 4.f));
  requireNear(product.x, -5.f, "complex multiply x");
  requireNear(product.y, 10.f, "complex multiply y");

  cm::Rect scaled = cm::scaleAndOffset(cm::Rect(2.f, 3.f),
                                       cm::Rect(4.f, -1.f),
                                       cm::Rect(0.5f, 2.f));
  requireNear(scaled.x, 11.5f, "scaleAndOffset x");
  requireNear(scaled.y, 12.f, "scaleAndOffset y");

  require(cm::compolyphonyForInput(cm::CoordinateMode::RectInterleaved, 3, 2) ==
              3,
          "interleaved compolyphony rounds up combined channel pairs");
  require(cm::compolyphonyForInput(cm::CoordinateMode::RectSeparated, 3, 2) ==
              3,
          "separated compolyphony uses max port channels");

  require(cm::channelIndexForOutput(4, cm::WrapMode::Normal, 1) == 0,
          "normal wrap copies mono channel");
  require(cm::channelIndexForOutput(4, cm::WrapMode::Cycle, 3) == 1,
          "cycle wrap repeats channels");
  require(cm::channelIndexForOutput(4, cm::WrapMode::Minimal, 3) == 4,
          "minimal wrap leaves channel index unchanged");
  require(cm::channelIndexForOutput(4, cm::WrapMode::Stall, 3) == 2,
          "stall wrap holds final channel");

  cm::PortChannelCounts interleavedCounts =
      cm::outputPortChannelCounts(cm::CoordinateMode::RectInterleaved, 9);
  require(interleavedCounts.a == 16 && interleavedCounts.b == 2,
          "interleaved output channel counts split after 16 lanes");

  cm::PortChannels rectInterleaved = {};
  for (int c = 0; c < cm::maxChannels; ++c) {
    if (c < 8) {
      rectInterleaved.a[2 * c] = static_cast<float>(10 + c);
      rectInterleaved.a[2 * c + 1] = static_cast<float>(20 + c);
    } else {
      rectInterleaved.b[(2 * c) % cm::maxChannels] = static_cast<float>(10 + c);
      rectInterleaved.b[(2 * c + 1) % cm::maxChannels] =
          static_cast<float>(20 + c);
    }
  }

  cm::RectChannels rect =
      cm::readRectFromPorts(rectInterleaved, cm::CoordinateMode::RectInterleaved);
  for (int c = 0; c < cm::maxChannels; ++c) {
    requireNear(rect.x[c], static_cast<float>(10 + c),
                "readRectFromPorts interleaved x");
    requireNear(rect.y[c], static_cast<float>(20 + c),
                "readRectFromPorts interleaved y");
  }

  cm::PortChannels packed =
      cm::writePortsFromRect(rect, cm::CoordinateMode::RectInterleaved);
  for (int c = 0; c < cm::maxChannels; ++c) {
    int index = (2 * c) % cm::maxChannels;
    const cm::Channels& a = c < 8 ? packed.a : packed.b;
    requireNear(a[index], static_cast<float>(10 + c),
                "writePortsFromRect interleaved x");
    requireNear(a[index + 1], static_cast<float>(20 + c),
                "writePortsFromRect interleaved y");
  }

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
