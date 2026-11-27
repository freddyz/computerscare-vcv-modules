// Included inside Beekapoo. These are presentation blocks only: original
// controls, module IDs, parameter quantities and cable endpoints stay intact.
std::vector<PackedPanelBlock*> memberFrames;
std::array<std::vector<Rect>, 3> groupedSections;
Rect hoveredGroupRegion;
struct PanelModuleGroup {
  int64_t id;
  std::vector<widget::Widget*> params, inputs, outputs;
};
std::vector<PanelModuleGroup> panelModuleGroups(
    const std::vector<widget::Widget*>& knobs,
    const std::vector<widget::Widget*>& inputs,
    const std::vector<widget::Widget*>& outputs) {
  std::vector<PanelModuleGroup> groups;
  auto get = [&](int64_t id) -> PanelModuleGroup& {
    for (auto& g : groups)
      if (g.id == id) return g;
    groups.push_back({id, {}, {}, {}});
    return groups.back();
  };
  // The existing ordered parameter/port lists define the module order.
  for (auto* item : knobs) {
    auto* p = static_cast<app::ParamWidget*>(item);
    if (p->module) get(p->module->id).params.push_back(item);
  }
  for (int kind = 0; kind < 2; ++kind)
    for (auto* item : kind ? outputs : inputs)
      for (const auto& p : ports)
        if (p.face == item) {
          auto& g = get(p.id);
          (kind ? g.outputs : g.inputs).push_back(item);
          break;
        }
  return groups;
}
void layoutGroupedControls(
    const std::vector<widget::Widget*>& knobs,
    const std::vector<widget::Widget*>& inputs,
    const std::vector<widget::Widget*>& outputs,
    const std::map<widget::Widget*, std::pair<std::string, std::string>>&
        names) {
  auto groups = panelModuleGroups(
      knobs, groupPortsByModule ? inputs : std::vector<widget::Widget*>{},
      groupPortsByModule ? outputs : std::vector<widget::Widget*>{});
  std::vector<ModuleGroupCount> counts;
  for (const auto& g : groups) {
    ModuleGroupCount c;
    c.params = g.params.size();
    c.inputs = g.inputs.size();
    c.outputs = g.outputs.size();
    counts.push_back(c);
  }
  float minimum = 45, groupTop = 0, groupBottom = 0;
  CompactPanelLayout portPlan;
  auto fits = [&](float width) {
    float ownBottom =
        headerLayout(width, module ? members.size() : browserMemberCount)
            .height +
        4 + tinyOwnHeight(width);
    groupTop = ownBottom + 8;
    groupBottom = 374;
    if (!groupPortsByModule) {
      portPlan = compactPanelLayout(width, 368 - ownBottom, knobs.size(),
                                    inputs.size(), outputs.size());
      groupBottom = 368 - portPlan.portHeight - 8;
      if (!portPlan.fits) return false;
    }
    return !moduleGroupLayout(counts, width - 20, groupBottom - groupTop - 8)
                .empty();
  };
  while (minimum < 30000 && !fits(minimum)) minimum += 15;
  float width = std::max(minimum, std::ceil(requestedWidth / 15) * 15);
  while (width < 30000 && !fits(width)) width += 15;
  bool changed = box.size.x != width;
  box.size.x = width;
  lastLayoutWidth = width;
  layoutTier = width < 210 ? 0 : 2;
  jacksOnly = width < 210;
  if (auto* m = getModule<Module>()) m->panelWidth.store(requestedWidth);
  leftHandle->minWidth = rightHandle->minWidth = minimum;
  rightHandle->box.pos.x = width - rightHandle->box.size.x;
  controlLabels.clear();
  headings.clear();
  responsive = ResponsiveLayout();
  responsive.width = width;
  float ownTop = layoutHeaderControls(width) + 4;
  layoutOwnControls(width, ownTop);
  frame->box = Rect(Vec(6, groupTop), Vec(width - 12, groupBottom - groupTop));
  frame->visible = controls->visible;
  inputFrame->visible = outputFrame->visible = false;
  if (!groupPortsByModule) {
    float y = 368 - portPlan.portHeight;
    layoutPortBlock(
        inputs, inputFrame,
        Rect(Vec(6, y), Vec(portPlan.inputWidth, portPlan.inputHeight)));
    if (!portPlan.beside && !inputs.empty()) y += portPlan.inputHeight + 8;
    layoutPortBlock(
        outputs, outputFrame,
        Rect(Vec(portPlan.beside ? width - 6 - portPlan.outputWidth : 6, y),
             Vec(portPlan.outputWidth, portPlan.outputHeight)));
  }
  finishFrames();
  // Keep the parent section's geometry for the existing section actions,
  // but draw only the individual module plaques over its face.
  Rect body = frameContent(frame);
  auto cells = moduleGroupLayout(counts, body.size.x, body.size.y);
  if (!groupPortsByModule) {
    layoutPortControls(inputs, inputFrame, portPlan.inputColumns, false, names);
    layoutPortControls(outputs, outputFrame, portPlan.outputColumns, true,
                       names);
  }
  while (memberFrames.size() < groups.size()) {
    auto* block = new PackedPanelBlock;
    block->configure(Rect(), 2, 1, 811, nvgRGB(204, 204, 204));
    addChildBelow(block, controls);
    memberFrames.push_back(block);
  }
  for (size_t i = 0; i < groups.size() && i < cells.size(); ++i) {
    auto& g = groups[i];
    auto c = cells[i];
    auto* block = memberFrames[i];
    block->box = Rect(body.pos + Vec(c.x, c.y), Vec(c.width, c.height));
    block->visible = controls->visible;
    block->skin = sectionSkin(appearance, unsigned(i) + 20);
    block->shapeSeed = unsigned(block->skin.seed);
    block->screwup = .8f;
    block->depth = 1.4f;
    block->faceColor = frame->faceColor;
    Rect inner = frameContent(block);
    float titleHeight = std::min(12.f, inner.size.y * .13f);
    controlLabels.push_back({Vec(inner.getCenter().x, inner.pos.y),
                             inner.size.x, memberName(g.id), false,
                             NVG_ALIGN_CENTER, 10, titleHeight, true});
    inner.pos.y += titleHeight + 2;
    inner.size.y -= titleHeight + 2;
    int portCount = g.inputs.size() + g.outputs.size();
    int cols = std::max(1, int(inner.size.x / 20));
    int rows = (portCount + cols - 1) / cols;
    float portHeight =
        rows ? std::min(inner.size.y - (g.params.empty() ? 0 : 26), rows * 32.f)
             : 0;
    portHeight = std::max(rows * 16.f, portHeight);
    Rect paramArea(inner.pos, Vec(inner.size.x, inner.size.y - portHeight));
    if (!g.params.empty()) {
      layoutMappedParameters(g.params, names, paramArea.pos.y,
                             paramArea.getBottom(), &paramArea);
      groupedSections[0].push_back(paramArea);
    }
    // Inputs precede outputs in each block; captions retain their direction.
    std::vector<widget::Widget*> jacks = g.inputs;
    jacks.insert(jacks.end(), g.outputs.begin(), g.outputs.end());
    for (int j = 0; j < portCount; ++j) {
      int row = j / cols, col = j % cols;
      int inRow = std::min(cols, portCount - row * cols);
      float cellW = inner.size.x / inRow, cellH = portHeight / rows;
      Rect cell(
          inner.pos + Vec(col * cellW, inner.size.y - portHeight + row * cellH),
          Vec(cellW, cellH));
      auto* face = static_cast<PortFace*>(jacks[j]);
      auto fit = planPortCell(cellW, cellH, false);
      face->compact = !fit.large;
      face->box = Rect(cell.pos + Vec(fit.jack.x, fit.jack.y),
                       Vec(fit.jack.w, fit.jack.h));
      face->visible = controls->visible;
      bool output = j >= int(g.inputs.size());
      groupedSections[output ? 2 : 1].push_back(cell);
      auto label = names.find(face);
      if (label != names.end() && fit.label.h > 0)
        controlLabels.push_back(
            {cell.pos + Vec(fit.label.x + fit.label.w * .5f, fit.label.y),
             fit.label.w, (output ? "Out " : "In ") + label->second.first,
             false, NVG_ALIGN_CENTER, fit.fontSize, fit.label.h, true});
    }
  }
  if (changed && !ejecting && parent == homeRack->getModuleContainer())
    homeRack->setModulePosForce(this, box.pos);
}
