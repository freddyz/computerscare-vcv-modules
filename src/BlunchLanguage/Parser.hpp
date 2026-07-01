#pragma once

#include <string>
#include <vector>

#include "Ast.hpp"
#include "Tokenizer.hpp"

namespace blunch {
namespace language {

ParseResult parseClockLiteral(const std::string& source);
ParseResult parseClockLiteral(const std::vector<Token>& tokens);
ClockUnit parseClockUnit(const std::string& unit);

}  // namespace language
}  // namespace blunch
