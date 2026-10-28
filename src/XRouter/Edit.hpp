#pragma once
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace xrouter {
inline bool parseSource(std::string text, int& source) {
  auto first = text.find_first_not_of(" \t\r\n"),
       last = text.find_last_not_of(" \t\r\n");
  if (first == std::string::npos) return false;
  text = text.substr(first, last - first + 1);
  if (text == "X" || text == "x" || text == "--") {
    source = -1;
    return true;
  }
  if (text.size() < 2 || text.size() > 3) return false;
  int port = std::tolower(static_cast<unsigned char>(text[0])) - 'a';
  if (port < 0 || port > 15 || text[1] == '0') return false;
  int channel = 0;
  for (size_t i = 1; i < text.size(); ++i) {
    if (text[i] < '0' || text[i] > '9') return false;
    channel = channel * 10 + text[i] - '0';
  }
  if (channel < 1 || channel > 16) return false;
  source = port * 16 + channel - 1;
  return true;
}
inline int matrixCell(float x, float y, float width, float height) {
  if (x < 5.f || x >= width - 5.f || y < 0.f || y >= height) return -1;
  return int(y * 16.f / height) * 16 + int((x - 5.f) * 16.f / (width - 10.f));
}
inline int pickerCell(float x, float y) {
  if (x < 25.f || x >= 345.f || y < 32.f || y >= 336.f) return -1;
  return int((y - 32.f) / 19.f) * 16 + int((x - 25.f) / 20.f);
}
inline int draggedSource(int start, float steps) {
  return std::max(-1, std::min(255, int(std::round(start + steps))));
}
}  // namespace xrouter
