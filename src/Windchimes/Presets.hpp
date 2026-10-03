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
  // Each preset targets an attack/tail/texture role; these are not alloy
  // models.
  static const SoundPreset metal[] = {
      {"Bright tube", .8f, .83f, .88f, 1.f, .95f, .5f},
      {"Soft tube", .78f, .58f, .25f, 1.f, .94f, .48f},
      {"Warm bar", .72f, .4f, .5f, 0.f, .9f, .5f},
      {"Thin strip", .56f, .86f, .82f, .26f, .62f, .58f},
      {"Muted clink", .35f, .75f, .95f, .65f, .4f, .5f},
      {"Shell ping", .58f, .65f, .86f, .78f, .66f, .63f},
      {"Singing bar", .94f, .34f, .42f, .08f, 1.f, .46f},
      {"Shimmering tube", .91f, .74f, .78f, 1.f, 1.f, .78f}};
  // Perceptual object roles; names do not claim calibrated polymer chemistry.
  static const SoundPreset plastic[] = {
      {"Hollow plastic", .58f, .6f, .74f, 1.f, .82f, .5f},
      {"Solid plastic knock", .42f, .45f, .86f, .08f, .48f, .5f},
      {"Soft plastic block", .38f, .22f, .2f, .3f, .38f, .46f},
      {"Rigid pipe", .76f, .83f, .94f, .94f, .96f, .56f},
      {"Thin cup", .56f, .9f, .86f, .66f, .65f, .62f},
      {"Bottle pop", .51f, .43f, .68f, .84f, .7f, .42f},
      {"Dry toy clack", .31f, .77f, .96f, .48f, .18f, .6f},
      {"Resonant bar", .84f, .66f, .82f, 0.f, .94f, .5f}};
  return material == 1   ? PresetList{wood, 8}
         : material == 2 ? PresetList{plastic, 8}
                         : PresetList{metal, 8};
}
}  // namespace windchimes
