#pragma once
#include <algorithm>
#include <utility>

namespace beekapoo {
// Allocate within the existing sections. A lone output needs enough width
// for its own name; it should not inherit a tiny input-column width.
inline std::pair<float, float> portPaneSpace(float available, int inputs,
                                             int outputs, int outputCount) {
  int columns = inputs + outputs;
  if (!columns) return {0, 0};
  float right = available * outputs / columns;
  if (inputs && outputCount == 1)
    right = std::min(available - 20 * inputs, std::max(right, 48.f));
  right = std::max(0.f, std::min(available, right));
  return {available - right, right};
}
struct PortRect {
  float x = 0, y = 0, w = 0, h = 0;
  PortRect() = default;
  PortRect(float x, float y, float w, float h) : x(x), y(y), w(w), h(h) {}
};
struct PortCell {
  PortRect jack, gain, label;
  bool large = false;
  float fontSize = 0;
};
// Two bounded arrangements: gain above the jack, or beside it. Use spare
// space for artwork and captions, preserving a stable centered group. This
// is only called on layout changes, never from DSP or the draw loop.
inline PortCell planPortCell(float width, float height, bool input) {
  PortCell best;
  float bestScore = -1;
  for (bool beside : {false, true}) {
    if (beside && !input) continue;
    for (bool captions : {true, false}) {
      if (!input && !captions) continue;
      float w = std::max(0.f, width - 4), h = std::max(0.f, height - 4);
      float side = beside ? 25.f : 0.f;
      float above = input && !beside ? 23.f : 0.f;
      if (w - side < 16 || h - above < 16 || (input && w < 20)) continue;
      float labelHeight = 0;
      float baseline = beside ? 20.f : 16.f;
      float room = h - above - baseline - 4;
      if (captions && w >= 20 && room >= 10)
        labelHeight = std::min(room, std::min(36.f, std::max(12.f, h * .28f)));
      float gap = labelHeight ? 4.f : 0.f;
      float jackW = w - side, jackH = h - above - labelHeight - gap;
      PortCell candidate;
      candidate.large = jackW >= 27 && jackH >= 29;
      float scale = candidate.large
                        ? std::min(1.5f, std::min(jackW / 27, jackH / 29))
                        : 1;
      jackW = candidate.large ? 27 * scale : 16;
      jackH = candidate.large ? 29 * scale : 16;
      float bodyHeight = above + std::max(jackH, beside ? 20.f : 0.f);
      float top = (height - bodyHeight - gap - labelHeight) * .5f;
      float left = (width - jackW - side) * .5f;
      candidate.jack = {left + side,
                        top + above + (beside ? (bodyHeight - jackH) * .5f : 0),
                        jackW, jackH};
      if (input)
        candidate.gain =
            beside ? PortRect(left, top + (bodyHeight - 20) * .5f, 20, 20)
                   : PortRect((width - 20) * .5f, top, 20, 20);
      if (labelHeight) {
        candidate.label = {2, top + bodyHeight + gap, w, labelHeight};
        candidate.fontSize = std::max(8.f, std::min(18.f, labelHeight * .7f));
      }
      float score = jackW + (input && candidate.large ? 12 : 0) +
                    (labelHeight ? (input ? 12 : 20) : 0) +
                    std::min(labelHeight, 24.f) * .25f;
      if (score > bestScore) {
        bestScore = score;
        best = candidate;
      }
    }
  }
  // Very dense existing grids must still show every endpoint.
  if (bestScore < 0) {
    best.jack = {(width - 16) * .5f, input ? height - 18 : (height - 16) * .5f,
                 16, 16};
    if (input) best.gain = {(width - 20) * .5f, best.jack.y - 23, 20, 20};
  }
  return best;
}
}  // namespace beekapoo
