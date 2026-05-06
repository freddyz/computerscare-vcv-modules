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

  std::puts("compoly routing tests passed");
  return 0;
}
