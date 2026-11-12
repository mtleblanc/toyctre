#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>

namespace toyctre {
namespace compile_time {

// Helper {{{
namespace helpers {

template <bool, typename, typename> struct if_;
template <typename T, typename F> struct if_<true, T, F> {
  using type = typename T::type;
};
template <typename T, typename F> struct if_<false, T, F> {
  using type = typename F::type;
};

template <bool b, typename t, typename f>
using if_t = typename if_<b, t, f>::type;

template <typename T> struct Ty {
  using type = T;
};

template <typename V, V v> struct Va {
  static constexpr V value = v;
};

template <typename, typename> struct Concat;
template <typename T, template <T...> class LIST, T... l1, T... l2>
struct Concat<LIST<l1...>, LIST<l2...>> : Ty<LIST<l1..., l2...>> {};
template <template <typename...> class LIST, typename... l1, typename... l2>
struct Concat<LIST<l1...>, LIST<l2...>> : Ty<LIST<l1..., l2...>> {};

template <typename l1, typename l2>
using concat_t = typename Concat<l1, l2>::type;

template <typename, typename> struct Push;
template <typename t, template <typename...> class LIST, typename... l1>
struct Push<t, LIST<l1...>> : Ty<LIST<t, l1...>> {};
template <typename t, typename l> using push_t = typename Push<t, l>::type;

template <typename, typename> struct ReplaceHead;
template <typename t, template <typename...> class LIST, typename h,
          typename... l1>
struct ReplaceHead<t, LIST<h, l1...>> : Ty<LIST<t, l1...>> {};
template <typename t, typename l>
using replace_head_t = typename ReplaceHead<t, l>::type;

template <typename, typename> struct PushBack;
template <typename t, template <typename...> class LIST, typename... l1>
struct PushBack<t, LIST<l1...>> : Ty<LIST<l1..., t>> {};
template <typename t, typename l>
using push_back_t = typename PushBack<t, l>::type;

template <typename> struct Head;
template <template <typename...> class LIST, typename h, typename... t>
struct Head<LIST<h, t...>> : Ty<h> {};
template <typename l> using head_t = typename Head<l>::type;

template <typename> struct Tail;
template <template <typename...> class LIST, typename h, typename... t>
struct Tail<LIST<h, t...>> : Ty<LIST<t...>> {};
template <typename l> using tail_t = typename Tail<l>::type;

} // namespace helpers
// }}}

namespace detail {
using namespace helpers;

// String {{{
template <size_t N> struct RegexString {
  constexpr RegexString(const char (&str)[N]) { std::copy_n(str, N, data); }
  constexpr std::string_view view() { return std::string_view{data, N}; }
  char data[N];
  constexpr auto size() const { return N; }
};

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
  A_Mark,
  A_Alts,
  A_Cats,
  A_Group,
  A_Lit,
  A_Esc,
  A_Rep,

  A_Reject,

  SIZE
};

template <char c> struct LexChar {
  static constexpr auto value = []() {
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
      return T_Slash;
    default:
      return T_Reg;
    }
  }();
};
template <char c> inline constexpr auto lex_char_v = LexChar<c>::value;
// }}}

// Tree {{{
namespace parse_tree {

template <typename t> inline constexpr auto isRegex = false;
template <typename t>
concept IsRegex = isRegex<t>;

template <IsRegex n> struct Group {
  static constexpr std::string toString() {
    return std::string{"("} + n::toString() + ')';
  }
};
template <IsRegex n> inline constexpr auto isRegex<Group<n>> = true;

template <IsRegex... ns> struct Sequence {
  static constexpr std::string toString() { return (ns::toString() + ...); }
};
template <IsRegex... ns> inline constexpr auto isRegex<Sequence<ns...>> = true;

template <IsRegex... ns> struct Alternatives {
  static constexpr std::string toString() {
    std::string res = ((ns::toString() + "|") + ...);
    return res.substr(0, res.size() - 1);
  }
};
template <IsRegex... ns>
inline constexpr auto isRegex<Alternatives<ns...>> = true;

template <IsRegex n, char r> struct Repetition {
  static constexpr std::string toString() { return n::toString() + r; }
};
template <IsRegex n, char r>
inline constexpr auto isRegex<Repetition<n, r>> = true;

template <bool e, char c> struct Literal {
  static constexpr std::string toString() {
    if constexpr (e) {
      return std::string{'\\', c};
    }
    return std::string{c};
  }
};
template <bool e, char c> inline constexpr auto isRegex<Literal<e, c>> = true;

} // namespace parse_tree
using namespace parse_tree;
// }}}

// Progress {{{
namespace progress {

template <RegexString r, size_t N> struct Progress {
  static_assert(N < r.size(), "Progress advanced past end of string");
};

template <typename p> inline constexpr auto isProgress = false;
template <RegexString r, size_t n>
inline constexpr auto isProgress<Progress<r, n>> = true;
template <typename p>
concept IsProgress = isProgress<p>;

template <IsProgress> struct Advance;
template <IsProgress p> using advance_t = typename Advance<p>::type;

template <size_t N, RegexString<N> r, size_t M> struct Advance<Progress<r, M>> {
  static_assert(M + 1 < N, "Advanced past end of input");
  using type = Progress<r, M + 1>;
};

template <IsProgress> struct CurrentChar;
template <RegexString r, size_t M> struct CurrentChar<Progress<r, M>> {
  static constexpr auto value = r.data[M];
};

template <IsProgress p>
inline constexpr auto current_char_v = CurrentChar<p>::value;

template <IsProgress> struct IsEot : std::false_type {};
template <RegexString r>
struct IsEot<Progress<r, r.size() - 1>> : std::true_type {};
template <IsProgress p> inline constexpr auto is_eot_v = IsEot<p>::value;
template <typename t>
concept IsAtEot = IsProgress<t> && is_eot_v<t>;

template <IsProgress p> struct Lex {
  static constexpr auto value =
      is_eot_v<p> ? Symbol::T_Eot : lex_char_v<current_char_v<p>>;
};
template <IsProgress p> inline constexpr auto lex_v = Lex<p>::value;

} // namespace progress
using progress::advance_t;
using progress::current_char_v;
using progress::is_eot_v;
using progress::IsAtEot;
using progress::IsProgress;
using progress::lex_v;
using progress::Progress;
// }}}

// State {{{
namespace state {

template <Symbol...> struct Sen {};
template <typename s> inline constexpr auto isSentinels = false;
template <Symbol... ss> inline constexpr auto isSentinels<Sen<ss...>> = true;
template <typename s>
concept IsSentinels = isSentinels<s>;

struct Mark {};
template <typename t>
concept IsNodeListItem = IsRegex<t> || std::same_as<t, Mark>;

template <IsNodeListItem...> struct Nodes {};
template <typename n> inline constexpr auto isNodes = false;
template <IsNodeListItem... nn>
inline constexpr auto isNodes<Nodes<nn...>> = true;
template <typename n>
concept IsNodes = isNodes<n>;

template <IsProgress, IsSentinels, IsNodes> struct State;
template <typename> inline constexpr auto is_state = false;
template <IsProgress p, IsSentinels s, IsNodes n>
inline constexpr auto is_state<State<p, s, n>> = true;
template <typename s>
concept IsState = is_state<s>;

struct ParseFailure {};
template <IsRegex n> struct ParseSuccess {
  using result = n;
};

template <typename> inline constexpr auto is_result = false;
template <typename r> inline constexpr auto is_result<ParseSuccess<r>> = true;
template <> inline constexpr auto is_result<ParseFailure> = true;
template <typename t>
concept IsResult = is_result<t>;

template <typename t>
concept IsResultOrState = IsResult<t> || IsState<t>;

} // namespace state
using namespace state;
// }}}

// ConsumeTerminal {{{
namespace consume {

template <Symbol t, IsProgress p, IsSentinels s, IsNodes n>
struct ConsumeTerminal : Ty<ParseFailure> {};

// Can't have this firing on EOT as advance would be an error.  The check is
// unnecessary since the IsAtEot specialization is more specific, but it is
// included for clarity
template <Symbol t, IsProgress p, IsSentinels s, IsNodes n>
  requires(t != Symbol::T_Eot && t == lex_v<p>)
struct ConsumeTerminal<t, p, s, n> : Ty<State<advance_t<p>, s, n>> {};

template <IsAtEot p, IsSentinels s, IsNodes n>
struct ConsumeTerminal<Symbol::T_Eot, p, s, n> : Ty<State<p, s, n>> {};

} // namespace consume
using consume::ConsumeTerminal;
// }}}

// Action {{{
namespace action {
using enum Symbol;

template <Symbol, char, IsNodes> struct NextNodes {
  static_assert(false, "Unrecognized action");
};
template <Symbol a, char c, IsNodes n>
using next_nodes_t = typename NextNodes<a, c, n>::type;

template <IsRegex acc, IsNodeListItem... ns> struct MergeInto;
template <IsRegex acc, IsRegex head, IsNodeListItem... rest>
struct MergeInto<acc, head, rest...> : MergeInto<push_t<head, acc>, rest...> {};
template <IsRegex acc, IsNodeListItem... rest>
struct MergeInto<acc, Mark, rest...> : Push<acc, Nodes<rest...>> {};

template <template <typename...> class L, IsNodes n> struct Merge;
template <template <typename...> class L, IsRegex first, IsRegex second,
          IsNodeListItem... rest>
struct Merge<L, Nodes<first, second, rest...>>
    : MergeInto<L<second, first>, rest...> {};
template <template <typename...> class L, IsRegex first, IsNodeListItem... rest>
struct Merge<L, Nodes<first, Mark, rest...>> : Push<first, Nodes<rest...>> {};

template <char c, IsNodes n> struct NextNodes<A_Mark, c, n> : Push<Mark, n> {};

template <char c, IsNodes n>
struct NextNodes<A_Alts, c, n> : Merge<Alternatives, n> {};
template <char c, IsNodes n>
struct NextNodes<A_Cats, c, n> : Merge<Sequence, n> {};
template <char c, IsNodes n>
struct NextNodes<A_Group, c, n> : ReplaceHead<Group<head_t<n>>, n> {};

template <char c, IsNodes n>
struct NextNodes<A_Lit, c, n> : Push<Literal<false, c>, n> {};
template <char c, IsNodes n>
struct NextNodes<A_Esc, c, n> : Push<Literal<true, c>, n> {};
template <char c, IsNodes n>
struct NextNodes<A_Rep, c, n> : ReplaceHead<Repetition<head_t<n>, c>, n> {};

template <Symbol a, IsProgress p, IsSentinels s, IsNodes n>
struct DoAction : Ty<State<p, s, next_nodes_t<a, current_char_v<p>, n>>> {};
template <IsProgress p, IsSentinels s, IsNodes n>
struct DoAction<A_Reject, p, s, n> : Ty<ParseFailure> {};

} // namespace action
using action::DoAction;
// }}}

// Predict {{{

namespace produce {
using enum Symbol;

template <Symbol nt, Symbol p> struct Produce {
  using type = Sen<A_Reject>;
};

template <size_t> struct Prod;

template <> struct Prod<1> : Ty<Sen<Alt, T_Eot>> {};                     // S
template <> struct Prod<2> : Ty<Sen<A_Mark, Cat, Cats, A_Alts>> {};      // Alt
template <> struct Prod<3> : Ty<Sen<T_Bar, Cat, Cats>> {};               // Cats
template <> struct Prod<4> : Ty<Sen<>> {};                               // Cats
template <> struct Prod<5> : Ty<Sen<A_Mark, Unt, Amt, Reps, A_Cats>> {}; // Cat
template <> struct Prod<6> : Ty<Sen<Unt, Amt, Reps>> {};                 // Reps
template <> struct Prod<7> : Ty<Sen<>> {};                               // Reps
template <> struct Prod<8> : Ty<Sen<A_Lit, T_Reg>> {};                   // Unt
template <> struct Prod<9> : Ty<Sen<T_Slash, A_Esc, Lit>> {};            // Unt
template <> struct Prod<10> : Ty<Sen<T_Reg>> {};                         // Lit
template <> struct Prod<11> : Ty<Sen<T_Rep>> {};                         // Lit
template <> struct Prod<12> : Ty<Sen<T_LPar>> {};                        // Lit
template <> struct Prod<13> : Ty<Sen<T_RPar>> {};                        // Lit
template <> struct Prod<14> : Ty<Sen<T_Slash>> {};                       // Lit
template <> struct Prod<15> : Ty<Sen<T_Bar>> {};                         // Lit
template <> struct Prod<16> : Ty<Sen<T_LPar, Alt, A_Group, T_RPar>> {};  // Unt
template <> struct Prod<17> : Ty<Sen<A_Rep, T_Rep>> {};                  // Amt
template <> struct Prod<18> : Ty<Sen<>> {};                              // Amt
template <> struct Produce<S, T_LPar> : Prod<1> {};
template <> struct Produce<S, T_Slash> : Prod<1> {};
template <> struct Produce<S, T_Reg> : Prod<1> {};
template <> struct Produce<Alt, T_LPar> : Prod<2> {};
template <> struct Produce<Alt, T_Slash> : Prod<2> {};
template <> struct Produce<Alt, T_Reg> : Prod<2> {};
template <> struct Produce<Cat, T_LPar> : Prod<5> {};
template <> struct Produce<Cat, T_Slash> : Prod<5> {};
template <> struct Produce<Cat, T_Reg> : Prod<5> {};
template <> struct Produce<Cats, T_RPar> : Prod<4> {};
template <> struct Produce<Cats, T_Bar> : Prod<3> {};
template <> struct Produce<Cats, T_Eot> : Prod<4> {};
template <> struct Produce<Reps, T_LPar> : Prod<6> {};
template <> struct Produce<Reps, T_RPar> : Prod<7> {};
template <> struct Produce<Reps, T_Slash> : Prod<6> {};
template <> struct Produce<Reps, T_Bar> : Prod<7> {};
template <> struct Produce<Reps, T_Reg> : Prod<6> {};
template <> struct Produce<Reps, T_Eot> : Prod<7> {};
template <> struct Produce<Unt, T_LPar> : Prod<16> {};
template <> struct Produce<Unt, T_Slash> : Prod<9> {};
template <> struct Produce<Unt, T_Reg> : Prod<8> {};
template <> struct Produce<Amt, T_Rep> : Prod<17> {};
template <> struct Produce<Amt, T_LPar> : Prod<18> {};
template <> struct Produce<Amt, T_RPar> : Prod<18> {};
template <> struct Produce<Amt, T_Slash> : Prod<18> {};
template <> struct Produce<Amt, T_Bar> : Prod<18> {};
template <> struct Produce<Amt, T_Reg> : Prod<18> {};
template <> struct Produce<Amt, T_Eot> : Prod<18> {};
template <> struct Produce<Lit, T_Rep> : Prod<11> {};
template <> struct Produce<Lit, T_LPar> : Prod<12> {};
template <> struct Produce<Lit, T_RPar> : Prod<13> {};
template <> struct Produce<Lit, T_Slash> : Prod<14> {};
template <> struct Produce<Lit, T_Bar> : Prod<15> {};
template <> struct Produce<Lit, T_Reg> : Prod<10> {};

template <Symbol nt, Symbol p>
using produce_t = typename produce::Produce<nt, p>::type;

template <Symbol nt, IsProgress p, IsSentinels s, IsNodes n>
struct Predict : Ty<State<p, concat_t<produce_t<nt, lex_v<p>>, s>, n>> {};

} // namespace produce
using produce::Predict;
using produce::produce_t;
// }}}

// Iterate {{{

namespace iterate {
constexpr bool isAction(Symbol s) { return s > Symbol::ACTIONS; }
constexpr bool isTerminal(Symbol s) {
  return s > Symbol::TERMINALS && s < Symbol::ACTIONS;
}

template <IsResultOrState> struct Step {
  static_assert(false, "Invalid state");
};

template <IsRegex node, IsAtEot p>
struct Step<State<p, Sen<>, Nodes<node>>> : Ty<ParseSuccess<node>> {};

template <IsRegex node, IsProgress p>
struct Step<State<p, Sen<>, Nodes<node>>> : Ty<ParseFailure> {};

template <IsProgress r, Symbol t, Symbol... ss, IsNodes n>
  requires(isAction(t))
struct Step<State<r, Sen<t, ss...>, n>> : DoAction<t, r, Sen<ss...>, n> {};

template <IsProgress r, Symbol t, Symbol... ss, IsNodes n>
  requires(isTerminal(t))
struct Step<State<r, Sen<t, ss...>, n>> : ConsumeTerminal<t, r, Sen<ss...>, n> {
};

template <IsProgress r, Symbol t, Symbol... ss, IsNodes n>
struct Step<State<r, Sen<t, ss...>, n>> : Predict<t, r, Sen<ss...>, n> {};

template <IsResult t> struct Step<t> : Ty<t> {};
template <IsResultOrState t> using step_t = typename Step<t>::type;

template <RegexString regex>
struct InitialState
    : Ty<State<Progress<regex.data, 0>, Sen<Symbol::S>, Nodes<>>> {};
template <RegexString regex>
using initial_state_t = typename InitialState<regex>::type;

template <IsResultOrState s, size_t N> struct Iter;
template <IsResultOrState s, size_t N> using iter_t = Iter<s, N>::type;

template <IsResultOrState s> struct Iter<s, 0> : Step<s> {};
template <IsResultOrState s, size_t N>
struct Iter : Iter<iter_t<s, N - 1>, N - 1> {};

template <IsResultOrState s, size_t N = 0>
struct IterTilFixed : if_<std::is_same_v<s, iter_t<s, N>>, Ty<s>,
                          IterTilFixed<iter_t<s, N>, N + 1>> {};
} // namespace iterate

using iterate::initial_state_t;
using iterate::IterTilFixed;
// }}}

struct ConsumeMark {};
template <> inline constexpr auto parse_tree::isRegex<ConsumeMark> = true;

namespace match {

template <IsRegex...> struct Matcher;

template <> struct Matcher<> {
  static constexpr auto match(std::string_view text,
                              bool expectConsume = false) {
    return text.empty() && !expectConsume;
  }
};

template <bool e, char c, IsRegex... rest>
struct Matcher<Literal<e, c>, rest...> {
  static constexpr auto match(std::string_view text, bool = false) {
    if (text.empty() || text.front() != c)
      return false;
    return Matcher<rest...>::match(text.substr(1), false);
  }
};

template <IsRegex... ns, IsRegex... rest>
struct Matcher<Sequence<ns...>, rest...> {
  static constexpr auto match(std::string_view text,
                              bool expectConsume = false) {
    return Matcher<ns..., rest...>::match(text, expectConsume);
  }
};

template <IsRegex... ns, IsRegex... rest>
struct Matcher<Alternatives<ns...>, rest...> {
  static constexpr auto match(std::string_view text,
                              bool expectConsume = false) {
    return (Matcher<ns, rest...>::match(text, expectConsume) || ...);
  }
};

template <IsRegex n, IsRegex... rest> struct Matcher<Group<n>, rest...> {
  static constexpr auto match(std::string_view text,
                              bool expectConsume = false) {
    return Matcher<n, rest...>::match(text, expectConsume);
  }
};

template <IsRegex n, IsRegex... rest>
struct Matcher<Repetition<n, '?'>, rest...> {
  static constexpr auto match(std::string_view text,
                              bool expectConsume = false) {
    return Matcher<n, rest...>::match(text, expectConsume) ||
           Matcher<rest...>::match(text, expectConsume);
  }
};

template <IsRegex n, IsRegex... rest>
struct Matcher<Repetition<n, '*'>, rest...> {
  static constexpr auto match(std::string_view text,
                              bool expectConsume = false) -> bool {
    return Matcher<n, ConsumeMark, Repetition<n, '*'>, rest...>::match(text,
                                                                       true) ||
           Matcher<rest...>::match(text, expectConsume);
  }
};

template <IsRegex... rest> struct Matcher<ConsumeMark, rest...> {
  static constexpr auto match(std::string_view text,
                              bool expectConsume = false) -> bool {
    return !expectConsume && Matcher<rest...>::match(text, expectConsume);
  }
};

template <IsRegex n, IsRegex... rest>
struct Matcher<Repetition<n, '+'>, rest...> {
  static constexpr auto match(std::string_view text,
                              bool expectConsume = false) {
    return Matcher<n, Repetition<n, '*'>, rest...>::match(text, expectConsume);
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
