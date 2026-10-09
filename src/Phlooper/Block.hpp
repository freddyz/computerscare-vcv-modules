#pragma once

#include "../Computerscare.hpp"

namespace phlooper {
// Static perspective plaque, with a deterministic warp and separately shaded
// sides.
struct WarpedBlock : widget::TransparentWidget {
  float depth = 4.f;
  float screwup = 1.f;
  unsigned shapeSeed = 1;
  NVGcolor faceColor = nvgRGB(222, 222, 222);

  void configure(Rect bounds, float thickness, float distortion, unsigned seed,
                 NVGcolor color) {
    box = bounds;
    depth = thickness;
    screwup = distortion;
    shapeSeed = seed;
    faceColor = color;
  }

  void draw(const DrawArgs& args) override {
    float d = clamp(depth, 0.f, std::min(box.size.x, box.size.y) * .25f);
    float amount = clamp(screwup, 0.f, std::min(box.size.x, box.size.y) * .12f);
    float margin = .5f + amount * .5f;
    auto jitter = [&](unsigned corner) {
      unsigned bits = shapeSeed * 0x9e3779b9u + corner * 0x85ebca6bu;
      bits ^= bits >> 16;
      bits *= 0x7feb352du;
      return (float(bits & 1023u) / 1023.f - .5f) * amount;
    };
    Vec face[4] = {Vec(margin, margin), Vec(box.size.x - d - margin, margin),
                   Vec(box.size.x - d - margin, box.size.y - d * .65f - margin),
                   Vec(margin, box.size.y - d * .65f - margin)};
    Vec back[4];
    for (int i = 0; i < 4; ++i) {
      face[i] = face[i].plus(Vec(jitter(i * 2), jitter(i * 2 + 1)));
      back[i] = face[i].plus(Vec(d, d * .65f));
    }
    auto quad = [&](Vec a, Vec b, Vec c, Vec e, NVGcolor color) {
      nvgBeginPath(args.vg);
      nvgMoveTo(args.vg, a.x, a.y);
      for (Vec p : {b, c, e}) nvgLineTo(args.vg, p.x, p.y);
      nvgClosePath(args.vg);
      nvgFillColor(args.vg, color);
      nvgFill(args.vg);
      nvgStrokeColor(args.vg, nvgRGB(0, 0, 0));
      nvgLineJoin(args.vg, NVG_ROUND);
      nvgStrokeWidth(args.vg, .65f);
      nvgStroke(args.vg);
    };
    auto shade = [&](float factor) {
      return nvgRGBAf(faceColor.r * factor, faceColor.g * factor,
                      faceColor.b * factor, faceColor.a);
    };
    quad(face[1], back[1], back[2], face[2], shade(.78f));
    quad(face[3], face[2], back[2], back[3], shade(.60f));
    quad(face[0], face[1], face[2], face[3], faceColor);
  }
};
}  // namespace phlooper
