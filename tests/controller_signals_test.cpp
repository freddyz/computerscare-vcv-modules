#include <cassert>
#include <cmath>

#include "Controller/Signals.hpp"
using namespace computerscare::controller;
int main() {
  const int expected1[] = {0, 1,  2,  3, 4,  5,  6,  7,
                           8, 19, 10, 9, 20, 11, 17, 18};
  const int expected2[] = {15, 16, 12, 13, 14};
  bool seen[21] = {};
  for (int i = 0; i < 16; ++i) {
    assert(buttonBankControl(0, i) == expected1[i]);
    assert(!seen[expected1[i]]);
    seen[expected1[i]] = true;
  }
  for (int i = 0; i < 5; ++i) {
    assert(buttonBankControl(1, i) == expected2[i]);
    assert(!seen[expected2[i]]);
    seen[expected2[i]] = true;
  }
  for (bool present : seen) assert(present);

  assert(rumbleLevel(-5) == 0 && rumbleLevel(0) == 0 &&
         rumbleLevel(5) == 32767 && rumbleLevel(10) == 65535 &&
         rumbleLevel(15) == 65535);
  assert(rumbleLevel(NAN) == 0 && rumbleLevel(INFINITY) == 0);
  float x = .01f, y = .02f;
  deadzone(x, y, .08f);
  assert(x == 0 && y == 0);
  x = 1;
  y = 1;
  deadzone(x, y, .08f);
  assert(std::abs(std::hypot(x, y) - 1) < 1e-5);
  ButtonSignal b;
  assert(b.process(true, true, 9, 0, .001, .01) ==
         10);  // Gate reflects a held control.
  assert(b.process(true, true, 9, 1, .001, .01) ==
         0);  // Mode changes do not create triggers.
  assert(b.process(true, false, 10, 1, .001, .01) ==
         10);  // A short press survives polling.
  for (int i = 0; i < 20; ++i) b.process(true, false, 10, 1, .001, .01);
  assert(b.process(true, false, 10, 1, .001, .01) == 0);
  assert(b.process(true, false, 10, 2, .001, .01) == 0);
  assert(b.process(true, false, 11, 2, .001, .01) == 10);
  assert(b.process(true, false, 13, 2, .001, .01) ==
         10);  // Two presses retain toggle parity.
  assert(b.process(true, false, 14, 2, .001, .01) == 0);
  assert(b.process(false, true, 15, 2, .001, .01) == 0);
  assert(b.process(true, true, 15, 2, .001, .01) ==
         0);  // Reconnection clears the latch.
}
