#pragma once
// A nonmodal session over the actual Rack. No proxy modules or cable aliases.
struct RackEditSession;
struct EditedInputDeleteItem : ui::MenuItem {
  WeakPtr<Beekapoo> owner;
  WeakPtr<app::PortWidget> port;
  void onAction(const ActionEvent&) override {
    if (!owner || !port) return;
    owner->withInputCablePresentation(port.get(), [&] {
      ScopedVisiblePortCables hidden(owner->homeRack, port.get(), true);
      port->deleteTopCableAction();
    });
  }
};
struct RackEditHighlight : widget::TransparentWidget {
  WeakPtr<RackEditSession> session;
  WeakPtr<app::ModuleWidget> sourceModule;
  void draw(const DrawArgs& args) override;
};
struct RackEditPreview : widget::OpaqueWidget {
  WeakPtr<Beekapoo> owner;
  void draw(const DrawArgs& args) override {
    if (!owner) return;
    nvgSave(args.vg);
    float scale = std::min(box.size.x / owner->box.size.x,
                           box.size.y / owner->box.size.y);
    nvgScale(args.vg, scale, scale);
    bool visible = owner->controls->visible, expanded = owner->expanded;
    owner->controls->visible = true;
    owner->expanded = false;
    DrawArgs local = args;
    local.clipBox = Rect(args.clipBox.pos / scale, args.clipBox.size / scale);
    // This is a second drawing of live widgets at a different scale. Draw
    // their artwork directly instead of reusing/mutating Rack's cached images.
    std::vector<std::pair<widget::FramebufferWidget*, bool>> caches;
    std::function<void(widget::Widget*)> bypassCaches = [&](widget::Widget* w) {
      if (auto* fb = dynamic_cast<widget::FramebufferWidget*>(w)) {
        caches.emplace_back(fb, fb->bypassed);
        fb->bypassed = true;
      }
      for (auto* child : w->children) bypassCaches(child);
    };
    bypassCaches(owner.get());
    owner->draw(local);
    owner->drawLayer(local, 1);
    for (auto& cache : caches) cache.first->bypassed = cache.second;
    owner->expanded = expanded;
    owner->controls->visible = visible;
    nvgRestore(args.vg);
  }
};
struct RackEditSession : widget::Widget {
  WeakPtr<Beekapoo> owner;
  Vec previousOffset;
  float previousZoom;
  RackLayoutSnapshot previousSelection;
  std::set<int64_t> known, admitted;
  bool exposeMode = false, randomLockMode = false, showPanel = true,
       showMembers = true, finished = false, showKeyboard = false;
  float toolbarHeight = 52;
  Vec descriptionPosition;
  widget::SvgWidget memberLogo;
  struct MemberTag {
    Vec corners[4];
    float tilt, shear, width, height;
    MemberTag() {
      auto vary = [](float amount) {
        return (random::uniform() * 2 - 1) * amount;
      };
      const Vec base[] = {Vec(0, 2), Vec(22, 0), Vec(24, 22), Vec(1, 24)};
      for (int i = 0; i < 4; ++i)
        corners[i] = base[i] + Vec(vary(1.2f), vary(1.2f));
      tilt = vary(.085f);
      shear = vary(.055f);
      width = 18 * (1 + vary(.045f));
      height = 18 * (1 + vary(.045f));
    }
  };
  // One fresh variation per member per editing session, never per frame.
  std::map<int64_t, MemberTag> memberTags;
  ActionButton *done, *exposeButton, *panelButton, *removeButton,
      *newModulesButton, *nearButton, *farButton, *membersButton,
      *randomLockButton, *optionsButton, *setsButton, *sequenceButton;
  RackEditPreview* preview;
  std::vector<WeakPtr<RackEditHighlight>> highlights;
  explicit RackEditSession(Beekapoo* group) : owner(group) {
    showMembers = group->markMembers;
    if (auto* m = group->getModule<Module>())
      showPanel = m->showEditPanel.load();
    memberLogo.setSvg(APP->window->loadSvg(
        asset::plugin(pluginInstance, "res/Beekapoo/computerscare-logo.svg")));
    auto* scroll = APP->scene->rackScroll;
    previousOffset = scroll->getGridOffset();
    previousZoom = scroll->getZoom();
    previousSelection.capture();
    for (auto id : APP->engine->getModuleIds()) known.insert(id);
    for (auto& member : group->members) admitted.insert(member.id);
    auto addButton = [this](const std::string& label, float x, float width,
                            std::function<void()> action) {
      auto* button = new ActionButton;
      button->text = label;
      button->textSize = 21;
      button->configure(width, 35, 3, 2, .8f, int(x) + 975);
      button->box.pos = Vec(x, 7);
      button->action = std::move(action);
      addChild(button);
      return button;
    };
    done = addButton("Done", 10, 78, [this] { finish(true); });
    done->help = [] { return "Finish editing [esc]"; };
    exposeButton = addButton("Expose", 100, 94, [this] {
      exposeMode = !exposeMode;
      randomLockMode = false;
      if (owner) owner->saveGroup();
    });
    exposeButton->pressed = [this] { return exposeMode; };
    exposeButton->help = [] { return "Switch Adjust / Expose [E / X]"; };
    randomLockButton = addButton("Lock", 196, 112, [this] {
      randomLockMode = !randomLockMode;
      exposeMode = false;
      if (owner) owner->saveGroup();
    });
    randomLockButton->pressed = [this] { return randomLockMode; };
    randomLockButton->help = [] {
      return "Click modules or parameters to lock randomization, wiggle and "
             "sequencing [L]";
    };
    setsButton = addButton("Module sets", 870, 144, [this] {
      if (owner) owner->editModuleSets();
    });
    setsButton->help = [] {
      return "Choose modules for new patches and growth";
    };
    optionsButton = addButton("...", 820, 38, [this] {
      auto group = owner;
      auto* menu = createMenu();
      menu->addChild(createMenuItem(
          "Unlock all", "",
          [group] {
            if (group) group->unlockRandomization();
          },
          !group || group->randomLocks.empty()));
    });
    optionsButton->help = [] { return "Edit options"; };
    panelButton = addButton("Panel", 206, 80, [this] {
      showPanel = !showPanel;
      if (owner) {
        owner->getModule<Module>()->showEditPanel.store(showPanel);
        owner->layoutControls();
        owner->saveGroup();
      }
    });
    panelButton->pressed = [this] { return showPanel; };
    panelButton->help = [] { return "Show Beekapoo panel [P]"; };
    newModulesButton = addButton("New: Inside", 298, 146, [this] {
      if (owner) {
        owner->captureNewModules = !owner->captureNewModules;
        owner->saveGroup();
      }
    });
    newModulesButton->caption = [this] {
      return owner && owner->captureNewModules ? "New: Inside" : "New: Outside";
    };
    newModulesButton->pressed = [this] {
      return owner && owner->captureNewModules;
    };
    newModulesButton->help = [] {
      return "Choose whether new modules join this subpatch [N]";
    };
    nearButton = addButton("Near", 456, 66, [this] {
      if (owner) owner->moveEditedMembers(true);
    });
    farButton = addButton("Far", 534, 60, [this] {
      if (owner) owner->moveEditedMembers(false);
    });
    nearButton->help = [] {
      return "Move this subpatch near Beekapoo [Shift+F]";
    };
    farButton->help = [] { return "Move the subpatch somewhere quiet [F]"; };
    membersButton = addButton("Members", 606, 100, [this] {
      showMembers = !showMembers;
      if (owner) {
        owner->markMembers = showMembers;
        owner->saveGroup();
      }
    });
    membersButton->pressed = [this] { return showMembers; };
    membersButton->help = [] { return "Mark subpatch modules"; };
    removeButton = addButton("Remove", 718, 96, [this] {
      if (owner) owner->removeSelection();
    });
    removeButton->help = [] { return "Remove selected modules from Beekapoo"; };
    removeButton->visible = false;
    sequenceButton = addButton("Originals LIVE", 1020, 290, [this] {
      if (owner) owner->showSequenceMenu();
    });
    sequenceButton->textSize = 16;
    sequenceButton->caption = [this] {
      return std::string("Originals LIVE | ") +
             (owner ? owner->sequenceStatus() : "");
    };
    sequenceButton->help = [] {
      return "Original modules and the main panel stay live. Step selection "
             "recalls their saved values.";
    };
    preview = new RackEditPreview;
    preview->owner = group;
    addChild(preview);
    layoutToolbar(APP->scene->box.size.x);
  }
  void layoutToolbar(float width) {
    float x = 10, y = 7;
    for (auto* button :
         {done, exposeButton, randomLockButton, panelButton, newModulesButton,
          nearButton, farButton, membersButton, removeButton, setsButton,
          optionsButton, sequenceButton}) {
      if (!button->visible) continue;
      if (x > 10 && x + button->box.size.x + 10 > width) {
        x = 10;
        y += 48;
      }
      button->box.pos = Vec(x, y);
      x += button->box.size.x + 12;
    }
    toolbarHeight = y + 45;
    descriptionPosition = x + 210 < width ? Vec(x + 5, y + 25) : Vec(-1, -1);
  }
  void adoptNewModules() {
    if (!owner) return;
    if (!owner->captureNewModules) {
      for (auto* w : owner->homeRack->getModules())
        if (w->module) known.insert(w->module->id);
      return;
    }
    if (APP->scene->browser->visible || APP->event->getDraggedWidget()) return;
    std::vector<app::ModuleWidget*> added;
    for (auto* w : owner->homeRack->getModules()) {
      if (!w->module ||
          (known.count(w->module->id) &&
           (!admitted.count(w->module->id) || owner->contains(w->module))))
        continue;
      known.insert(w->module->id);
      if (w != owner.get() && !dynamic_cast<Beekapoo*>(w) &&
          !owner->ownedElsewhere(w->module) &&
          !w->module->leftExpander.producerMessage &&
          !w->module->rightExpander.producerMessage)
        added.push_back(w);
    }
    if (added.empty()) return;
    auto* action = owner->beginAction("add modules to Beekapoo");
    for (auto* w : added) {
      owner->members.push_back({w->module->id, w->box.pos, w, nullptr});
      admitted.insert(w->module->id);
    }
    owner->syncBoundaryPorts();
    owner->compactMembers();
    owner->layoutControls();
    owner->saveGroup();
    owner->finishAction(action);
  }
  void finish(bool collapse) {
    if (finished) return;
    finished = true;
    for (auto& highlight : highlights)
      if (highlight) {
        highlight->hide();
        highlight->requestDelete();
      }
    highlights.clear();
    if (!APP->window) {
      // Rack destroys the graphics context before the scene. During that
      // teardown, detach the session without rebuilding fonts, panel artwork,
      // or the viewport as a normal Done/Escape operation would.
      if (owner) {
        owner->editSession = nullptr;
        owner->editPreview = false;
      }
      hide();
      requestDelete();
      return;
    }
    if (owner) {
      owner->editSession = nullptr;
      owner->editPreview = false;
      if (collapse && owner->expanded) {
        if (owner->members.empty()) {
          owner->expanded = false;
          owner->controls->visible = true;
          owner->saveGroup();
        } else
          owner->collapse();
      }
      owner->layoutControls();
      if (collapse) owner->returnHighlight = .8f;
    }
    auto* scroll = APP->scene->rackScroll;
    scroll->setZoom(previousZoom);
    scroll->setGridOffset(previousOffset);
    auto* rack = APP->scene->rack;
    rack->deselectAll();
    for (auto id : previousSelection.selected)
      if (auto* w = rack->getModule(id))
        if (w->visible) rack->select(w, true);
    hide();
    requestDelete();
  }
  template <class F>
  void forControls(F visit) {
    if (!owner) return;
    float zoom = APP->scene->rackScroll->getZoom();
    for (auto& member : owner->members)
      if (auto* w = member.widget.get()) {
        for (auto* p : w->getPorts())
          visit(Rect(p->getAbsoluteOffset(Vec()) - box.pos, p->box.size * zoom),
                p, static_cast<app::ParamWidget*>(nullptr));
        for (auto* p : w->getParams())
          if (p->module && p->paramId >= 0)
            visit(
                Rect(p->getAbsoluteOffset(Vec()) - box.pos, p->box.size * zoom),
                static_cast<app::PortWidget*>(nullptr), p);
      }
  }
  void onHoverKey(const HoverKeyEvent& e) override {
    bool blocked = !owner || owner->keyboardBlocked();
    int mods = e.mods & RACK_MOD_MASK;
    if (e.key == GLFW_KEY_GRAVE_ACCENT &&
        (mods == 0 || mods == GLFW_MOD_SHIFT)) {
      showKeyboard = !blocked && e.action != GLFW_RELEASE;
      if (!blocked) e.consume(this);
      return;
    }
    if (!blocked) {
      ActionButton* button = nullptr;
      if (!mods) {
        switch (e.key) {
          case GLFW_KEY_E:
          case GLFW_KEY_X:
            button = exposeButton;
            break;
          case GLFW_KEY_L:
            button = randomLockButton;
            break;
          case GLFW_KEY_P:
            button = panelButton;
            break;
          case GLFW_KEY_N:
            button = newModulesButton;
            break;
          case GLFW_KEY_F:
            button = farButton;
            break;
        }
      } else if (mods == GLFW_MOD_SHIFT && e.key == GLFW_KEY_F)
        button = nearButton;
      if (button) {
        if (e.action == GLFW_PRESS) button->action();
        e.consume(this);
        return;
      }
    }
    if ((e.action == GLFW_PRESS || e.action == GLFW_REPEAT) &&
        e.key == GLFW_KEY_A && (e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL &&
        !blocked) {
      owner->homeRack->deselectAll();
      for (const auto& member : owner->members)
        if (auto* w = member.widget.get())
          if (w->visible) owner->homeRack->select(w, true);
      e.consume(this);
      return;
    }
    if (e.action == GLFW_PRESS && e.key == GLFW_KEY_ESCAPE &&
        !APP->scene->browser->visible) {
      finish(true);
      e.consume(this);
      return;
    }
    widget::Widget::onHoverKey(e);
  }
  void onButton(const ButtonEvent& e) override {
    widget::Widget::onButton(e);
    if (e.isConsumed() || !owner || e.action != GLFW_PRESS ||
        (e.button != GLFW_MOUSE_BUTTON_LEFT &&
         e.button != GLFW_MOUSE_BUTTON_RIGHT) ||
        e.pos.y < toolbarHeight)
      return;
    if (!exposeMode && !randomLockMode) {
      forControls([&](Rect r, app::PortWidget* port, app::ParamWidget*) {
        if (e.isConsumed() || !port || !r.contains(e.pos) ||
            port->type != engine::Port::INPUT || !owner->exposed(port))
          return;
        // Only visible musical cables belong in Rack's native plug stack.
        // The invisible gain bridge must never be picked up by this gesture.
        owner->withInputCablePresentation(port, [&] {
          ScopedVisiblePortCables hidden(owner->homeRack, port, true);
          std::set<widget::Widget*> before(APP->scene->children.begin(),
                                           APP->scene->children.end());
          port->onButton(e);
          if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.context &&
              e.context->target == port)
            APP->event->setDraggedWidget(port, e.button);
          if (e.button == GLFW_MOUSE_BUTTON_RIGHT)
            for (auto* child : APP->scene->children)
              if (!before.count(child))
                for (auto* sub : child->children)
                  if (auto* menu = dynamic_cast<ui::Menu*>(sub))
                    if (menu->children.size() > 1)
                      if (auto* item = dynamic_cast<ui::MenuItem*>(
                              *std::next(menu->children.begin()))) {
                        auto* safe = new EditedInputDeleteItem;
                        safe->owner = owner;
                        safe->port = port;
                        safe->text = item->text;
                        safe->rightText = item->rightText;
                        safe->disabled = item->disabled;
                        menu->addChildAbove(safe, item);
                        menu->removeChild(item);
                        delete item;
                      }
        });
      });
      return;
    }
    if (e.button != GLFW_MOUSE_BUTTON_LEFT) return;
    if (randomLockMode) {
      forControls([&](Rect r, app::PortWidget*, app::ParamWidget* param) {
        if (!e.isConsumed() && param && r.contains(e.pos)) {
          owner->toggleRandomLock(param->module->id, param->paramId);
          e.consume(this);
        }
      });
      if (!e.isConsumed()) {
        float zoom = APP->scene->rackScroll->getZoom();
        for (const auto& member : owner->members)
          if (auto* w = member.widget.get()) {
            Rect r(w->getAbsoluteOffset(Vec()) - box.pos, w->box.size * zoom);
            if (!e.isConsumed() && r.contains(e.pos)) {
              owner->toggleRandomLock(member.id, -1);
              e.consume(this);
            }
          }
      }
      return;
    }
    forControls([&](Rect r, app::PortWidget* port, app::ParamWidget* param) {
      if (e.isConsumed() || !r.contains(e.pos)) return;
      if (port)
        owner->togglePort(port);
      else
        owner->toggleParameter(param);
      e.consume(this);
    });
  }
  void step() override {
    if (finished) return;
    if (!owner) {
      finish(false);
      return;
    }
    adoptNewModules();
    removeButton->visible = false;
    for (auto* selected : owner->homeRack->getSelected())
      if (owner->contains(selected->module)) removeButton->visible = true;
    for (auto& member : owner->members)
      if (auto* w = member.widget.get()) {
        bool found = false;
        for (auto& highlight : highlights)
          if (highlight && highlight->sourceModule.get() == w) {
            highlight->box.size = w->box.size;
            found = true;
          }
        if (found) continue;
        auto* highlight = new RackEditHighlight;
        highlight->session = this;
        highlight->sourceModule = w;
        highlight->box.size = w->box.size;
        w->addChild(highlight);
        highlights.push_back(highlight);
      }

    box.pos = Vec(0, APP->scene->menuBar->box.getBottom());
    box.size = APP->scene->box.size - box.pos;
    layoutToolbar(box.size.x);
    preview->visible = showPanel;
    float width = std::min(280.f, owner->box.size.x);
    preview->box =
        Rect(Vec(std::max(260.f, box.size.x - width - 14), toolbarHeight + 5),
             Vec(width, width * RACK_GRID_HEIGHT / owner->box.size.x));
    widget::Widget::step();
  }
  void paintTag(const DrawArgs& args, int64_t id, NVGcolor color) {
    const auto& tag = memberTags[id];
    auto& logo = memberLogo;
    nvgBeginPath(args.vg);
    nvgMoveTo(args.vg, tag.corners[0].x, tag.corners[0].y);
    for (int i = 1; i < 4; ++i)
      nvgLineTo(args.vg, tag.corners[i].x, tag.corners[i].y);
    nvgClosePath(args.vg);
    nvgFillColor(args.vg, color);
    nvgFill(args.vg);
    nvgStrokeColor(args.vg, nvgRGB(20, 24, 21));
    nvgStrokeWidth(args.vg, .8f);
    nvgStroke(args.vg);
    nvgTranslate(args.vg, 12, 12);
    nvgRotate(args.vg, tag.tilt);
    nvgSkewX(args.vg, tag.shear);
    nvgScale(args.vg, tag.width / logo.box.size.x,
             tag.height / logo.box.size.y);
    nvgTranslate(args.vg, -logo.box.size.x * .5f, -logo.box.size.y * .5f);
    logo.draw(args);
  }
  void paintKeyboard(const DrawArgs& args) {
    auto font =
        APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
    if (!font) return;
    auto* vg = args.vg;
    nvgSave(vg);
    nvgFontFaceId(vg, font->handle);
    nvgFontSize(vg, 10.5f);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    const std::pair<ActionButton*, const char*> hints[] = {
        {done, "Esc"},      {exposeButton, "E / X"}, {randomLockButton, "L"},
        {panelButton, "P"}, {newModulesButton, "N"}, {nearButton, "Shift+F"},
        {farButton, "F"}};
    for (const auto& hint : hints) {
      const auto& r = hint.first->box;
      float width = nvgTextBounds(vg, 0, 0, hint.second, nullptr, nullptr) + 8;
      float x = r.getRight() - width, y = r.pos.y - 6;
      nvgBeginPath(vg);
      nvgMoveTo(vg, x, y + 1);
      nvgLineTo(vg, x + width - 1, y);
      nvgLineTo(vg, x + width, y + 12);
      nvgLineTo(vg, x + 1, y + 13);
      nvgClosePath(vg);
      nvgFillColor(vg, nvgRGB(236, 223, 139));
      nvgFill(vg);
      nvgStrokeColor(vg, nvgRGB(24, 26, 25));
      nvgStrokeWidth(vg, .7f);
      nvgStroke(vg);
      nvgFillColor(vg, nvgRGB(24, 26, 25));
      nvgText(vg, x + width / 2, y + 6.5f, hint.second, nullptr);
    }
    nvgRestore(vg);
  }
  void draw(const DrawArgs& args) override {
    if (owner && !finished) {
      nvgSave(args.vg);
      nvgIntersectScissor(args.vg, 0, toolbarHeight, box.size.x,
                          box.size.y - toolbarHeight);
      float zoom = APP->scene->rackScroll->getZoom();
      for (auto* module : owner->homeRack->getModules()) {
        if (!module->visible || !module->module || module == owner.get() ||
            owner->contains(module->module))
          continue;
        Vec position = module->getAbsoluteOffset(Vec()) - box.pos;
        nvgBeginPath(args.vg);
        nvgRect(args.vg, position.x, position.y, module->box.size.x * zoom,
                module->box.size.y * zoom);
        nvgFillColor(args.vg, nvgRGBAf(0, 0, 0, preferences().outsideFade));
        nvgFill(args.vg);
      }
      nvgRestore(args.vg);
    }
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, box.size.x, toolbarHeight);
    nvgFillColor(args.vg, nvgRGB(60, 76, 70));
    nvgFill(args.vg);
    auto font = computerscare::type::font();
    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 18);
    nvgFillColor(args.vg, nvgRGB(214, 225, 218));
    const char* description = randomLockMode
                                  ? "Click modules or parameters to lock"
                              : exposeMode ? "Click controls to expose"
                                           : "Editing Beekapoo";
    float textWidth =
        nvgTextBounds(args.vg, 0, 0, description, nullptr, nullptr);
    if (descriptionPosition.x >= 0 &&
        descriptionPosition.x + textWidth < box.size.x - 8)
      nvgText(args.vg, descriptionPosition.x, descriptionPosition.y,
              description, nullptr);
    if (owner) {
      auto mark = [&](app::ModuleWidget* module, NVGcolor color) {
        if (!module || !module->visible || !module->module) return;
        float zoom = APP->scene->rackScroll->getZoom();
        Vec position =
            module->getAbsoluteOffset(Vec(module->box.size.x - 22, -2)) -
            box.pos;
        nvgSave(args.vg);
        nvgIntersectScissor(args.vg, 0, toolbarHeight, box.size.x,
                            box.size.y - toolbarHeight);
        nvgTranslate(args.vg, position.x, position.y);
        nvgScale(args.vg, zoom, zoom);
        paintTag(args, module->module->id, color);
        nvgRestore(args.vg);
      };
      if (showMembers)
        for (auto& member : owner->members)
          mark(member.widget.get(), COLOR_COMPUTERSCARE_YELLOW);
      for (const auto& member : owner->members)
        if (owner->randomLocks.count({member.id, -1}))
          if (auto* w = member.widget.get()) {
            Vec p = w->getAbsoluteOffset(Vec(w->box.size.x - 46, -2)) - box.pos;
            float zoom = APP->scene->rackScroll->getZoom();
            nvgSave(args.vg);
            nvgIntersectScissor(args.vg, 0, toolbarHeight, box.size.x,
                                box.size.y - toolbarHeight);
            nvgTranslate(args.vg, p.x, p.y);
            nvgScale(args.vg, zoom, zoom);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, 2);
            nvgLineTo(args.vg, 22, 0);
            nvgLineTo(args.vg, 24, 22);
            nvgLineTo(args.vg, 1, 24);
            nvgClosePath(args.vg);
            nvgFillColor(args.vg, nvgRGB(169, 177, 173));
            nvgFill(args.vg);
            nvgStrokeColor(args.vg, nvgRGB(24, 29, 26));
            nvgStrokeWidth(args.vg, 1);
            nvgStroke(args.vg);
            drawRandomLock(args.vg, Vec(12, 12), 16);
            nvgRestore(args.vg);
          }
      mark(owner.get(), COLOR_COMPUTERSCARE_GREEN);
    }
    widget::Widget::draw(args);
    if (showKeyboard && owner && !owner->keyboardBlocked()) paintKeyboard(args);
  }
};
void RackEditHighlight::draw(const DrawArgs& args) {
  if (!session || session->finished || !session->owner || !sourceModule ||
      !sourceModule->module)
    return;
  auto* group = session->owner.get();
  auto* module = sourceModule.get();
  if (!group->contains(module->module)) return;
  auto paint = [&](widget::Widget* control, int index, bool selected,
                   bool locked) {
    if (!control->visible) return;
    for (auto* parent = control->parent; parent && parent != module;
         parent = parent->parent)
      if (!parent->visible) return;
    auto* hovered = APP->event->getHoveredWidget();
    bool hovering = false;
    for (auto* p = hovered; p; p = p->parent)
      if (p == control) {
        hovering = true;
        break;
      }
    Rect r(control->getRelativeOffset(Vec(), module), control->box.size);
    if (!r.pos.isFinite() || !r.size.isFinite()) return;
    unsigned seed =
        unsigned(module->module->id) ^ unsigned(index + 1) * 2654435761u;
    // Stable per-control corner variation, without jitter while hovering.
    auto irregular = [&]() {
      seed ^= seed << 13;
      seed ^= seed >> 17;
      seed ^= seed << 5;
      return float(seed % 1024) / 1023.f * 2.8f - 1.4f;
    };
    Vec corners[] = {
        r.pos + Vec(-4 + irregular(), -4 + irregular()),
        Vec(r.getRight() + 4 + irregular(), r.pos.y - 4 + irregular()),
        Vec(r.getRight() + 4 + irregular(), r.getBottom() + 4 + irregular()),
        Vec(r.pos.x - 4 + irregular(), r.getBottom() + 4 + irregular())};
    bool randomLocked = index >= 0 &&
                        dynamic_cast<app::ParamWidget*>(control) &&
                        group->randomLocks.count({module->module->id, index});
    bool mapped = session->exposeMode && (selected || locked);
    hovering &= session->exposeMode || session->randomLockMode;
    if (!hovering && !mapped && !randomLocked) return;
    if (!session->exposeMode && !randomLocked &&
        !dynamic_cast<app::ParamWidget*>(control))
      return;
    // Keep the original control visible beneath a full-size mapping overlay.
    nvgBeginPath(args.vg);
    nvgMoveTo(args.vg, corners[0].x, corners[0].y);
    for (int i = 1; i < 4; ++i) nvgLineTo(args.vg, corners[i].x, corners[i].y);
    nvgClosePath(args.vg);
    NVGcolor color = hovering       ? nvgRGBA(247, 214, 65, 65)
                     : randomLocked ? nvgRGBA(165, 177, 170, 90)
                                    : nvgRGBA(31, 204, 156, 60);
    nvgFillColor(args.vg, color);
    nvgFill(args.vg);
    color.a = .95f;
    nvgStrokeColor(args.vg, color);
    nvgStrokeWidth(args.vg, 1.6f);
    nvgStroke(args.vg);
    if (randomLocked)
      drawRandomLock(args.vg, Vec(r.getRight() - 1, r.pos.y + 1), 10);
  };
  for (auto* p : module->getParams())
    if (p->module && p->paramId >= 0)
      paint(p, p->paramId, group->exposed(p), false);
  for (auto* p : module->getPorts())
    paint(p, 1000 + p->portId + int(p->type) * 100, group->exposed(p),
          group->boundaryPort(p));
}
void Beekapoo::forgetEditedMembers(const std::set<int64_t>& removed) {
  auto* session = dynamic_cast<RackEditSession*>(editSession.get());
  if (!session) return;
  for (auto id : removed) session->admitted.erase(id);
  for (auto it = session->highlights.begin();
       it != session->highlights.end();) {
    auto* highlight = it->get();
    auto* source = highlight ? highlight->sourceModule.get() : nullptr;
    if (!source || removed.count(source->module->id)) {
      if (highlight) {
        highlight->hide();
        highlight->requestDelete();
      }
      it = session->highlights.erase(it);
    } else
      ++it;
  }
}
void Beekapoo::closeEditSession() {
  if (auto* session = dynamic_cast<RackEditSession*>(editSession.get()))
    session->finish(false);
}
void Beekapoo::editSubpatch() {
  setFrontLockMode(false);
  if (editSession) {
    static_cast<RackEditSession*>(editSession.get())->finish(true);
    return;
  }
  // One active editing target, so automatic membership is unambiguous.
  for (auto* w : homeRack->getModules())
    if (auto* group = dynamic_cast<Beekapoo*>(w))
      if (group != this && group->editSession)
        static_cast<RackEditSession*>(group->editSession.get())->finish(true);
  closeView();
  auto* session = new RackEditSession(this);
  repairSeparatedRows();
  revealMembers(false);
  // Restore originals as direct Rack children. Native movement, hotkeys,
  // module menus, selection and browser placement now use their real widgets.
  for (auto& member : members)
    if (auto* holder = member.holder.get()) {
      auto* w = member.widget.get();
      homeRack->select(holder, false);
      auto* parent = holder->parent;
      Vec position = holder->box.pos;
      holder->removeChild(w);
      parent->removeChild(holder);
      w->box.pos = position;
      w->visible = true;
      parent->addChild(w);
      member.holder = nullptr;
      delete holder;
    }
  homeRack->deselectAll();
  expanded = true;
  editPreview = true;
  editSession = session;
  layoutControls();
  APP->scene->addChildBelow(session, APP->scene->browser);
  moveEditedMembers(false, false, hasEditPlacement);
}

void Beekapoo::focusEditedMembers() {
  if (!editSession) return;
  Vec origin(INFINITY, INFINITY);
  Vec end(-INFINITY, -INFINITY);
  for (auto& member : members)
    if (auto* w = member.widget.get()) {
      origin.x = std::min(origin.x, w->box.pos.x);
      origin.y = std::min(origin.y, w->box.pos.y);
      end.x = std::max(end.x, w->box.getRight());
      end.y = std::max(end.y, w->box.getBottom());
    }
  if (!origin.isFinite()) {
    origin = box.pos + Vec(0, 4 * RACK_GRID_HEIGHT);
    end = origin;
  }
  auto* scroll = APP->scene->rackScroll;
  // Center below the editing toolbar, without changing the user's zoom.
  auto* session = static_cast<RackEditSession*>(editSession.get());
  session->layoutToolbar(scroll->box.size.x);
  Vec viewportCenter(scroll->box.size.x * .5f,
                     (scroll->box.size.y + session->toolbarHeight) * .5f);
  Vec center = (origin + end) * .5f;
  scroll->setGridOffset(
      (center - viewportCenter / scroll->getZoom() - RACK_OFFSET) /
      RACK_GRID_SIZE);
}
void Beekapoo::moveEditedMembers(bool near, bool recordHistory,
                                 bool keepPosition) {
  if (!editSession) return;
  auto* action = recordHistory ? beginAction(near ? "move subpatch near"
                                                  : "move subpatch far")
                               : nullptr;
  Vec minimum(INFINITY, INFINITY), maximum(-INFINITY, -INFINITY);
  for (auto& member : members)
    if (auto* w = member.widget.get()) {
      minimum.x = std::min(minimum.x, w->box.pos.x);
      minimum.y = std::min(minimum.y, w->box.pos.y);
      maximum.x = std::max(maximum.x, w->box.getRight());
      maximum.y = std::max(maximum.y, w->box.getBottom());
    }
  bool empty = !minimum.isFinite();
  Vec origin(
      near ? std::ceil((box.getRight() + RACK_GRID_WIDTH) / RACK_GRID_WIDTH) *
                 RACK_GRID_WIDTH
           : std::floor(box.pos.x / RACK_GRID_WIDTH) * RACK_GRID_WIDTH,
      (std::floor(box.pos.y / RACK_GRID_HEIGHT) + (near ? 0 : 4)) *
          RACK_GRID_HEIGHT);
  if (keepPosition && !empty) origin = minimum;
  if (!empty) {
    auto outsideModules = homeRack->getModules();
    if (near && !keepPosition) {
      Vec size = maximum - minimum;
      float best = INFINITY;
      // Search away from each side, rather than always pushing right.
      for (int side = 0; side < 4; ++side) {
        Vec candidate = origin;
        if (side == 1) candidate.x = box.pos.x - size.x - RACK_GRID_WIDTH;
        if (side >= 2) {
          candidate.x = box.pos.x;
          candidate.y = side == 2 ? box.pos.y - size.y : box.getBottom();
        }
        bool collision;
        do {
          collision = false;
          for (auto* outside : outsideModules)
            if (outside->visible && !contains(outside->module) &&
                Rect(candidate, size).intersects(outside->box)) {
              if (side == 0)
                candidate.x = outside->box.getRight() + RACK_GRID_WIDTH;
              else if (side == 1)
                candidate.x = outside->box.pos.x - size.x - RACK_GRID_WIDTH;
              else if (side == 2)
                candidate.y = outside->box.pos.y - size.y;
              else
                candidate.y = outside->box.getBottom();
              candidate = (candidate / RACK_GRID_SIZE).round() * RACK_GRID_SIZE;
              collision = true;
              break;
            }
        } while (collision);
        Vec distance = candidate + size * .5f - box.getCenter();
        float score = distance.x * distance.x + distance.y * distance.y;
        if (score < best) {
          best = score;
          origin = candidate;
        }
      }
    }
    bool blocked;
    do {
      blocked = false;
      for (auto* outside : outsideModules)
        if (outside->visible && !contains(outside->module) &&
            Rect(origin, maximum - minimum).intersects(outside->box)) {
          if (near && !keepPosition)
            origin.x = std::ceil((outside->box.getRight() + RACK_GRID_WIDTH) /
                                 RACK_GRID_WIDTH) *
                       RACK_GRID_WIDTH;
          else
            origin.y += RACK_GRID_HEIGHT;
          blocked = true;
          break;
        }
    } while (blocked);
    Vec delta = origin - minimum;
    for (auto& member : members)
      if (auto* w = member.widget.get()) w->box.pos += delta;
    // All members are already in their non-colliding positions. Refresh native
    // expander neighbors through Rack's public placement API.
    for (auto& member : members)
      if (auto* w = member.widget.get()) {
        homeRack->requestModulePos(w, w->box.pos);
        break;
      }
    syncMemberPositions();
  }
  hasEditPlacement = true;
  saveGroup();
  focusEditedMembers();
  if (action) finishAction(action);
}

json_t* Beekapoo::editSessionState() {
  auto* session = dynamic_cast<RackEditSession*>(editSession.get());
  if (!session || session->finished) return nullptr;
  auto* saved = json_pack(
      "{s:f,s:f,s:f,s:b,s:b}", "x", double(session->previousOffset.x), "y",
      double(session->previousOffset.y), "zoom", double(session->previousZoom),
      "panel", session->showPanel, "expose", session->exposeMode);
  json_object_set_new(saved, "members", json_boolean(session->showMembers));
  json_object_set_new(saved, "randomLock",
                      json_boolean(session->randomLockMode));
  auto* selected = json_array();
  for (auto id : session->previousSelection.selected)
    json_array_append_new(selected, json_integer(id));
  json_object_set_new(saved, "selected", selected);
  return saved;
}
void Beekapoo::resumeEditSession(json_t* saved) {
  auto* session = new RackEditSession(this);
  Vec offset(json_number_value(json_object_get(saved, "x")),
             json_number_value(json_object_get(saved, "y")));
  double zoom = json_number_value(json_object_get(saved, "zoom"));
  if (offset.isFinite()) session->previousOffset = offset;
  if (std::isfinite(zoom) && zoom > 0)
    session->previousZoom = clamp(float(zoom), .1f, 4.f);
  if (auto* panel = json_object_get(saved, "panel"))
    session->showPanel = !json_is_false(panel);
  getModule<Module>()->showEditPanel.store(session->showPanel);
  session->exposeMode = json_is_true(json_object_get(saved, "expose"));
  session->randomLockMode = json_is_true(json_object_get(saved, "randomLock"));
  session->randomLockMode |=
      json_is_true(json_object_get(saved, "sequenceLock"));
  if (session->randomLockMode) session->exposeMode = false;
  session->showMembers = !json_is_false(json_object_get(saved, "members"));
  session->previousSelection.selected.clear();
  auto* selected = json_object_get(saved, "selected");
  for (size_t i = 0; i < json_array_size(selected); ++i)
    session->previousSelection.selected.push_back(
        json_integer_value(json_array_get(selected, i)));
  for (auto& member : members)
    if (auto* holder = member.holder.get()) {
      auto* w = member.widget.get();
      auto* parent = holder->parent;
      Vec position = holder->box.pos;
      homeRack->select(holder, false);
      holder->removeChild(w);
      parent->removeChild(holder);
      w->box.pos = position;
      w->visible = true;
      parent->addChild(w);
      member.holder = nullptr;
      delete holder;
    }
  editSession = session;
  layoutControls();
  APP->scene->addChildBelow(session, APP->scene->browser);
  saveGroup();
}
