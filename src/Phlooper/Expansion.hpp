#pragma once
namespace phlooper {
// Same mapping rules as Compoly: mono broadcast, cycle, zero pad, stall.
inline int expansionSource(int voice, int count, int mode) {
  if (count <= 0) return -1;
  if (voice < count) return voice;
  if (mode == 1) return voice % count;
  if (mode == 3) return count - 1;
  return mode == 0 && count == 1 ? 0 : -1;
}
}  // namespace phlooper
