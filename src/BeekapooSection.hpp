#pragma once
#include <algorithm>
#include <array>
#include <vector>

#include "BeekapooIdentity.hpp"
#include "Computerscare.hpp"

namespace beekapoo {
struct SectionSkin {
  unsigned profile = 0;
  uint64_t seed = 0;
  float rounding = 0;
};
inline SectionSkin sectionSkin(const PatchIdentity& patch, unsigned role) {
  SectionSkin skin;
  skin.seed = mixIdentity(mixIdentity(patch.manufacturers, patch.wiring), role);
  unsigned choice = unsigned(
      mixIdentity(patch.manufacturerTypes, mixIdentity(patch.types, role)) %
      100);
  // Mostly square plaques, occasional clipped corners, rarely rounded.
  skin.profile = choice < 65   ? 0
                 : choice < 73 ? 1
                 : choice < 85 ? 2
                 : choice < 95 ? 3
                               : 4;
  skin.rounding = skin.profile == 1 ? 2.5f + float(skin.seed % 30) / 10.f : 0;
  return skin;
}
inline std::vector<Vec> bevelSection(const std::array<Vec, 4>& corners,
                                     float cut) {
  std::vector<Vec> result;
  for (int i = 0; i < 4; ++i) {
    Vec p = corners[i], before = corners[(i + 3) % 4] - p,
        after = corners[(i + 1) % 4] - p;
    result.push_back(
        p + before * std::min(.15f, cut / std::max(1.f, before.norm())));
    result.push_back(p +
                     after * std::min(.15f, cut / std::max(1.f, after.norm())));
  }
  return result;
}
inline std::vector<std::vector<Vec>> sectionPolygons(
    const std::array<Vec, 4>& corners, const SectionSkin& skin) {
  if (skin.profile == 4) return {bevelSection(corners, 2.5f)};
  if (skin.profile == 2 || skin.profile == 3) {
    auto points = bevelSection(corners, skin.profile == 2 ? 4.f : 2.f);
    if (skin.profile == 3) {
      // Unequal clipped corners, with the central content area left intact.
      for (size_t i = 0; i < points.size(); ++i) {
        Vec corner = corners[i / 2];
        float amount = .45f + float((skin.seed >> (i * 7)) & 63u) / 32.f;
        points[i] = corner + (points[i] - corner) * amount;
      }
    }
    return {points};
  }
  return {std::vector<Vec>(corners.begin(), corners.end())};
}
// One contour owns the front, extrusion, silhouette, and visible creases.
// Rounded corners are flattened once, never independently on each face.
struct SectionGeometry {
  struct Side {
    std::array<Vec, 4> points;
    float shade;
  };
  std::vector<Vec> front, silhouette;
  std::vector<Side> sides;
  std::vector<std::array<Vec, 2>> creases;
  static float cross(Vec a, Vec b) { return a.x * b.y - a.y * b.x; }
  static std::vector<Vec> hull(std::vector<Vec> points) {
    std::sort(points.begin(), points.end(),
              [](Vec a, Vec b) { return a.x == b.x ? a.y < b.y : a.x < b.x; });
    points.erase(std::unique(points.begin(), points.end(),
                             [](Vec a, Vec b) { return a.equals(b); }),
                 points.end());
    if (points.size() < 3) return points;
    std::vector<Vec> result;
    for (Vec p : points) {
      while (result.size() > 1 &&
             cross(result.back() - result[result.size() - 2],
                   p - result.back()) <= 0)
        result.pop_back();
      result.push_back(p);
    }
    size_t lower = result.size();
    for (size_t i = points.size() - 1; i-- > 0;) {
      Vec p = points[i];
      while (result.size() > lower &&
             cross(result.back() - result[result.size() - 2],
                   p - result.back()) <= 0)
        result.pop_back();
      result.push_back(p);
    }
    result.pop_back();
    return result;
  }
  SectionGeometry() = default;
  SectionGeometry(const std::vector<Vec>& points, Vec extrusion,
                  float rounding) {
    if (points.size() < 3) return;
    if (rounding <= 0)
      front = points;
    else
      for (size_t i = 0; i < points.size(); ++i) {
        Vec p = points[i];
        auto tangent = [&](size_t neighbour) {
          Vec direction = points[neighbour] - p;
          return p +
                 direction *
                     std::min(.2f, rounding / std::max(1.f, direction.norm()));
        };
        Vec entry = tangent((i + points.size() - 1) % points.size());
        Vec exit = tangent((i + 1) % points.size());
        for (int sample = 0; sample <= 12; ++sample) {
          float t = sample / 12.f, u = 1 - t;
          front.push_back(entry * (u * u) + p * (2 * u * t) + exit * (t * t));
        }
      }
    std::vector<Vec> all = front;
    for (Vec p : front) all.push_back(p + extrusion);
    silhouette = hull(all);
    for (size_t i = 0; i < front.size(); ++i) {
      Vec a = front[i], b = front[(i + 1) % front.size()], edge = b - a;
      if (cross(edge, extrusion) >= 0) continue;
      // Continuous shading across rounded corners, without stroked
      // tessellation.
      float vertical = std::abs(edge.y) /
                       std::max(.001f, std::abs(edge.x) + std::abs(edge.y));
      sides.push_back(
          {{{a, b, b + extrusion, a + extrusion}}, .6f + .18f * vertical});
      Vec previous = a - front[(i + front.size() - 1) % front.size()];
      if (rounding <= 0 && cross(previous, extrusion) < 0)
        creases.push_back({{a, a + extrusion}});
    }
  }
  template <class Points>
  static void path(NVGcontext* vg, const Points& points) {
    nvgBeginPath(vg);
    bool first = true;
    for (Vec p : points) {
      if (first)
        nvgMoveTo(vg, p.x, p.y);
      else
        nvgLineTo(vg, p.x, p.y);
      first = false;
    }
    nvgClosePath(vg);
  }
  void draw(NVGcontext* vg, NVGcolor color) const {
    if (front.empty()) return;
    auto shade = [&](float factor) {
      return nvgRGBAf(color.r * factor, color.g * factor, color.b * factor,
                      color.a);
    };
    nvgSave(vg);
    nvgLineJoin(vg, NVG_BEVEL);
    nvgStrokeWidth(vg, .65f);
    nvgStrokeColor(vg, nvgRGB(0, 0, 0));
    // Fill the complete solid first, then shade the walls. Only the silhouette,
    // front outline, and genuinely visible sharp creases receive a stroke.
    path(vg, silhouette);
    nvgFillColor(vg, shade(.65f));
    nvgFill(vg);
    nvgShapeAntiAlias(vg, 0);
    for (const auto& side : sides) {
      path(vg, side.points);
      nvgFillColor(vg, shade(side.shade));
      nvgFill(vg);
    }
    nvgShapeAntiAlias(vg, 1);
    path(vg, silhouette);
    nvgStroke(vg);
    for (const auto& crease : creases) {
      nvgBeginPath(vg);
      nvgMoveTo(vg, crease[0].x, crease[0].y);
      nvgLineTo(vg, crease[1].x, crease[1].y);
      nvgStroke(vg);
    }
    path(vg, front);
    nvgFillColor(vg, color);
    nvgFill(vg);
    nvgStroke(vg);
    nvgRestore(vg);
  }
};
}  // namespace beekapoo
