#include "Tokenizer.hpp"

#include <cctype>

namespace blunch {
namespace language {

namespace {
bool isIdentifierChar(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) != 0;
}
}  // namespace

std::vector<Token> tokenize(const std::string& source) {
  std::vector<Token> tokens;
  int index = 0;
  const int length = static_cast<int>(source.size());

  while (index < length) {
    char c = source[index];
    if (std::isspace(static_cast<unsigned char>(c)) != 0) {
      index++;
      continue;
    }

    if (std::isdigit(static_cast<unsigned char>(c)) != 0 ||
        (c == '.' && index + 1 < length &&
         std::isdigit(static_cast<unsigned char>(source[index + 1])) != 0)) {
      int begin = index;
      bool sawDot = c == '.';
      index++;

      while (index < length) {
        char next = source[index];
        if (std::isdigit(static_cast<unsigned char>(next)) != 0) {
          index++;
        } else if (next == '.') {
          if (sawDot) {
            index++;
            while (index < length && (std::isdigit(static_cast<unsigned char>(
                                          source[index])) != 0 ||
                                      source[index] == '.')) {
              index++;
            }
            tokens.push_back({TokenType::Invalid,
                              source.substr(begin, index - begin), begin,
                              index});
            break;
          }
          sawDot = true;
          index++;
        } else {
          break;
        }
      }

      if (tokens.empty() || tokens.back().begin != begin) {
        tokens.push_back({TokenType::Number,
                          source.substr(begin, index - begin), begin, index});
      }
      continue;
    }

    if (isIdentifierChar(c)) {
      int begin = index;
      index++;
      while (index < length && isIdentifierChar(source[index])) {
        index++;
      }
      tokens.push_back({TokenType::Identifier,
                        source.substr(begin, index - begin), begin, index});
      continue;
    }

    if (c == ':') {
      tokens.push_back(
          {TokenType::Colon, source.substr(index, 1), index, index + 1});
      index++;
      continue;
    }

    if (c == ',') {
      tokens.push_back(
          {TokenType::Comma, source.substr(index, 1), index, index + 1});
      index++;
      continue;
    }

    if (c == '@') {
      tokens.push_back(
          {TokenType::At, source.substr(index, 1), index, index + 1});
      index++;
      continue;
    }

    if (c == '#') {
      tokens.push_back(
          {TokenType::Hash, source.substr(index, 1), index, index + 1});
      index++;
      continue;
    }

    if (c == '?') {
      tokens.push_back(
          {TokenType::Question, source.substr(index, 1), index, index + 1});
      index++;
      continue;
    }

    if (c == '~') {
      tokens.push_back(
          {TokenType::Tilde, source.substr(index, 1), index, index + 1});
      index++;
      continue;
    }

    if (c == '[') {
      tokens.push_back(
          {TokenType::LeftBracket, source.substr(index, 1), index, index + 1});
      index++;
      continue;
    }

    if (c == ']') {
      tokens.push_back(
          {TokenType::RightBracket, source.substr(index, 1), index, index + 1});
      index++;
      continue;
    }

    if (c == '(') {
      tokens.push_back(
          {TokenType::LeftParen, source.substr(index, 1), index, index + 1});
      index++;
      continue;
    }

    if (c == ')') {
      tokens.push_back(
          {TokenType::RightParen, source.substr(index, 1), index, index + 1});
      index++;
      continue;
    }

    if (c == '{') {
      tokens.push_back(
          {TokenType::LeftBrace, source.substr(index, 1), index, index + 1});
      index++;
      continue;
    }

    if (c == '-') {
      tokens.push_back(
          {TokenType::Dash, source.substr(index, 1), index, index + 1});
      index++;
      continue;
    }

    if (c == '|') {
      tokens.push_back(
          {TokenType::Pipe, source.substr(index, 1), index, index + 1});
      index++;
      continue;
    }

    if (c == '}') {
      tokens.push_back(
          {TokenType::RightBrace, source.substr(index, 1), index, index + 1});
      index++;
      continue;
    }

    tokens.push_back(
        {TokenType::Invalid, source.substr(index, 1), index, index + 1});
    index++;
  }

  tokens.push_back({TokenType::End, "", length, length});
  return tokens;
}

std::string tokenTypeName(TokenType type) {
  switch (type) {
    case TokenType::Number:
      return "Number";
    case TokenType::Identifier:
      return "Identifier";
    case TokenType::Colon:
      return "Colon";
    case TokenType::Comma:
      return "Comma";
    case TokenType::At:
      return "At";
    case TokenType::Hash:
      return "Hash";
    case TokenType::Question:
      return "Question";
    case TokenType::Tilde:
      return "Tilde";
    case TokenType::LeftBracket:
      return "LeftBracket";
    case TokenType::RightBracket:
      return "RightBracket";
    case TokenType::LeftParen:
      return "LeftParen";
    case TokenType::RightParen:
      return "RightParen";
    case TokenType::LeftBrace:
      return "LeftBrace";
    case TokenType::Dash:
      return "Dash";
    case TokenType::Pipe:
      return "Pipe";
    case TokenType::RightBrace:
      return "RightBrace";
    case TokenType::End:
      return "End";
    case TokenType::Invalid:
      return "Invalid";
  }
  return "Invalid";
}

}  // namespace language
}  // namespace blunch
