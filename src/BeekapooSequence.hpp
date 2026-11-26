#pragma once
#include <jansson.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <vector>

namespace beekapoo {
struct StepValue {
  int64_t module;
  int param;
  float value;
  StepValue(int64_t m, int p, float v) : module(m), param(p), value(v) {}
  float getValue() const { return value; }
};
using StepValues = std::vector<StepValue>;
struct SequenceBank {
  std::vector<StepValues> steps;
  std::set<std::pair<int64_t, int>> locks;
  bool locked(int64_t id, int param) const {
    return locks.count({id, -1}) || locks.count({id, param});
  }
};
inline int scanStep(float voltage, int count) {
  if (count <= 0) return -1;
  if (!std::isfinite(voltage)) voltage = 0;
  return std::max(
      0, std::min(count - 1,
                  int(std::floor(std::max(0.f, std::min(10.f, voltage)) *
                                 count / 10.f))));
}
inline json_t* stepValuesToJson(const StepValues& values) {
  auto* list = json_array();
  for (const auto& v : values)
    if (std::isfinite(v.getValue()))
      json_array_append_new(
          list, json_pack("{s:I,s:i,s:f}", "module", json_int_t(v.module),
                          "param", v.param, "value", double(v.getValue())));
  return list;
}
inline StepValues stepValuesFromJson(json_t* list) {
  StepValues values;
  std::map<std::pair<int64_t, int>, bool> seen;
  for (size_t i = 0; i < json_array_size(list); ++i) {
    auto* item = json_array_get(list, i);
    auto* m = json_object_get(item, "module");
    auto* p = json_object_get(item, "param");
    auto* v = json_object_get(item, "value");
    if (!json_is_integer(m) || !json_is_integer(p) || !json_is_number(v))
      continue;
    int64_t id = json_integer_value(m), index = json_integer_value(p);
    double value = json_number_value(v);
    if (id < 0 || index < 0 || index > 1000000 || !std::isfinite(value) ||
        !std::isfinite(float(value)))
      continue;
    if (seen.emplace(std::make_pair(id, int(index)), true).second)
      values.emplace_back(id, int(index), float(value));
  }
  return values;
}
inline void remapSequence(json_t* sequence,
                          const std::map<int64_t, int64_t>& ids) {
  auto remap = [&](json_t* values) {
    for (size_t i = json_array_size(values); i > 0; --i) {
      auto* item = json_array_get(values, i - 1);
      auto old = json_integer_value(json_object_get(item, "module"));
      auto found = ids.find(old);
      if (found == ids.end())
        json_array_remove(values, i - 1);
      else
        json_object_set_new(item, "module", json_integer(found->second));
    }
  };
  auto* steps = json_object_get(sequence, "steps");
  for (size_t i = 0; i < json_array_size(steps); ++i)
    remap(json_array_get(steps, i));
}
}  // namespace beekapoo
