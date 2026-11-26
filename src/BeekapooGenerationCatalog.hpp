#pragma once

// Adapt installed Rack models to the standalone planner. Detailed rules win;
// other models use conservative port-name/tag guesses, cached on the UI thread.
namespace generated {
inline bool hasTag(plugin::Model* model, const char* name) {
  int id = tag::findId(name);
  return std::find(model->tagIds.begin(), model->tagIds.end(), id) !=
         model->tagIds.end();
}
inline std::string unavailableReason(plugin::Model* model) {
  if (!model) return "Not installed";
  if (model->hidden) return "Hidden module";
  if (model->plugin == pluginInstance &&
      (model->slug.find("beekapoo") != std::string::npos ||
       model->slug.find("peekaboo") != std::string::npos ||
       model->slug.find("patch-abstractor") != std::string::npos))
    return "Subpatch containers cannot generate themselves";
  if (hasTag(model, "MIDI") || hasTag(model, "External") ||
      hasTag(model, "Expander"))
    return "Needs an external device or adjacent module";
  if (hasTag(model, "Blank")) return "No signal outputs";
  return "";
}
inline bool knownRules(plugin::Model* model) {
  if (model && model->plugin->slug == "Fundamental")
    for (const auto& spec : beekapoo_patch::library())
      if (spec.slug == model->slug) return true;
  return model && beekapoo_patch::portLibrary().count(
                      {model->plugin->slug, model->slug});
}
inline bool containsAny(const std::string& text,
                        std::initializer_list<const char*> words) {
  for (auto* word : words)
    if (text.find(word) != std::string::npos) return true;
  return false;
}
inline const beekapoo_patch::Spec& specFor(plugin::Model* model,
                                           engine::Module* existing = nullptr) {
  using namespace beekapoo_patch;
  if (model->plugin->slug == "Fundamental")
    for (const auto& spec : library())
      if (spec.slug == model->slug) return spec;
  static std::map<ModuleKey, Spec> cache;
  ModuleKey key{model->plugin->slug, model->slug};
  auto found = cache.find(key);
  if (found != cache.end()) return found->second;
  Spec spec;
  spec.slug = model->slug;
  std::unique_ptr<engine::Module> temporary;
  if (!existing) {
    temporary.reset(model->createModule());
    existing = temporary.get();
  }
  auto* m = existing;
  bool source = m->inputs.empty() || hasTag(model, "Oscillator") ||
                hasTag(model, "Noise") ||
                hasTag(model, "Low-frequency oscillator") ||
                hasTag(model, "Random") || hasTag(model, "Clock generator") ||
                hasTag(model, "Synth voice");
  bool envelope = hasTag(model, "Envelope generator");
  int signalInput = -1, gateInput = -1;
  for (int i = 0; i < int(m->inputs.size()); ++i) {
    auto* info = m->getInputInfo(i);
    auto name = info ? string::lowercase(info->name) : "";
    if (gateInput < 0 &&
        containsAny(name, {"gate", "trigger", "trig", "clock"}))
      gateInput = i;
    if (signalInput < 0 &&
        (name == "in" || name == "input" || name == "left" || name == "l" ||
         containsAny(name, {"audio", "signal", "input 1", "channel 1"})))
      signalInput = i;
  }
  std::vector<Requirement> requirements;
  if (envelope && gateInput >= 0)
    requirements.push_back({{gateInput}});
  else if (!source && !m->inputs.empty()) {
    if (signalInput < 0) signalInput = 0;
    requirements.push_back({{signalInput}});
  }
  for (int i = 0; i < int(m->outputs.size()); ++i) {
    auto* info = m->getOutputInfo(i);
    auto name = info ? string::lowercase(info->name) : "";
    int priority =
        name == "mix" || name == "sum" || name == "out" || name == "output" ? 1
                                                                            : 0;
    spec.outputs.push_back({i, requirements, priority});
  }
  for (int i = 0; i < int(m->inputs.size()); ++i) {
    bool required = false;
    for (const auto& r : requirements)
      required |= std::find(r.anyOf.begin(), r.anyOf.end(), i) != r.anyOf.end();
    if (!required) spec.modulationInputs.push_back({i, requirements, {}});
  }
  for (int i = 0; i < int(m->params.size()); ++i)
    if (auto* q = m->getParamQuantity(i)) {
      auto name = string::lowercase(q->name);
      if (q->isBounded() && q->getMaxValue() > q->getMinValue() &&
          !containsAny(
              name, {"trigger", "reset", "randomize", "initialize", "clear"}))
        spec.parameters.push_back({i, requirements, {}});
    }
  spec.inputRules = spec.modulationInputs;
  // The inferred main input itself has no prerequisites.
  for (const auto& r : requirements)
    for (int id : r.anyOf) spec.inputRules.push_back({id});
  auto rules = portLibrary().find({model->plugin->slug, model->slug});
  if (rules != portLibrary().end()) {
    spec.outputs = rules->second.outputs;
    spec.modulationInputs = rules->second.modulationInputs;
    spec.inputRules = rules->second.inputRules;
  }
  // Keep unfamiliar modules at their own defaults. Guessing arbitrary ranges
  // here can mute a voice or turn a switch into an unrelated operating mode.
  return cache.emplace(std::move(key), std::move(spec)).first->second;
}
struct Catalog {
  std::vector<beekapoo_patch::Spec> specs;
  std::vector<plugin::Model*> models;
  std::string error;
  explicit Catalog(const std::string& id) {
    auto* set = moduleSets().find(id);
    if (!set) {
      error = "Module set is missing; choose one in Edit > Module sets";
      return;
    }
    for (const auto& key : set->modules) {
      auto* model = plugin::getModel(key.plugin, key.model);
      if (!unavailableReason(model).empty()) continue;
      const auto& spec = specFor(model);
      if (spec.outputs.empty()) continue;
      specs.push_back(spec);
      models.push_back(model);
    }
    if (models.empty()) error = "No available modules in this set";
  }
};
}  // namespace generated
