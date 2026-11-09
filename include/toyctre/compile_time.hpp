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

template <typename V, V v> struct HasValue {
  static constexpr V value = v;
};

template <typename, typename> struct Concat;
template <typename T, template <T...> class LIST, T... l1, T... l2>
struct Concat<LIST<l1...>, LIST<l2...>> : Ty<LIST<l1..., l2...>> {};
template <template <typename...> class LIST, typename... l1, typename... l2>
struct Concat<LIST<l1...>, LIST<l2...>> : Ty<LIST<l1..., l2...>> {};

template <typename l1, typename l2>
using concat_t = typename Concat<l1, l2>::type;

} // namespace helpers

namespace detail {
using namespace helpers;

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

template <Symbol t, char c> struct Matches {
  static constexpr auto value = lex_char_v<c> == t;
};
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

template <typename p>
struct Lex : if_value<Symbol, is_eot_v<p>, Symbol::T_Eot,
                      lex_char_v<current_char_v<p>>> {};

////////////////// End Lex ////////////////////////////////////////////////////

template <Symbol...> struct Sen {};
template <typename...> struct Nodes {};
template <typename, typename, typename> struct ParseRegex;
template <Symbol, typename, typename, typename> struct DoAction;
template <Symbol, typename, typename, typename> struct ConsumeTerminal;
template <Symbol, typename, typename, typename> struct Predict;

template <typename node, typename p>
struct ParseRegex<p, Sen<>, Nodes<node>>
    : if_<is_eot_v<p>, Ty<node>, Ty<ParseFailure>> {};
template <typename, typename, typename> struct ParseRegex {};

constexpr bool isAction(Symbol s) { return s > Symbol::ACTIONS; }
constexpr bool isTerminal(Symbol s) {
  return s > Symbol::TERMINALS && s < Symbol::ACTIONS;
}

template <typename r, Symbol t, Symbol... ss, typename n>
struct ParseRegex<r, Sen<t, ss...>, n>
    : if_<isAction(t), DoAction<t, r, Sen<ss...>, n>,
          if_<isTerminal(t), ConsumeTerminal<t, r, Sen<ss...>, n>,
              Predict<t, r, Sen<ss...>, n>>> {};

////////////////////// ConsumeTerminal ////////////////////////////////////////
template <typename p, typename s, typename n>
struct ParseNext : ParseRegex<advance_t<p>, s, n> {};

template <Symbol t, typename p, typename s, typename n>
struct ConsumeTerminal : if_<!is_eot_v<p> && matches_v<t, current_char_v<p>>,
                             ParseNext<p, s, n>, Ty<ParseFailure>> {};

template <typename p, typename s, typename n>
struct ConsumeTerminal<Symbol::T_Eot, p, s, n>
    : if_<is_eot_v<p>, ParseRegex<p, s, n>, Ty<ParseFailure>> {};
////////////////// End ConsumeTerminal ////////////////////////////////////////

////////////////////// DoAction ///////////////////////////////////////////////
template <Symbol, typename p, typename s, typename>
struct DoAction : ParseRegex<p, s, Nodes<int>> {};

template <typename p, typename s, typename n>
struct DoAction<Symbol::A_Reject, p, s, n> : Ty<ParseFailure> {};
////////////////// End DoAction ///////////////////////////////////////////////

////////////////////// Predict ////////////////////////////////////////////////

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

template <Symbol nt, typename p, typename s, typename n>
struct Predict
    : ParseRegex<p, concat_t<produce_t<nt, lex_v<p>>, s>, Nodes<int>> {};

////////////////// End Predict ////////////////////////////////////////////////

} // namespace detail

template <RegexString regex>
struct CompileTimeRegex
    : detail::ParseRegex<detail::Progress<regex.data, 0>,
                         detail::Sen<Symbol::S>, detail::Nodes<>> {};
template <RegexString regex>
using compile_time_regex_t = typename CompileTimeRegex<regex>::type;

} // namespace compile_time
} // namespace toyctre
