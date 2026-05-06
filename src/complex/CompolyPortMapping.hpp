#pragma once

#include <algorithm>
#include <array>

#include "CompolyRouting.hpp"
#include "math/ComplexMath.hpp"

namespace cpx {
namespace compoly {

struct PortChannels {
  cpx::complex_math::Channels a;
  cpx::complex_math::Channels b;

  PortChannels() : a(), b() {}
};

struct PortChannelCounts {
  int a;
  int b;

  PortChannelCounts() : a(0), b(0) {}

  PortChannelCounts(int a, int b) : a(a), b(b) {}
};

struct CoordinatePairTransform {
  float aScale;
  float aOffset;
  float bScale;
  float bOffset;

  CoordinatePairTransform()
      : aScale(1.f), aOffset(0.f), bScale(1.f), bOffset(0.f) {}

  CoordinatePairTransform(float aScale, float aOffset, float bScale,
                          float bOffset)
      : aScale(aScale), aOffset(aOffset), bScale(bScale), bOffset(bOffset) {}
};

inline int compolyLanesForInput(cpx::complex_math::CoordinateMode mode,
                                int portAChannels, int portBChannels) {
  SeparatedCablePolyChannels cables =
      SeparatedCablePolyChannels(CablePolyChannels(portAChannels),
                                 CablePolyChannels(portBChannels));
  if (cpx::complex_math::isInterleaved(mode))
    return compolyLanesForInterleavedCables(cables);
  return compolyLanesForSeparatedCables(cables);
}

inline std::array<int, 2> separatedInputChannelIndices(
    int outputIndex, WrapMode wrapMode, int portAChannels, int portBChannels) {
  SeparatedCableChannels channels = separatedCableChannelsForCompolyLane(
      CompolyLane(outputIndex), wrapMode,
      SeparatedCablePolyChannels(CablePolyChannels(portAChannels),
                                 CablePolyChannels(portBChannels)));
  return {{channels.first, channels.second}};
}

inline PortChannelCounts outputPortChannelCounts(
    cpx::complex_math::CoordinateMode mode, int compolyChannels) {
  compolyChannels = cpx::complex_math::clampChannelCount(compolyChannels);
  if (!cpx::complex_math::isInterleaved(mode))
    return PortChannelCounts(compolyChannels, compolyChannels);

  int totalChannels = compolyChannels * 2;
  return PortChannelCounts(
      std::min(cpx::complex_math::maxChannels, totalChannels),
      std::max(0, totalChannels - cpx::complex_math::maxChannels));
}

inline cpx::complex_math::RectChannels readRectFromPorts(
    const PortChannels& ports, cpx::complex_math::CoordinateMode mode) {
  cpx::complex_math::RectChannels rect = {};

  for (int c = 0; c < cpx::complex_math::maxChannels; ++c) {
    if (mode == cpx::complex_math::CoordinateMode::RectSeparated) {
      rect.x[c] = ports.a[c];
      rect.y[c] = ports.b[c];
    } else if (mode == cpx::complex_math::CoordinateMode::RectInterleaved) {
      rect.x[c] = c < 8 ? ports.a[2 * c]
                        : ports.b[(2 * c) % cpx::complex_math::maxChannels];
      rect.y[c] =
          c < 8 ? ports.a[2 * c + 1]
                : ports.b[(2 * c + 1) % cpx::complex_math::maxChannels];
    } else if (mode == cpx::complex_math::CoordinateMode::PolarSeparated) {
      cpx::complex_math::Rect z = cpx::complex_math::polarToRect(
          cpx::complex_math::Polar(
              ports.a[c],
              cpx::complex_math::thetaCableVoltageToRadians(ports.b[c])));
      rect.x[c] = z.x;
      rect.y[c] = z.y;
    } else if (mode == cpx::complex_math::CoordinateMode::PolarInterleaved) {
      float r = c < 8 ? ports.a[2 * c]
                      : ports.b[(2 * c) % cpx::complex_math::maxChannels];
      float theta =
          c < 8 ? ports.a[2 * c + 1]
                : ports.b[(2 * c + 1) % cpx::complex_math::maxChannels];
      cpx::complex_math::Rect z = cpx::complex_math::polarToRect(
          cpx::complex_math::Polar(
              r, cpx::complex_math::thetaCableVoltageToRadians(theta)));
      rect.x[c] = z.x;
      rect.y[c] = z.y;
    }
  }

  return rect;
}

inline cpx::complex_math::RectChannels readWrappedSeparatedInputToRect(
    const PortChannels& ports, PortChannelCounts portChannels,
    cpx::complex_math::CoordinateMode mode, WrapMode wrapMode,
    int compolyChannels,
    CoordinatePairTransform transform = CoordinatePairTransform(),
    cpx::complex_math::Rect rectOffset = cpx::complex_math::Rect()) {
  cpx::complex_math::RectChannels rect = {};
  compolyChannels = cpx::complex_math::clampChannelCount(compolyChannels);

  for (int c = 0; c < compolyChannels; ++c) {
    std::array<int, 2> inputChannels = separatedInputChannelIndices(
        c, wrapMode, portChannels.a, portChannels.b);
    float a = ports.a[inputChannels[0]] * transform.aScale + transform.aOffset;
    float b = ports.b[inputChannels[1]] * transform.bScale + transform.bOffset;

    if (mode == cpx::complex_math::CoordinateMode::PolarSeparated) {
      b = cpx::complex_math::thetaCableVoltageToRadians(
              ports.b[inputChannels[1]]) *
              transform.bScale +
          transform.bOffset;
    }

    cpx::complex_math::Rect z =
        cpx::complex_math::isPolar(mode)
            ? cpx::complex_math::polarToRect(cpx::complex_math::Polar(a, b))
            : cpx::complex_math::Rect(a, b);
    rect.x[c] = z.x + rectOffset.x;
    rect.y[c] = z.y + rectOffset.y;
  }

  return rect;
}

inline PortChannels writePortsFromRect(
    const cpx::complex_math::RectChannels& rect,
    cpx::complex_math::CoordinateMode mode) {
  PortChannels ports = {};

  for (int c = 0; c < cpx::complex_math::maxChannels; ++c) {
    cpx::complex_math::Quad z =
        cpx::complex_math::quadFromRect(cpx::complex_math::Rect(rect.x[c],
                                                                rect.y[c]));
    float a = cpx::complex_math::isPolar(mode) ? z.r : z.x;
    float b = cpx::complex_math::isPolar(mode)
                  ? cpx::complex_math::thetaRadiansToCableVoltage(z.theta)
                  : z.y;

    if (cpx::complex_math::isInterleaved(mode)) {
      if (c < 8) {
        ports.a[2 * c] = a;
        ports.a[2 * c + 1] = b;
      } else {
        ports.b[(2 * c) % cpx::complex_math::maxChannels] = a;
        ports.b[(2 * c + 1) % cpx::complex_math::maxChannels] = b;
      }
    } else {
      ports.a[c] = a;
      ports.b[c] = b;
    }
  }

  return ports;
}

}  // namespace compoly
}  // namespace cpx
