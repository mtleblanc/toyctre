#include "ct_matcher_helpers.hpp"
#include "toyctre/compile_time.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <initializer_list>
#include <string_view>
#include <tuple>

// Capture groups in the compile-time matcher.
//
// match<re>(text) returns a MatchResult whose groups[0] is the whole match and
// groups[i] is the i-th capture group, numbered by its opening paren. Spans are
// [start, end) offsets into text. When a group matches more than once (inside
// a repetition) the last iteration is the one reported.
//
// What an unset group (one that took no part in the match) reports is not
// pinned down here. The tests for it only check that a span from a failed
// branch doesn't leak through.

namespace re {
namespace {

using namespace ct_test;
using ct::RegexString;

struct Span {
  std::size_t start;
  std::size_t end;
};

// In an expected list, `unset` marks a group whose span isn't checked because
// it took no part in the match.
struct Unset {};
constexpr auto unset = Unset{};

struct Expected {
  bool checked;
  Span span;
  constexpr Expected(std::size_t start, std::size_t end)
      : checked{true}, span{start, end} {}
  constexpr Expected(Unset) : checked{false}, span{} {}
};

// Number of entries in the result's groups array, including group 0.
template <RegexString re>
constexpr auto groupCount =
    std::tuple_size_v<decltype(ct::match<re>("").groups)>;

// Check that `text` matches, that group 0 is the whole text, and that groups
// 1..n have the expected spans. `expected` must list every group; use
// `unset` for a group that took no part in the match.
template <RegexString re>
void checkCaptures(std::string_view text,
                   std::initializer_list<Expected> expected) {
  auto pattern = patternOf<re>();
  CAPTURE(pattern, text);
  auto result = ct::match<re>(text);
  REQUIRE(result.success);
  REQUIRE(groupCount<re> == expected.size() + 1);
  CHECK(result.groups[0].start == 0);
  CHECK(result.groups[0].end == text.size());
  auto i = std::size_t{1};
  for (auto [checked, span] : expected) {
    CAPTURE(i);
    if (checked) {
      CHECK(result.groups[i].start == span.start);
      CHECK(result.groups[i].end == span.end);
    }
    ++i;
  }
}

// Check that `text` matches and that group `i` does not report `stale`.
template <RegexString re>
void checkNotCaptured(std::string_view text, std::size_t i, Span stale) {
  auto pattern = patternOf<re>();
  CAPTURE(pattern, text, i);
  auto result = ct::match<re>(text);
  REQUIRE(result.success);
  CHECK_FALSE((result.groups[i].start == stale.start &&
               result.groups[i].end == stale.end));
}

} // namespace

// ---------------------------------------------------------------------------
// Result shape
// ---------------------------------------------------------------------------

TEST_CASE("CT capture: one entry per group plus the whole match",
          "[ct_matcher][capture]") {
  STATIC_CHECK(groupCount<"abc"> == 1);
  STATIC_CHECK(groupCount<"(a)"> == 2);
  STATIC_CHECK(groupCount<"(a)(b)"> == 3);
  STATIC_CHECK(groupCount<"((a))"> == 3);
  STATIC_CHECK(groupCount<"(a|(b))*"> == 3);
  // Escaped parens aren't groups.
  STATIC_CHECK(groupCount<R"re(\(a\))re"> == 1);
  STATIC_CHECK(groupCount<R"re(\((a)\))re"> == 2);
  // An escaped backslash doesn't escape the paren after it.
  STATIC_CHECK(groupCount<R"re(\\(a))re"> == 2);
}

TEST_CASE("CT capture: group 0 is the whole match", "[ct_matcher][capture]") {
  checkCaptures<"abc">("abc", {});
  checkCaptures<"a*">("", {});
  checkCaptures<"a*">("aaa", {});
  checkCaptures<"a|bc">("bc", {});
}

TEST_CASE("CT capture: failed match", "[ct_matcher][capture]") {
  CHECK_FALSE(ct::match<"a(b)c">("abd").success);
  CHECK_FALSE(ct::match<"a(b)c">("").success);
  CHECK_FALSE(ct::match<"(a)">("aa").success);
}

TEST_CASE("CT capture: usable in constant expressions",
          "[ct_matcher][capture][constexpr]") {
  constexpr auto result = ct::match<"a(bc)d">("abcd");
  STATIC_CHECK(result.success);
  STATIC_CHECK(result.groups[1].start == 1);
  STATIC_CHECK(result.groups[1].end == 3);
}

// ---------------------------------------------------------------------------
// Spans
// ---------------------------------------------------------------------------

TEST_CASE("CT capture: simple groups", "[ct_matcher][capture]") {
  checkCaptures<"(a)">("a", {{0, 1}});
  checkCaptures<"a(bc)">("abc", {{1, 3}});
  checkCaptures<"(ab)c">("abc", {{0, 2}});
  checkCaptures<"a(b)c">("abc", {{1, 2}});
  checkCaptures<"(a)(b)(c)">("abc", {{0, 1}, {1, 2}, {2, 3}});
  checkCaptures<"(abc)">("abc", {{0, 3}});
}

TEST_CASE("CT capture: numbered by opening paren", "[ct_matcher][capture]") {
  checkCaptures<"((a))">("a", {{0, 1}, {0, 1}});
  checkCaptures<"((a)(b))">("ab", {{0, 2}, {0, 1}, {1, 2}});
  checkCaptures<"(a(b(c)))">("abc", {{0, 3}, {1, 3}, {2, 3}});
  checkCaptures<"(a)((b)c)">("abc", {{0, 1}, {1, 3}, {1, 2}});
  checkCaptures<"((a)b)(c)">("abc", {{0, 2}, {0, 1}, {2, 3}});
}

TEST_CASE("CT capture: escaped parens", "[ct_matcher][capture][escape]") {
  checkCaptures<R"re(\((a)\))re">("(a)", {{1, 2}});
  checkCaptures<R"re((\())re">("(", {{0, 1}});
  checkCaptures<R"re(\\(a))re">("\\a", {{1, 2}});
}

TEST_CASE("CT capture: groups matching the empty string",
          "[ct_matcher][capture]") {
  checkCaptures<"(a*)">("", {{0, 0}});
  checkCaptures<"(a*)b">("b", {{0, 0}});
  checkCaptures<"a(b*)">("a", {{1, 1}});
  checkCaptures<"a(b?)c">("ac", {{1, 1}});
  checkCaptures<"(a*)(b*)">("", {{0, 0}, {0, 0}});
  checkCaptures<"(a*)(b*)">("bb", {{0, 0}, {0, 2}});
}

TEST_CASE("CT capture: groups in alternation", "[ct_matcher][capture][alt]") {
  checkCaptures<"(a|bc)d">("ad", {{0, 1}});
  checkCaptures<"(a|bc)d">("bcd", {{0, 2}});
  checkCaptures<"x(a|b)y">("xby", {{1, 2}});
  checkCaptures<"(a)|b">("a", {{0, 1}});
  checkCaptures<"a|(b)">("b", {{0, 1}});
  checkCaptures<"(ab)|(cd)">("ab", {{0, 2}, unset});
  checkCaptures<"(ab)|(cd)">("cd", {unset, {0, 2}});
  checkCaptures<"(a)|(b)">("b", {unset, {0, 1}});
}

// ---------------------------------------------------------------------------
// Backtracking
// ---------------------------------------------------------------------------

TEST_CASE("CT capture: spans after backtracking",
          "[ct_matcher][capture][backtrack]") {
  // The group gives back characters to what follows it.
  checkCaptures<"(a*)a">("aaa", {{0, 2}});
  checkCaptures<"(a+)(a+)">("aaa", {{0, 2}, {2, 3}});
  checkCaptures<"(a*)(a*)">("aa", {{0, 2}, {2, 2}});
  checkCaptures<"(a?)a">("a", {{0, 0}});
  // The first alternative fails later, and the second one is reported.
  checkCaptures<"(a|ab)c">("abc", {{0, 2}});
  checkCaptures<"(a|ab)(c|bcd)">("abcd", {{0, 1}, {1, 4}});
  checkCaptures<"(ab|a)(bc)?">("abc", {{0, 1}, {1, 3}});
  checkCaptures<"((a)|ab)c">("abc", {{0, 2}, unset});
}

TEST_CASE("CT capture: no spans from failed branches",
          "[ct_matcher][capture][backtrack]") {
  // In each case the group matches first, the rest of that branch fails, and
  // the successful path never enters the group.
  checkNotCaptured<"(a)b|ac">("ac", 1, {0, 1});
  checkNotCaptured<"(a)?ab">("ab", 1, {0, 1});
  checkNotCaptured<"((a)b|a)c">("ac", 2, {0, 1});
  checkNotCaptured<"(a)(b)c|abd">("abd", 1, {0, 1});
  checkNotCaptured<"(a)(b)c|abd">("abd", 2, {1, 2});
  checkNotCaptured<"((a)|ab)c">("abc", 2, {0, 1});
  // Same, with a different group capturing the same text on the successful
  // path.
  checkCaptures<"((a)b|(a)c)">("ac", {{0, 2}, unset, {0, 1}});
  checkNotCaptured<"((a)b|(a)c)">("ac", 2, {0, 1});
}

// ---------------------------------------------------------------------------
// Repetition: the last iteration is reported
// ---------------------------------------------------------------------------

TEST_CASE("CT capture: optional groups", "[ct_matcher][capture][rep]") {
  checkCaptures<"(a)?">("a", {{0, 1}});
  checkCaptures<"(ab)?c">("abc", {{0, 2}});
  checkCaptures<"x(a)?y">("xay", {{1, 2}});
}

TEST_CASE("CT capture: starred groups", "[ct_matcher][capture][rep]") {
  checkCaptures<"(a)*">("a", {{0, 1}});
  checkCaptures<"(a)*">("aa", {{1, 2}});
  checkCaptures<"(ab)*">("abab", {{2, 4}});
  checkCaptures<"(a|b)*">("ab", {{1, 2}});
  checkCaptures<"(a|bc)*">("abca", {{3, 4}});
  checkCaptures<"x(a)*y">("xaay", {{2, 3}});
  checkCaptures<"(a)*b">("aab", {{1, 2}});
}

TEST_CASE("CT capture: plus groups", "[ct_matcher][capture][rep]") {
  checkCaptures<"(a)+">("a", {{0, 1}});
  checkCaptures<"(a)+">("aaa", {{2, 3}});
  checkCaptures<"(ab)+c">("ababc", {{2, 4}});
  checkCaptures<"(a|b)+">("ba", {{1, 2}});
}

TEST_CASE("CT capture: nested groups in a repetition",
          "[ct_matcher][capture][rep]") {
  checkCaptures<"((a)b)*">("abab", {{2, 4}, {2, 3}});
  checkCaptures<"((a)*b)*">("abaab", {{2, 5}, {3, 4}});
  checkCaptures<"((ab)+)*">("abab", {{0, 4}, {2, 4}});
}

TEST_CASE("CT capture: repetition backtracks into the continuation",
          "[ct_matcher][capture][rep][backtrack]") {
  // The last iteration is the last one kept, not the last one tried.
  checkCaptures<"(a)*a">("aaa", {{1, 2}});
  checkCaptures<"(ab|a)*b">("abab", {{2, 3}});
  checkCaptures<"(a|ab)*c">("ababc", {{2, 4}});
}

} // namespace re
