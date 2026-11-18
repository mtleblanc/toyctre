#pragma once

#include "toyctre/ct_base.hpp"
#include "toyctre/grammar.hpp"
#include "toyctre/regex_nodes.hpp"
#include <array>
#include <concepts>
#include <cstddef>
#include <utility>

namespace re::ct {
using grammar::Symbol;

namespace helpers {
using std::size_t;

template <typename T> struct Id {
  using type = T;
};

template <size_t, typename> struct Indexed {};
template <typename, typename...> struct IndexedAll;
template <size_t... Is, typename... Ts>
struct IndexedAll<std::index_sequence<Is...>, Ts...> : Indexed<Is, Ts>... {};

template <size_t I, typename T> Id<T> pick(Indexed<I, T>);
template <size_t I, typename... Ts>
using at_t = typename decltype(pick<I>(
    IndexedAll<std::index_sequence_for<Ts...>, Ts...>{}))::type;

} // namespace helpers

namespace progress {
using std::size_t;

template <RegexString r, size_t N> struct Progress {
  static_assert(N <= r.size(), "Progress advanced past end of string");

  using next_t = Progress<r, N + 1>;

  static constexpr auto at_eot_v = N == r.size();
  static constexpr auto current_char_v = r.data[N];

  static constexpr auto lex_v =
      at_eot_v ? Symbol::T_Eot : grammar::lexChar(current_char_v);

  // specifying size_t return type instead of auto lets the compiler skip
  // instantiation if not referenced. The lambda uses a significant portion of
  // gcc memory despite the number of Progress instantiations being only N+1
  static constexpr size_t capture_number_v = [] {
    auto ret = 0uz;
    for (auto i = 0uz; i < N; ++i) {
      if (r.data[i] == '\\') {
        ++i;
      } else if (r.data[i] == '(') {
        ++ret;
      }
    }
    return ret;
  }();
};

template <typename T>
concept IsEot = T::at_eot_v;

} // namespace progress

namespace state {
using ctn::IsRegex;

template <Symbol... ss> struct Sentinels {
  // this is only called on a Sentinels from produce_t, so only the dozen or so
  // productions actually get instantiated, and the tt... will tend to be unique
  // but aliases don't get cached
  template <Symbol... tt> using append = Sentinels<ss..., tt...>;
};
template <typename...> struct Nodes;
template <typename, typename, typename> struct State;

struct Mark;
template <size_t> struct CaptureMark;

struct ParseFailure {};

template <typename> inline constexpr auto is_result_v = false;
template <IsRegex R> inline constexpr auto is_result_v<R> = true;
template <> inline constexpr auto is_result_v<ParseFailure> = true;
template <typename T>
concept IsResult = is_result_v<T>;

} // namespace state
using namespace state;

namespace consume {
using enum Symbol;

template <Symbol t, typename P, typename S, typename N> struct ConsumeTerminal {
  using type = ParseFailure;
};

// EOT check is unnecessary since the IsEot specialization is more specific, but
// it is included for clarity
template <Symbol t, typename P, typename S, typename N>
  requires(t != T_Eot && t == P::lex_v)
struct ConsumeTerminal<t, P, S, N> {
  using type = State<typename P::next_t, S, N>;
};

template <progress::IsEot P, typename S, typename N>
struct ConsumeTerminal<T_Eot, P, S, N> {
  using type = State<P, S, N>;
};

} // namespace consume
using consume::ConsumeTerminal;

namespace action {
using namespace ctn;
using enum Symbol;

template <Symbol, char, typename> struct NextNodes {
  static_assert(false, "Unrecognized action");
};
template <Symbol a, char c, typename N>
using next_nodes_t = typename NextNodes<a, c, N>::type;

template <template <typename...> class, typename> struct Merge;
template <template <typename...> class List, typename... Ts>
struct Merge<List, Nodes<Ts...>> {
  static constexpr auto mark_index = []() {
    auto marks = std::array<bool, sizeof...(Ts)>{{std::same_as<Mark, Ts>...}};
    auto index = std::find(marks.begin(), marks.end(), true);
    return static_cast<size_t>(std::distance(marks.begin(), index));
  }();

  static_assert(mark_index < sizeof...(Ts), "Mark not found");

  template <size_t... Is>
  static List<helpers::at_t<mark_index - Is - 1, Ts...>...>
      merge_f(std::index_sequence<Is...>);
  using merged = decltype(merge_f(std::make_index_sequence<mark_index>{}));

  template <size_t... Is>
  static Nodes<merged, helpers::at_t<mark_index + Is + 1, Ts...>...>
      next_f(std::index_sequence<Is...>);
  using type = decltype(next_f(
      std::make_index_sequence<sizeof...(Ts) - mark_index - 1>{}));
};

template <template <typename...> class List, typename R, typename... Rs>
struct Merge<List, Nodes<R, Mark, Rs...>> {
  using type = Nodes<R, Rs...>;
};

template <char c, typename... Rs> struct NextNodes<A_Mark, c, Nodes<Rs...>> {
  using type = Nodes<Mark, Rs...>;
};

template <char c, typename N>
struct NextNodes<A_Alts, c, N> : Merge<Alternatives, N> {};

template <char c, typename N>
struct NextNodes<A_Cats, c, N> : Merge<Sequence, N> {};

template <char c, typename R, typename... Rs, size_t C>
struct NextNodes<A_Group, c, Nodes<R, CaptureMark<C>, Rs...>> {
  using type = Nodes<Group<R, C>, Rs...>;
};

template <char c, typename... Rs> struct NextNodes<A_Lit, c, Nodes<Rs...>> {
  using type = Nodes<LiteralString<c>, Rs...>;
};

template <char c, typename R, typename... Rs>
struct NextNodes<A_Rep, c, Nodes<R, Rs...>> {
  using type = Nodes<Repetition<R, c>, Rs...>;
};

template <char c, typename... Rs> struct NextNodes<A_NoRep, c, Nodes<Rs...>> {
  using type = Nodes<Rs...>;
};

template <char c, char d, char... ds, typename... Rs>
struct NextNodes<A_NoRep, c,
                 Nodes<LiteralString<d>, LiteralString<ds...>, Rs...>> {
  using type = Nodes<LiteralString<ds..., d>, Rs...>;
};

template <Symbol a, typename P, typename S, typename N> struct DoAction {
  using type = State<P, S, next_nodes_t<a, P::current_char_v, N>>;
};

template <typename P, typename S, typename... Rs>
struct DoAction<A_Capture, P, S, Nodes<Rs...>> {
  using type = State<P, S, Nodes<CaptureMark<P::capture_number_v>, Rs...>>;
};

template <typename P, typename S, typename N>
struct DoAction<A_Reject, P, S, N> {
  using type = ParseFailure;
};

} // namespace action
using action::DoAction;

namespace produce {
using enum Symbol;

template <Symbol nt, Symbol la>
using produce_t = grammar::produce_list_t<Sentinels, nt, la>;

template <Symbol nt, Symbol p, Symbol... ss>
using predict_t = typename produce_t<nt, p>::template append<ss...>;

} // namespace produce
using produce::predict_t;
using produce::produce_t;

namespace iterate {
constexpr bool isAction(Symbol s) { return s > Symbol::Actions_; }
constexpr bool isTerminal(Symbol s) {
  return s > Symbol::Terminals_ && s < Symbol::Actions_;
}

template <typename> struct Step {
  static_assert(false, "Invalid state");
};

template <typename R, progress::IsEot P>
struct Step<State<P, Sentinels<>, Nodes<R>>> {
  static_assert(IsRegex<R>, "Parse ended with non-regex on stack");
  using type = R;
};

template <typename R, typename P> struct Step<State<P, Sentinels<>, Nodes<R>>> {
  using type = ParseFailure;
};

template <typename P, Symbol t, Symbol... ss, typename N>
  requires(isAction(t))
struct Step<State<P, Sentinels<t, ss...>, N>>
    : DoAction<t, P, Sentinels<ss...>, N> {};

template <typename P, Symbol t, Symbol... ss, typename N>
  requires(isTerminal(t))
struct Step<State<P, Sentinels<t, ss...>, N>>
    : ConsumeTerminal<t, P, Sentinels<ss...>, N> {};

template <typename P, Symbol t, Symbol... ss, typename N>
struct Step<State<P, Sentinels<t, ss...>, N>> {
  using type = State<P, predict_t<t, P::lex_v, ss...>, N>;
};

template <IsResult R> struct Step<R> {
  using type = R;
};

template <RegexString regex> struct InitialState {
  using type = State<progress::Progress<regex.data, 0>,
                     Sentinels<Symbol::Start>, Nodes<>>;
};
template <RegexString regex>
using initial_state_t = typename InitialState<regex>::type;

template <typename, size_t> struct Iter;
template <typename S, size_t N> using iter_t = typename Iter<S, N>::type;

template <typename S> struct Iter<S, 0> : Step<S> {};
template <typename S, size_t N> struct Iter : Iter<iter_t<S, N - 1>, N - 1> {};

template <typename S, size_t N = 0>
struct IterTilResult : IterTilResult<iter_t<S, N>, N + 1> {};

template <IsResult S, size_t N> struct IterTilResult<S, N> {
  using type = S;
};
} // namespace iterate

} // namespace re::ct
