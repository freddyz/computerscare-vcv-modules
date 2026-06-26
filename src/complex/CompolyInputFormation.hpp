#pragma once

#include <algorithm>
#include <array>

#include "CompolyPortMapping.hpp"

namespace cpx {
namespace compoly {
namespace input_formation {

enum class PartialPairPolicy {
  Strict = 0,
  ZeroFill = 1,
};

enum class InterleavedBankPolicy {
  Compact = 0,
  PreserveSecondBankPosition = 1,
  Strict = 2,
};

enum class SeparatedLanePolicy {
  Strict = 0,
  ZeroFill = 1,
};

struct Options {
  PartialPairPolicy partialPairPolicy;
  InterleavedBankPolicy interleavedBankPolicy;
  SeparatedLanePolicy separatedLanePolicy;

  Options()
      : partialPairPolicy(PartialPairPolicy::Strict),
        interleavedBankPolicy(InterleavedBankPolicy::Strict),
        separatedLanePolicy(SeparatedLanePolicy::Strict) {}

  Options(PartialPairPolicy partialPairPolicy,
          InterleavedBankPolicy interleavedBankPolicy,
          SeparatedLanePolicy separatedLanePolicy)
      : partialPairPolicy(partialPairPolicy),
        interleavedBankPolicy(interleavedBankPolicy),
        separatedLanePolicy(separatedLanePolicy) {}
};

struct PairSource {
  int port;
  int firstChannel;
  bool present;

  PairSource() : port(0), firstChannel(0), present(false) {}

  PairSource(int port, int firstChannel, bool present)
      : port(port), firstChannel(firstChannel), present(present) {}
};

inline PartialPairPolicy partialPairPolicyFromInt(int value) {
  return value == static_cast<int>(PartialPairPolicy::ZeroFill)
             ? PartialPairPolicy::ZeroFill
             : PartialPairPolicy::Strict;
}

inline InterleavedBankPolicy interleavedBankPolicyFromInt(int value) {
  if (value == static_cast<int>(InterleavedBankPolicy::Compact))
    return InterleavedBankPolicy::Compact;
  if (value ==
      static_cast<int>(InterleavedBankPolicy::PreserveSecondBankPosition))
    return InterleavedBankPolicy::PreserveSecondBankPosition;
  return InterleavedBankPolicy::Strict;
}

inline SeparatedLanePolicy separatedLanePolicyFromInt(int value) {
  return value == static_cast<int>(SeparatedLanePolicy::ZeroFill)
             ? SeparatedLanePolicy::ZeroFill
             : SeparatedLanePolicy::Strict;
}

inline int clampBankChannels(int channels) {
  return std::max(0, std::min(maxCablePolyChannels, channels));
}

inline int interleavedBankLanes(int channels, PartialPairPolicy policy) {
  channels = clampBankChannels(channels);
  if (policy == PartialPairPolicy::ZeroFill) return (channels + 1) / 2;
  return channels / 2;
}

inline bool interleavedBankComplete(int channels, PartialPairPolicy policy) {
  return interleavedBankLanes(channels, policy) >= 8;
}

inline int interleavedLanesForInput(PortChannelCounts portChannels,
                                    Options options) {
  const int aLanes =
      interleavedBankLanes(portChannels.a, options.partialPairPolicy);
  const int bLanes =
      interleavedBankLanes(portChannels.b, options.partialPairPolicy);

  if (options.interleavedBankPolicy == InterleavedBankPolicy::Compact)
    return clampCompolyLanes(aLanes + bLanes);

  if (options.interleavedBankPolicy == InterleavedBankPolicy::Strict &&
      !interleavedBankComplete(portChannels.a, options.partialPairPolicy)) {
    return aLanes;
  }

  return bLanes > 0 ? clampCompolyLanes(8 + bLanes) : aLanes;
}

inline int separatedLanesForInput(PortChannelCounts portChannels,
                                  Options options) {
  if (options.separatedLanePolicy == SeparatedLanePolicy::ZeroFill)
    return std::max(portChannels.a, portChannels.b);
  return std::min(portChannels.a, portChannels.b);
}

inline int lanesForInput(cpx::complex_math::CoordinateMode mode,
                         PortChannelCounts portChannels, Options options) {
  if (cpx::complex_math::isInterleaved(mode))
    return interleavedLanesForInput(portChannels, options);
  return separatedLanesForInput(portChannels, options);
}

inline PairSource interleavedPairSourceForLane(int lane,
                                               PortChannelCounts portChannels,
                                               Options options) {
  const int aLanes =
      interleavedBankLanes(portChannels.a, options.partialPairPolicy);
  const int bLanes =
      interleavedBankLanes(portChannels.b, options.partialPairPolicy);

  if (lane < 0 || lane >= maxCompolyLanes) return PairSource();

  if (options.interleavedBankPolicy == InterleavedBankPolicy::Compact) {
    if (lane < aLanes) return PairSource(0, lane * 2, true);
    const int bLane = lane - aLanes;
    return bLane < bLanes ? PairSource(1, bLane * 2, true) : PairSource();
  }

  if (lane < 8) {
    return lane < aLanes ? PairSource(0, lane * 2, true) : PairSource();
  }

  if (options.interleavedBankPolicy == InterleavedBankPolicy::Strict &&
      !interleavedBankComplete(portChannels.a, options.partialPairPolicy)) {
    return PairSource();
  }

  const int bLane = lane - 8;
  return bLane < bLanes ? PairSource(1, bLane * 2, true) : PairSource();
}

inline std::array<float, 2> interleavedPairForLane(
    const PortChannels& ports, PortChannelCounts portChannels, int lane,
    Options options) {
  PairSource source = interleavedPairSourceForLane(lane, portChannels, options);
  if (!source.present) return {{0.f, 0.f}};

  const cpx::complex_math::Channels& channels =
      source.port == 0 ? ports.a : ports.b;
  const int channelCount = source.port == 0 ? portChannels.a : portChannels.b;
  return {{channelVoltage(channels, source.firstChannel, channelCount),
           channelVoltage(channels, source.firstChannel + 1, channelCount)}};
}

inline std::array<float, 2> separatedPairForLane(const PortChannels& ports,
                                                 PortChannelCounts portChannels,
                                                 int lane, Options options) {
  const int laneCount = separatedLanesForInput(portChannels, options);
  if (lane < 0 || lane >= laneCount) return {{0.f, 0.f}};
  return {{channelVoltage(ports.a, lane, portChannels.a),
           channelVoltage(ports.b, lane, portChannels.b)}};
}

inline std::array<float, 2> pairForLane(const PortChannels& ports,
                                        PortChannelCounts portChannels,
                                        cpx::complex_math::CoordinateMode mode,
                                        int lane, Options options) {
  if (cpx::complex_math::isInterleaved(mode))
    return interleavedPairForLane(ports, portChannels, lane, options);
  return separatedPairForLane(ports, portChannels, lane, options);
}

inline cpx::complex_math::RectChannels readInputToRect(
    const PortChannels& ports, PortChannelCounts portChannels,
    cpx::complex_math::CoordinateMode mode, int compolyChannels,
    Options options) {
  cpx::complex_math::RectChannels rect = {};
  compolyChannels = cpx::complex_math::clampChannelCount(compolyChannels);
  for (int c = 0; c < compolyChannels; ++c) {
    std::array<float, 2> pair =
        pairForLane(ports, portChannels, mode, c, options);
    cpx::complex_math::Quad z =
        cpx::complex_math::quadFromPair(pair[0], pair[1], mode);
    rect.x[c] = z.x;
    rect.y[c] = z.y;
  }
  return rect;
}

}  // namespace input_formation
}  // namespace compoly
}  // namespace cpx
