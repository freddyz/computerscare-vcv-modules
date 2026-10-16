#include <cstdlib>
#include <iostream>
#include <string>

#include "Blunch/BlunchKeyboardShortcuts.hpp"
#include "Computerscare.hpp"

NVGcolor nvgRGBA(unsigned char r, unsigned char g, unsigned char b,
                 unsigned char a) {
  NVGcolor color;
  color.r = r / 255.f;
  color.g = g / 255.f;
  color.b = b / 255.f;
  color.a = a / 255.f;
  return color;
}

NVGcolor nvgRGB(unsigned char r, unsigned char g, unsigned char b) {
  return nvgRGBA(r, g, b, 255);
}

namespace {

void require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << std::endl;
    std::exit(1);
  }
}

void testBracketChannelNavigationDirection() {
  computerscare::blunch::KeyboardShortcutOptions options;
  ComputerscareTextEditorCommands commands;

  bool handled = computerscare::blunch::handleKeyboardShortcut(
      GLFW_KEY_LEFT_BRACKET, "[", GLFW_MOD_CONTROL, GLFW_PRESS, options,
      commands);

  require(handled, "left bracket shortcut should be handled");
  require(commands.navigateChannelBackwardCount == 1,
          "left bracket should navigate backward");
  require(commands.navigateChannelForwardCount == 0,
          "left bracket should not navigate forward");

  commands = ComputerscareTextEditorCommands();
  handled = computerscare::blunch::handleKeyboardShortcut(
      GLFW_KEY_RIGHT_BRACKET, "]", GLFW_MOD_CONTROL, GLFW_PRESS, options,
      commands);

  require(handled, "right bracket shortcut should be handled");
  require(commands.navigateChannelForwardCount == 1,
          "right bracket should navigate forward");
  require(commands.navigateChannelBackwardCount == 0,
          "right bracket should not navigate backward");
}

void testSubmitEnterModifiers() {
  computerscare::blunch::KeyboardShortcutOptions options;
  options.submitOnEnter = true;
  ComputerscareTextEditorCommands commands;

  bool handled = computerscare::blunch::handleKeyboardShortcut(
      GLFW_KEY_ENTER, "Enter", GLFW_MOD_CONTROL, GLFW_PRESS, options,
      commands);
  require(handled, "ctrl enter submit should be handled");
  require(commands.submitCount == 1, "ctrl enter should submit");

  commands = ComputerscareTextEditorCommands();
  handled = computerscare::blunch::handleKeyboardShortcut(
      GLFW_KEY_ENTER, "Enter", GLFW_MOD_SUPER, GLFW_PRESS, options, commands);
  require(handled, "cmd enter submit should be handled");
  require(commands.submitCount == 1, "cmd enter should submit");
}

void testRunToggleShortcutDoesNotStartAll() {
  computerscare::blunch::KeyboardShortcutOptions options;
  ComputerscareTextEditorCommands commands;

  bool handled = computerscare::blunch::handleKeyboardShortcut(
      GLFW_KEY_SLASH, "/", GLFW_MOD_CONTROL, GLFW_PRESS, options, commands);

  require(handled, "ctrl slash run toggle should be handled");
  require(commands.runToggleCount == 1, "ctrl slash should toggle run");
  require(commands.startAllCount == 0, "ctrl slash should not start all");
}

}  // namespace

int main() {
  using namespace computerscare::blunch;
  require(isShortcutHelpKey(GLFW_KEY_GRAVE_ACCENT, "`", 0), "backtick opens shortcut help");
  require(!isShortcutHelpKey(GLFW_KEY_GRAVE_ACCENT, "~", GLFW_MOD_SHIFT), "shifted backtick remains available for the rest operator");
  require(isShortcutHelpKey(GLFW_KEY_UNKNOWN, "`", 0), "localized key name recognizes backtick");
  require(!isShortcutHelpKey(GLFW_KEY_GRAVE_ACCENT, "`", GLFW_MOD_CONTROL), "modified OS shortcuts remain available");
  require(!isShortcutHelpKey(GLFW_KEY_A, "a", 0), "ordinary typing does not invoke help");
  require(isShortcutHelpCharacter('`') && !isShortcutHelpCharacter('~'), "backtick is reserved while tilde remains available for rests");
  require(!isShortcutHelpCharacter('a'), "ordinary editor characters are retained");
  ControlSelection selection;
  require(selection.targets()==1u,"control selection starts nonempty");
  auto key=[&](int key,int mods=0,int action=GLFW_PRESS) {
    return handleControlShortcut(key,mods,action,selection);
  };
  key(GLFW_KEY_DOWN);
  require(selection.focus==1&&selection.targets()==2u,"singleton selection follows cursor");
  key(GLFW_KEY_DOWN,GLFW_MOD_SHIFT); key(GLFW_KEY_DOWN,GLFW_MOD_SHIFT);
  require(selection.targets()==14u,"shift arrows select channels two through four");
  key(GLFW_KEY_UP,GLFW_MOD_SHIFT);
  require(selection.targets()==6u,"shift reversing shrinks a range");
  key(GLFW_KEY_DOWN,GLFW_MOD_SHIFT);
  for(int i=0;i<11;++i) key(GLFW_KEY_DOWN);
  require(selection.focus==14&&selection.targets()==14u,"moving focus preserves a selected range");
  key(GLFW_KEY_ENTER,GLFW_MOD_CONTROL);
  require(selection.targets()==(14u|(1u<<14)),"modifier Enter adds a distant channel");
  key(GLFW_KEY_ENTER,GLFW_MOD_SUPER);
  require(selection.targets()==14u,"platform modifier Enter removes a selected channel");
  auto preserved=selection.targets();
  require(key(GLFW_KEY_ESCAPE)==ControlCommand::Unhandled&&selection.targets()==preserved,"Escape retains selection and uses normal view navigation");
  require(key(GLFW_KEY_ENTER)==ControlCommand::Unhandled,"plain Enter retains normal open-channel behavior");
  require(key(GLFW_KEY_TAB)==ControlCommand::Unhandled,"Tab retains normal view switching");
  require(key(GLFW_KEY_SPACE)==ControlCommand::TogglePlayback,"space toggles selected channels");
  require(key(GLFW_KEY_SPACE,0,GLFW_REPEAT)==ControlCommand::None,"held space does not retrigger toggles");
  require(key(GLFW_KEY_R)==ControlCommand::Reset,"R resets selected channels");
  selection.selectOnly(4); selection.toggleFocused();
  require(selection.targets()==(1u<<4),"cannot remove the final selected channel");
  selection.selectOnly(9);
  require(selection.targets()==(1u<<9)&&selection.focus==9,"plain click replaces selection");
  selection.focus=3; selection.toggleFocused();
  require(selection.targets()==((1u<<9)|(1u<<3)),"modifier click adds a row");
  selection.toggleFocused();
  require(selection.targets()==(1u<<9),"modifier click removes a row");
  testBracketChannelNavigationDirection();
  testSubmitEnterModifiers();
  testRunToggleShortcutDoesNotStartAll();
  return 0;
}
