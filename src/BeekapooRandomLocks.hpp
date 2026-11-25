#pragma once
#include "Computerscare.hpp"

namespace beekapoo {
inline float randomRangeValue(engine::ParamQuantity* q, float value,
                              float range) {
  float zero = clamp(0.f, q->getMinValue(), q->getMaxValue());
  return zero + (value - zero) * range;
}
// Execute the native callback and restore protected values under the same
// engine lock. A custom onRandomize() may ignore randomizeEnabled entirely.
inline void randomizeWithLocks(engine::Module* source,
                               const std::set<int>& protectedParams,
                               float range = 1.f, float probability = 1.f) {
  struct Callback : engine::Module {
    engine::Module* source;
    std::vector<std::pair<engine::ParamQuantity*, float>> saved;
    std::vector<engine::ParamQuantity*> selected;
    float range;
    Callback(engine::Module* source, const std::set<int>& ids, float range,
             float probability)
        : source(source), range(range) {
      for (auto* q : source->paramQuantities) {
        if (!q) continue;
        if (ids.count(q->paramId) ||
            (probability < 1.f && random::uniform() >= probability))
          saved.push_back({q, q->getValue()});
        else if (q->isBounded())
          selected.push_back(q);
      }
    }
    void onRandomize(const RandomizeEvent& event) override {
      struct Restore {
        Callback* callback;
        ~Restore() {
          for (auto& item : callback->saved)
            item.first->setImmediateValue(item.second);
        }
      } restore{this};
      source->onRandomize(event);
      if (range < 1.f)
        for (auto* q : selected)
          q->setImmediateValue(randomRangeValue(q, q->getValue(), range));
    }
  } callback(source, protectedParams, range, probability);
  APP->engine->randomizeModule(&callback);
}

inline void drawRandomLock(NVGcontext* vg, Vec center, float size) {
  nvgSave(vg);
  nvgTranslate(vg, center.x, center.y);
  nvgScale(vg, size / 16.f, size / 16.f);
  nvgBeginPath(vg);
  nvgMoveTo(vg, -4, 0);
  nvgLineTo(vg, -4, -3);
  nvgBezierTo(vg, -4, -9, 5, -9, 5, -3);
  nvgLineTo(vg, 5, 0);
  nvgStrokeColor(vg, nvgRGB(24, 29, 26));
  nvgStrokeWidth(vg, 1.8f);
  nvgStroke(vg);
  nvgBeginPath(vg);
  nvgMoveTo(vg, -6, -1);
  nvgLineTo(vg, 6, -.5f);
  nvgLineTo(vg, 5, 7);
  nvgLineTo(vg, -5, 6);
  nvgClosePath(vg);
  nvgFillColor(vg, nvgRGB(24, 29, 26));
  nvgFill(vg);
  nvgBeginPath(vg);
  nvgMoveTo(vg, 0, 1);
  nvgLineTo(vg, .5f, 4);
  nvgStrokeColor(vg, nvgRGB(218, 223, 220));
  nvgStrokeWidth(vg, 1.4f);
  nvgStroke(vg);
  nvgRestore(vg);
}
}  // namespace beekapoo
