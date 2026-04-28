#pragma once

#include "ComplexMath.hpp"

namespace cpx {
namespace complex_math {
namespace simd {

#if defined(__GNUC__) || defined(__clang__)
using Float4 = float __attribute__((vector_size(16)));

inline Float4 load4(const float* values) {
  return {values[0], values[1], values[2], values[3]};
}

inline void store4(float* values, Float4 lanes) {
  values[0] = lanes[0];
  values[1] = lanes[1];
  values[2] = lanes[2];
  values[3] = lanes[3];
}
#else
struct Float4 {
  float lanes[4] = {};

  float& operator[](int i) { return lanes[i]; }

  const float& operator[](int i) const { return lanes[i]; }
};

inline Float4 operator+(Float4 a, Float4 b) {
  return {{a[0] + b[0], a[1] + b[1], a[2] + b[2], a[3] + b[3]}};
}

inline Float4 operator-(Float4 a, Float4 b) {
  return {{a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}};
}

inline Float4 operator*(Float4 a, Float4 b) {
  return {{a[0] * b[0], a[1] * b[1], a[2] * b[2], a[3] * b[3]}};
}

inline Float4 load4(const float* values) {
  return {{values[0], values[1], values[2], values[3]}};
}

inline void store4(float* values, Float4 lanes) {
  values[0] = lanes[0];
  values[1] = lanes[1];
  values[2] = lanes[2];
  values[3] = lanes[3];
}
#endif

struct Rect4 {
  Float4 x;
  Float4 y;

  Rect4() : x(), y() {}

  Rect4(Float4 x, Float4 y) : x(x), y(y) {}
};

inline Rect4 loadRect4(const float* x, const float* y) {
  return Rect4(load4(x), load4(y));
}

inline void storeRect4(float* x, float* y, Rect4 z) {
  store4(x, z.x);
  store4(y, z.y);
}

inline Rect4 add(Rect4 z, Rect4 w) { return Rect4(z.x + w.x, z.y + w.y); }

inline Rect4 multiply(Rect4 z, Rect4 w) {
  return Rect4(z.x * w.x - z.y * w.y, z.x * w.y + z.y * w.x);
}

inline RectChannels addChannels(const RectChannels& z, const RectChannels& w) {
  RectChannels out = {};
  for (int c = 0; c < maxChannels; c += 4) {
    storeRect4(&out.x[c], &out.y[c],
               add(loadRect4(&z.x[c], &z.y[c]), loadRect4(&w.x[c], &w.y[c])));
  }
  return out;
}

inline RectChannels multiplyChannels(const RectChannels& z,
                                     const RectChannels& w) {
  RectChannels out = {};
  for (int c = 0; c < maxChannels; c += 4) {
    storeRect4(
        &out.x[c], &out.y[c],
        multiply(loadRect4(&z.x[c], &z.y[c]), loadRect4(&w.x[c], &w.y[c])));
  }
  return out;
}

}  // namespace simd
}  // namespace complex_math
}  // namespace cpx
