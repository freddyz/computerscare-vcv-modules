#pragma once
// Run only when creating a group from a selection, never during restore or
// layout. These are normal editable mappings, saved in the same transaction.
void Beekapoo::exposeSelectionDefaults() {
  if (!module || members.empty()) return;
  std::mt19937 rng(random::u32());
  auto targets = defaultExposureCounts(int(members.size()), rng);
  struct Candidates {
    int64_t id;
    std::vector<app::PortWidget*> inputs, outputs;
    std::vector<int> parameters;
  };
  std::vector<Candidates> candidates;
  int inputCount = 0, outputCount = 0;
  for (const auto& port : ports) (port.input ? inputCount : outputCount)++;
  for (const auto& member : members) {
    auto* w = find(member.id);
    if (!w || !w->module) continue;
    Candidates c;
    c.id = member.id;
    for (auto* input : w->getInputs())
      if (input->visible && !exposed(input) &&
          homeRack->getCompleteCablesOnPort(input).empty())
        c.inputs.push_back(input);
    for (auto* output : w->getOutputs())
      if (output->visible && !exposed(output)) c.outputs.push_back(output);
    for (auto* param : w->getParams()) {
      auto* q = param->getParamQuantity();
      auto* button = dynamic_cast<app::Switch*>(param);
      if (param->visible && q && q->isBounded() && q->maxValue > q->minValue &&
          !(button && button->momentary) && !exposed(param))
        c.parameters.push_back(param->paramId);
    }
    std::shuffle(c.inputs.begin(), c.inputs.end(), rng);
    std::shuffle(c.outputs.begin(), c.outputs.end(), rng);
    std::shuffle(c.parameters.begin(), c.parameters.end(), rng);
    candidates.push_back(std::move(c));
  }
  std::shuffle(candidates.begin(), candidates.end(), rng);
  // Take one of each kind per module per pass. A module with many controls
  // should not crowd out the smaller modules in the selection.
  bool added;
  do {
    added = false;
    for (auto& c : candidates) {
      auto* w = find(c.id);
      if (inputCount < targets.inputs && !c.inputs.empty()) {
        expose(w, c.inputs.back());
        c.inputs.pop_back();
        ++inputCount;
        added = true;
      }
      if (outputCount < targets.outputs && !c.outputs.empty()) {
        expose(w, c.outputs.back());
        c.outputs.pop_back();
        ++outputCount;
        added = true;
      }
      if (int(parameters.size()) < targets.parameters &&
          !c.parameters.empty()) {
        exposeParameter(c.id, c.parameters.back());
        c.parameters.pop_back();
        added = true;
      }
    }
  } while (added);
  layoutControls();
  saveGroup();
}
