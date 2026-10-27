#pragma once
#include <array>

#include "ComputerscareTextEditorTypes.hpp"

namespace computerscare {
namespace text_editor {
// Semantic colors only: themes never change font metrics, layout or transport.
struct Theme {
  const char* name = "Moss";
  const char* description = "Dark";
  NVGcolor background = nvgRGB(5, 6, 8), text = nvgRGB(238, 238, 234);
  NVGcolor selection = nvgRGBA(36, 201, 166, 133),
           placeholder = nvgRGBA(238, 238, 234, 102);
  NVGcolor stripe = nvgRGBA(255, 255, 255, 22);
  NVGcolor inactiveEven = nvgRGB(43, 45, 46), inactiveOdd = nvgRGB(51, 53, 54);
  NVGcolor focus = nvgRGBA(184, 184, 184, 66),
           controlFocus = nvgRGBA(255, 238, 154, 28);
  NVGcolor yellow = nvgRGB(255, 238, 154), green = COLOR_COMPUTERSCARE_GREEN;
  NVGcolor token = nvgRGB(200, 159, 22), caption = nvgRGBA(10, 15, 13, 235);
  NVGcolor wall = nvgRGB(173, 158, 98);
  void apply(ComputerscareTextEditorStyle& style) const {
    style.backgroundColor = background;
    style.textColor = text;
    style.selectionColor = selection;
    style.placeholderColor = placeholder;
  }
};
inline const Theme& editorTheme(int id) {
  static const std::array<Theme, 4> themes = [] {
    std::array<Theme, 4> result;
    auto& light = result[1];
    light.name = "Mauer";
    light.description = "Light";
    light.background = nvgRGB(247, 245, 235);
    light.text = nvgRGB(40, 48, 46);
    light.selection = nvgRGBA(18, 137, 101, 65);
    light.placeholder = nvgRGBA(40, 48, 46, 115);
    light.stripe = nvgRGBA(40, 48, 46, 13);
    light.inactiveEven = nvgRGB(231, 230, 220);
    light.inactiveOdd = nvgRGB(222, 222, 212);
    light.focus = nvgRGBA(37, 75, 65, 24);
    light.controlFocus = nvgRGBA(157, 116, 20, 24);
    light.yellow = nvgRGB(136, 91, 7);
    light.green = nvgRGB(7, 127, 87);
    light.token = nvgRGB(197, 151, 38);
    light.caption = nvgRGBA(40, 48, 46, 245);
    auto& solar = result[2];
    solar.name = "Garnett";
    solar.description = "Solar";
    solar.background = nvgRGB(0, 43, 54);
    solar.text = nvgRGB(213, 217, 195);
    solar.selection = nvgRGBA(42, 161, 152, 115);
    solar.placeholder = nvgRGBA(147, 161, 161, 120);
    solar.stripe = nvgRGBA(147, 161, 161, 19);
    solar.inactiveEven = nvgRGB(18, 58, 66);
    solar.inactiveOdd = nvgRGB(25, 66, 73);
    solar.focus = nvgRGBA(147, 161, 161, 45);
    solar.controlFocus = nvgRGBA(181, 137, 0, 32);
    solar.yellow = nvgRGB(221, 184, 74);
    solar.green = nvgRGB(67, 194, 157);
    solar.token = nvgRGB(181, 137, 0);
    solar.caption = nvgRGBA(0, 37, 47, 242);
    auto& cool = result[3];
    cool.name = "Puckett";
    cool.description = "Cool dark";
    cool.background = nvgRGB(25, 27, 44);
    cool.text = nvgRGB(225, 230, 244);
    cool.selection = nvgRGBA(82, 183, 179, 115);
    cool.placeholder = nvgRGBA(180, 188, 212, 115);
    cool.stripe = nvgRGBA(175, 186, 226, 19);
    cool.inactiveEven = nvgRGB(41, 44, 64);
    cool.inactiveOdd = nvgRGB(48, 52, 73);
    cool.focus = nvgRGBA(180, 191, 220, 40);
    cool.controlFocus = nvgRGBA(224, 202, 135, 28);
    cool.yellow = nvgRGB(235, 207, 139);
    cool.green = nvgRGB(91, 215, 179);
    cool.token = nvgRGB(178, 145, 59);
    cool.caption = nvgRGBA(22, 24, 41, 242);
    return result;
  }();
  return themes[std::max(0, std::min(3, id))];
}
inline int editorThemeId(const std::string& name) {
  for (int i = 0; i < 4; ++i)
    if (name == editorTheme(i).name) return i;
  return 0;
}
}  // namespace text_editor
}  // namespace computerscare
