#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace toyctre {
namespace compile_time {

namespace helpers {
using std::size_t;

template <bool, typename, typename> struct If;
template <typename T, typename F> struct If<true, T, F> {
  using type = typename T::type;
};
template <typename T, typename F> struct If<false, T, F> {
  using type = typename F::type;
};

template <typename T> struct Id {
  using type = T;
};

template <typename, typename> struct Concat;
template <typename T, template <T...> class List, T... As, T... Bs>
struct Concat<List<As...>, List<Bs...>> : Id<List<As..., Bs...>> {};
template <template <typename...> class List, typename... Ts, typename... Us>
struct Concat<List<Ts...>, List<Us...>> : Id<List<Ts..., Us...>> {};

template <typename ListA, typename ListB>
using concat_t = typename Concat<ListA, ListB>::type;

template <typename, typename> struct Push;
template <typename T, template <typename...> class List, typename... Ts>
struct Push<T, List<Ts...>> : Id<List<T, Ts...>> {};
template <typename T, typename List>
using push_t = typename Push<T, List>::type;

template <typename, typename> struct ReplaceHead;
template <typename T, template <typename...> class List, typename H,
          typename... Ts>
struct ReplaceHead<T, List<H, Ts...>> : Id<List<T, Ts...>> {};

template <typename> struct Head;
template <template <typename...> class List, typename H, typename... Ts>
struct Head<List<H, Ts...>> : Id<H> {};
template <typename List> using head_t = typename Head<List>::type;

template <size_t, typename> struct Indexed {};
template <typename, typename...> struct IndexedAll;
template <size_t... Is, typename... Ts>
struct IndexedAll<std::index_sequence<Is...>, Ts...> : Indexed<Is, Ts>... {};

template <size_t I, typename T> Id<T> pick(Indexed<I, T>);
template <size_t I, typename... Ts>
using at_t = typename decltype(pick<I>(
    IndexedAll<std::index_sequence_for<Ts...>, Ts...>{}))::type;

} // namespace helpers

namespace detail {
using helpers::at_t;
using helpers::concat_t;
using helpers::head_t;
using helpers::Id;
using helpers::If;
using helpers::Push;
using helpers::ReplaceHead;
using std::size_t;

template <size_t N> struct RegexString {
  constexpr RegexString(const char (&str)[N]) { std::copy_n(str, N, data); }
  char data[N];
  constexpr auto size() const { return N; }
};

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

namespace parse_tree {

template <typename T> inline constexpr auto is_regex_v = false;
template <typename T>
concept IsRegex = is_regex_v<T>;

template <IsRegex R> struct Group {
  static constexpr std::string toString() {
    return std::string{"("} + R::toString() + ')';
  }
};
template <IsRegex R> inline constexpr auto is_regex_v<Group<R>> = true;

template <IsRegex... Ns> struct Sequence {
  static constexpr std::string toString() { return (Ns::toString() + ...); }
};
template <IsRegex... Ns>
inline constexpr auto is_regex_v<Sequence<Ns...>> = true;

template <IsRegex... Ns> struct Alternatives {
  static constexpr std::string toString() {
    std::string res = ((Ns::toString() + "|") + ...);
    return res.substr(0, res.size() - 1);
  }
};
template <IsRegex... Ns>
inline constexpr auto is_regex_v<Alternatives<Ns...>> = true;

template <IsRegex R, char r> struct Repetition {
  static constexpr std::string toString() { return R::toString() + r; }
};
template <IsRegex R, char r>
inline constexpr auto is_regex_v<Repetition<R, r>> = true;

template <bool e, char c> struct Literal {
  static constexpr std::string toString() {
    if constexpr (e) {
      return std::string{'\\', c};
    }
    return std::string{c};
  }
};
template <bool e, char c>
inline constexpr auto is_regex_v<Literal<e, c>> = true;

} // namespace parse_tree
using parse_tree::Alternatives;
using parse_tree::Group;
using parse_tree::IsRegex;
using parse_tree::Literal;
using parse_tree::Repetition;
using parse_tree::Sequence;

namespace progress {

template <RegexString r, size_t N> struct Progress {
  static_assert(N < r.size(), "Progress advanced past end of string");
  static constexpr auto at_eot = N + 1 == r.size();
  static constexpr auto current_char = r.data[N];
  using next = Progress<r, N + 1>;
};

template <typename P> inline constexpr auto is_progress_v = false;
template <RegexString r, size_t N>
inline constexpr auto is_progress_v<Progress<r, N>> = true;
template <typename P>
concept IsProgress = is_progress_v<P>;

template <typename T>
concept IsEot = IsProgress<T> && T::at_eot;

template <IsProgress P>
inline constexpr auto lex_v =
    P::at_eot ? Symbol::T_Eot : lexChar(P::current_char);

} // namespace progress
using progress::IsEot;
using progress::IsProgress;
using progress::lex_v;
using progress::Progress;

namespace state {

template <Symbol...> struct Sen {};
template <typename S> inline constexpr auto is_sentinels_v = false;
template <Symbol... ss> inline constexpr auto is_sentinels_v<Sen<ss...>> = true;
template <typename S>
concept IsSentinels = is_sentinels_v<S>;

struct Mark {};
template <typename T>
concept IsNodeListItem = IsRegex<T> || std::same_as<T, Mark>;

template <IsNodeListItem...> struct Nodes {};
template <typename N> inline constexpr auto is_nodes_v = false;
template <IsNodeListItem... Ns>
inline constexpr auto is_nodes_v<Nodes<Ns...>> = true;
template <typename N>
concept IsNodes = is_nodes_v<N>;

template <IsProgress, IsSentinels, IsNodes> struct State;
template <typename> inline constexpr auto is_state_v = false;
template <IsProgress P, IsSentinels S, IsNodes N>
inline constexpr auto is_state_v<State<P, S, N>> = true;
template <typename S>
concept IsState = is_state_v<S>;

struct ParseFailure {};
template <IsRegex R> struct ParseSuccess {
  using result = R;
};

template <typename> inline constexpr auto is_result_v = false;
template <typename R> inline constexpr auto is_result_v<ParseSuccess<R>> = true;
template <> inline constexpr auto is_result_v<ParseFailure> = true;
template <typename T>
concept IsResult = is_result_v<T>;

template <typename T>
concept IsResultOrState = IsResult<T> || IsState<T>;

} // namespace state
using state::IsNodeListItem;
using state::IsNodes;
using state::IsResult;
using state::IsResultOrState;
using state::IsSentinels;
using state::IsState;
using state::Mark;
using state::Nodes;
using state::ParseFailure;
using state::ParseSuccess;
using state::Sen;
using state::State;

namespace consume {

template <Symbol t, IsProgress P, IsSentinels S, IsNodes N>
struct ConsumeTerminal : Id<ParseFailure> {};

// EOT check is unnecessary since the IsEot specialization is more specific, but
// it is included for clarity
template <Symbol t, IsProgress P, IsSentinels S, IsNodes N>
  requires(t != Symbol::T_Eot && t == lex_v<P>)
struct ConsumeTerminal<t, P, S, N> : Id<State<typename P::next, S, N>> {};

template <IsEot P, IsSentinels S, IsNodes N>
struct ConsumeTerminal<Symbol::T_Eot, P, S, N> : Id<State<P, S, N>> {};

} // namespace consume
using consume::ConsumeTerminal;

namespace action {
using enum Symbol;

template <Symbol, char, IsNodes> struct NextNodes {
  static_assert(false, "Unrecognized action");
};
template <Symbol a, char c, IsNodes N>
using next_nodes_t = typename NextNodes<a, c, N>::type;

template <template <typename...> class, IsNodes> struct Merge;
template <template <typename...> class List, IsNodeListItem... Ts>
struct Merge<List, Nodes<Ts...>> {
  static constexpr auto mark_index = []() {
    auto marks = std::array<bool, sizeof...(Ts)>{{std::same_as<Mark, Ts>...}};
    auto index = std::find(marks.begin(), marks.end(), true);
    return static_cast<size_t>(std::distance(marks.begin(), index));
  }();

  static_assert(mark_index < sizeof...(Ts), "Mark not found");

  template <size_t... Is>
  static Id<List<at_t<mark_index - Is - 1, Ts...>...>>
      merge_f(std::index_sequence<Is...>);
  using merged =
      typename decltype(merge_f(std::make_index_sequence<mark_index>{}))::type;

  template <size_t... Is>
  static Id<Nodes<merged, at_t<mark_index + Is + 1, Ts...>...>>
      next_f(std::index_sequence<Is...>);
  using type = typename decltype(next_f(
      std::make_index_sequence<sizeof...(Ts) - mark_index - 1>{}))::type;
};

template <template <typename...> class List, IsRegex R, IsNodeListItem... Rs>
struct Merge<List, Nodes<R, Mark, Rs...>> : Push<R, Nodes<Rs...>> {};

template <char c, IsNodes N> struct NextNodes<A_Mark, c, N> : Push<Mark, N> {};

template <char c, IsNodes N>
struct NextNodes<A_Alts, c, N> : Merge<Alternatives, N> {};
template <char c, IsNodes N>
struct NextNodes<A_Cats, c, N> : Merge<Sequence, N> {};
template <char c, IsNodes N>
struct NextNodes<A_Group, c, N> : ReplaceHead<Group<head_t<N>>, N> {};

template <char c, IsNodes N>
struct NextNodes<A_Lit, c, N> : Push<Literal<false, c>, N> {};
template <char c, IsNodes N>
struct NextNodes<A_Esc, c, N> : Push<Literal<true, c>, N> {};
template <char c, IsNodes N>
struct NextNodes<A_Rep, c, N> : ReplaceHead<Repetition<head_t<N>, c>, N> {};

template <Symbol a, IsProgress P, IsSentinels S, IsNodes N>
struct DoAction : Id<State<P, S, next_nodes_t<a, P::current_char, N>>> {};
template <IsProgress P, IsSentinels S, IsNodes N>
struct DoAction<A_Reject, P, S, N> : Id<ParseFailure> {};

} // namespace action
using action::DoAction;

namespace produce {
using enum Symbol;

template <Symbol, Symbol> struct Produce {
  using type = Sen<A_Reject>;
};

template <size_t> struct Prod;

template <> struct Prod<1> : Id<Sen<Alt, T_Eot>> {};                     // S
template <> struct Prod<2> : Id<Sen<A_Mark, Cat, Cats, A_Alts>> {};      // Alt
template <> struct Prod<3> : Id<Sen<T_Bar, Cat, Cats>> {};               // Cats
template <> struct Prod<4> : Id<Sen<>> {};                               // Cats
template <> struct Prod<5> : Id<Sen<A_Mark, Unt, Amt, Reps, A_Cats>> {}; // Cat
template <> struct Prod<6> : Id<Sen<Unt, Amt, Reps>> {};                 // Reps
template <> struct Prod<7> : Id<Sen<>> {};                               // Reps
template <> struct Prod<8> : Id<Sen<A_Lit, T_Reg>> {};                   // Unt
template <> struct Prod<9> : Id<Sen<T_Esc, A_Esc, Lit>> {};              // Unt
template <> struct Prod<10> : Id<Sen<T_Reg>> {};                         // Lit
template <> struct Prod<11> : Id<Sen<T_Rep>> {};                         // Lit
template <> struct Prod<12> : Id<Sen<T_LPar>> {};                        // Lit
template <> struct Prod<13> : Id<Sen<T_RPar>> {};                        // Lit
template <> struct Prod<14> : Id<Sen<T_Esc>> {};                         // Lit
template <> struct Prod<15> : Id<Sen<T_Bar>> {};                         // Lit
template <> struct Prod<16> : Id<Sen<T_LPar, Alt, A_Group, T_RPar>> {};  // Unt
template <> struct Prod<17> : Id<Sen<A_Rep, T_Rep>> {};                  // Amt
template <> struct Prod<18> : Id<Sen<>> {};                              // Amt
template <> struct Produce<Start, T_LPar> : Prod<1> {};
template <> struct Produce<Start, T_Esc> : Prod<1> {};
template <> struct Produce<Start, T_Reg> : Prod<1> {};
template <> struct Produce<Alt, T_LPar> : Prod<2> {};
template <> struct Produce<Alt, T_Esc> : Prod<2> {};
template <> struct Produce<Alt, T_Reg> : Prod<2> {};
template <> struct Produce<Cat, T_LPar> : Prod<5> {};
template <> struct Produce<Cat, T_Esc> : Prod<5> {};
template <> struct Produce<Cat, T_Reg> : Prod<5> {};
template <> struct Produce<Cats, T_RPar> : Prod<4> {};
template <> struct Produce<Cats, T_Bar> : Prod<3> {};
template <> struct Produce<Cats, T_Eot> : Prod<4> {};
template <> struct Produce<Reps, T_LPar> : Prod<6> {};
template <> struct Produce<Reps, T_RPar> : Prod<7> {};
template <> struct Produce<Reps, T_Esc> : Prod<6> {};
template <> struct Produce<Reps, T_Bar> : Prod<7> {};
template <> struct Produce<Reps, T_Reg> : Prod<6> {};
template <> struct Produce<Reps, T_Eot> : Prod<7> {};
template <> struct Produce<Unt, T_LPar> : Prod<16> {};
template <> struct Produce<Unt, T_Esc> : Prod<9> {};
template <> struct Produce<Unt, T_Reg> : Prod<8> {};
template <> struct Produce<Amt, T_Rep> : Prod<17> {};
template <> struct Produce<Amt, T_LPar> : Prod<18> {};
template <> struct Produce<Amt, T_RPar> : Prod<18> {};
template <> struct Produce<Amt, T_Esc> : Prod<18> {};
template <> struct Produce<Amt, T_Bar> : Prod<18> {};
template <> struct Produce<Amt, T_Reg> : Prod<18> {};
template <> struct Produce<Amt, T_Eot> : Prod<18> {};
template <> struct Produce<Lit, T_Rep> : Prod<11> {};
template <> struct Produce<Lit, T_LPar> : Prod<12> {};
template <> struct Produce<Lit, T_RPar> : Prod<13> {};
template <> struct Produce<Lit, T_Esc> : Prod<14> {};
template <> struct Produce<Lit, T_Bar> : Prod<15> {};
template <> struct Produce<Lit, T_Reg> : Prod<10> {};

template <Symbol nt, Symbol la>
using produce_t = typename produce::Produce<nt, la>::type;

template <Symbol nt, IsProgress P, IsSentinels S, IsNodes N>
struct Predict : Id<State<P, concat_t<produce_t<nt, lex_v<P>>, S>, N>> {};

} // namespace produce
using produce::Predict;
using produce::produce_t;

namespace iterate {
constexpr bool isAction(Symbol s) { return s > Symbol::Actions_; }
constexpr bool isTerminal(Symbol s) {
  return s > Symbol::Terminals_ && s < Symbol::Actions_;
}

template <IsResultOrState> struct Step {
  static_assert(false, "Invalid state");
};

template <IsRegex R, IsEot P>
struct Step<State<P, Sen<>, Nodes<R>>> : Id<ParseSuccess<R>> {};

template <IsRegex R, IsProgress P>
struct Step<State<P, Sen<>, Nodes<R>>> : Id<ParseFailure> {};

template <IsProgress P, Symbol t, Symbol... ss, IsNodes N>
  requires(isAction(t))
struct Step<State<P, Sen<t, ss...>, N>> : DoAction<t, P, Sen<ss...>, N> {};

template <IsProgress P, Symbol t, Symbol... ss, IsNodes N>
  requires(isTerminal(t))
struct Step<State<P, Sen<t, ss...>, N>> : ConsumeTerminal<t, P, Sen<ss...>, N> {
};

template <IsProgress P, Symbol t, Symbol... ss, IsNodes N>
struct Step<State<P, Sen<t, ss...>, N>> : Predict<t, P, Sen<ss...>, N> {};

template <IsResult R> struct Step<R> : Id<R> {};

template <RegexString regex>
struct InitialState
    : Id<State<Progress<regex.data, 0>, Sen<Symbol::Start>, Nodes<>>> {};
template <RegexString regex>
using initial_state_t = typename InitialState<regex>::type;

template <IsResultOrState, size_t> struct Iter;
template <IsResultOrState S, size_t N> using iter_t = typename Iter<S, N>::type;

template <IsResultOrState S> struct Iter<S, 0> : Step<S> {};
template <IsResultOrState S, size_t N>
struct Iter : Iter<iter_t<S, N - 1>, N - 1> {};

template <IsResultOrState S, size_t N = 0>
struct IterTilFixed : If<std::is_same_v<S, iter_t<S, N>>, Id<S>,
                         IterTilFixed<iter_t<S, N>, N + 1>> {};
} // namespace iterate

using iterate::initial_state_t;
using iterate::IterTilFixed;

namespace match {

template <IsRegex...> struct Matcher {
  static_assert(false, "Missing matcher case");
};

template <IsRegex> struct StarNormal {
  static_assert(false, "Missing StarNormal case");
};
template <IsRegex R> using star_normal_t = typename StarNormal<R>::type;

template <IsRegex R>
inline constexpr auto is_nullable_v = []() {
  static_assert(false, "Missing nullable check");
  return false;
}();

template <bool e, char c>
inline constexpr auto is_nullable_v<Literal<e, c>> = false;

template <IsRegex... Rs>
inline constexpr auto is_nullable_v<Sequence<Rs...>> =
    (is_nullable_v<Rs> && ...);

template <IsRegex... Rs>
inline constexpr auto is_nullable_v<Alternatives<Rs...>> =
    (is_nullable_v<Rs> || ...);

template <IsRegex R>
inline constexpr auto is_nullable_v<Group<R>> = is_nullable_v<R>;

template <IsRegex R>
inline constexpr auto is_nullable_v<Repetition<R, '*'>> = true;

template <IsRegex R>
inline constexpr auto is_nullable_v<Repetition<R, '?'>> = true;

template <IsRegex R>
inline constexpr auto is_nullable_v<Repetition<R, '+'>> = is_nullable_v<R>;

template <bool e, char c>
struct StarNormal<Literal<e, c>> : Id<Literal<e, c>> {};

// needs to be typename here because the fold for IsRegex creates an atomic
// constraint and breaks subsumption with the nonnullable case below, causing
// ambiguity.  Fixed in C++26 (P2963)
template <typename... Rs>
  requires(is_nullable_v<Rs> && ...)
struct StarNormal<Sequence<Rs...>> : Id<Alternatives<star_normal_t<Rs>...>> {};

// needs to be typename, see above
template <typename... Rs>
struct StarNormal<Sequence<Rs...>> : Id<Sequence<Rs...>> {};

template <IsRegex... Rs>
struct StarNormal<Alternatives<Rs...>>
    : Id<Alternatives<star_normal_t<Rs>...>> {};

template <IsRegex R> struct StarNormal<Group<R>> : StarNormal<R> {};

template <IsRegex R, char r>
struct StarNormal<Repetition<R, r>> : StarNormal<R> {};

template <> struct Matcher<> {
  static constexpr auto match(std::string_view text) { return text.empty(); }
};

template <bool e, char c, IsRegex... Rs> struct Matcher<Literal<e, c>, Rs...> {
  static constexpr auto match(std::string_view text) {
    if (text.empty() || text.front() != c)
      return false;
    return Matcher<Rs...>::match(text.substr(1));
  }
};

template <IsRegex... Ras, IsRegex... Rbs>
struct Matcher<Sequence<Ras...>, Rbs...> {
  static constexpr auto match(std::string_view text) {
    return Matcher<Ras..., Rbs...>::match(text);
  }
};

template <IsRegex... Ras, IsRegex... Rbs>
struct Matcher<Alternatives<Ras...>, Rbs...> {
  static constexpr auto match(std::string_view text) {
    return (Matcher<Ras, Rbs...>::match(text) || ...);
  }
};

template <IsRegex R, IsRegex... Rs> struct Matcher<Group<R>, Rs...> {
  static constexpr auto match(std::string_view text) {
    return Matcher<R, Rs...>::match(text);
  }
};

template <IsRegex R, IsRegex... Rs> struct Matcher<Repetition<R, '?'>, Rs...> {
  static constexpr auto match(std::string_view text) {
    return Matcher<R, Rs...>::match(text) || Matcher<Rs...>::match(text);
  }
};

template <IsRegex R, IsRegex... Rs> struct Matcher<Repetition<R, '*'>, Rs...> {
  // need to specify return type due to recursion
  static constexpr auto match(std::string_view text) -> bool {
    return Matcher<star_normal_t<R>, Repetition<R, '*'>, Rs...>::match(text) ||
           Matcher<Rs...>::match(text);
  }
};

template <IsRegex R, IsRegex... Rs> struct Matcher<Repetition<R, '+'>, Rs...> {
  static constexpr auto match(std::string_view text) {
    return Matcher<R, Repetition<R, '*'>, Rs...>::match(text);
  }
};

} // namespace match
using match::Matcher;

} // namespace detail

using detail::Matcher;
using detail::ParseFailure;
using detail::ParseSuccess;
using detail::RegexString;

template <RegexString regex>
struct CompileTimeRegex : detail::IterTilFixed<detail::initial_state_t<regex>> {
};

template <RegexString regex>
using compile_time_regex_t = typename CompileTimeRegex<regex>::type;

} // namespace compile_time
} // namespace toyctre
