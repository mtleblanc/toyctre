#pragma once

namespace toyctre::grammar {
enum class Symbol {
  Start,
  Alt,
  Cat,
  Cats,
  Reps,
  Unt,
  Amt,
  Lit,

  Terminals_,
  T_Rep,
  T_LPar,
  T_RPar,
  T_Esc,
  T_Bar,
  T_Reg,
  T_Eot,

  Actions_,
  A_Mark,
  A_Alts,
  A_Cats,
  A_Group,
  A_Lit,
  A_Esc,
  A_Rep,

  A_Reject,

  Size_
};

constexpr auto lexChar(char c) {
  using enum Symbol;
  switch (c) {
  case '(':
    return T_LPar;
  case ')':
    return T_RPar;
  case '*':
  case '?':
  case '+':
    return T_Rep;
  case '|':
    return T_Bar;
  case '\\':
    return T_Esc;
  default:
    return T_Reg;
  }
}
} // namespace toyctre::grammar
