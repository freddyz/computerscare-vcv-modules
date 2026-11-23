#pragma once
#include <array>

#include "../Computerscare.hpp"
#include "../ComputerscareContrast.hpp"

namespace phlooper {
// A raised face and a recessed face share the same warped panel footprint.
// Text follows the face geometry, rather than a separately tuned state offset.
struct WarpedDropdown : widget::OpaqueWidget {
  float textSize = 13.f;
  float textInset = 2.f;
  bool showLabel = true;
  float upDepth = 4.f;
  float downDepth = 3.f;
  float screwup = .8f;
  unsigned shapeSeed = 1;
  NVGcolor faceColor = nvgRGB(222, 222, 222);
  NVGcolor textColor = nvgRGB(15, 42, 36);
  NVGcolor borderColor = nvgRGB(0, 0, 0);
  float borderWidth = .65f;
  std::array<Vec, 4> cornerOffsets{};
  enum Shade { SOCKET, TOP, LEFT, RIGHT, BOTTOM, DOWN, UP, SHADE_COUNT };
  bool adaptiveContrast = false;
  static constexpr float brightestBorder = .8f;
  std::array<float, SHADE_COUNT> textInks{};
  NVGcolor shadeColor(Shade shade) const {
    const float grey[] = {116, 125, 163, 173, 132, 209, 222};
    float factor = grey[shade] / 222.f;
    return nvgRGBAf(faceColor.r * factor, faceColor.g * factor,
                    faceColor.b * factor, faceColor.a);
  }
  // Opt in per control. Cache color decisions when its face/theme changes;
  // pressed-state drawing only selects the already calculated inks.
  void updateContrast(NVGcolor background,
                      float borderCeiling = brightestBorder) {
    adaptiveContrast = true;
    float panel = computerscare::contrast::luminance(background.r, background.g,
                                                     background.b);
    std::array<float, SHADE_COUNT + 1> surfaces{};
    surfaces[SHADE_COUNT] = panel;
    for (int i = 0; i < SHADE_COUNT; ++i) {
      auto fill = shadeColor(Shade(i));
      float surface =
          computerscare::contrast::luminance(fill.r, fill.g, fill.b);
      surfaces[i] = surface;
      textInks[i] = computerscare::contrast::textInk(surface);
    }
    float ink = computerscare::contrast::controlBorder(
        surfaces[UP], surfaces[DOWN], panel, borderCeiling);
    borderColor = nvgRGBf(ink, ink, ink);
  }
  NVGcolor foregroundColor() {
    if (!adaptiveContrast) return textColor;
    float ink = textInks[isPressed() ? DOWN : UP];
    return nvgRGBf(ink, ink, ink);
  }
  virtual std::string label() = 0;
  virtual bool isPressed() = 0;
  virtual std::shared_ptr<window::Font> labelFont() {
    return APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/Oswald-Regular.ttf"));
  }
  virtual std::string displayLabel() { return label(); }
  virtual bool fitLabel() const { return false; }

  void configure(float width, float height, float raised, float recessed,
                 float distortion, unsigned seed) {
    box.size = Vec(width, height);
    upDepth = raised;
    downDepth = recessed;
    screwup = distortion;
    shapeSeed = seed;
  }

  struct Geometry {
    std::array<Vec, 4> front, back, face;
  };
  Geometry geometry(bool pressed) const {
    Geometry result;
    auto& front = result.front;
    auto& back = result.back;
    auto& face = result.face;
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
    front = {{Vec(1, 1), Vec(box.size.x - depth - 1, 1),
              Vec(box.size.x - depth - 1, box.size.y - depth * .65f - 1),
              Vec(1, box.size.y - depth * .65f - 1)}};
    for (int i = 0; i < 4; ++i)
      front[i] = front[i].plus(Vec(jitter(i * 2), jitter(i * 2 + 1))) +
                 cornerOffsets[i];
    for (int i = 0; i < 4; ++i) {
      back[i] = front[i].plus(Vec(depth, depth * .65f));
      face[i] = pressed ? back[i] : front[i];
    }
    if (pressed) {
      // Inset along the warped sides, so the recessed face meets the socket
      // exactly even when the corners are skewed independently.
      float u = clamp(inset / std::max(1.f, back[1].x - back[0].x), 0.f, .4f);
      float v =
          clamp(inset * .65f / std::max(1.f, back[3].y - back[0].y), 0.f, .4f);
      auto lerp = [](Vec a, Vec b, float t) { return a + (b - a) * t; };
      face[1] = lerp(back[1], back[2], v);
      face[3] = lerp(back[3], back[2], u);
      face[0] = lerp(lerp(back[0], back[3], v), face[1], u);
    }
    return result;
  }
  Rect labelArea(const Geometry& shape) const {
    const auto& face = shape.face;
    float padding = fitLabel() ? 1.5f + borderWidth * .5f : 1.f;
    float left = std::max(face[0].x, face[3].x) + padding;
    float top = std::max(face[0].y, face[1].y) + padding;
    float right = std::min(face[1].x, face[2].x) - padding;
    float bottom = std::min(face[2].y, face[3].y) - padding;
    left = std::max(left, face[0].x + textInset);
    return Rect(Vec(left, top),
                Vec(std::max(0.f, right - left), std::max(0.f, bottom - top)));
  }
  struct LabelPlacement {
    Vec origin;
    float size;
  };
  LabelPlacement fitText(NVGcontext* vg, const std::string& text,
                         Rect area) const {
    // Measure actual glyph bounds: custom glyphs can extend beyond their
    // advance or ascender. Fit height as well as width, including short faces.
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
    // Glyph rasterization snaps the left edge down to a pixel. Leave a pixel
    // of room in local coordinates, including when Rack is zoomed out.
    float transform[6];
    nvgCurrentTransform(vg, transform);
    float scale = std::min(std::hypot(transform[0], transform[1]),
                           std::hypot(transform[2], transform[3]));
    float pixel = std::max(.3f, 1.f / std::max(.1f, scale)) + .05f;
    float size = textSize, bounds[4]{};
    LabelPlacement fitted{};
    for (int pass = 0; pass < 8; ++pass) {
      nvgFontSize(vg, size);
      nvgTextBounds(vg, 0, 0, text.c_str(), nullptr, bounds);
      fitted = {
          Vec(area.pos.x + pixel - bounds[0],
              area.pos.y + area.size.y * .5f - (bounds[1] + bounds[3]) * .5f),
          size};
      // Measure at the actual origin too: translating across a pixel boundary
      // can add a pixel to the glyph bounds even without changing font size.
      nvgTextBounds(vg, fitted.origin.x, fitted.origin.y, text.c_str(), nullptr,
                    bounds);
      if (bounds[0] >= area.pos.x && bounds[1] >= area.pos.y &&
          bounds[2] <= area.pos.x + area.size.x &&
          bounds[3] <= area.pos.y + area.size.y)
        return fitted;
      float fit = std::min({.92f,
                            std::max(0.f, area.size.x - pixel - .1f) /
                                std::max(.01f, bounds[2] - bounds[0]),
                            std::max(0.f, area.size.y - .2f) /
                                std::max(.01f, bounds[3] - bounds[1])});
      size *= fit;
    }
    return fitted;
  }
  void draw(const DrawArgs& args) override {
    bool pressed = isPressed();
    auto shape = geometry(pressed);
    const auto& front = shape.front;
    const auto& back = shape.back;
    const auto& face = shape.face;
    auto quad = [&](Vec a, Vec b, Vec c, Vec d, Shade shade) {
      nvgBeginPath(args.vg);
      nvgMoveTo(args.vg, a.x, a.y);
      for (Vec p : {b, c, d}) nvgLineTo(args.vg, p.x, p.y);
      nvgClosePath(args.vg);
      nvgFillColor(args.vg, shadeColor(shade));
      nvgFill(args.vg);
    };
    if (pressed) {
      quad(back[0], back[1], back[2], back[3], SOCKET);
      quad(back[0], back[1], face[1], face[0], TOP);
      quad(back[0], face[0], face[3], back[3], LEFT);
    } else {
      quad(front[1], back[1], back[2], front[2], RIGHT);
      quad(front[3], front[2], back[2], back[3], BOTTOM);
    }
    quad(face[0], face[1], face[2], face[3], pressed ? DOWN : UP);
    // Draw the connected edge network once, after every fill. Shared edges
    // must not be covered by the next facet or acquire a different ink.
    nvgBeginPath(args.vg);
    auto move = [&](Vec p) { nvgMoveTo(args.vg, p.x, p.y); };
    auto line = [&](Vec p) { nvgLineTo(args.vg, p.x, p.y); };
    const auto& outline = pressed ? back : front;
    move(outline[0]);
    for (int i = 1; i < 4; ++i) line(outline[i]);
    nvgClosePath(args.vg);
    if (pressed) {
      move(face[1]);
      line(face[0]);
      line(face[3]);
      move(back[0]);
      line(face[0]);
    } else {
      move(front[1]);
      line(back[1]);
      line(back[2]);
      line(back[3]);
      line(front[3]);
      move(front[2]);
      line(back[2]);
    }
    nvgLineJoin(args.vg, NVG_ROUND);
    nvgLineCap(args.vg, NVG_ROUND);
    nvgStrokeColor(args.vg, borderColor);
    nvgStrokeWidth(args.vg, borderWidth);
    nvgStroke(args.vg);

    auto area = labelArea(shape);
    nvgSave(args.vg);
    nvgIntersectScissor(args.vg, area.pos.x, area.pos.y, area.size.x,
                        area.size.y);
    auto font = labelFont();
    if (font && showLabel) {
      nvgFontFaceId(args.vg, font->handle);
      nvgFontSize(args.vg, textSize);
      auto text = displayLabel();
      nvgFillColor(args.vg, foregroundColor());
      if (fitLabel()) {
        auto fitted = fitText(args.vg, text, area);
        nvgText(args.vg, fitted.origin.x, fitted.origin.y, text.c_str(),
                nullptr);
      } else {
        nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(args.vg, face[0].x + textInset, (face[0].y + face[3].y) * .5f,
                text.c_str(), nullptr);
      }
    }
    nvgRestore(args.vg);
  }
};
}  // namespace phlooper
