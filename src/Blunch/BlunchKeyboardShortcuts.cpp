#include "BlunchKeyboardShortcuts.hpp"

#include "../Computerscare.hpp"

namespace computerscare {
namespace blunch {

namespace {

bool isEnterKey(int key) {
  return key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER;
}

bool isCommaKey(int key, const std::string& keyName) {
  return key == GLFW_KEY_COMMA || keyName == ",";
}

bool isPeriodKey(int key, const std::string& keyName) {
  return key == GLFW_KEY_PERIOD || keyName == ".";
}

bool isSlashKey(int key, const std::string& keyName) {
  return key == GLFW_KEY_SLASH || keyName == "/";
}

bool isLeftBracketKey(int key, const std::string& keyName) {
  return key == GLFW_KEY_LEFT_BRACKET || keyName == "[";
}

bool isRightBracketKey(int key, const std::string& keyName) {
  return key == GLFW_KEY_RIGHT_BRACKET || keyName == "]";
}

}  // namespace

void ControlSelection::moveTo(int row, bool extend) {
  row = std::max(0, std::min(15, row));
  if (extend) {
    if (!extending) {
      anchor = focus;
      rangeBase = selected;
    }
    extending = true;
    unsigned range = 0;
    for (int i = std::min(anchor, row); i <= std::max(anchor, row); ++i)
      range |= 1u << i;
    selected = rangeBase | range;
  } else {
    if (selected == (1u << focus)) selected = 1u << row;
    extending = false;
    anchor = row;
  }
  focus = row;
}
void ControlSelection::toggleFocused() {
  unsigned next = selected ^ (1u << focus);
  if (next) selected = next;
  extending = false;
  anchor = focus;
}
void ControlSelection::selectOnly(int row) {
  focus = std::max(0, std::min(15, row));
  selected = 1u << focus;
  extending = false;
  anchor = focus;
}
ControlCommand handleControlShortcut(int key, int mods, int action,
                                     ControlSelection& selection) {
  int modifiers = mods & RACK_MOD_MASK;
  bool navigation = (key == GLFW_KEY_UP || key == GLFW_KEY_DOWN ||
                     key == GLFW_KEY_HOME || key == GLFW_KEY_END) &&
                    (modifiers == 0 || modifiers == GLFW_MOD_SHIFT);
  bool toggle = isEnterKey(key) &&
                (modifiers == GLFW_MOD_CONTROL || modifiers == GLFW_MOD_SUPER);
  bool play = key == GLFW_KEY_SPACE && modifiers == 0;
  bool reset = key == GLFW_KEY_R && modifiers == 0;
  if (!navigation && !toggle && !play && !reset)
    return ControlCommand::Unhandled;
  if (action != GLFW_PRESS && !(navigation && action == GLFW_REPEAT))
    return ControlCommand::None;
  if (navigation) {
    int row = key == GLFW_KEY_HOME ? 0
              : key == GLFW_KEY_END
                  ? 15
                  : selection.focus + (key == GLFW_KEY_UP ? -1 : 1);
    selection.moveTo(row, modifiers == GLFW_MOD_SHIFT);
    return ControlCommand::Navigate;
  }
  if (toggle) {
    selection.toggleFocused();
    return ControlCommand::ToggleSelection;
  }
  if (play) return ControlCommand::TogglePlayback;
  return ControlCommand::Reset;
}

bool handleKeyboardShortcut(int key, const std::string& keyName, int mods,
                            int action, const KeyboardShortcutOptions& options,
                            ComputerscareTextEditorCommands& commands) {
  int rackMods = mods & RACK_MOD_MASK;

  if (options.openOnEnter && isEnterKey(key) && rackMods == 0) {
    if (action == GLFW_PRESS) {
      commands.openCount++;
    }
    return true;
  }

  if (options.submitOnEnter && isEnterKey(key) &&
      (rackMods == GLFW_MOD_CONTROL || rackMods == GLFW_MOD_SUPER)) {
    if (action == GLFW_PRESS) {
      commands.submitCount++;
    }
    return true;
  }

  if (key == GLFW_KEY_ESCAPE) {
    if (action == GLFW_PRESS) {
      commands.cancelCount++;
    }
    return true;
  }

  if (options.stopShortcutEnabled && isCommaKey(key, keyName) &&
      rackMods == GLFW_MOD_CONTROL) {
    if (action == GLFW_PRESS) {
      commands.stopCount++;
    }
    return true;
  }

  if (key == GLFW_KEY_TAB) {
    if (action == GLFW_PRESS) {
      if ((mods & GLFW_MOD_SHIFT) != 0) {
        commands.switchViewBackwardCount++;
      } else {
        commands.switchViewForwardCount++;
      }
    }
    return true;
  }

  if (key == GLFW_KEY_SPACE && rackMods == GLFW_MOD_CONTROL) {
    if (action == GLFW_PRESS) {
      commands.startAllCount++;
    }
    return true;
  }

  if (isRunToggleShortcut(key, keyName, mods)) {
    if (action == GLFW_PRESS) {
      commands.runToggleCount++;
    }
    return true;
  }

  if (isHardStopShortcut(key, keyName, mods)) {
    if (action == GLFW_PRESS) {
      commands.hardStopCount++;
    }
    return true;
  }

  bool isLeftBracket = isLeftBracketKey(key, keyName);
  bool isRightBracket = isRightBracketKey(key, keyName);
  if ((isLeftBracket || isRightBracket) && rackMods == GLFW_MOD_CONTROL) {
    if (action == GLFW_PRESS) {
      if (isLeftBracket) {
        commands.navigateChannelBackwardCount++;
      } else {
        commands.navigateChannelForwardCount++;
      }
    }
    return true;
  }

  return false;
}

bool isShortcutHelpCharacter(uint32_t codepoint) { return codepoint == '`'; }

bool isShortcutHelpKey(int key, const std::string& keyName, int mods) {
  return (key == GLFW_KEY_GRAVE_ACCENT || keyName == "`") &&
         (mods & RACK_MOD_MASK) == 0;
}

bool isHardStopShortcut(int key, const std::string& keyName, int mods) {
  return isPeriodKey(key, keyName) &&
         (mods & RACK_MOD_MASK) == GLFW_MOD_CONTROL;
}

bool isRunToggleShortcut(int key, const std::string& keyName, int mods) {
  return isSlashKey(key, keyName) && (mods & RACK_MOD_MASK) == GLFW_MOD_CONTROL;
}

}  // namespace blunch
}  // namespace computerscare
