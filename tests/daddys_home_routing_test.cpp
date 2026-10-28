#include <algorithm>
#include <cassert>
#include <random>

#include "DaddysHome/Edit.hpp"
#include "DaddysHome/Randomize.hpp"
#include "DaddysHome/Routing.hpp"

int main() {
  auto routing = daddys_home::identity();
  auto rows = daddys_home::view(routing, {});
  assert(rows.size() == 16 && rows[0].label == "A");
  assert(rows[0].cells.size() == 16 && rows[0].cells[15] == "a16");
  rows = daddys_home::view(routing, {-1, -1, 0, -1});
  assert(rows.size() == 16 && rows[3].label == "a4 ->" &&
         rows[3].cells[0] == "A4");
  // A source can feed many destinations, and each destination has one source.
  for (int ch : {1, 2, 3, 8}) routing[16 + ch] = 1;
  routing[32] = -1;
  rows = daddys_home::view(routing, {0, -1, -1, -1});
  assert(rows.size() == 16 && rows[1].cells.size() == 5);
  assert(rows[1].cells[4] == "B9");
  rows = daddys_home::view(routing, {0, 1, 1, -1});
  assert(rows.size() == 1 && rows[0].cells.size() == 4);
  rows = daddys_home::view(routing, {0, 1, 1, 8});
  assert(rows.size() == 1 && rows[0].cells[0] == "B9");
  rows = daddys_home::view(routing, {0, 1, 2, 0});
  assert(rows[0].cells[0] == "--");
  rows = daddys_home::view(routing, {-1, -1, 2, 0});
  assert(rows[0].label == "-- ->" && rows[0].cells[0] == "C1");
  daddys_home::Availability available;
  available.inputs[0] = 4;
  available.inputs[1] = 16;
  available.outputs[0] = 2;
  available.outputs[1] = 4;
  assert(available.source(3) && !available.source(4));
  assert(!available.source(32) && !available.source(-1));
  assert(available.route(routing, 1) && !available.route(routing, 2));
  assert(available.route(routing, 19) && !available.route(routing, 24));
  assert(!available.route(routing, 32));
  // Every combination of All/specific selectors uses the same intersection.
  for (int mask = 0; mask < 16; ++mask) {
    daddys_home::Filter f{mask & 1 ? 0 : -1, mask & 2 ? 1 : -1,
                          mask & 4 ? 1 : -1, mask & 8 ? 8 : -1};
    for (int dest = 0; dest < 256; ++dest) {
      int source = routing[dest];
      bool expected =
          (f.inputPort < 0 || (source >= 0 && source / 16 == f.inputPort)) &&
          (f.inputChannel < 0 ||
           (source >= 0 && source % 16 == f.inputChannel)) &&
          (f.outputPort < 0 || dest / 16 == f.outputPort) &&
          (f.outputChannel < 0 || dest % 16 == f.outputChannel);
      assert(daddys_home::matches(f, dest, source) == expected);
    }
    assert(!daddys_home::view(routing, f).empty());
  }
  std::mt19937 rng(123);
  auto pick = [&](int count) { return int(rng() % unsigned(count)); };
  daddys_home::Availability all;
  all.inputs.fill(16);
  all.outputs.fill(16);
  auto initial = daddys_home::identity();
  for (int mask = 0; mask < 16; ++mask) {
    daddys_home::Filter f{mask & 1 ? 0 : -1, mask & 2 ? 1 : -1,
                          mask & 4 ? 0 : -1, mask & 8 ? 1 : -1};
    for (int mode = 0; mode < 4; ++mode) {
      daddys_home::RandomOptions options;
      options.mode = static_cast<daddys_home::RandomMode>(mode);
      auto after = daddys_home::randomize(initial, f, all, options, pick);
      std::vector<int> beforeSet, afterSet;
      for (int dest = 0; dest < 256; ++dest) {
        if (!daddys_home::matches(f, dest, initial[dest])) {
          assert(after[dest] == initial[dest]);
          continue;
        }
        assert(after[dest] >= 0 && after[dest] < 256);
        if (f.inputPort >= 0) assert(after[dest] / 16 == f.inputPort);
        if (f.inputChannel >= 0) assert(after[dest] % 16 == f.inputChannel);
        if (mode == 2) assert(after[dest] / 16 == initial[dest] / 16);
        if (mode == 3) assert(after[dest] % 16 == initial[dest] % 16);
        beforeSet.push_back(initial[dest]);
        afterSet.push_back(after[dest]);
      }
      if (mode == 1) {
        std::sort(beforeSet.begin(), beforeSet.end());
        std::sort(afterSet.begin(), afterSet.end());
        assert(beforeSet == afterSet);
      }
    }
  }
  daddys_home::RandomOptions options;
  options.unique = true;
  auto shuffled = daddys_home::randomize(initial, {}, all, options, pick);
  auto sorted = shuffled;
  std::sort(sorted.begin(), sorted.end());
  assert(sorted == initial);
  options.connectedSources = true;
  options.activeDestinations = true;
  daddys_home::Availability limited;
  limited.inputs[0] = 2;
  limited.outputs[0] = 8;
  auto limitedResult =
      daddys_home::randomize(initial, {}, limited, options, pick);
  int blanks = 0;
  for (int dest = 0; dest < 8; ++dest) {
    if (limitedResult[dest] < 0)
      ++blanks;
    else
      assert(limitedResult[dest] < 2);
  }
  assert(blanks == 6);
  for (int dest = 8; dest < 256; ++dest)
    assert(limitedResult[dest] == initial[dest]);
  daddys_home::Availability none;
  options = {};
  options.connectedSources = true;
  assert(daddys_home::randomize(initial, {}, none, options, pick) == initial);
  options.allowBlank = true;
  auto silent = daddys_home::randomize(initial, {}, none, options, pick);
  for (int source : silent) assert(source == -1);
  for (int source = 0; source < 256; ++source) {
    int parsed = -2;
    assert(daddys_home::parseSource(daddys_home::address(source), parsed) &&
           parsed == source);
  }
  int parsed = -2;
  assert(daddys_home::parseSource(" X ", parsed) && parsed == -1);
  assert(daddys_home::parseSource("I8", parsed) && parsed == 135);
  for (const char* bad :
       {"", "a0", "a17", "q1", "a01", "i8x", "a-1", "5a", "a 5"})
    assert(!daddys_home::parseSource(bad, parsed));
  assert(daddys_home::matrixCell(5, 0, 356, 344) == 0);
  assert(daddys_home::matrixCell(350.9f, 343.9f, 356, 344) == 255);
  assert(daddys_home::matrixCell(351, 10, 356, 344) == -1);
  assert(daddys_home::pickerCell(25, 32) == 0 &&
         daddys_home::pickerCell(344.9f, 335.9f) == 255);
  assert(daddys_home::pickerCell(345, 32) == -1 &&
         daddys_home::pickerCell(25, 336) == -1);
  assert(daddys_home::draggedSource(-1, -200) == -1 &&
         daddys_home::draggedSource(255, 200) == 255);
  for (int source = -1; source <= 255; ++source)
    assert(daddys_home::draggedSource(source, 0) == source);
  options = {};
  assert(daddys_home::randomize(initial, {}, all, options, pick, 0.f) ==
         initial);
  for (int mode = 0; mode < 4; ++mode) {
    options.mode = static_cast<daddys_home::RandomMode>(mode);
    auto moved =
        daddys_home::randomize(initial, {}, all, options, pick, 1.f, true);
    if (mode == 1) {
      auto sortedMoved = moved;
      std::sort(sortedMoved.begin(), sortedMoved.end());
      assert(sortedMoved == initial);
      continue;
    }
    for (int dest = 0; dest < 256; ++dest) {
      assert(moved[dest] >= 0 && moved[dest] < 256);
      assert(std::abs(moved[dest] / 16 - dest / 16) <= 1 &&
             std::abs(moved[dest] % 16 - dest % 16) <= 1);
      if (mode == 2) assert(moved[dest] / 16 == dest / 16);
      if (mode == 3) assert(moved[dest] % 16 == dest % 16);
    }
  }
  options = {};
  options.allowBlank = true;
  auto withBlank = daddys_home::randomize(initial, {}, all, options, pick);
  for (int value : withBlank) assert(value >= -1 && value < 256);
}
