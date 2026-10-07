#pragma once
#include "Types.hpp"
namespace windchimes {
struct Selection {
  int selected = 0;
  uint32_t mask = 1u;
  bool contains(int i) const { return (mask & (1u << i)) != 0; }
  void selectOnly(int i) {
    selected = i;
    mask = 1u << i;
  }
  void clear() { mask = 0; }
  void toggle(int i) {
    mask ^= 1u << i;
    if (contains(i))
      selected = i;
    else if (selected == i)
      for (int j = 0; j < maxSets; ++j)
        if (contains(j)) {
          selected = j;
          break;
        }
  }
  void retain(uint32_t enabled) {
    mask &= enabled;
    if (!contains(selected))
      for (int i = 0; i < maxSets; ++i)
        if (contains(i)) {
          selected = i;
          break;
        }
  }
};
}  // namespace windchimes
