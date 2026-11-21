#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
#include "ComputerscareFontGlyphs.hpp"

// Use Rack's actual font rasterizer. No Python font tooling / display needed.
int main() {
  using namespace computerscare::type;
  for (const char* path : {"res/fonts/computerscare/ComputerscareText.ttf",
                           "res/fonts/computerscare/ComputerscareDisplay.ttf"}) {
    std::ifstream file(path, std::ios::binary);
    assert(file.good());
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)), {});
    stbtt_fontinfo font;
    assert(stbtt_InitFont(&font, bytes.data(), 0));
    for (size_t i = 0; i < sizeof(glyphCodepoints) / sizeof(*glyphCodepoints); ++i) {
      uint32_t cp = glyphCodepoints[i];
      int base = stbtt_FindGlyphIndex(&font, cp);
      assert(base > 0);
      int advance = 0, bearing = 0;
      stbtt_GetGlyphHMetrics(&font, base, &advance, &bearing);
      for (unsigned variant = 0; variant < glyphVariants; ++variant) {
        int index = stbtt_FindGlyphIndex(&font, glyphPrivateBase + i * glyphVariants + variant);
        assert(index > 0);
        assert(variant != 0 || index == base);
        int width = 0;
        stbtt_GetGlyphHMetrics(&font, index, &width, &bearing);
        assert(width == advance);
        int left, bottom, right, top;
        if (stbtt_GetGlyphBox(&font, index, &left, &bottom, &right, &top)) {
          assert(!width || (left >= 0 && right <= width));
          assert(top > bottom && right > left);
          // Every alternate must rasterize at a realistic panel text size.
          int w = 0, h = 0;
          auto pixels = stbtt_GetGlyphBitmap(&font, 0, stbtt_ScaleForPixelHeight(&font, 18), index, &w, &h, nullptr, nullptr);
          assert(pixels && w > 0 && h > 0);
          stbtt_FreeBitmap(pixels, nullptr);
        }
      }
    }
  }
  std::cout << "computerscare fonts: bundled files, all glyphs/alternates, shared advances, bounds and rasterization passed\n";
}
