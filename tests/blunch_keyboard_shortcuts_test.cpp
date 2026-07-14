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

}  // namespace

int main() {
  testBracketChannelNavigationDirection();
  return 0;
}
