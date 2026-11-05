#include "toyctre/lib.hpp"
#include <algorithm>
#include <cassert>
#include <ranges>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace toyctre {
/*
 *  Productions:
 *
 *  S -> Alt                            Norm (\
 *  Alt -> Concat Concat*               Norm (\
 *  Concat* -> e                        )$
 *  Concat* -> "|" Concat Concat*       |
 *  Concat -> Unit Amt Repetition*       Norm (\
 *  Repetition* -> e                    |)$
 *  Repetition* -> Unit Amt Repetition* Norm (\
 *  Amt -> e                            Norm (\|
 *  Amt -> "*+?"                        Rep
 *  Unit -> "(" Concat Concat* ")"      (
 *  Unit -> "\" Lit                     \
 *  Unit -> [^?+*()\\|]                 Norm
 *  Lit -> [^?+*()\\|]                  Norm
 *  Lit -> [?+*()\\|]                   Rep ()\|
 *
 *
 *  NT                Fi(NT)          Fo(NT)
 *  S                 Norm, (\        $
 *  Alt               Norm, (\        )$
 *  Concat            Norm, (\        |)$
 *  Repetition        Norm, (\        Norm (\|)$
 *  Unit              Norm, (\        Norm Rep (\|$
 *  Lit               All             Norm Rep (\|$
 *  Concat*           e, |            )$
 *  Repetition*       e, Norm, (\     |)$
 *  Amt               e, Rep          Norm (\|$
 */

enum class Symbols {
  S,
  Alt,
  Cat,
  Cats,
  Reps,
  Unt,
  Amt,
  Lit,
  T_Rep,
  T_LPar,
  T_RPar,
  T_Slash,
  T_Bar,
  T_Reg,
  T_Eot,

  S_Alt,
  S_Lit,
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

  //  Alt -> Concat Concat*              Norm ( Esc
  t(Alt, T_Reg) = 2;
  t(Alt, T_LPar) = 2;
  t(Alt, T_Slash) = 2;

  //  Concat* -> e                       )$
  t(Cats, T_RPar) = 3;
  t(Cats, T_Eot) = 3;

  //  Concat* -> "|" Concat Concat*     |
  t(Cats, T_Bar) = 4;

  //  Concat -> Unit Amt Repetition*    Norm ( Esc
  t(Cat, T_Reg) = 5;
  t(Cat, T_LPar) = 5;
  t(Cat, T_Slash) = 5;

  //  Repetition* -> e                    |)$
  t(Reps, T_Bar) = 6;
  t(Reps, T_RPar) = 6;
  t(Reps, T_Eot) = 6;

  //  Repetition* -> Unit Amt Repetition*               Norm ( Esc
  t(Reps, T_Reg) = 7;
  t(Reps, T_LPar) = 7;
  t(Reps, T_Slash) = 7;

  // S_Lit -> e
  t(S_Lit, T_Reg) = 8;
  t(S_Lit, T_Rep) = 8;
  t(S_Lit, T_LPar) = 8;
  t(S_Lit, T_RPar) = 8;
  t(S_Lit, T_Slash) = 8;
  t(S_Lit, T_Bar) = 8;

  //  Amt -> e                       Norm ( Esc|
  t(Amt, T_Reg) = 9;
  t(Amt, T_LPar) = 9;
  t(Amt, T_Slash) = 9;
  t(Amt, T_Bar) = 9;
  t(Amt, T_RPar) = 9;
  t(Amt, T_Eot) = 9;

  //  Amt -> "*?+"                   *
  t(Amt, T_Rep) = 10;

  //  Unit -> "(" Concat Concat* S_Alt ")"     (
  t(Unt, T_LPar) = 11;

  //  Unit -> "/" Lit                     Esc
  t(Unt, T_Slash) = 12;

  //  Unit -> Norm                        Norm
  t(Unt, T_Reg) = 13;

  // S_Alt -> e
  t(S_Alt, T_RPar) = 14;
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

using Node = std::variant<Regex, Alternative, Concatenation, Repetition, Unit>;

constexpr ParseResult parse(std::string_view regex) {
  auto syms = regex | std::views::transform(lex);
  auto it = syms.begin();
  using enum Symbols;
  auto sentinels = std::vector<Symbols>{S};
  constexpr auto tab = table();
  auto nodes = std::vector<Node>{};
  nodes.emplace_back(std::in_place_type<Regex>);
  auto steps = 0;
  while (it != syms.end() && !sentinels.empty()) {
    std::ignore = ++steps;
    if (*it == sentinels.back()) {
      if (sentinels.back() == T_Reg) {
        std::get<Repetition>(nodes.back()).unit.literal = (*it).c;
      }
      it++;
      sentinels.pop_back();
      continue;
    }
    auto rule = tab[static_cast<size_t>(sentinels.back())]
                   [static_cast<size_t>((*it).symbol)];
    switch (rule) {
    case 0:
      return {{}, false};
      //  S -> Alt
    case 1: {
      sentinels.back() = Alt;
      nodes.emplace_back(std::in_place_type<Alternative>);
      break;
    }

    //  Alt -> Concat Concat*
    case 2: {
      sentinels.back() = Cats;
      sentinels.push_back(Cat);
      nodes.emplace_back(std::in_place_type<Concatenation>);
      break;
    }

    //  Concat* -> e
    case 3: {
      sentinels.pop_back();
      auto cat = std::move(std::get<Concatenation>(nodes.back()));
      nodes.pop_back();
      std::get<Alternative>(nodes.back())
          .concatenations.emplace_back(std::move(cat));
      break;
    }

    //  Concat* -> "|" Concat Concat*
    case 4: {
      sentinels.push_back(Cat);
      sentinels.push_back(T_Bar);
      auto cat = std::move(std::get<Concatenation>(nodes.back()));
      nodes.pop_back();
      std::get<Alternative>(nodes.back())
          .concatenations.push_back(std::move(cat));
      nodes.emplace_back(std::in_place_type<Concatenation>);
      break;
    }

    //  Concat -> Unit Amt Repetition*
    case 5: {
      sentinels.back() = Reps;
      sentinels.push_back(Amt);
      sentinels.push_back(Unt);
      nodes.emplace_back(std::in_place_type<Repetition>);
      break;
    }

    //  Repetition* -> e
    case 6: {
      sentinels.pop_back();
      auto rep = std::move(std::get<Repetition>(nodes.back()));
      nodes.pop_back();
      std::get<Concatenation>(nodes.back())
          .repetitions.push_back(std::move(rep));
      break;
    }

    //  Repetition* -> Unit Amt Repetition*
    case 7: {
      sentinels.push_back(Amt);
      sentinels.push_back(Unt);
      auto rep = std::move(std::get<Repetition>(nodes.back()));
      nodes.pop_back();
      std::get<Concatenation>(nodes.back())
          .repetitions.push_back(std::move(rep));
      nodes.emplace_back(std::in_place_type<Repetition>);
      break;
    }

    // S_Lit -> e
    case 8: {
      sentinels.pop_back();
      auto &unit = std::get<Repetition>(nodes.back()).unit;
      unit.literal = (*it).c;
      unit.wasEscaped = true;
      break;
    }

    //  Amt -> e
    case 9: {
      sentinels.pop_back();
      break;
    }

    //  Amt -> "*+?"
    case 10: {
      sentinels.back() = T_Rep;
      std::get<Repetition>(nodes.back()).rep = (*it).c;
      break;
    }

    //  Unit -> "(" Alt ")"
    case 11: {
      sentinels.back() = T_RPar;
      sentinels.push_back(S_Alt);
      sentinels.push_back(Alt);
      sentinels.push_back(T_LPar);
      nodes.emplace_back(std::in_place_type<Alternative>);
      break;
    }

    //  Unit -> "\" S_Lit Lit
    case 12: {
      sentinels.back() = Lit;
      sentinels.push_back(S_Lit);
      sentinels.push_back(T_Slash);
      break;
    }

    //  Unit -> Norm
    case 13: {
      sentinels.back() = T_Reg;
      break;
    }

    // S_Alt -> e
    case 14: {
      sentinels.pop_back();
      auto alt = std::move(std::get<Alternative>(nodes.back()));
      nodes.pop_back();
      auto &unit = std::get<Repetition>(nodes.back()).unit;
      unit.regex.alternatives = alt;
      unit.isNested = true;
      break;
    }

    //  Lit -> Norm
    case 15: {
      sentinels.back() = T_Reg;
      break;
    }

    //  Lit -> [?+*() Esc |]
    case 16: {
      sentinels.back() = T_Rep;
      break;
    }
    case 17: {
      sentinels.back() = T_LPar;
      break;
    }
    case 18: {
      sentinels.back() = T_RPar;
      break;
    }
    case 19: {
      sentinels.back() = T_Slash;
      break;
    }
    case 20: {
      sentinels.back() = T_Bar;
      break;
    }
    default:
      std::unreachable();
    }
  }

  if (it != syms.end()) {
    return {{}, false};
  }

  while (!sentinels.empty()) {

    switch (tab[static_cast<size_t>(sentinels.back())]
               [static_cast<size_t>(T_Eot)]) {
    case 3: {
      sentinels.pop_back();
      auto cat = std::move(std::get<Concatenation>(nodes.back()));
      nodes.pop_back();
      std::get<Alternative>(nodes.back())
          .concatenations.emplace_back(std::move(cat));
      break;
    }
    case 6: {
      sentinels.pop_back();
      auto rep = std::move(std::get<Repetition>(nodes.back()));
      nodes.pop_back();
      std::get<Concatenation>(nodes.back())
          .repetitions.push_back(std::move(rep));
      break;
    }
    case 9:
      sentinels.pop_back();
      break;
    default:
      return {{}, false};
    }
  }
  auto alt = std::move(std::get<Alternative>(nodes.back()));
  nodes.pop_back();
  auto reg = std::move(std::get<Regex>(nodes.back()));
  reg.alternatives = alt;
  return {std::move(reg), true};
}

std::string Unit::toString() const {
  return this->isNested
             ? "(" + this->regex.toString() + ")"
             : (this->wasEscaped ? "\\" : "") + std::string{this->literal};
}

std::string Repetition::toString() const {
  return this->unit.toString() + (this->rep != 0 ? std::string{this->rep} : "");
}

std::string Concatenation::toString() const {
  return std::ranges::fold_left_first(
             this->repetitions | std::views::transform([](const auto &r) {
               return r.toString();
             }),
             [](auto l, auto r) { return l += r; })
      .value();
}

std::string Alternative::toString() const {

  return std::ranges::fold_left_first(
             this->concatenations | std::views::transform([](const auto &r) {
               return r.toString();
             }),
             [](auto l, auto r) { return (l += "|") += r; })
      .value();
}

std::string Regex::toString() const { return this->alternatives.toString(); }

ParseResult parseRegex(std::string_view regex) { return parse(regex); }

} // namespace toyctre
