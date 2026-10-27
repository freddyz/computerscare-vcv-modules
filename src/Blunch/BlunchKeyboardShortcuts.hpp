#pragma once

#include <cstdint>
#include <string>

#include "../ComputerscareTextEditorShortcuts.hpp"

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

// Keep existing Blunch callers source-compatible; the reusable editor owns
// command dispatch and has no dependency on Blunch's channel model.
using text_editor::handleKeyboardShortcut;
using text_editor::isHardStopShortcut;
using text_editor::isRunToggleShortcut;
using text_editor::KeyboardShortcutOptions;
bool isShortcutHelpCharacter(uint32_t codepoint);
bool isShortcutHelpKey(int key, const std::string& keyName, int mods);

}  // namespace blunch
}  // namespace computerscare
