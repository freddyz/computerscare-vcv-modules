#pragma once

#include <string>
#include <vector>

#include "Ast.hpp"

namespace cloly {
namespace language {

enum class TokenType {
  Number,
  Identifier,
  Colon,
  Comma,
  At,
  Hash,
  Question,
  Tilde,
  LeftBracket,
  RightBracket,
  LeftParen,
  RightParen,
  LeftBrace,
  Dash,
  Pipe,
  RightBrace,
  End,
  Invalid
};

struct Token {
  TokenType type = TokenType::Invalid;
  std::string lexeme;
  int begin = 0;
  int end = 0;

  Token() {}
  Token(TokenType type, std::string lexeme, int begin, int end)
      : type(type), lexeme(lexeme), begin(begin), end(end) {}
};

std::vector<Token> tokenize(const std::string& source);
std::string tokenTypeName(TokenType type);

}  // namespace language
}  // namespace cloly
