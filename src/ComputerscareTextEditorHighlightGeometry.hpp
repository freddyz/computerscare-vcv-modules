#pragma once
#include "Computerscare.hpp"
namespace computerscare {
namespace text_editor {
inline void highlightFillPath(NVGcontext* vg, float x, float y, float width,
                              float height, float distortion, unsigned seed) {
  if (distortion <= 0.f) {
    nvgRect(vg, x, y, width, height);
    return;
  }
  float amount = std::min(distortion, std::min(width, height) * .2f);
  auto offset = [&](unsigned corner) {
    unsigned bits = seed * 0x9e3779b9u + corner * 0x85ebca6bu;
    bits ^= bits >> 16;
    bits *= 0x7feb352du;
    return float(bits & 1023u) / 1023.f * amount;
  };
  // Keep all four corners inside the row, with stable asymmetric edges.
  nvgMoveTo(vg, x + offset(0), y + offset(1));
  nvgLineTo(vg, x + width - offset(2), y + offset(3));
  nvgLineTo(vg, x + width - offset(4), y + height - offset(5));
  nvgLineTo(vg, x + offset(6), y + height - offset(7));
  nvgClosePath(vg);
}
inline void highlightBorderPath(NVGcontext* vg, float x, float y, float width,
                                float height, float distortion, unsigned seed) {
  if (distortion <= 0.f) {
    nvgRoundedRect(vg, x, y, width, height, 2.f);
    return;
  }
  float amount = std::min(distortion, std::min(width, height) * .08f);
  auto jitter = [&](unsigned corner) {
    unsigned bits = seed * 0x9e3779b9u + corner * 0x85ebca6bu;
    bits ^= bits >> 16;
    bits *= 0x7feb352du;
    return (float(bits & 1023u) / 1023.f - .5f) * amount * 2.f;
  };
  Vec corners[] = {Vec(x + jitter(0), y + jitter(1)),
                   Vec(x + width + jitter(2), y + jitter(3)),
                   Vec(x + width + jitter(4), y + height + jitter(5)),
                   Vec(x + jitter(6), y + height + jitter(7))};
  bool curved = jitter(17) > 0.f;
  if (curved) {
    Vec entry[4], exit[4];
    for (int i = 0; i < 4; ++i) {
      entry[i] =
          corners[i].plus(corners[(i + 3) % 4].minus(corners[i]).mult(.12f));
      exit[i] =
          corners[i].plus(corners[(i + 1) % 4].minus(corners[i]).mult(.12f));
    }
    nvgMoveTo(vg, entry[0].x, entry[0].y);
    for (int i = 0; i < 4; ++i) {
      nvgQuadTo(vg, corners[i].x, corners[i].y, exit[i].x, exit[i].y);
      Vec edge = entry[(i + 1) % 4].minus(exit[i]);
      float length = std::max(.01f, edge.norm());
      Vec bend = Vec(-edge.y, edge.x).mult(jitter(20 + i) / length);
      Vec first = exit[i].plus(edge.mult(1.f / 3.f)).plus(bend);
      Vec second = exit[i].plus(edge.mult(2.f / 3.f)).plus(bend);
      Vec end = entry[(i + 1) % 4];
      nvgBezierTo(vg, first.x, first.y, second.x, second.y, end.x, end.y);
    }
  } else {
    nvgMoveTo(vg, corners[0].x, corners[0].y);
    for (int i = 1; i < 4; ++i) nvgLineTo(vg, corners[i].x, corners[i].y);
  }
  nvgClosePath(vg);
  nvgLineJoin(vg, curved ? NVG_ROUND : NVG_MITER);
}
}  // namespace text_editor
}  // namespace computerscare
