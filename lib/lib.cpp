#include "toyctre/lib.hpp"
#include <algorithm>
#include <cassert>
#include <initializer_list>
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

struct Production {
  Symbol head{};
  std::vector<Symbol> body;
  std::vector<Symbol> predict;

  constexpr Production(Symbol head, std::vector<Symbol> body,
                       std::vector<Symbol> predict)
      : head{head}, body{std::move(body)}, predict{std::move(predict)} {}

  constexpr auto operator|(Symbol predict) {
    this->predict.push_back(predict);
    return *this;
  }
  constexpr auto operator|(std::vector<Symbol> predict) {
    std::copy(predict.cbegin(), predict.cend(),
              std::back_inserter(this->predict));
    return *this;
  }
};

constexpr std::vector<Symbol> operator+(Symbol l, Symbol r) { return {l, r}; };
constexpr std::vector<Symbol> operator+(std::vector<Symbol> l, Symbol r) {
  l.push_back(r);
  return l;
}
constexpr Production operator>>(Symbol head, std::vector<Symbol> body) {
  return Production{head, std::move(body), {}};
}
constexpr Production operator>>(Symbol head, Symbol body) {
  return Production{head, {body}, {}};
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wparentheses"
constexpr auto productions() {
  using enum Symbol;
  const auto epsilon = std::vector<Symbol>{};
  return std::vector<Production>{
      A_Reject >> A_Reject | A_Reject,
      S >> A_EmptyAlt + Alt + A_MakeRegex | T_Reg + T_LPar + T_Slash,
      Alt >> A_EmptyCat + Cat + A_AppendCat + Cats | T_Reg + T_LPar + T_Slash,
      Cats >> T_Bar + A_EmptyCat + Cat + A_AppendCat + Cats | T_Bar,
      Cats >> epsilon | T_RPar + T_Eot,
      Cat >> A_EmptyRep + Unt + Amt + A_AppendRep + Reps |
          T_Reg + T_LPar + T_Slash,
      Reps >> A_EmptyRep + Unt + Amt + A_AppendRep + Reps |
          T_Reg + T_LPar + T_Slash,
      Reps >> epsilon | T_Bar + T_RPar + T_Eot,
      Unt >> A_RecordLit + T_Reg | T_Reg,
      Unt >> T_Slash + A_RecordEscaped + Lit | T_Slash,
      // TODO: maybe separate out a lexer to avoid this
      Lit >> T_Reg | T_Reg,
      Lit >> T_Rep | T_Rep,
      Lit >> T_LPar | T_LPar,
      Lit >> T_RPar | T_RPar,
      Lit >> T_Slash | T_Slash,
      Lit >> T_Bar | T_Bar,
      Unt >> T_LPar + A_EmptyAlt + Alt + A_MakeGroup + T_RPar | T_LPar,
      Amt >> A_RecordRep + T_Rep | T_Rep,
      Amt >> epsilon | T_Reg + T_LPar + T_Slash + T_Bar + T_RPar + T_Eot,
  };
}
#pragma GCC diagnostic pop

constexpr auto test() {}

constexpr auto table() {
  auto ret = std::array<std::array<int, static_cast<size_t>(Symbol::SIZE)>,
                        static_cast<size_t>(Symbol::SIZE)>{};

  auto i = 0;
  for (const auto &[nonterminal, prod, syms] : productions()) {
    for (auto sym : syms) {
      ret[static_cast<size_t>(nonterminal)][static_cast<size_t>(sym)] = i;
    }
    ++i;
  }
  return ret;
}

struct Sym {
  Symbol symbol;
  char c;
  constexpr bool operator==(Symbol o) const { return symbol == o; };
};

constexpr auto classify(char c) {
  switch (c) {
  case '(':
    return Symbol::T_LPar;
  case ')':
    return Symbol::T_RPar;
  case '+':
  case '*':
  case '?':
    return Symbol::T_Rep;
  case '\\':
    return Symbol::T_Slash;
  case '|':
    return Symbol::T_Bar;
  case 0:
    return Symbol::T_Eot;
  default:
    return Symbol::T_Reg;
  }
}

constexpr auto lex(char c) -> Sym { return {.symbol = classify(c), .c = c}; }

using Node = std::variant<Regex, Alternative, Concatenation, Repetition>;

constexpr ParseResult parse(std::string_view regex) {
  using enum Symbol;

  auto r = std::string(regex);
  r.append({0});

  auto syms = std::views::iota(0) | std::views::transform([regex](auto i) {
                return i < std::ssize(regex) ? lex(regex[i]) : Sym{T_Eot, 0};
              });
  auto it = syms.begin();
  auto sentinels = std::vector<Symbol>{S};
  constexpr auto tab = table();
  auto nodes = std::vector<Node>{};
  nodes.emplace_back(std::in_place_type<Regex>);
  auto prods = productions();
  while (!sentinels.empty()) {
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
        auto &unt = std::get<Repetition>(nodes.back());
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
      case A_RecordEscaped: {
        auto &unit = std::get<Repetition>(nodes.back());
        unit.literal = (*it).c;
        unit.wasEscaped = true;
        break;
      }
      case A_RecordLit: {
        auto &unit = std::get<Repetition>(nodes.back());
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
      const auto &prod = prods[rule].body;
      std::copy(prod.crbegin(), prod.crend(), std::back_inserter(sentinels));
    }
  }

  if (*it != T_Eot) {
    return {{}, false};
  }

  return {std::move(std::get<Regex>(nodes.back())), true};
}

std::string Repetition::toString() const {
  return (this->isNested
              ? "(" + this->regex.toString() + ")"
              : (this->wasEscaped ? "\\" : "") + std::string{this->literal}) +
         (this->rep != 0 ? std::string{this->rep} : "");
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
