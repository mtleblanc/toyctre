#pragma once

#include "lib.hpp"
#include <algorithm>

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

template <typename V, bool, V, V> struct if_value;
template <typename V, V t, V f> struct if_value<V, true, t, f> {
  static constexpr V value = t;
};
template <typename V, V t, V f> struct if_value<V, false, t, f> {
  static constexpr V value = f;
};

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
template <char c> static constexpr auto lex_char_v = LexChar<c>::value;
// }}}

// Tree {{{
namespace parse_tree {

template <typename t> static constexpr auto isParseNode = false;
template <typename t>
concept IsParseNode = isParseNode<t>;

template <IsParseNode n> struct Regex {
  static constexpr std::string toString() { return n::toString(); }
};
template <IsParseNode n> static constexpr auto isParseNode<Regex<n>> = true;

template <IsParseNode n> struct Group {
  static constexpr std::string toString() {
    return std::string{"("} + n::toString() + ')';
  }
};
template <IsParseNode n> static constexpr auto isParseNode<Group<n>> = true;

template <IsParseNode... ns> struct Sequence {
  static constexpr std::string toString() { return (ns::toString() + ...); }
};
template <IsParseNode... ns>
static constexpr auto isParseNode<Sequence<ns...>> = true;

template <IsParseNode... ns> struct Alternatives {
  static constexpr std::string toString() {
    std::string res = ((ns::toString() + "|") + ...);
    return res.substr(0, res.size() - 1);
  }
};
template <IsParseNode... ns>
static constexpr auto isParseNode<Alternatives<ns...>> = true;

template <IsParseNode n, char r> struct Repetition {
  static constexpr std::string toString() { return n::toString() + r; }
};
template <IsParseNode n, char r>
static constexpr auto isParseNode<Repetition<n, r>> = true;

template <bool e, char c> struct Literal {
  static constexpr std::string toString() {
    if constexpr (e) {
      return std::string{'\\', c};
    }
    return std::string{c};
  }
};
template <bool e, char c>
static constexpr auto isParseNode<Literal<e, c>> = true;

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
static constexpr auto current_char_v = CurrentChar<p>::value;

template <IsProgress> struct IsEot : std::false_type {};
template <RegexString r>
struct IsEot<Progress<r, r.size() - 1>> : std::true_type {};
template <IsProgress p> static constexpr auto is_eot_v = IsEot<p>::value;

template <Symbol t, char c> struct Matches {
  static constexpr auto value = lex_char_v<c> == t;
};
template <Symbol t, char c>
static constexpr auto matches_v = Matches<t, c>::value;

template <IsProgress p>
struct Lex : if_value<Symbol, is_eot_v<p>, Symbol::T_Eot,
                      lex_char_v<current_char_v<p>>> {};
template <IsProgress p> static constexpr auto lex_v = Lex<p>::value;

} // namespace progress

using progress::advance_t;
using progress::current_char_v;
using progress::is_eot_v;
using progress::IsProgress;
using progress::lex_v;
using progress::matches_v;
using progress::Progress;
// }}}

// State {{{
namespace state {

template <Symbol...> struct Sen {};
template <typename s> inline constexpr auto isSentinels = false;
template <Symbol... ss> inline constexpr auto isSentinels<Sen<ss...>> = true;
template <typename s>
concept IsSentinels = isSentinels<s>;

template <typename...> struct Nodes {};
template <typename n> inline constexpr auto isNodes = false;
template <typename... nn> inline constexpr auto isNodes<Nodes<nn...>> = true;
template <typename n>
concept IsNodes = isNodes<n>;

template <IsProgress, IsSentinels, IsNodes> struct State;
template <typename> inline constexpr auto is_state = false;
template <IsProgress p, IsSentinels s, IsNodes n>
inline constexpr auto is_state<State<p, s, n>> = true;
template <typename s>
concept IsState = is_state<s>;

struct ParseFailure {};
template <IsParseNode n> struct ParseSuccess {
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
template <IsProgress p, IsSentinels s, IsNodes n>
struct ParseNext : Ty<State<advance_t<p>, s, n>> {};

template <Symbol t, IsProgress p, IsSentinels s, IsNodes n>
struct ConsumeTerminal : if_<!is_eot_v<p> && matches_v<t, current_char_v<p>>,
                             ParseNext<p, s, n>, Ty<ParseFailure>> {};

template <IsProgress p, IsSentinels s, IsNodes n>
struct ConsumeTerminal<Symbol::T_Eot, p, s, n>
    : if_<is_eot_v<p>, Ty<State<p, s, n>>, Ty<ParseFailure>> {};
} // namespace consume

using consume::ConsumeTerminal;
// }}}

// Action {{{
namespace action {
using enum Symbol;

template <Symbol, IsProgress, IsSentinels, IsNodes> struct DoAction {
  static_assert(false, "Unrecognized action");
};

template <IsProgress p, IsSentinels s, IsNodes n>
struct DoAction<A_EAlt, p, s, n> : Ty<State<p, s, push_t<Alternatives<>, n>>> {
};

template <IsProgress p, IsSentinels s, IsNodes n>
struct DoAction<A_AReg, p, s, n> {
  using next = push_t<Regex<head_t<n>>, tail_t<n>>;
  using type = State<p, s, next>;
};

template <IsProgress p, IsSentinels s, IsNodes n>
struct DoAction<A_AGrp, p, s, n> {
  using group = Group<head_t<n>>;
  using next = push_t<group, tail_t<n>>;
  using type = State<p, s, next>;
};

template <IsProgress p, IsSentinels s, IsNodes n>
struct DoAction<A_ECat, p, s, n> : Ty<State<p, s, push_t<Sequence<>, n>>> {};

template <IsProgress p, IsSentinels s, IsNodes n>
struct DoAction<A_ACat, p, s, n> {
  using cat = head_t<n>;
  using alt = head_t<tail_t<n>>;
  using rest = tail_t<tail_t<n>>;
  using new_alt = push_back_t<cat, alt>;
  using next = push_t<new_alt, rest>;
  using type = State<p, s, next>;
};

template <IsProgress p, IsSentinels s, IsNodes n>
struct DoAction<A_ERep, p, s, n> : Ty<State<p, s, n>> {};

template <IsProgress p, IsSentinels s, IsNodes n>
struct DoAction<A_ARep, p, s, n> {
  using cat = head_t<n>;
  using alt = head_t<tail_t<n>>;
  using rest = tail_t<tail_t<n>>;
  using new_alt = push_back_t<cat, alt>;
  using next = push_t<new_alt, rest>;
  using type = State<p, s, next>;
};

template <IsProgress p, IsSentinels s, IsNodes n>
struct DoAction<A_REsc, p, s, n> {
  using lit = Literal<true, current_char_v<p>>;
  using next = push_t<lit, n>;
  using type = State<p, s, next>;
};

template <IsProgress p, IsSentinels s, IsNodes n>
struct DoAction<A_RLit, p, s, n> {
  using lit = Literal<false, current_char_v<p>>;
  using next = push_t<lit, n>;
  using type = State<p, s, next>;
};

template <IsProgress p, IsSentinels s, IsNodes n>
struct DoAction<A_RRep, p, s, n> {
  using rep = Repetition<head_t<n>, current_char_v<p>>;
  using next = push_t<rep, tail_t<n>>;
  using type = State<p, s, next>;
};

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

template <> struct Prod<1> : Ty<Sen<A_EAlt, Alt, T_Eot, A_AReg>> {}; // S
template <> struct Prod<2> : Ty<Sen<A_ECat, Cat, A_ACat, Cats>> {};  // Alt
template <>
struct Prod<3> : Ty<Sen<T_Bar, A_ECat, Cat, A_ACat, Cats>> {};           // Cats
template <> struct Prod<4> : Ty<Sen<>> {};                               // Cats
template <> struct Prod<5> : Ty<Sen<A_ERep, Unt, Amt, A_ARep, Reps>> {}; // Cat
template <> struct Prod<6> : Ty<Sen<A_ERep, Unt, Amt, A_ARep, Reps>> {}; // Reps
template <> struct Prod<7> : Ty<Sen<>> {};                               // Reps
template <> struct Prod<8> : Ty<Sen<A_RLit, T_Reg>> {};                  // Unt
template <> struct Prod<9> : Ty<Sen<T_Slash, A_REsc, Lit>> {};           // Unt
template <> struct Prod<10> : Ty<Sen<T_Reg>> {};                         // Lit
template <> struct Prod<11> : Ty<Sen<T_Rep>> {};                         // Lit
template <> struct Prod<12> : Ty<Sen<T_LPar>> {};                        // Lit
template <> struct Prod<13> : Ty<Sen<T_RPar>> {};                        // Lit
template <> struct Prod<14> : Ty<Sen<T_Slash>> {};                       // Lit
template <> struct Prod<15> : Ty<Sen<T_Bar>> {};                         // Lit
template <>
struct Prod<16> : Ty<Sen<T_LPar, A_EAlt, Alt, A_AGrp, T_RPar>> {}; // Unt
template <> struct Prod<17> : Ty<Sen<A_RRep, T_Rep>> {};           // Amt
template <> struct Prod<18> : Ty<Sen<>> {};                        // Amt
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

} // namespace produce
template <Symbol nt, Symbol p>
using produce_t = typename produce::Produce<nt, p>::type;

template <Symbol nt, IsProgress p, IsSentinels s, IsNodes n>
struct Predict : Ty<State<p, concat_t<produce_t<nt, lex_v<p>>, s>, n>> {};
// }}}

// Iterate {{{

constexpr bool isAction(Symbol s) { return s > Symbol::ACTIONS; }
constexpr bool isTerminal(Symbol s) {
  return s > Symbol::TERMINALS && s < Symbol::ACTIONS;
}

template <IsResultOrState s> struct Test {
  static constexpr auto value = true;
};

template <IsResultOrState> struct Step;
template <typename node, IsProgress p>
struct Step<State<p, Sen<>, Nodes<node>>>
    : if_<is_eot_v<p>, Ty<ParseSuccess<node>>, Ty<ParseFailure>> {};

template <IsProgress r, Symbol t, Symbol... ss, IsNodes n>
struct Step<State<r, Sen<t, ss...>, n>>
    : if_<isAction(t), DoAction<t, r, Sen<ss...>, n>,
          if_<isTerminal(t), ConsumeTerminal<t, r, Sen<ss...>, n>,
              Predict<t, r, Sen<ss...>, n>>> {};

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

// }}}

} // namespace detail

using detail::ParseFailure;
using detail::ParseSuccess;
using detail::RegexString;

template <RegexString regex>
struct CompileTimeRegex : detail::IterTilFixed<detail::initial_state_t<regex>> {
};

template <RegexString regex>
using compile_time_regex_t = typename CompileTimeRegex<regex>::type;

using namespace detail::parse_tree;
static_assert(compile_time_regex_t<"a">::result::toString() == "a");

} // namespace compile_time
} // namespace toyctre
