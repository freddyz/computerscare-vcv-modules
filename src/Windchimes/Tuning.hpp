#pragma once
#include "Types.hpp"
namespace windchimes {
// Existing scale IDs stay first so saved patches retain their tuning.
constexpr int scaleCount = 18;
constexpr const char* scaleNames[scaleCount] = {"Major pentatonic",
                                                "Minor pentatonic",
                                                "Major",
                                                "Minor",
                                                "Whole tone",
                                                "Chromatic",
                                                "Dorian",
                                                "Phrygian",
                                                "Lydian",
                                                "Mixolydian",
                                                "Locrian",
                                                "Harmonic minor",
                                                "Melodic minor (ascending)",
                                                "Minor blues",
                                                "Major blues",
                                                "Diminished (half–whole)",
                                                "Hirajoshi",
                                                "Insen"};
constexpr const char* scaleCaptions[scaleCount] = {
    "Major pent.", "Minor pent.", "Major",      "Minor",       "Whole tone",
    "Chromatic",   "Dorian",      "Phrygian",   "Lydian",      "Mixolydian",
    "Locrian",     "Harm. minor", "Mel. minor", "Minor blues", "Major blues",
    "Dim. H/W",    "Hirajoshi",   "Insen"};
// Reference semitones are mapped to the selected EDO; unused entries are zero.
constexpr int scaleDegrees[scaleCount][12] = {
    {0, 2, 4, 7, 9},        {0, 2, 3, 7, 10},
    {0, 2, 4, 5, 7, 9, 11}, {0, 2, 3, 5, 7, 8, 10},
    {0, 2, 4, 6, 8, 10},    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11},
    {0, 2, 3, 5, 7, 9, 10}, {0, 1, 3, 5, 7, 8, 10},
    {0, 2, 4, 6, 7, 9, 11}, {0, 2, 4, 5, 7, 9, 10},
    {0, 1, 3, 5, 6, 8, 10}, {0, 2, 3, 5, 7, 8, 11},
    {0, 2, 3, 5, 7, 9, 11}, {0, 3, 5, 6, 7, 10},
    {0, 2, 3, 4, 7, 9},     {0, 1, 3, 4, 6, 7, 9, 10},
    {0, 2, 3, 7, 8},        {0, 1, 5, 7, 10}};
constexpr int scaleSizes[scaleCount] = {5, 5, 7, 7, 6, 12, 7, 7, 7,
                                        7, 7, 7, 7, 6, 6,  8, 5, 5};
inline float tubeFrequency(const SetConfig& c, int tube, float transpose) {
  int scale = std::max(0, std::min(c.scale, scaleCount - 1));
  int divisions = std::max(5, std::min(c.divisions, 24));
  int index = tube * std::max(1, std::min(c.spread, 4));
  int degree = scaleDegrees[scale][index % scaleSizes[scale]];
  float step = std::round(degree * divisions / 12.f);
  float pitch = c.octave + c.root / 12.f + c.fine / 1200.f + transpose;
  pitch += index / scaleSizes[scale] + step / divisions;
  return clamp(261.625565f * std::exp2(pitch), 20.f, 12000.f);
}
}  // namespace windchimes
