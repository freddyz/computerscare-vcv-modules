#include "complex/CompolyInputFormation.hpp"

#include <cstdio>
#include <cstdlib>

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
}

void requireNear(float actual, float expected, const char* message,
                 float epsilon = 1e-5f) {
  float diff = actual - expected;
  if (diff < 0.f) diff = -diff;
  if (diff > epsilon) {
    std::fprintf(stderr, "FAIL: %s: got %.9f expected %.9f\n", message, actual,
                 expected);
    std::exit(1);
  }
}

}  // namespace

int main() {
  namespace cm = cpx::complex_math;
  namespace cp = cpx::compoly;
  namespace cf = cpx::compoly::input_formation;

  cp::PortChannels ports = {};
  for (int c = 0; c < cm::maxChannels; ++c) {
    ports.a[c] = static_cast<float>(100 + c);
    ports.b[c] = static_cast<float>(200 + c);
  }

  cf::Options compactStrict(cf::PartialPairPolicy::Strict,
                            cf::InterleavedBankPolicy::Compact,
                            cf::SeparatedLanePolicy::Strict);
  cf::Options compactFill(cf::PartialPairPolicy::ZeroFill,
                          cf::InterleavedBankPolicy::Compact,
                          cf::SeparatedLanePolicy::Strict);
  cf::Options preserveSecondBankPositionFill(cf::PartialPairPolicy::ZeroFill,
                        cf::InterleavedBankPolicy::PreserveSecondBankPosition,
                        cf::SeparatedLanePolicy::Strict);
  cf::Options strictFill(cf::PartialPairPolicy::ZeroFill,
                         cf::InterleavedBankPolicy::Strict,
                         cf::SeparatedLanePolicy::Strict);
  cf::Options strictStrict(cf::PartialPairPolicy::Strict,
                           cf::InterleavedBankPolicy::Strict,
                           cf::SeparatedLanePolicy::Strict);

  require(cf::interleavedBankLanes(3, cf::PartialPairPolicy::Strict) == 1,
          "strict partial interleaved pairs drop odd channel");
  require(cf::interleavedBankLanes(3, cf::PartialPairPolicy::ZeroFill) == 2,
          "zero-fill partial interleaved pairs keep odd channel");
  require(cf::interleavedBankComplete(15, cf::PartialPairPolicy::ZeroFill),
          "zero-fill makes a 15-channel bank complete");
  require(!cf::interleavedBankComplete(15, cf::PartialPairPolicy::Strict),
          "strict requires 16 channels for a complete bank");

  require(cf::lanesForInput(cm::CoordinateMode::RectangularInterleaved,
                            cp::PortChannelCounts(4, 1),
                            compactStrict) == 2,
          "compact strict ignores incomplete jack B pair");
  require(cf::lanesForInput(cm::CoordinateMode::RectangularInterleaved,
                            cp::PortChannelCounts(4, 1), compactFill) == 3,
          "compact zero-fill places jack B after jack A lanes");
  require(cf::lanesForInput(cm::CoordinateMode::RectangularInterleaved,
                            cp::PortChannelCounts(4, 1), preserveSecondBankPositionFill) == 9,
          "preserve second bank position zero-fill preserves jack B offset");
  require(cf::lanesForInput(cm::CoordinateMode::RectangularInterleaved,
                            cp::PortChannelCounts(4, 1), strictFill) == 2,
          "strict bank ignores jack B until jack A bank is complete");
  require(cf::lanesForInput(cm::CoordinateMode::RectangularInterleaved,
                            cp::PortChannelCounts(15, 1), strictFill) == 9,
          "strict bank allows jack B after zero-filled complete jack A bank");
  require(cf::lanesForInput(cm::CoordinateMode::RectangularInterleaved,
                            cp::PortChannelCounts(15, 1), strictStrict) == 7,
          "strict bank keeps jack B out when strict jack A has only 7 lanes");
  require(cf::lanesForInput(cm::CoordinateMode::RectangularInterleaved,
                            cp::PortChannelCounts(16, 1), strictStrict) == 8,
          "strict partial ignores incomplete jack B pair after full jack A");

  std::array<float, 2> compactB = cf::pairForLane(
      ports, cp::PortChannelCounts(4, 1),
      cm::CoordinateMode::RectangularInterleaved, 2, compactFill);
  requireNear(compactB[0], 200.f, "compact jack B lane first coordinate");
  requireNear(compactB[1], 0.f, "compact jack B lane zero-filled coordinate");

  std::array<float, 2> preserveSecondBankPositionGap = cf::pairForLane(
      ports, cp::PortChannelCounts(4, 1),
      cm::CoordinateMode::RectangularInterleaved, 2, preserveSecondBankPositionFill);
  requireNear(preserveSecondBankPositionGap[0], 0.f, "preserve second bank position fills intervening x with zero");
  requireNear(preserveSecondBankPositionGap[1], 0.f, "preserve second bank position fills intervening y with zero");

  std::array<float, 2> preserveSecondBankPositionB = cf::pairForLane(
      ports, cp::PortChannelCounts(4, 1),
      cm::CoordinateMode::RectangularInterleaved, 8, preserveSecondBankPositionFill);
  requireNear(preserveSecondBankPositionB[0], 200.f,
              "preserve second bank position reads jack B at second bank");
  requireNear(preserveSecondBankPositionB[1], 0.f, "preserve second bank position zero-fills jack B partial pair");

  std::array<float, 2> strictIgnoredB = cf::pairForLane(
      ports, cp::PortChannelCounts(4, 1),
      cm::CoordinateMode::RectangularInterleaved, 8, strictFill);
  requireNear(strictIgnoredB[0], 0.f, "strict bank hides early jack B x");
  requireNear(strictIgnoredB[1], 0.f, "strict bank hides early jack B y");

  std::array<float, 2> strictAllowedB = cf::pairForLane(
      ports, cp::PortChannelCounts(15, 1),
      cm::CoordinateMode::RectangularInterleaved, 8, strictFill);
  requireNear(strictAllowedB[0], 200.f,
              "strict bank admits jack B after complete jack A");
  requireNear(strictAllowedB[1], 0.f,
              "strict bank admits zero-filled jack B pair");

  cf::Options separatedStrict(cf::PartialPairPolicy::Strict,
                              cf::InterleavedBankPolicy::Strict,
                              cf::SeparatedLanePolicy::Strict);
  cf::Options separatedFill(cf::PartialPairPolicy::Strict,
                            cf::InterleavedBankPolicy::Strict,
                            cf::SeparatedLanePolicy::ZeroFill);
  require(cf::lanesForInput(cm::CoordinateMode::RectangularSeparated,
                            cp::PortChannelCounts(3, 2),
                            separatedStrict) == 2,
          "separated strict uses only complete lanes");
  require(cf::lanesForInput(cm::CoordinateMode::RectangularSeparated,
                            cp::PortChannelCounts(3, 2), separatedFill) == 3,
          "separated zero-fill keeps lanes with either coordinate");

  std::array<float, 2> separatedStrictMissing = cf::pairForLane(
      ports, cp::PortChannelCounts(3, 2),
      cm::CoordinateMode::RectangularSeparated, 2, separatedStrict);
  requireNear(separatedStrictMissing[0], 0.f,
              "separated strict omits incomplete x");
  requireNear(separatedStrictMissing[1], 0.f,
              "separated strict omits incomplete y");

  std::array<float, 2> separatedFillMissing = cf::pairForLane(
      ports, cp::PortChannelCounts(3, 2),
      cm::CoordinateMode::RectangularSeparated, 2, separatedFill);
  requireNear(separatedFillMissing[0], 102.f,
              "separated zero-fill keeps present x");
  requireNear(separatedFillMissing[1], 0.f,
              "separated zero-fill pads missing y");

  for (int a = 0; a <= 16; ++a) {
    for (int b = 0; b <= 16; ++b) {
      for (int partial = 0; partial <= 1; ++partial) {
        for (int bank = 0; bank <= 2; ++bank) {
          cf::Options options(
              cf::partialPairPolicyFromInt(partial),
              cf::interleavedBankPolicyFromInt(bank),
              cf::SeparatedLanePolicy::Strict);
          int lanes = cf::lanesForInput(
              cm::CoordinateMode::RectangularInterleaved,
              cp::PortChannelCounts(a, b), options);
          require(lanes >= 0 && lanes <= 16,
                  "interleaved lane count stays within Rack limits");
          for (int lane = 0; lane < lanes; ++lane) {
            std::array<float, 2> pair = cf::pairForLane(
                ports, cp::PortChannelCounts(a, b),
                cm::CoordinateMode::RectangularInterleaved, lane, options);
            (void)pair;
          }
        }
      }

      for (int separated = 0; separated <= 1; ++separated) {
        cf::Options options(cf::PartialPairPolicy::Strict,
                            cf::InterleavedBankPolicy::Strict,
                            cf::separatedLanePolicyFromInt(separated));
        int lanes = cf::lanesForInput(
            cm::CoordinateMode::RectangularSeparated, cp::PortChannelCounts(a, b),
            options);
        require(lanes >= 0 && lanes <= 16,
                "separated lane count stays within Rack limits");
        for (int lane = 0; lane < lanes; ++lane) {
          std::array<float, 2> pair = cf::pairForLane(
              ports, cp::PortChannelCounts(a, b),
              cm::CoordinateMode::RectangularSeparated, lane, options);
          (void)pair;
        }
      }
    }
  }

  std::puts("compoly input formation tests passed");
  return 0;
}
