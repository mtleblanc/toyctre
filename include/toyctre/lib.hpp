#pragma once

#include <string>
#include <string_view>
#include <vector>
namespace toyctre {

enum class Symbol {
  S,
  Alt,
  Cat,
  Cats,
  Reps,
  Unt,
  Amt,
  Lit,

  TERMINALS,
  T_Rep,
  T_LPar,
  T_RPar,
  T_Slash,
  T_Bar,
  T_Reg,
  T_Eot,

  ACTIONS,
  A_EAlt,
  A_AReg,
  A_AGrp,
  A_ECat,
  A_ACat,
  A_ERep,
  A_ARep,
  A_REsc,
  A_RLit,
  A_RRep,

  A_Reject,

  SIZE
};

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
  Alternative alternative;

  std::string toString() const;
};

struct Repetition {
  Alternative altenative;
  char literal{};
  bool isNested{};
  bool wasEscaped{};
  char rep{};

  std::string toString() const;
};

struct ParseResult {
  Regex regex;
  bool accepted{};
};

ParseResult parseRegex(std ::string_view regex);

} // namespace toyctre
