#pragma once

#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

namespace cpx {
namespace complex_math {

enum class AngleUnit { Degree, Radian };
enum class PolarDisplayStyle { Engineering, Exponential };

struct ComplexRenderParts {
  // Rect:        real sign imag imagUnit    e.g.  "3.14" "+" "2.72" "i"
  // Engineering: magnitude angleSym angle   e.g.  "4.44" "∠" "40.80°"
  // Exponential: magnitude angleSym angle angleClose
  //                                         e.g.  "4.44" "e^(i·" "0.71°" ")"
  std::string real;
  std::string sign;
  std::string imag;
  std::string imagUnit;
  std::string magnitude;
  std::string angleSym;
  std::string angle;
  std::string angleClose;
};

namespace {

inline std::string fmtFloat(float v, int decimals) {
  int d = decimals < 0 ? 2 : decimals;
  std::ostringstream ss;
  ss << std::fixed << std::setprecision(d) << v;
  return ss.str();
}

}  // namespace

inline ComplexRenderParts rectParts(float x, float y, int decimals = -1) {
  ComplexRenderParts p;
  p.real = fmtFloat(x, decimals);
  p.sign = (y < 0.f) ? "-" : "+";
  p.imag = fmtFloat(std::fabs(y), decimals);
  p.imagUnit = "i";
  return p;
}

inline ComplexRenderParts polarParts(float r, float theta,
                                      AngleUnit unit = AngleUnit::Degree,
                                      PolarDisplayStyle style = PolarDisplayStyle::Engineering,
                                      int decimals = -1) {
  constexpr float pi = 3.14159265358979f;
  ComplexRenderParts p;
  p.magnitude = fmtFloat(r, decimals);

  float displayAngle = (unit == AngleUnit::Degree) ? theta * 180.f / pi : theta;
  std::string unitSuffix = (unit == AngleUnit::Degree) ? "°" : " rad";
  std::string angleStr = fmtFloat(displayAngle, decimals) + unitSuffix;

  if (style == PolarDisplayStyle::Engineering) {
    p.angleSym = "∠";  // ∠
    p.angle = angleStr;
    p.angleClose = "";
  } else {
    p.angleSym = "e^(i·";  // e^(i·
    p.angle = angleStr;
    p.angleClose = ")";
  }
  return p;
}

}  // namespace complex_math
}  // namespace cpx
