#include "BlunchKeyboardShortcuts.hpp"

#include "../Computerscare.hpp"

namespace computerscare {
namespace blunch {

namespace {

bool isEnterKey(int key) {
  return key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER;
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

bool isShortcutHelpCharacter(uint32_t codepoint) { return codepoint == '`'; }

bool isShortcutHelpKey(int key, const std::string& keyName, int mods) {
  return (key == GLFW_KEY_GRAVE_ACCENT || keyName == "`") &&
         (mods & RACK_MOD_MASK) == 0;
}

}  // namespace blunch
}  // namespace computerscare
