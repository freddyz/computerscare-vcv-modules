#pragma once
namespace generated {
// Reuse the independent template expander used by Blunch. Vocabulary and
// weighted naming structures are data, not a collection of C++ special cases.
inline std::string name(unsigned entropy = random::u32()) {
  auto path = asset::plugin(pluginInstance, "res/programs/beekapoo-names.json");
  auto* data = json_load_file(path.c_str(), 0, nullptr);
  DEFER({ json_decref(data); });
  try {
    blunch::random_program::Catalogue catalogue;
    const char* key;
    json_t* value;
    json_object_foreach(json_object_get(data, "words"), key, value) {
      for (size_t i = 0; i < json_array_size(value); ++i) {
        auto* word = json_string_value(json_array_get(value, i));
        if (word && *word) catalogue.definitions[key].emplace_back(1., word);
      }
    }
    json_object_foreach(json_object_get(data, "definitions"), key, value) {
      if (auto* text = json_string_value(value))
        catalogue.definitions[key].emplace_back(1., text);
    }
    auto* patterns = json_object_get(data, "patterns");
    for (size_t i = 0; i < json_array_size(patterns); ++i) {
      auto* pattern = json_array_get(patterns, i);
      auto* text = json_string_value(json_object_get(pattern, "template"));
      double weight = json_number_value(json_object_get(pattern, "weight"));
      if (text && std::isfinite(weight) && weight > 0)
        catalogue.definitions["name"].emplace_back(weight, text);
    }
    auto result = blunch::random_program::expandTemplate(catalogue,
                                                         "(ref name)", entropy);
    if (!result.empty() && result.size() <= 96 &&
        result.find('\n') == std::string::npos)
      return result;
  } catch (const std::exception& e) {
    WARN("Beekapoo naming: %s", e.what());
  }
  return "Beek " + std::to_string(entropy % 1000);
}
}  // namespace generated
