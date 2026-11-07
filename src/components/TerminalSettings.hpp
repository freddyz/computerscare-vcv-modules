#pragma once
#include <functional>
#include <string>
#include <vector>

#include "../ComputerscareTextEditorLayout.hpp"
#include "../ComputerscareTextEditorTheme.hpp"
#include "../ComputerscareTextEditorViewport.hpp"

namespace computerscare {
// A setting has one live binding, shared by Rack menus and terminal screens.
struct TerminalSetting {
  std::string name, help, shortName, body, copyText;
  std::vector<TerminalSetting> children;
  std::function<bool()> checked;
  std::function<void()> action;
  std::function<std::string()> value;
};
inline void appendTerminalSettings(
    rack::ui::Menu* menu, const std::vector<TerminalSetting>& settings) {
  for (const auto& entry : settings) {
    if (!entry.children.empty())
      menu->addChild(
          rack::createSubmenuItem(entry.name, entry.value ? entry.value() : "",
                                  [entry](rack::ui::Menu* sub) {
                                    appendTerminalSettings(sub, entry.children);
                                  }));
    else if (entry.checked)
      menu->addChild(rack::createCheckMenuItem(entry.name, "", entry.checked,
                                               entry.action));
    else
      menu->addChild(rack::createMenuItem(entry.name, "", entry.action));
  }
}

struct TerminalSettings : rack::widget::OpaqueWidget {
  struct Screen {
    std::string name;
    std::vector<TerminalSetting> entries;
    int selected;
    std::string body;
    int scroll = 0;
    Screen(std::string title, std::vector<TerminalSetting> items, int row = 0)
        : name(std::move(title)), entries(std::move(items)), selected(row) {}
  };
  std::vector<Screen> screens;
  std::function<void()> closed;
  std::function<void(const DrawArgs&)> frame;
  const ComputerscareTextEditorStyle* textStyle = nullptr;
  std::function<std::string()> sourceText;
  std::function<const text_editor::Theme&()> theme;
  std::string copiedText;
  double copiedAt = -10;
  bool opened = false, leaving = false;
  double transitionStart = -1;
  static constexpr double transitionSeconds = .5;
  std::string before;
  int firstRow = 0;
  unsigned transitionSerial = 0;
  // Preview fixtures may freeze the real animation at a representative frame.
  float previewProgress = -1.f;

  float transitionProgress() const {
    return previewProgress >= 0
               ? previewProgress
               : float((rack::system::getTime() - transitionStart) /
                       transitionSeconds);
  }
  bool transitioning() const {
    return transitionStart >= 0 && transitionProgress() < 1.f;
  }
  void open(const std::vector<TerminalSetting>& entries) {
    if (opened || entries.empty()) return;
    ++transitionSerial;
    before = sourceText ? sourceText() : "";
    screens = {{"Settings", entries, 0}};
    firstRow = 0;
    opened = true;
    leaving = false;
    visible = true;
    transitionStart = rack::system::getTime();
    APP->event->setSelectedWidget(this);
  }
  void leave() {
    if (!opened || leaving) return;
    ++transitionSerial;
    before = screenText();
    leaving = true;
    transitionStart = rack::system::getTime();
  }
  void step() override {
    if (leaving && !transitioning()) {
      opened = false;
      leaving = false;
      visible = false;
      transitionStart = -1;
      if (closed) closed();
    }
    rack::widget::OpaqueWidget::step();
  }
  const ComputerscareTextEditorStyle& style() const {
    static const ComputerscareTextEditorStyle fallback;
    return textStyle ? *textStyle : fallback;
  }
  Vec textScale() const {
    return ComputerscareTextEditorViewport(box.size, style()).scale;
  }
  Vec textSize() const {
    return ComputerscareTextEditorViewport(box.size, style()).logicalSize();
  }
  void applyFont(NVGcontext* vg) const {
    auto font = APP->window->loadFont(rack::asset::system(style().fontPath));
    if (!font) font = APP->window->uiFont;
    if (font) nvgFontFaceId(vg, font->handle);
    nvgFontSize(vg, std::max(6.f, style().fontSize));
    nvgTextLetterSpacing(vg, style().letterSpacing);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
  }
  ComputerscareTextEditorMetrics metrics() const {
    auto* vg = APP->window->vg;
    nvgSave(vg);
    applyFont(vg);
    auto result =
        ComputerscareTextEditorLayout::build(vg, "", textSize(), style())
            .metrics;
    nvgRestore(vg);
    return result;
  }
  int capacity() const {
    return std::max(1, int(textSize().y / metrics().lineHeight) - 5);
  }
  std::string fitText(NVGcontext* vg, std::string text, float width) const {
    if (nvgTextBounds(vg, 0, 0, text.c_str(), nullptr, nullptr) <= width)
      return text;
    while (!text.empty() && nvgTextBounds(vg, 0, 0, (text + "...").c_str(),
                                          nullptr, nullptr) > width)
      text.pop_back();
    return text + "...";
  }
  void keepVisible(int visibleRows = -1) {
    if (screens.empty()) return;
    if (visibleRows < 0) visibleRows = capacity();
    if (!screens.back().body.empty()) return;
    int selected = screens.back().selected;
    if (selected < firstRow) firstRow = selected;
    if (selected >= firstRow + visibleRows)
      firstRow = selected - visibleRows + 1;
  }
  void activate() {
    if (screens.empty() || transitioning() || leaving) return;
    auto entry = screens.back().entries.at(screens.back().selected);
    if (!entry.children.empty()) {
      screens.push_back({entry.name, entry.children, 0});
      firstRow = 0;
    } else if (!entry.body.empty()) {
      screens.push_back({entry.name, {}, 0});
      screens.back().body = entry.body;
      firstRow = 0;
    } else if (!entry.copyText.empty()) {
      glfwSetClipboardString(APP->window->win, entry.copyText.c_str());
      copiedText = entry.copyText;
      copiedAt = rack::system::getTime();
    } else if (entry.action)
      entry.action();
  }
  void back() {
    if (transitioning() || leaving) return;
    if (screens.size() > 1) {
      screens.pop_back();
      firstRow = 0;
      keepVisible();
    } else
      leave();
  }
  template <typename KeyEvent>
  void key(const KeyEvent& e) {
    if (e.key == GLFW_KEY_F8 && !(e.mods & RACK_MOD_MASK)) {
      if (e.action == GLFW_PRESS) leave();
      e.consume(this);
      return;
    }
    if (e.action == GLFW_PRESS || e.action == GLFW_REPEAT) {
      if (!transitioning() && !leaving && !screens.empty()) {
        auto& screen = screens.back();
        if (!screen.body.empty()) {
          if (e.key == GLFW_KEY_UP)
            screen.scroll = std::max(0, screen.scroll - 1);
          else if (e.key == GLFW_KEY_DOWN)
            ++screen.scroll;
          else if (e.key == GLFW_KEY_PAGE_DOWN)
            screen.scroll += capacity();
          else if (e.key == GLFW_KEY_PAGE_UP)
            screen.scroll = std::max(0, screen.scroll - capacity());
          else if (e.key == GLFW_KEY_ESCAPE || e.key == GLFW_KEY_LEFT)
            back();
          e.consume(this);
          return;
        }
        if (e.key == GLFW_KEY_UP)
          screen.selected = std::max(0, screen.selected - 1);
        else if (e.key == GLFW_KEY_DOWN)
          screen.selected =
              std::min(int(screen.entries.size()) - 1, screen.selected + 1);
        else if (e.action == GLFW_PRESS &&
                 (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER ||
                  e.key == GLFW_KEY_SPACE || e.key == GLFW_KEY_RIGHT))
          activate();
        else if (e.action == GLFW_PRESS &&
                 (e.key == GLFW_KEY_ESCAPE || e.key == GLFW_KEY_LEFT))
          back();
        keepVisible();
      }
    }
    e.consume(this);
  }
  void onSelectKey(const rack::event::SelectKey& e) override { key(e); }
  void onSelectText(const rack::event::SelectText& e) override {
    e.consume(this);
  }
  // Pointer events never navigate or activate terminal settings.
  void onButton(const rack::event::Button& e) override { e.consume(this); }
  void onHover(const rack::event::Hover& e) override { e.consume(this); }
  void onHoverScroll(const rack::event::HoverScroll& e) override {
    e.consume(this);
  }
  std::string rowText(const TerminalSetting& entry) const {
    const std::string& caption =
        entry.shortName.empty() ? entry.name : entry.shortName;
    if (entry.checked)
      return std::string(entry.checked() ? "[x] " : "[ ] ") + caption;
    return caption +
           ((entry.children.empty() && entry.body.empty()) ? "" : "  >");
  }
  std::string screenText() const {
    if (screens.empty()) return "";
    std::string text = screens.size() > 1 ? screens[screens.size() - 2].name +
                                                " / " + screens.back().name
                                          : "Settings";
    text += "\n\n";
    const auto& screen = screens.back();
    if (!screen.body.empty()) return text + screen.body;
    const int visibleRows = capacity();
    for (int i = firstRow;
         i < std::min(int(screen.entries.size()), firstRow + visibleRows); ++i)
      text += (i == screen.selected ? "> " : "  ") +
              rowText(screen.entries[i]) + "\n";
    return text;
  }
  static unsigned hash(unsigned x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    return x ^ (x >> 16);
  }
  void drawTransition(const DrawArgs& args, const text_editor::Theme& palette) {
    float p = rack::math::clamp(transitionProgress(), 0.f, 1.f);
    const std::string destination =
        leaving ? (sourceText ? sourceText() : "") : screenText();
    const std::string& text = p < .5f ? before : destination;
    const auto geometry = metrics();
    const Vec size = textSize();
    applyFont(args.vg);
    float advance =
        std::max(1.f, nvgTextBounds(args.vg, 0, 0, "M", nullptr, nullptr));
    int cols = std::max(1, int((size.x - 2 * geometry.textX) / advance));
    int rows = std::max(1, int(size.y / geometry.lineHeight));
    unsigned frame = unsigned(p * 24) + transitionSerial * 17u;
    nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
    int x = 0, y = 0;
    for (char c : text) {
      if (c == '\n') {
        x = 0;
        ++y;
        continue;
      }
      if (x >= cols) {
        x = 0;
        ++y;
      }
      if (y >= rows) break;
      unsigned h = hash(unsigned(x + y * cols) + transitionSerial * 1009u);
      float threshold = float(h % 1000) / 1000.f;
      float visibility = std::abs(2.f * p - 1.f);
      if (threshold < visibility && c != ' ') {
        char glyph[2] = {c, 0};
        nvgFillColor(args.vg, palette.text);
        nvgText(args.vg, geometry.textX + x * advance,
                geometry.baselineY + y * geometry.lineHeight, glyph, nullptr);
      } else if (threshold < visibility + .75f * std::sin(3.14159265f * p) &&
                 c != ' ') {
        char glyph[2] = {".:/+*#"[hash(h + frame) % 6], 0};
        nvgFillColor(args.vg, palette.green);
        nvgText(args.vg, geometry.textX + x * advance,
                geometry.baselineY + y * geometry.lineHeight, glyph, nullptr);
      }
      ++x;
    }
  }

  void draw(const DrawArgs& args) override {
    const auto& palette = theme ? theme() : text_editor::editorTheme(0);
    // The surrounding extrusion is drawn before clipping to the screen face.
    if (frame) frame(args);
    nvgSave(args.vg);
    nvgIntersectScissor(args.vg, 0, 0, box.size.x, box.size.y);
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
    nvgFillColor(args.vg, palette.background);
    nvgFill(args.vg);
    const Vec scale = textScale(), size = textSize();
    nvgScale(args.vg, scale.x, scale.y);
    applyFont(args.vg);
    const auto geometry =
        ComputerscareTextEditorLayout::build(args.vg, "", size, style())
            .metrics;
    float rowHeight = geometry.lineHeight;
    const int visibleRows = std::max(1, int(size.y / rowHeight) - 5);
    nvgTextLineHeight(args.vg, rowHeight / std::max(6.f, style().fontSize));
    if (transitioning()) {
      drawTransition(args, palette);
      nvgRestore(args.vg);
      return;
    }
    if (screens.empty()) {
      nvgRestore(args.vg);
      return;
    }
    keepVisible(visibleRows);
    nvgFillColor(args.vg, palette.green);
    std::string title = screens.size() > 1 ? screens[screens.size() - 2].name +
                                                 " / " + screens.back().name
                                           : "Settings";
    while (nvgTextBounds(args.vg, 0, 0, title.c_str(), nullptr, nullptr) >
               size.x - 2 * geometry.textX &&
           title.find(" / ") != std::string::npos)
      title = title.substr(title.find(" / ") + 3);
    nvgText(args.vg, geometry.textX, geometry.baselineY,
            fitText(args.vg, title, size.x - 2 * geometry.textX).c_str(),
            nullptr);
    auto& screen = screens.back();
    if (!screen.body.empty()) {
      auto wrappedStyle = style();
      wrappedStyle.lineWrapping = true;
      auto body = ComputerscareTextEditorLayout::build(args.vg, screen.body,
                                                       size, wrappedStyle);
      int count = std::max(1, int(size.y / rowHeight) - 3);
      screen.scroll = rack::math::clamp(
          screen.scroll, 0, std::max(0, int(body.rows.size()) - count));
      nvgFillColor(args.vg, palette.text);
      for (int i = screen.scroll;
           i < std::min(int(body.rows.size()), screen.scroll + count); ++i) {
        const auto& row = body.rows[i];
        std::string line = screen.body.substr(row.begin, row.end - row.begin);
        nvgText(args.vg, geometry.textX,
                (2 + i - screen.scroll) * rowHeight + geometry.baselineY,
                line.c_str(), nullptr);
      }
      nvgFillColor(args.vg, palette.green);
      nvgText(args.vg, geometry.textX,
              (int(size.y / rowHeight) - 1) * rowHeight + geometry.baselineY,
              fitText(args.vg, "Up/Down scroll  Esc back",
                      size.x - 2 * geometry.textX)
                  .c_str(),
              nullptr);
      nvgRestore(args.vg);
      return;
    }
    for (int i = firstRow;
         i < std::min(int(screen.entries.size()), firstRow + visibleRows);
         ++i) {
      const auto& entry = screen.entries[i];
      float top = (2 + i - firstRow) * rowHeight;
      if (i == screen.selected) {
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0, top, size.x, rowHeight);
        nvgFillColor(args.vg, palette.selection);
        nvgFill(args.vg);
      }
      nvgFillColor(args.vg,
                   i == screen.selected ? palette.yellow : palette.text);
      std::string value = entry.value ? entry.value() : "";
      float valueWidth =
          value.empty()
              ? 0
              : nvgTextBounds(args.vg, 0, 0, value.c_str(), nullptr, nullptr) +
                    12;
      std::string line = fitText(
          args.vg, (i == screen.selected ? "> " : "  ") + rowText(entry),
          size.x - 2 * geometry.textX - valueWidth);
      nvgText(args.vg, geometry.textX, top + geometry.baselineY, line.c_str(),
              nullptr);
      if (!value.empty()) {
        nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
        nvgText(args.vg, size.x - geometry.textX, top + geometry.baselineY,
                value.c_str(), nullptr);
        nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
      }
    }
    nvgFillColor(args.vg, palette.text);
    int rows = std::max(1, int(size.y / rowHeight));
    int helpRow = std::min(
        rows - 3, 3 + std::min(int(screen.entries.size()), visibleRows));
    const auto& focused = screen.entries[screen.selected];
    std::string description = focused.help;
    if (!focused.copyText.empty() &&
        nvgTextBounds(args.vg, 0, 0, ("> " + focused.copyText).c_str(), nullptr,
                      nullptr) > size.x - 2 * geometry.textX)
      description = focused.copyText + "\n" + description;
    nvgTextBox(args.vg, geometry.textX,
               helpRow * rowHeight + geometry.baselineY,
               size.x - 2 * geometry.textX, description.c_str(), nullptr);
    nvgFillColor(args.vg, palette.green);
    std::string footer = screen.entries[screen.selected].copyText.empty()
                             ? "Enter/Space select  Esc back"
                             : "Enter copies  Esc back";
    if (!copiedText.empty() &&
        copiedText == screen.entries[screen.selected].copyText &&
        rack::system::getTime() - copiedAt < 1.2)
      footer = "Copied to clipboard";
    nvgText(
        args.vg, geometry.textX, (rows - 1) * rowHeight + geometry.baselineY,
        fitText(args.vg, footer, size.x - 2 * geometry.textX).c_str(), nullptr);
    nvgRestore(args.vg);
  }
};
}  // namespace computerscare
