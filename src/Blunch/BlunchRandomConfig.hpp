#pragma once

#include <jansson.h>

#include <cmath>
#include <stdexcept>

#include "BlunchRandomProgram.hpp"

namespace blunch {
namespace random_program {
inline std::string configText(json_t* value) {
  if (!json_is_string(value) || json_string_length(value) > 4096)
    throw std::runtime_error(
        "Expected a template string of at most 4096 bytes");
  return std::string(json_string_value(value), json_string_length(value));
}
inline Catalogue readCatalogue(json_t* root) {
  if (!json_is_object(root) ||
      !json_is_integer(json_object_get(root, "schemaVersion")) ||
      json_integer_value(json_object_get(root, "schemaVersion")) != 1)
    throw std::runtime_error("Expected random-template schemaVersion 1");
  Catalogue result;
  result.startup = configText(json_object_get(root, "startup"));
  result.randomize = configText(json_object_get(root, "randomize"));
  json_t* definitions = json_object_get(root, "definitions");
  if (!json_is_object(definitions) || json_object_size(definitions) > 128)
    throw std::runtime_error("Expected at most 128 named template definitions");
  const char* name;
  json_t* definition;
  json_object_foreach(definitions, name, definition) {
    auto& choices = result.definitions[name];
    if (json_is_string(definition)) {
      choices.emplace_back(1., configText(definition));
    } else {
      if (!json_is_array(definition) || json_array_size(definition) == 0 ||
          json_array_size(definition) > 64)
        throw std::runtime_error("Expected 1..64 weighted templates for " +
                                 std::string(name));
      size_t index;
      json_t* choice;
      double total = 0.;
      json_array_foreach(definition, index, choice) {
        json_t* weight = json_object_get(choice, "weight");
        double value = json_number_value(weight);
        if (!json_is_number(weight) || !std::isfinite(value) || value < 0.)
          throw std::runtime_error("Invalid weight for " + std::string(name));
        total += value;
        choices.emplace_back(value,
                             configText(json_object_get(choice, "template")));
      }
      if (!std::isfinite(total) || total <= 0.)
        throw std::runtime_error("Expected positive total weight for " +
                                 std::string(name));
    }
  }
  return result;
}
}  // namespace random_program
}  // namespace blunch
