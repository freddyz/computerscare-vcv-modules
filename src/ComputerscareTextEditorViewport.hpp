#pragma once
#include "ComputerscareTextEditorTypes.hpp"
struct ComputerscareTextEditorViewport {
  Vec size, scale;
  ComputerscareTextEditorViewport(Vec physicalSize,
                                  const ComputerscareTextEditorStyle& style)
      : size(physicalSize),
        scale(std::max(.5f, 1.f + style.fontWidthOffset),
              std::max(.5f, 1.f + style.fontHeightOffset)) {}
  Vec toLogical(Vec point) const {
    return Vec(point.x / scale.x, point.y / scale.y);
  }
  Vec logicalSize() const { return toLogical(size); }
};
