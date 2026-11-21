#pragma once
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
inline void sectionPath(NVGcontext* vg, const std::vector<Vec>& points,
                        Vec offset, float rounding) {
  nvgBeginPath(vg);
  if (rounding <= 0) {
    for (size_t i = 0; i < points.size(); ++i) {
      Vec p = points[i] + offset;
      if (i)
        nvgLineTo(vg, p.x, p.y);
      else
        nvgMoveTo(vg, p.x, p.y);
    }
  } else {
    auto tangent = [&](size_t i, size_t neighbour) {
      Vec direction = points[neighbour] - points[i];
      return points[i] + offset +
             direction *
                 std::min(.2f, rounding / std::max(1.f, direction.norm()));
    };
    Vec first = tangent(0, points.size() - 1);
    nvgMoveTo(vg, first.x, first.y);
    for (size_t i = 0; i < points.size(); ++i) {
      Vec p = points[i] + offset;
      Vec entry = tangent(i, (i + points.size() - 1) % points.size());
      Vec exit = tangent(i, (i + 1) % points.size());
      nvgLineTo(vg, entry.x, entry.y);
      nvgQuadTo(vg, p.x, p.y, exit.x, exit.y);
    }
  }
  nvgClosePath(vg);
}
inline void drawSection(NVGcontext* vg,
                        const std::vector<std::vector<Vec>>& polygons,
                        NVGcolor color, float depth, float rounding) {
  Vec extrusion(depth, depth * .65f);
  auto shade = [&](float factor) {
    return nvgRGBAf(color.r * factor, color.g * factor, color.b * factor,
                    color.a);
  };
  nvgLineJoin(vg, NVG_BEVEL);
  nvgStrokeWidth(vg, .65f);
  nvgStrokeColor(vg, nvgRGB(0, 0, 0));
  // Draw all sides first, then both faces, so paired plaques form a quiet seam.
  for (const auto& points : polygons) {
    sectionPath(vg, points, extrusion, rounding);
    nvgFillColor(vg, shade(.65f));
    nvgFill(vg);
    nvgStroke(vg);
    for (size_t i = 0; i < points.size(); ++i) {
      Vec a = points[i], b = points[(i + 1) % points.size()];
      nvgBeginPath(vg);
      nvgMoveTo(vg, a.x, a.y);
      for (Vec p : {b, b + extrusion, a + extrusion}) nvgLineTo(vg, p.x, p.y);
      nvgClosePath(vg);
      nvgFillColor(
          vg, shade(std::abs(b.x - a.x) > std::abs(b.y - a.y) ? .6f : .78f));
      nvgFill(vg);
      nvgStroke(vg);
    }
  }
  for (const auto& points : polygons) {
    sectionPath(vg, points, Vec(), rounding);
    nvgFillColor(vg, color);
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGB(0, 0, 0));
    nvgStroke(vg);
  }
}
}  // namespace beekapoo
