#pragma once

#include <string>

#include "../ComputerscareTextEditorTypes.hpp"

namespace computerscare {
namespace blunch {

struct KeyboardShortcutOptions {
  bool submitOnEnter = false;
  bool openOnEnter = false;
  bool stopShortcutEnabled = false;
};

bool handleKeyboardShortcut(int key, const std::string& keyName, int mods,
                            int action, const KeyboardShortcutOptions& options,
                            ComputerscareTextEditorCommands& commands);
bool isHardStopShortcut(int key, const std::string& keyName, int mods);
bool isRunToggleShortcut(int key, const std::string& keyName, int mods);

}  // namespace blunch
}  // namespace computerscare
