#pragma once

#include <cstdint>
#include <string>

#include "../ComputerscareTextEditorTypes.hpp"

namespace computerscare {
namespace blunch {

struct ControlSelection {
  int focus = 0;
  int anchor = 0;
  unsigned selected = 1;
  unsigned rangeBase = 0;
  bool extending = false;
  void moveTo(int row, bool extend);
  void toggleFocused();
  void selectOnly(int row);
  unsigned targets() const { return selected; }
};
enum class ControlCommand {
  Unhandled,
  None,
  Navigate,
  ToggleSelection,
  TogglePlayback,
  Reset
};
ControlCommand handleControlShortcut(int key, int mods, int action,
                                     ControlSelection& selection);

struct KeyboardShortcutOptions {
  bool submitOnEnter = false;
  bool openOnEnter = false;
  bool stopShortcutEnabled = false;
};

bool handleKeyboardShortcut(int key, const std::string& keyName, int mods,
                            int action, const KeyboardShortcutOptions& options,
                            ComputerscareTextEditorCommands& commands);
bool isShortcutHelpCharacter(uint32_t codepoint);
bool isShortcutHelpKey(int key, const std::string& keyName, int mods);
bool isHardStopShortcut(int key, const std::string& keyName, int mods);
bool isRunToggleShortcut(int key, const std::string& keyName, int mods);

}  // namespace blunch
}  // namespace computerscare
