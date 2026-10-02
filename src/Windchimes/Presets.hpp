#pragma once
#include "Types.hpp"
namespace windchimes {
struct SoundPreset {
  const char* name;
  float decay, brightness, hardness, shape, body, inharmonicity;
};
struct PresetList {
  const SoundPreset* items;
  int count;
};
inline PresetList soundPresets(int material) {
  static const SoundPreset wood[] = {
      {"Hollow bamboo", .66f, .48f, .68f, 1.f, .65f, .5f},
      {"Split bamboo clack", .88f, .55f, .93f, .67f, .27f, .57f},
      {"Deep bamboo", .71f, .24f, .55f, .94f, .62f, .46f},
      {"Dry bamboo rattle", .75f, .68f, .88f, .82f, .25f, .62f},
      {"Resonant wood block", .72f, .4f, .65f, .33f, .65f, .5f},
      {"Hard lumber knock", .7f, .38f, .9f, 0.f, .42f, .5f},
      {"Soft timber", .6f, .23f, .4f, .12f, .5f, .44f},
      {"Singing hardwood", .84f, .52f, .75f, .24f, .82f, .55f}};
  static const SoundPreset metal[] = {
      {"Bright tubular", .8f, .83f, .88f, 1.f, .95f, .5f},
      {"Warm suspended bar", .72f, .4f, .5f, 0.f, .9f, .5f},
      {"Dry metal clink", .35f, .75f, .95f, .65f, .4f, .5f}};
  static const SoundPreset plastic[] = {
      {"Hollow plastic", .56f, .55f, .75f, .98f, .8f, .5f},
      {"Solid plastic knock", .34f, .38f, .85f, .12f, .3f, .5f},
      {"Soft plastic block", .45f, .24f, .3f, .33f, .65f, .5f}};
  return material == 1   ? PresetList{wood, 8}
         : material == 2 ? PresetList{plastic, 3}
                         : PresetList{metal, 3};
}
}  // namespace windchimes
