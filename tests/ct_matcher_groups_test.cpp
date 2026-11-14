#include "ct_matcher_helpers.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string_view>

// Compile-time matcher tests, continued from ct_matcher_test.cpp (see there for
// the matching semantics): groups, precedence, escapes, composite expressions
// and unusual bytes. Split out so the files compile in parallel.

namespace toyctre {
namespace {

using namespace std::string_view_literals;
using namespace ct_test;

} // namespace

// ---------------------------------------------------------------------------
// Groups
// ---------------------------------------------------------------------------

TEST_CASE("CT: Plain groups", "[ct_matcher][group]") {
  checkMatches<"(a)">({"a"}, {"", "(a)", "aa"});
  checkMatches<"((a))">({"a"}, {"", "aa"});
  checkMatches<"(ab)">({"ab"}, {"a", "abab"});
  checkMatches<"(a)(b)(c)">({"abc"}, {"ab", "abcc"});
  checkMatches<"a(b)c">({"abc"}, {"ac", "abbc"});
}

TEST_CASE("CT: Repeated groups", "[ct_matcher][group][rep]") {
  checkMatches<"(ab)*">({"", "ab", "abab", "ababab"},
                        {"a", "b", "aba", "abb", "ba"});
  checkMatches<"(ab)+">({"ab", "abab"}, {"", "a", "aba"});
  checkMatches<"(ab)?">({"", "ab"}, {"a", "abab"});
  checkMatches<"(ab)*c">({"c", "abc", "ababc"}, {"", "ab", "abac"});
  checkMatches<"x(ab)+y">({"xaby", "xababy"}, {"xy", "xay", "xabay"});
}

TEST_CASE("CT: Groups containing alternation", "[ct_matcher][group][alt]") {
  checkMatches<"(a|b)">({"a", "b"}, {"", "ab"});
  checkMatches<"(a|b)c">({"ac", "bc"}, {"c", "abc", "a"});
  checkMatches<"a(b|c)d">({"abd", "acd"}, {"ad", "abcd", "abd "});
  checkMatches<"(a|b)(c|d)">({"ac", "ad", "bc", "bd"}, {"ab", "cd", "a"});
  checkMatches<"(a|ab)(c|bcd)">({"ac", "abcd", "abc", "abbcd"},
                                {"ab", "abd", "acd"});
  checkMatches<"((a|b)c|d)e">({"ace", "bce", "de"}, {"ce", "e", "abce"});
}

// An alternative inside a group must be retried when what follows the group
// fails.
TEST_CASE("CT: Backtracking into groups", "[ct_matcher][group][alt]") {
  checkMatches<"(a|ab)c">({"ac", "abc"}, {"ab", "abbc"});
  checkMatches<"(ab|a)bc">({"abc", "abbc"}, {"ac", "bc"});
  checkMatches<"(a|ab)(b|bc)d">({"abd", "abbd", "abcd", "abbcd"},
                                {"ad", "abbbd"});
  checkMatches<"((a|ab)b)c">({"abc", "abbc"}, {"ac"});
}

TEST_CASE("CT: Alternation inside repetition",
          "[ct_matcher][group][alt][rep]") {
  checkMatches<"(a|b)*">({"", "a", "b", "abba", "bbbb"}, {"c", "abc"});
  checkMatches<"(a|b)+">({"a", "ab", "ba"}, {"", "c"});
  checkMatches<"(a|b)*abb">({"abb", "aabb", "babb", "ababb"},
                            {"", "ab", "abba", "bb"});
  checkMatches<"(ab|c)*">({"", "ab", "c", "abc", "cab", "ccab"},
                          {"a", "b", "ac", "abab a"});
  checkMatches<"(a|bc)+d">({"ad", "bcd", "abcad"}, {"d", "bd", "abd"});
  checkMatches<"(a|ab)*c">({"c", "ac", "abc", "aabac", "ababc"},
                           {"bc", "abbc"});
}

TEST_CASE("CT: Nested repetition", "[ct_matcher][group][rep]") {
  checkMatches<"(a+)*">({"", "a", "aaaa"}, {"b"});
  checkMatches<"(a+)+">({"a", "aaaa"}, {"", "b"});
  checkMatches<"((ab)*c)*">({"", "c", "abc", "cc", "ababcabc"},
                            {"ab", "abcab"});
  checkMatches<"(a(b(c)*)+)?">({"", "ab", "abc", "abcc", "abcbc", "abbb"},
                               {"a", "ac", "abab"});
}

// The repeated unit can itself match the empty string.
TEST_CASE("CT: Repetition of a unit that can match empty",
          "[ct_matcher][group][rep]") {
  checkMatches<"(a*)*">({"", "a", "aaaa"}, {"b", "ab"});
  checkMatches<"(a*)+">({"", "a", "aaaa"}, {"b"});
  checkMatches<"(a?)*">({"", "a", "aaaa"}, {"b"});
  checkMatches<"(a?)+">({"", "a", "aaaa"}, {"b"});
  checkMatches<"(a*b*)*">({"", "a", "b", "abba", "bbaa"}, {"c", "abc"});
  checkMatches<"(a|b*)*">({"", "a", "b", "abbab"}, {"c"});
  checkMatches<"(a*)*b">({"b", "ab", "aaab"}, {"", "a", "ba"});
}

// ---------------------------------------------------------------------------
// Precedence between operators
// ---------------------------------------------------------------------------

TEST_CASE("CT: Alternation has lowest precedence", "[ct_matcher][alt]") {
  checkMatches<"ab|cd*">({"ab", "c", "cddd"}, {"abd", "abcd", "cdcd"});
  checkMatches<"a|b*">({"", "a", "bbb"}, {"ab", "ba", "aa"});
  checkMatches<"a*|b">({"", "aaa", "b"}, {"ab", "bb"});
  checkMatches<"ab|c">({"ab", "c"}, {"ac", "abc"});
}

// ---------------------------------------------------------------------------
// Escapes
// ---------------------------------------------------------------------------

TEST_CASE("CT: Escaped metacharacters match literally",
          "[ct_matcher][escape]") {
  checkMatches<R"re(\*)re">({"*"}, {"", "a", "**"});
  checkMatches<R"re(\+)re">({"+"}, {"", "++"});
  checkMatches<R"re(\?)re">({"?"}, {""});
  checkMatches<R"re(\()re">({"("}, {"", ")"});
  checkMatches<R"re(\))re">({")"}, {"", "("});
  checkMatches<R"re(\|)re">({"|"}, {"", "||"});
  checkMatches<R"re(\\)re">({"\\"}, {"", "\\\\"});
}

TEST_CASE("CT: Escaped ordinary characters", "[ct_matcher][escape]") {
  checkMatches<R"re(\a)re">({"a"}, {"\\a", ""});
  checkMatches<R"re(\.)re">({"."}, {"a"});
  checkMatches<R"re(\n)re">({"n"}, {"\n", "\\n"});
}

TEST_CASE("CT: Escapes in context", "[ct_matcher][escape]") {
  checkMatches<R"re(a\+)re">({"a+"}, {"a", "aa", "aaa"});
  checkMatches<R"re(\(a\))re">({"(a)"}, {"a", "()"});
  checkMatches<R"re(a\|b)re">({"a|b"}, {"a", "b"});
  checkMatches<R"re(\**)re">({"", "*", "***"}, {"a", "**a"});
  checkMatches<R"re((\(|\))*)re">({"", "(", ")()(", "(("}, {"a", "(a)"});
  checkMatches<R"re(\\\\)re">({"\\\\"}, {"\\"});
  checkMatches<R"re(a\*b)re">({"a*b"}, {"ab", "aab"});
}

// ---------------------------------------------------------------------------
// Composite / classic expressions
// ---------------------------------------------------------------------------

TEST_CASE("CT: Binary multiples of three", "[ct_matcher][composite]") {
  checkMatches<"(0|1(01*0)*1)*">(
      {"", "0", "11", "110", "1001", "1100", "1111", "10010"},
      {"1", "10", "100", "101", "111", "1000", "1010"});
}

TEST_CASE("CT: Composite expressions", "[ct_matcher][composite]") {
  checkMatches<"x*(y|z)+w?">({"y", "xxz", "yzyw", "xyw"},
                             {"", "x", "w", "xw", "yww"});
  checkMatches<"((ab)*|c+)?d">({"d", "abd", "ababd", "cd", "cccd"},
                               {"", "abcd", "ad", "cabd"});
  checkMatches<"ab|c(d|e)*f?">({"ab", "c", "cf", "cdedf", "cee"},
                               {"abf", "cff", "df"});
}

// ---------------------------------------------------------------------------
// Non-ASCII and unusual bytes
// ---------------------------------------------------------------------------

TEST_CASE("CT: Control and boundary bytes", "[ct_matcher][bytes]") {
  checkMatches<"a\tb">({"a\tb"}, {"a b"});
  checkMatches<"\x01">({"\x01"}, {"", "\x02"});
  checkMatches<"\x7e">({"~"}, {"", "\x7f"});
  checkMatches<"a\x7f"
               "b">({"a\x7f"
                     "b"},
                    {"ab", "a"});
  checkMatches<"a">({}, {"\x7f", "a\0"sv, "\0a"sv});
  checkMatches<"a\0b">({"a\0b"sv}, {"ab", "a", "a\0"sv});
}

TEST_CASE("CT: High-bit bytes", "[ct_matcher][bytes]") {
  checkMatches<"\x80">({"\x80"}, {"", "\x81"});
  checkMatches<"\xff">({"\xff"}, {"", "\xfe"});
  checkMatches<"a">({}, {"\xff", "\x80"});
  checkMatches<"(a|b)*">({"ab"}, {"a\xff"});
  checkMatches<"é">({"é"}, {"e", ""});
  checkMatches<"(é)+">({"é", "ééé"}, {"", "e"});
}

} // namespace toyctre
