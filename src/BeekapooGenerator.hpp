#pragma once
#include "BeekapooNames.hpp"
namespace generated {
using beekapoo_patch::library;
using beekapoo_patch::moduleCount;
inline float moduleWidth(plugin::Model* model) {
  static std::map<std::string, float> cache;
  std::string key = model->plugin->slug + "/" + model->slug;
  auto found = cache.find(key);
  if (found != cache.end()) return found->second;
  std::unique_ptr<app::ModuleWidget> preview(
      model->createModuleWidget(nullptr));
  return cache[key] = preview->box.size.x;
}
// Build a portable group without touching the rack. Each processor gets its
// signal input; envelopes get a running gate. Spare inputs stay available on
// the front, rather than breaking a required connection to expose a jack.
inline json_t* plan(int requestedCount = 0,
                    const beekapoo_patch::ExposureCounts* exact = nullptr) {
  const auto& specs = library();
  for (const auto& spec : specs)
    if (!plugin::getModel("Fundamental", spec.slug)) return nullptr;
  auto graph =
      beekapoo_patch::Planner(random::u32()).make(requestedCount, exact);
  if (graph.modules.empty()) return nullptr;
  const auto& selected = graph.modules;
  int count = int(selected.size());
  auto* root = json_pack("{s:[],s:[],s:[]}", "members", "ports", "parameters");
  auto* nodes = json_array();
  auto* cables = json_array();
  json_object_set_new(
      root, "contents",
      json_pack("{s:o,s:o}", "modules", nodes, "cables", cables));
  std::vector<std::unique_ptr<engine::Module>> modules;
  double nextX = 0;
  for (int i = 0; i < count; ++i) {
    const auto& spec = specs[selected[i]];
    modules.emplace_back(
        plugin::getModel("Fundamental", spec.slug)->createModule());
    auto* m = modules.back().get();
    for (const auto& range : spec.initial)
      if (auto* q = m->getParamQuantity(range.param))
        q->setImmediateValue(clamp(range.sample(random::uniform()),
                                   q->getMinValue(), q->getMaxValue()));
    auto* node = m->toJson();
    json_object_set_new(node, "sourceId", json_integer(i + 1));
    json_array_append_new(nodes, node);
    json_array_append_new(
        json_object_get(root, "members"),
        json_pack("{s:i,s:f,s:f}", "id", i + 1, "x", nextX, "y", 0.));
    nextX += moduleWidth(m->model);
  }
  for (const auto& c : graph.cables)
    json_array_append_new(
        cables,
        json_pack("{s:i,s:i,s:i,s:i,s:i}", "id",
                  int(json_array_size(cables)) + 1, "outputModuleId",
                  c.from.node + 1, "outputId", c.from.port, "inputModuleId",
                  c.to.node + 1, "inputId", c.to.port));
  auto expose = [&](beekapoo_patch::Endpoint e, bool input) {
    json_array_append_new(json_object_get(root, "ports"),
                          json_pack("{s:i,s:i,s:b}", "module", e.node + 1,
                                    "port", e.port, "input", input));
  };
  for (auto e : graph.inputs) expose(e, true);
  for (auto e : graph.outputs) expose(e, false);
  for (auto e : graph.parameters)
    json_array_append_new(
        json_object_get(root, "parameters"),
        json_pack("{s:i,s:i}", "module", e.node + 1, "param", e.port));
  std::vector<std::string> styleModules;
  std::vector<StyleCable> styleCables;
  for (const auto& m : modules)
    styleModules.push_back(moduleStyleName(m->model));
  for (size_t i = 0; i < json_array_size(cables); ++i) {
    auto* c = json_array_get(cables, i);
    styleCables.emplace_back(
        int(json_integer_value(json_object_get(c, "outputModuleId"))) - 1,
        int(json_integer_value(json_object_get(c, "outputId"))),
        int(json_integer_value(json_object_get(c, "inputModuleId"))) - 1,
        int(json_integer_value(json_object_get(c, "inputId"))));
  }
  auto identity = patchIdentity(styleModules, styleCables);
  for (size_t i = 0; i < json_array_size(cables); ++i)
    json_object_set_new(
        json_array_get(cables, i), "color",
        json_string(color::toHexString(patchCableColor(identity, i)).c_str()));
  return root;
}
}  // namespace generated

// Only structural edits rename generated titles. Native knobs, external
// cables, exposure, layout and theme do not change this identity. Restore
// seeds a new baseline so Undo, duplication and patch reload keep saved names.
void Beekapoo::refreshGeneratedName() {
  if (!module) return;
  refreshAppearance();
  if (namedTopologyReady && namedTopology != appearance.wiring &&
      !members.empty()) {
    auto* m = getModule<Module>();
    if (!m->hasCustomPatchName()) m->setPatchName(generated::name());
  }
  namedTopology = appearance.wiring;
  namedTopologyReady = true;
}

bool Beekapoo::generateDefaultPatch(int requestedCount) {
  if (!module || !members.empty()) return false;
  auto* root = generated::plan(requestedCount);
  if (!root) {
    status = "Install VCV Free to generate a subpatch";
    return false;
  }
  DEFER({ json_decref(root); });
  auto* m = getModule<Module>();
  auto previous = m->getPatchName();
  m->setPatchName(generated::name());
  bool result = importContents(root);
  namedTopologyReady = false;
  refreshGeneratedName();
  if (!result) m->setPatchName(previous);
  return result;
}
bool Beekapoo::regeneratePatch() {
  if (!module) return false;
  ScopedSubviewRack mainScope(homeRack);
  if (!randomLocks.empty()) {
    // Keep native identities and every locked connection intact. Locks turn
    // replacement into an in-place variation of the unlocked graph.
    auto* batch = new TopologyHistory(module->id);
    batch->name = "regenerate unlocked subpatch";
    parameterAction(4, true, batch);
    int growth = 1 + random::u32() % 3;
    for (int i = 0; i < growth; ++i) growPatch(batch);
    if (batch->isEmpty()) {
      delete batch;
      status = "All subpatch modules are locked";
      return false;
    }
    APP->history->push(batch);
    status = "Unlocked subpatch regenerated";
    return true;
  }
  beekapoo_patch::ExposureCounts counts{0, 0, int(parameters.size())};
  for (const auto& p : ports) ++(p.input ? counts.inputs : counts.outputs);
  auto* root = generated::plan(0, &counts);
  if (!root) {
    status = "Could not generate a subpatch with these control counts";
    return false;
  }
  DEFER({ json_decref(root); });
  auto* change = new history::ModuleChange;
  change->name = "generate new subpatch";
  change->moduleId = module->id;
  change->oldModuleJ = toJson();
  auto previous = getModule<Module>()->getPatchName();
  getModule<Module>()->setPatchName(generated::name());
  if (!importContents(root, "", -1, change)) {
    getModule<Module>()->setPatchName(previous);
    delete change;
    return false;
  }
  namedTopologyReady = false;
  refreshGeneratedName();
  return true;
}
void Beekapoo::initializeBrowserPreview() {
  requestedWidth = 90;
  auto* root = generated::plan();
  browserName = generated::name();
  std::vector<std::string> types;
  std::vector<StyleCable> wires;
  std::map<int64_t, size_t> ids;
  if (root) {
    auto* nodes = json_object_get(json_object_get(root, "contents"), "modules");
    for (size_t i = 0; i < json_array_size(nodes); ++i) {
      auto* node = json_array_get(nodes, i);
      auto* model =
          plugin::getModel(json_string_value(json_object_get(node, "plugin")),
                           json_string_value(json_object_get(node, "model")));
      if (!model) continue;
      std::unique_ptr<engine::Module> m(model->createModule());
      m->fromJson(node);
      ids[json_integer_value(json_object_get(node, "sourceId"))] =
          browserModules.size();
      types.push_back(moduleStyleName(model));
      browserModules.push_back(std::move(m));
    }
    auto* cables = json_object_get(json_object_get(root, "contents"), "cables");
    for (size_t i = 0; i < json_array_size(cables); ++i) {
      auto* c = json_array_get(cables, i);
      auto a = ids.find(
               json_integer_value(json_object_get(c, "outputModuleId"))),
           b = ids.find(
               json_integer_value(json_object_get(c, "inputModuleId")));
      if (a != ids.end() && b != ids.end())
        wires.emplace_back(
            a->second, json_integer_value(json_object_get(c, "outputId")),
            b->second, json_integer_value(json_object_get(c, "inputId")));
    }
  }
  if (browserModules.empty()) {
    std::unique_ptr<engine::Module> m(new engine::Module);
    m->config(3, 0, 0, 0);
    m->model = modelComputerscareBeekapoo;
    m->configParam(0, 0, 1, .6f, "Frequency");
    m->configParam(1, 0, 1, .4f, "Pulse width");
    m->configParam(2, 0, 1, .8f, "Level");
    types = {"VCV\x1fVCO", "VCV\x1fLFO", "VCV\x1fVCF"};
    browserModules.push_back(std::move(m));
  }
  browserMemberCount = std::max(size_t(3), browserModules.size());
  appearance = patchIdentity(types, wires);
  std::vector<widget::Widget*> knobs, ins, outs;
  std::map<widget::Widget*, std::pair<std::string, std::string>> names;
  auto addKnob = [&](engine::Module* m, int param) {
    auto* q = m->getParamQuantity(param);
    if (!q) return;
    auto* knob = new MappedParam<ParametricKnob>;
    knob->module = m;
    knob->paramId = param;
    knob->initParamQuantity();
    knob->setIdentity(q->getLabel(), moduleStyleName(m->model), appearance);
    controls->addChild(knob);
    knobs.push_back(knob);
    names[knob] = {q->getLabel(), ""};
  };
  if (root && !ids.empty()) {
    auto* params = json_object_get(root, "parameters");
    for (size_t i = 0; i < json_array_size(params); ++i) {
      auto* p = json_array_get(params, i);
      auto it = ids.find(json_integer_value(json_object_get(p, "module")));
      if (it != ids.end())
        addKnob(browserModules[it->second].get(),
                json_integer_value(json_object_get(p, "param")));
    }
  } else
    for (int i = 0; i < 3; ++i) addKnob(browserModules.front().get(), i);
  browserInputGains.reset(new engine::Module);
  browserInputGains->model = modelComputerscareBeekapoo;
  browserInputGains->config(3, 0, 0, 0);
  for (int i = 0; i < 4; ++i) {
    auto* port = new PortFace;
    port->input = i < 3;
    port->box.size = Vec(16, 16);
    controls->addChild(port);
    if (port->input) {
      browserInputGains->configParam(i, -1, 1, 1, "Input attenuverter", "%", 0,
                                     100);
      auto* gain = new MappedParam<ThemedSmallKnob>;
      gain->module = browserInputGains.get();
      gain->paramId = i;
      gain->initParamQuantity();
      controls->addChild(gain);
      port->gain = gain;
    }
    (port->input ? ins : outs).push_back(port);
    names[port] = {port->input ? "Input" : "Output", ""};
  }
  layoutOnlyJacks(ins, outs, knobs, names);
  if (root) json_decref(root);
}
void Beekapoo::generateOnBrowserAdd() {
  auto* m = getModule<Module>();
  if (!m || !m->freshDefault || homeRack->getModule(m->id) != this) return;
  m->freshDefault = false;
  if (!members.empty() || expanded || m->restorePending) return;
  // Only the browser's native add action, never a null-module thumbnail,
  // direct fixture construction, patch restore, selection grouping or paste.
  auto* history = APP->history;
  if (history->actionIndex <= 0) return;
  std::function<bool(history::Action*)> added = [&](history::Action* action) {
    if (auto* add = dynamic_cast<history::ModuleAdd*>(action))
      return !dynamic_cast<history::ModuleRemove*>(add) &&
             add->moduleId == m->id;
    if (auto* batch = dynamic_cast<history::ComplexAction*>(action))
      for (auto* child : batch->actions)
        if (added(child)) return true;
    return false;
  };
  if (added(history->actions[history->actionIndex - 1])) generateDefaultPatch();
}
void Beekapoo::renameMenu(ui::Menu* menu) {
  menu->addChild(createMenuItem("Rename…", "", [this] { editName(); }));
}
