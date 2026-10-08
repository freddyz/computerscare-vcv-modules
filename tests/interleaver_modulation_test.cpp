#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>

#include "Interleaver/Engine.hpp"
using namespace interleaver;
static float tone(int n, float hz = 200, float amplitude = 5) {
  return amplitude * std::sin(6.283185307179586 * hz * n / 48000);
}
static std::unique_ptr<Engine> make(const Settings& s) {
  std::unique_ptr<Engine> e(new Engine);
  e->setSampleRate(48000);
  e->configure(s);
  return e;
}
int main() {
  assert(cv(40, 10, 1, 20, 200) == 200);
  assert(cv(40, 10, -1, 20, 200) == 20);
  assert(cv(40, 1000, .5, 20, 200) == 130);
  assert(cv(40, 5, 0, 20, 200) == 40);
  assert(cv(40, std::numeric_limits<float>::quiet_NaN(), 1, 20, 200) == 40);
  Settings s;
  // Every operation, timing and routing combination, including live selector
  // changes.
  auto e = make(s);
  for (int operation = 0; operation < 7; ++operation)
    for (int timing = 0; timing < 3; ++timing)
      for (int routing = 0; routing < 4; ++routing) {
        s.operation = operation;
        s.timing = timing;
        s.routing = routing;
        s.a = 3;
        s.b = 1;
        s.period = 10;
        e->configure(s);
        double energy = 0;
        for (int n = 0; n < 12000; ++n) {
          float v = e->process(tone(n), tone(n, 400), (n / 1000) % 2 ? 10 : 0);
          assert(std::isfinite(v) && std::fabs(v) < 15);
          if (n > 4000) energy += v * v;
        }
        assert(energy > 100);
        assert(e->activeOperation() == operation &&
               e->activeTiming() == timing && e->activeRouting() == routing);
      }
  // A rising edge toggles exactly once; a held gate isn't a stream of triggers.
  s = Settings();
  s.routing = Trigger;
  e = make(s);
  for (int n = 0; n < 22000; ++n) {
    float trigger = (n >= 10000 && n < 15000) ? 10 : 0;
    if (n == 17000) trigger = 10;
    float reset = n == 20000 ? 10 : 0;
    e->process(tone(n), tone(n, 400), trigger, reset);
    if (n == 11000 || n == 16000) assert(e->activeSource() == 1);
    if (n == 18000 || n == 21000) assert(e->activeSource() == 0);
  }
  s.routing = Gate;
  e = make(s);
  for (int n = 0; n < 18000; ++n) {
    e->process(tone(n), tone(n, 400), n >= 9000 ? 10 : 0);
    if (n == 8000) assert(e->activeSource() == 0);
    if (n == 10000 || n == 17000) assert(e->activeSource() == 1);
  }
  // Timer patterns use counts as interval multipliers: 3 A, 1 B at 100 ms.
  s.routing = Timer;
  s.a = 3;
  s.b = 1;
  s.period = 100;
  e = make(s);
  long totals[2] = {0, 0};
  int runs[2] = {0, 0}, prior = -1, length = 0;
  for (int n = 0; n < 192000; ++n) {
    e->process(tone(n), tone(n, 400));
    int source = e->activeSource();
    if (source != prior) {
      if (n > 22000 && prior >= 0) {
        totals[prior] += length;
        ++runs[prior];
      }
      prior = source;
      length = 0;
    }
    ++length;
  }
  assert(runs[0] > 4 && runs[1] > 4);
  assert(std::fabs(double(totals[0]) / runs[0] - 14400) < 250);
  assert(std::fabs(double(totals[1]) / runs[1] - 4800) < 250);
  // Weave's endpoints select one source exclusively in both anchored and free
  // playback.
  for (int timing = 0; timing < 3; ++timing)
    for (int endpoint = 0; endpoint < 2; ++endpoint) {
      s = Settings();
      s.operation = Weave;
      s.timing = timing;
      s.balance = endpoint;
      e = make(s);
      for (int n = 0; n < 24000; ++n) {
        e->process(tone(n), tone(n, 400));
        if (n > 5000) assert(e->activeSource() == endpoint);
      }
    }
  s = Settings();
  s.operation = Half;
  e = make(s);
  for (int n = 0; n < 48000; ++n) e->process(tone(n), tone(n, 400));
  assert(std::abs(int(e->capturedHalves(0)) - 2 * int(e->capturedCycles(0))) <=
         2);
  // Reverse playback of a steady sine should invert its waveform.
  s = Settings();
  s.join = 0;
  auto forward = make(s);
  s.operation = Reverse;
  auto reverse = make(s);
  double error = 0, energy = 0;
  for (int n = 0; n < 48000; ++n) {
    float a = forward->process(tone(n), tone(n)),
          b = reverse->process(tone(n), tone(n));
    if (n > 24000) {
      error += (a + b) * (a + b);
      energy += a * a;
    }
  }
  assert(error < energy * .001);
  // Repeat retains a captured cycle through the three-cycle A run.
  s = Settings();
  s.operation = Repeat;
  s.a = 3;
  s.b = 1;
  s.join = 0;
  e = make(s);
  int old = -1, at = -1, checked = 0;
  float first = 0, second = 0;
  for (int n = 0; n < 48000; ++n) {
    float amplitude = 1 + float((n / 240) % 4);
    float v = e->process(tone(n, 200, amplitude), tone(n, 200, 2));
    int source = e->activeSource();
    if (source == 0 && old == 1) at = n;
    old = source;
    if (at > 10000) {
      if (n == at + 60) first = v;
      if (n == at + 300) second = v;
      if (n == at + 540) {
        assert(std::fabs(first - second) < .1 && std::fabs(first - v) < .1);
        ++checked;
      }
    }
  }
  assert(checked > 10);
  // Buffer modulation doesn't reset the scheduler or jump/backtrack the read
  // clock.
  s = Settings();
  e = make(s);
  int switches = 0, oldSource = -1;
  float previous = 0;
  for (int n = 0; n < 96000; ++n) {
    if (n % 32 == 0) {
      s.buffer = (n / 32) % 2 ? 200 : 20;
      e->configure(s);
    }
    double before = e->bufferSamples();
    float v = e->process(tone(n), tone(n, 400));
    assert(std::fabs(e->bufferSamples() - before) <= .12500001);
    if (n > 15000) assert(std::fabs(v - previous) < 1);
    previous = v;
    int source = e->activeSource();
    if (source != oldSource) ++switches;
    oldSource = source;
  }
  assert(switches > 200);
  // Equal-value routing eventually responds even if no intersection is
  // available.
  s = Settings();
  s.operation = Equal;
  s.routing = Gate;
  e = make(s);
  for (int n = 0; n < 15000; ++n) {
    e->process(5, -5, n >= 10000 ? 10 : 0);
    if (n == 12000) assert(e->activeSource() == 1);
  }
  // A missing timing anchor must not prevent an external switch selecting B.
  s = Settings();
  s.routing = Gate;
  e = make(s);
  double audible = 0;
  for (int n = 0; n < 12000; ++n) {
    float v = e->process(0, tone(n), 10);
    if (n > 6000) audible += v * v;
  }
  assert(e->activeSource() == 1 && audible > 10000);
  // Direct switch responds immediately after a gate edge and crossfades without
  // a step.
  s = Settings();
  s.operation = Direct;
  s.routing = Gate;
  e = make(s);
  float previousDirect = 0;
  for (int n = 0; n < 15000; ++n) {
    float gate = n >= 10000 ? 10 : 0;
    float v = e->process(tone(n), tone(n, 400), gate);
    if (n == 10000) assert(e->activeSource() == 1);
    if (n > 8000) assert(std::fabs(v - previousDirect) < .5);
    previousDirect = v;
  }
  // Selector modulation during silence has a bounded fallback and remains
  // finite.
  s = Settings();
  e = make(s);
  for (int n = 0; n < 4000; ++n) e->process(0, 0);
  s.operation = Reverse;
  s.timing = 2;
  s.routing = Trigger;
  e->configure(s);
  for (int n = 0; n < 1000; ++n) assert(e->process(0, 0) == 0);
  assert(e->activeOperation() == Reverse && e->activeTiming() == 2 &&
         e->activeRouting() == Trigger);
  // New operations at every supported test sample rate, with both silence and
  // audio.
  for (float rate : {44100.f, 96000.f, 192000.f})
    for (int operation = 0; operation < 7; ++operation) {
      s = Settings();
      s.operation = operation;
      s.routing = Timer;
      s.period = 10;
      s.timing = operation % 3;
      e = make(s);
      e->setSampleRate(rate);
      e->configure(s);
      for (int n = 0; n < int(rate * .06f); ++n) assert(e->process(0, 0) == 0);
      double energy = 0;
      for (int n = 0; n < int(rate * .2f); ++n) {
        float a = 5 * std::sin(6.283185307179586 * 200 * n / rate);
        float b = 5 * std::sin(6.283185307179586 * 330 * n / rate);
        float v = e->process(a, b);
        assert(std::isfinite(v) && std::fabs(v) < 15);
        energy += v * v;
      }
      assert(energy > rate * .1);
    }
  // A gate may hold Repeat for longer than history capacity; refresh before
  // wrap.
  s = Settings();
  s.operation = Repeat;
  s.routing = Gate;
  e = make(s);
  float lastHeld = 0;
  for (int n = 0; n < 48000 * 7; ++n) {
    float v = e->process(tone(n), tone(n, 400), 0);
    assert(std::isfinite(v) && std::fabs(v) < 11);
    if (n > 8000) assert(std::fabs(v - lastHeld) < 1);
    lastHeld = v;
  }
  // Noisy crossing metadata wraps; non-finite settings/CV and impulses stay
  // bounded.
  s = Settings();
  s.operation = Half;
  s.timing = 2;
  e = make(s);
  unsigned noise = 123;
  for (int n = 0; n < 96000; ++n) {
    noise = noise * 1664525u + 1013904223u;
    float a = float(noise >> 16) / 6553.5f - 5;
    noise = noise * 1664525u + 1013904223u;
    float b = float(noise >> 16) / 6553.5f - 5;
    float v = e->process(a, b);
    assert(std::isfinite(v) && std::fabs(v) < 20);
  }
  assert(e->capturedHalves(0) > 8192);
  s.buffer = std::numeric_limits<float>::infinity();
  s.join = std::numeric_limits<float>::quiet_NaN();
  s.period = -100;
  s.balance = 100;
  e->configure(s);
  for (int n = 0; n < 48000; ++n) {
    float v = e->process(n % 1000 == 0 ? 1000 : 0, n % 1001 == 0 ? -1000 : 0);
    assert(std::isfinite(v) && std::fabs(v) < 2100);
  }
  std::cout << "Interleaver modulation: all operations/routes, CV limits, "
               "gates/triggers/reset, timer, weave, repeat/reverse and buffer "
               "slew passed\n";
}
