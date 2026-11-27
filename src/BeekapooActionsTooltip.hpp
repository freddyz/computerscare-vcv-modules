// Included after ActionJack, inside namespace beekapoo.
// Decorate Rack's own tooltip so voltage formatting and cable details stay
// native.
struct ActionsTooltip : ui::Tooltip {
  WeakPtr<ui::Tooltip> native;
  WeakPtr<app::PortWidget> port;
  static const char* channelLabel(int channel) {
    const char* labels[] = {"wig exp", "rnd exp",   "wig all",
                            "rnd all", "new patch", "grow",
                            "clock",   "reset",     "scan"};
    return channel >= 0 && channel < 9 ? labels[channel] : "unused";
  }
  void step() override {
    if (!native || !port || !port->module) {
      hide();
      requestDelete();
      return;
    }
    native->step();
    text = native->text;
    auto* info = port->getPortInfo();
    std::string heading = info->getFullName();
    auto description = info->getDescription();
    if (!description.empty()) heading += "\n" + description;
    int channels = port->getPort()->getChannels();
    if (text.compare(0, heading.size(), heading) == 0) {
      std::string labeled = heading;
      size_t cursor = heading.size();
      for (int i = 0; i < 16; ++i) {
        if (i >= channels) {
          labeled += "\n" + std::to_string(i + 1) + " [" + channelLabel(i) +
                     "]: no connection";
          continue;
        }
        if (cursor >= text.size() || text[cursor] != '\n') break;
        size_t end = text.find('\n', cursor + 1);
        if (end == std::string::npos) end = text.size();
        auto voltage = text.substr(cursor + 1, end - cursor - 1);
        if (channels > 1) {
          auto separator = voltage.find(": ");
          if (separator == std::string::npos) break;
          voltage.erase(0, separator + 2);
        }
        labeled += "\n" + std::to_string(i + 1) + " [" + channelLabel(i) +
                   "]: " + voltage;
        cursor = end;
      }
      text = labeled + text.substr(cursor);
    }
    ui::Tooltip::step();
    box.pos = port->getAbsoluteOffset(port->box.size).round();
    if (parent) box = box.nudge(parent->box.zeroPos());
  }
};
struct ActionsCvJack : ActionJack {
  WeakPtr<ActionsTooltip> tip;
  void clearTip() {
    if (tip) {
      tip->native = nullptr;
      tip->port = nullptr;
      tip->hide();
      tip->requestDelete();
      tip = nullptr;
    }
  }
  ~ActionsCvJack() override { clearTip(); }
  void nativeAction(const std::function<void()>& action) {
    std::set<widget::Widget*> before(APP->scene->children.begin(),
                                     APP->scene->children.end());
    action();
    for (auto* child : APP->scene->children)
      if (!before.count(child))
        if (auto* native = dynamic_cast<ui::Tooltip*>(child)) {
          clearTip();
          auto* decorated = new ActionsTooltip;
          decorated->native = native;
          decorated->port = this;
          native->hide();
          APP->scene->addChild(decorated);
          tip = decorated;
          break;
        }
  }
  void onEnter(const EnterEvent& e) override {
    nativeAction([&] { ActionJack::onEnter(e); });
  }
  void onLeave(const LeaveEvent& e) override {
    clearTip();
    ActionJack::onLeave(e);
  }
  void onDragEnter(const DragEnterEvent& e) override {
    nativeAction([&] { ActionJack::onDragEnter(e); });
  }
  void onDragLeave(const DragLeaveEvent& e) override {
    clearTip();
    ActionJack::onDragLeave(e);
  }
  void onDragDrop(const DragDropEvent& e) override {
    nativeAction([&] { ActionJack::onDragDrop(e); });
  }
  void step() override {
    if (tip) {
      bool shown = visible;
      for (auto* p = parent; p && shown; p = p->parent) shown = p->visible;
      if (!shown || !settings::tooltips || !tip->native ||
          (APP->event->getHoveredWidget() != this &&
           APP->event->getDragHoveredWidget() != this)) {
        clearTip();
        destroyTooltip();
      }
    }
    ActionJack::step();
  }
};
