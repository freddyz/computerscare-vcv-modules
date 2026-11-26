#pragma once
#include <jansson.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace beekapoo {
struct StepValue {
  int64_t module;
  int param;
  struct Cell {
    std::atomic<uint64_t> state;
    static uint32_t bits(float v) {
      uint32_t b;
      std::memcpy(&b, &v, sizeof(b));
      return b;
    }
    static float number(uint32_t b) {
      float v;
      std::memcpy(&v, &b, sizeof(v));
      return v;
    }
    static uint64_t pack(float value, float observed) {
      return uint64_t(bits(value)) | (uint64_t(bits(observed)) << 32);
    }
    explicit Cell(float v) : state(pack(v, v)) {}
  };
  // Copies share only scalar state. Structure is immutable during playback.
  // A single CAS keeps saved/observed values together across UI/audio updates.
  std::shared_ptr<Cell> cell;
  StepValue(int64_t m, int p, float v)
      : module(m), param(p), cell(std::make_shared<Cell>(v)) {}
  float getValue() const { return Cell::number(uint32_t(cell->state.load())); }
  uint64_t token() const { return cell->state.load(); }
  void observe(float v) const {
    auto state = token();
    while (!cell->state.compare_exchange_weak(
        state, uint64_t(uint32_t(state)) | (uint64_t(Cell::bits(v)) << 32))) {
    }
  }
  void edit(float v) const {
    if (!std::isfinite(v)) return;
    auto state = token();
    while (Cell::number(uint32_t(state >> 32)) != v)
      if (cell->state.compare_exchange_weak(state, Cell::pack(v, v))) return;
  }
  void editIfUnchanged(float v, uint64_t state) const {
    if (std::isfinite(v) && Cell::number(uint32_t(state >> 32)) != v)
      cell->state.compare_exchange_strong(state, Cell::pack(v, v));
  }
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
