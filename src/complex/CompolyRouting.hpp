#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace cpx {
namespace compoly {

constexpr int maxCablePolyChannels = 16;
constexpr int maxCompolyLanes = 16;

enum class WrapMode {
  Normal = 0,
  Cycle = 1,
  Minimal = 2,
  Stall = 3,
};

struct CablePolyChannels {
  int count;

  CablePolyChannels() : count(0) {
  }

  explicit CablePolyChannels(int count) : count(clamp(count)) {
  }

  static int clamp(int count) {
    return std::max(0, std::min(maxCablePolyChannels, count));
  }
};

struct CompolyLane {
  int index;

  CompolyLane() : index(0) {
  }

  explicit CompolyLane(int index) : index(index) {
  }
};

struct SeparatedCablePolyChannels {
  CablePolyChannels first;
  CablePolyChannels second;

  SeparatedCablePolyChannels() {
  }

  SeparatedCablePolyChannels(CablePolyChannels first,
                             CablePolyChannels second)
      : first(first), second(second) {
  }
};

struct SeparatedCableChannels {
  int first;
  int second;

  SeparatedCableChannels() : first(0), second(0) {
  }

  SeparatedCableChannels(int first, int second)
      : first(first), second(second) {
  }
};

inline const std::vector<std::string>& wrapModeDescriptions() {
  static const std::vector<std::string> descriptions = {
      "Normal (Standard Polyphonic Behavior)",
      "Cycle (Repeat Channels)",
      "Minimal (Pad with 0v)",
      "Stall (Pad with final voltage)",
  };
  return descriptions;
}

inline int clampCompolyLanes(int lanes) {
  return std::max(0, std::min(maxCompolyLanes, lanes));
}

inline int compolyLanesForSeparatedCables(
    SeparatedCablePolyChannels cables) {
  return std::max(cables.first.count, cables.second.count);
}

inline int compolyLanesForInterleavedCables(
    SeparatedCablePolyChannels cables) {
  return (cables.first.count + cables.second.count + 1) / 2;
}

inline int outputCompolyLanes(int requestedLanes, int detectedLanes) {
  if (requestedLanes != 0)
    return clampCompolyLanes(requestedLanes);
  detectedLanes = clampCompolyLanes(detectedLanes);
  return detectedLanes == 0 ? 1 : detectedLanes;
}

inline int cableChannelForCompolyLane(CompolyLane lane, WrapMode wrapMode,
                                      CablePolyChannels cableChannels) {
  if (cableChannels.count <= 0)
    return 0;

  switch (wrapMode) {
    case WrapMode::Normal:
      return cableChannels.count == 1 ? 0 : lane.index;
    case WrapMode::Cycle:
      return lane.index % cableChannels.count;
    case WrapMode::Minimal:
      return lane.index;
    case WrapMode::Stall:
      return lane.index > cableChannels.count - 1 ? cableChannels.count - 1
                                                  : lane.index;
  }
  return lane.index;
}

inline SeparatedCableChannels separatedCableChannelsForCompolyLane(
    CompolyLane lane, WrapMode wrapMode, SeparatedCablePolyChannels cables) {
  return SeparatedCableChannels(
      cableChannelForCompolyLane(lane, wrapMode, cables.first),
      cableChannelForCompolyLane(lane, wrapMode, cables.second));
}

}  // namespace compoly
}  // namespace cpx
