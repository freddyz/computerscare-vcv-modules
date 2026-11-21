#pragma once
// Eject replaces patched, non-unity input bridges with native Fundamental
// 8vert channels. Identical source stacks are adjacent and use normalization.
struct EjectedInputGains {
  static constexpr int channels = 8;
  struct Route {
    int64_t target;
    int input;
    float gain;
    std::vector<int64_t> cables;
    std::vector<std::pair<int64_t, int>> sources;
  };
  std::vector<Route> routes;
  plugin::Model* model = nullptr;
  Vec position;
  float moduleWidth = 0;
  explicit EjectedInputGains(Beekapoo* owner, bool enabled) {
    if (!enabled) return;
    for (const auto& p : owner->ports)
      if (p.route >= 0) {
        float gain =
            owner->module->getParamQuantity(Module::INPUT_GAIN_BASE + p.route)
                ->getValue();
        if (gain == 1.f) continue;
        Route route{p.id, p.portId, gain, {}, {}};
        for (auto* cw : owner->homeRack->getCompleteCables()) {
          auto* c = cw->cable;
          if (c->inputModule != owner->module ||
              c->inputId != Module::inputBase + p.route)
            continue;
          route.cables.push_back(c->id);
          route.sources.emplace_back(c->outputModule->id, c->outputId);
        }
        if (route.cables.empty()) continue;
        std::sort(route.sources.begin(), route.sources.end());
        routes.push_back(std::move(route));
      }
    // Compare the whole source stack, including duplicate sources.
    std::stable_sort(
        routes.begin(), routes.end(),
        [](const Route& a, const Route& b) { return a.sources < b.sources; });
    if (!routes.empty()) {
      model = plugin::getModel("Fundamental", "8vert");
      if (!model) return;
      std::unique_ptr<app::ModuleWidget> preview(
          model->createModuleWidget(nullptr));
      moduleWidth = preview->box.size.x;
    }
  }
  float width() const {
    return ((routes.size() + channels - 1) / channels) * moduleWidth;
  }
  struct SelectedAdd : history::ModuleAdd {
    void redo() override {
      history::ModuleAdd::redo();
      if (auto* w = APP->scene->rack->getModule(moduleId))
        APP->scene->rack->select(w, true);
    }
  };
  void install(Beekapoo* owner, history::ComplexAction* batch) {
    auto* rack = owner->homeRack;
    app::ModuleWidget* adapter = nullptr;
    for (size_t i = 0; i < routes.size(); ++i) {
      int channel = i % channels;
      if (!channel) {
        auto* m = model->createModule();
        for (size_t j = i; j < std::min(routes.size(), i + channels); ++j)
          m->params[j - i].setValue(routes[j].gain);
        APP->engine->addModule(m);
        adapter = model->createModuleWidget(m);
        adapter->box.pos = position + Vec(i / channels * moduleWidth, 0);
        rack->addModule(adapter);
        rack->select(adapter, true);
        auto* add = new SelectedAdd;
        add->setModule(adapter);
        batch->push(add);
      }
      const auto& route = routes[i];
      bool normalized = channel > 0 && routes[i - 1].sources == route.sources;
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
      auto* target = rack->getModule(route.target);
      if (!target) continue;
      auto* c = new engine::Cable;
      c->outputModule = adapter->module;
      c->outputId = channel;
      c->inputModule = target->module;
      c->inputId = route.input;
      APP->engine->addCable(c);
      auto* cw = new app::CableWidget;
      cw->setCable(c);
      rack->addCable(cw);
      auto* add = new history::CableAdd;
      add->setCable(cw);
      batch->push(add);
    }
  }
};
