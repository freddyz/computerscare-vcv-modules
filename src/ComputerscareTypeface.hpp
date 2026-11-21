#pragma once
#include <algorithm>
#include <iterator>
#include <string>

#include "ComputerscareFontGlyphs.hpp"
#include "plugin.hpp"

namespace computerscare {
namespace type {
enum class Face { Text, Display };
inline std::shared_ptr<window::Font> font(Face face = Face::Text) {
  if (face == Face::Display)
    return APP->window->loadFont(asset::plugin(
        pluginInstance, "res/fonts/computerscare/ComputerscareDisplay.ttf"));
  return APP->window->loadFont(asset::plugin(
      pluginInstance, "res/fonts/computerscare/ComputerscareText.ttf"));
}
inline int handle(Face face = Face::Text) {
  auto loaded = font(face);
  return loaded ? loaded->handle : APP->window->uiFont->handle;
}
inline uint64_t mix(uint64_t x) {
  x ^= x >> 30;
  x *= 0xbf58476d1ce4e5b9ull;
  x ^= x >> 27;
  x *= 0x94d049bb133111ebull;
  return x ^ (x >> 31);
}
// Private-use codepoints choose outlines only for drawing. Stored strings,
// clipboard contents and accessibility names always remain ordinary Unicode.
// Whitespace is preserved so NanoVG's native word wrapping still works.
inline std::string alternates(const std::string& source, uint64_t seed = 0,
                              int fixedVariant = -1) {
  std::string result;
  result.reserve(source.size() * 3);
  size_t index = 0;
  for (size_t i = 0; i < source.size(); ++index) {
    size_t start = i;
    unsigned char lead = source[i++];
    uint32_t cp = lead;
    int count = lead < 0x80             ? 0
                : (lead & 0xe0) == 0xc0 ? 1
                : (lead & 0xf0) == 0xe0 ? 2
                : (lead & 0xf8) == 0xf0 ? 3
                                        : -1;
    if (count > 0) {
      cp &= (1u << (6 - count)) - 1;
      for (int n = 0; n < count; ++n) {
        if (i == source.size() ||
            (static_cast<unsigned char>(source[i]) & 0xc0) != 0x80) {
          count = -1;
          break;
        }
        cp = (cp << 6) | (static_cast<unsigned char>(source[i++]) & 0x3f);
      }
    }
    auto* found = std::lower_bound(std::begin(glyphCodepoints),
                                   std::end(glyphCodepoints), cp);
    if (count < 0 || cp == ' ' || cp == 0xa0 ||
        found == std::end(glyphCodepoints) || *found != cp) {
      result.append(source, start, i - start);
      continue;
    }
    unsigned variant =
        fixedVariant >= 0
            ? unsigned(fixedVariant) % glyphVariants
            : mix(seed ^ (uint64_t(cp) << 32) ^ index) % glyphVariants;
    uint32_t alias = glyphPrivateBase +
                     (found - std::begin(glyphCodepoints)) * glyphVariants +
                     variant;
    result += char(0xe0 | (alias >> 12));
    result += char(0x80 | ((alias >> 6) & 0x3f));
    result += char(0x80 | (alias & 0x3f));
  }
  return result;
}
struct Text {
  std::string source, glyphs;
  uint64_t salt = 0;
  const std::string& get(const std::string& text, uint64_t seed = 0) {
    if (source != text || salt != seed) {
      source = text;
      salt = seed;
      glyphs = alternates(text, seed);
    }
    return glyphs;
  }
};
// Rack's native text field uses Blendish for both hit testing and drawing.
// Scope the font to those operations, preserving its cursor/clipboard logic.
struct Scope {
  Scope() { bndSetFont(handle()); }
  ~Scope() { bndSetFont(APP->window->uiFont->handle); }
};
struct Field : ui::TextField {
  void draw(const DrawArgs& args) override {
    Scope fontScope;
    ui::TextField::draw(args);
  }
  int getTextPosition(Vec position) override {
    Scope fontScope;
    return ui::TextField::getTextPosition(position);
  }
};
struct Tooltip : ui::Tooltip {
  void step() override {
    Scope fontScope;
    ui::Tooltip::step();
  }
  void draw(const DrawArgs& args) override {
    Scope fontScope;
    ui::Tooltip::draw(args);
  }
};
struct Button : ui::Button {
  void draw(const DrawArgs& args) override {
    Scope fontScope;
    ui::Button::draw(args);
  }
};
}  // namespace type
}  // namespace computerscare
