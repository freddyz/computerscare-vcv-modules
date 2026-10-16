#pragma once

#include <string>
#include <vector>

#include "Ast.hpp"
#include "Tokenizer.hpp"

namespace blunch {
namespace language {

ParseResult parseClockLiteral(const std::string& source,
                              ClockUnit defaultUnit = ClockUnit::Bpm);
ParseResult parseClockLiteral(const std::vector<Token>& tokens,
                              ClockUnit defaultUnit = ClockUnit::Bpm);
ClockUnit parseClockUnit(const std::string& unit);

}  // namespace language
}  // namespace blunch
