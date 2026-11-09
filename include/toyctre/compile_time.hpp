#pragma once

#include "lib.hpp"
#include <algorithm>

namespace toyctre {
namespace compile_time {

struct ParseFailure {};

template <size_t N> struct RegexString {
  constexpr RegexString(const char (&str)[N]) { std::copy_n(str, N, data); }
  constexpr std::string_view view() { return std::string_view{data, N}; }
  char data[N];
  constexpr auto size() const { return N; }
};

namespace helpers {

template <bool, typename, typename> struct if_;
template <typename T, typename F> struct if_<true, T, F> {
  using type = typename T::type;
};
template <typename T, typename F> struct if_<false, T, F> {
  using type = typename F::type;
};

template <typename T> struct HasType {
  using type = T;
};

template <typename, typename> struct Concat;
template <typename T, template <T...> class LIST, T... l1, T... l2>
struct Concat<LIST<l1...>, LIST<l2...>> : HasType<LIST<l1..., l2...>> {};
template <template <typename...> class LIST, typename... l1, typename... l2>
struct Concat<LIST<l1...>, LIST<l2...>> : HasType<LIST<l1..., l2...>> {};

template <typename l1, typename l2>
using concat_t = typename Concat<l1, l2>::type;

} // namespace helpers

namespace detail {
using namespace helpers;

/////////////// Progress /////////////////////////////////////
namespace progress {
template <RegexString r, size_t N> struct Progress {
  static_assert(N < r.size(), "Progress advanced past end of string");
};
template <typename> struct Advance;
template <typename> struct CurrentChar;
template <Symbol, char> struct Matches;
template <typename> struct IsEot;

template <size_t N, RegexString<N> r, size_t M> struct Advance<Progress<r, M>> {
  static_assert(M + 1 < N, "Advanced past end of input");
  using type = Progress<r, M + 1>;
};

template <RegexString r, size_t M> struct CurrentChar<Progress<r, M>> {
  static constexpr auto value = r.data[M];
};

template <typename> struct IsEot : std::false_type {};
template <RegexString r>
struct IsEot<Progress<r, r.size() - 1>> : std::true_type {};

template <Symbol t, char c> struct Matches : std::false_type {};

template <char c> struct Matches<Symbol::T_Reg, c> : std::true_type {};
template <> struct Matches<Symbol::T_Reg, '('> : std::false_type {};
template <> struct Matches<Symbol::T_Reg, ')'> : std::false_type {};
template <> struct Matches<Symbol::T_Reg, '*'> : std::false_type {};
template <> struct Matches<Symbol::T_Reg, '?'> : std::false_type {};
template <> struct Matches<Symbol::T_Reg, '+'> : std::false_type {};
template <> struct Matches<Symbol::T_Reg, '|'> : std::false_type {};
template <> struct Matches<Symbol::T_Reg, '\\'> : std::false_type {};

template <> struct Matches<Symbol::T_Rep, '*'> : std::true_type {};
template <> struct Matches<Symbol::T_Rep, '?'> : std::true_type {};
template <> struct Matches<Symbol::T_Rep, '+'> : std::true_type {};

template <> struct Matches<Symbol::T_LPar, '('> : std::true_type {};
template <> struct Matches<Symbol::T_RPar, ')'> : std::true_type {};
template <> struct Matches<Symbol::T_Slash, '\\'> : std::true_type {};
template <> struct Matches<Symbol::T_Bar, '|'> : std::true_type {};
} // namespace progress

using progress::Progress;
template <typename p>
static constexpr auto current_char_v = progress::CurrentChar<p>::value;
template <Symbol t, char c>
static constexpr auto matches_v = progress::Matches<t, c>::value;
template <typename p>
static constexpr auto is_eot_v = progress::IsEot<p>::value;
template <typename p> using advance_t = typename progress::Advance<p>::type;
//////////////////////////////////////////////////////////////////

////////////////////// Lex ////////////////////////////////////////////////////
template <typename> struct Lex;
template <typename c> static constexpr auto lex_v = Lex<c>::value;

template <char c> struct LexChar {
  static constexpr auto value = Symbol::T_Reg;
};

template <> struct LexChar<'('> {
  static constexpr auto value = Symbol::T_LPar;
};
template <> struct LexChar<')'> {
  static constexpr auto value = Symbol::T_RPar;
};
template <> struct LexChar<'*'> {
  static constexpr auto value = Symbol::T_Rep;
};
template <> struct LexChar<'?'> {
  static constexpr auto value = Symbol::T_Rep;
};
template <> struct LexChar<'+'> {
  static constexpr auto value = Symbol::T_Rep;
};
template <> struct LexChar<'|'> {
  static constexpr auto value = Symbol::T_Bar;
};
template <> struct LexChar<'\\'> {
  static constexpr auto value = Symbol::T_Slash;
};

struct EotChar {
  static constexpr auto value = Symbol::T_Eot;
};

template <typename p>
struct Lex : if_<is_eot_v<p>, HasType<EotChar>,
                 HasType<LexChar<current_char_v<p>>>>::type {};

////////////////// End Lex ////////////////////////////////////////////////////

template <Symbol...> struct Sentinels {};
template <typename...> struct Nodes {};
template <typename, typename, typename> struct ParseRegex;
template <Symbol, typename, typename, typename> struct DoAction;
template <Symbol, typename, typename, typename> struct ConsumeTerminal;
template <Symbol, typename, typename, typename> struct Predict;

template <typename node, typename p>
struct ParseRegex<p, Sentinels<>, Nodes<node>>
    : if_<is_eot_v<p>, HasType<node>, HasType<ParseFailure>> {};
template <typename, typename, typename> struct ParseRegex {};

constexpr bool isAction(Symbol s) { return s > Symbol::ACTIONS; }
constexpr bool isTerminal(Symbol s) {
  return s > Symbol::TERMINALS && s < Symbol::ACTIONS;
}

template <typename r, Symbol t, Symbol... ss, typename n>
struct ParseRegex<r, Sentinels<t, ss...>, n>
    : if_<isAction(t), DoAction<t, r, Sentinels<ss...>, n>,
          if_<isTerminal(t), ConsumeTerminal<t, r, Sentinels<ss...>, n>,
              Predict<t, r, Sentinels<ss...>, n>>> {};

////////////////////// ConsumeTerminal ////////////////////////////////////////
template <typename p, typename s, typename n>
struct ParseNext : ParseRegex<advance_t<p>, s, n> {};

template <Symbol t, typename p, typename s, typename n>
struct ConsumeTerminal : if_<!is_eot_v<p> && matches_v<t, current_char_v<p>>,
                             ParseNext<p, s, n>, HasType<ParseFailure>> {};

template <typename p, typename s, typename n>
struct ConsumeTerminal<Symbol::T_Eot, p, s, n> : ParseRegex<p, s, n> {
  static_assert(is_eot_v<p>, "Expected end of text");
};
////////////////// End ConsumeTerminal ////////////////////////////////////////

////////////////////// DoAction ///////////////////////////////////////////////
template <Symbol, typename p, typename s, typename>
struct DoAction : ParseRegex<p, s, Nodes<int>> {};

template <typename p, typename s, typename n>
struct DoAction<Symbol::A_Reject, p, s, n> : HasType<ParseFailure> {};
////////////////// End DoAction ///////////////////////////////////////////////

////////////////////// Predict ////////////////////////////////////////////////

namespace produce {
using enum Symbol;
template <Symbol nt, Symbol p> struct Produce {
  using type = Sentinels<A_Reject>;
};

template <size_t> struct Production;

template <>
struct Production<1> : HasType<Sentinels<A_EmptyAlt, Alt, A_MakeRegex>> {}; // S
template <>
struct Production<2> : HasType<Sentinels<A_EmptyCat, Cat, A_AppendCat, Cats>> {
}; // Alt
template <>
struct Production<3>
    : HasType<Sentinels<T_Bar, A_EmptyCat, Cat, A_AppendCat, Cats>> {}; // Cats
template <> struct Production<4> : HasType<Sentinels<>> {};             // Cats
template <>
struct Production<5>
    : HasType<Sentinels<A_EmptyRep, Unt, Amt, A_AppendRep, Reps>> {}; // Cat
template <>
struct Production<6>
    : HasType<Sentinels<A_EmptyRep, Unt, Amt, A_AppendRep, Reps>> {}; // Reps
template <> struct Production<7> : HasType<Sentinels<>> {};           // Reps
template <>
struct Production<8> : HasType<Sentinels<A_RecordLit, T_Reg>> {}; // Unt
template <>
struct Production<9> : HasType<Sentinels<T_Slash, A_RecordEscaped, Lit>> {
}; // Unt
template <> struct Production<10> : HasType<Sentinels<T_Reg>> {};   // Lit
template <> struct Production<11> : HasType<Sentinels<T_Rep>> {};   // Lit
template <> struct Production<12> : HasType<Sentinels<T_LPar>> {};  // Lit
template <> struct Production<13> : HasType<Sentinels<T_RPar>> {};  // Lit
template <> struct Production<14> : HasType<Sentinels<T_Slash>> {}; // Lit
template <> struct Production<15> : HasType<Sentinels<T_Bar>> {};   // Lit
template <>
struct Production<16>
    : HasType<Sentinels<T_LPar, A_EmptyAlt, Alt, A_MakeGroup, T_RPar>> {
}; // Unt
template <>
struct Production<17> : HasType<Sentinels<A_RecordRep, T_Rep>> {}; // Amt
template <> struct Production<18> : HasType<Sentinels<>> {};       // Amt

template <> struct Produce<S, T_LPar> : Production<1> {};
template <> struct Produce<S, T_Slash> : Production<1> {};
template <> struct Produce<S, T_Reg> : Production<1> {};

template <> struct Produce<Alt, T_LPar> : Production<2> {};
template <> struct Produce<Alt, T_Slash> : Production<2> {};
template <> struct Produce<Alt, T_Reg> : Production<2> {};

template <> struct Produce<Cat, T_LPar> : Production<5> {};
template <> struct Produce<Cat, T_Slash> : Production<5> {};
template <> struct Produce<Cat, T_Reg> : Production<5> {};

template <> struct Produce<Cats, T_RPar> : Production<4> {};
template <> struct Produce<Cats, T_Bar> : Production<3> {};
template <> struct Produce<Cats, T_Eot> : Production<4> {};

// 4 10 6
template <> struct Produce<Reps, T_LPar> : Production<6> {};
// 4 11 7
template <> struct Produce<Reps, T_RPar> : Production<7> {};
// 4 12 6
template <> struct Produce<Reps, T_Slash> : Production<6> {};
// 4 13 7
template <> struct Produce<Reps, T_Bar> : Production<7> {};
// 4 14 6
template <> struct Produce<Reps, T_Reg> : Production<6> {};
// 4 15 7
template <> struct Produce<Reps, T_Eot> : Production<7> {};
// 5 10 16
template <> struct Produce<Unt, T_LPar> : Production<16> {};
// 5 12 9
template <> struct Produce<Unt, T_Slash> : Production<9> {};
// 5 14 8
template <> struct Produce<Unt, T_Reg> : Production<8> {};
// 6 9 17
template <> struct Produce<Amt, T_Rep> : Production<17> {};
// 6 10 18
template <> struct Produce<Amt, T_LPar> : Production<18> {};
// 6 11 18
template <> struct Produce<Amt, T_RPar> : Production<18> {};
// 6 12 18
template <> struct Produce<Amt, T_Slash> : Production<18> {};
// 6 13 18
template <> struct Produce<Amt, T_Bar> : Production<18> {};
// 6 14 18
template <> struct Produce<Amt, T_Reg> : Production<18> {};
// 6 15 18
template <> struct Produce<Amt, T_Eot> : Production<18> {};
// 7 9 11
template <> struct Produce<Lit, T_Rep> : Production<11> {};
// 7 10 12
template <> struct Produce<Lit, T_LPar> : Production<12> {};
// 7 11 13
template <> struct Produce<Lit, T_RPar> : Production<13> {};
// 7 12 14
template <> struct Produce<Lit, T_Slash> : Production<14> {};
// 7 13 15
template <> struct Produce<Lit, T_Bar> : Production<15> {};
// 7 14 10
template <> struct Produce<Lit, T_Reg> : Production<10> {};

} // namespace produce
template <Symbol nt, Symbol p>
using produce_t = typename produce::Produce<nt, p>::type;

template <Symbol nt, typename p, typename s, typename n>
struct Predict
    : ParseRegex<p, concat_t<produce_t<nt, lex_v<p>>, s>, Nodes<int>> {};

////////////////// End Predict ////////////////////////////////////////////////

} // namespace detail

template <RegexString regex>
struct CompileTimeRegex
    : detail::ParseRegex<detail::Progress<regex.data, 0>,
                         detail::Sentinels<Symbol::S>, detail::Nodes<>> {};
template <RegexString regex>
using compile_time_regex_t = typename CompileTimeRegex<regex>::type;

static_assert(std::is_same_v<compile_time_regex_t<"hello">, int>);
static_assert(std::is_same_v<compile_time_regex_t<"a)">, ParseFailure>);
} // namespace compile_time
} // namespace toyctre
