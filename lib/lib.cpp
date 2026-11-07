#include "toyctre/lib.hpp"
#include <algorithm>
#include <bitset>
#include <cassert>
#include <initializer_list>
#include <print>
#include <ranges>
#include <string_view>
#include <utility>
#include <variant>
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

namespace detail {
struct Production {
  Symbol head{};
  std::vector<Symbol> body;

  constexpr Production(Symbol head, std::vector<Symbol> body)
      : head{head}, body{std::move(body)} {}
};

constexpr std::vector<Symbol> operator+(Symbol l, Symbol r) { return {l, r}; };
constexpr std::vector<Symbol> operator+(std::vector<Symbol> l, Symbol r) {
  l.push_back(r);
  return l;
}
constexpr Production operator>>=(Symbol head, std::vector<Symbol> body) {
  return Production{head, std::move(body)};
}
constexpr Production operator>>=(Symbol head, Symbol body) {
  return Production{head, {body}};
}

constexpr auto productions() {
  using enum Symbol;
  const auto epsilon = std::vector<Symbol>{};
  return std::vector<Production>{
      A_Reject >>= A_Reject,
      S >>= A_EmptyAlt + Alt + A_MakeRegex,
      Alt >>= A_EmptyCat + Cat + A_AppendCat + Cats,
      Cats >>= T_Bar + A_EmptyCat + Cat + A_AppendCat + Cats,
      Cats >>= epsilon,
      Cat >>= A_EmptyRep + Unt + Amt + A_AppendRep + Reps,
      Reps >>= A_EmptyRep + Unt + Amt + A_AppendRep + Reps,
      Reps >>= epsilon,
      Unt >>= A_RecordLit + T_Reg,
      Unt >>= T_Slash + A_RecordEscaped + Lit,
      // TODO: maybe separate out a lexer to avoid this
      Lit >>= T_Reg,
      Lit >>= T_Rep,
      Lit >>= T_LPar,
      Lit >>= T_RPar,
      Lit >>= T_Slash,
      Lit >>= T_Bar,
      Unt >>= T_LPar + A_EmptyAlt + Alt + A_MakeGroup + T_RPar,
      Amt >>= A_RecordRep + T_Rep,
      Amt >>= epsilon,
  };
}

template <size_t BodySize> struct FixedSizedProduction {
  template <typename T, size_t N> struct InPlace {
    std::array<T, N> elems{};
    size_t used{};
  };
  Symbol head{};
  InPlace<Symbol, BodySize> body_;

  [[nodiscard]] constexpr auto body() const noexcept {
    return std::span{body_.elems.data(), body_.used};
  }

  constexpr FixedSizedProduction() = default;

  constexpr FixedSizedProduction(const Production &o) {
    head = o.head;

    assert(o.body.size() <= BodySize);
    std::ranges::copy(o.body, body_.elems.begin());
    body_.used = o.body.size();
  }
};

template <size_t ProductionSize, size_t BodySize>
consteval auto fixedSizedProductions() {
  auto ret = std::array<FixedSizedProduction<BodySize>, ProductionSize>{};
  auto prods = productions();
  assert(prods.size() == ProductionSize);
  std::ranges::copy(
      std::views::transform(
          prods, [](auto &p) { return FixedSizedProduction<BodySize>{p}; }),
      ret.begin());
  return ret;
}

consteval size_t requiredBodySize(const std::vector<Production> &productions) {
  return std::ranges::max(std::views::transform(
      productions, [](const auto &p) { return p.body.size(); }));
}
} // namespace detail

constexpr auto productions = []() {
  constexpr auto szs = []() {
    auto p = detail::productions();
    return std::make_pair(p.size(), requiredBodySize(p));
  }();
  return detail::fixedSizedProductions<szs.first, szs.second>();
}();

constexpr auto NSymbols = static_cast<size_t>(Symbol::SIZE);

template <typename E, typename V, size_t N> struct EnumIndexedArray {
  std::array<V, N> elems{};

  template <typename Self> constexpr auto &&operator[](this Self &&self, E e) {
    return std::forward<Self>(self).elems[static_cast<size_t>(e)];
  }
};

constexpr auto table = []() {
  using LL1Table =
      EnumIndexedArray<Symbol, EnumIndexedArray<Symbol, size_t, NSymbols>,
                       NSymbols>;
  using SetTable =
      EnumIndexedArray<Symbol, std::bitset<NSymbols + 1>, NSymbols>;

  constexpr auto epsilon = NSymbols;
  constexpr auto terminals = static_cast<size_t>(Symbol::TERMINALS);
  constexpr auto actions = static_cast<size_t>(Symbol::ACTIONS);
  auto ret = LL1Table{};
  auto firsts = SetTable{};
  auto changed = false;
  do {
    changed = false;
    for (const auto &p : productions) {
      auto &fi = firsts[p.head];
      auto orig = fi;
      auto setEpsilon = true;
      for (auto s : p.body()) {
        if (s > Symbol::ACTIONS) {
          continue;
        }
        if (s > Symbol::TERMINALS) {
          fi.set(static_cast<size_t>(s));
          setEpsilon = false;
          break;
        }
        auto o = firsts[s];
        if (!o.test(epsilon)) {
          fi |= o;
          setEpsilon = false;
          break;
        }
        o.reset(epsilon);
        fi |= o;
      }
      if (setEpsilon) {
        fi.set(epsilon);
      }
      changed |= fi != orig;
    }
  } while (changed);

  {
    auto i = terminals;
    for (; i < actions; ++i) {
      firsts[static_cast<Symbol>(i)].set(i);
    }
    for (; i < NSymbols; ++i) {
      firsts[static_cast<Symbol>(i)].set(epsilon);
    }
  }

  auto first = [&](auto word) {
    auto fi = std::bitset<epsilon + 1>{};
    fi.set(epsilon);
    for (auto s : word) {
      if (!fi.test(epsilon))
        break;
      fi.reset(epsilon);
      fi |= firsts[s];
    }
    return fi;
  };

  auto follows = SetTable{};

  follows[Symbol::S].set(static_cast<size_t>(Symbol::T_Eot));

  do {
    changed = false;
    for (const auto &p : productions) {
      auto body = p.body();
      auto &fo_head = follows[p.head];
      while (body.size()) {
        auto cur = body.front();
        auto &fo = follows[cur];
        auto orig = fo;
        body = body.subspan(1);
        auto fi = first(body);
        fo |= fi;
        if (fi.test(epsilon)) {
          fo |= fo_head;
        }
        fo.reset(epsilon);
        changed |= fo != orig;
      }
    }
  } while (changed);

  for (auto pi = 0uz; pi < productions.size(); ++pi) {
    const auto &p = productions[pi];
    auto fi = first(p.body());
    if (fi.test(epsilon)) {
      fi |= follows[p.head];
    }

    for (auto i = terminals + 1; i < actions; ++i) {
      if (fi.test(i)) {
        auto &entry = ret[p.head][static_cast<Symbol>(i)];
        if (entry != 0 && entry != pi) {
          throw "LL1 conflict";
        }
        entry = pi;
      }
    }
  }
  return ret;
}();

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
  default:
    return Symbol::T_Reg;
  }
}

constexpr auto lex(char c) -> Sym { return {.symbol = classify(c), .c = c}; }

using Node = std::variant<Regex, Alternative, Concatenation, Repetition>;

constexpr ParseResult parse(std::string_view regex) {
  using enum Symbol;

  auto syms = std::views::iota(0) | std::views::transform([regex](auto i) {
                return i < std::ssize(regex) ? lex(regex[i]) : Sym{T_Eot, 0};
              });
  auto it = syms.begin();
  auto sentinels = std::vector<Symbol>{S};
  auto nodes = std::vector<Node>{};
  nodes.emplace_back(std::in_place_type<Regex>);
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
        reg.alternative = std::move(alt);
        break;
      }
      case A_MakeGroup: {
        auto alt = std::move(std::get<Alternative>(nodes.back()));
        nodes.pop_back();
        auto &unt = std::get<Repetition>(nodes.back());
        unt.altenative = std::move(alt);
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
      auto rule = table[cur][(*it).symbol];
      auto body = productions[rule].body();
      std::copy(body.crbegin(), body.crend(), std::back_inserter(sentinels));
    }
  }

  if (*it != T_Eot) {
    return {{}, false};
  }

  return {std::move(std::get<Regex>(nodes.back())), true};
}

std::string Repetition::toString() const {
  return (this->isNested
              ? "(" + this->altenative.toString() + ")"
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

std::string Regex::toString() const { return this->alternative.toString(); }

ParseResult parseRegex(std::string_view regex) { return parse(regex); }
} // namespace toyctre
