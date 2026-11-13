#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

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

consteval auto dynamicProductions() {
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
      Unt >>= A_Lit + T_Reg,
      Unt >>= T_Esc + A_Esc + Lit,
      Lit >>= T_Reg,
      Lit >>= T_Rep,
      Lit >>= T_LPar,
      Lit >>= T_RPar,
      Lit >>= T_Esc,
      Lit >>= T_Bar,
      Unt >>= T_LPar + Alt + A_Group + T_RPar,
      Amt >>= A_Rep + T_Rep,
      Amt >>= epsilon,
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

  consteval static InPlaceVector flatten(std::ranges::range auto &&r) {
    auto ret = InPlaceVector{};
    auto it = ret.data.begin();
    for (const auto &v : r) {
      ret.used += v.size();
      if (ret.used > N) {
        throw std::out_of_range{"More elements in range than capacity"};
      }
      it = std::copy(v.begin(), v.end(), it);
    }
    return ret;
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

template <typename F = decltype([]() { return dynamicProductions(); }),
          size_t N = 50>
consteval auto packedProductions() {
  constexpr auto oversized = InPlaceVector<Symbol, N>::flatten(
      F{}() | std::ranges::views::transform([](auto p) { return p.body; }));
  constexpr InPlaceVector<Symbol, oversized.size()> sized = oversized;
  constexpr auto v = view<sized>();

  constexpr auto M = F{}().size();

  auto ret = InPlaceVector<CtProduction, M>{};
  auto rem = v;
  for (const auto &p : F{}()) {
    ret.push_back(CtProduction{p.head, rem.subspan(0, p.body.size())});
    rem = rem.subspan(p.body.size());
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

} // namespace detail

using detail::table;

template <Symbol nt, Symbol la> struct Produce {
  static constexpr auto production = table[nt][la];
  static constexpr auto body = productions[production].body;
  template <template <Symbol...> class List, size_t... Is>
  static List<body[Is]...> f(std::index_sequence<Is...>);
  template <template <Symbol...> class List>
  using type = decltype(f<List>(std::make_index_sequence<body.size()>{}));
};

template <template <Symbol...> class List, Symbol nt, Symbol la>
using produce_list_t = typename Produce<nt, la>::template type<List>;

} // namespace toyctre::grammar
