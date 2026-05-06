#pragma once

#include <algorithm>
#include <array>
#include <cmath>

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

  Rect() : x(0.f), y(0.f) {}

  Rect(float x, float y) : x(x), y(y) {}
};

struct Polar {
  float r;
  float theta;

  Polar() : r(0.f), theta(0.f) {}

  Polar(float r, float theta) : r(r), theta(theta) {}
};

struct Quad {
  float x;
  float y;
  float r;
  float theta;

  Quad() : x(0.f), y(0.f), r(0.f), theta(0.f) {}

  Quad(float x, float y, float r, float theta)
      : x(x), y(y), r(r), theta(theta) {}
};

using Channels = std::array<float, maxChannels>;

struct RectChannels {
  Channels x;
  Channels y;

  RectChannels() : x(), y() {}
};

inline bool isRect(CoordinateMode mode) {
  return mode == CoordinateMode::RectInterleaved ||
         mode == CoordinateMode::RectSeparated;
}

inline bool isPolar(CoordinateMode mode) { return !isRect(mode); }

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

inline Rect add(Rect z, Rect w) { return Rect(z.x + w.x, z.y + w.y); }

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

inline RectChannels addChannels(const RectChannels& z, const RectChannels& w) {
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
