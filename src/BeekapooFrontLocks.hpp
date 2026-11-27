// Included inside Beekapoo. Front-panel locking uses the same saved lock set
// as the subpatch editor; changing a lock never writes a parameter value.
bool frontLockMode = false;
std::pair<int64_t, int> frontLockHover{-1, -1};
void setFrontLockMode(bool enabled) {
  releaseButtonKeys();
  frontLockMode = enabled;
  frontLockHover = {-1, -1};
}
Parameter* frontLockParameter(Vec position) {
  if (!frontLockMode || !controls->visible) return nullptr;
  for (auto& p : parameters)
    if (p.knob->visible && p.knob->box.contains(position)) return &p;
  return nullptr;
}
void toggleFrontLock(int64_t id, int param) {
  auto* w = find(id);
  if (!w || !w->module) return;
  auto* action = beginAction("toggle subpatch lock");
  if (randomLocks.erase({id, -1})) {
    // Unlocking one control of a locked module keeps every other control
    // protected, including controls not exposed on this panel.
    for (int i = 0; i < int(w->module->params.size()); ++i)
      if (i != param) randomLocks.insert({id, i});
    randomLocks.erase({id, param});
  } else {
    auto key = std::make_pair(id, param);
    if (!randomLocks.erase(key)) randomLocks.insert(key);
  }
  saveGroup();
  finishAction(action);
}
void paintFrontLocks(const DrawArgs& args) {
  if (!frontLockMode || !controls->visible) return;
  auto* vg = args.vg;
  nvgSave(vg);
  for (const auto& p : parameters) {
    if (!p.knob->visible) continue;
    bool locked = randomLocked(p.id, p.paramId);
    bool hovered = frontLockHover == std::make_pair(p.id, p.paramId);
    Rect r = p.knob->box;
    uint32_t seed = uint32_t(p.id) ^ uint32_t(p.paramId + 1) * 2654435761u;
    auto jitter = [&]() {
      seed ^= seed << 13;
      seed ^= seed >> 17;
      seed ^= seed << 5;
      return float(seed % 1024) / 1023.f * 2 - 1;
    };
    nvgBeginPath(vg);
    nvgMoveTo(vg, r.pos.x - 2 + jitter(), r.pos.y - 2 + jitter());
    nvgLineTo(vg, r.getRight() + 2 + jitter(), r.pos.y - 2 + jitter());
    nvgLineTo(vg, r.getRight() + 2 + jitter(), r.getBottom() + 2 + jitter());
    nvgLineTo(vg, r.pos.x - 2 + jitter(), r.getBottom() + 2 + jitter());
    nvgClosePath(vg);
    NVGcolor color = hovered ? nvgRGB(247, 214, 65) : nvgRGB(165, 177, 170);
    nvgFillColor(vg, nvgTransRGBAf(color, locked ? .42f : .08f));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgTransRGBAf(color, locked || hovered ? .95f : .45f));
    nvgStrokeWidth(vg, locked || hovered ? 1.4f : .7f);
    nvgStroke(vg);
    if (locked) drawRandomLock(vg, Vec(r.getRight() - 3, r.pos.y + 3), 10);
  }
  nvgRestore(vg);
}
