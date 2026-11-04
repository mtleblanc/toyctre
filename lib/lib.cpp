#include "toyctre/lib.hpp"
#include <ranges>
#include <string_view>
#include <utility>
#include <vector>

namespace toyctre {
/*
 *  Productions:
 *
 *  S -> Alt                            Norm (\
 *  Alt -> Concat MaybeAlt              Norm (\
 *  MaybeAlt -> e                       )$
 *  MaybeAlt -> "|" Alt                 |
 *  Concat -> Repetition MaybeConcat    Norm (\
 *  MaybeConcat -> e                    |)$
 *  MaybeConcat -> Concat               Norm (\
 *  Repetition -> Unit MaybeRep         Norm (\
 *  MaybeRep -> e                       Norm (\|
 *  MaybeRep -> "*+?"                   Rep
 *  Unit -> "(" Alt ")"                 (
 *  Unit -> "\" Lit                     \
 *  Unit -> Norm                        Norm
 *  Norm -> [^?+*()\\|]                 Norm
 *  Lit -> Norm                         Norm
 *  Lit -> [?+*()\\|]                   Rep ()\|
 *
 *
 *  NT                Fi(NT)          Fo(NT)
 *  S                 Norm, (\        $
 *  Alt               Norm, (\        )$
 *  Concat            Norm, (\        |)$
 *  Repetition        Norm, (\        Norm (\|)$
 *  Unit              Norm, (\        Norm Rep (\|$
 *  Norm              Norm            Norm Rep (\|$
 *  Lit               All             Norm Rep (\|$
 *  MaybeAlt          e, |            )$
 *  MaybeConcat       e, Norm, (\     |)$
 *  MaybeRep          e, Rep          Norm (\|$
 */

enum class Symbols {
  S,
  Alt,
  Cat,
  MAlt,
  Rep,
  MCat,
  Unt,
  MRep,
  Lit,
  Nor,
  T_Rep,
  T_LPar,
  T_RPar,
  T_Slash,
  T_Bar,
  T_Reg,
  T_Eot,
  SIZE
};

struct Sym {
  Symbols symbol;
  char c;
  constexpr bool operator==(Symbols o) const { return symbol == o; };
};

constexpr auto classify(char c) {
  switch (c) {
  case '(':
    return Symbols::T_LPar;
  case ')':
    return Symbols::T_RPar;
  case '+':
  case '*':
  case '?':
    return Symbols::T_Rep;
  case '\\':
    return Symbols::T_Slash;
  case '|':
    return Symbols::T_Bar;
  default:
    return Symbols::T_Reg;
  }
}

constexpr auto table() {
  auto table = std::array<std::array<int, static_cast<size_t>(Symbols::SIZE)>,
                          static_cast<size_t>(Symbols::SIZE)>{};
  auto t = [&](Symbols s, Symbols t) -> auto & {
    return table[static_cast<size_t>(s)][static_cast<size_t>(t)];
  };
  using enum Symbols;

  //  S -> Alt                            Norm ( Esc
  t(S, T_Reg) = 1;
  t(S, T_LPar) = 1;
  t(S, T_Slash) = 1;

  //  Alt -> Concat MaybeAlt              Norm ( Esc
  t(Alt, T_Reg) = 2;
  t(Alt, T_LPar) = 2;
  t(Alt, T_Slash) = 2;

  //  MaybeAlt -> e                       )$
  t(MAlt, T_RPar) = 3;
  t(MAlt, T_Eot) = 3;

  //  MaybeAlt -> "|" Alt                 |
  t(MAlt, T_Bar) = 4;

  //  Concat -> Repetition MaybeConcat    Norm ( Esc
  t(Cat, T_Reg) = 5;
  t(Cat, T_LPar) = 5;
  t(Cat, T_Slash) = 5;

  //  MaybeConcat -> e                    |)$
  t(MCat, T_Bar) = 6;
  t(MCat, T_RPar) = 6;
  t(MCat, T_Eot) = 6;

  //  MaybeConcat -> Concat               Norm ( Esc
  t(MCat, T_Reg) = 7;
  t(MCat, T_LPar) = 7;
  t(MCat, T_Slash) = 7;

  //  Repetition -> Unit MaybeRep         Norm ( Esc
  t(Rep, T_Reg) = 8;
  t(Rep, T_LPar) = 8;
  t(Rep, T_Slash) = 8;

  //  MaybeRep -> e                       Norm ( Esc|
  t(MRep, T_Reg) = 9;
  t(MRep, T_LPar) = 9;
  t(MRep, T_Slash) = 9;
  t(MRep, T_Bar) = 9;
  t(MRep, T_RPar) = 9;
  t(MRep, T_Eot) = 9;

  //  MaybeRep -> "*?+"                   *
  t(MRep, T_Rep) = 10;

  //  Unit -> "(" Alt ")"                 (
  t(Unt, T_LPar) = 11;

  //  Unit -> "/" Lit                     Esc
  t(Unt, T_Slash) = 12;

  //  Unit -> Norm                        Norm
  t(Unt, T_Reg) = 13;

  //  Norm -> [^?+*() Esc\|]                Norm
  t(Nor, T_Reg) = 14;

  //  Lit -> Norm                         Norm
  t(Lit, T_Reg) = 15;

  //  Lit -> [?+*() Esc\|]                  Rep .()\|
  t(Lit, T_Rep) = 16;
  t(Lit, T_LPar) = 17;
  t(Lit, T_RPar) = 18;
  t(Lit, T_Slash) = 19;
  t(Lit, T_Bar) = 20;

  return table;
}

constexpr auto lex(char c) -> Sym { return {.symbol = classify(c), .c = c}; }

constexpr bool parse(std::string_view regex) {
  auto syms = regex | std::views::transform(lex);
  auto it = syms.begin();
  using enum Symbols;
  auto stack = std::vector<Symbols>{S};
  constexpr auto tab = table();
  while (it != syms.end() && !stack.empty()) {
    if (*it == stack.back()) {
      it++;
      stack.pop_back();
      continue;
    }
    switch (tab[static_cast<size_t>(stack.back())]
               [static_cast<size_t>((*it).symbol)]) {
    case 0:
      return false;
      //  S -> Alt
    case 1: {
      stack.back() = Alt;
      break;
    }

    //  Alt -> Concat MaybeAlt
    case 2: {
      stack.back() = MAlt;
      stack.push_back(Cat);
      break;
    }

    //  MaybeAlt -> e
    case 3: {
      stack.pop_back();
      break;
    }

    //  MaybeAlt -> "|" Alt
    case 4: {
      stack.back() = Alt;
      stack.push_back(T_Bar);
      break;
    }

    //  Concat -> Repetition MaybeConcat
    case 5: {
      stack.back() = MCat;
      stack.push_back(Rep);
      break;
    }

    //  MaybeConcat -> e
    case 6: {
      stack.pop_back();
      break;
    }

    //  MaybeConcat -> Concat
    case 7: {
      stack.back() = Cat;
      break;
    }

    //  Repetition -> Unit MaybeRep
    case 8: {
      stack.back() = MRep;
      stack.push_back(Unt);
      break;
    }

    //  MaybeRep -> e
    case 9: {
      stack.pop_back();
      break;
    }

    //  MaybeRep -> "*+?"
    case 10: {
      stack.back() = T_Rep;
      break;
    }

    //  Unit -> "(" Alt ")"
    case 11: {
      stack.back() = T_RPar;
      stack.push_back(Alt);
      stack.push_back(T_LPar);
      break;
    }

    //  Unit -> "\" Lit
    case 12: {
      stack.back() = Lit;
      stack.push_back(T_Slash);
      break;
    }

    //  Unit -> Norm
    case 13: {
      stack.back() = T_Reg;
      break;
    }

    //  Norm -> [^.?+*()\\|]
    case 14: {
      stack.back() = T_Reg;
      break;
    }

    //  Lit -> Norm
    case 15: {
      stack.back() = Nor;
      break;
    }

    //  Lit -> [?+*() Esc |]
    case 16: {
      stack.back() = T_Rep;
      break;
    }
    case 17: {
      stack.back() = T_LPar;
      break;
    }
    case 18: {
      stack.back() = T_RPar;
      break;
    }
    case 19: {
      stack.back() = T_Slash;
      break;
    }
    case 20: {
      stack.back() = T_Bar;
      break;
    }
    default:
      std::unreachable();
    }
  }

  if (it != syms.end()) {
    return false;
  }

  while (!stack.empty()) {

    switch (
        tab[static_cast<size_t>(stack.back())][static_cast<size_t>(T_Eot)]) {
    case 3:
    case 6:
    case 9:
      stack.pop_back();
      break;
    default:
      return false;
    }
  }
  return true;
}
bool parseRegex(std::string_view regex) { return parse(regex); }

void test() {
  static_assert(parse("hello"));
  static_assert(!parse("(hello"));
  static_assert(parse("\\(hello"));
  static_assert(parse("hello|world"));
  static_assert(!parse(""));
  static_assert(!parse("|alt"));
  static_assert(!parse("alt|"));
  static_assert(!parse("hello||world"));
}

} // namespace toyctre
