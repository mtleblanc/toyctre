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
  A_EmptyAlt,
  A_MakeRegex,
  A_MakeGroup,
  A_EmptyCat,
  A_AppendCat,
  A_EmptyRep,
  A_AppendRep,
  A_RecordEscaped,
  A_RecordLit,
  A_RecordRep,

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
