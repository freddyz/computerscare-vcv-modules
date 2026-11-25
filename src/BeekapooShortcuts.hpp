#pragma once
namespace beekapoo {
enum class KeyAction {
  Clock,
  Next,
  Previous,
  Reset,
  Steps,
  Store,
  New,
  WiggleAll,
  WiggleExposed,
  RandomExposed,
  RandomAll,
  Generate,
  Edit,
  Eject,
  Save,
  Add,
  Functions,
  Grow,
  Compact,
  ExposeMore,
  ExposeLess,
  Reroll
};
struct KeyBinding {
  int key, mods;
  const char *badge, *help;
  KeyAction action;
};
inline const std::vector<KeyBinding>& keyBindings() {
  static const std::vector<KeyBinding> bindings{
      {GLFW_KEY_SPACE, 0, "SPC", "Clock on/off", KeyAction::Clock},
      {GLFW_KEY_RIGHT, 0, "→", "Next step", KeyAction::Next},
      {GLFW_KEY_LEFT, 0, "←", "Previous step", KeyAction::Previous},
      {GLFW_KEY_HOME, 0, "↖", "Reset to step 1 (Home)", KeyAction::Reset},
      {GLFW_KEY_ENTER, 0, "Enter", "Choose step", KeyAction::Steps},
      {GLFW_KEY_S, 0, "S", "Store step", KeyAction::Store},
      {GLFW_KEY_N, 0, "N", "Save as New", KeyAction::New},
      {GLFW_KEY_Q, 0, "Q", "Wiggle all", KeyAction::WiggleAll},
      {GLFW_KEY_W, 0, "W", "Wiggle exposed", KeyAction::WiggleExposed},
      {GLFW_KEY_R, 0, "R", "Randomize exposed", KeyAction::RandomExposed},
      {GLFW_KEY_T, 0, "T", "Randomize all", KeyAction::RandomAll},
      {GLFW_KEY_N, GLFW_MOD_SHIFT, "⇧N", "Generate new subpatch",
       KeyAction::Generate},
      {GLFW_KEY_E, 0, "E", "Edit subpatch", KeyAction::Edit},
      {GLFW_KEY_E, GLFW_MOD_SHIFT, "⇧E", "Eject", KeyAction::Eject},
      {GLFW_KEY_S, GLFW_MOD_SHIFT, "⇧S", "Save Beek preset", KeyAction::Save},
      {GLFW_KEY_A, 0, "A", "Chomp selected modules", KeyAction::Add},
      {GLFW_KEY_F, 0, "F", "Functions menu", KeyAction::Functions},
      {GLFW_KEY_G, 0, "G", "Grow", KeyAction::Grow},
      {GLFW_KEY_G, GLFW_MOD_SHIFT, "⇧G", "Compact subpatch",
       KeyAction::Compact},
      {GLFW_KEY_EQUAL, 0, "+", "Expose another", KeyAction::ExposeMore},
      {GLFW_KEY_MINUS, 0, "−", "Remove one exposed", KeyAction::ExposeLess},
      {GLFW_KEY_R, GLFW_MOD_ALT, "⌥R", "Re-roll hovered section",
       KeyAction::Reroll}};
  return bindings;
}
// Layer 3 follows Rack's cables. Lifetime is owned by the module's callback.
struct ShortcutOverlay : widget::TransparentWidget {
  std::weak_ptr<std::function<void(const DrawArgs&)>> paint;
  void step() override {
    if (paint.expired()) requestDelete();
    widget::TransparentWidget::step();
  }
  void drawLayer(const DrawArgs& args, int layer) override {
    if (layer == 3)
      if (auto draw = paint.lock()) (*draw)(args);
  }
};
}  // namespace beekapoo
