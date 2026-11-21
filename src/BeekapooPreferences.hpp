#pragma once

#include "BeekapooTheme.hpp"

namespace beekapoo {
// UI-only preferences belong to the Rack user, not a patch or module instance.
struct Preferences {
  float outsideFade = .45f;
  float savedOutsideFade = .45f;
  int defaultTheme = 0, savedDefaultTheme = 0;
  std::string path;

  explicit Preferences(std::string path) : path(std::move(path)) {
    json_error_t error;
    auto* root = json_load_file(this->path.c_str(), 0, &error);
    auto* fade = json_object_get(root, "outsideFade");
    if (json_is_number(fade))
      outsideFade = math::clamp(float(json_number_value(fade)), 0.f, .9f);
    savedOutsideFade = outsideFade;
    auto* theme = json_object_get(root, "defaultTheme");
    if (json_is_integer(theme))
      defaultTheme = validPanelTheme(json_integer_value(theme));
    savedDefaultTheme = defaultTheme;
    json_decref(root);
  }

  void save() {
    if (outsideFade == savedOutsideFade && defaultTheme == savedDefaultTheme)
      return;
    auto* root = json_pack("{s:f,s:i}", "outsideFade", double(outsideFade),
                           "defaultTheme", defaultTheme);
    std::string temporary = path + ".tmp";
    int result = json_dump_file(root, temporary.c_str(), JSON_INDENT(2));
    json_decref(root);
    if (result == 0 && system::rename(temporary, path)) {
      savedOutsideFade = outsideFade;
      savedDefaultTheme = defaultTheme;
    } else {
      system::remove(temporary);
      WARN("Could not save Beekapoo preferences: %s", path.c_str());
    }
  }
};

inline Preferences& preferences() {
  static Preferences value(asset::user("computerscare-beekapoo.json"));
  return value;
}

struct OutsideFadeQuantity : Quantity {
  void setValue(float value) override {
    if (std::isfinite(value))
      preferences().outsideFade = math::clamp(value, 0.f, .9f);
  }
  float getValue() override { return preferences().outsideFade; }
  float getMaxValue() override { return .9f; }
  float getDefaultValue() override { return .45f; }
  float getDisplayValue() override { return getValue() * 100.f; }
  void setDisplayValue(float value) override { setValue(value / 100.f); }
  std::string getDisplayValueString() override {
    return string::f("%.0f", getDisplayValue());
  }
  std::string getLabel() override { return "Fade outside modules (global)"; }
  std::string getUnit() override { return "%"; }
};

struct OutsideFadeSlider : ui::Slider {
  OutsideFadeQuantity fade;
  OutsideFadeSlider() {
    quantity = &fade;
    box.size = Vec(300, 24);
  }
  ~OutsideFadeSlider() override { preferences().save(); }
  void onDragEnd(const DragEndEvent& e) override {
    ui::Slider::onDragEnd(e);
    preferences().save();
  }
  void onDoubleClick(const DoubleClickEvent& e) override {
    ui::Slider::onDoubleClick(e);
    preferences().save();
  }
};
}  // namespace beekapoo
