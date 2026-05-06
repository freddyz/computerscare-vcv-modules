#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace cpx {
namespace polyphonic {

constexpr int maxPolyChannels = 16;

enum class MappingMode {
  Normal = 0,
  Cycle = 1,
  PadWithZero = 2,
  Stall = 3,

  Minimal = PadWithZero,
};

constexpr int firstMappingModeValue = static_cast<int>(MappingMode::Normal);
constexpr int lastMappingModeValue = static_cast<int>(MappingMode::Stall);
constexpr int defaultMappingModeValue = static_cast<int>(MappingMode::Normal);

struct ChannelCount {
  int count;

  ChannelCount() : count(0) {}

  explicit ChannelCount(int count) : count(clamp(count)) {}

  static int clamp(int count) {
    return std::max(0, std::min(maxPolyChannels, count));
  }
};

struct OutputChannel {
  int index;

  OutputChannel() : index(0) {}

  explicit OutputChannel(int index) : index(index) {}
};

inline int inputChannelForOutputChannel(OutputChannel outputChannel,
                                        MappingMode mappingMode,
                                        ChannelCount inputChannels) {
  if (inputChannels.count <= 0) return 0;

  switch (mappingMode) {
    case MappingMode::Normal:
      return inputChannels.count == 1 ? 0 : outputChannel.index;
    case MappingMode::Cycle:
      return outputChannel.index % inputChannels.count;
    case MappingMode::PadWithZero:
      return outputChannel.index;
    case MappingMode::Stall:
      return outputChannel.index > inputChannels.count - 1
                 ? inputChannels.count - 1
                 : outputChannel.index;
  }
  return outputChannel.index;
}

inline const std::vector<std::string>& mappingModeDescriptions() {
  static const std::vector<std::string> descriptions = {
      "Normal (Standard Polyphonic Behavior)",
      "Cycle (Repeat Channels)",
      "Minimal (Pad with 0v)",
      "Stall (Pad with final voltage)",
  };
  return descriptions;
}

}  // namespace polyphonic
}  // namespace cpx
