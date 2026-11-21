#pragma once

// A single, immediately focused native Rack field. ZoomWidget scales both the
// drawing and mouse coordinates, preserving Rack's selection/clipboard logic.
struct BeekNameEditor : ui::MenuOverlay {
  struct Field : computerscare::type::Field {
    BeekNameEditor* editor = nullptr;
    void onSelectKey(const SelectKeyEvent& e) override {
      if (e.action == GLFW_PRESS &&
          (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER ||
           e.key == GLFW_KEY_ESCAPE)) {
        e.consume(this);
        editor->close(e.key != GLFW_KEY_ESCAPE);
        return;
      }
      ui::TextField::onSelectKey(e);
    }
  };
  struct Panel : widget::OpaqueWidget {
    BeekNameEditor* editor;
    phlooper::WarpedBlock* frame;
    explicit Panel(BeekNameEditor* owner) : editor(owner) {
      frame = new phlooper::WarpedBlock;
      frame->configure(Rect(), 3, .8f, 1079, nvgRGB(204, 204, 204));
      addChild(frame);
    }
    void draw(const DrawArgs& args) override {
      widget::OpaqueWidget::draw(args);
      nvgFontFaceId(args.vg, computerscare::type::handle());
      nvgFillColor(args.vg,
                   editor->dark ? nvgRGB(235, 235, 235) : nvgRGB(32, 32, 32));
      nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
      nvgFontSize(args.vg, 19);
      nvgText(args.vg, 16, 12, "Name", nullptr);
    }
  };
  WeakPtr<Beekapoo> owner;
  WeakPtr<widget::Widget> previousFocus;
  Panel* panel;
  widget::ZoomWidget* zoom;
  Field* field;
  ActionButton *saveButton, *cancelButton;
  bool dark = false, closed = false, focused = false;
  Vec anchor;

  explicit BeekNameEditor(Beekapoo* module) : owner(module) {
    dark = module->darkPanel();
    previousFocus = APP->event->getSelectedWidget();
    anchor = module->getRelativeOffset(module->headerArtworkArea.getCenter(),
                                       APP->scene);
    bgColor = nvgRGBA(0, 0, 0, 65);
    panel = new Panel(this);
    panel->frame->faceColor = dark ? nvgRGB(48, 48, 48) : nvgRGB(204, 204, 204);
    addChild(panel);
    zoom = new widget::ZoomWidget;
    zoom->setZoom(1.8f);
    panel->addChild(zoom);
    field = new Field;
    field->editor = this;
    field->setText(module->getModule<Module>()->displayPatchName(
        module->appearance.modules));
    field->placeholder = "Beekapoo";
    field->selectAll();
    zoom->addChild(field);
    auto button = [&](const char* label, bool save) {
      auto* result = new ActionButton;
      result->text = label;
      result->textSize = 16;
      result->configure(96, 29, 1.5f, 1.f, .6f, save ? 1081 : 1083);
      result->faceColor = dark ? nvgRGB(83, 83, 83) : nvgRGB(228, 228, 228);
      result->textColor = dark ? nvgRGB(239, 239, 239) : nvgRGB(30, 30, 30);
      result->help = [save] {
        return save ? "Save name [Enter]" : "Cancel [Esc]";
      };
      result->action = [this, save] { close(save); };
      panel->addChild(result);
      return result;
    };
    saveButton = button("Save", true);
    cancelButton = button("Cancel", false);
  }
  void restoreFocus() {
    if (!focused || !APP || !APP->event) return;
    for (auto* w = APP->event->getSelectedWidget(); w; w = w->parent)
      if (w == this) {
        APP->event->setSelectedWidget(previousFocus.get());
        break;
      }
    focused = false;
  }
  ~BeekNameEditor() override { restoreFocus(); }
  void close(bool save) {
    if (closed) return;
    closed = true;
    if (save && owner) owner->rename(field->text);
    if (owner) owner->nameEditor = nullptr;
    restoreFocus();
    hide();
    requestDelete();
  }
  void step() override {
    if (closed || !parent) return;
    if (!owner) {
      close(false);
      return;
    }
    box = parent->box.zeroPos();
    float width = std::min(560.f, std::max(160.f, box.size.x - 24));
    Vec size(width, 146);
    Vec position = anchor - Vec(width * .5f, 18);
    position.x =
        clamp(position.x, 12.f, std::max(12.f, box.size.x - width - 12));
    position.y =
        clamp(position.y, 12.f, std::max(12.f, box.size.y - size.y - 12));
    panel->box = Rect(position, size);
    panel->frame->box = Rect(Vec(), size);
    zoom->box = Rect(Vec(16, 43), Vec(width - 36, 47));
    field->box = Rect(Vec(), zoom->box.size / zoom->getZoom());
    float buttonWidth = std::min(96.f, (width - 44) * .5f);
    saveButton->box.size.x = cancelButton->box.size.x = buttonWidth;
    saveButton->box.pos = Vec(width - buttonWidth - 21, 104);
    cancelButton->box.pos = Vec(width - buttonWidth * 2 - 29, 104);
    if (!focused) {
      APP->event->setSelectedWidget(field);
      APP->event->setHoveredWidget(nullptr);
      focused = true;
    }
    widget::Widget::step();
  }
  void onButton(const ButtonEvent& e) override {
    widget::OpaqueWidget::onButton(e);
    if (e.action == GLFW_PRESS && (!e.isConsumed() || e.getTarget() == this))
      close(true);
    if (!e.isConsumed()) e.consume(this);
  }
  void onAction(const ActionEvent&) override { close(false); }
};

void Beekapoo::editName() {
  if (!module) return;
  if (auto* open = dynamic_cast<BeekNameEditor*>(nameEditor.get())) {
    APP->event->setSelectedWidget(open->field);
    return;
  }
  auto* editor = new BeekNameEditor(this);
  nameEditor = editor;
  APP->scene->addChild(editor);
  editor->step();
}
