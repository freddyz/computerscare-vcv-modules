#pragma once
#include "../Wind/Types.hpp"
namespace windchimes {
constexpr int maxSets = 16;
constexpr int maxTubes = 12;
constexpr int modesPerTube = 12;
using computerscare::wind::clamp;
using computerscare::wind::pi;
using computerscare::wind::Random;
using computerscare::wind::Stereo;
struct SetConfig {
  bool enabled = false;
  int tubes = 6, material = 0, scale = 0, root = 0, divisions = 12, spread = 1;
  float octave = 0.f, fine = 0.f, decay = 0.55f, brightness = 0.55f;
  float strikerWeight = 0.5f, sailSize = 0.5f;
  float hardness = 0.5f, level = 0.7f, swing = 0.5f;
  float x = 0.5f, y = 0.5f;
  float shape = 0.35f, body = 0.65f, inharmonicity = 0.5f;
};
}  // namespace windchimes
