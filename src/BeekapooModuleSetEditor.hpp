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
  struct Card : widget::OpaqueWidget {
    ModuleSetEditor* editor;
    ModuleKey key;
    plugin::Model* model = nullptr;
    app::ModuleWidget* preview = nullptr;
    widget::ZoomWidget* zoom = nullptr;
    bool picker, failed = false;
    std::string reason;
    Card(ModuleSetEditor* editor, ModuleKey key, bool picker);
    void ensurePreview();
    void draw(const DrawArgs& args) override;
    // PreviewSurface already includes layer 1, as in Rack's browser.
    void drawLayer(const DrawArgs&, int) override {}
    void onHover(const HoverEvent& e) override { e.consume(this); }
    void onButton(const ButtonEvent& e) override;

    // Leave scrolling to the enclosing ScrollWidget, without visiting knobs.
    void onHoverScroll(const HoverScrollEvent&) override {}
  };
  struct Canvas : widget::OpaqueWidget {
    ModuleSetEditor* editor;
    explicit Canvas(ModuleSetEditor* e) : editor(e) {}
    void draw(const DrawArgs& args) override;
  };
  // Rack's browser draws both the ordinary widget and its light/display layer.
  struct PreviewSurface : widget::Widget {
    void draw(const DrawArgs& args) override {
      widget::Widget::draw(args);
      widget::Widget::drawLayer(args, 1);
    }
  };
  struct Panel : widget::OpaqueWidget {
    ModuleSetEditor* editor;
    explicit Panel(ModuleSetEditor* e) : editor(e) {}
    void draw(const DrawArgs& args) override;
  };
  struct Snapshot {
    std::vector<ModuleSet> sets;
    std::string defaultId, viewed;
  };
  struct Edit {
    Snapshot before, after;
  };
  WeakPtr<Beekapoo> owner;
  WeakPtr<widget::Widget> previousFocus;
  Panel* panel;
  widget::ZoomWidget *zoom, *rackZoom;
  ui::ScrollWidget *models, *rackScroll;
  Canvas* canvas;
  Field *name, *search;
  Row* defaultRow;
  ActionButton* deleteSetButton = nullptr;
  std::vector<Card*> cards;
  std::vector<ActionButton*> buttons;
  std::vector<Edit> edits;
  size_t editCursor = 0;
  bool dirty = true, rackDirty = true, closed = false;
  bool favoritesOnly = false;
  int filterTag = -1;
  float rackScale = .65f;
  std::string brand = "Fundamental", viewedId, displayedId;
  std::string hint = "Click a module on either side to include or remove it.";

  static void text(NVGcontext* vg, float x, float y, const std::string& value,
                   float size = 14, NVGcolor color = nvgRGB(231, 234, 231)) {
    auto font =
        APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
    if (!font) return;
    nvgFontFaceId(vg, font->handle);
    nvgFontSize(vg, size);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgFillColor(vg, color);
    nvgText(vg, x, y, value.c_str(), nullptr);
  }
  explicit ModuleSetEditor(Beekapoo* group) : owner(group) {
    previousFocus = APP->event->getSelectedWidget();
    viewedId = group->getModule<Module>()->getGenerationSet();
    bgColor = nvgRGBA(0, 0, 0, 170);
    panel = new Panel(this);
    panel->box.size = Vec(1100, 900);
    zoom = new widget::ZoomWidget;
    addChild(zoom);
    zoom->addChild(panel);
    auto button = [&](const std::string& title, Rect r,
                      std::function<void()> action) {
      auto* b = new ActionButton;
      b->text = title;
      b->textSize = 15;
      b->configure(r.size.x, r.size.y, 2, 1, .7f, unsigned(r.pos.x + 823));
      b->box.pos = r.pos;
      b->action = std::move(action);
      panel->addChild(b);
      buttons.push_back(b);
      return b;
    };
    button("Done", Rect(Vec(16, 12), Vec(76, 30)), [this] { close(); });
    auto* choose = button("", Rect(Vec(16, 51), Vec(258, 30)), [this] {
      WeakPtr<ModuleSetEditor> self = this;
      auto* menu = createMenu();
      for (const auto& set : moduleSets().sets) {
        auto id = set.id;
        menu->addChild(createCheckMenuItem(
            set.name, ModuleSets::isReadOnly(id) ? "read-only" : "",
            [self, id] { return self && self->selectedId() == id; },
            [self, id] {
              if (self) self->choose(id);
            }));
      }
    });
    choose->caption = [this] {
      auto* s = current();
      return s ? s->name : "Missing set — choose a set";
    };
    button("New", Rect(Vec(284, 51), Vec(60, 30)),
           [this] { chooseSet(false); });
    button("Duplicate", Rect(Vec(352, 51), Vec(97, 30)),
           [this] { chooseSet(true); });
    deleteSetButton =
        button("Delete set", Rect(Vec(457, 51), Vec(100, 30)), [this] {
          if (readOnly()) return;
          if (selectedId() == "fundamental") {
            hint = "The Fundamental set can be edited, but not deleted.";
            return;
          }
          WeakPtr<ModuleSetEditor> self = this;
          auto* menu = createMenu();
          menu->addChild(createMenuItem("Delete this saved set", "", [self] {
            if (self) self->deleteSet();
          }));
        });
    auto* use = button("Use this set", Rect(Vec(567, 51), Vec(126, 30)),
                       [this] { useSelected(); });
    use->pressed = [this] {
      return owner &&
             owner->getModule<Module>()->getGenerationSet() == selectedId();
    };
    use->caption = [this] {
      return owner && owner->getModule<Module>()->getGenerationSet() ==
                          selectedId()
                 ? "Using this set"
                 : "Use this set";
    };
    defaultRow = new Row;
    defaultRow->checkbox = true;
    defaultRow->label = "Default for new Beeks";
    defaultRow->box = Rect(Vec(708, 51), Vec(376, 30));
    defaultRow->selected = [this] {
      return selectedId() == moduleSets().defaultId;
    };
    defaultRow->action = [this] {
      if (current())
        change([&] { moduleSets().defaultId = selectedId(); }, false);
    };
    panel->addChild(defaultRow);
    name = new Field;
    name->box = Rect(Vec(16, 91), Vec(258, 28));
    name->placeholder = "Set name";
    name->changed = [this](const std::string& value) {
      auto v = string::trim(value);
      if (current() && !readOnly() && !v.empty() && current()->name != v)
        change([&] { current()->name = v; }, false);
    };
    panel->addChild(name);
    button("Undo", Rect(Vec(284, 91), Vec(76, 28)), [this] { undo(); });
    button("Redo", Rect(Vec(370, 91), Vec(76, 28)), [this] { redo(); });
    search = new Field;
    search->box = Rect(Vec(16, 135), Vec(258, 29));
    search->placeholder = "Search modules, brands, tags";
    search->changed = [this](const std::string&) { dirty = true; };
    panel->addChild(search);
    auto* brandButton = button("", Rect(Vec(16, 173), Vec(158, 27)), [this] {
      auto* menu = createMenu();
      WeakPtr<ModuleSetEditor> self = this;
      menu->addChild(createCheckMenuItem(
          "All brands", "", [self] { return self && self->brand.empty(); },
          [self] {
            if (self) {
              self->brand.clear();
              self->dirty = true;
            }
          }));
      std::vector<plugin::Plugin*> brands = plugin::plugins;
      std::sort(brands.begin(), brands.end(),
                [](plugin::Plugin* a, plugin::Plugin* b) {
                  return string::lowercase(a->getBrand()) <
                         string::lowercase(b->getBrand());
                });
      for (auto* p : brands) {
        auto slug = p->slug;
        menu->addChild(createCheckMenuItem(
            p->getBrand(), "",
            [self, slug] { return self && self->brand == slug; },
            [self, slug] {
              if (self) {
                self->brand = slug;
                self->dirty = true;
              }
            }));
      }
    });
    brandButton->caption = [this] {
      auto* p = plugin::getPlugin(brand);
      return brand.empty() ? "All brands" : p ? p->getBrand() : brand;
    };
    auto* fav = button("Favorites", Rect(Vec(182, 173), Vec(92, 27)), [this] {
      favoritesOnly = !favoritesOnly;
      dirty = true;
    });
    fav->pressed = [this] { return favoritesOnly; };
    auto* tags = button("", Rect(Vec(16, 208), Vec(258, 27)), [this] {
      auto* menu = createMenu();
      WeakPtr<ModuleSetEditor> self = this;
      menu->addChild(createCheckMenuItem(
          "All tags", "", [self] { return self && self->filterTag < 0; },
          [self] {
            if (self) {
              self->filterTag = -1;
              self->dirty = true;
            }
          }));
      for (int i = 0; i < int(tag::tagAliases.size()); ++i)
        if (!tag::tagAliases[i].empty())
          menu->addChild(createCheckMenuItem(
              tag::tagAliases[i][0], "",
              [self, i] { return self && self->filterTag == i; },
              [self, i] {
                if (self) {
                  self->filterTag = i;
                  self->dirty = true;
                }
              }));
    });
    tags->caption = [this] {
      return filterTag < 0 ? "All tags" : tag::tagAliases[filterTag][0];
    };
    models = new ui::ScrollWidget;
    models->box = Rect(Vec(16, 246), Vec(258, 600));
    panel->addChild(models);
    rackScroll = new ui::ScrollWidget;
    rackScroll->box = Rect(Vec(288, 135), Vec(796, 711));
    panel->addChild(rackScroll);
    rackZoom = new widget::ZoomWidget;
    rackScroll->container->addChild(rackZoom);
    canvas = new Canvas(this);
    rackZoom->addChild(canvas);
    auto* rails = new app::RailWidget;
    rails->box.size = Vec(10000, 10000);
    canvas->addChild(rails);
    APP->event->setSelectedWidget(search);
  }
  std::string selectedId() const { return viewedId; }
  bool readOnly() const { return ModuleSets::isReadOnly(viewedId); }
  ModuleSet* current() { return moduleSets().find(viewedId); }
  Snapshot snapshot() {
    return {moduleSets().sets, moduleSets().defaultId, viewedId};
  }
  void record(Snapshot before) {
    edits.resize(editCursor);
    edits.push_back({std::move(before), snapshot()});
    if (edits.size() > 80) edits.erase(edits.begin());
    editCursor = edits.size();
    moduleSets().save();
    if (owner) owner->generationError.clear();
  }
  void change(const std::function<void()>& action, bool rebuild = true) {
    auto before = snapshot();
    action();
    record(std::move(before));
    if (rebuild) rackDirty = true;
  }
  void restore(const Snapshot& state) {
    moduleSets().sets = state.sets;
    moduleSets().defaultId = state.defaultId;
    viewedId = state.viewed;
    displayedId.clear();
    moduleSets().save();
    rackDirty = true;
  }
  void undo() {
    if (editCursor) restore(edits[--editCursor].before);
  }
  void redo() {
    if (editCursor < edits.size()) restore(edits[editCursor++].after);
  }
  void choose(const std::string& id) {
    if (!moduleSets().find(id) || viewedId == id) return;
    viewedId = id;
    rackDirty = true;
    rackScroll->offset = Vec();
    hint = readOnly()
               ? "Default is read-only. Duplicate to customize."
               : "Click a module on either side to include or remove it.";
  }
  void useSelected() {
    if (!owner || !current()) return;
    auto* m = owner->getModule<Module>();
    if (m->getGenerationSet() == viewedId) return;
    auto* action = new ModuleSetChoice;
    action->name = "choose Beekapoo module set";
    action->moduleId = m->id;
    action->before = m->getGenerationSet();
    action->after = viewedId;
    m->setGenerationSet(viewedId);
    owner->generationError.clear();
    APP->history->push(action);
  }
  void chooseSet(bool copy) {
    change([&] { viewedId = moduleSets().add(copy, viewedId); });
    rackScroll->offset = Vec();
  }
  void deleteSet() {
    if (readOnly() || viewedId == "fundamental") return;
    change([&] {
      moduleSets().remove(viewedId);
      viewedId = moduleSets().defaultId;
    });
  }
  Card* findCard(const ModuleKey& key) {
    for (auto* c : cards)
      if (c->key == key) return c;
    return nullptr;
  }
  void addModel(const ModuleKey& key) {
    if (!current() || readOnly() || current()->modules.count(key)) return;
    auto* model = plugin::getModel(key.plugin, key.model);
    auto reason = generated::unavailableReason(model);
    if (!reason.empty()) {
      hint = reason;
      return;
    }
    change([&] { current()->modules.insert(key); });
    hint = "Added " + model->name + ".";
  }
  void toggleModel(const ModuleKey& key) {
    if (!current()) return;
    if (readOnly()) {
      hint = "Default is read-only. Duplicate to customize.";
      return;
    }
    if (current()->modules.count(key)) {
      change([&] { current()->modules.erase(key); });
      hint = "Removed from this set.";
    } else
      addModel(key);
  }
  void fitRack() {
    const Vec available = rackScroll->box.size - Vec(16, 16);
    float total = 0, widest = 1;
    for (auto* c : cards) {
      total += c->box.size.x;
      widest = std::max(widest, c->box.size.x);
    }
    float bestScale = 0, bestWidth = widest;
    // Try shelf widths, keeping module order stable, and maximize preview size.
    for (int candidate = 0; candidate <= 128; ++candidate) {
      float width = widest + (total - widest) * candidate / 128.f;
      float x = 0, used = 0;
      int rows = 1;
      for (auto* c : cards) {
        if (x > 0 && x + c->box.size.x > width + .01f) {
          x = 0;
          ++rows;
        }
        x += c->box.size.x;
        used = std::max(used, x);
      }
      float scale = std::min(1.25f, std::min(available.x / std::max(1.f, used),
                                             available.y / (rows * 380.f)));
      if (scale > bestScale) {
        bestScale = scale;
        bestWidth = width;
      }
    }
    rackScale = std::max(.001f, bestScale);
    Vec size = rackScroll->box.size / rackScale;
    float x = 0, y = 0;
    for (auto* c : cards) {
      if (x > 0 && x + c->box.size.x > bestWidth + .01f) {
        x = 0;
        y += 380;
      }
      c->box.pos = Vec(x, y);
      x += c->box.size.x;
    }
    canvas->box.size = size;
    rackZoom->setZoom(rackScale);
    rackZoom->box.size = rackScroll->box.size;
    rackScroll->offset = Vec();
  }
  void rebuildRack() {
    for (auto* c : cards) {
      canvas->removeChild(c);
      delete c;
    }
    cards.clear();
    if (auto* set = current())
      for (const auto& key : set->modules) {
        auto* c = new Card(this, key, false);
        canvas->addChild(c);
        cards.push_back(c);
      }
    rackDirty = false;
    fitRack();
  }
  void rebuild() {
    models->container->clearChildren();
    models->offset = Vec();
    std::vector<std::pair<ModuleKey, plugin::Model*>> entries;
    auto filter = string::lowercase(search->text);
    for (auto* p : plugin::plugins)
      for (auto* model : p->models) {
        if (model->hidden || (!brand.empty() && p->slug != brand)) continue;
        if (favoritesOnly) {
          auto brandInfo = settings::moduleInfos.find(p->slug);
          if (brandInfo == settings::moduleInfos.end()) continue;
          auto info = brandInfo->second.find(model->slug);
          if (info == brandInfo->second.end() || !info->second.favorite)
            continue;
        }
        if (filterTag >= 0 &&
            std::find(model->tagIds.begin(), model->tagIds.end(), filterTag) ==
                model->tagIds.end())
          continue;
        std::string match =
            p->getBrand() + " " + model->name + " " + model->slug;
        for (int t : model->tagIds)
          if (t >= 0 && t < int(tag::tagAliases.size()))
            for (auto& alias : tag::tagAliases[t]) match += " " + alias;
        if (!filter.empty() &&
            string::lowercase(match).find(filter) == std::string::npos)
          continue;
        entries.push_back({{p->slug, model->slug}, model});
      }
    std::sort(entries.begin(), entries.end(),
              [](const std::pair<ModuleKey, plugin::Model*>& a,
                 const std::pair<ModuleKey, plugin::Model*>& b) {
                return string::lowercase(a.second->name) <
                       string::lowercase(b.second->name);
              });
    int i = 0;
    for (auto& entry : entries) {
      auto* c = new Card(this, entry.first, true);
      c->box.pos = Vec((i % 2) * 123, (i / 2) * 170);
      models->container->addChild(c);
      ++i;
    }
    dirty = false;
  }
  bool textFocused() const {
    auto* w = APP->event->getSelectedWidget();
    return w == name || w == search;
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
    float scale = std::min(std::max(100.f, box.size.x - 24) / 1100.f,
                           std::max(100.f, box.size.y - 24) / 900.f);
    zoom->setZoom(scale);
    zoom->box = Rect((box.size - panel->box.size * scale) * .5f,
                     panel->box.size * scale);
    if (displayedId != viewedId) {
      displayedId = viewedId;
      auto callback = name->changed;
      name->changed = nullptr;
      name->setText(current() ? current()->name : "");
      name->changed = std::move(callback);
      rackDirty = true;
    }
    name->visible = !readOnly();
    deleteSetButton->visible = !readOnly();
    if (rackDirty) rebuildRack();
    if (dirty) rebuild();
    widget::Widget::step();
  }
  void onAction(const ActionEvent&) override { close(); }
  void onButton(const ButtonEvent& e) override {
    widget::OpaqueWidget::onButton(e);
    if (!e.isConsumed()) e.consume(this);
  }
  void onHoverKey(const HoverKeyEvent& e) override {
    int mods = e.mods & RACK_MOD_MASK;
    if (e.action == GLFW_PRESS && e.key == GLFW_KEY_ESCAPE) {
      close();
      e.consume(this);
      return;
    }
    if (!textFocused() && e.action == GLFW_PRESS) {
      if (e.key == GLFW_KEY_Z && mods == RACK_MOD_CTRL)
        undo();
      else if ((e.key == GLFW_KEY_Z &&
                mods == (RACK_MOD_CTRL | GLFW_MOD_SHIFT)) ||
               (e.key == GLFW_KEY_Y && mods == RACK_MOD_CTRL))
        redo();
      else if (e.key == GLFW_KEY_SPACE && !mods) {
        APP->event->setSelectedWidget(search);
        search->selectAll();
      }
      e.consume(this);
      return;
    }
    widget::Widget::onHoverKey(e);
    if (!e.isConsumed()) e.consume(this);
  }
};

ModuleSetEditor::Card::Card(ModuleSetEditor* e, ModuleKey k, bool p)
    : editor(e), key(std::move(k)), picker(p) {
  model = plugin::getModel(key.plugin, key.model);
  reason = generated::unavailableReason(model);
  box.size = picker ? Vec(116, 162) : Vec(180, 380);
  if (!picker) ensurePreview();
}
void ModuleSetEditor::Card::ensurePreview() {
  if (preview || failed || !model) return;
  try {
    preview = model->createModuleWidget(nullptr);
    if (!preview) {
      failed = true;
      return;
    }
    zoom = new widget::ZoomWidget;
    addChild(zoom);
    auto* fb = new widget::FramebufferWidget;
    if (APP->window->pixelRatio < 2.0) fb->oversample = 2.0;
    zoom->addChild(fb);
    auto* surface = new PreviewSurface;
    fb->addChild(surface);
    surface->addChild(preview);
    surface->box.size = preview->box.size;
    preview->box.pos = Vec();
    preview->step();
    if (!picker) box.size = Vec(std::max(15.f, preview->box.size.x), 380);
    float scale = picker ? std::min(108.f / std::max(1.f, preview->box.size.x),
                                    128.f / std::max(1.f, preview->box.size.y))
                         : 1.f;
    zoom->setZoom(scale);
    zoom->box = Rect(Vec(picker ? (116 - preview->box.size.x * scale) * .5f : 0,
                         picker ? 4 : 0),
                     preview->box.size * scale);
  } catch (const std::exception&) {
    failed = true;
    reason = "Preview unavailable";
  }
}
void ModuleSetEditor::Card::draw(const DrawArgs& args) {
  ensurePreview();
  auto* vg = args.vg;
  nvgBeginPath(vg);
  nvgRect(vg, 0, 0, box.size.x, box.size.y);
  nvgFillColor(vg, picker ? nvgRGB(52, 56, 54) : nvgRGB(70, 73, 69));
  nvgFill(vg);
  widget::OpaqueWidget::draw(args);
  bool included = editor->current() && editor->current()->modules.count(key);
  if (!preview) {
    nvgSave(vg);
    nvgIntersectScissor(vg, 3, 4, box.size.x - 6, box.size.y - 8);
    ModuleSetEditor::text(vg, 8, 18, key.model, 15);
    ModuleSetEditor::text(vg, 8, 42,
                          model ? "Preview unavailable" : "Not installed", 12);
    ModuleSetEditor::text(vg, 8, 65, key.plugin, 12);
    nvgRestore(vg);
  }
  if (picker) {
    nvgSave(vg);
    nvgIntersectScissor(vg, 4, 136, 108, 26);
    auto title = model ? model->name : key.model;
    float size = std::min(13.f, 160.f / std::max(10.f, float(title.size())));
    ModuleSetEditor::text(vg, 5, 139, title, std::max(8.f, size));
    nvgRestore(vg);
    if (included) {
      nvgBeginPath(vg);
      nvgCircle(vg, 107, 9, 4);
      nvgFillColor(vg, nvgRGB(98, 207, 164));
      nvgFill(vg);
    }
    if (!reason.empty()) {
      nvgBeginPath(vg);
      nvgRect(vg, 0, 0, box.size.x, box.size.y);
      nvgFillColor(vg, nvgRGBA(20, 20, 20, 130));
      nvgFill(vg);
    }
  }
  if (picker && included) {
    nvgBeginPath(vg);
    nvgRect(vg, 1.5f, 1.5f, box.size.x - 3, box.size.y - 3);
    nvgStrokeColor(vg, nvgRGB(99, 218, 177));
    nvgStrokeWidth(vg, picker ? 1.5f : 3.f);
    nvgStroke(vg);
  }
}
void ModuleSetEditor::Card::onButton(const ButtonEvent& e) {
  if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
    // Focus the stable canvas: the clicked card may disappear at the next step.
    APP->event->setSelectedWidget(editor->canvas);
    editor->toggleModel(key);
  }
  e.consume(this);
}
void ModuleSetEditor::Canvas::draw(const DrawArgs& args) {
  nvgBeginPath(args.vg);
  nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
  nvgFillColor(args.vg, nvgRGB(35, 36, 35));
  nvgFill(args.vg);
  widget::OpaqueWidget::draw(args);
  if (editor->cards.empty())
    ModuleSetEditor::text(args.vg, 25, 80, "Add modules from the picker", 22);
}
void ModuleSetEditor::Panel::draw(const DrawArgs& args) {
  auto* vg = args.vg;
  nvgBeginPath(vg);
  nvgRect(vg, 0, 0, box.size.x, box.size.y);
  nvgFillColor(vg, nvgRGB(45, 49, 47));
  nvgFill(vg);
  ModuleSetEditor::text(vg, 110, 13, "Module sets", 24);
  ModuleSetEditor::text(vg, 300, 20, "Choose modules for new patches and Grow",
                        13);
  auto* set = editor->current();
  if (editor->readOnly())
    ModuleSetEditor::text(vg, 16, 99, "Read-only · Duplicate to customize", 12);
  ModuleSetEditor::text(
      vg, 470, 98,
      string::f("%zu modules  ·  %d%%", set ? set->modules.size() : 0,
                int(editor->rackScale * 100)),
      13);
  widget::OpaqueWidget::draw(args);
  nvgSave(vg);
  nvgIntersectScissor(vg, 16, 858, box.size.x - 32, 36);
  ModuleSetEditor::text(
      vg, 16, 858,
      moduleSets().error.empty()
          ? (editor->readOnly()
                 ? "Default is read-only. Duplicate to customize."
                 : editor->hint)
          : moduleSets().error,
      13);
  ModuleSetEditor::text(
      vg, 16, 879,
      "Changes save automatically. Sets are shared; each Beek chooses its own. "
      "  Undo: Mod+Z · Redo: Mod+Shift+Z · Close: Esc",
      11);
  nvgRestore(vg);
}

void Beekapoo::editModuleSets() {
  if (!module || moduleSetEditor) return;
  auto* editor = new ModuleSetEditor(this);
  moduleSetEditor = editor;
  APP->scene->addChild(editor);
  editor->step();
}
