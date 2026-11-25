#pragma once
#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "BeekapooLabels.hpp"

namespace beekapoo {
struct ParamLayoutItem {
  std::string label;
  int module;
  float scale;
  float captionWidth = 0;
  ParamLayoutItem(std::string text, int source, float weight = 1)
      : label(std::move(text)), module(source), scale(weight) {}
};
// Break only at whitespace. Oversized words remain intact for fitting/clipping.
inline std::vector<std::string> labelWords(const std::string& text) {
  std::istringstream input(text);
  std::vector<std::string> result;
  for (std::string word; input >> word;) result.push_back(word);
  return result;
}
template <class Measure>
inline std::vector<std::string> wholeWordLines(const std::string& text,
                                               float width, Measure measure) {
  std::vector<std::string> lines;
  for (const auto& word : labelWords(text)) {
    if (lines.empty() || measure(lines.back() + " " + word) > width)
      lines.push_back(word);
    else
      lines.back() += " " + word;
  }
  return lines;
}
// Ignore punctuation, case and numeric channel suffixes when identifying a
// family. Preserve UTF-8 bytes; only ASCII characters are case folded.
inline std::string paramFamily(const std::string& label) {
  std::string key;
  for (unsigned char c : label)
    if (c >= 128 || std::isalpha(c))
      key += char(c >= 128 ? c : std::tolower(c));
  return key.empty() ? label : key;
}
inline int paramNumber(const std::string& label) {
  int number = 0;
  for (size_t i = 0; i < label.size(); ++i)
    if (label[i] >= '0' && label[i] <= '9') {
      for (; i < label.size() && label[i] >= '0' && label[i] <= '9'; ++i)
        number = std::min(100000, number * 10 + label[i] - '0');
      break;
    }
  return number;
}
struct ParamCell {
  int index;
  float x, y, diameter, labelWidth, labelScale, labelHeight;
  // Block layouts own a single shared caption, independently of knob rows.
  int block = -1;
  bool fixedCaption = false;
  float captionX = 0, captionY = 0;
  std::string captionText;
  ParamCell(int index, float x, float y, float diameter, float labelWidth,
            float labelScale = 1, float labelHeight = 32)
      : index(index),
        x(x),
        y(y),
        diameter(diameter),
        labelWidth(labelWidth),
        labelScale(labelScale),
        labelHeight(labelHeight) {}
};
inline std::vector<std::vector<int>> paramGroups(
    const std::vector<ParamLayoutItem>& items, bool byModule) {
  std::map<std::string, int> frequency;
  for (const auto& item : items) ++frequency[paramFamily(item.label)];
  std::map<std::string, size_t> groupIndex;
  std::vector<std::vector<int>> groups;
  std::vector<int> singles;
  for (int i = 0; i < int(items.size()); ++i) {
    auto family = paramFamily(items[i].label);
    if (!byModule && frequency[family] == 1) {
      singles.push_back(i);
      continue;
    }
    std::string key = byModule ? std::to_string(items[i].module) : family;
    if (!groupIndex.count(key)) {
      groupIndex[key] = groups.size();
      groups.emplace_back();
    }
    groups[groupIndex[key]].push_back(i);
  }
  if (!singles.empty()) groups.push_back(singles);
  for (auto& group : groups) {
    bool singletonGroup = group == singles;
    std::stable_sort(group.begin(), group.end(), [&](int a, int b) {
      if (byModule || singletonGroup) {
        if (items[a].module != items[b].module)
          return items[a].module < items[b].module;
        auto fa = paramFamily(items[a].label), fb = paramFamily(items[b].label);
        if (fa != fb) return fa < fb;
      }
      int na = paramNumber(items[a].label), nb = paramNumber(items[b].label);
      if (na != nb) return na < nb;
      return items[a].module < items[b].module;
    });
  }
  return groups;
}
// Dense view: retain every control, choosing the largest regular grid.
inline std::vector<ParamCell> compactParams(
    const std::vector<ParamLayoutItem>& items, float width, float height,
    bool byModule) {
  if (items.empty() || width <= 0 || height <= 0) return {};
  std::vector<int> order;
  for (const auto& group : paramGroups(items, byModule))
    order.insert(order.end(), group.begin(), group.end());
  int columns = 1;
  float size = 0;
  float reservedCaption = 0;
  float captionGap = 6;
  // If full captions could not fit, retain small captions wherever possible
  // instead of making the entire dense grid anonymous. Never trade a usable
  // control for a label: very dense grids still fall back to controls alone.
  // Eight-pixel text needs about ten pixels of line height in our font.
  // Reserve that actual height rather than allocating an unusable 8px slot.
  for (float caption : {24.f, 12.f, 10.f, 0.f}) {
    float separation = caption > 0 && caption <= 10 ? 2.f : 6.f;
    int bestColumns = 1;
    float bestSize = 0;
    for (int cols = 1; cols <= int(items.size()); ++cols) {
      int rows = (int(items.size()) + cols - 1) / cols;
      float gap = std::min(2.f, std::min(width / cols, height / rows) * .08f);
      float candidate = std::min(
          64.f,
          std::min(
              (width - gap * (cols - 1)) / cols,
              (height - (caption > 0 && caption <= 10 ? 0 : gap * (rows - 1))) /
                      rows -
                  (caption > 0 ? caption + separation : 0)));
      if (candidate > bestSize) {
        bestSize = candidate;
        bestColumns = cols;
      }
    }
    float minimum = caption >= 24   ? 30.f
                    : caption >= 12 ? 24.f
                    : caption >= 10 ? 16.f
                                    : 16.f;
    if (caption == 0 || bestSize >= minimum) {
      size = bestSize;
      columns = bestColumns;
      reservedCaption = caption;
      captionGap = separation;
      break;
    }
  }
  if (size <= 0) return {};
  int rows = (int(items.size()) + columns - 1) / columns;
  float rowStep = height / rows;
  // Dense grids often still have vertical room for a small caption. Keep
  // their control sizes; use that space before dropping labels altogether.
  float captionHeight =
      std::max(reservedCaption, std::min(28.f, rowStep - size - captionGap));
  bool captions = size >= 16.f && captionHeight >= 8.f;
  std::vector<ParamCell> cells;
  for (int i = 0; i < int(order.size()); ++i) {
    int row = i / columns, col = i % columns;
    int rowCount = std::min(columns, int(order.size()) - row * columns);
    float columnStep = width / columns;
    float left = (width - rowCount * columnStep) * .5f;
    float variation =
        .94f +
        .06f *
            std::max(0.f, std::min(1.f, (items[order[i]].scale - .7f) / .7f));
    cells.push_back(
        {order[i], left + (col + .5f) * columnStep,
         (row + .5f) * rowStep -
             (captions ? (captionHeight + captionGap) * .5f : 0),
         captions ? std::min(size, std::max(reservedCaption >= 24   ? 30.f
                                            : reservedCaption >= 10 ? 20.f
                                                                    : 16.f,
                                            size * variation))
                  : std::max(std::min(16.f, size), size * variation),
         captions ? columnStep - 2 : 0, .65f, captions ? captionHeight : 0});
    if (captions) {
      // Explicit caption bounds use the full cell and its actual gap. The
      // renderer must not assume the larger spacing of the roomy layout.
      auto& cell = cells.back();
      cell.fixedCaption = true;
      cell.captionX = cell.x;
      cell.captionY = (row + 1) * rowStep - captionHeight;
    }
  }
  return cells;
}
// Choose row breaks by available space rather than forcing each group onto
// its own row. Search largest controls first; group order remains stable.
inline std::vector<ParamCell> rowParams(
    const std::vector<ParamLayoutItem>& items, float width, float height,
    bool byModule) {
  if (items.empty() || width <= 0 || height <= 0) return {};
  std::vector<std::string> compactLabels;
  compactLabels.reserve(items.size());
  for (const auto& item : items)
    compactLabels.push_back(abbreviatedLabel(item.label, true));
  std::vector<int> order;
  for (const auto& group : paramGroups(items, byModule))
    order.insert(order.end(), group.begin(), group.end());
  auto key = [&](int index) {
    return (byModule ? std::to_string(items[index].module) + ":" : "") +
           paramFamily(items[index].label);
  };
  std::map<std::string, int> last, counts;
  for (int i = 0; i < int(order.size()); ++i) {
    last[key(order[i])] = i;
    ++counts[key(order[i])];
  }
  struct Row {
    int begin, end;
    float width, controlHeight, height;
  };
  std::vector<Row> rows;
  std::vector<float> diameters, footprints;
  bool fitted = false;
  float fittedLabelScale = 1;
  // Preserve the roomy layout first, then try smaller captions while keeping
  // the same usable knob sizes. Only the final fallback omits labels.
  for (float labelScale : {1.f, .8f, .6f}) {
    float minimum = width < 90 || labelScale < 1 ? 32.f : 36.f;
    for (int base = 64; base >= 16; base -= 2) {
      diameters.clear();
      footprints.clear();
      for (int index : order) {
        float variation = std::max(.85f, std::min(1.15f, items[index].scale));
        float size = std::min(
            width - 2, std::max(minimum, std::min(64.f, base * variation)));
        diameters.push_back(size);
        const auto& text =
            labelScale < 1 ? compactLabels[index] : items[index].label;
        float caption = labelScale < 1 ? 0 : items[index].captionWidth;
        if (caption <= 0)
          for (const auto& word : labelWords(text))
            caption = std::max(caption, float(word.size()) * 7.f + 4);
        footprints.push_back(std::max(
            size, std::min(width, caption * labelScale) / counts[key(index)]));
      }
      // Keep a small matching family together when it can fit at normal
      // size. Extra vertical room must not turn a balanced group into 2+1.
      std::map<std::string, bool> keepTogether;
      std::map<std::string, float> familyWidths;
      for (int i = 0; i < int(order.size()); ++i)
        familyWidths[key(order[i])] += footprints[i] + 6;
      bool familyTooWide = false;
      for (const auto& entry : counts) {
        bool keep = entry.second > 1 &&
                    entry.second * 36 + (entry.second - 1) * 6 <= width;
        keepTogether[entry.first] = keep;
        familyTooWide |= keep && familyWidths[entry.first] - 6 > width;
      }
      if (familyTooWide) continue;
      const int count = int(order.size());
      std::vector<float> cost(count + 1, INFINITY);
      std::vector<Row> choices(count);
      cost[count] = 0;
      for (int begin = count - 1; begin >= 0; --begin) {
        float occupied = 0, tallest = 0;
        float captionSpace = 0;
        for (int end = begin; end < count; ++end) {
          occupied += footprints[end] + (end > begin ? 6 : 0);
          if (occupied > width + .01f) break;
          tallest = std::max(tallest, diameters[end]);
          if (last[key(order[end])] == end) {
            int words = 0;
            const auto& text = labelScale < 1 ? compactLabels[order[end]]
                                              : items[order[end]].label;
            for (const auto& word : labelWords(text))
              words += std::any_of(
                  word.begin(), word.end(),
                  [](unsigned char c) { return c >= 128 || std::isalpha(c); });
            captionSpace = std::max(captionSpace,
                                    (words > 1 ? 32.f : 18.f) * labelScale + 6);
          }
          float rowHeight = tallest + std::max(6.f, captionSpace);
          if (end + 1 < count && key(order[end]) == key(order[end + 1]) &&
              keepTogether[key(order[end])])
            continue;
          // A tiny tie-break keeps matching controls together without wasting
          // a whole row to enforce a group boundary.
          float split =
              end + 1 < count && key(order[end]) == key(order[end + 1]) ? .01f
                                                                        : 0;
          float candidate = rowHeight + cost[end + 1] + split;
          if (candidate < cost[begin]) {
            cost[begin] = candidate;
            choices[begin] = {begin, end + 1, occupied, tallest, rowHeight};
          }
        }
      }
      rows.clear();
      float used = 0;
      for (int begin = 0; begin < count;) {
        const auto& row = choices[begin];
        if (row.end <= begin) break;
        rows.push_back(row);
        used += row.height;
        begin = row.end;
      }
      if (used <= height && !rows.empty() && rows.back().end == count) {
        fitted = true;
        fittedLabelScale = labelScale;
        break;
      }
    }
    if (fitted) break;
  }
  if (!fitted) return compactParams(items, width, height, byModule);
  std::vector<ParamCell> cells;
  float y = 0;
  for (const auto& row : rows) {
    if (y + row.height > height + .01f) break;
    float x = (width - row.width) * .5f;
    for (int column = row.begin; column < row.end; ++column) {
      float center = x + footprints[column] * .5f;
      float before = column == row.begin ? 0 : x - 3;
      float after = column + 1 == row.end ? width : x + footprints[column] + 3;
      float captionWidth =
          std::max(8.f, 2 * std::min(center - before, after - center) - 2);
      cells.push_back({order[column], center, y + row.controlHeight * .5f,
                       diameters[column], captionWidth, fittedLabelScale,
                       std::max(0.f, row.height - row.controlHeight - 6)});
      x += footprints[column] + 6;
    }
    y += row.height;
  }
  return cells;
}
}  // namespace beekapoo

#include "BeekapooParamBlocks.hpp"
