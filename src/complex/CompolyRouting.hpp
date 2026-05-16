#pragma once

#include <algorithm>

#include "PolyphonicMapping.hpp"

namespace cpx {
namespace compoly {

constexpr int maxCablePolyChannels = 16;
constexpr int maxCompolyLanes = 16;

using PolyphonicMappingMode = cpx::polyphonic::MappingMode;
using WrapMode = cpx::polyphonic::MappingMode;

constexpr int firstPolyphonicMappingModeValue =
    cpx::polyphonic::firstMappingModeValue;
constexpr int lastPolyphonicMappingModeValue =
    cpx::polyphonic::lastMappingModeValue;
constexpr int defaultPolyphonicMappingModeValue =
    cpx::polyphonic::defaultMappingModeValue;

struct CablePolyChannels {
  int count;

  CablePolyChannels() : count(0) {}

  explicit CablePolyChannels(int count) : count(clamp(count)) {}

  static int clamp(int count) {
    return std::max(0, std::min(maxCablePolyChannels, count));
  }
};

struct CompolyLane {
  int index;

  CompolyLane() : index(0) {}

  explicit CompolyLane(int index) : index(index) {}
};

struct SeparatedCablePolyChannels {
  CablePolyChannels first;
  CablePolyChannels second;

  SeparatedCablePolyChannels() {}

  SeparatedCablePolyChannels(CablePolyChannels first, CablePolyChannels second)
      : first(first), second(second) {}
};

struct SeparatedCableChannels {
  int first;
  int second;

  SeparatedCableChannels() : first(0), second(0) {}

  SeparatedCableChannels(int first, int second)
      : first(first), second(second) {}
};

inline const std::vector<std::string>& wrapModeDescriptions() {
  static const std::vector<std::string> descriptions = {
      "Standard Compoly Behavior",
      "Cycle",
      "Zero Pad",
      "Stall",
  };
  return descriptions;
}

inline int clampCompolyLanes(int lanes) {
  return std::max(0, std::min(maxCompolyLanes, lanes));
}

inline int compolyLanesForSeparatedCables(SeparatedCablePolyChannels cables) {
  return std::max(cables.first.count, cables.second.count);
}

inline int compolyLanesForInterleavedCables(SeparatedCablePolyChannels cables) {
  return (cables.first.count + cables.second.count + 1) / 2;
}

inline int outputCompolyLanes(int requestedLanes, int detectedLanes) {
  if (requestedLanes != 0) return clampCompolyLanes(requestedLanes);
  detectedLanes = clampCompolyLanes(detectedLanes);
  return detectedLanes == 0 ? 1 : detectedLanes;
}

inline int cableChannelForCompolyLane(CompolyLane lane, WrapMode wrapMode,
                                      CablePolyChannels cableChannels) {
  return cpx::polyphonic::inputChannelForOutputChannel(
      cpx::polyphonic::OutputChannel(lane.index), wrapMode,
      cpx::polyphonic::ChannelCount(cableChannels.count));
}

inline SeparatedCableChannels separatedCableChannelsForCompolyLane(
    CompolyLane lane, WrapMode wrapMode, SeparatedCablePolyChannels cables) {
  return SeparatedCableChannels(
      cableChannelForCompolyLane(lane, wrapMode, cables.first),
      cableChannelForCompolyLane(lane, wrapMode, cables.second));
}

}  // namespace compoly
}  // namespace cpx
