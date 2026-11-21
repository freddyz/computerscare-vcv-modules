#pragma once
struct BeekImportChoice {
  std::string title, description;
  int64_t owner = -1;
  bool convertAudio = false, enabled = true;
  BeekImportChoice(std::string label, std::string help, int64_t id = -1,
                   bool audio = false, bool available = true)
      : title(std::move(label)),
        description(std::move(help)),
        owner(id),
        convertAudio(audio),
        enabled(available) {}
};
// Modal import choices belong to the scene, not to the mouse/context menu.
struct BeekImportDialog : ui::MenuOverlay {
  struct Button : computerscare::type::Button {
    BeekImportDialog* dialog = nullptr;
    std::function<void()> action;
    void onAction(const ActionEvent&) override { action(); }
    void onSelect(const SelectEvent& e) override { e.consume(dialog); }
  };
  struct Choice : Button {
    int index = 0;
    void draw(const DrawArgs& args) override {
      const auto& choice = dialog->choices[index];
      bool selected = dialog->selected == index;
      bool hovered = APP->event->getHoveredWidget() == this;
      nvgSave(args.vg);
      if (!choice.enabled) nvgGlobalAlpha(args.vg, .45f);
      nvgBeginPath(args.vg);
      nvgRect(args.vg, 1, 1, box.size.x - 2, box.size.y - 2);
      nvgFillColor(args.vg, selected  ? nvgRGB(157, 202, 185)
                            : hovered ? nvgRGB(199, 213, 205)
                                      : nvgRGB(192, 202, 196));
      nvgFill(args.vg);
      nvgStrokeColor(args.vg,
                     selected ? nvgRGB(31, 95, 77) : nvgRGB(112, 130, 121));
      nvgStrokeWidth(args.vg, selected ? 2.f : 1.f);
      nvgStroke(args.vg);
      nvgBeginPath(args.vg);
      nvgCircle(args.vg, 18, 21, 6);
      nvgStrokeColor(args.vg, nvgRGB(30, 56, 45));
      nvgStrokeWidth(args.vg, 1.3f);
      nvgStroke(args.vg);
      if (selected) {
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, 18, 21, 3);
        nvgFillColor(args.vg, nvgRGB(30, 91, 70));
        nvgFill(args.vg);
      }
      nvgFontFaceId(args.vg, computerscare::type::handle());
      nvgFillColor(args.vg, nvgRGB(24, 39, 31));
      nvgFontSize(args.vg, 16);
      nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
      nvgTextBox(args.vg, 35, 12, box.size.x - 48, choice.title.c_str(),
                 nullptr);
      nvgFontSize(args.vg, 13);
      nvgTextBox(args.vg, 35, box.size.x < 430 ? 53 : 39, box.size.x - 48,
                 choice.description.c_str(), nullptr);
      nvgRestore(args.vg);
    }
  };
  struct Panel : widget::OpaqueWidget {
    BeekImportDialog* dialog;
    phlooper::WarpedBlock* frame;
    explicit Panel(BeekImportDialog* owner) : dialog(owner) {
      frame = new phlooper::WarpedBlock;
      frame->configure(Rect(Vec(), Vec(620, 350)), 5, 1.2f, 1053,
                       nvgRGB(184, 198, 190));
      addChild(frame);
    }
    void onSelect(const SelectEvent& e) override { e.consume(dialog); }
    void draw(const DrawArgs& args) override {
      widget::OpaqueWidget::draw(args);
      nvgSave(args.vg);
      nvgFontFaceId(args.vg, computerscare::type::handle());
      nvgFillColor(args.vg, nvgRGB(23, 39, 30));
      nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
      nvgFontSize(args.vg, 23);
      nvgText(args.vg, 22, 17, dialog->title.c_str(), nullptr);
      nvgSave(args.vg);
      nvgIntersectScissor(args.vg, 22, 47, box.size.x - 44, 22);
      nvgFontSize(args.vg, 14);
      nvgText(args.vg, 22, 49, dialog->filename.c_str(), nullptr);
      nvgRestore(args.vg);
      nvgRestore(args.vg);
    }
  };
  std::string title, filename;
  std::vector<BeekImportChoice> choices;
  std::function<void(int64_t, bool)> apply;
  Panel* panel;
  ui::ScrollWidget* scroll;
  std::vector<Choice*> rows;
  Button *loadButton, *cancelButton;
  int selected = -1;
  bool closed = false;
  bool focused = false;
  WeakPtr<widget::Widget> previousFocus;
  BeekImportDialog(std::string heading, std::string file,
                   std::vector<BeekImportChoice> options,
                   std::function<void(int64_t, bool)> callback)
      : title(std::move(heading)),
        filename(std::move(file)),
        choices(std::move(options)),
        apply(std::move(callback)) {
    bgColor = nvgRGBA(0, 0, 0, 150);
    panel = new Panel(this);
    addChild(panel);
    scroll = new ui::ScrollWidget;
    panel->addChild(scroll);
    for (size_t i = 0; i < choices.size(); ++i) {
      if (selected < 0 && choices[i].enabled) selected = int(i);
      auto* row = new Choice;
      row->dialog = this;
      row->index = int(i);
      row->action = [this, i] { choose(int(i)); };
      rows.push_back(row);
      scroll->container->addChild(row);
    }
    loadButton = new Button;
    loadButton->dialog = this;
    loadButton->text = "Load";
    loadButton->action = [this] { submit(); };
    panel->addChild(loadButton);
    cancelButton = new Button;
    cancelButton->dialog = this;
    cancelButton->text = "Cancel";
    cancelButton->action = [this] { dismiss(); };
    panel->addChild(cancelButton);
  }
  void choose(int index) {
    if (closed || index < 0 || index >= int(choices.size()) ||
        !choices[index].enabled)
      return;
    selected = index;
    scroll->scrollTo(rows[index]->box);
  }
  void dismiss() {
    closed = true;
    hide();
    requestDelete();
    restoreFocus();
  }
  void restoreFocus() {
    if (!focused || !APP || !APP->event) return;
    auto* selectedWidget = APP->event->getSelectedWidget();
    for (auto* w = selectedWidget; w; w = w->parent)
      if (w == this) {
        APP->event->setSelectedWidget(previousFocus.get());
        break;
      }
    focused = false;
  }
  ~BeekImportDialog() override { restoreFocus(); }
  void submit() {
    if (closed || selected < 0 || !choices[selected].enabled) return;
    auto option = choices[selected];
    auto callback = apply;
    dismiss();
    callback(option.owner, option.convertAudio);
  }
  void step() override {
    if (!parent || closed) return;
    if (!focused) {
      previousFocus = APP->event->getSelectedWidget();
      focused = true;
      APP->event->setHoveredWidget(nullptr);
    }
    APP->event->setSelectedWidget(this);
    box = parent->box.zeroPos();
    float width = std::min(620.f, std::max(160.f, box.size.x - 40));
    float rowHeight = width < 490 ? 114.f : 84.f;
    float height = std::min(box.size.y - 32, 146.f + rowHeight * rows.size());
    panel->box =
        Rect((box.size - Vec(width, height)) * .5f, Vec(width, height));
    panel->frame->box = Rect(Vec(), panel->box.size);
    scroll->box =
        Rect(Vec(20, 80), Vec(width - 40, std::max(0.f, height - 140)));
    for (size_t i = 0; i < rows.size(); ++i)
      rows[i]->box =
          Rect(Vec(0, i * rowHeight), Vec(width - 58, rowHeight - 7));
    cancelButton->box = Rect(Vec(width - 216, height - 49), Vec(90, 28));
    loadButton->box = Rect(Vec(width - 114, height - 49), Vec(90, 28));
    widget::Widget::step();
  }
  void onButton(const ButtonEvent& e) override {
    // Clicking the backdrop blocks Rack but does not dismiss this dialog.
    widget::OpaqueWidget::onButton(e);
    if (!e.isConsumed()) e.consume(this);
  }
  template <class KeyEvent>
  void handleKey(const KeyEvent& e) {
    if (e.action == GLFW_PRESS || e.action == GLFW_REPEAT) {
      if (e.key == GLFW_KEY_ESCAPE)
        dismiss();
      else if (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER ||
               e.key == GLFW_KEY_SPACE)
        submit();
      else if (!choices.empty() &&
               (e.key == GLFW_KEY_DOWN || e.key == GLFW_KEY_UP ||
                e.key == GLFW_KEY_TAB)) {
        int direction = e.key == GLFW_KEY_UP || (e.key == GLFW_KEY_TAB &&
                                                 (e.mods & GLFW_MOD_SHIFT))
                            ? -1
                            : 1;
        for (int n = 1; n <= int(choices.size()); ++n) {
          int next = (selected + direction * n + int(choices.size()) * 2) %
                     int(choices.size());
          if (choices[next].enabled) {
            choose(next);
            break;
          }
        }
      }
    }
    e.consume(this);
  }
  void onHoverKey(const HoverKeyEvent& e) override { handleKey(e); }
  void onSelectKey(const SelectKeyEvent& e) override { handleKey(e); }
  void onSelectText(const SelectTextEvent& e) override { e.consume(this); }
};
