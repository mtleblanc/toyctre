#pragma once

#include "toyctre/grammar.hpp"
#include <string_view>
#include <vector>
namespace toyctre {
using grammar::Symbol;

struct Repetition;

struct Concatenation {
  std::vector<Repetition> repetitions;
};

struct Alternative {
  std::vector<Concatenation> concatenations;
};

struct Regex {
  Alternative alternative;
};

struct Repetition {
  Alternative altenative;
  char literal{};
  bool isNested{};
  bool wasEscaped{};
  char rep{};
};

struct ParseResult {
  Regex regex;
  bool accepted{};
};

ParseResult parseRegex(std ::string_view regex);

} // namespace toyctre
