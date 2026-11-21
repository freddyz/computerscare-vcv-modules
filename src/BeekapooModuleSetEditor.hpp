#pragma once

struct ModuleSetChoice : history::ModuleAction {
  std::string before, after;
  void apply(const std::string& id) {
    if (auto* m = dynamic_cast<Module*>(APP->engine->getModule(moduleId)))
      m->setGenerationSet(id);
  }
  void undo() override { apply(before); }
  void redo() override { apply(after); }
};

struct ModuleSetEditor : ui::MenuOverlay {
  struct Row : widget::OpaqueWidget {
    std::string label;
    bool checkbox = false, enabled = true, hovered = false;
    std::function<bool()> selected;
    std::function<void()> action;
    std::function<void()> hover;
    void draw(const DrawArgs& args) override {
      bool active = selected && selected();
      auto* vg = args.vg;
      nvgBeginPath(vg);
      nvgRect(vg, 0, 0, box.size.x, box.size.y);
      nvgFillColor(vg, hovered               ? nvgRGB(77, 84, 82)
                       : active && !checkbox ? nvgRGB(51, 82, 72)
                                             : nvgRGB(35, 38, 38));
      nvgFill(vg);
      if (checkbox) {
        nvgBeginPath(vg);
        nvgRect(vg, 7, 7, 13, 13);
        nvgStrokeColor(vg, nvgRGB(170, 185, 180));
        nvgStrokeWidth(vg, 1);
        nvgStroke(vg);
        if (active) {
          nvgBeginPath(vg);
          nvgMoveTo(vg, 9, 12);
          nvgLineTo(vg, 13, 17);
          nvgLineTo(vg, 19, 8);
          nvgStrokeColor(vg, nvgRGB(117, 225, 184));
          nvgStrokeWidth(vg, 2);
          nvgStroke(vg);
        }
      }
      nvgFontFaceId(
          vg, APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"))
                  ->handle);
      nvgFontSize(vg, 14);
      nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
      nvgFillColor(vg, enabled ? nvgRGB(234, 238, 236) : nvgRGB(135, 141, 139));
      nvgSave(vg);
      nvgIntersectScissor(vg, 0, 0, box.size.x - 6, box.size.y);
      nvgText(vg, checkbox ? 29 : 8, box.size.y * .5f, label.c_str(), nullptr);
      nvgRestore(vg);
    }
    void onEnter(const EnterEvent& e) override {
      hovered = true;
      if (hover) hover();
      widget::OpaqueWidget::onEnter(e);
    }
    void onLeave(const LeaveEvent& e) override {
      hovered = false;
      widget::OpaqueWidget::onLeave(e);
    }
    void onButton(const ButtonEvent& e) override {
      if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS) {
        e.consume(this);
        if (action) action();
        return;
      }
      widget::OpaqueWidget::onButton(e);
    }
  };
  struct Field : computerscare::type::Field {
    std::function<void(const std::string&)> changed;
    void onChange(const ChangeEvent& e) override {
      ui::TextField::onChange(e);
      if (changed) changed(text);
    }
  };
  struct Panel : widget::OpaqueWidget {
    ModuleSetEditor* editor;
    explicit Panel(ModuleSetEditor* e) : editor(e) {}
    void draw(const DrawArgs& args) override {
      auto* vg = args.vg;
      nvgBeginPath(vg);
      nvgRect(vg, 0, 0, box.size.x, box.size.y);
      nvgFillColor(vg, nvgRGB(47, 51, 50));
      nvgFill(vg);
      nvgStrokeColor(vg, nvgRGB(17, 20, 19));
      nvgStrokeWidth(vg, 2);
      nvgStroke(vg);
      nvgFontFaceId(
          vg, APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"))
                  ->handle);
      nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
      nvgFillColor(vg, nvgRGB(236, 240, 238));
      nvgFontSize(vg, 21);
      nvgText(vg, 16, 14, "Module sets", nullptr);
      nvgFontSize(vg, 13);
      nvgText(vg, 16, 91, "Name", nullptr);
      nvgText(vg, 16, 166, "Brands", nullptr);
      nvgText(vg, 202, 166, "Modules", nullptr);
      auto* set = moduleSets().find(editor->selectedId());
      auto count = string::f("%zu included", set ? set->modules.size() : 0);
      nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
      nvgText(vg, box.size.x - 16, 166, count.c_str(), nullptr);
      nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
      nvgFontSize(vg, 12);
      auto message =
          moduleSets().error.empty() ? editor->hint : moduleSets().error;
      nvgSave(vg);
      nvgIntersectScissor(vg, 16, box.size.y - 38, box.size.x - 32, 36);
      nvgText(vg, 16, box.size.y - 35, message.c_str(), nullptr);
      nvgText(vg, 16, box.size.y - 19,
              "Sets are shared. This Beek keeps its own choice. Changes apply "
              "to the next generation.",
              nullptr);
      nvgRestore(vg);
      widget::OpaqueWidget::draw(args);
    }
  };
  WeakPtr<Beekapoo> owner;
  WeakPtr<widget::Widget> previousFocus;
  Panel* panel;
  widget::ZoomWidget* zoom;
  ui::ScrollWidget *brands, *models;
  Field *name, *search;
  Row* defaultRow;
  std::string brand = "Fundamental",
              hint = "Click a module to include or exclude it.", displayedId;
  bool dirty = true, closed = false;
  std::vector<ActionButton*> buttons;
  explicit ModuleSetEditor(Beekapoo* group) : owner(group) {
    previousFocus = APP->event->getSelectedWidget();
    if (!group->generationError.empty()) hint = group->generationError;
    bgColor = nvgRGBA(0, 0, 0, 120);
    panel = new Panel(this);
    panel->box.size = Vec(700, 540);
    zoom = new widget::ZoomWidget;
    addChild(zoom);
    zoom->addChild(panel);
    auto button = [&](const std::string& title, Rect box,
                      std::function<void()> action) {
      auto* b = new ActionButton;
      b->text = title;
      b->textSize = 16;
      b->configure(box.size.x, box.size.y, 2, 1, .5f, int(box.pos.x) + 823);
      b->box.pos = box.pos;
      b->action = std::move(action);
      panel->addChild(b);
      buttons.push_back(b);
      return b;
    };
    button("Done", Rect(Vec(610, 12), Vec(74, 29)), [this] { close(); });
    auto* choose = button("", Rect(Vec(16, 49), Vec(318, 29)), [this] {
      WeakPtr<ModuleSetEditor> self = this;
      auto* menu = createMenu();
      for (const auto& set : moduleSets().sets) {
        auto id = set.id;
        menu->addChild(createCheckMenuItem(
            set.name, "",
            [self, id] { return self && self->selectedId() == id; },
            [self, id] {
              if (self) self->choose(id);
            }));
      }
    });
    choose->caption = [this] {
      auto* set = moduleSets().find(selectedId());
      return set ? set->name : "Missing set — choose a set";
    };
    button("New", Rect(Vec(347, 49), Vec(68, 29)),
           [this] { chooseSet(false); });
    button("Copy", Rect(Vec(426, 49), Vec(68, 29)),
           [this] { chooseSet(true); });
    auto* remove = button("Delete", Rect(Vec(505, 49), Vec(80, 29)), [this] {
      if (selectedId() == "fundamental") return;
      WeakPtr<ModuleSetEditor> self = this;
      auto id = selectedId();
      auto* menu = createMenu();
      menu->addChild(createMenuItem("Delete this saved set", "", [self, id] {
        if (!self) return;
        moduleSets().remove(id);
        self->choose(moduleSets().defaultId);
      }));
    });
    remove->help = [] { return "Delete saved set; Fundamental is retained"; };
    name = new Field;
    name->box = Rect(Vec(70, 86), Vec(264, 28));
    name->placeholder = "Set name";
    name->changed = [this](const std::string& value) {
      auto text = string::trim(value);
      if (text.empty()) return;
      if (auto* set = moduleSets().find(selectedId())) {
        set->name = text;
        moduleSets().save();
      }
    };
    panel->addChild(name);
    defaultRow = new Row;
    defaultRow->checkbox = true;
    defaultRow->label = "Default for new Beeks";
    defaultRow->box = Rect(Vec(347, 86), Vec(337, 28));
    defaultRow->selected = [this] {
      return selectedId() == moduleSets().defaultId;
    };
    defaultRow->action = [this] {
      if (moduleSets().find(selectedId())) {
        moduleSets().defaultId = selectedId();
        moduleSets().save();
      }
    };
    panel->addChild(defaultRow);
    search = new Field;
    search->box = Rect(Vec(16, 124), Vec(668, 28));
    search->placeholder = "Find a module or brand";
    search->changed = [this](const std::string&) { dirty = true; };
    panel->addChild(search);
    brands = new ui::ScrollWidget;
    brands->box = Rect(Vec(16, 188), Vec(174, 304));
    panel->addChild(brands);
    models = new ui::ScrollWidget;
    models->box = Rect(Vec(202, 188), Vec(482, 304));
    panel->addChild(models);
    APP->event->setSelectedWidget(search);
  }
  std::string selectedId() const {
    return owner ? owner->getModule<Module>()->getGenerationSet() : "";
  }
  void choose(const std::string& id) {
    if (!owner || selectedId() == id) return;
    auto* change = new ModuleSetChoice;
    change->name = "choose Beekapoo module set";
    change->moduleId = owner->module->id;
    change->before = selectedId();
    change->after = id;
    owner->getModule<Module>()->setGenerationSet(id);
    owner->generationError.clear();
    hint = "Click a module to include or exclude it.";
    APP->history->push(change);
    dirty = true;
  }
  void chooseSet(bool copy) {
    choose(moduleSets().add(copy, selectedId()));
    dirty = true;
  }
  void rebuild() {
    brands->container->clearChildren();
    models->container->clearChildren();
    auto* set = moduleSets().find(selectedId());
    std::map<std::string, std::vector<std::pair<ModuleKey, plugin::Model*>>>
        groups;
    auto filter = string::lowercase(search->text);
    for (auto* plugin : plugin::plugins)
      for (auto* model : plugin->models) {
        ModuleKey key{plugin->slug, model->slug};
        bool checked = set && set->modules.count(key);
        if (model->hidden && !checked) continue;
        auto match = string::lowercase(plugin->getBrand() + " " + model->name +
                                       " " + model->slug);
        if (!filter.empty() && match.find(filter) == std::string::npos)
          continue;
        groups[plugin->slug].push_back({key, model});
      }
    if (set)
      for (const auto& key : set->modules)
        if (!plugin::getModel(key.plugin, key.model) &&
            (filter.empty() ||
             string::lowercase(key.model).find(filter) != std::string::npos))
          groups[key.plugin].push_back({key, nullptr});
    if (!groups.count(brand))
      brand = groups.empty() ? "" : groups.begin()->first;
    std::vector<std::string> order;
    for (const auto& group : groups) order.push_back(group.first);
    auto brandName = [](const std::string& slug) {
      auto* p = plugin::getPlugin(slug);
      return p ? (p->getBrand().empty() ? slug : p->getBrand())
               : slug + " (missing)";
    };
    std::sort(order.begin(), order.end(),
              [&](const std::string& a, const std::string& b) {
                return string::lowercase(brandName(a)) <
                       string::lowercase(brandName(b));
              });
    int index = 0;
    for (const auto& slug : order) {
      auto* row = new Row;
      row->label = brandName(slug);
      row->box = Rect(Vec(0, index++ * 28), Vec(160, 28));
      row->selected = [this, slug] { return brand == slug; };
      row->action = [this, slug] {
        brand = slug;
        models->offset = Vec();
        dirty = true;
      };
      brands->container->addChild(row);
    }
    auto entries = groups[brand];
    std::sort(
        entries.begin(), entries.end(),
        [](const std::pair<ModuleKey, plugin::Model*>& a,
           const std::pair<ModuleKey, plugin::Model*>& b) {
          return string::lowercase(a.second ? a.second->name : a.first.model) <
                 string::lowercase(b.second ? b.second->name : b.first.model);
        });
    index = 0;
    for (const auto& entry : entries) {
      auto key = entry.first;
      auto* model = entry.second;
      auto reason = generated::unavailableReason(model);
      auto* row = new Row;
      row->checkbox = true;
      row->enabled = reason.empty();
      row->label = model ? model->name : key.model + " (not installed)";
      if (model && !reason.empty()) row->label += " — unavailable";
      row->box = Rect(Vec(0, index++ * 28), Vec(468, 28));
      row->selected = [this, key] {
        auto* current = moduleSets().find(selectedId());
        return current && current->modules.count(key);
      };
      row->hover = [this, model, reason] {
        hint = !reason.empty() ? reason
               : generated::knownRules(model)
                   ? "Uses module-specific patching rules."
                   : "Best-effort patching from port names and module tags.";
      };
      row->action = [this, key, reason, model] {
        if (auto* current = moduleSets().find(selectedId())) {
          if (current->modules.erase(key))
            moduleSets().save();
          else if (reason.empty()) {
            if (generated::specFor(model).outputs.empty()) {
              hint = "This module has no signal outputs.";
              return;
            }
            current->modules.insert(key);
            moduleSets().save();
          }
          owner->generationError.clear();
          hint = "Click a module to include or exclude it.";
        }
      };
      models->container->addChild(row);
    }
    dirty = false;
  }
  void close() {
    if (closed) return;
    closed = true;
    if (owner) owner->moduleSetEditor = nullptr;
    if (APP && APP->event) APP->event->setSelectedWidget(previousFocus.get());
    hide();
    requestDelete();
  }
  void step() override {
    if (closed || !parent) return;
    if (!owner) {
      close();
      return;
    }
    box = parent->box.zeroPos();
    float scale =
        std::min(1.f, std::min(std::max(100.f, box.size.x - 24) / 700.f,
                               std::max(100.f, box.size.y - 24) / 540.f));
    zoom->setZoom(scale);
    zoom->box = Rect((box.size - panel->box.size * scale) * .5f,
                     panel->box.size * scale);
    if (displayedId != selectedId()) {
      displayedId = selectedId();
      auto* set = moduleSets().find(displayedId);
      auto callback = name->changed;
      name->changed = nullptr;
      name->setText(set ? set->name : "");
      name->changed = std::move(callback);
      dirty = true;
    }
    if (dirty) rebuild();
    widget::Widget::step();
  }
  void onAction(const ActionEvent&) override { close(); }
  void onButton(const ButtonEvent& e) override {
    widget::OpaqueWidget::onButton(e);
    if (e.action == GLFW_PRESS && (!e.isConsumed() || e.getTarget() == this))
      close();
    if (!e.isConsumed()) e.consume(this);
  }
};
void Beekapoo::editModuleSets() {
  if (!module || moduleSetEditor) return;
  auto* editor = new ModuleSetEditor(this);
  moduleSetEditor = editor;
  APP->scene->addChild(editor);
  editor->step();
}
