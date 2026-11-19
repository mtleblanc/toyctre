#pragma once

#include "toyctre/grammar.hpp"
#include <string_view>
#include <vector>
namespace re {
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
  Alternative alternative;
  char literal{};
  bool isNested{};
  char rep{};
};

struct ParseResult {
  Regex regex;
  bool accepted{};
};

ParseResult parseRegex(std ::string_view regex);

} // namespace re
