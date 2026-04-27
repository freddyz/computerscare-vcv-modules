#include "complex/math/ComplexFormat.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
}

void requireStr(const std::string& actual, const std::string& expected,
                const char* message) {
  if (actual != expected) {
    std::fprintf(stderr, "FAIL: %s: got \"%s\" expected \"%s\"\n", message,
                 actual.c_str(), expected.c_str());
    std::exit(1);
  }
}

}  // namespace

int main() {
  namespace cf = cpx::complex_math;
  constexpr float pi = 3.14159265358979f;

  // ── rectParts ──────────────────────────────────────────────────────────────

  {
    cf::ComplexRenderParts p = cf::rectParts(3.f, 2.f);
    requireStr(p.real, "3.00", "rectParts positive: real");
    requireStr(p.sign, "+", "rectParts positive: sign");
    requireStr(p.imag, "2.00", "rectParts positive: imag");
    requireStr(p.imagUnit, "i", "rectParts: imagUnit");
  }

  {
    cf::ComplexRenderParts p = cf::rectParts(3.f, -2.f);
    requireStr(p.sign, "-", "rectParts negative imag: sign");
    requireStr(p.imag, "2.00", "rectParts negative imag: abs value");
  }

  {
    cf::ComplexRenderParts p = cf::rectParts(0.f, 0.f);
    requireStr(p.real, "0.00", "rectParts zero: real");
    requireStr(p.sign, "+", "rectParts zero: sign");
    requireStr(p.imag, "0.00", "rectParts zero: imag");
  }

  {
    cf::ComplexRenderParts p = cf::rectParts(3.f, 2.f, 0);
    requireStr(p.real, "3", "rectParts decimals=0: real");
    requireStr(p.imag, "2", "rectParts decimals=0: imag");
  }

  {
    cf::ComplexRenderParts p = cf::rectParts(3.14159f, 2.71828f, 4);
    requireStr(p.real, "3.1416", "rectParts decimals=4: real");
    requireStr(p.imag, "2.7183", "rectParts decimals=4: imag");
  }

  // ── polarParts: engineering ────────────────────────────────────────────────

  {
    cf::ComplexRenderParts p = cf::polarParts(
        4.f, pi / 4.f, cf::AngleUnit::Degree, cf::PolarDisplayStyle::Engineering);
    requireStr(p.magnitude, "4.00", "polar engineering deg: magnitude");
    requireStr(p.angleSym, "∠", "polar engineering: angleSym");
    requireStr(p.angle, "45.00°", "polar engineering deg: angle");
    requireStr(p.angleClose, "", "polar engineering: angleClose empty");
  }

  {
    cf::ComplexRenderParts p = cf::polarParts(
        1.f, pi / 2.f, cf::AngleUnit::Radian, cf::PolarDisplayStyle::Engineering);
    requireStr(p.angle, "1.57 rad", "polar engineering rad: angle");
  }

  {
    cf::ComplexRenderParts p = cf::polarParts(
        5.f, 0.f, cf::AngleUnit::Degree, cf::PolarDisplayStyle::Engineering);
    requireStr(p.magnitude, "5.00", "polar engineering zero angle: magnitude");
    requireStr(p.angle, "0.00°", "polar engineering zero angle: angle");
  }

  // ── polarParts: exponential ────────────────────────────────────────────────

  {
    cf::ComplexRenderParts p = cf::polarParts(
        2.f, pi / 3.f, cf::AngleUnit::Degree, cf::PolarDisplayStyle::Exponential);
    requireStr(p.magnitude, "2.00", "polar exponential: magnitude");
    requireStr(p.angleSym, "e^(i·", "polar exponential: angleSym");
    requireStr(p.angle, "60.00°", "polar exponential: angle");
    requireStr(p.angleClose, ")", "polar exponential: angleClose");
  }

  {
    cf::ComplexRenderParts p = cf::polarParts(
        1.f, pi, cf::AngleUnit::Radian, cf::PolarDisplayStyle::Exponential);
    requireStr(p.angle, "3.14 rad", "polar exponential rad: angle");
  }

  std::puts("complex format tests passed");
  return 0;
}
