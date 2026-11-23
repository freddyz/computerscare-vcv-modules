#pragma once
#include "BeekapooKnob.hpp"

namespace beekapoo {
// Related controls share a four-sided family. Patch identity chooses its
// proportions, bevel and asymmetry independently of the knob silhouette.
struct ButtonStyle {
  Vec size;
  std::array<Vec, 4> corners;
  float raised, recessed, border;
  unsigned seed;
};
inline ButtonStyle buttonStyle(const std::string& label,
                               const std::string& source,
                               const PatchIdentity& patch) {
  uint64_t hash = mixIdentity(controlHash(paramFamily(label)),
                              mixIdentity(controlHash(source), patch.modules));
  hash = mixIdentity(hash, patch.types ^ patch.wiring);
  auto part = [&](int shift) { return float((hash >> shift) & 255) / 255.f; };
  unsigned family = hash % 10;
  Vec size = family < 5   ? Vec(24, 23)
             : family < 8 ? Vec(25, 19 + part(8) * 3)
                          : Vec(19.5f + part(8) * 3, 24);
  float warp = (hash >> 10) % 3 == 0 ? 2.7f : .75f;
  ButtonStyle style{
      size,
      {},
      std::min(1.4f + part(16) * 2.4f, std::min(size.x, size.y) * .12f),
      .6f + part(24) * .4f,
      .42f + part(32) * .10f,
      unsigned(hash)};
  for (int i = 0; i < 4; ++i) {
    hash = mixIdentity(hash, i + 1);
    float x = float(hash & 255) / 255.f * warp;
    float y = float((hash >> 8) & 255) / 255.f * warp;
    if (family >= 8) x *= .5f;
    if (family >= 5 && family < 8) y *= .5f;
    style.corners[i] = Vec(i == 0 || i == 3 ? x : -x, i < 2 ? y : -y);
  }
  return style;
}
}  // namespace beekapoo
