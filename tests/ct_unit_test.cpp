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

#include <string_view>
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

TEST_CASE("CT unit: Id and LazyIf", "[ct_unit][helpers]") {
  STATIC_REQUIRE(same<Id<int>::type, int>);
  STATIC_REQUIRE(same<LazyIf<true, Id<int>, Id<char>>::type, int>);
  STATIC_REQUIRE(same<LazyIf<false, Id<int>, Id<char>>::type, char>);
  // Laziness: the untaken branch has no ::type and is never asked for one.
  STATIC_REQUIRE(same<LazyIf<true, Id<int>, NoType>::type, int>);
  STATIC_REQUIRE(same<LazyIf<false, NoType, Id<char>>::type, char>);
}

TEST_CASE("CT unit: Type list helpers", "[ct_unit][helpers]") {
  // Concat on type lists and on value lists (Sen<Symbol...>).
  STATIC_REQUIRE(
      same<concat_t<Nodes<int>, Nodes<char, long>>, Nodes<int, char, long>>);
  STATIC_REQUIRE(same<concat_t<Nodes<>, Nodes<>>, Nodes<>>);
  STATIC_REQUIRE(
      same<concat_t<Sen<Alt, T_Eot>, Sen<T_Bar>>, Sen<Alt, T_Eot, T_Bar>>);
  STATIC_REQUIRE(same<concat_t<Sen<>, Sen<T_Bar>>, Sen<T_Bar>>);

  STATIC_REQUIRE(same<helpers::push_t<int, Nodes<char>>, Nodes<int, char>>);
  STATIC_REQUIRE(same<helpers::push_t<int, Nodes<>>, Nodes<int>>);

  STATIC_REQUIRE(
      same<ReplaceHead<int, Nodes<char, long>>::type, Nodes<int, long>>);
  STATIC_REQUIRE(same<ReplaceHead<int, Nodes<char>>::type, Nodes<int>>);

  STATIC_REQUIRE(same<head_t<Nodes<int, char>>, int>);
}

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
  // The trailing NUL is stored and counted.
  STATIC_REQUIRE(s.size() == 3);
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

// ---------------------------------------------------------------------------
// Parse tree
// ---------------------------------------------------------------------------

TEST_CASE("CT unit: is_regex_v", "[ct_unit][parse_tree]") {
  STATIC_REQUIRE(IsRegex<La>);
  STATIC_REQUIRE(IsRegex<EscStar>);
  STATIC_REQUIRE(IsRegex<Group<La>>);
  STATIC_REQUIRE(IsRegex<Sequence<La, Lb>>);
  STATIC_REQUIRE(IsRegex<Alternatives<La, Lb>>);
  STATIC_REQUIRE(IsRegex<Repetition<La, '*'>>);
  STATIC_REQUIRE(IsRegex<Repetition<Group<Sequence<La, Lb>>, '+'>>);

  STATIC_REQUIRE(!IsRegex<int>);
  STATIC_REQUIRE(!IsRegex<Mark>);
  STATIC_REQUIRE(!IsRegex<ParseFailure>);
  STATIC_REQUIRE(!IsRegex<Nodes<La>>);
}

// ---------------------------------------------------------------------------
// State concepts
// ---------------------------------------------------------------------------

TEST_CASE("CT unit: State concepts", "[ct_unit][state]") {
  using S = State<Progress<"a", 0>, Sen<Start>, Nodes<>>;

  STATIC_REQUIRE(IsSentinels<Sen<>>);
  STATIC_REQUIRE(IsSentinels<Sen<Start, T_Eot>>);
  STATIC_REQUIRE(!IsSentinels<Nodes<>>);

  STATIC_REQUIRE(IsNodes<Nodes<>>);
  STATIC_REQUIRE(IsNodes<Nodes<La, Mark>>);
  STATIC_REQUIRE(!IsNodes<Sen<>>);

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
  using S = Sen<A_Cats>;
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
  STATIC_REQUIRE(same<next_nodes_t<A_Rep, '+', Nodes<La, Mark>>,
                      Nodes<Repetition<La, '+'>, Mark>>);
  STATIC_REQUIRE(same<next_nodes_t<A_Group, 'x', Nodes<La, Mark>>,
                      Nodes<Group<La>, Mark>>);
  STATIC_REQUIRE(same<next_nodes_t<A_Cats, 'x', Nodes<Lb, La, Mark>>,
                      Nodes<Sequence<La, Lb>>>);
  STATIC_REQUIRE(same<next_nodes_t<A_Alts, 'x', Nodes<Lb, La, Mark>>,
                      Nodes<Alternatives<La, Lb>>>);
}

TEST_CASE("CT unit: DoAction", "[ct_unit][parser]") {
  using P = Progress<"*", 0>;
  using S = Sen<T_Rep>;

  // The action sees the current char but doesn't consume it.
  STATIC_REQUIRE(same<DoAction<A_Rep, P, S, Nodes<La>>::type,
                      State<P, S, Nodes<Repetition<La, '*'>>>>);
  STATIC_REQUIRE(same<DoAction<A_Reject, P, S, Nodes<La>>::type, ParseFailure>);
}

// ---------------------------------------------------------------------------
// Prediction
// ---------------------------------------------------------------------------

TEST_CASE("CT unit: produce_t", "[ct_unit][parser]") {
  STATIC_REQUIRE(same<produce_t<Start, T_Reg>, Sen<Alt, T_Eot>>);
  STATIC_REQUIRE(same<produce_t<Alt, T_LPar>, Sen<A_Mark, Cat, Cats, A_Alts>>);
  STATIC_REQUIRE(same<produce_t<Cats, T_Bar>, Sen<T_Bar, Cat, Cats>>);
  STATIC_REQUIRE(same<produce_t<Cats, T_Eot>, Sen<>>);
  STATIC_REQUIRE(
      same<produce_t<Unt, T_LPar>, Sen<T_LPar, Alt, A_Group, T_RPar>>);
  STATIC_REQUIRE(same<produce_t<Unt, T_Esc>, Sen<T_Esc, A_Esc, Lit>>);
  STATIC_REQUIRE(same<produce_t<Amt, T_Rep>, Sen<A_Rep, T_Rep>>);
  STATIC_REQUIRE(same<produce_t<Amt, T_Reg>, Sen<>>);
  // An escaped metacharacter lexes as a plain T_Reg afterwards.
  STATIC_REQUIRE(same<produce_t<Lit, T_Rep>, Sen<T_Rep>>);

  // Empty table cells reject.
  STATIC_REQUIRE(same<produce_t<Start, T_Eot>, Sen<A_Reject>>); // empty regex
  STATIC_REQUIRE(same<produce_t<Cat, T_Bar>, Sen<A_Reject>>);   // empty alt
  STATIC_REQUIRE(same<produce_t<Unt, T_RPar>, Sen<A_Reject>>);  // empty group
  STATIC_REQUIRE(same<produce_t<Unt, T_Rep>, Sen<A_Reject>>);   // leading rep
  STATIC_REQUIRE(same<produce_t<Lit, T_Eot>, Sen<A_Reject>>);   // trailing '\'
}

TEST_CASE("CT unit: Predict", "[ct_unit][parser]") {
  using P = Progress<"a", 0>;
  // The production goes in front of the remaining sentinels.
  STATIC_REQUIRE(same<Predict<Start, P, Sen<T_Bar>, Nodes<>>::type,
                      State<P, Sen<Alt, T_Eot, T_Bar>, Nodes<>>>);
  STATIC_REQUIRE(same<Predict<Amt, P, Sen<T_Bar>, Nodes<>>::type,
                      State<P, Sen<T_Bar>, Nodes<>>>);
  STATIC_REQUIRE(same<Predict<Cats, P, Sen<>, Nodes<>>::type,
                      State<P, Sen<A_Reject>, Nodes<>>>);
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
  STATIC_REQUIRE(same<Step<State<P0, Sen<A_Mark, T_Eot>, Nodes<>>>::type,
                      State<P0, Sen<T_Eot>, Nodes<Mark>>>);
  STATIC_REQUIRE(same<Step<State<P0, Sen<T_Reg>, Nodes<>>>::type,
                      State<P1, Sen<>, Nodes<>>>);
  STATIC_REQUIRE(same<Step<State<P0, Sen<Start>, Nodes<>>>::type,
                      State<P0, Sen<Alt, T_Eot>, Nodes<>>>);

  // No sentinels left: success only with exactly one node at EOT.
  STATIC_REQUIRE(same<Step<State<P1, Sen<>, Nodes<La>>>::type, La>);
  STATIC_REQUIRE(same<Step<State<P0, Sen<>, Nodes<La>>>::type, ParseFailure>);

  // Results are fixed points.
  STATIC_REQUIRE(same<Step<La>::type, La>);
  STATIC_REQUIRE(same<Step<ParseFailure>::type, ParseFailure>);
}

TEST_CASE("CT unit: Iter and IterTilFixed", "[ct_unit][parser]") {
  using iterate::iter_t;
  using iterate::Step;
  using S0 = initial_state_t<"a">;

  STATIC_REQUIRE(same<S0, State<Progress<"a", 0>, Sen<Start>, Nodes<>>>);

  using S1 = Step<S0>::type;
  using S2 = Step<S1>::type;
  using S3 = Step<S2>::type;
  using S4 = Step<S3>::type;
  // iter_t<S, n> takes 2^n steps.
  STATIC_REQUIRE(same<iter_t<S0, 0>, S1>);
  STATIC_REQUIRE(same<iter_t<S0, 1>, S2>);
  STATIC_REQUIRE(same<iter_t<S0, 2>, S4>);

  STATIC_REQUIRE(same<IterTilFixed<S0>::type, La>);
  STATIC_REQUIRE(same<IterTilFixed<La>::type, La>);
  STATIC_REQUIRE(same<IterTilFixed<ParseFailure>::type, ParseFailure>);
}

TEST_CASE("CT unit: compile_time_regex_t tree shapes", "[ct_unit][parser]") {
  // A single child is never wrapped in a one-element Sequence/Alternatives.
  STATIC_REQUIRE(same<compile_time_regex_t<"a">, La>);
  STATIC_REQUIRE(same<compile_time_regex_t<"ab">, Sequence<La, Lb>>);
  STATIC_REQUIRE(same<compile_time_regex_t<"a|b">, Alternatives<La, Lb>>);
  STATIC_REQUIRE(
      same<compile_time_regex_t<"a|bc">, Alternatives<La, Sequence<Lb, Lc>>>);
  STATIC_REQUIRE(same<compile_time_regex_t<"(a)">, Group<La>>);
  STATIC_REQUIRE(same<compile_time_regex_t<"(ab)*">,
                      Repetition<Group<Sequence<La, Lb>>, '*'>>);
  STATIC_REQUIRE(
      same<compile_time_regex_t<"ab+">, Sequence<La, Repetition<Lb, '+'>>>);
  STATIC_REQUIRE(same<compile_time_regex_t<R"(\*)">, EscStar>);
  STATIC_REQUIRE(same<compile_time_regex_t<"a(">, ParseFailure>);
  STATIC_REQUIRE(same<compile_time_regex_t<"">, ParseFailure>);
}

// ---------------------------------------------------------------------------
// Matcher support
// ---------------------------------------------------------------------------

TEST_CASE("CT unit: is_nullable_v", "[ct_unit][matcher]") {
  using match::is_nullable_v;

  STATIC_REQUIRE(!is_nullable_v<La>);
  STATIC_REQUIRE(is_nullable_v<Repetition<La, '*'>>);
  STATIC_REQUIRE(is_nullable_v<Repetition<La, '?'>>);
  STATIC_REQUIRE(!is_nullable_v<Repetition<La, '+'>>);
  STATIC_REQUIRE(is_nullable_v<Repetition<Repetition<La, '?'>, '+'>>);
  STATIC_REQUIRE(!is_nullable_v<Group<La>>);
  STATIC_REQUIRE(is_nullable_v<Group<Repetition<La, '*'>>>);

  // Sequence: all children nullable. Alternatives: any child nullable.
  STATIC_REQUIRE(!is_nullable_v<Sequence<La, Repetition<Lb, '*'>>>);
  STATIC_REQUIRE(
      is_nullable_v<Sequence<Repetition<La, '?'>, Repetition<Lb, '*'>>>);
  STATIC_REQUIRE(is_nullable_v<Alternatives<La, Repetition<Lb, '*'>>>);
  STATIC_REQUIRE(!is_nullable_v<Alternatives<La, Lb>>);
}

TEST_CASE("CT unit: star_normal_t", "[ct_unit][matcher]") {
  using match::is_nullable_v;
  using match::star_normal_t;
  using AStar = Repetition<La, '*'>;
  using BOpt = Repetition<Lb, '?'>;

  // Already non-nullable: unchanged.
  STATIC_REQUIRE(same<star_normal_t<La>, La>);
  STATIC_REQUIRE(same<star_normal_t<Sequence<La, BOpt>>, Sequence<La, BOpt>>);
  // Repetitions and groups are stripped down to their (normalized) body.
  STATIC_REQUIRE(same<star_normal_t<AStar>, La>);
  STATIC_REQUIRE(same<star_normal_t<Repetition<La, '+'>>, La>);
  STATIC_REQUIRE(same<star_normal_t<Group<AStar>>, La>);
  // A nullable sequence becomes the alternatives of its normalized parts.
  STATIC_REQUIRE(
      same<star_normal_t<Sequence<AStar, BOpt>>, Alternatives<La, Lb>>);
  // Alternatives normalize each branch.
  STATIC_REQUIRE(
      same<star_normal_t<Alternatives<AStar, Lc>>, Alternatives<La, Lc>>);

  // The defining property: the result never matches empty.
  STATIC_REQUIRE(!is_nullable_v<star_normal_t<AStar>>);
  STATIC_REQUIRE(!is_nullable_v<star_normal_t<Sequence<AStar, BOpt>>>);
  STATIC_REQUIRE(!is_nullable_v<star_normal_t<Alternatives<AStar, BOpt>>>);
  STATIC_REQUIRE(!is_nullable_v<
                 star_normal_t<Sequence<Repetition<Group<BOpt>, '+'>, AStar>>>);
}

TEST_CASE("CT unit: Matcher with explicit continuations",
          "[ct_unit][matcher]") {
  // The pack is the node plus everything that must match after it.
  STATIC_REQUIRE(Matcher<>::match(""));
  STATIC_REQUIRE(!Matcher<>::match("a"));
  STATIC_REQUIRE(Matcher<La>::match("a"));
  STATIC_REQUIRE(Matcher<La, Lb>::match("ab"));
  STATIC_REQUIRE(!Matcher<La, Lb>::match("a"));
  // Repetition gives back characters to the continuation.
  STATIC_REQUIRE(Matcher<Repetition<La, '*'>, La>::match("aaa"));
  STATIC_REQUIRE(!Matcher<Repetition<La, '+'>, La>::match("a"));
  // Alternatives backtrack into the continuation.
  STATIC_REQUIRE(Matcher<Alternatives<La, Sequence<La, Lb>>, Lb>::match("ab"));
  STATIC_REQUIRE(Matcher<Alternatives<La, Sequence<La, Lb>>, Lb>::match("abb"));
  // Empty-matching loop body terminates.
  STATIC_REQUIRE(Matcher<Repetition<Repetition<La, '?'>, '*'>>::match("aa"));
  STATIC_REQUIRE(Matcher<Repetition<Repetition<La, '?'>, '*'>>::match(""));
}

} // namespace
} // namespace toyctre::compile_time::detail
