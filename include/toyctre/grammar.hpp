#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <cstddef>
#include <iterator>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace re::grammar {

enum class Symbol {
  Start,
  Alt,
  Cat,
  Cats,
  Reps,
  Unt,
  Amt,
  Class,
  CPos,
  CNeg,
  CPostChar,
  CPostRange,
  CEsc,

  Reg,
  Rep,
  Meta,
  Any,
  CReg,
  CNoHyphen,
  CNoCarat,
  CAny,

  Terminals_,
  T_LPar,
  T_RPar,
  T_LSquare,
  T_RSquare,
  T_Carat,
  T_Dollar,
  T_Hyphen,
  T_Dot,
  T_Rep,
  T_Question,
  T_Esc,
  T_Bar,
  T_Reg,
  T_Eot,

  Actions_,
  A_Dot,
  A_Mark,
  A_Alts,
  A_Cats,
  A_Capture,
  A_Group,
  A_Lit,
  A_Rep,
  A_NoRep,

  A_Class,
  A_CNegate,
  A_CLit,
  A_CRange,

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
  case '[':
    return T_LSquare;
  case ']':
    return T_RSquare;
  case '^':
    return T_Carat;
  case '$':
    return T_Dollar;
  case '-':
    return T_Hyphen;
  case '.':
    return T_Dot;
  case '*':
  case '+':
    return T_Rep;
  case '?':
    return T_Question;
  case '|':
    return T_Bar;
  case '\\':
    return T_Esc;
  default:
    return T_Reg;
  }
}

namespace detail {
using std::size_t;

struct Production {
  Symbol head{};
  std::vector<Symbol> body;

  consteval Production(Symbol head, std::vector<Symbol> body)
      : head{head}, body{std::move(body)} {}
};

consteval std::vector<Symbol> operator+(Symbol l, Symbol r) { return {l, r}; };
consteval std::vector<Symbol> operator+(std::vector<Symbol> l, Symbol r) {
  l.push_back(r);
  return l;
}
consteval Production operator>>=(Symbol head, std::vector<Symbol> body) {
  return Production{head, std::move(body)};
}
consteval Production operator>>=(Symbol head, Symbol body) {
  return Production{head, {body}};
}

/* https://pcre.org/original/doc/html/pcrepattern.html#SEC4
There are two different sets of metacharacters: those that are recognized
anywhere in the pattern except within square brackets, and those that are
recognized within square brackets. Outside square brackets, the metacharacters
are as follows:

  \      general escape character with several uses
  ^      assert start of string (or line, in multiline mode)
  $      assert end of string (or line, in multiline mode)
  .      match any character except newline (by default)
  [      start character class definition
  |      start of alternative branch
  (      start subpattern
  )      end subpattern
  ?      extends the meaning of (
         also 0 or 1 quantifier
         also quantifier minimizer
  *      0 or more quantifier
  +      1 or more quantifier
         also "possessive quantifier"
  {      start min/max quantifier

Part of a pattern that is in square brackets is called a "character class". In a
character class the only metacharacters are:
  \      general escape character
  ^      negate the class, but only if the first character
  -      indicates character range
  [      POSIX character class (only if followed by POSIX syntax)
  ]      terminates the character class
  */

consteval auto productionsVector() {
  using enum Symbol;
  const auto epsilon = std::vector<Symbol>{};
  return std::vector<Production>{
      A_Reject >>= A_Reject,
      Start >>= Alt + T_Eot,
      Alt >>= A_Mark + Cat + Cats + A_Alts,
      Cats >>= T_Bar + Cat + Cats,
      Cats >>= epsilon,
      Cat >>= A_Mark + Unt + Amt + Reps + A_Cats,
      Reps >>= Unt + Amt + Reps,
      Reps >>= epsilon,
      Unt >>= A_Lit + Reg,
      Unt >>= T_Esc + A_Lit + Any,

      Unt >>= T_LSquare + A_Class + Class + T_RSquare,
      Class >>= CPos,
      Class >>= T_Carat + A_CNegate + CNeg,
      CPos >>= A_CLit + CNoCarat + CPostChar,
      CNeg >>= A_CLit + CAny + CPostChar,
      CPos >>= A_CLit + T_RSquare + CPostChar,
      CNeg >>= A_CLit + T_RSquare + CPostChar,
      CPostChar >>= A_CLit + CNoHyphen + CPostChar,
      CPostChar >>= epsilon,
      CPostChar >>= T_Hyphen + A_CRange + CPostRange,
      CPostRange >>= A_CLit + CAny + CPostChar,
      CPostRange >>= epsilon,

      CPos >>= CEsc,
      CNeg >>= CEsc,
      CPostChar >>= CEsc,
      CPostRange >>= CEsc,
      CEsc >>= T_Esc + A_CLit + Any + CPostChar,

      Unt >>= T_LPar + A_Capture + Alt + A_Group + T_RPar,
      Amt >>= A_Rep + T_Rep,
      Amt >>= A_Rep + T_Question,
      Amt >>= A_NoRep,

      // Character classes

      Reg >>= T_Reg,
      Reg >>= T_RSquare,
      Reg >>= T_Dot,
      Reg >>= T_Hyphen,
      Reg >>= T_Carat,
      Reg >>= T_Dollar,

      Rep >>= T_Rep,
      Rep >>= T_Question,

      Any >>= Reg,
      Any >>= Rep,
      Any >>= T_LSquare,
      Any >>= T_LPar,
      Any >>= T_RPar,
      Any >>= T_Esc,
      Any >>= T_Bar,

      CReg >>= T_Reg,
      CReg >>= T_LSquare,
      CReg >>= T_LPar,
      CReg >>= T_RPar,
      CReg >>= T_Bar,
      CReg >>= Rep,
      CReg >>= T_Dollar,
      CReg >>= T_Dot,

      CNoCarat >>= CReg,
      CNoCarat >>= T_Hyphen,
      CNoHyphen >>= CReg,
      CNoHyphen >>= T_Carat,
      CAny >>= CReg,
      CAny >>= T_Hyphen,
      CAny >>= T_Carat,
  };
}

struct CtProduction {
  Symbol head{};
  std::span<const Symbol> body;
};

template <typename T, size_t N> struct InPlaceVector {
  std::array<T, N> data{};
  size_t used{};

  consteval InPlaceVector() = default;

  template <size_t M>
  consteval InPlaceVector(const InPlaceVector<T, M> &o) : used{o.used} {
    if (used > N) {
      throw std::out_of_range{"More elements in source than capacity"};
    }
    std::copy(o.begin(), o.end(), data.begin());
  }

  [[nodiscard]] constexpr auto begin() const noexcept { return data.begin(); }
  [[nodiscard]] constexpr auto end() const noexcept {
    return std::next(data.begin(), static_cast<ptrdiff_t>(used));
  }
  [[nodiscard]] constexpr auto size() const noexcept { return used; }

  constexpr auto push_back(auto &&v) {
    if (used + 1 > N) {
      throw std::out_of_range{"push_back to full InPlaceVector"};
    }
    data[used++] = std::forward<decltype(v)>(v);
  }

  template <typename Self>
  constexpr auto &&operator[](this Self &&self, size_t i) {
    return std::forward<Self>(self).data[i];
  }
};

// template parameters have static storage duration, so it's valid to return a
// view of them
template <InPlaceVector v> consteval auto view() {
  return std::span{v.data.data(), v.size()};
}

template <std::size_t N = 400, std::size_t M = 100>
consteval auto packedProductions() {
  constexpr auto data = [] {
    auto vec = productionsVector();
    auto oversized = InPlaceVector<Symbol, N>{};
    auto heads = std::array<Symbol, M>{};
    auto lengths = std::array<std::size_t, M>{};
    auto it = oversized.data.begin();
    for (auto i = 0uz; i < vec.size(); ++i) {
      auto prod = vec[i].body;
      heads[i] = vec[i].head;
      oversized.used += prod.size();
      lengths[i] = prod.size();
      it = std::copy(prod.begin(), prod.end(), it);
    }
    return std::make_tuple(vec.size(), oversized.size(), oversized, lengths,
                           heads);
  }();
  constexpr auto productionCount = std::get<0>(data);
  constexpr auto bodyCount = std::get<1>(data);
  constexpr auto bodies = std::get<2>(data);
  constexpr auto lengths = std::get<3>(data);
  constexpr auto heads = std::get<4>(data);

  constexpr InPlaceVector<Symbol, bodyCount> sized = bodies;

  auto ret = InPlaceVector<CtProduction, productionCount>{};
  auto rem = view<sized>();
  for (auto i = 0uz; i < productionCount; ++i) {
    ret.push_back(CtProduction{heads[i], rem.subspan(0, lengths[i])});
    rem = rem.subspan(lengths[i]);
  }
  return ret;
}

} // namespace detail

inline constexpr auto productions = detail::packedProductions();

namespace detail {

constexpr auto NSymbols = static_cast<size_t>(Symbol::Size_);

template <typename E, typename V, size_t N> struct EnumIndexedArray {
  std::array<V, N> elems{};

  template <typename Self> constexpr auto &&operator[](this Self &&self, E e) {
    return std::forward<Self>(self).elems[static_cast<size_t>(e)];
  }
};

inline constexpr auto table = []() {
  using LL1Table =
      EnumIndexedArray<Symbol, EnumIndexedArray<Symbol, size_t, NSymbols>,
                       NSymbols>;
  using SetTable =
      EnumIndexedArray<Symbol, std::bitset<NSymbols + 1>, NSymbols>;

  constexpr auto epsilon = NSymbols;
  constexpr auto terminals = static_cast<size_t>(Symbol::Terminals_);
  constexpr auto actions = static_cast<size_t>(Symbol::Actions_);
  auto ret = LL1Table{};
  auto firsts = SetTable{};
  auto changed = false;
  do {
    changed = false;
    for (const auto &p : productions) {
      auto &fi = firsts[p.head];
      auto orig = fi;
      auto setEpsilon = true;
      for (auto s : p.body) {
        if (s > Symbol::Actions_) {
          continue;
        }
        if (s > Symbol::Terminals_) {
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

  follows[Symbol::Start].set(static_cast<size_t>(Symbol::T_Eot));

  do {
    changed = false;
    for (const auto &p : productions) {
      auto body = p.body;
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
    auto fi = first(p.body);
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

// This could just be a type alias, but it's a small fixed set of instantiations
// and benchmarking finds making it an alias slower
template <Symbol nt, Symbol la> struct Produce {
  static constexpr auto production = table[nt][la];
  static constexpr auto body = productions[production].body;
  template <template <Symbol...> class List, size_t... Is>
  static List<body[Is]...> f(std::index_sequence<Is...>);
  template <template <Symbol...> class List>
  using type = decltype(f<List>(std::make_index_sequence<body.size()>{}));
};

} // namespace detail

using detail::table;

template <template <Symbol...> class List, Symbol nt, Symbol la>
using produce_list_t = typename detail::Produce<nt, la>::template type<List>;

} // namespace re::grammar
