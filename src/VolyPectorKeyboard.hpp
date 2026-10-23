#pragma once
namespace computerscare {
namespace voly {
struct FocusKey {
  int band = -1;
  int channel = -1;
  bool valid() const { return band >= 0 || channel >= 0; }
};
inline FocusKey focusKey(int key) {
  FocusKey result;
  if (key >= 'a' && key <= 'z') key += 'A' - 'a';
  if (key >= 'A' && key <= 'P')
    result.band = key - 'A';
  else if (key >= '1' && key <= '9')
    result.channel = key - '1';
  else if (key == '0')
    result.channel = 9;
  else {
    const char* extra = "QWRTY";
    for (int i = 0; i < 5; ++i)
      if (key == extra[i]) result.channel = 10 + i;
  }
  return result;
}
inline char channelKey(int channel) {
  return channel >= 0 && channel < 15 ? "1234567890qwrty"[channel] : '\0';
}
}  // namespace voly
}  // namespace computerscare
