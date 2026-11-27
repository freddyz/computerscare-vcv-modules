// Included inside Beekapoo. Keyboard dispatch and the read-only hint overlay.
std::shared_ptr<std::function<void(const DrawArgs&)>> shortcutPaint;
std::map<int, WeakPtr<app::ParamWidget>> heldButtonKeys;
int buttonKeyBank = 0;

bool keyboardBlocked() const {
  if (!APP || !APP->scene || APP->scene->browser->visible) return true;
  for (auto* w = APP->event->getSelectedWidget(); w; w = w->parent)
    if (dynamic_cast<ui::TextField*>(w)) return true;
  for (auto* w : APP->scene->children)
    if (w->visible && dynamic_cast<ui::MenuOverlay*>(w)) return true;
  return false;
}
std::vector<ExposedSwitch*> keyboardButtons() {
  std::vector<ExposedSwitch*> buttons;
  if (!controls->visible) return buttons;
  for (auto& p : parameters)
    if (p.knob->visible)
      if (auto* b = dynamic_cast<ExposedSwitch*>(p.knob)) buttons.push_back(b);
  // Quantized rows avoid letting a slightly slanted face reorder a row.
  std::stable_sort(buttons.begin(), buttons.end(),
                   [](ExposedSwitch* a, ExposedSwitch* b) {
                     int rowA = int(std::round(a->box.getCenter().y / 12));
                     int rowB = int(std::round(b->box.getCenter().y / 12));
                     if (rowA != rowB) return rowA < rowB;
                     return a->box.getCenter().x < b->box.getCenter().x;
                   });
  return buttons;
}
void releaseButtonKey(int key) {
  auto it = heldButtonKeys.find(key);
  if (it == heldButtonKeys.end()) return;
  auto target = it->second;
  heldButtonKeys.erase(it);
  if (target) {
    widget::Widget::DragEndEvent end;
    end.button = GLFW_MOUSE_BUTTON_LEFT;
    target->onDragEnd(end);
  }
}
void releaseButtonKeys() {
  while (!heldButtonKeys.empty())
    releaseButtonKey(heldButtonKeys.begin()->first);
}
bool playButtonKey(int key) {
  int index = key == GLFW_KEY_0 ? 9 : key - GLFW_KEY_1;
  if (index < 0 || index >= 10) return false;
  auto buttons = keyboardButtons();
  int banks = std::max(1, (int(buttons.size()) + 9) / 10);
  buttonKeyBank = clamp(buttonKeyBank, 0, banks - 1);
  index += buttonKeyBank * 10;
  if (index >= int(buttons.size())) return true;
  if (heldButtonKeys.count(key)) return true;
  auto* b = buttons[index];
  WeakPtr<app::ParamWidget> target = b->source ? b->source.get() : b;
  if (frontLockMode) {
    toggleFrontLock(b->module->id, b->paramId);
    return true;
  }
  bool momentary = b->momentary;
  widget::Widget::DragStartEvent start;
  start.button = GLFW_MOUSE_BUTTON_LEFT;
  target->onDragStart(start);
  if (!target) return true;
  if (momentary)
    heldButtonKeys[key] = target;
  else {
    widget::Widget::DragEndEvent end;
    end.button = GLFW_MOUSE_BUTTON_LEFT;
    target->onDragEnd(end);
  }
  return true;
}
ActionButton* shortcutButton(KeyAction action) {
  switch (action) {
    case KeyAction::Clock:
      return sequenceButtons[0];
    case KeyAction::Next:
      return sequenceButtons[1];
    case KeyAction::Reset:
      return sequenceButtons[2];
    case KeyAction::New:
      return sequenceButtons[3];
    case KeyAction::Update:
      return sequenceButtons[4];
    case KeyAction::Recall:
      return sequenceButtons[5];
    case KeyAction::DeleteStep:
      return sequenceButtons[6];
    case KeyAction::WiggleAll:
      return actionButtons[3];
    case KeyAction::WiggleExposed:
      return actionButtons[0];
    case KeyAction::RandomExposed:
      return actionButtons[1];
    case KeyAction::RandomAll:
      return actionButtons[4];
    case KeyAction::Generate:
      return regenerateButton;
    case KeyAction::Edit:
      return managementButtons[0];
    case KeyAction::Eject:
      return managementButtons[1];
    case KeyAction::Save:
      return managementButtons[2];
    default:
      return nullptr;
  }
}
void configureKeyboardHints() {
  for (const auto& binding : keyBindings()) {
    auto* b = shortcutButton(binding.action);
    if (!b) continue;
    auto previous = b->help;
    std::string key = binding.badge;
    b->help = [previous, key] {
      return (previous ? previous() : std::string()) + " [" + key + "]";
    };
  }
}
bool handleKeyboard(const HoverKeyEvent& e) {
  if (e.action == GLFW_RELEASE && heldButtonKeys.count(e.key)) {
    releaseButtonKey(e.key);
    return true;
  }
  if (!module || keyboardBlocked()) return false;
  if (editSession) {
    editSession->onHoverKey(e);
    if (e.isConsumed()) return true;
  }
  int mods = e.mods & RACK_MOD_MASK;
  if (!mods && e.key == GLFW_KEY_ESCAPE && frontLockMode) {
    if (e.action == GLFW_PRESS) setFrontLockMode(false);
    return true;
  }
  if (e.key == GLFW_KEY_GRAVE_ACCENT && (mods == 0 || mods == GLFW_MOD_SHIFT))
    return true;
  if (!mods && e.key >= GLFW_KEY_0 && e.key <= GLFW_KEY_9) {
    if (e.action == GLFW_PRESS) playButtonKey(e.key);
    return true;
  }
  if (!mods &&
      (e.key == GLFW_KEY_LEFT_BRACKET || e.key == GLFW_KEY_RIGHT_BRACKET)) {
    if (e.action == GLFW_PRESS) {
      int count = std::max(1, (int(keyboardButtons().size()) + 9) / 10);
      buttonKeyBank =
          (buttonKeyBank + (e.key == GLFW_KEY_LEFT_BRACKET ? count - 1 : 1)) %
          count;
    }
    return true;
  }
  for (const auto& binding : keyBindings()) {
    if (e.key != binding.key || mods != binding.mods) continue;
    if (e.action != GLFW_PRESS) return true;
    if (binding.action == KeyAction::Lock) {
      setFrontLockMode(!frontLockMode);
    } else if (binding.action == KeyAction::Previous) {
      auto* m = getModule<Module>();
      int count = int(sequence.steps.size());
      if (count)
        selectSequenceStep((std::max(0, m->playingStep.load()) + count - 1) %
                           count);
    } else if (binding.action == KeyAction::Edit && editSession)
      editSession->onHoverKey(e);
    else if (auto* b = shortcutButton(binding.action)) {
      // A callback may remove the module (Eject); do not access members after
      // it.
      auto action = b->action;
      if (action) action();
    }
    return true;
  }
  return false;
}
void stepKeyboard() {
  if (module && parent && !shortcutPaint) {
    shortcutPaint = std::make_shared<std::function<void(const DrawArgs&)>>(
        [this](const DrawArgs& args) {
          if (args.fb || !module || !APP->window->win ||
              glfwGetKey(APP->window->win, GLFW_KEY_GRAVE_ACCENT) !=
                  GLFW_PRESS ||
              keyboardBlocked())
            return;
          bool hovered = false;
          for (auto* w = APP->event->getHoveredWidget(); w; w = w->parent)
            if (w == this) hovered = true;
          if (!hovered) return;
          nvgSave(args.vg);
          Vec origin = getRelativeOffset(Vec(), homeRack);
          nvgTranslate(args.vg, origin.x, origin.y);
          paintKeyboard(args, true);
          nvgRestore(args.vg);
        });
    auto* overlay = new ShortcutOverlay;
    overlay->paint = shortcutPaint;
    homeRack->addChild(overlay);
  }
  if (!heldButtonKeys.empty() && APP->window->win) {
    bool releaseAll = keyboardBlocked() ||
                      !glfwGetWindowAttrib(APP->window->win, GLFW_FOCUSED);
    std::vector<int> released;
    for (const auto& entry : heldButtonKeys)
      if (releaseAll || glfwGetKey(APP->window->win, entry.first) != GLFW_PRESS)
        released.push_back(entry.first);
    for (int key : released) releaseButtonKey(key);
  }
}
void paintKeyboard(const DrawArgs& args, bool fitViewport) {
  auto font = APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
  if (!font) return;
  auto* vg = args.vg;
  nvgSave(vg);
  nvgFontFaceId(vg, font->handle);
  auto plaque = [&](Rect r, bool accent) {
    nvgBeginPath(vg);
    nvgMoveTo(vg, r.pos.x, r.pos.y + 1);
    nvgLineTo(vg, r.getRight() - 1, r.pos.y);
    nvgLineTo(vg, r.getRight(), r.getBottom() - 1);
    nvgLineTo(vg, r.pos.x + 1, r.getBottom());
    nvgClosePath(vg);
    nvgFillColor(
        vg, accent ? nvgRGBA(236, 223, 139, 245) : nvgRGBA(31, 34, 34, 245));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGBA(6, 8, 8, 235));
    nvgStrokeWidth(vg, .7f);
    nvgStroke(vg);
  };
  auto badge = [&](widget::Widget* control, const std::string& key) {
    if (!control || !control->visible) return;
    nvgFontSize(vg, 7.5f);
    float width = nvgTextBounds(vg, 0, 0, key.c_str(), nullptr, nullptr) + 4;
    Vec corner = control->getRelativeOffset(Vec(control->box.size.x, 0), this);
    Rect rect(corner + Vec(1 - width, -7), Vec(width, 10));
    Vec center = rect.getCenter();
    plaque(rect, true);
    nvgFillColor(vg, nvgRGB(24, 26, 25));
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(vg, center.x, center.y, key.c_str(), nullptr);
  };
  for (const auto& binding : keyBindings()) {
    badge(shortcutButton(binding.action), binding.badge);
  }
  auto buttons = keyboardButtons();
  int banks = std::max(1, (int(buttons.size()) + 9) / 10);
  int bank = clamp(buttonKeyBank, 0, banks - 1);
  for (int i = 0; i < 10 && bank * 10 + i < int(buttons.size()); ++i)
    badge(buttons[bank * 10 + i], std::to_string((i + 1) % 10));
  float width = 218, height = 100 + keyBindings().size() * 11.5f;
  Vec guide(box.size.x + 9, 5);
  if (fitViewport) {
    float zoom = std::max(.1f, getAbsoluteZoom());
    Vec origin = getAbsoluteOffset(Vec());
    Vec available = APP->scene->box.size / zoom, offset = origin / zoom;
    if (offset.x + guide.x + width > available.x - 4) guide.x = -width - 9;
    guide.x = clamp(guide.x, 4 - offset.x,
                    std::max(4 - offset.x, available.x - width - offset.x - 4));
    guide.y =
        clamp(guide.y, 4 - offset.y,
              std::max(4 - offset.y, available.y - height - offset.y - 4));
  }
  plaque(Rect(guide, Vec(width, height)), false);
  nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
  nvgFontSize(vg, 12);
  nvgFillColor(vg, nvgRGB(236, 223, 139));
  nvgText(vg, guide.x + 10, guide.y + 9, "BEEKAPOO KEYS", nullptr);
  nvgFontSize(vg, 8.5f);
  nvgFillColor(vg, nvgRGB(205, 210, 209));
  nvgText(vg, guide.x + 10, guide.y + 27,
          "Hold ~   ·   shortcuts act on this Beek", nullptr);
  float y = guide.y + 45;
  for (const auto& binding : keyBindings()) {
    nvgFillColor(vg, nvgRGB(236, 223, 139));
    nvgText(vg, guide.x + 10, y, binding.badge, nullptr);
    nvgFillColor(vg, nvgRGB(235, 238, 237));
    nvgText(vg, guide.x + 53, y, binding.help, nullptr);
    y += 11.5f;
  }
  y += 7;
  std::string bankText = "1–9, 0  Exposed buttons  ·  bank " +
                         std::to_string(bank + 1) + "/" + std::to_string(banks);
  nvgText(vg, guide.x + 10, y, bankText.c_str(), nullptr);
  nvgText(vg, guide.x + 10, y + 13, "[ / ]  Change bank   ·   ⇧ Shift   ⌥ Alt",
          nullptr);
  nvgText(vg, guide.x + 10, y + 28, "Edit: E / X  Expose   ·   Esc  Done",
          nullptr);
  nvgRestore(vg);
}
