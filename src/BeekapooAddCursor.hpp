#pragma once
#include <array>
#include <cmath>
#include <rack.hpp>

namespace beekapoo {
// One small arrow-and-plus bitmap, shared by the native cursor and previews.
// GLFW owns its lifetime through termination, like Rack's standard cursors.
struct AddCursor {
  enum { size = 24 };
  using Pixels = std::array<unsigned char, size * size * 4>;
  static const Pixels& pixels() {
    static const Pixels image = [] {
      Pixels result{};
      const rack::math::Vec outline[] = {{1, 1},   {1, 18}, {5, 14}, {8, 21},
                                         {11, 20}, {8, 13}, {15, 13}};
      for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
          rack::math::Vec p(x + .5f, y + .5f);
          bool inside = false;
          float distance = 100;
          for (int i = 0, j = 6; i < 7; j = i++) {
            auto a = outline[i], b = outline[j], d = b - a;
            if ((a.y > p.y) != (b.y > p.y) &&
                p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x)
              inside = !inside;
            float t = rack::math::clamp((p - a).dot(d) / d.dot(d), 0.f, 1.f);
            distance = std::min(distance, (p - a - d * t).norm());
          }
          int shade = inside ? (distance < 1.f ? 255 : 20) : -1;
          if (x >= 14 && x <= 22 && y >= 14 && y <= 22)
            shade = x == 14 || x == 22 || y == 14 || y == 22 ||
                            (x == 18 && y >= 16 && y <= 20) ||
                            (y == 18 && x >= 16 && x <= 20)
                        ? 255
                        : 20;
          if (shade < 0) continue;
          int offset = (y * size + x) * 4;
          for (int c = 0; c < 3; ++c) result[offset + c] = shade;
          result[offset + 3] = 255;
        }
      return result;
    }();
    return image;
  }
  static GLFWcursor* get() {
    static GLFWcursor* cursor = nullptr;
    if (!cursor) {
      GLFWimage image{size, size, const_cast<unsigned char*>(pixels().data())};
      cursor = glfwCreateCursor(&image, 1, 1);
    }
    return cursor;
  }
};
}  // namespace beekapoo
