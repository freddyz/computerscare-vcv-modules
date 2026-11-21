#pragma once

#include <map>
#include <memory>

#include "Computerscare.hpp"

namespace beekapoo {
// Stored values: 0 follows the shared default (or Rack), 1 light, 2 dark.
inline int validPanelTheme(int value) {
  return value >= 0 && value <= 2 ? value : 0;
}
inline bool resolveDarkTheme(int local, int shared, bool rackDark) {
  int choice = validPanelTheme(local);
  if (!choice) choice = validPanelTheme(shared);
  return choice ? choice == 2 : rackDark;
}
inline const char* panelThemeName(int choice) {
  return choice == 1 ? "Light" : choice == 2 ? "Dark" : "Default";
}

// Separate cached SVGs keep the editable source artwork and Rack's shared
// light SVG intact. Recolor only the copy, once, never during drawing.
inline std::shared_ptr<window::Svg> darkHeaderSvg(const std::string& path) {
  static std::map<std::string, std::shared_ptr<window::Svg>> cache;
  auto& svg = cache[path];
  if (!svg) {
    svg = std::make_shared<window::Svg>();
    svg->loadFile(path);
    for (auto* shape = svg->handle ? svg->handle->shapes : nullptr; shape;
         shape = shape->next) {
      for (auto* paint : {&shape->fill, &shape->stroke}) {
        if (paint->type != NSVG_PAINT_COLOR) continue;
        unsigned color = paint->color;
        unsigned gray =
            ((color & 255) + ((color >> 8) & 255) + ((color >> 16) & 255)) / 3;
        unsigned value = gray < 128 ? 230 : 35;
        paint->color =
            (color & 0xff000000u) | value | (value << 8) | (value << 16);
      }
    }
  }
  return svg;
}
// Beek-specific copies: never recolor Rack's cached shared component SVG.
inline std::shared_ptr<window::Svg> darkSmallKnobSvg() {
  static std::shared_ptr<window::Svg> svg;
  if (!svg) {
    svg = std::make_shared<window::Svg>();
    svg->loadFile(asset::plugin(
        pluginInstance, "res/components/computerscare-small-knob-effed.svg"));
    for (auto* shape = svg->handle ? svg->handle->shapes : nullptr; shape;
         shape = shape->next) {
      bool pointer = std::string(shape->id) == "Button_Pointer";
      if (shape->fill.type == NSVG_PAINT_COLOR)
        shape->fill.color = pointer ? 0xffc4d3a8u : 0xff5e6722u;
      if (shape->stroke.type == NSVG_PAINT_COLOR)
        shape->stroke.color = 0xff11130au;
    }
  }
  return svg;
}
struct ThemedSmallKnob : SmallKnob {
  bool dark = false;
  void setDark(bool next) {
    if (dark == next) return;
    dark = next;
    auto size = box.size;
    setSvg(dark ? darkSmallKnobSvg()
                : APP->window->loadSvg(asset::plugin(
                      pluginInstance,
                      "res/components/computerscare-small-knob-effed.svg")));
    box.size = size;
  }
};
}  // namespace beekapoo
