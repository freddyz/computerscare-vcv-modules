#include "complex/CompolyPortMapping.hpp"

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
  namespace cp = cpx::compoly;
  constexpr float pi = 3.14159265358979323846f;

  require(cp::compolyLanesForInput(cm::CoordinateMode::RectangularInterleaved,
                                   3, 2) == 3,
          "interleaved compolyphony rounds up combined channel pairs");
  require(cp::compolyLanesForInput(cm::CoordinateMode::RectangularSeparated, 3,
                                   2) == 3,
          "separated compolyphony uses max port channels");

  cp::PortChannelCounts interleavedCounts =
      cp::outputPortChannelCounts(cm::CoordinateMode::RectangularInterleaved,
                                  9);
  require(interleavedCounts.a == 16 && interleavedCounts.b == 2,
          "interleaved output channel counts split after 16 lanes");

  cp::PortChannels rectInterleaved = {};
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
      cp::readRectFromPorts(rectInterleaved,
                            cm::CoordinateMode::RectangularInterleaved);
  for (int c = 0; c < cm::maxChannels; ++c) {
    requireNear(rect.x[c], static_cast<float>(10 + c),
                "readRectFromPorts interleaved x");
    requireNear(rect.y[c], static_cast<float>(20 + c),
                "readRectFromPorts interleaved y");
  }

  cp::PortChannels packed =
      cp::writePortsFromRect(rect,
                             cm::CoordinateMode::RectangularInterleaved);
  for (int c = 0; c < cm::maxChannels; ++c) {
    int index = (2 * c) % cm::maxChannels;
    const cm::Channels& a = c < 8 ? packed.a : packed.b;
    requireNear(a[index], static_cast<float>(10 + c),
                "writePortsFromRect interleaved x");
    requireNear(a[index + 1], static_cast<float>(20 + c),
                "writePortsFromRect interleaved y");
  }

  cm::RectChannels polarSource = {};
  polarSource.x[0] = 0.f;
  polarSource.y[0] = 1.f;
  cp::PortChannels polarPacked =
      cp::writePortsFromRect(polarSource, cm::CoordinateMode::PolarSeparated);
  requireNear(polarPacked.a[0], 1.f, "polar output radius");
  requireNear(polarPacked.b[0], 2.5f, "polar output theta volts");

  cp::PortChannels polarInput = {};
  polarInput.a[0] = 1.f;
  polarInput.b[0] = 2.5f;
  cm::RectChannels polarRect =
      cp::readRectFromPorts(polarInput, cm::CoordinateMode::PolarSeparated);
  requireNear(polarRect.x[0], 0.f, "polar input x");
  requireNear(polarRect.y[0], 1.f, "polar input y");

  cp::PortChannels wrappedSeparated = {};
  wrappedSeparated.a[0] = 1.f;
  wrappedSeparated.a[1] = 2.f;
  wrappedSeparated.a[2] = 3.f;
  wrappedSeparated.b[0] = 10.f;
  wrappedSeparated.b[1] = 20.f;
  wrappedSeparated.b[2] = 30.f;
  cm::RectChannels wrappedRect = cp::readWrappedSeparatedInputToRect(
      wrappedSeparated, cp::PortChannelCounts(2, 3),
      cm::CoordinateMode::RectangularSeparated,
      cpx::polyphonic::MappingMode::Cycle, 5,
      cp::CoordinatePairTransform(2.f, 0.5f, 1.f, -1.f));
  requireNear(wrappedRect.x[0], 2.5f, "wrapped separated rect x0");
  requireNear(wrappedRect.x[2], 2.5f, "wrapped separated rect cycles x");
  requireNear(wrappedRect.y[4], 19.f, "wrapped separated rect cycles y");

  cm::RectChannels genericWrappedRect = cp::readWrappedInputToRect(
      wrappedSeparated, cp::PortChannelCounts(3, 3),
      cm::CoordinateMode::RectangularSeparated,
      cpx::polyphonic::MappingMode::Stall, 5);
  requireNear(genericWrappedRect.x[0], 1.f,
              "generic wrapped separated rect x0");
  requireNear(genericWrappedRect.x[4], 3.f,
              "generic wrapped separated stalls x");
  requireNear(genericWrappedRect.y[4], 30.f,
              "generic wrapped separated stalls y");

  cp::PortChannels monoSeparated = {};
  monoSeparated.a[0] = 7.f;
  monoSeparated.b[0] = 70.f;
  cm::RectChannels standardMonoRect = cp::readWrappedInputToRect(
      monoSeparated, cp::PortChannelCounts(1, 1),
      cm::CoordinateMode::RectangularSeparated,
      cpx::polyphonic::MappingMode::Normal, 4);
  requireNear(standardMonoRect.x[0], 7.f,
              "standard wrapped separated reads mono x");
  requireNear(standardMonoRect.x[3], 7.f,
              "standard wrapped separated spreads one lane x");
  requireNear(standardMonoRect.y[3], 70.f,
              "standard wrapped separated spreads one lane y");

  cm::RectChannels standardPolyRect = cp::readWrappedInputToRect(
      wrappedSeparated, cp::PortChannelCounts(3, 3),
      cm::CoordinateMode::RectangularSeparated,
      cpx::polyphonic::MappingMode::Normal, 5);
  requireNear(standardPolyRect.x[2], 3.f,
              "standard wrapped separated matches existing lanes");
  requireNear(standardPolyRect.x[3], 0.f,
              "standard wrapped separated zeroes after input lanes x");
  requireNear(standardPolyRect.y[3], 0.f,
              "standard wrapped separated zeroes after input lanes y");

  cm::RectChannels genericZeroPadRect = cp::readWrappedInputToRect(
      wrappedSeparated, cp::PortChannelCounts(2, 3),
      cm::CoordinateMode::RectangularSeparated,
      cpx::polyphonic::MappingMode::PadWithZero, 5);
  requireNear(genericZeroPadRect.x[4], 0.f,
              "generic wrapped separated zero-pads x");
  requireNear(genericZeroPadRect.y[4], 0.f,
              "generic wrapped separated zero-pads y");

  cp::PortChannels shortInterleaved = {};
  shortInterleaved.a[0] = 1.f;
  shortInterleaved.a[1] = 10.f;
  shortInterleaved.a[2] = 2.f;
  shortInterleaved.a[3] = 20.f;
  cm::RectChannels cycledInterleaved = cp::readWrappedInputToRect(
      shortInterleaved, cp::PortChannelCounts(4, 0),
      cm::CoordinateMode::RectangularInterleaved,
      cpx::polyphonic::MappingMode::Cycle, 5);
  requireNear(cycledInterleaved.x[0], 1.f,
              "generic wrapped interleaved reads lane 0 x");
  requireNear(cycledInterleaved.y[1], 20.f,
              "generic wrapped interleaved reads lane 1 y");
  requireNear(cycledInterleaved.x[2], 1.f,
              "generic wrapped interleaved cycles lanes");

  cp::PortChannels wrappedPolar = {};
  wrappedPolar.a[0] = 2.f;
  wrappedPolar.a[1] = 4.f;
  wrappedPolar.b[0] = 0.f;
  wrappedPolar.b[1] = 2.5f;
  cm::RectChannels polarWrappedRect = cp::readWrappedSeparatedInputToRect(
      wrappedPolar, cp::PortChannelCounts(2, 2),
      cm::CoordinateMode::PolarSeparated, cpx::polyphonic::MappingMode::Stall, 4,
      cp::CoordinatePairTransform(1.f, 1.f, 1.f, pi / 2.f),
      cm::Rect(0.25f, -0.5f));
  requireNear(polarWrappedRect.x[0], 0.25f,
              "wrapped polar applies theta transform before conversion");
  requireNear(polarWrappedRect.y[0], 2.5f,
              "wrapped polar applies rect y offset after conversion");
  requireNear(polarWrappedRect.x[3], -4.75f, "wrapped polar stalls radius");
  requireNear(polarWrappedRect.y[3], -0.5f, "wrapped polar stalls theta");

  std::puts("compoly port mapping tests passed");
  return 0;
}
