#include "ct_matcher_helpers.hpp"
#include "toyctre/compile_time.hpp"

#include <catch2/catch_test_macros.hpp>

// Match/no-match tests for the compile-time (continuation) matcher.
//
// Semantics are the same as the runtime matcher tests (matcher_test.cpp):
//   - compile_time::match<re>() is a full match
//   - '.' is an ordinary literal
//   - an escaped character is the literal character
//
// The regex is a template argument, so it is parsed at compile time. The
// match itself runs at runtime (CHECK) so that a failure reports the regex and
// text instead of breaking the build. "Usable in constant expressions" is the
// exception: it checks with STATIC_CHECK, so a failure there is a compile
// error.
//
// Each TEST_CASE is a separate ctest entry, so a crash in one category (e.g. a
// stack overflow) doesn't hide results from the others.
//
// The tests are split across files so they compile in parallel:
//   ct_matcher_test.cpp         literals, alternation, repetition
//   ct_matcher_groups_test.cpp  groups, precedence, escapes, composite, bytes
//   ct_matcher_stress_test.cpp  long/pathological patterns, differential tests

namespace re {
namespace {

using namespace ct_test;

} // namespace

// ---------------------------------------------------------------------------
// Constant evaluation
// ---------------------------------------------------------------------------

TEST_CASE("CT: Usable in constant expressions", "[ct_matcher][constexpr]") {
  STATIC_CHECK(ct::match<"abc">("abc").success);
  STATIC_CHECK_FALSE(ct::match<"abc">("abd").success);
  STATIC_CHECK(ct::match<"a|b">("b").success);
  STATIC_CHECK_FALSE(ct::match<"a|b">("ab").success);
  STATIC_CHECK(ct::match<"ab*c">("abbbc").success);
  STATIC_CHECK(ct::match<"a+">("aaa").success);
  STATIC_CHECK_FALSE(ct::match<"a+">("").success);
  STATIC_CHECK(ct::match<"ab?">("a").success);
  STATIC_CHECK(ct::match<R"re(a\*)re">("a*").success);
}

// ---------------------------------------------------------------------------
// Literals
// ---------------------------------------------------------------------------

TEST_CASE("CT: Single literal", "[ct_matcher][literal]") {
  checkMatches<"a">({"a"}, {"", "b", "A", "aa", " a", "a "});
  checkMatches<"Z">({"Z"}, {"z", ""});
  checkMatches<"0">({"0"}, {"1", "00"});
  checkMatches<" ">({" "}, {"", "  "});
}

TEST_CASE("CT: Literal sequence", "[ct_matcher][literal]") {
  checkMatches<"abc">({"abc"},
                      {"", "a", "ab", "abcd", "xabc", "abd", "acb", "ABC"});
  checkMatches<"hello world">({"hello world"}, {"hello", "helloworld"});
  checkMatches<"aaa">({"aaa"}, {"aa", "aaaa"});
}

TEST_CASE("CT: Dot is a literal", "[ct_matcher][literal]") {
  checkMatches<"a.c">({"a.c"}, {"abc", "ac", "a..c"});
  checkMatches<"...">({"..."}, {"abc", ".."});
}

// ---------------------------------------------------------------------------
// Alternation
// ---------------------------------------------------------------------------

TEST_CASE("CT: Alternation of literals", "[ct_matcher][alt]") {
  checkMatches<"a|b">({"a", "b"}, {"", "c", "ab", "ba", "aa"});
  checkMatches<"a|b|c">({"a", "b", "c"}, {"", "d", "abc"});
  checkMatches<"ab|cd">({"ab", "cd"}, {"", "a", "ad", "cb", "abcd", "b|c"});
  checkMatches<"abc|d">({"abc", "d"}, {"abd", "ab", "dd"});
}

// The first alternative to match a prefix must not be committed to.
TEST_CASE("CT: Alternatives sharing prefixes", "[ct_matcher][alt]") {
  checkMatches<"a|ab">({"a", "ab"}, {"b", "aab", "abb"});
  checkMatches<"ab|a">({"a", "ab"}, {"b", "aab"});
  checkMatches<"abc|ab|a">({"a", "ab", "abc"}, {"", "abcd", "ac", "bc"});
  checkMatches<"ab|ac">({"ab", "ac"}, {"a", "abc", "bc"});
}

TEST_CASE("CT: Many alternatives", "[ct_matcher][alt]") {
  checkMatches<"a|b|c|d|e|f|g|h">({"a", "d", "h"}, {"", "i", "ab"});
}

// ---------------------------------------------------------------------------
// Repetition
// ---------------------------------------------------------------------------

TEST_CASE("CT: Star", "[ct_matcher][rep]") {
  checkMatches<"a*">({"", "a", "aa", "aaaaaaaa"}, {"b", "ab", "ba", "aab"});
  checkMatches<"ab*">({"a", "ab", "abbbb"}, {"", "b", "aab", "aba"});
  checkMatches<"a*b">({"b", "ab", "aaab"}, {"", "a", "abb", "ba"});
  checkMatches<"a*b*">({"", "a", "b", "aab", "abbb", "aaa", "bbb"},
                       {"ba", "aba", "c"});
  checkMatches<"xa*y">({"xy", "xay", "xaaay"}, {"x", "y", "xby", "xayy"});
}

TEST_CASE("CT: Plus", "[ct_matcher][rep]") {
  checkMatches<"a+">({"a", "aa", "aaaaaa"}, {"", "b", "ab"});
  checkMatches<"ab+c">({"abc", "abbbc"}, {"ac", "abcc", "abbb"});
  checkMatches<"a+b+">({"ab", "aabbb"}, {"", "a", "b", "aba"});
}

TEST_CASE("CT: Question", "[ct_matcher][rep]") {
  checkMatches<"a?">({"", "a"}, {"aa", "b"});
  checkMatches<"ab?c">({"ac", "abc"}, {"abbc", "a", "bc"});
  checkMatches<"a?b?c?">({"", "a", "b", "c", "ab", "ac", "bc", "abc"},
                         {"ba", "cb", "aa", "abcc"});
  checkMatches<"a?a?a?aaa">({"aaa", "aaaa", "aaaaa", "aaaaaa"},
                            {"aa", "aaaaaaa"});
}

// Greedy repetition must give back characters when the rest fails.
TEST_CASE("CT: Repetition gives back characters", "[ct_matcher][rep]") {
  checkMatches<"a*a">({"a", "aa", "aaaa"}, {""});
  checkMatches<"a*aa">({"aa", "aaa"}, {"", "a"});
  checkMatches<"a+a">({"aa", "aaa"}, {"a"});
  checkMatches<"a?a">({"a", "aa"}, {"", "aaa"});
  checkMatches<"a*ab">({"ab", "aaab"}, {"b", "aa"});
}

TEST_CASE("CT: Repetition precedence binds to single unit",
          "[ct_matcher][rep]") {
  checkMatches<"ab*">({"abbb"}, {"abab"});
  checkMatches<"ab+">({"abb"}, {"abab"});
  checkMatches<"ab?">({"a", "ab"}, {"", "b"});
}

} // namespace re
