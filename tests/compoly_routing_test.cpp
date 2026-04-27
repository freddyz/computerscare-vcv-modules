#include "complex/CompolyRouting.hpp"

#include <cstdio>
#include <cstdlib>

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
}

}  // namespace

int main() {
  namespace cr = cpx::compoly;

  require(cr::CablePolyChannels(-4).count == 0,
          "cable poly channels clamp negative to zero");
  require(cr::CablePolyChannels(99).count == 16,
          "cable poly channels clamp above Rack max");

  cr::SeparatedCablePolyChannels separated(cr::CablePolyChannels(16),
                                           cr::CablePolyChannels(7));
  require(cr::compolyLanesForSeparatedCables(separated) == 16,
          "separated compoly lanes use max cable poly channels");

  cr::SeparatedCablePolyChannels interleaved(cr::CablePolyChannels(15),
                                             cr::CablePolyChannels(2));
  require(cr::compolyLanesForInterleavedCables(interleaved) == 9,
          "interleaved compoly lanes pair two cable poly channels per lane");

  require(cr::outputCompolyLanes(0, 0) == 1,
          "auto compoly lanes falls back to one lane without input");
  require(cr::outputCompolyLanes(0, 12) == 12,
          "auto compoly lanes uses detected lanes");
  require(cr::outputCompolyLanes(5, 12) == 5,
          "manual compoly lanes overrides detected lanes");
  require(cr::outputCompolyLanes(99, 12) == 16,
          "manual compoly lanes clamps to Rack max");

  require(cr::cableChannelForCompolyLane(
              cr::CompolyLane(4), cr::WrapMode::Normal,
              cr::CablePolyChannels(1)) == 0,
          "normal wrap copies mono cable channel to every compoly lane");
  require(cr::cableChannelForCompolyLane(
              cr::CompolyLane(4), cr::WrapMode::Normal,
              cr::CablePolyChannels(8)) == 4,
          "normal wrap uses matching cable channel for poly cables");
  require(cr::cableChannelForCompolyLane(
              cr::CompolyLane(4), cr::WrapMode::Cycle,
              cr::CablePolyChannels(3)) == 1,
          "cycle wrap repeats cable channels");
  require(cr::cableChannelForCompolyLane(
              cr::CompolyLane(4), cr::WrapMode::Minimal,
              cr::CablePolyChannels(3)) == 4,
          "minimal wrap leaves compoly lane index unchanged");
  require(cr::cableChannelForCompolyLane(
              cr::CompolyLane(4), cr::WrapMode::Stall,
              cr::CablePolyChannels(3)) == 2,
          "stall wrap reuses final cable channel");
  require(cr::cableChannelForCompolyLane(
              cr::CompolyLane(4), cr::WrapMode::Cycle,
              cr::CablePolyChannels(0)) == 0,
          "zero-channel cable maps to channel zero");

  cr::SeparatedCableChannels channels =
      cr::separatedCableChannelsForCompolyLane(
          cr::CompolyLane(5), cr::WrapMode::Cycle,
          cr::SeparatedCablePolyChannels(cr::CablePolyChannels(4),
                                         cr::CablePolyChannels(3)));
  require(channels.first == 1 && channels.second == 2,
          "separated cable channel mapping is independent per cable");

  require(cr::wrapModeDescriptions().size() == 4,
          "wrap mode descriptions are reusable by Rack params");

  std::puts("compoly routing tests passed");
  return 0;
}
