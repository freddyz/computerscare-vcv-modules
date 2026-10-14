#pragma once
#include "Types.hpp"
namespace windchimes {
constexpr float pivotY = -0.8f;
constexpr float strikerLength = 0.68f;
constexpr float sailLength = 0.46f;
constexpr float strikerRadius = 0.075f;
constexpr float tubeSwingLimit = 0.65f;
constexpr float strikerSwingLimit = 0.95f;
struct Point {
  float x, y, z;
  Point(float a = 0.f, float b = 0.f, float c = 0.f) : x(a), y(b), z(c) {}
  Point operator+(Point b) const { return {x + b.x, y + b.y, z + b.z}; }
  Point operator-(Point b) const { return {x - b.x, y - b.y, z - b.z}; }
  Point operator*(float b) const { return {x * b, y * b, z * b}; }
  float dot(Point b) const { return x * b.x + y * b.y + z * b.z; }
};
inline Point tubeAnchorPoint(int t, int count) {
  float angle = 2.f * pi * t / count;
  return {0.22f * std::cos(angle), pivotY, 0.22f * std::sin(angle)};
}
inline float tubeAnchor(int t, int count) {
  return tubeAnchorPoint(t, count).x;
}
inline Point pendulumAxis(float angle, float depth = 0.f) {
  float radius = std::sqrt(angle * angle + depth * depth);
  float scale = radius > 1e-6f ? std::sin(radius) / radius : 1.f;
  return {angle * scale, std::cos(radius), depth * scale};
}
inline float tubeLength(int t) { return 0.54f + (t % 5) * 0.025f; }
inline float tubeHalfLength(int t) { return 0.21f - (t % 5) * 0.02f; }
inline float tubeRadius(int count) { return std::min(0.032f, 0.22f / count); }
inline Point pendulumCenter(float anchor, float length, float angle) {
  return {anchor + length * std::sin(angle), pivotY + length * std::cos(angle)};
}
struct Capsule {
  Point center, axis;
  float halfLength, radius;
  Point top() const { return center - axis * halfLength; }
  Point bottom() const { return center + axis * halfLength; }
};
inline Capsule tubeCapsule(int t, int count, float angle, float depth = 0.f) {
  Point axis = pendulumAxis(angle, depth);
  return {tubeAnchorPoint(t, count) + axis * tubeLength(t), axis,
          tubeHalfLength(t), tubeRadius(count)};
}
inline Point closestPoint(Point p, Point a, Point b) {
  Point d = b - a;
  float length = d.dot(d);
  return a +
         d * (length > 1e-10f ? clamp((p - a).dot(d) / length, 0.f, 1.f) : 0.f);
}
struct Contact {
  Point a, b;
  float distance;
};
inline Contact capsuleContact(const Capsule& a, const Capsule& b) {
  Point p = a.top(), q = b.top(), d = a.bottom() - p, e = b.bottom() - q,
        r = p - q;
  float aa = d.dot(d), ee = e.dot(e), bb = d.dot(e), cc = d.dot(r),
        ff = e.dot(r);
  float s = 0.f, t = 0.f;
  float denominator = aa * ee - bb * bb;
  if (aa > 1e-10f && ee > 1e-10f) {
    if (denominator > 1e-10f)
      s = clamp((bb * ff - cc * ee) / denominator, 0.f, 1.f);
    t = (bb * s + ff) / ee;
    if (t < 0.f) {
      t = 0.f;
      s = clamp(-cc / aa, 0.f, 1.f);
    } else if (t > 1.f) {
      t = 1.f;
      s = clamp((bb - cc) / aa, 0.f, 1.f);
    }
  }
  Point first = p + d * s, second = q + e * t;
  Point delta = second - first;
  return {first, second, std::sqrt(delta.dot(delta))};
}
}  // namespace windchimes
