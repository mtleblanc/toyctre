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
  A_EmptyUnit,
  A_AppendUnit,
  A_RecordEscaped,
  A_RecordLit,
  A_RecordRep,

  A_Reject,

  SIZE
};

constexpr auto productions() {
  using enum Symbols;
  return std::vector{
      std::make_pair(std::vector{A_Reject, A_Reject}, std::vector{A_Reject}),
      std::make_pair(std::vector{S, A_EmptyAlt, Alt, A_MakeRegex, T_Eot},
                     std::vector{T_Reg, T_LPar, T_Slash}),
      std::make_pair(std::vector{Alt, A_EmptyCat, Cat, A_AppendCat, Cats},
                     std::vector{T_Reg, T_LPar, T_Slash}),
      std::make_pair(
          std::vector{Cats, T_Bar, A_EmptyCat, Cat, A_AppendCat, Cats},
          std::vector{T_Bar}),
      std::make_pair(std::vector{Cats}, std::vector{T_RPar, T_Eot}),
      std::make_pair(std::vector{Cat, A_EmptyRep, A_EmptyUnit, Unt,
                                 A_AppendUnit, Amt, A_AppendRep, Reps},
                     std::vector{T_Reg, T_LPar, T_Slash}),
      std::make_pair(std::vector{Reps, A_EmptyRep, A_EmptyUnit, Unt,
                                 A_AppendUnit, Amt, A_AppendRep, Reps},
                     std::vector{T_Reg, T_LPar, T_Slash}),
      std::make_pair(std::vector{Reps}, std::vector{T_Bar, T_RPar, T_Eot}),
      // TODO: separate out a lexer to avoid this
      std::make_pair(std::vector{Unt, A_RecordLit, T_Reg}, std::vector{T_Reg}),
      std::make_pair(std::vector{Unt, T_Slash, A_RecordEscaped, Lit},
                     std::vector{T_Slash}),
      std::make_pair(std::vector{Lit, T_Reg}, std::vector{T_Reg}),
      std::make_pair(std::vector{Lit, T_Rep}, std::vector{T_Rep}),
      std::make_pair(std::vector{Lit, T_LPar}, std::vector{T_LPar}),
      std::make_pair(std::vector{Lit, T_RPar}, std::vector{T_RPar}),
      std::make_pair(std::vector{Lit, T_Slash}, std::vector{T_Slash}),
      std::make_pair(std::vector{Lit, T_Bar}, std::vector{T_Bar}),
      std::make_pair(
          std::vector{Unt, T_LPar, A_EmptyAlt, Alt, A_MakeGroup, T_RPar},
          std::vector{T_LPar}),
      std::make_pair(std::vector{Amt, A_RecordRep, T_Rep}, std::vector{T_Rep}),
      std::make_pair(std::vector{Amt},
                     std::vector{T_Reg, T_LPar, T_Slash, T_Bar, T_RPar, T_Eot}),
  };
}

constexpr auto table() {
  auto ret = std::array<std::array<int, static_cast<size_t>(Symbols::SIZE)>,
                        static_cast<size_t>(Symbols::SIZE)>{};

  auto i = 0;
  for (const auto &[prod, syms] : productions()) {
    for (auto sym : syms) {
      ret[static_cast<size_t>(prod[0])][static_cast<size_t>(sym)] = i;
    }
    ++i;
  }
  return ret;
}

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
  case 0:
    return Symbols::T_Eot;
  default:
    return Symbols::T_Reg;
  }
}

constexpr auto lex(char c) -> Sym { return {.symbol = classify(c), .c = c}; }

using Node = std::variant<Regex, Alternative, Concatenation, Repetition, Unit>;

constexpr ParseResult parse(std::string_view regex) {

  auto r = std::string(regex);
  r.append({0});

  auto syms = r | std::views::transform(lex);
  auto it = syms.begin();
  using enum Symbols;
  auto sentinels = std::vector<Symbols>{S};
  constexpr auto tab = table();
  auto nodes = std::vector<Node>{};
  nodes.emplace_back(std::in_place_type<Regex>);
  auto prods = productions();
  while (it != syms.end() && !sentinels.empty()) {
    auto cur = sentinels.back();
    sentinels.pop_back();
    if (cur > ACTIONS) {
      switch (cur) {
      case (A_EmptyAlt): {
        nodes.emplace_back(std::in_place_type<Alternative>);
        break;
      }
      case A_MakeRegex: {
        auto alt = std::move(std::get<Alternative>(nodes.back()));
        nodes.pop_back();
        auto &reg = std::get<Regex>(nodes.back());
        reg.alternatives = std::move(alt);
        break;
      }
      case A_MakeGroup: {
        auto alt = std::move(std::get<Alternative>(nodes.back()));
        nodes.pop_back();
        auto &unt = std::get<Unit>(nodes.back());
        unt.regex = Regex{std::move(alt)};
        unt.isNested = true;
        break;
      }
      case A_EmptyCat: {
        nodes.emplace_back(std::in_place_type<Concatenation>);
        break;
      }
      case A_AppendCat: {
        auto cat = std::move(std::get<Concatenation>(nodes.back()));
        nodes.pop_back();
        auto &alt = std::get<Alternative>(nodes.back());
        alt.concatenations.push_back(std::move(cat));
        break;
      }
      case A_EmptyRep: {
        nodes.emplace_back(std::in_place_type<Repetition>);
        break;
      }
      case A_AppendRep: {
        auto rep = std::move(std::get<Repetition>(nodes.back()));
        nodes.pop_back();
        auto &cat = std::get<Concatenation>(nodes.back());
        cat.repetitions.push_back(std::move(rep));
        break;
        break;
      }
      case A_EmptyUnit: {
        nodes.emplace_back(std::in_place_type<Unit>);
        break;
      }
      case A_AppendUnit: {
        auto unit = std::move(std::get<Unit>(nodes.back()));
        nodes.pop_back();
        auto &rep = std::get<Repetition>(nodes.back());
        rep.unit = std::move(unit);
        break;
      }
      case A_RecordEscaped: {
        auto &unit = std::get<Unit>(nodes.back());
        unit.literal = (*it).c;
        unit.wasEscaped = true;
        break;
      }
      case A_RecordLit: {
        auto &unit = std::get<Unit>(nodes.back());
        unit.literal = (*it).c;
        break;
      }
      case A_RecordRep: {
        auto &rep = std::get<Repetition>(nodes.back());
        rep.rep = (*it).c;
        break;
      }

      case A_Reject: {
        return {{}, false};
      }
      default:
        std::unreachable();
      }

    } else if (cur > TERMINALS) {
      if (*it != cur) {
        return {{}, false};
      }
      ++it;
    } else {
      auto rule =
          tab[static_cast<size_t>(cur)][static_cast<size_t>((*it).symbol)];
      const auto &prod = prods[rule].first;
      for (auto i = prod.size(); i-- > 1;) {
        sentinels.push_back(prod[i]);
      }
    }
  }

  if (it != syms.end() || !sentinels.empty()) {
    return {{}, false};
  }

  return {std::move(std::get<Regex>(nodes.back())), true};
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
