#pragma once

#include "../Computerscare.hpp"

namespace phlooper {
// A raised face and a recessed face share the same warped panel footprint.
// Text follows the face geometry, rather than a separately tuned state offset.
struct WarpedDropdown : widget::OpaqueWidget {
  float upDepth = 4.f;
  float downDepth = 3.f;
  float screwup = .8f;
  unsigned shapeSeed = 1;
  NVGcolor faceColor = nvgRGB(222, 222, 222);
  virtual std::string label() = 0;
  virtual bool isPressed() = 0;

  void configure(float width, float height, float raised, float recessed,
                 float distortion, unsigned seed) {
    box.size = Vec(width, height);
    upDepth = raised;
    downDepth = recessed;
    screwup = distortion;
    shapeSeed = seed;
  }

  void draw(const DrawArgs& args) override {
    bool pressed = isPressed();
    float depth = clamp(upDepth, 0.f, std::min(box.size.x, box.size.y) * .3f);
    float inset =
        clamp(downDepth, 0.f, std::min(box.size.x, box.size.y) * .25f);
    float wobble = clamp(screwup, 0.f, 1.5f);
    auto jitter = [&](unsigned corner) {
      unsigned bits = shapeSeed * 0x9e3779b9u + corner * 0x85ebca6bu;
      bits ^= bits >> 16;
      bits *= 0x7feb352du;
      return (float(bits & 1023u) / 1023.f - .5f) * wobble;
    };
    Vec front[4] = {Vec(1, 1), Vec(box.size.x - depth - 1, 1),
                    Vec(box.size.x - depth - 1, box.size.y - depth * .65f - 1),
                    Vec(1, box.size.y - depth * .65f - 1)};
    for (int i = 0; i < 4; ++i)
      front[i] = front[i].plus(Vec(jitter(i * 2), jitter(i * 2 + 1)));
    Vec back[4], face[4];
    for (int i = 0; i < 4; ++i) {
      back[i] = front[i].plus(Vec(depth, depth * .65f));
      face[i] = pressed ? back[i] : front[i];
    }
    if (pressed) {
      // The socket is at the block’s base footprint; the raised outline
      // disappears.
      face[0] = front[0].plus(Vec(depth + inset, depth * .65f + inset * .65f));
      face[1] = Vec(back[1].x, face[0].y + front[1].y - front[0].y);
      face[3] = Vec(face[0].x + front[3].x - front[0].x, back[3].y);
    }
    auto shade = [&](int grey) {
      float factor = grey / 222.f;
      return nvgRGBAf(faceColor.r * factor, faceColor.g * factor,
                      faceColor.b * factor, faceColor.a);
    };
    auto quad = [&](Vec a, Vec b, Vec c, Vec d, int grey) {
      nvgBeginPath(args.vg);
      nvgMoveTo(args.vg, a.x, a.y);
      for (Vec p : {b, c, d}) nvgLineTo(args.vg, p.x, p.y);
      nvgClosePath(args.vg);
      nvgFillColor(args.vg, shade(grey));
      nvgFill(args.vg);
      nvgStrokeColor(args.vg, nvgRGB(0, 0, 0));
      nvgLineJoin(args.vg, NVG_ROUND);
      nvgStrokeWidth(args.vg, .65f);
      nvgStroke(args.vg);
    };
    if (pressed) {
      quad(back[0], back[1], back[2], back[3], 116);
      quad(back[0], back[1], face[1], face[0], 125);
      quad(back[0], face[0], face[3], back[3], 163);
    } else {
      quad(front[1], back[1], back[2], front[2], 173);
      quad(front[3], front[2], back[2], back[3], 132);
    }
    quad(face[0], face[1], face[2], face[3], pressed ? 209 : 222);
    // Conservative inset rectangle keeps lettering within the warped face.
    float left = std::max(face[0].x, face[3].x) + 1.f;
    float top = std::max(face[0].y, face[1].y) + 1.f;
    float right = std::min(face[1].x, face[2].x) - 1.f;
    float bottom = std::min(face[2].y, face[3].y) - 1.f;
    nvgSave(args.vg);
    nvgIntersectScissor(args.vg, left, top, std::max(0.f, right - left),
                        std::max(0.f, bottom - top));
    auto font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/Oswald-Regular.ttf"));
    if (font) {
      nvgFontFaceId(args.vg, font->handle);
      nvgFontSize(args.vg, 13.f);
      nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
      nvgFillColor(args.vg, nvgRGB(15, 42, 36));
      nvgText(args.vg, face[0].x + 2.f, (face[0].y + face[3].y) * .5f,
              label().c_str(), nullptr);
    }
    nvgRestore(args.vg);
  }
};
}  // namespace phlooper
