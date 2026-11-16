// Unit tests for the individual metafunctions in compile_time.hpp.
//
// The end-to-end tests (ct_matcher_test.cpp, the ct_parse cases) run whole
// parses; these check each piece on its own, so a failure points at the
// metafunction that broke rather than somewhere in a deep instantiation stack.
//
// Every check is a STATIC_REQUIRE: a wrong result is a compile error at that
// line, and a passing one is recorded as a success under its TEST_CASE.
//
// These reach into detail:: on purpose, so they will need updating when the
// internals change. The node, Symbol and State shapes are the ones most likely
// to move.

#include "toyctre/compile_time.hpp"
#include "toyctre/grammar.hpp"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <type_traits>

namespace toyctre::compile_time::detail {
namespace {

using enum Symbol;

template <typename A, typename B> constexpr bool same = std::is_same_v<A, B>;

// Leaf nodes used throughout.
using La = Literal<false, 'a'>;
using Lb = Literal<false, 'b'>;
using Lc = Literal<false, 'c'>;
using EscStar = Literal<true, '*'>;

// Has no ::type, to check that LazyIf never touches the branch it doesn't take.
struct NoType {};

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

TEST_CASE("CT unit: at_t", "[ct_unit][helpers]") {
  STATIC_REQUIRE(same<at_t<0, int, char, long>, int>);
  STATIC_REQUIRE(same<at_t<1, int, char, long>, char>);
  STATIC_REQUIRE(same<at_t<2, int, char, long>, long>);
  // Duplicates are fine: each element has its own index.
  STATIC_REQUIRE(same<at_t<1, int, int, int>, int>);
  // The Id wrapper preserves types a plain return type would mangle or reject.
  STATIC_REQUIRE(same<at_t<0, const int>, const int>);
  STATIC_REQUIRE(same<at_t<0, int &>, int &>);
  STATIC_REQUIRE(same<at_t<0, int[3]>, int[3]>);
  STATIC_REQUIRE(same<at_t<0, void()>, void()>);
}

// ---------------------------------------------------------------------------
// RegexString, lexing, Progress
// ---------------------------------------------------------------------------

TEST_CASE("CT unit: RegexString", "[ct_unit][progress]") {
  constexpr auto s = RegexString{"ab"};
  // The trailing NUL is stored but not counted.
  STATIC_REQUIRE(s.size() == 2);
  STATIC_REQUIRE(s.data[0] == 'a');
  STATIC_REQUIRE(s.data[1] == 'b');
  STATIC_REQUIRE(s.data[2] == '\0');
}

TEST_CASE("CT unit: lexChar", "[ct_unit][progress]") {
  STATIC_REQUIRE(grammar::lexChar('(') == T_LPar);
  STATIC_REQUIRE(grammar::lexChar(')') == T_RPar);
  STATIC_REQUIRE(grammar::lexChar('*') == T_Rep);
  STATIC_REQUIRE(grammar::lexChar('?') == T_Rep);
  STATIC_REQUIRE(grammar::lexChar('+') == T_Rep);
  STATIC_REQUIRE(grammar::lexChar('|') == T_Bar);
  STATIC_REQUIRE(grammar::lexChar('\\') == T_Esc);
  STATIC_REQUIRE(grammar::lexChar('a') == T_Reg);
  STATIC_REQUIRE(grammar::lexChar('.') == T_Reg);
  // NUL lexes as an ordinary char; EOT comes from Progress::at_eot instead.
  STATIC_REQUIRE(grammar::lexChar('\0') == T_Reg);
}

TEST_CASE("CT unit: Progress", "[ct_unit][progress]") {
  using P0 = Progress<"a(", 0>;
  using P1 = Progress<"a(", 1>;
  using P2 = Progress<"a(", 2>;

  STATIC_REQUIRE(P0::current_char == 'a');
  STATIC_REQUIRE(P1::current_char == '(');
  STATIC_REQUIRE(!P0::at_eot);
  STATIC_REQUIRE(!P1::at_eot);
  STATIC_REQUIRE(P2::at_eot);

  STATIC_REQUIRE(same<P0::next, P1>);
  STATIC_REQUIRE(same<P1::next, P2>);

  STATIC_REQUIRE(lex_v<P0> == T_Reg);
  STATIC_REQUIRE(lex_v<P1> == T_LPar);
  STATIC_REQUIRE(lex_v<P2> == T_Eot);

  STATIC_REQUIRE(IsProgress<P0>);
  STATIC_REQUIRE(!IsProgress<int>);
  STATIC_REQUIRE(IsEot<P2>);
  STATIC_REQUIRE(!IsEot<P0>);
  STATIC_REQUIRE(!IsEot<int>);

  // The empty regex is at EOT immediately.
  STATIC_REQUIRE(IsEot<Progress<"", 0>>);
}

TEST_CASE("CT unit: Progress::capture_number_v", "[ct_unit][progress]") {
  // The number of unescaped '(' before position N.
  STATIC_REQUIRE(Progress<"a(b)", 0>::capture_number_v == 0);
  STATIC_REQUIRE(Progress<"a(b)", 1>::capture_number_v == 0);
  STATIC_REQUIRE(Progress<"a(b)", 2>::capture_number_v == 1);
  STATIC_REQUIRE(Progress<"a(b)", 4>::capture_number_v == 1);
  STATIC_REQUIRE(Progress<"((a)(b))", 2>::capture_number_v == 2);
  STATIC_REQUIRE(Progress<"((a)(b))", 5>::capture_number_v == 3);
  STATIC_REQUIRE(Progress<"", 0>::capture_number_v == 0);
  // An escaped paren isn't counted, but one after an escaped backslash is.
  STATIC_REQUIRE(Progress<R"re(\()re", 2>::capture_number_v == 0);
  STATIC_REQUIRE(Progress<R"re(\\()re", 3>::capture_number_v == 1);
}

// ---------------------------------------------------------------------------
// Parse tree
// ---------------------------------------------------------------------------

TEST_CASE("CT unit: is_regex_v", "[ct_unit][parse_tree]") {
  STATIC_REQUIRE(IsRegex<La>);
  STATIC_REQUIRE(IsRegex<EscStar>);
  STATIC_REQUIRE(IsRegex<Group<La, 1>>);
  STATIC_REQUIRE(IsRegex<Sequence<La, Lb>>);
  STATIC_REQUIRE(IsRegex<Alternatives<La, Lb>>);
  STATIC_REQUIRE(IsRegex<Repetition<La, '*', 0>>);
  STATIC_REQUIRE(IsRegex<Repetition<Group<Sequence<La, Lb>, 1>, '+', 0>>);

  STATIC_REQUIRE(!IsRegex<int>);
  STATIC_REQUIRE(!IsRegex<Mark>);
  STATIC_REQUIRE(!IsRegex<ParseFailure>);
  STATIC_REQUIRE(!IsRegex<Nodes<La>>);
}

// ---------------------------------------------------------------------------
// State concepts
// ---------------------------------------------------------------------------

TEST_CASE("CT unit: State concepts", "[ct_unit][state]") {
  using S = State<Progress<"a", 0>, Sentinels<Start>, Nodes<>>;

  STATIC_REQUIRE(IsSentinels<Sentinels<>>);
  STATIC_REQUIRE(IsSentinels<Sentinels<Start, T_Eot>>);
  STATIC_REQUIRE(!IsSentinels<Nodes<>>);

  STATIC_REQUIRE(IsNodes<Nodes<>>);
  STATIC_REQUIRE(IsNodes<Nodes<La, Mark>>);
  STATIC_REQUIRE(!IsNodes<Sentinels<>>);

  STATIC_REQUIRE(state::IsState<S>);
  STATIC_REQUIRE(!state::IsState<ParseFailure>);

  // A result is either a finished tree or a failure.
  STATIC_REQUIRE(IsResult<ParseFailure>);
  STATIC_REQUIRE(IsResult<La>);
  STATIC_REQUIRE(IsResult<Sequence<La, Lb>>);
  STATIC_REQUIRE(!IsResult<S>);
  STATIC_REQUIRE(!IsResult<Mark>);

  STATIC_REQUIRE(IsResultOrState<S>);
  STATIC_REQUIRE(IsResultOrState<La>);
  STATIC_REQUIRE(IsResultOrState<ParseFailure>);
  STATIC_REQUIRE(!IsResultOrState<Mark>);
  STATIC_REQUIRE(!IsResultOrState<int>);
}

// ---------------------------------------------------------------------------
// ConsumeTerminal
// ---------------------------------------------------------------------------

TEST_CASE("CT unit: ConsumeTerminal", "[ct_unit][parser]") {
  using P0 = Progress<"a", 0>;
  using P1 = Progress<"a", 1>; // at EOT
  using S = Sentinels<A_Cats>;
  using N = Nodes<Mark>;

  // Matching terminal: advance, keep sentinels and nodes.
  STATIC_REQUIRE(same<ConsumeTerminal<T_Reg, P0, S, N>::type, State<P1, S, N>>);
  // Mismatched terminal.
  STATIC_REQUIRE(same<ConsumeTerminal<T_Bar, P0, S, N>::type, ParseFailure>);
  // EOT at EOT: matched without advancing (there's nothing to advance to).
  STATIC_REQUIRE(same<ConsumeTerminal<T_Eot, P1, S, N>::type, State<P1, S, N>>);
  // EOT expected but input remains.
  STATIC_REQUIRE(same<ConsumeTerminal<T_Eot, P0, S, N>::type, ParseFailure>);
  // A real terminal expected but input is exhausted.
  STATIC_REQUIRE(same<ConsumeTerminal<T_Reg, P1, S, N>::type, ParseFailure>);
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

TEST_CASE("CT unit: Merge", "[ct_unit][parser]") {
  using action::Merge;

  // The stack is top-first, so the merged children come out reversed back
  // into source order. The Mark is consumed; what's below it is kept.
  STATIC_REQUIRE(same<Merge<Sequence, Nodes<Lb, La, Mark, Lc>>::type,
                      Nodes<Sequence<La, Lb>, Lc>>);
  STATIC_REQUIRE(same<Merge<Sequence, Nodes<Lc, Lb, La, Mark>>::type,
                      Nodes<Sequence<La, Lb, Lc>>>);
  STATIC_REQUIRE(same<Merge<Alternatives, Nodes<Lb, La, Mark, Mark>>::type,
                      Nodes<Alternatives<La, Lb>, Mark>>);
  // Only the nearest Mark counts.
  STATIC_REQUIRE(same<Merge<Sequence, Nodes<Lb, La, Mark, Lc, Mark>>::type,
                      Nodes<Sequence<La, Lb>, Lc, Mark>>);
  // A single child is not wrapped.
  STATIC_REQUIRE(
      same<Merge<Sequence, Nodes<La, Mark, Lc>>::type, Nodes<La, Lc>>);
  STATIC_REQUIRE(same<Merge<Alternatives, Nodes<La, Mark>>::type, Nodes<La>>);
}

TEST_CASE("CT unit: NextNodes", "[ct_unit][parser]") {
  using action::next_nodes_t;

  STATIC_REQUIRE(same<next_nodes_t<A_Mark, 'x', Nodes<La>>, Nodes<Mark, La>>);
  STATIC_REQUIRE(same<next_nodes_t<A_Lit, 'b', Nodes<La>>, Nodes<Lb, La>>);
  STATIC_REQUIRE(same<next_nodes_t<A_Esc, '*', Nodes<>>, Nodes<EscStar>>);
  // STATIC_REQUIRE(same<next_nodes_t<A_Rep, '+', Nodes<La, Mark>>,
  //                     Nodes<Repetition<La, '+'>, Mark>>);
  STATIC_REQUIRE(
      same<next_nodes_t<A_Group, 'x', Nodes<La, CaptureMark<1>, Mark>>,
           Nodes<Group<La, 1>, Mark>>);
  STATIC_REQUIRE(same<next_nodes_t<A_Cats, 'x', Nodes<Lb, La, Mark>>,
                      Nodes<Sequence<La, Lb>>>);
  STATIC_REQUIRE(same<next_nodes_t<A_Alts, 'x', Nodes<Lb, La, Mark>>,
                      Nodes<Alternatives<La, Lb>>>);
}

TEST_CASE("CT unit: DoAction", "[ct_unit][parser]") {
  using P = Progress<"*", 0>;
  using S = Sentinels<T_Rep>;

  // The action sees the current char but doesn't consume it.
  STATIC_REQUIRE(same<DoAction<A_Rep, P, S, Nodes<La>>::type,
                      State<P, S, Nodes<Repetition<La, '*', 0>>>>);
  STATIC_REQUIRE(same<DoAction<A_Reject, P, S, Nodes<La>>::type, ParseFailure>);
}

// ---------------------------------------------------------------------------
// Prediction
// ---------------------------------------------------------------------------

TEST_CASE("CT unit: produce_t", "[ct_unit][parser]") {
  STATIC_REQUIRE(same<produce_t<Start, T_Reg>, Sentinels<Alt, T_Eot>>);
  STATIC_REQUIRE(
      same<produce_t<Alt, T_LPar>, Sentinels<A_Mark, Cat, Cats, A_Alts>>);
  STATIC_REQUIRE(same<produce_t<Cats, T_Bar>, Sentinels<T_Bar, Cat, Cats>>);
  STATIC_REQUIRE(same<produce_t<Cats, T_Eot>, Sentinels<>>);
  STATIC_REQUIRE(same<produce_t<Unt, T_LPar>,
                      Sentinels<T_LPar, A_Capture, Alt, A_Group, T_RPar>>);
  STATIC_REQUIRE(same<produce_t<Unt, T_Esc>, Sentinels<T_Esc, A_Esc, Lit>>);
  STATIC_REQUIRE(same<produce_t<Amt, T_Rep>, Sentinels<A_Rep, T_Rep>>);
  STATIC_REQUIRE(same<produce_t<Amt, T_Reg>, Sentinels<>>);
  // An escaped metacharacter lexes as a plain T_Reg afterwards.
  STATIC_REQUIRE(same<produce_t<Lit, T_Rep>, Sentinels<T_Rep>>);

  // Empty table cells reject.
  STATIC_REQUIRE(
      same<produce_t<Start, T_Eot>, Sentinels<A_Reject>>); // empty regex
  STATIC_REQUIRE(same<produce_t<Cat, T_Bar>, Sentinels<A_Reject>>); // empty alt
  STATIC_REQUIRE(
      same<produce_t<Unt, T_RPar>, Sentinels<A_Reject>>); // empty group
  STATIC_REQUIRE(
      same<produce_t<Unt, T_Rep>, Sentinels<A_Reject>>); // leading rep
  STATIC_REQUIRE(
      same<produce_t<Lit, T_Eot>, Sentinels<A_Reject>>); // trailing '\'
}

TEST_CASE("CT unit: Predict", "[ct_unit][parser]") {
  // The production goes in front of the remaining sentinels.
  STATIC_REQUIRE(
      same<predict_t<Start, T_Reg, T_Bar>, Sentinels<Alt, T_Eot, T_Bar>>);
  STATIC_REQUIRE(same<predict_t<Amt, T_Reg, T_Bar>, Sentinels<T_Bar>>);
  STATIC_REQUIRE(same<predict_t<Cats, T_Reg>, Sentinels<A_Reject>>);
}

// ---------------------------------------------------------------------------
// Iteration
// ---------------------------------------------------------------------------

TEST_CASE("CT unit: Symbol classification", "[ct_unit][parser]") {
  using iterate::isAction;
  using iterate::isTerminal;

  STATIC_REQUIRE(isTerminal(T_Rep));
  STATIC_REQUIRE(isTerminal(T_Eot));
  STATIC_REQUIRE(!isTerminal(Start));
  STATIC_REQUIRE(!isTerminal(Lit));
  STATIC_REQUIRE(!isTerminal(A_Mark));

  STATIC_REQUIRE(isAction(A_Mark));
  STATIC_REQUIRE(isAction(A_Reject));
  STATIC_REQUIRE(!isAction(T_Eot));
  STATIC_REQUIRE(!isAction(Start));
}

TEST_CASE("CT unit: Step", "[ct_unit][parser]") {
  using iterate::Step;
  using P0 = Progress<"a", 0>;
  using P1 = Progress<"a", 1>;

  // Each kind of top sentinel dispatches to its handler.
  STATIC_REQUIRE(same<Step<State<P0, Sentinels<A_Mark, T_Eot>, Nodes<>>>::type,
                      State<P0, Sentinels<T_Eot>, Nodes<Mark>>>);
  STATIC_REQUIRE(same<Step<State<P0, Sentinels<T_Reg>, Nodes<>>>::type,
                      State<P1, Sentinels<>, Nodes<>>>);
  STATIC_REQUIRE(same<Step<State<P0, Sentinels<Start>, Nodes<>>>::type,
                      State<P0, Sentinels<Alt, T_Eot>, Nodes<>>>);

  // No sentinels left: success only with exactly one node at EOT.
  STATIC_REQUIRE(same<Step<State<P1, Sentinels<>, Nodes<La>>>::type, La>);
  STATIC_REQUIRE(
      same<Step<State<P0, Sentinels<>, Nodes<La>>>::type, ParseFailure>);

  // Results are fixed points.
  STATIC_REQUIRE(same<Step<La>::type, La>);
  STATIC_REQUIRE(same<Step<ParseFailure>::type, ParseFailure>);
}

TEST_CASE("CT unit: Iter and IterTilResult", "[ct_unit][parser]") {
  using iterate::iter_t;
  using iterate::Step;
  using S0 = initial_state_t<"a">;

  STATIC_REQUIRE(same<S0, State<Progress<"a", 0>, Sentinels<Start>, Nodes<>>>);

  using S1 = Step<S0>::type;
  using S2 = Step<S1>::type;
  using S3 = Step<S2>::type;
  using S4 = Step<S3>::type;
  // iter_t<S, n> takes 2^n steps.
  STATIC_REQUIRE(same<iter_t<S0, 0>, S1>);
  STATIC_REQUIRE(same<iter_t<S0, 1>, S2>);
  STATIC_REQUIRE(same<iter_t<S0, 2>, S4>);

  STATIC_REQUIRE(same<IterTilResult<S0>::type, La>);
  STATIC_REQUIRE(same<IterTilResult<La>::type, La>);
  STATIC_REQUIRE(same<IterTilResult<ParseFailure>::type, ParseFailure>);
}

TEST_CASE("CT unit: compile_time_regex_t tree shapes", "[ct_unit][parser]") {
  // A single child is never wrapped in a one-element Sequence/Alternatives.
  STATIC_REQUIRE(same<compile_time_regex_t<"a">, La>);
  STATIC_REQUIRE(same<compile_time_regex_t<"ab">, Sequence<La, Lb>>);
  STATIC_REQUIRE(same<compile_time_regex_t<"a|b">, Alternatives<La, Lb>>);
  STATIC_REQUIRE(
      same<compile_time_regex_t<"a|bc">, Alternatives<La, Sequence<Lb, Lc>>>);
  STATIC_REQUIRE(same<compile_time_regex_t<"(a)">, Group<La, 1>>);
  STATIC_REQUIRE(same<compile_time_regex_t<"(a)(b)">,
                      Sequence<Group<La, 1>, Group<Lb, 2>>>);
  STATIC_REQUIRE(same<compile_time_regex_t<"((a))">, Group<Group<La, 2>, 1>>);
  STATIC_REQUIRE(same<compile_time_regex_t<"(ab)*">,
                      Repetition<Group<Sequence<La, Lb>, 1>, '*', 0>>);
  STATIC_REQUIRE(
      same<compile_time_regex_t<"ab+">, Sequence<La, Repetition<Lb, '+', 0>>>);
  STATIC_REQUIRE(same<compile_time_regex_t<R"(\*)">, EscStar>);
  STATIC_REQUIRE(same<compile_time_regex_t<"a(">, ParseFailure>);
  STATIC_REQUIRE(same<compile_time_regex_t<"">, ParseFailure>);
}

// ---------------------------------------------------------------------------
// Matcher support
// ---------------------------------------------------------------------------

//
// TEST_CASE("CT unit: Matcher with explicit continuations",
//           "[ct_unit][matcher]") {
//   // The pack is the node plus everything that must match after it.
//   using CaptureData = match::CaptureData<0, 2>;
//   CaptureData captureData{};
//   STATIC_REQUIRE(
//       CapturingMatcher<CaptureData>::Matcher<>::match("", captureData));
//   STATIC_REQUIRE(
//       !CapturingMatcher<CaptureData>::Matcher<>::match("a", captureData));
//   STATIC_REQUIRE(
//       CapturingMatcher<CaptureData>::Matcher<La>::match("a", captureData));
//   STATIC_REQUIRE(
//       CapturingMatcher<CaptureData>::Matcher<La, Lb>::match("ab",
//       captureData));
//   STATIC_REQUIRE(
//       !CapturingMatcher<CaptureData>::Matcher<La, Lb>::match("a",
//       captureData));
//   // Repetition gives back characters to the continuation.
//   STATIC_REQUIRE(
//       CapturingMatcher<CaptureData>::Matcher<Repetition<La, '*'>, La>::match(
//           "aaa", captureData));
//   STATIC_REQUIRE(
//       !CapturingMatcher<CaptureData>::Matcher<Repetition<La, '+'>,
//       La>::match(
//           "a", captureData));
//   // Alternatives backtrack CaptureDatao the continuation.
//   STATIC_REQUIRE(
//       CapturingMatcher<CaptureData>::Matcher<Alternatives<La, Sequence<La,
//       Lb>>,
//                                              Lb>::match("ab", captureData));
//   STATIC_REQUIRE(
//       CapturingMatcher<CaptureData>::Matcher<Alternatives<La, Sequence<La,
//       Lb>>,
//                                              Lb>::match("abb", captureData));
//   // Empty-matching loop body terminates.
//   STATIC_REQUIRE(
//       CapturingMatcher<CaptureData>::Matcher<
//           Repetition<Repetition<La, '?'>, '*'>>::match("aa", captureData));
//   STATIC_REQUIRE(CapturingMatcher<CaptureData>::Matcher<
//                  Repetition<Repetition<La, '?'>, '*'>>::match("",
//                  captureData));
// }

TEST_CASE("CT unit: CaptureData", "[ct_unit][matcher]") {
  // Groups are numbered from 1, and start out zeroed.
  STATIC_REQUIRE([] {
    auto d = CaptureData<1, 0>{};
    return d.groupStart(1) == 0 && d.groupEnd(1) == 0;
  }());
  // Each group's start and end are separate slots.
  STATIC_REQUIRE([] {
    auto d = CaptureData<2, 0>{};
    d.groupStart(1) = 10;
    d.groupEnd(1) = 11;
    d.groupStart(2) = 20;
    d.groupEnd(2) = 21;
    return d.groupStart(1) == 10 && d.groupEnd(1) == 11 &&
           d.groupStart(2) == 20 && d.groupEnd(2) == 21;
  }());

  auto d = CaptureData<1, 0>{};
  CHECK_THROWS_AS(d.groupStart(0), std::out_of_range);
  CHECK_THROWS_AS(d.groupEnd(0), std::out_of_range);
  CHECK_THROWS_AS(d.groupStart(2), std::out_of_range);
  CHECK_THROWS_AS(d.groupEnd(2), std::out_of_range);
}

TEST_CASE("CT unit: Group and EndGroup matchers", "[ct_unit][matcher]") {
  using M = CapturingMatcher<CaptureData<2, 1>>;
  using match::EndGroup;

  // Positions are recorded as the length of the text remaining.
  STATIC_REQUIRE([] {
    auto d = CaptureData<2, 1>{};
    return M::Matcher<La, Group<Lb, 1>, Lc>::match("abc", d) &&
           d.groupStart(1) == 2 && d.groupEnd(1) == 1;
  }());
  // EndGroup records only the end.
  STATIC_REQUIRE([] {
    auto d = CaptureData<2, 1>{};
    return M::Matcher<La, EndGroup<1>, Lb>::match("ab", d) &&
           d.groupStart(1) == 0 && d.groupEnd(1) == 1;
  }());
  // A group matching the empty string has start == end.
  STATIC_REQUIRE([] {
    auto d = CaptureData<2, 1>{};
    return M::Matcher<La, Group<Repetition<Lb, '*', 0>, 1>>::match("a", d) &&
           d.groupStart(1) == 0 && d.groupEnd(1) == 0;
  }());

  // When the match fails, the previous values are restored...
  // ...if the group's contents fail,
  STATIC_REQUIRE([] {
    auto d = CaptureData<2, 1>{};
    d.groupStart(1) = 7;
    d.groupEnd(1) = 8;
    return !M::Matcher<Group<Lb, 1>>::match("a", d) && d.groupStart(1) == 7 &&
           d.groupEnd(1) == 8;
  }());
  // ...or what follows the group fails.
  STATIC_REQUIRE([] {
    auto d = CaptureData<2, 1>{};
    d.groupStart(1) = 7;
    d.groupEnd(1) = 8;
    return !M::Matcher<Group<La, 1>, Lc>::match("ab", d) &&
           d.groupStart(1) == 7 && d.groupEnd(1) == 8;
  }());
  STATIC_REQUIRE([] {
    auto d = CaptureData<2, 1>{};
    d.groupEnd(1) = 8;
    return !M::Matcher<EndGroup<1>, Lc>::match("a", d) && d.groupEnd(1) == 8;
  }());

  // A failed first alternative leaves nothing behind for the second.
  STATIC_REQUIRE([] {
    auto d = CaptureData<2, 1>{};
    return M::Matcher<Alternatives<Sequence<Group<La, 1>, Lb>,
                                   Sequence<La, Lc>>>::match("ac", d) &&
           d.groupStart(1) == 0 && d.groupEnd(1) == 0;
  }());
}

} // namespace
} // namespace toyctre::compile_time::detail
