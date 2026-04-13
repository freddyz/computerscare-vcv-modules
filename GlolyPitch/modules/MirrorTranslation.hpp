#pragma once
// Mirror effect: pan the image within the panel.
// tx/ty are offsets in NVG units applied before scale/rotation.
// Applied by shifting the center translation, so the image moves in
// panel-local space (before other transforms).
// This is handled at the nvgTranslate(cx+tx, cy+ty) level, not here,
// but these helpers are provided for readability.
inline float translationOffset(bool on, float value) {
  return on ? value : 0.f;
}
