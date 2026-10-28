#pragma once

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace daddys_home {
constexpr int ports = 16;
constexpr int routes = ports * ports;
using Routing = std::array<int, routes>;
// -1 means All. Source and destination addresses are port * 16 + channel.
struct Filter {
  int inputPort = -1;
  int inputChannel = -1;
  int outputPort = -1;
  int outputChannel = -1;
  Filter(int inPort = -1, int inChannel = -1, int outPort = -1,
         int outChannel = -1)
      : inputPort(inPort),
        inputChannel(inChannel),
        outputPort(outPort),
        outputChannel(outChannel) {}
  bool operator==(const Filter& other) const {
    return inputPort == other.inputPort && inputChannel == other.inputChannel &&
           outputPort == other.outputPort &&
           outputChannel == other.outputChannel;
  }
};
inline Routing identity() {
  Routing result{};
  for (int i = 0; i < routes; ++i) result[i] = i;
  return result;
}
inline std::string address(int value, bool output = false) {
  if (value < 0) return "--";
  return std::string(1, (output ? 'A' : 'a') + value / ports) +
         std::to_string(value % ports + 1);
}
inline bool matches(const Filter& f, int destination, int source) {
  if (f.outputPort >= 0 && destination / ports != f.outputPort) return false;
  if (f.outputChannel >= 0 && destination % ports != f.outputChannel)
    return false;
  if (f.inputPort >= 0 && (source < 0 || source / ports != f.inputPort))
    return false;
  if (f.inputChannel >= 0 && (source < 0 || source % ports != f.inputChannel))
    return false;
  return true;
}
struct Availability {
  std::array<int, ports> inputs{};
  std::array<int, ports> outputs{};
  bool source(int index) const {
    return index >= 0 && index < routes &&
           index % ports < inputs[index / ports];
  }
  bool destination(int index) const {
    return index >= 0 && index < routes &&
           index % ports < outputs[index / ports];
  }
  bool route(const Routing& routing, int dest) const {
    return destination(dest) && source(routing[dest]);
  }
};
struct Row {
  std::string label;
  std::vector<std::string> cells;
  std::vector<int> destinations;
  int source = -1;
  int destination = -1;
  int outputPort = -1;
  Row(std::string text, std::vector<std::string> values)
      : label(std::move(text)), cells(std::move(values)) {}
};
inline std::vector<Row> view(const Routing& routing, const Filter& filter) {
  std::vector<Row> rows;
  bool sourceView = filter.inputPort >= 0 || filter.inputChannel >= 0;
  if (sourceView) {
    for (int source = 0; source < routes; ++source) {
      if (filter.inputPort >= 0 && source / ports != filter.inputPort) continue;
      if (filter.inputChannel >= 0 && source % ports != filter.inputChannel)
        continue;
      Row row{address(source) + " ->", {}};
      row.source = source;
      for (int dest = 0; dest < routes; ++dest)
        if (routing[dest] == source && matches(filter, dest, source)) {
          row.cells.push_back(address(dest, true));
          row.destinations.push_back(dest);
        }
      // Keep the selected sources visible, including those with no
      // destinations.
      if (row.cells.empty()) {
        row.cells.push_back("--");
        row.destinations.push_back(-1);
      }
      rows.push_back(row);
    }
  } else if (filter.outputPort >= 0) {
    for (int dest = filter.outputPort * ports;
         dest < (filter.outputPort + 1) * ports; ++dest) {
      if (!matches(filter, dest, routing[dest])) continue;
      Row row{address(routing[dest]) + " ->", {address(dest, true)}};
      row.source = routing[dest];
      row.destination = dest;
      row.destinations.push_back(dest);
      rows.push_back(row);
    }
  } else {
    for (int port = 0; port < ports; ++port) {
      Row row{filter.outputChannel >= 0
                  ? address(port * ports + filter.outputChannel, true)
                  : std::string(1, 'A' + port),
              {}};
      row.outputPort = port;
      if (filter.outputChannel >= 0)
        row.destination = port * ports + filter.outputChannel;
      for (int ch = 0; ch < ports; ++ch) {
        int dest = port * ports + ch;
        if (matches(filter, dest, routing[dest])) {
          row.cells.push_back(address(routing[dest]));
          row.destinations.push_back(dest);
        }
      }
      rows.push_back(row);
    }
  }
  return rows;
}
}  // namespace daddys_home
