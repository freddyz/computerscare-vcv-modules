#pragma once
#include <array>
#include <cstdint>

#include "BeekapooIdentity.hpp"
#include "BeekapooParamLayout.hpp"
#include "Computerscare.hpp"

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
  int shape;
  NVGcolor color(float shade = 0) const {
    return panelGray(
        nvgHSL(hue, saturation, clamp(lightness + shade, .12f, .8f)));
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
  float aspect = .55f + part(maker, 24) * .2f + part(module, 24) * .25f;
  return {
      hues[palette] + (part(family, 8) - .5f) * .025f,
      palette == 4 ? .09f : .28f + part(family, 16) * .18f,
      .44f + part(family, 24) * .2f + (paramNumber(parameter) % 3 - 1) * .012f,
      1.15f + part(maker, 0) * 3.8f + part(module, 0) * 2.f,
      .04f + part(detail, 8) * .17f,
      (part(detail, 16) - .5f) * .65f,
      .7f + part(module, 8) * 1.4f,
      3 + int((detail >> 24) % 7),
      .7f + part(family, 0) * .7f,
      module & 1 ? aspect : 1.f,
      module & 1 ? 1.f : aspect,
      .8f + part(makerCount, 32) * 1.1f + part(detail, 32) * .7f,
      int((maker % 3) * 2 + ((patch.types + family + module) % 2))};
}
inline NVGcolor patchCableColor(const PatchIdentity& patch, size_t cable) {
  auto style = controlStyle("Cable", "Cable", patch);
  const float shades[] = {-.08f, .04f, .14f, -.16f};
  return panelGray(
      nvgHSL(style.hue + (cable % 3 - 1.f) * .008f,
             std::min(.65f, style.saturation + .12f),
             clamp(style.lightness + shades[cable % 4], .28f, .74f)));
}
// Native Rack interaction and parameter identity, with a cached procedural
// face. No SVG rotation, parameter proxy, new DSP work or per-frame geometry.
struct ParametricKnob : app::Knob {
  ControlStyle style = controlStyle("Parameter", "Module");
  std::array<Vec, 48> profile;
  widget::FramebufferWidget* faceBuffer;
  std::string identity;
  int channelNumber = 0;
  bool numberedLabel = false;
  float lastRange = NAN;
  struct Face : widget::Widget {
    ParametricKnob* owner;
    void draw(const DrawArgs& args) override { owner->drawFace(args); }
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
    float blend = .22f + float((blendHash >> 8) & 255u) / 255.f * .32f;
    float phase = float((blendHash >> 24) & 255u) / 255.f * 2.f * M_PI;
    float dent = .03f + float((blendHash >> 40) & 255u) / 255.f * .15f;
    for (size_t i = 0; i < profile.size(); ++i) {
      float angle = float(i) * 2.f * M_PI / profile.size();
      float c = std::cos(angle), s = std::sin(angle);
      float x = c, y = s;
      float radius = 1;
      if (style.shape == 0 || style.shape == 3) {
        x = std::copysign(std::pow(std::abs(c), 2.f / style.exponent), c);
        y = std::copysign(std::pow(std::abs(s), 2.f / style.exponent), s);
        if (style.shape == 3) x += .2f * y;
      } else if (style.shape == 1) {
        int sides = 3 + style.lobes % 5;
        float sector = 2.f * M_PI / sides;
        radius =
            std::cos(M_PI / sides) /
            std::cos(std::fmod(angle + M_PI / sides, sector) - M_PI / sides);
      } else if (style.shape == 2) {
        radius = 1 + style.ripple * std::cos(style.lobes * angle);
      } else if (style.shape == 4) {
        radius = i % 4 < 2 ? 1.f : .77f;
      } else {
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
    float maximumX = 0, maximumY = 0;
    for (const auto& point : profile) {
      maximumX = std::max(maximumX, std::abs(point.x));
      maximumY = std::max(maximumY, std::abs(point.y));
    }
    float fitted = std::min(10.5f / std::max(1.f, maximumX),
                            (11.5f - style.bevel) / std::max(1.f, maximumY));
    for (auto& point : profile) point *= fitted;
    faceBuffer->setDirty();
  }
  void shape(NVGcontext* vg, Vec offset) const {
    nvgBeginPath(vg);
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
    auto* q = getParamQuantity();
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
      nvgStrokeColor(vg, style.color(-.3f));
      nvgStroke(vg);
    }
    shape(vg, Vec(12, 11.5f + style.bevel));
    nvgFillColor(vg, style.color(-.27f));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGB(29, 29, 29));
    nvgStrokeWidth(vg, .8f);
    nvgStroke(vg);
    shape(vg, Vec(12, 11.5f));
    nvgFillPaint(vg, nvgLinearGradient(vg, 7, 4, 18, 20, style.color(.12f),
                                       style.color(-.06f)));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGB(32, 32, 32));
    nvgStrokeWidth(vg, .8f);
    nvgStroke(vg);
    nvgBeginPath(vg);
    nvgEllipse(vg, 12, 11.5f, 4.7f * style.aspectX, 4.7f * style.aspectY);
    nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 45));
    nvgStrokeWidth(vg, .45f);
    nvgStroke(vg);
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
  void draw(const DrawArgs& args) override {
    app::Knob::draw(args);
    auto* q = getParamQuantity();
    float value = q ? (q->isBounded() ? q->getScaledValue()
                                      : std::fmod(q->getValue(), 1.f))
                    : .5f;
    if (!std::isfinite(value)) value = 0;
    float angle = minAngle + (maxAngle - minAngle) * clamp(value, 0.f, 1.f);
    Vec direction(std::sin(angle), -std::cos(angle));
    Vec center(12, 11.5f), end = center + Vec(direction.x * style.aspectX,
                                              direction.y * style.aspectY) *
                                              (style.shape == 1 ? 2.6f : 4.7f);
    nvgStrokeColor(args.vg, nvgRGB(26, 26, 26));
    nvgLineCap(args.vg, NVG_ROUND);
    if (snap) {
      nvgBeginPath(args.vg);
      nvgCircle(args.vg, end.x, end.y, 1.25f);
      nvgFillColor(args.vg, nvgRGB(26, 26, 26));
      nvgFill(args.vg);
    } else {
      Vec start =
          center +
          Vec(direction.x * style.aspectX, direction.y * style.aspectY) * 1.4f;
      nvgBeginPath(args.vg);
      nvgMoveTo(args.vg, start.x, start.y);
      nvgLineTo(args.vg, end.x, end.y);
      nvgStrokeWidth(args.vg, style.pointer);
      nvgStroke(args.vg);
    }
    if ((snap && q && q->getRange() <= 32) || numberedLabel) {
      auto font =
          APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
      if (font) {
        auto number = std::to_string(
            numberedLabel ? channelNumber : int(std::round(q->getValue())));
        nvgFontFaceId(args.vg, font->handle);
        nvgFontSize(args.vg, numberedLabel ? 5 : 6);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(args.vg, nvgRGB(28, 28, 28));
        nvgText(args.vg, 12, numberedLabel ? 14.5f : 12, number.c_str(),
                nullptr);
      }
    }
  }
};
}  // namespace beekapoo
