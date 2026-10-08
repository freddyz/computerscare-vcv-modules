#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>

#include "Interleaver/Engine.hpp"
static float sine(int n, float rate, float frequency) {
  return 5.f * std::sin(6.283185307179586 * frequency * n / rate);
}
int main() {
  for (float rate : {44100.f, 48000.f, 96000.f, 192000.f}) {
    std::unique_ptr<interleaver::Engine> e(new interleaver::Engine);
    e->setSampleRate(rate);
    e->configure(0, 1, 1, 40, .15f);
    for (int n = 0; n < int(rate * .1f); ++n) assert(e->process(0, 0) == 0);
    e->reset();
    for (int mode = 0; mode < 3; ++mode) {
      e->configure(mode, 3, 2, 40, .15f);
      double energy = 0;
      int counts[2] = {0, 0};
      for (int n = 0; n < int(rate); ++n) {
        float v = e->process(sine(n, rate, 200), sine(n, rate, 400));
        assert(std::isfinite(v) && std::fabs(v) < 11);
        if (n > rate * .2f) {
          energy += v * v;
          int s = e->activeSource();
          if (s >= 0) ++counts[s];
        }
      }
      assert(energy > rate && counts[0] > rate * .1f && counts[1] > rate * .1f);
    }
    e->reset();
    e->configure(0, 1, 1, 40, 0);
    for (int n = 0; n < int(rate * .04f); ++n) assert(e->process(5, 5) == 0);
    for (int n = 0; n < int(rate * 2); ++n)
      assert(std::isfinite(e->process(5, 5)));
    e->reset();
    assert(e->process(std::numeric_limits<float>::infinity(),
                      std::numeric_limits<float>::quiet_NaN()) == 0);
  }
  std::unique_ptr<interleaver::Engine> e(new interleaver::Engine);
  e->setSampleRate(48000);
  for (int mode = 0; mode < 3; ++mode) {
    e->reset();
    e->configure(mode, 3, 2, 40, 0);
    long total[2] = {0, 0};
    int runs[2] = {0, 0};
    int prior = -1, length = 0;
    for (int n = 0; n < 48000; ++n) {
      e->process(sine(n, 48000, 200), sine(n, 48000, 400));
      int s = e->activeSource();
      if (s != prior) {
        if (n > 10000 && prior >= 0) {
          total[prior] += length;
          ++runs[prior];
        }
        prior = s;
        length = 0;
      }
      ++length;
    }
    double a = double(total[0]) / runs[0], b = double(total[1]) / runs[1];
    assert(std::fabs(a - (mode == 1 ? 360 : 720)) < 2);
    assert(std::fabs(b - (mode == 0 ? 480 : 240)) < 2);
  }
  e->reset();
  e->configure(2, 1, 1, 40, .15f);
  for (int n = 0; n < 48000 * 7; ++n) {
    float v = e->process(sine(n, 48000, 2000), sine(n, 48000, 3000));
    assert(std::isfinite(v) && std::fabs(v) < 11);
  }
  assert(e->capturedCycles(0) > 8192 && e->activeSource() >= 0);
  e->configure(0, 1, 1, 20, 1);
  for (int n = 0; n < 48000; ++n)
    assert(std::isfinite(e->process(sine(n, 48000, 3), 0)));
  std::cout << "Interleaver: timing, sample rates, runs, silence/DC, invalid "
               "inputs and ring wrap passed\n";
}
