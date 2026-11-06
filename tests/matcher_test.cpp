#include "toyctre/lib.hpp"
#include "toyctre/matcher.hpp"

#include <catch2/catch_test_macros.hpp>

#include <initializer_list>
#include <string>
#include <string_view>

// Match/no-match tests for the Thompson NFA matcher.
//
// Semantics assumed:
//   - match() is a full match: the whole text must be matched, not a substring
//   - '.' is an ordinary literal (no wildcard yet)
//   - an escaped character is the literal character
//
// Each TEST_CASE is a separate ctest entry, so a crash in one category
// doesn't hide results from the others.

namespace toyctre {
namespace {

using namespace std::string_view_literals;

void checkMatches(std::string_view re,
                  std::initializer_list<std::string_view> matching,
                  std::initializer_list<std::string_view> nonMatching) {
  CAPTURE(re);
  auto parsed = parseRegex(re);
  REQUIRE(parsed.accepted);
  for (auto text : matching) {
    CAPTURE(text);
    CHECK(match(parsed.regex, text));
  }
  for (auto text : nonMatching) {
    CAPTURE(text);
    CHECK_FALSE(match(parsed.regex, text));
  }
}

std::string repeat(std::string_view s, int n) {
  auto ret = std::string{};
  for (int i = 0; i < n; ++i) {
    ret += s;
  }
  return ret;
}

} // namespace

// ---------------------------------------------------------------------------
// Literals
// ---------------------------------------------------------------------------

TEST_CASE("Single literal", "[matcher][literal]") {
  checkMatches("a", {"a"}, {"", "b", "A", "aa", " a", "a "});
  checkMatches("Z", {"Z"}, {"z", ""});
  checkMatches("0", {"0"}, {"1", "00"});
  checkMatches(" ", {" "}, {"", "  "});
}

TEST_CASE("Literal sequence", "[matcher][literal]") {
  checkMatches("abc", {"abc"},
               {"", "a", "ab", "abcd", "xabc", "abd", "acb", "ABC"});
  checkMatches("hello world", {"hello world"}, {"hello", "helloworld"});
  checkMatches("aaa", {"aaa"}, {"aa", "aaaa"});
}

TEST_CASE("Dot is a literal", "[matcher][literal]") {
  checkMatches("a.c", {"a.c"}, {"abc", "ac", "a..c"});
  checkMatches("...", {"..."}, {"abc", ".."});
}

// ---------------------------------------------------------------------------
// Alternation
// ---------------------------------------------------------------------------

TEST_CASE("Alternation of literals", "[matcher][alt]") {
  checkMatches("a|b", {"a", "b"}, {"", "c", "ab", "ba", "aa"});
  checkMatches("a|b|c", {"a", "b", "c"}, {"", "d", "abc"});
  checkMatches("ab|cd", {"ab", "cd"}, {"", "a", "ad", "cb", "abcd", "b|c"});
  checkMatches("abc|d", {"abc", "d"}, {"abd", "ab", "dd"});
}

TEST_CASE("Alternatives sharing prefixes", "[matcher][alt]") {
  checkMatches("a|ab", {"a", "ab"}, {"b", "aab", "abb"});
  checkMatches("ab|a", {"a", "ab"}, {"b", "aab"});
  checkMatches("abc|ab|a", {"a", "ab", "abc"}, {"", "abcd", "ac", "bc"});
  checkMatches("ab|ac", {"ab", "ac"}, {"a", "abc", "bc"});
}

TEST_CASE("Many alternatives", "[matcher][alt]") {
  checkMatches("a|b|c|d|e|f|g|h", {"a", "d", "h"}, {"", "i", "ab"});
}

// ---------------------------------------------------------------------------
// Repetition
// ---------------------------------------------------------------------------

TEST_CASE("Star", "[matcher][rep]") {
  checkMatches("a*", {"", "a", "aa", "aaaaaaaa"}, {"b", "ab", "ba", "aab"});
  checkMatches("ab*", {"a", "ab", "abbbb"}, {"", "b", "aab", "aba"});
  checkMatches("a*b", {"b", "ab", "aaab"}, {"", "a", "abb", "ba"});
  checkMatches("a*b*", {"", "a", "b", "aab", "abbb", "aaa", "bbb"},
               {"ba", "aba", "c"});
  checkMatches("xa*y", {"xy", "xay", "xaaay"}, {"x", "y", "xby", "xayy"});
}

TEST_CASE("Plus", "[matcher][rep]") {
  checkMatches("a+", {"a", "aa", "aaaaaa"}, {"", "b", "ab"});
  checkMatches("ab+c", {"abc", "abbbc"}, {"ac", "abcc", "abbb"});
  checkMatches("a+b+", {"ab", "aabbb"}, {"", "a", "b", "aba"});
}

TEST_CASE("Question", "[matcher][rep]") {
  checkMatches("a?", {"", "a"}, {"aa", "b"});
  checkMatches("ab?c", {"ac", "abc"}, {"abbc", "a", "bc"});
  checkMatches("a?b?c?", {"", "a", "b", "c", "ab", "ac", "bc", "abc"},
               {"ba", "cb", "aa", "abcc"});
  checkMatches("a?a?a?aaa", {"aaa", "aaaa", "aaaaa", "aaaaaa"},
               {"aa", "aaaaaaa"});
}

TEST_CASE("Repetition precedence binds to single unit", "[matcher][rep]") {
  checkMatches("ab*", {"abbb"}, {"abab"});
  checkMatches("ab+", {"abb"}, {"abab"});
  checkMatches("ab?", {"a", "ab"}, {"", "b"});
}

// ---------------------------------------------------------------------------
// Groups
// ---------------------------------------------------------------------------

TEST_CASE("Plain groups", "[matcher][group]") {
  checkMatches("(a)", {"a"}, {"", "(a)", "aa"});
  checkMatches("((a))", {"a"}, {"", "aa"});
  checkMatches("(ab)", {"ab"}, {"a", "abab"});
  checkMatches("(a)(b)(c)", {"abc"}, {"ab", "abcc"});
  checkMatches("a(b)c", {"abc"}, {"ac", "abbc"});
}

TEST_CASE("Repeated groups", "[matcher][group][rep]") {
  checkMatches("(ab)*", {"", "ab", "abab", "ababab"},
               {"a", "b", "aba", "abb", "ba"});
  checkMatches("(ab)+", {"ab", "abab"}, {"", "a", "aba"});
  checkMatches("(ab)?", {"", "ab"}, {"a", "abab"});
  checkMatches("(ab)*c", {"c", "abc", "ababc"}, {"", "ab", "abac"});
  checkMatches("x(ab)+y", {"xaby", "xababy"}, {"xy", "xay", "xabay"});
}

TEST_CASE("Groups containing alternation", "[matcher][group][alt]") {
  checkMatches("(a|b)", {"a", "b"}, {"", "ab"});
  checkMatches("(a|b)c", {"ac", "bc"}, {"c", "abc", "a"});
  checkMatches("a(b|c)d", {"abd", "acd"}, {"ad", "abcd", "abd "});
  checkMatches("(a|b)(c|d)", {"ac", "ad", "bc", "bd"}, {"ab", "cd", "a"});
  checkMatches("(a|ab)(c|bcd)", {"ac", "abcd", "abc", "abbcd"},
               {"ab", "abd", "acd"});
  checkMatches("((a|b)c|d)e", {"ace", "bce", "de"}, {"ce", "e", "abce"});
}

TEST_CASE("Alternation inside repetition", "[matcher][group][alt][rep]") {
  checkMatches("(a|b)*", {"", "a", "b", "abba", "bbbb"}, {"c", "abc"});
  checkMatches("(a|b)+", {"a", "ab", "ba"}, {"", "c"});
  checkMatches("(a|b)*abb", {"abb", "aabb", "babb", "ababb"},
               {"", "ab", "abba", "bb"});
  checkMatches("(ab|c)*", {"", "ab", "c", "abc", "cab", "ccab"},
               {"a", "b", "ac", "abab a"});
  checkMatches("(a|bc)+d", {"ad", "bcd", "abcad"}, {"d", "bd", "abd"});
}

TEST_CASE("Nested repetition", "[matcher][group][rep]") {
  checkMatches("(a*)*", {"", "a", "aaaa"}, {"b", "ab"});
  checkMatches("(a*)+", {"", "a", "aaaa"}, {"b"});
  checkMatches("(a+)*", {"", "a", "aaaa"}, {"b"});
  checkMatches("(a?)*", {"", "a", "aaaa"}, {"b"});
  checkMatches("(a*b*)*", {"", "a", "b", "abba", "bbaa"}, {"c", "abc"});
  checkMatches("(a|b*)*", {"", "a", "b", "abbab"}, {"c"});
  checkMatches("((ab)*c)*", {"", "c", "abc", "cc", "ababcabc"},
               {"ab", "abcab"});
  checkMatches("(a(b(c)*)+)?", {"", "ab", "abc", "abcc", "abcbc", "abbb"},
               {"a", "ac", "abab"});
}

// ---------------------------------------------------------------------------
// Precedence between operators
// ---------------------------------------------------------------------------

TEST_CASE("Alternation has lowest precedence", "[matcher][alt]") {
  checkMatches("ab|cd*", {"ab", "c", "cddd"}, {"abd", "abcd", "cdcd"});
  checkMatches("a|b*", {"", "a", "bbb"}, {"ab", "ba", "aa"});
  checkMatches("a*|b", {"", "aaa", "b"}, {"ab", "bb"});
  checkMatches("ab|c", {"ab", "c"}, {"ac", "abc"});
}

// ---------------------------------------------------------------------------
// Escapes
// ---------------------------------------------------------------------------

TEST_CASE("Escaped metacharacters match literally", "[matcher][escape]") {
  checkMatches(R"re(\*)re", {"*"}, {"", "a", "**"});
  checkMatches(R"re(\+)re", {"+"}, {"", "++"});
  checkMatches(R"re(\?)re", {"?"}, {""});
  checkMatches(R"re(\()re", {"("}, {"", ")"});
  checkMatches(R"re(\))re", {")"}, {"", "("});
  checkMatches(R"re(\|)re", {"|"}, {"", "||"});
  checkMatches(R"re(\\)re", {"\\"}, {"", "\\\\"});
}

TEST_CASE("Escaped ordinary characters", "[matcher][escape]") {
  checkMatches(R"re(\a)re", {"a"}, {"\\a", ""});
  checkMatches(R"re(\.)re", {"."}, {"a"});
  checkMatches(R"re(\n)re", {"n"}, {"\n", "\\n"});
}

TEST_CASE("Escapes in context", "[matcher][escape]") {
  checkMatches(R"re(a\+)re", {"a+"}, {"a", "aa", "aaa"});
  checkMatches(R"re(\(a\))re", {"(a)"}, {"a", "()"});
  checkMatches(R"re(a\|b)re", {"a|b"}, {"a", "b"});
  checkMatches(R"re(\**)re", {"", "*", "***"}, {"a", "**a"});
  checkMatches(R"re((\(|\))*)re", {"", "(", ")()(", "(("}, {"a", "(a)"});
  checkMatches(R"re(\\\\)re", {"\\\\"}, {"\\"});
  checkMatches(R"re(a\*b)re", {"a*b"}, {"ab", "aab"});
}

// ---------------------------------------------------------------------------
// Composite / classic expressions
// ---------------------------------------------------------------------------

TEST_CASE("Binary multiples of three", "[matcher][composite]") {
  checkMatches("(0|1(01*0)*1)*",
               {"", "0", "11", "110", "1001", "1100", "1111", "10010"},
               {"1", "10", "100", "101", "111", "1000", "1010"});
}

TEST_CASE("Composite expressions", "[matcher][composite]") {
  checkMatches("x*(y|z)+w?", {"y", "xxz", "yzyw", "xyw"},
               {"", "x", "w", "xw", "yww"});
  checkMatches("((ab)*|c+)?d", {"d", "abd", "ababd", "cd", "cccd"},
               {"", "abcd", "ad", "cabd"});
  checkMatches("ab|c(d|e)*f?", {"ab", "c", "cf", "cdedf", "cee"},
               {"abf", "cff", "df"});
}

// ---------------------------------------------------------------------------
// Non-ASCII and unusual bytes
// ---------------------------------------------------------------------------

TEST_CASE("Control and boundary bytes", "[matcher][bytes]") {
  checkMatches("a\tb", {"a\tb"}, {"a b"});
  checkMatches("\x01", {"\x01"}, {"", "\x02"});
  checkMatches("\x7e", {"~"}, {"", "\x7f"});
  checkMatches("a\x7f"
               "b",
               {"a\x7f"
                "b"},
               {"ab", "a"});
  checkMatches("a"sv, {}, {"\x7f"});
  checkMatches("a\0b"sv, {"a\0b"sv}, {"ab", "a"});
}

TEST_CASE("High-bit bytes", "[matcher][bytes]") {
  checkMatches("\x80", {"\x80"}, {"", "\x81"});
  checkMatches("\xff", {"\xff"}, {"", "\xfe"});
  checkMatches("a", {}, {"\xff", "\x80"});
  checkMatches("(a|b)*", {"ab"}, {"a\xff"});
  checkMatches("é", {"é"}, {"e", ""});
  checkMatches("(é)+", {"é", "ééé"}, {"", "e"});
}

// ---------------------------------------------------------------------------
// Size / pathological inputs
// ---------------------------------------------------------------------------

TEST_CASE("Long matcher inputs", "[matcher][stress]") {
  SECTION("long text against star") {
    auto re = parseRegex("a*");
    REQUIRE(re.accepted);
    CHECK(match(re.regex, std::string(10000, 'a')));
    CHECK_FALSE(match(re.regex, std::string(10000, 'a') + "b"));
  }

  SECTION("long text against alternation star") {
    auto re = parseRegex("(a|b)*c");
    REQUIRE(re.accepted);
    CHECK(match(re.regex, repeat("ab", 5000) + "c"));
    CHECK_FALSE(match(re.regex, repeat("ab", 5000)));
  }

  SECTION("many alternatives") {
    auto pat = std::string("a0");
    for (char c = '1'; c <= '9'; ++c) {
      pat += "|a";
      pat += c;
    }
    auto re = parseRegex(pat);
    REQUIRE(re.accepted);
    CHECK(match(re.regex, "a5"));
    CHECK_FALSE(match(re.regex, "a"));
  }
}

// Tagged [slow]: excluded from ctest in Debug builds (see
// tests/CMakeLists.txt).
TEST_CASE("Long literal pattern", "[matcher][stress][slow]") {
  auto pat = repeat("abc", 200);
  auto re = parseRegex(pat);
  REQUIRE(re.accepted);
  CHECK(match(re.regex, pat));
  CHECK_FALSE(match(re.regex, pat + "a"));
  CHECK_FALSE(match(re.regex, pat.substr(1)));
}

// These blow up exponentially in naive backtracking matchers, but are linear
// in the text for a Thompson NFA simulation.
TEST_CASE("Pathological for backtracking", "[matcher][stress]") {
  constexpr auto n = 30;

  SECTION("a?^n a^n against a^n") {
    auto re = parseRegex(repeat("a?", n) + repeat("a", n));
    REQUIRE(re.accepted);
    CHECK(match(re.regex, std::string(n, 'a')));
    CHECK(match(re.regex, std::string(2 * n, 'a')));
    CHECK_FALSE(match(re.regex, std::string(n - 1, 'a')));
    CHECK_FALSE(match(re.regex, std::string(2 * n + 1, 'a')));
  }

  SECTION("(a*)*b against a^n") {
    auto re = parseRegex("(a*)*b");
    REQUIRE(re.accepted);
    CHECK_FALSE(match(re.regex, std::string(n, 'a')));
    CHECK(match(re.regex, std::string(n, 'a') + "b"));
  }

  SECTION("(a|aa)*c against a^n") {
    auto re = parseRegex("(a|aa)*c");
    REQUIRE(re.accepted);
    CHECK_FALSE(match(re.regex, std::string(n, 'a')));
  }
}

} // namespace toyctre
