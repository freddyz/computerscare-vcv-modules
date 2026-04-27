#pragma once

#include <array>
#include <algorithm>
#include <cmath>

#include "../CompolyRouting.hpp"

namespace cpx {
namespace complex_math {

constexpr int maxChannels = 16;
constexpr float pi = 3.14159265358979323846f;
constexpr float thetaVoltsPerRadian = 5.f / pi;
constexpr float thetaRadiansPerVolt = pi / 5.f;

enum class CoordinateMode {
  RectInterleaved = 0,
  PolarInterleaved = 1,
  RectSeparated = 2,
  PolarSeparated = 3,
};

struct Rect {
  float x;
  float y;

  Rect() : x(0.f), y(0.f) {
  }

  Rect(float x, float y) : x(x), y(y) {
  }
};

struct Polar {
  float r;
  float theta;

  Polar() : r(0.f), theta(0.f) {
  }

  Polar(float r, float theta) : r(r), theta(theta) {
  }
};

struct Quad {
  float x;
  float y;
  float r;
  float theta;

  Quad() : x(0.f), y(0.f), r(0.f), theta(0.f) {
  }

  Quad(float x, float y, float r, float theta)
      : x(x), y(y), r(r), theta(theta) {
  }
};

using Channels = std::array<float, maxChannels>;

struct RectChannels {
  Channels x;
  Channels y;

  RectChannels() : x(), y() {
  }
};

struct PortChannels {
  Channels a;
  Channels b;

  PortChannels() : a(), b() {
  }
};

struct PortChannelCounts {
  int a;
  int b;

  PortChannelCounts() : a(0), b(0) {
  }

  PortChannelCounts(int a, int b) : a(a), b(b) {
  }
};

inline bool isRect(CoordinateMode mode) {
  return mode == CoordinateMode::RectInterleaved ||
         mode == CoordinateMode::RectSeparated;
}

inline bool isPolar(CoordinateMode mode) {
  return !isRect(mode);
}

inline bool isInterleaved(CoordinateMode mode) {
  return mode == CoordinateMode::RectInterleaved ||
         mode == CoordinateMode::PolarInterleaved;
}

inline Polar rectToPolar(Rect z) {
  return Polar(std::hypot(z.x, z.y), std::atan2(z.y, z.x));
}

inline Rect polarToRect(Polar z) {
  return Rect(z.r * std::cos(z.theta), z.r * std::sin(z.theta));
}

inline float thetaRadiansToCableVoltage(float thetaRadians) {
  return thetaRadians * thetaVoltsPerRadian;
}

inline float thetaCableVoltageToRadians(float thetaVoltage) {
  return thetaVoltage * thetaRadiansPerVolt;
}

inline Quad quadFromRect(Rect z) {
  Polar polar = rectToPolar(z);
  return Quad(z.x, z.y, polar.r, polar.theta);
}

inline Quad quadFromPolar(Polar z) {
  Rect rect = polarToRect(z);
  return Quad(rect.x, rect.y, z.r, z.theta);
}

inline Quad quadFromPair(float a, float b, CoordinateMode mode) {
  return isRect(mode) ? quadFromRect(Rect(a, b))
                      : quadFromPolar(Polar(a, thetaCableVoltageToRadians(b)));
}

inline Rect add(Rect z, Rect w) {
  return Rect(z.x + w.x, z.y + w.y);
}

inline Rect multiply(Rect z, Rect w) {
  return Rect(z.x * w.x - z.y * w.y, z.x * w.y + z.y * w.x);
}

inline Rect scaleAndOffset(Rect z, Rect scale, Rect offset) {
  Rect scaled = multiply(z, scale);
  return add(scaled, offset);
}

inline int clampChannelCount(int channels) {
  return std::max(0, std::min(maxChannels, channels));
}

inline int compolyphonyForInput(CoordinateMode mode, int portAChannels,
                                int portBChannels) {
  cpx::compoly::CablePolyChannels portA(portAChannels);
  cpx::compoly::CablePolyChannels portB(portBChannels);
  cpx::compoly::SeparatedCablePolyChannels cables(portA, portB);
  if (isInterleaved(mode))
    return cpx::compoly::compolyLanesForInterleavedCables(cables);
  return cpx::compoly::compolyLanesForSeparatedCables(cables);
}

inline int outputCompolyphony(int knobSetting, int maxInputCompolyphony) {
  return cpx::compoly::outputCompolyLanes(knobSetting, maxInputCompolyphony);
}

inline int channelIndexForOutput(int outputIndex, cpx::compoly::WrapMode wrapMode,
                                 int channelCount) {
  return cpx::compoly::cableChannelForCompolyLane(
      cpx::compoly::CompolyLane(outputIndex), wrapMode,
      cpx::compoly::CablePolyChannels(channelCount));
}

inline std::array<int, 2> separatedInputChannelIndices(
    int outputIndex, cpx::compoly::WrapMode wrapMode, int portAChannels,
    int portBChannels) {
  cpx::compoly::SeparatedCableChannels channels =
      cpx::compoly::separatedCableChannelsForCompolyLane(
          cpx::compoly::CompolyLane(outputIndex), wrapMode,
          cpx::compoly::SeparatedCablePolyChannels(
              cpx::compoly::CablePolyChannels(portAChannels),
              cpx::compoly::CablePolyChannels(portBChannels)));
  return {{channels.first, channels.second}};
}

inline PortChannelCounts outputPortChannelCounts(CoordinateMode mode,
                                                 int compolyChannels) {
  compolyChannels = clampChannelCount(compolyChannels);
  if (!isInterleaved(mode))
    return PortChannelCounts(compolyChannels, compolyChannels);

  int totalChannels = compolyChannels * 2;
  return PortChannelCounts(std::min(maxChannels, totalChannels),
                           std::max(0, totalChannels - maxChannels));
}

inline RectChannels readRectFromPorts(const PortChannels& ports,
                                      CoordinateMode mode) {
  RectChannels rect = {};

  for (int c = 0; c < maxChannels; ++c) {
    if (mode == CoordinateMode::RectSeparated) {
      rect.x[c] = ports.a[c];
      rect.y[c] = ports.b[c];
    } else if (mode == CoordinateMode::RectInterleaved) {
      rect.x[c] = c < 8 ? ports.a[2 * c] : ports.b[(2 * c) % maxChannels];
      rect.y[c] =
          c < 8 ? ports.a[2 * c + 1] : ports.b[(2 * c + 1) % maxChannels];
    } else if (mode == CoordinateMode::PolarSeparated) {
      Rect z = polarToRect(
          Polar(ports.a[c], thetaCableVoltageToRadians(ports.b[c])));
      rect.x[c] = z.x;
      rect.y[c] = z.y;
    } else if (mode == CoordinateMode::PolarInterleaved) {
      float r = c < 8 ? ports.a[2 * c] : ports.b[(2 * c) % maxChannels];
      float theta =
          c < 8 ? ports.a[2 * c + 1] : ports.b[(2 * c + 1) % maxChannels];
      Rect z = polarToRect(Polar(r, thetaCableVoltageToRadians(theta)));
      rect.x[c] = z.x;
      rect.y[c] = z.y;
    }
  }

  return rect;
}

inline PortChannels writePortsFromRect(const RectChannels& rect,
                                       CoordinateMode mode) {
  PortChannels ports = {};

  for (int c = 0; c < maxChannels; ++c) {
    Quad z = quadFromRect(Rect(rect.x[c], rect.y[c]));
    float a = isPolar(mode) ? z.r : z.x;
    float b = isPolar(mode) ? thetaRadiansToCableVoltage(z.theta) : z.y;

    if (isInterleaved(mode)) {
      if (c < 8) {
        ports.a[2 * c] = a;
        ports.a[2 * c + 1] = b;
      } else {
        ports.b[(2 * c) % maxChannels] = a;
        ports.b[(2 * c + 1) % maxChannels] = b;
      }
    } else {
      ports.a[c] = a;
      ports.b[c] = b;
    }
  }

  return ports;
}

inline RectChannels addChannels(const RectChannels& z,
                                const RectChannels& w) {
  RectChannels out = {};
  for (int c = 0; c < maxChannels; ++c) {
    Rect sum = add(Rect(z.x[c], z.y[c]), Rect(w.x[c], w.y[c]));
    out.x[c] = sum.x;
    out.y[c] = sum.y;
  }
  return out;
}

inline RectChannels multiplyChannels(const RectChannels& z,
                                     const RectChannels& w) {
  RectChannels out = {};
  for (int c = 0; c < maxChannels; ++c) {
    Rect product = multiply(Rect(z.x[c], z.y[c]), Rect(w.x[c], w.y[c]));
    out.x[c] = product.x;
    out.y[c] = product.y;
  }
  return out;
}

}  // namespace complex_math
}  // namespace cpx
