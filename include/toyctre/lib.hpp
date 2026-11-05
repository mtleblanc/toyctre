#pragma once

#include <string_view>
#include <vector>
namespace toyctre {

struct Repetition;

struct Concatenation {
  std::vector<Repetition> repetitions;

  std::string toString() const;
};

struct Alternative {
  std::vector<Concatenation> concatenations;

  std::string toString() const;
};

struct Regex {
  Alternative alternatives;

  std::string toString() const;
};

struct Unit {
  Regex regex;
  char literal{};
  bool isNested{};
  bool wasEscaped{};

  std::string toString() const;
};

struct Repetition {
  Unit unit;
  char rep{};

  std::string toString() const;
};

struct ParseResult {
  Regex regex;
  bool accepted{};
};

ParseResult parseRegex(std ::string_view regex);

} // namespace toyctre
