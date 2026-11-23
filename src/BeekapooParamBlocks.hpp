#pragma once
#include <memory>

namespace beekapoo {
namespace param_blocks {
// Families are small, unboxed layout regions. A slicing tree packs these
// regions in both directions, so one family's rows do not dictate another's.
// All policy values live here; planning only runs when the panel layout
// changes.
constexpr float gap = 8, knobGap = 6, maximumDiameter = 64;
constexpr int maxFrontier = 20;
struct Family {
  std::vector<int> indices;
  std::string label, compactLabel;
  float wordWidth = 0, compactWidth = 0;
};
inline std::string commonLabel(const std::vector<ParamLayoutItem>& items,
                               const std::vector<int>& indices) {
  std::string label = items[indices.front()].label;
  bool numbered = false;
  for (int i : indices) numbered |= items[i].label != label;
  if (!numbered) return label;
  label.erase(std::remove_if(label.begin(), label.end(),
                             [](unsigned char c) { return std::isdigit(c); }),
              label.end());
  auto words = labelWords(label);
  label.clear();
  for (const auto& word : words) {
    if (word == "-" || word == "#") continue;
    if (!label.empty()) label += ' ';
    label += word;
  }
  return label;
}
struct Shape {
  float width, height;
  int count, family = -1;
  bool horizontal = false;
  std::shared_ptr<Shape> a, b;
  Shape(float w, float h, int n) : width(w), height(h), count(n) {}
};
using Plan = std::shared_ptr<Shape>;
using Choices = std::vector<Plan>;
// Retain the non-dominated width/height tradeoffs. Bound the search even for
// large patches; no parameter value, external cable or random seed is involved.
inline Choices frontier(Choices choices) {
  std::stable_sort(choices.begin(), choices.end(),
                   [](const Plan& a, const Plan& b) {
                     return a->width == b->width ? a->height < b->height
                                                 : a->width < b->width;
                   });
  Choices result;
  float height = INFINITY;
  for (auto& choice : choices)
    if (choice->height < height - .01f) {
      height = choice->height;
      result.push_back(choice);
    }
  if (int(result.size()) <= maxFrontier) return result;
  Choices sampled;
  for (int i = 0; i < maxFrontier; ++i)
    sampled.push_back(result[i * (result.size() - 1) / (maxFrontier - 1)]);
  return sampled;
}
inline float captionHeight(const Family& family, float width, float scale) {
  auto lines = wholeWordLines(scale < 1 ? family.compactLabel : family.label,
                              width, [scale](const std::string& text) {
                                return float(text.size()) * 7.f * scale;
                              });
  return std::max(1, int(lines.size())) * 16.f * scale;
}
inline Choices join(const Choices& left, const Choices& right, float width,
                    float height) {
  Choices result;
  for (const auto& a : left)
    for (const auto& b : right)
      for (bool horizontal : {true, false}) {
        float w = horizontal ? a->width + gap + b->width
                             : std::max(a->width, b->width);
        float h = horizontal ? std::max(a->height, b->height)
                             : a->height + gap + b->height;
        if (w > width || h > height) continue;
        auto node = std::make_shared<Shape>(w, h, a->count + b->count);
        node->a = a;
        node->b = b;
        node->horizontal = horizontal;
        result.push_back(node);
      }
  return frontier(std::move(result));
}
inline Choices packRange(const std::vector<Choices>& children, int first,
                         int last, float width, float height,
                         std::map<std::pair<int, int>, Choices>& memo) {
  if (last - first == 1) return children[first];
  if (first == last) return {};
  auto key = std::make_pair(first, last);
  auto cached = memo.find(key);
  if (cached != memo.end()) return cached->second;
  // Split close to equal control counts, while keeping families intact.
  int total = 0;
  for (int i = first; i < last; ++i) {
    if (children[i].empty()) return {};
    total += children[i].front()->count;
  }
  int split = first + 1, before = children[first].front()->count;
  for (int i = first + 1, sum = before; i < last - 1; ++i) {
    sum += children[i].front()->count;
    if (std::abs(total - 2 * sum) < std::abs(total - 2 * before)) {
      before = sum;
      split = i + 1;
    }
  }
  Choices result;
  // Explore small sets fully: a 3+7 split can fit where a balanced 6+4
  // cannot. Larger patches recurse through balanced chunks to bound work.
  int from = last - first <= 8 ? first + 1 : split;
  int to = last - first <= 8 ? last : split + 1;
  for (int cut = from; cut < to; ++cut) {
    auto choices = join(packRange(children, first, cut, width, height, memo),
                        packRange(children, cut, last, width, height, memo),
                        width, height);
    result.insert(result.end(), choices.begin(), choices.end());
  }
  return memo[key] = frontier(std::move(result));
}
inline Choices pack(const std::vector<Choices>& children, int first, int last,
                    float width, float height) {
  std::map<std::pair<int, int>, Choices> memo;
  return packRange(children, first, last, width, height, memo);
}
struct Fit {
  int columns;
  float diameter, caption;
  Fit(int c = 1, float d = 0, float h = 0)
      : columns(c), diameter(d), caption(h) {}
};
inline Fit fit(const Family& family, float width, float height, float scale) {
  Fit best;
  int count = int(family.indices.size());
  float caption = captionHeight(family, width, scale);
  for (int cols = 1; cols <= count; ++cols) {
    int rows = (count + cols - 1) / cols;
    float d = std::min(
        maximumDiameter,
        std::min((width - knobGap * (cols - 1)) / cols,
                 (height - caption - 5 - knobGap * (rows - 1)) / rows));
    if (d > best.diameter + .01f ||
        (std::abs(d - best.diameter) < .01f &&
         rows < (count + best.columns - 1) / best.columns))
      best = {cols, d, caption};
  }
  return best;
}
inline void place(const Plan& plan, const std::vector<Family>& families,
                  float x, float y, float width, float height, float scale,
                  std::vector<ParamCell>& cells) {
  if (plan->family >= 0) {
    const auto& family = families[plan->family];
    auto fitted = fit(family, width, height, scale);
    int count = int(family.indices.size()),
        rows = (count + fitted.columns - 1) / fitted.columns;
    float used =
        rows * fitted.diameter + (rows - 1) * knobGap + 5 + fitted.caption;
    float top = y + (height - used) * .5f;
    for (int i = 0; i < count; ++i) {
      int row = i / fitted.columns, col = i % fitted.columns;
      int n = std::min(fitted.columns, count - row * fitted.columns);
      float rowWidth = n * fitted.diameter + (n - 1) * knobGap;
      ParamCell cell(
          family.indices[i],
          x + (width - rowWidth) * .5f + col * (fitted.diameter + knobGap) +
              fitted.diameter * .5f,
          top + row * (fitted.diameter + knobGap) + fitted.diameter * .5f,
          fitted.diameter, i == count - 1 ? width : 0, scale,
          i == count - 1 ? fitted.caption : 0);
      cell.block = plan->family;
      cell.captionX = x + width * .5f;
      cell.captionY = top + used - fitted.caption;
      cell.captionText = family.label;
      cells.push_back(std::move(cell));
    }
    return;
  }
  // Distribute spare room locally. A small neighbouring family can grow
  // independently instead of inheriting the dense family's knob diameter.
  float share = float(plan->a->count) / plan->count;
  if (plan->horizontal) {
    float spare = std::max(0.f, width - plan->width);
    float aw = plan->a->width + spare * share;
    place(plan->a, families, x, y, aw, height, scale, cells);
    place(plan->b, families, x + aw + gap, y, width - aw - gap, height, scale,
          cells);
  } else {
    float spare = std::max(0.f, height - plan->height);
    float ah = plan->a->height + spare * share;
    place(plan->a, families, x, y, width, ah, scale, cells);
    place(plan->b, families, x, y + ah + gap, width, height - ah - gap, scale,
          cells);
  }
}
inline std::vector<ParamCell> layout(const std::vector<ParamLayoutItem>& items,
                                     float width, float height, bool byModule) {
  if (items.empty() || items.size() > 128 || width < 32 || height < 48)
    return {};
  std::vector<Family> families;
  std::map<std::string, int> familyIndex;
  for (const auto& group : paramGroups(items, byModule))
    for (int i : group) {
      std::string key =
          (byModule ? std::to_string(items[i].module) + ":" : "") +
          paramFamily(items[i].label);
      if (!familyIndex.count(key)) {
        familyIndex[key] = int(families.size());
        families.emplace_back();
      }
      auto& family = families[familyIndex[key]];
      family.indices.push_back(i);
      family.wordWidth = std::max(family.wordWidth, items[i].captionWidth);
    }
  for (auto& family : families) {
    family.label = commonLabel(items, family.indices);
    family.compactLabel = abbreviatedLabel(family.label, true);
    for (const auto& word : labelWords(family.compactLabel))
      family.compactWidth =
          std::max(family.compactWidth, float(word.size()) * 7 + 4);
    if (family.wordWidth <= 0)
      for (const auto& word : labelWords(family.label))
        family.wordWidth =
            std::max(family.wordWidth, float(word.size()) * 7 + 4);
  }
  std::vector<ParamCell> best;
  float bestScore = -INFINITY;
  for (float scale : {1.f, .8f, .65f})
    for (float base : {56.f, 48.f, 40.f, 32.f, 28.f}) {
      // The smallest option is for crowded groups, not a handful of knobs.
      if (items.size() <= 4 && base < 32) continue;
      std::vector<Choices> children;
      std::vector<int> modules;
      for (int f = 0; f < int(families.size()); ++f) {
        const auto& family = families[f];
        int count = int(family.indices.size());
        Choices shapes;
        for (int cols = 1; cols <= count; ++cols) {
          float w = std::max(
              cols * base + (cols - 1) * knobGap,
              (scale < 1 ? family.compactWidth : family.wordWidth) * scale);
          int rows = (count + cols - 1) / cols;
          float h = rows * base + (rows - 1) * knobGap + 5 +
                    captionHeight(family, w, scale);
          if (w > width || h > height) continue;
          auto node = std::make_shared<Shape>(w, h, count);
          node->family = f;
          shapes.push_back(node);
        }
        children.push_back(frontier(std::move(shapes)));
        modules.push_back(items[family.indices.front()].module);
      }
      if (byModule) {
        std::vector<Choices> moduleBlocks;
        for (int begin = 0; begin < int(children.size());) {
          int end = begin + 1;
          while (end < int(children.size()) && modules[begin] == modules[end])
            ++end;
          moduleBlocks.push_back(pack(children, begin, end, width, height));
          begin = end;
        }
        children = std::move(moduleBlocks);
      }
      for (const auto& plan :
           pack(children, 0, int(children.size()), width, height)) {
        std::vector<ParamCell> cells;
        place(plan, families, 0, 0, width, height, scale, cells);
        float smallest = maximumDiameter, sum = 0;
        for (const auto& cell : cells) {
          smallest = std::min(smallest, cell.diameter);
          sum += cell.diameter;
        }
        // Prefer readable controls and captions, not decoration or wasted
        // space.
        float score = smallest * .65f + sum / items.size() * .35f + scale * 12;
        if (score > bestScore) {
          bestScore = score;
          best = std::move(cells);
        }
      }
    }
  return best;
}
}  // namespace param_blocks
inline std::vector<ParamCell> layoutParams(
    const std::vector<ParamLayoutItem>& items, float width, float height,
    bool byModule) {
  auto blocks = param_blocks::layout(items, width, height, byModule);
  if (!blocks.empty()) return blocks;
  // Extremely narrow/dense panels still retain every control.
  return rowParams(items, width, height, byModule);
}
}  // namespace beekapoo
