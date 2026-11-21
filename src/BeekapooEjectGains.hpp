#pragma once
// Eject replaces patched, non-unity input bridges with native Fundamental
// 8vert channels. Identical source stacks are adjacent and use normalization.
struct EjectedInputGains {
  static constexpr int channels = 8;
  struct Route {
    int64_t target;
    int input;
    float gain;
    std::vector<Beekapoo::InputDestination> extra;
    std::vector<int64_t> cables;
    std::vector<std::pair<int64_t, int>> sources;
  };
  std::vector<Route> routes;
  plugin::Model* model = nullptr;
  Vec position;
  float moduleWidth = 0;
  explicit EjectedInputGains(Beekapoo* owner, bool enabled,
                             int onlyRoute = -1) {
    if (!enabled) return;
    for (const auto& p : owner->ports)
      if (p.route >= 0 && (onlyRoute < 0 || p.route == onlyRoute)) {
        float gain =
            owner->module->getParamQuantity(Module::INPUT_GAIN_BASE + p.route)
                ->getValue();
        if (gain == 1.f && p.shared.extra.empty() && p.shared.key < 0) continue;
        Route route{p.id, p.portId, gain, p.shared.extra, {}, {}};
        for (auto* cw : owner->homeRack->getCompleteCables()) {
          auto* c = cw->cable;
          if (c->inputModule != owner->module ||
              c->inputId != Module::inputBase + p.route)
            continue;
          route.cables.push_back(c->id);
          route.sources.emplace_back(c->outputModule->id, c->outputId);
        }
        if (route.cables.empty()) {
          if (p.shared.key < 0) continue;
          // Imported hardware capture remains silence on Eject, even when
          // its destination would otherwise normalize an unpatched input.
          route.gain = 0.f;
        } else if (route.gain == 1.f && route.extra.empty())
          continue;
        std::sort(route.sources.begin(), route.sources.end());
        routes.push_back(std::move(route));
      }
    // Compare the whole source stack, including duplicate sources.
    std::stable_sort(
        routes.begin(), routes.end(),
        [](const Route& a, const Route& b) { return a.sources < b.sources; });
    if (adapterCount()) {
      model = plugin::getModel("Fundamental", "8vert");
      if (!model) return;
      std::unique_ptr<app::ModuleWidget> preview(
          model->createModuleWidget(nullptr));
      moduleWidth = preview->box.size.x;
    }
  }
  size_t adapterCount() const {
    size_t count = 0;
    for (const auto& route : routes) count += route.gain != 1.f;
    return (count + channels - 1) / channels;
  }
  float width() const { return adapterCount() * moduleWidth; }
  struct SelectedAdd : history::ModuleAdd {
    void redo() override {
      history::ModuleAdd::redo();
      if (auto* w = APP->scene->rack->getModule(moduleId))
        APP->scene->rack->select(w, true);
    }
  };
  std::vector<app::ModuleWidget*> install(Beekapoo* owner,
                                          history::ComplexAction* batch,
                                          bool select = true) {
    auto* rack = owner->homeRack;
    std::vector<app::ModuleWidget*> added;
    size_t insertion = batch->actions.size();
    app::ModuleWidget* adapter = nullptr;
    auto connect = [&](app::PortWidget* from, int64_t targetId, int input,
                       NVGcolor color) {
      auto* target = owner->find(targetId);
      if (!target || !target->getInput(input)) return;
      auto* c = new engine::Cable;
      c->outputModule = from->module;
      c->outputId = from->portId;
      c->inputModule = target->module;
      c->inputId = input;
      APP->engine->addCable(c);
      auto* cw = new app::CableWidget;
      cw->setCable(c);
      cw->color = color;
      rack->addCable(cw);
      auto* add = new history::CableAdd;
      add->setCable(cw);
      batch->push(add);
    };
    size_t used = 0;
    const Route* previous = nullptr;
    for (const auto& route : routes) {
      if (route.gain == 1.f) {
        // detachInput retains the original stack at its first destination.
        // Fan it out to the others using native cables, not another module.
        for (auto id : route.cables)
          if (auto* cw = rack->getCable(id))
            for (const auto& to : route.extra)
              connect(cw->outputPort, to.id, to.port, cw->color);
        continue;
      }
      int channel = used % channels;
      if (!channel) {
        auto* m = model->createModule();
        APP->engine->addModule(m);
        adapter = model->createModuleWidget(m);
        adapter->box.pos = position + Vec(used / channels * moduleWidth, 0);
        rack->addModule(adapter);
        added.push_back(adapter);
        if (select) rack->select(adapter, true);
      }
      adapter->module->params[channel].setValue(route.gain);
      // Capture the adapter after its full bank of values has been filled.
      bool normalized =
          channel > 0 && previous && previous->sources == route.sources;
      for (auto id : route.cables)
        if (auto* cw = rack->getCable(id)) {
          auto* remove = new history::CableRemove;
          remove->setCable(cw);
          batch->push(remove);
          if (normalized) {
            rack->removeCable(cw);
            delete cw;
          } else {
            owner->rewireInput(cw, adapter->getInput(channel));
            auto* add = new history::CableAdd;
            add->setCable(cw);
            batch->push(add);
          }
        }
      auto color = settings::cableColors.empty()
                       ? nvgRGB(130, 130, 130)
                       : settings::cableColors.front();
      connect(adapter->getOutput(channel), route.target, route.input, color);
      for (const auto& to : route.extra)
        connect(adapter->getOutput(channel), to.id, to.port, color);
      previous = &route;
      ++used;
    }
    // Module creation must precede all cable actions on Redo, and its saved
    // parameter state must contain every occupied channel.
    for (size_t i = added.size(); i-- > 0;) {
      history::ModuleAdd* add =
          select ? new SelectedAdd : new history::ModuleAdd;
      add->setModule(added[i]);
      batch->actions.insert(batch->actions.begin() + insertion, add);
    }
    return added;
  }
};
