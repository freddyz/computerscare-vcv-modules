#include "complex/PolyphonicMapping.hpp"

#include <cstdio>
#include <cstdlib>

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
}

int inputChannel(int outputChannel, cpx::polyphonic::MappingMode mode,
                 int inputChannels) {
  return cpx::polyphonic::inputChannelForOutputChannel(
      cpx::polyphonic::OutputChannel(outputChannel), mode,
      cpx::polyphonic::ChannelCount(inputChannels));
}

}  // namespace

int main() {
  namespace pm = cpx::polyphonic;

  require(pm::ChannelCount(-4).count == 0,
          "input channel count clamps negative to zero");
  require(pm::ChannelCount(99).count == 16,
          "input channel count clamps above Rack max");

  require(inputChannel(0, pm::MappingMode::Normal, 1) == 0,
          "normal maps mono output channel zero to input channel zero");
  require(inputChannel(7, pm::MappingMode::Normal, 1) == 0,
          "normal copies mono input to later output channels");
  require(inputChannel(3, pm::MappingMode::Normal, 8) == 3,
          "normal maps matching polyphonic channels");
  require(inputChannel(7, pm::MappingMode::Normal, 8) == 7,
          "normal maps later matching polyphonic channels");

  require(inputChannel(0, pm::MappingMode::Cycle, 3) == 0,
          "cycle starts on input channel zero");
  require(inputChannel(3, pm::MappingMode::Cycle, 3) == 0,
          "cycle wraps after the final input channel");
  require(inputChannel(7, pm::MappingMode::Cycle, 3) == 1,
          "cycle repeats input channels");
  require(inputChannel(7, pm::MappingMode::Cycle, 5) == 2,
          "cycle uses the active input channel count");

  require(inputChannel(2, pm::MappingMode::Stall, 3) == 2,
          "stall maps channels inside the input range normally");
  require(inputChannel(3, pm::MappingMode::Stall, 3) == 2,
          "stall repeats the final input channel");
  require(inputChannel(7, pm::MappingMode::Stall, 5) == 4,
          "stall uses the final active input channel");

  require(inputChannel(2, pm::MappingMode::PadWithZero, 3) == 2,
          "pad maps channels inside the input range normally");
  require(inputChannel(7, pm::MappingMode::PadWithZero, 3) == 7,
          "pad leaves later channels unmapped for downstream zero padding");

  require(inputChannel(7, pm::MappingMode::Normal, 0) == 0,
          "zero input channels maps to channel zero");
  require(inputChannel(7, pm::MappingMode::Cycle, 0) == 0,
          "cycle zero input channels maps to channel zero");
  require(inputChannel(7, pm::MappingMode::Stall, 0) == 0,
          "stall zero input channels maps to channel zero");

  std::puts("polyphonic mapping tests passed");
  return 0;
}
