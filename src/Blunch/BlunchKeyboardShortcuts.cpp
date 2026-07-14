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

bool isLeftBracketKey(int key, const std::string& keyName) {
  return key == GLFW_KEY_LEFT_BRACKET || keyName == "[";
}

bool isRightBracketKey(int key, const std::string& keyName) {
  return key == GLFW_KEY_RIGHT_BRACKET || keyName == "]";
}

}  // namespace

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
      rackMods == GLFW_MOD_CONTROL) {
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

bool isHardStopShortcut(int key, const std::string& keyName, int mods) {
  return isPeriodKey(key, keyName) &&
         (mods & RACK_MOD_MASK) == GLFW_MOD_CONTROL;
}

}  // namespace blunch
}  // namespace computerscare
