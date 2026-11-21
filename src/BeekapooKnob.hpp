#pragma once
#include <array>
#include <cstdint>

#include "BeekapooIdentity.hpp"
#include "BeekapooParamLayout.hpp"
#include "Computerscare.hpp"
#include "ComputerscareTypeface.hpp"

namespace beekapoo {
// Retain palette selection separately while the panel uses a monochrome skin.
inline NVGcolor panelGray(NVGcolor color) {
  float value = color.r * .2126f + color.g * .7152f + color.b * .0722f;
  return nvgRGBAf(value, value, value, color.a);
}

inline uint32_t controlHash(const std::string& text) {
  uint32_t hash = 2166136261u;
  for (unsigned char c : text) {
    hash ^= c;
    hash *= 16777619u;
  }
  return hash;
}
inline std::string moduleStyleName(const plugin::Model* model) {
  if (!model || !model->plugin) return "Unknown module";
  return model->plugin->getBrand() + "\x1f" + model->name + "\x1f" +
         model->plugin->slug + "/" + model->slug;
}
struct ControlStyle {
  float hue, saturation, lightness, exponent, ripple, rotation, pointer;
  int lobes;
  float size, aspectX, aspectY, bevel;
  int shape, indicator;
  float ink;
  bool tipDot, innerRing;
  int border, ridges;
  float borderWidth;
  NVGcolor color(float shade = 0, bool dark = false) const {
    if (dark) {
      float value = clamp(.86f - lightness * .55f + shade, .015f, .95f);
      return nvgRGBf(value, value, value);
    }
    return panelGray(
        nvgHSL(hue, saturation, clamp(lightness + shade, .015f, .9f)));
  }
  NVGcolor indicatorColor(bool dark = false) const {
    float value = dark ? .05f + .08f * (1.f - ink) : ink;
    return nvgRGBf(value, value, value);
  }
};
inline ControlStyle controlStyle(const std::string& parameter,
                                 const std::string& source,
                                 const PatchIdentity& patch = PatchIdentity()) {
  uint64_t family =
      mixIdentity(controlHash(paramFamily(parameter)), patch.types);
  uint64_t module = mixIdentity(controlHash(source), patch.modules);
  uint64_t detail = mixIdentity(family ^ module, patch.wiring);
  uint64_t maker = mixIdentity(patch.manufacturerTypes, 0x6d616b6572ull);
  uint64_t makerCount = mixIdentity(patch.manufacturers, maker);
  auto part = [](uint64_t hash, unsigned shift) {
    return float((hash >> shift) & 255u) / 255.f;
  };
  // One discrete, restrained palette for the complete subpatch. Parameter
  // families vary in shade and a neighbouring hue, never across a rainbow.
  const float hues[] = {.48f, .25f, .59f, .07f, .52f, .77f};
  unsigned palette = patchPalette(patch);
  // Half the families are circular. The patch chooses their shared edge
  // treatment, with occasional family-wide variations.
  uint64_t theme = mixIdentity(patch.types, maker);
  constexpr unsigned circleWeight = 50;
  unsigned roll = mixIdentity(maker ^ family, module) % 100;
  unsigned unusual = mixIdentity(module, family ^ 0x7368617065ull) % 100;
  int shape = roll < circleWeight ? 6
              : unusual < 52      ? 0
              : unusual < 76      ? 3
              : unusual < 88      ? 5
              : unusual < 96      ? 1
              : unusual < 99      ? 2
                                  : 4;
  bool circle = shape == 6;
  uint64_t edgeFamily = mixIdentity(controlHash(paramFamily(parameter)), theme);
  int border = int((theme >> 24) % 4);
  if (edgeFamily % 5 == 0) border = int((edgeFamily >> 12) % 4);
  uint64_t surface = mixIdentity(family, maker);
  const float grays[] = {.035f, .075f, .13f, .21f, .32f, .46f, .6f, .75f};
  float lightness =
      grays[(surface >> 8) % 8] + (paramNumber(parameter) % 3 - 1) * .008f;
  float ink = lightness < .4f ? .76f + part(detail, 56) * .23f
                              : .035f + part(detail, 56) * .13f;
  bool oblong = !circle && (module >> 48) % 6 == 0;
  float aspect = circle   ? 1.f
                 : oblong ? .74f + part(module, 24) * .12f
                          : .9f + part(module, 24) * .1f;
  return {hues[palette] + (part(family, 8) - .5f) * .025f,
          palette == 4 ? .09f : .28f + part(family, 16) * .18f,
          lightness,
          2.6f + part(maker, 0) * 1.4f + part(module, 0) * 3.8f,
          .025f + part(detail, 8) * .065f,
          (part(detail, 16) - .5f) * .65f,
          .65f + part(detail, 40) * 2.f,
          3 + int((detail >> 24) % 5),
          .7f + part(family, 0) * .7f,
          module & 1 ? aspect : 1.f,
          module & 1 ? 1.f : aspect,
          circle ? .45f + part(detail, 32) * .65f
                 : .7f + part(makerCount, 32) * .8f + part(detail, 32) * .6f,
          shape,
          int((detail >> 48) % (circle ? 2 : 4)),
          ink,
          !circle && ((detail >> 52) % 3 != 0),
          (surface >> 40) % 3 == 0 && (!circle || border == 0),
          border,
          16 + 4 * int((theme >> 40) % 5),
          .45f + part(theme, 48) * .5f};
}
inline NVGcolor patchCableColor(const PatchIdentity& patch, size_t cable) {
  auto style = controlStyle("Cable", "Cable", patch);
  // Cable shades stay independent of the knob's wider black-to-gray range.
  uint64_t family = mixIdentity(controlHash(paramFamily("Cable")), patch.types);
  float lightness = .428f + float((family >> 24) & 255u) / 255.f * .2f;
  const float shades[] = {-.08f, .04f, .14f, -.16f};
  return panelGray(nvgHSL(style.hue + (cable % 3 - 1.f) * .008f,
                          std::min(.65f, style.saturation + .12f),
                          clamp(lightness + shades[cable % 4], .28f, .74f)));
}
// Native Rack interaction and parameter identity, with a cached procedural
// face. No SVG rotation, parameter proxy, new DSP work or per-frame geometry.
struct ParametricKnob : app::Knob {
  ControlStyle style = controlStyle("Parameter", "Module");
  std::array<Vec, 48> profile;
  std::array<float, 64> pointerRadii{};
  std::array<Vec, 64> ridgePoints{};
  float circleRadius = 0;
  widget::FramebufferWidget* faceBuffer;
  std::string identity;
  int channelNumber = 0;
  bool numberedLabel = false;
  bool dark = false;
  computerscare::type::Text numberText;
  void setDark(bool next) {
    if (dark == next) return;
    dark = next;
    faceBuffer->setDirty();
  }
  float lastRange = NAN;
  struct Face : widget::Widget {
    ParametricKnob* owner;
    void draw(const DrawArgs& args) override { owner->drawControl(args); }
  };
  ParametricKnob() {
    box.size = Vec(24, 24);
    minAngle = -.75f * M_PI;
    maxAngle = .75f * M_PI;
    faceBuffer = new widget::FramebufferWidget;
    faceBuffer->box.size = box.size;
    addChild(faceBuffer);
    auto* face = new Face;
    face->owner = this;
    face->box.size = box.size;
    faceBuffer->addChild(face);
    setIdentity("Parameter", "Module");
  }
  void setIdentity(const std::string& parameter, const std::string& source,
                   const PatchIdentity& patch = PatchIdentity()) {
    std::string next =
        source + "\n" + parameter + std::to_string(patch.modules) + ":" +
        std::to_string(patch.types) + ":" + std::to_string(patch.wiring) + ":" +
        std::to_string(patch.manufacturers) + ":" +
        std::to_string(patch.manufacturerTypes);
    if (next == identity) return;
    identity = next;
    channelNumber = paramNumber(parameter);
    numberedLabel =
        std::any_of(parameter.begin(), parameter.end(),
                    [](unsigned char c) { return std::isdigit(c); });
    style = controlStyle(parameter, source, patch);
    uint64_t blendHash =
        mixIdentity(controlHash(paramFamily(parameter)) ^ controlHash(source),
                    mixIdentity(patch.wiring, patch.manufacturers));
    bool circle = style.shape == 6;
    float blend =
        circle ? 0.f : .08f + float((blendHash >> 8) & 255u) / 255.f * .2f;
    float phase = float((blendHash >> 24) & 255u) / 255.f * 2.f * M_PI;
    float dent =
        circle ? 0.f : .015f + float((blendHash >> 40) & 255u) / 255.f * .07f;
    for (size_t i = 0; i < profile.size(); ++i) {
      float angle = float(i) * 2.f * M_PI / profile.size();
      float c = std::cos(angle), s = std::sin(angle);
      float x = c, y = s;
      float radius = 1;
      if (style.shape == 0 || style.shape == 3) {
        x = std::copysign(std::pow(std::abs(c), 2.f / style.exponent), c);
        y = std::copysign(std::pow(std::abs(s), 2.f / style.exponent), s);
        if (style.shape == 3) {
          float skew = (float((blendHash >> 48) & 255u) / 255.f - .5f) * .55f;
          x += skew * y;
          y *= 1.f + .1f * c;
        }
      } else if (style.shape == 1) {
        int sides = 3 + style.lobes % 5;
        float sector = 2.f * M_PI / sides;
        radius =
            std::cos(M_PI / sides) /
            std::cos(std::fmod(angle + M_PI / sides, sector) - M_PI / sides);
      } else if (style.shape == 2) {
        radius = 1 + style.ripple * std::cos(style.lobes * angle);
      } else if (style.shape == 4) {
        radius = i % 4 < 2 ? 1.f : .88f + style.ripple * .4f;
      } else if (!circle) {
        radius = 1 + .19f * std::cos(angle) + .11f * std::sin(3 * angle);
      }
      Vec secondary;
      if (blendHash % 3 == 0) {
        float exponent = 2.5f + float((blendHash >> 16) & 255u) / 255.f * 3.f;
        secondary =
            Vec(std::copysign(std::pow(std::abs(c), 2.f / exponent), c),
                std::copysign(std::pow(std::abs(s), 2.f / exponent), s));
      } else {
        int lobes = blendHash % 3 == 1 ? 2 : 3 + int((blendHash >> 32) % 3);
        float r = 1.f + .22f * std::cos(lobes * angle + phase);
        secondary = Vec(c, s) * r;
      }
      Vec combined = Vec(x, y) * radius * (1 - blend) + secondary * blend;
      float damping =
          1.f - dent * std::pow(std::max(0.f, std::cos(angle - phase)), 8.f);
      x = combined.x * damping;
      y = combined.y * damping;
      radius = 1;
      profile[i] =
          Vec((x * std::cos(style.rotation) - y * std::sin(style.rotation)) *
                  style.aspectX,
              (x * std::sin(style.rotation) + y * std::cos(style.rotation)) *
                  style.aspectY) *
          (7.8f * radius);
    }
    float maximumX = 0, maximumY = 0, maximumRadius = 0;
    for (const auto& point : profile) {
      maximumX = std::max(maximumX, std::abs(point.x));
      maximumY = std::max(maximumY, std::abs(point.y));
      maximumRadius = std::max(maximumRadius, point.norm());
    }
    float fitted = std::min(10.5f / std::max(1.f, maximumX),
                            (11.5f - style.bevel) / std::max(1.f, maximumY));
    fitted =
        std::min(fitted, (11.5f - style.bevel) / std::max(1.f, maximumRadius));
    for (auto& point : profile) point *= fitted;
    circleRadius = circle ? profile[0].norm() : 0;
    if (circle && style.border == 3)
      for (int i = 0; i < style.ridges; ++i) {
        float angle = 2.f * M_PI * (i + .5f) / style.ridges;
        Vec direction(std::cos(angle), std::sin(angle));
        ridgePoints[i * 2] = direction * (circleRadius - .35f);
        ridgePoints[i * 2 + 1] = direction * (circleRadius - 1.35f);
      }
    // Cache the distance to the actual irregular face in each direction.
    // The longer pointer stays inside narrow/triangular profiles too.
    auto cross = [](Vec a, Vec b) { return a.x * b.y - a.y * b.x; };
    for (size_t i = 0; i < pointerRadii.size(); ++i) {
      float angle = i * 2.f * M_PI / pointerRadii.size();
      Vec ray(std::sin(angle), -std::cos(angle));
      float radius = 12;
      for (size_t edge = 0; edge < profile.size(); ++edge) {
        Vec a = profile[edge], side = profile[(edge + 1) % profile.size()] - a;
        float denominator = cross(ray, side);
        if (std::abs(denominator) < .0001f) continue;
        float distance = cross(a, side) / denominator;
        float along = cross(a, ray) / denominator;
        if (distance > 0 && along >= 0 && along <= 1)
          radius = std::min(radius, distance);
      }
      pointerRadii[i] = radius;
    }
    faceBuffer->setDirty();
  }
  void shape(NVGcontext* vg, Vec offset) const {
    nvgBeginPath(vg);
    if (style.shape == 6) {
      nvgCircle(vg, offset.x, offset.y, circleRadius);
      return;
    }
    for (size_t i = 0; i < profile.size(); ++i) {
      Vec p = profile[i] + offset;
      if (!i)
        nvgMoveTo(vg, p.x, p.y);
      else
        nvgLineTo(vg, p.x, p.y);
    }
    nvgClosePath(vg);
  }
  void drawFace(const DrawArgs& args) {
    auto* vg = args.vg;
    shape(vg, Vec(12, 11.5f + style.bevel));
    nvgFillColor(vg, style.color(-.27f, dark));
    nvgFill(vg);
    nvgStrokeColor(vg, dark ? nvgRGB(120, 120, 120) : nvgRGB(29, 29, 29));
    nvgStrokeWidth(vg, .8f);
    nvgStroke(vg);
    shape(vg, Vec(12, 11.5f));
    nvgFillPaint(vg, nvgLinearGradient(
                         vg, 7, 4, 18, 20,
                         style.color(style.shape == 6 ? .04f : .09f, dark),
                         style.color(style.shape == 6 ? -.02f : -.05f, dark)));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGB(32, 32, 32));
    nvgStrokeWidth(vg, .8f);
    nvgStroke(vg);
    if (style.shape == 6 && style.border) {
      auto ink = style.indicatorColor(dark);
      nvgStrokeColor(vg, nvgTransRGBAf(ink, style.border == 3 ? .5f : .42f));
      nvgStrokeWidth(vg, style.borderWidth);
      nvgBeginPath(vg);
      nvgCircle(vg, 12, 11.5f,
                circleRadius - (style.border == 3 ? 1.5f : .75f));
      if (style.border == 2) nvgCircle(vg, 12, 11.5f, circleRadius - 1.65f);
      nvgStroke(vg);
      if (style.border == 3) {
        nvgBeginPath(vg);
        for (int i = 0; i < style.ridges; ++i) {
          Vec a = Vec(12, 11.5f) + ridgePoints[i * 2],
              b = Vec(12, 11.5f) + ridgePoints[i * 2 + 1];
          nvgMoveTo(vg, a.x, a.y);
          nvgLineTo(vg, b.x, b.y);
        }
        nvgLineCap(vg, NVG_BUTT);
        nvgStrokeWidth(vg, style.borderWidth * .65f);
        nvgStroke(vg);
      }
    }
    if (style.innerRing) {
      nvgBeginPath(vg);
      nvgEllipse(vg, 12, 11.5f, 4.7f * style.aspectX, 4.7f * style.aspectY);
      nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 32));
      nvgStrokeWidth(vg, .45f);
      nvgStroke(vg);
    }
  }
  void step() override {
    auto* q = getParamQuantity();
    float range = q ? q->getRange() : 0;
    if (range != lastRange) {
      lastRange = range;
      faceBuffer->setDirty();
    }
    app::Knob::step();
  }
  void onChange(const ChangeEvent& e) override {
    faceBuffer->setDirty();
    app::Knob::onChange(e);
  }
  void drawControl(const DrawArgs& args) {
    auto* q = getParamQuantity();
    float value = q ? (q->isBounded() ? q->getScaledValue()
                                      : std::fmod(q->getValue(), 1.f))
                    : .5f;
    if (!std::isfinite(value)) value = 0;
    float angle = minAngle + (maxAngle - minAngle) * clamp(value, 0.f, 1.f);
    auto* vg = args.vg;
    if (snap && q && q->getRange() >= 1) {
      int ticks =
          std::max(2, std::min(17, int(std::min(16.f, q->getRange())) + 1));
      nvgBeginPath(vg);
      for (int i = 0; i < ticks; ++i) {
        float angle = minAngle + (maxAngle - minAngle) * i / (ticks - 1);
        Vec direction(std::sin(angle), -std::cos(angle));
        Vec a = Vec(12, 12) + direction * 10.2f,
            b = Vec(12, 12) + direction * 11.5f;
        nvgMoveTo(vg, a.x, a.y);
        nvgLineTo(vg, b.x, b.y);
      }
      nvgStrokeWidth(vg, .65f);
      nvgStrokeColor(vg, dark ? nvgRGB(205, 205, 205) : style.color(-.3f));
      nvgStroke(vg);
    }
    nvgSave(vg);
    nvgTranslate(vg, 12, 11.5f);
    nvgRotate(vg, angle);
    nvgTranslate(vg, -12, -11.5f);
    drawFace(args);
    // The cached body and pointer rotate together; scale marks and numbers
    // stay upright. The normalized profile fits at every angle.
    angle = 0;
    Vec direction(std::sin(angle), -std::cos(angle));
    float index = std::fmod(angle + 2.f * M_PI, 2.f * M_PI) *
                  pointerRadii.size() / (2.f * M_PI);
    int lower = int(index) % pointerRadii.size();
    float edge =
        pointerRadii[lower] + (pointerRadii[(lower + 1) % pointerRadii.size()] -
                               pointerRadii[lower]) *
                                  (index - int(index));
    float length = edge * (.76f + style.ripple * .55f);
    Vec center(12, 11.5f),
        end =
            center + direction * std::max(1.5f, length - style.pointer * .35f);
    const auto ink = style.indicatorColor(dark);
    nvgStrokeColor(args.vg, ink);
    nvgLineCap(args.vg, NVG_ROUND);
    if (snap) {
      nvgBeginPath(args.vg);
      nvgCircle(args.vg, end.x, end.y, 1.45f);
      nvgFillColor(args.vg, ink);
      nvgFill(args.vg);
    } else {
      Vec start = center + direction * (.4f + style.pointer * .25f);
      nvgBeginPath(vg);
      if (style.indicator < 2) {
        nvgLineCap(vg, style.indicator == 0 ? NVG_BUTT : NVG_ROUND);
        nvgMoveTo(vg, start.x, start.y);
        nvgLineTo(vg, end.x, end.y);
        nvgStrokeWidth(vg, style.pointer);
        nvgStroke(vg);
      } else {
        float half = style.pointer * .85f;
        nvgMoveTo(vg, start.x - half, start.y);
        nvgLineTo(vg, start.x + half,
                  start.y + (style.indicator == 3 ? .5f : 0));
        if (style.indicator == 3) nvgLineTo(vg, end.x + half * .6f, end.y + 1);
        nvgLineTo(vg, end.x, end.y);
        nvgClosePath(vg);
        nvgFillColor(vg, ink);
        nvgFill(vg);
      }
      if (style.tipDot) {
        nvgBeginPath(vg);
        nvgCircle(vg, end.x, end.y, .7f + style.pointer * .3f);
        nvgFillColor(vg, ink);
        nvgFill(vg);
      }
    }
    nvgRestore(vg);
    if ((snap && q && q->getRange() <= 32) || numberedLabel) {
      auto font = computerscare::type::font();
      if (font) {
        const auto& number = numberText.get(std::to_string(
            numberedLabel ? channelNumber : int(std::round(q->getValue()))));
        nvgFontFaceId(args.vg, font->handle);
        nvgFontSize(args.vg, numberedLabel ? 5 : 6);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(args.vg, ink);
        nvgText(args.vg, 12, numberedLabel ? 14.5f : 12, number.c_str(),
                nullptr);
      }
    }
  }
};
}  // namespace beekapoo
