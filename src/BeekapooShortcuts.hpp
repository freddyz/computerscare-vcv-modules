#pragma once
namespace beekapoo {
enum class KeyAction {
  Clock,
  Next,
  Previous,
  Reset,
  Update,
  Recall,
  DeleteStep,
  New,
  WiggleAll,
  WiggleExposed,
  RandomExposed,
  RandomAll,
  Generate,
  Edit,
  Eject,
  Save
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
      {GLFW_KEY_D, 0, "D", "Delete current step", KeyAction::DeleteStep},
      {GLFW_KEY_N, 0, "N", "Capture New", KeyAction::New},
      {GLFW_KEY_U, 0, "U", "Update current step", KeyAction::Update},
      {GLFW_KEY_BACKSPACE, 0, "⌫", "Recall saved step", KeyAction::Recall},
      {GLFW_KEY_Q, 0, "Q", "Wiggle all", KeyAction::WiggleAll},
      {GLFW_KEY_W, 0, "W", "Wiggle exposed", KeyAction::WiggleExposed},
      {GLFW_KEY_R, 0, "R", "Randomize exposed", KeyAction::RandomExposed},
      {GLFW_KEY_T, 0, "T", "Randomize all", KeyAction::RandomAll},
      {GLFW_KEY_N, GLFW_MOD_SHIFT, "⇧N", "Generate new subpatch",
       KeyAction::Generate},
      {GLFW_KEY_E, 0, "E", "Edit subpatch", KeyAction::Edit},
      {GLFW_KEY_E, GLFW_MOD_SHIFT, "⇧E", "Eject", KeyAction::Eject},
      {GLFW_KEY_S, GLFW_MOD_SHIFT, "⇧S", "Save Beek preset", KeyAction::Save},
  };
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
