#pragma once

#include <array>
#include <atomic>
#include <cmath>

#include "Computerscare.hpp"

// Audio-side RMS envelope and a short peak hold, published at a reduced
// rate.
struct ComputerscareMeterSignal {
  std::atomic<float> level{0.f}, peak{0.f};
  float power = 0.f, held = 0.f, holdTime = 0.f;
  unsigned divider = 0;
  void process(float left, float right, float dt) {
    float target = (left * left + right * right) * .5f;
    power += (target - power) * std::min(1.f, dt / .02f);
    float instantaneous = std::max(std::fabs(left), std::fabs(right));
    if (instantaneous >= held) {
      held = instantaneous;
      holdTime = .4f;
    } else if (holdTime > 0.f) {
      holdTime -= dt;
    } else {
      held *= std::max(0.f, 1.f - dt * 3.f);
    }
    if (++divider == 32) {
      divider = 0;
      level.store(std::sqrt(std::max(0.f, power)), std::memory_order_relaxed);
      peak.store(held, std::memory_order_relaxed);
    }
  }
};

// Small, hand-warped segmented meter. 5 V RMS is the 0 dB reference.
struct ComputerscareMeter : widget::TransparentWidget {
  ComputerscareMeterSignal* signal = nullptr;
  const char* label = "IN";
  unsigned shapeSeed = 1;
  static constexpr int maxSegments = 18;
  int segmentCount() const {
    unsigned bits = shapeSeed * 0x9e3779b9u;
    bits ^= bits >> 16;
    return 8 + int(bits % 11);
  }
  std::array<float, maxSegments + 1> segmentEdges() const {
    int segments = segmentCount();
    std::array<float, maxSegments + 1> edges{};
    std::array<float, maxSegments> weights{};
    float shortest = 1.f, longest = 0.f;
    for (int i = 0; i < segments; ++i) {
      unsigned bits = shapeSeed * 0x9e3779b9u + unsigned(i + 1) * 0x85ebca6bu;
      bits ^= bits >> 16;
      bits *= 0x7feb352du;
      bits ^= bits >> 15;
      weights[i] = float(bits & 1023u) / 1023.f;
      shortest = std::min(shortest, weights[i]);
      longest = std::max(longest, weights[i]);
    }
    float total = 0.f;
    for (int i = 0; i < segments; ++i) {
      float random = longest > shortest
                         ? (weights[i] - shortest) / (longest - shortest)
                         : float(i) / (segments - 1);
      total += 1.f + 2.f * random;
      edges[i + 1] = total;
    }
    float width = box.size.x - (label[0] ? 15.f : 2.f) - 2.f;
    float visibleWidth = std::max(0.f, width - segments * .6f);
    for (int i = 1; i <= segments; ++i)
      edges[i] = (edges[i] / total * visibleWidth + i * .6f) / width;
    return edges;
  }
  static float normalized(float volts) {
    float db = 20.f * std::log10(std::max(1e-4f, volts / 5.f));
    return clamp((db + 48.f) / 54.f, 0.f, 1.f);
  }
  static NVGcolor segmentColor(float position, bool held) {
    return held              ? nvgRGB(25, 116, 94)
           : position >= .9f ? nvgRGB(145, 46, 44)
           : position >= .8f ? nvgRGB(139, 143, 74)
                             : nvgRGB(25, 116, 94);
  }
  void draw(const DrawArgs& args) override {
    float level =
        signal ? normalized(signal->level.load(std::memory_order_relaxed))
               : 0.f;
    float peak =
        signal ? normalized(signal->peak.load(std::memory_order_relaxed)) : 0.f;
    int segments = segmentCount();
    auto edges = segmentEdges();
    float left = label[0] ? 15.f : 2.f;
    float width = box.size.x - left - 2.f;
    for (int i = 0; i < segments; ++i) {
      float x = left + width * edges[i];
      float segmentWidth = width * (edges[i + 1] - edges[i]);
      float wobble = float((unsigned(i) * 7 + shapeSeed * 3) % 5) * .18f;
      nvgBeginPath(args.vg);
      nvgMoveTo(args.vg, x, 2.f + wobble);
      nvgLineTo(args.vg, x + segmentWidth - .6f, 1.5f + wobble);
      nvgLineTo(args.vg, x + segmentWidth - .8f, box.size.y - 2.f);
      nvgLineTo(args.vg, x + .2f, box.size.y - 2.5f - wobble);
      nvgClosePath(args.vg);
      bool lit = level > edges[i];
      bool held = peak > edges[i] && peak <= edges[i + 1];
      nvgFillColor(args.vg, lit ? segmentColor(edges[i], false)
                                : nvgRGBA(69, 85, 78, 65));
      nvgFill(args.vg);
      if (held) {
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, x + segmentWidth - .6f, 1.5f + wobble);
        nvgLineTo(args.vg, x + segmentWidth - .8f, box.size.y - 2.f);
        nvgStrokeColor(args.vg, segmentColor(edges[i], true));
        nvgStrokeWidth(args.vg, 2.4f);
        nvgStroke(args.vg);
      }
    }
    auto font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/Oswald-Regular.ttf"));
    if (font) {
      nvgFontFaceId(args.vg, font->handle);
      nvgFontSize(args.vg, 7.f);
      nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
      nvgFillColor(args.vg, nvgRGB(15, 42, 36));
      nvgText(args.vg, 0, box.size.y * .5f, label, nullptr);
    }
  }
  void drawLayer(const DrawArgs& args, int layer) override {
    if (layer == 1 && !args.fb && signal && settings::haloBrightness > 0.f) {
      float level = normalized(signal->level.load(std::memory_order_relaxed));
      float peak = normalized(signal->peak.load(std::memory_order_relaxed));
      int segments = segmentCount();
      auto edges = segmentEdges();
      float left = label[0] ? 15.f : 2.f;
      float width = box.size.x - left - 2.f;
      nvgSave(args.vg);
      nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
      for (int i = 0; i < segments; ++i) {
        bool held = peak > edges[i] && peak <= edges[i + 1];
        if (level <= edges[i] && !held) continue;
        float x = left + width * edges[i];
        float segmentWidth = width * (edges[i + 1] - edges[i]);
        float glowWidth = segmentWidth - .6f;
        if (held) {
          x += glowWidth - 1.2f;
          glowWidth = 2.4f;
        }
        NVGcolor color = segmentColor(edges[i], held);
        color.a = .10f * settings::haloBrightness;
        nvgBeginPath(args.vg);
        nvgRect(args.vg, x - 5.f, -4.f, segmentWidth + 10.f, box.size.y + 8.f);
        nvgFillPaint(args.vg, nvgBoxGradient(args.vg, x, 2.f, glowWidth,
                                             box.size.y - 4.f, 1.f, 5.f, color,
                                             nvgRGBA(0, 0, 0, 0)));
        nvgFill(args.vg);
      }
      nvgRestore(args.vg);
    }
    widget::TransparentWidget::drawLayer(args, layer);
  }
};
