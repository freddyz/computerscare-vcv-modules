#pragma once

inline beekapoo::BeekapooRackHooks* findBeekRackHooks() {
  for (auto* child : APP->scene->children)
    if (auto* hooks = dynamic_cast<beekapoo::BeekapooRackHooks*>(child))
      return hooks;
  return nullptr;
}

template <class Require>
void testBeekMenuStartup(Require require) {
#ifdef ARCH_MAC
  require(APP->engine->getModuleIds().empty(),
          "Menu startup test must begin with an empty patch");
  if (auto* hooks = findBeekRackHooks()) {
    APP->scene->removeChild(hooks);
    delete hooks;
  }
  auto* context = APP;
  contextSet(nullptr);
  for (int i = 0; i < 4; ++i) installBeekapooRackHooks();
  contextSet(context);
  // Reproduce plugin init before Rack creates its Context. Only native UI
  // events may complete installation: no module widget or manual retry.
  for (int i = 0; i < 100 && !findBeekRackHooks(); ++i) {
    glfwPollEvents();
    system::sleep(.005);
  }
  require(findBeekRackHooks(),
          "An empty patch must get selection support after UI startup");
  for (int i = 0; i < 4; ++i) installBeekapooRackHooks();
  int count = 0;
  for (auto* child : APP->scene->children)
    count += dynamic_cast<beekapoo::BeekapooRackHooks*>(child) != nullptr;
  require(count == 1, "Deferred and repeated installation must not add hooks twice");
  std::puts("Beek selection menu: deferred startup without any module passed");
#endif
}

template <class Require>
void testBeekEditMenu(const std::string& screenshot, float scale,
                     bool selectedGroup, Require require) {
  ui::Button* edit = nullptr;
  std::function<void(widget::Widget*)> findEdit = [&](widget::Widget* root) {
    if (auto* button = dynamic_cast<ui::Button*>(root))
      if (button->text == string::translate("MenuBar.edit")) edit = button;
    for (auto* child : root->children) findEdit(child);
  };
  findEdit(APP->scene->menuBar);
  require(edit, "Rack's native Edit button must exist");
  widget::Widget::ActionEvent action;
  edit->onAction(action);
  ui::MenuOverlay* overlay = nullptr;
  ui::Menu* menu = nullptr;
  for (auto* child : APP->scene->children)
    if (auto* candidate = dynamic_cast<ui::MenuOverlay*>(child)) {
      overlay = candidate;
      for (auto* content : candidate->children)
        if (auto* m = dynamic_cast<ui::Menu*>(content)) menu = m;
    }
  require(menu && overlay, "Edit must open Rack's actual menu");
  auto* hooks = findBeekRackHooks();
  require(hooks, "Selection support must already be installed");
  hooks->step();
  // Visiting another menu must not append Create twice or wrap actions again.
  ui::Menu unrelated;
  unrelated.addChild(createMenuItem("Other action", "", [] {}));
  hooks->wrapMenus(&unrelated);
  hooks->step();
  int creates = 0, removes = 0, duplicates = 0, disconnects = 0;
  for (auto* child : menu->children) {
    if (auto* item = dynamic_cast<beekapoo::CreateBeekapooMenuItem*>(child)) {
      ++creates;
      require(!item->disabled, "Eligible selection must enable Create Beek-A-Poo");
    }
    removes += dynamic_cast<beekapoo::GroupDeleteMenuItem*>(child) != nullptr;
    duplicates += dynamic_cast<beekapoo::GroupDuplicateMenuItem*>(child) != nullptr;
    disconnects += dynamic_cast<beekapoo::GroupDisconnectMenuItem*>(child) != nullptr;
  }
  require(creates == int(APP->scene->rack->getSelected().size() >= 2),
          "Edit must offer Create exactly once for a multi-selection");
  require(removes == int(selectedGroup) && duplicates == 2 * int(selectedGroup) &&
              disconnects == int(selectedGroup),
          "Edit must preserve Beek contents for native delete, duplicate and disconnect");
  if (!screenshot.empty()) {
    menu->step();
    auto position = menu->box.pos;
    overlay->removeChild(menu);
    menu->box.pos = Vec();
    capture(menu, screenshot, scale, false);
    menu->box.pos = position;
    overlay->addChild(menu);
  }
  APP->scene->removeChild(overlay);
  delete overlay;
}
