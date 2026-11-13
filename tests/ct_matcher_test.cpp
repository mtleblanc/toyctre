#include "toyctre/compile_time.hpp"
#include "toyctre/lib.hpp"
#include "toyctre/matcher.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

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

namespace toyctre {
namespace {

using namespace std::string_view_literals;
using compile_time::RegexString;

template <RegexString re> constexpr std::string_view patternOf() {
  // RegexString stores the trailing NUL.
  return {re.data, re.size() - 1};
}

template <RegexString re>
void checkMatches(std::initializer_list<std::string_view> matching,
                  std::initializer_list<std::string_view> nonMatching) {
  auto pattern = patternOf<re>();
  CAPTURE(pattern);
  for (auto text : matching) {
    CAPTURE(text);
    CHECK(compile_time::match<re>(text));
  }
  for (auto text : nonMatching) {
    CAPTURE(text);
    CHECK_FALSE(compile_time::match<re>(text));
  }
}

// Compare against the runtime (Thompson NFA) matcher on every string over
// `alphabet` up to length `maxLen`.
template <RegexString re>
void checkAgainstRuntime(std::string_view alphabet, std::size_t maxLen) {
  auto pattern = patternOf<re>();
  CAPTURE(pattern);
  auto parsed = parseRegex(pattern);
  REQUIRE(parsed.accepted);

  auto texts = std::vector<std::string>{""};
  for (std::size_t begin = 0, len = 0; len < maxLen; ++len) {
    auto end = texts.size();
    for (auto i = begin; i < end; ++i) {
      for (auto c : alphabet) {
        texts.push_back(texts[i] + c);
      }
    }
    begin = end;
  }

  for (const auto &text : texts) {
    CAPTURE(text);
    CHECK(compile_time::match<re>(text) == match(parsed.regex, text));
  }
}

constexpr std::string repeat(std::string_view s, std::size_t n) {
  auto ret = std::string{};
  for (std::size_t i = 0; i < n; ++i) {
    ret += s;
  }
  return ret;
}

// Freeze a constexpr std::string into a char array usable as a RegexString
// template argument: checkMatches<frozen<F>().data>(...).
template <std::size_t N> struct Chars {
  char data[N];
};
template <auto F> constexpr auto frozen() {
  constexpr auto n = F().size();
  Chars<n + 1> ret{};
  std::ranges::copy(F(), ret.data);
  return ret;
}

} // namespace

// ---------------------------------------------------------------------------
// Constant evaluation
// ---------------------------------------------------------------------------

TEST_CASE("CT: Usable in constant expressions", "[ct_matcher][constexpr]") {
  STATIC_CHECK(compile_time::match<"abc">("abc"));
  STATIC_CHECK_FALSE(compile_time::match<"abc">("abd"));
  STATIC_CHECK(compile_time::match<"a|b">("b"));
  STATIC_CHECK_FALSE(compile_time::match<"a|b">("ab"));
  STATIC_CHECK(compile_time::match<"ab*c">("abbbc"));
  STATIC_CHECK(compile_time::match<"a+">("aaa"));
  STATIC_CHECK_FALSE(compile_time::match<"a+">(""));
  STATIC_CHECK(compile_time::match<"ab?">("a"));
  STATIC_CHECK(compile_time::match<R"re(a\*)re">("a*"));
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

// ---------------------------------------------------------------------------
// Agreement with the runtime matcher on all short strings
// ---------------------------------------------------------------------------

TEST_CASE("CT: Agrees with runtime matcher", "[ct_matcher][differential]") {
  checkAgainstRuntime<"ab|ba">("ab", 6);
  checkAgainstRuntime<"a*b*">("ab", 6);
  checkAgainstRuntime<"a?b+a?">("ab", 6);
  checkAgainstRuntime<"(ab|ba)*">("ab", 8);
  checkAgainstRuntime<"(a|b)*abb">("ab", 8);
  checkAgainstRuntime<"(a|ab)(b|bb)*a?">("ab", 8);
  checkAgainstRuntime<"((ab)*|b+)a">("ab", 8);
  checkAgainstRuntime<"(a(b|c)*)+c">("abc", 6);
  checkAgainstRuntime<"(0|1(01*0)*1)*">("01", 10);
}

TEST_CASE("CT: Agrees with runtime matcher, empty-matching loops",
          "[ct_matcher][differential]") {
  checkAgainstRuntime<"(a*)*b">("ab", 6);
  checkAgainstRuntime<"(a?b?)+">("ab", 6);
  checkAgainstRuntime<"(a|b*)*a">("ab", 6);
}

// ---------------------------------------------------------------------------
// Size / pathological inputs
// ---------------------------------------------------------------------------

// A 300-element sequence currently exceeds GCC's default template depth (900)
// while parsing, which would fail the whole test build, so this stays below it.
TEST_CASE("CT: Long pattern", "[ct_matcher][stress]") {
  static constexpr auto pat = frozen<[] { return repeat("abc", 50); }>();
  auto text = repeat("abc", 50);
  checkMatches<pat.data>({text}, {text + "a", text.substr(1), ""});
}

TEST_CASE("CT: Long inputs", "[ct_matcher][stress]") {
  auto as = std::string(1000, 'a');
  auto abs = repeat("ab", 500);
  checkMatches<"a*">({as}, {as + "b"});
  checkMatches<"(a|b)*c">({abs + "c"}, {abs});
  checkMatches<"(ab)+">({abs}, {abs + "a"});
}

TEST_CASE("CT: Very long inputs", "[ct_matcher][stress][slow]") {
  auto as = std::string(10000, 'a');
  auto abs = repeat("ab", 5000);
  checkMatches<"a*">({as}, {as + "b"});
  checkMatches<"(a|b)*c">({abs + "c"}, {abs});
}

// A backtracking matcher is exponential on these, so n is kept small; the
// runtime (Thompson) tests use n = 30.
TEST_CASE("CT: Pathological for backtracking", "[ct_matcher][stress]") {
  constexpr std::size_t n = 12;

  SECTION("a?^n a^n against a^n") {
    static constexpr auto pat =
        frozen<[] { return repeat("a?", n) + repeat("a", n); }>();
    checkMatches<pat.data>(
        {std::string(n, 'a'), std::string(2 * n, 'a')},
        {std::string(n - 1, 'a'), std::string(2 * n + 1, 'a')});
  }

  SECTION("(a|aa)*c against a^n") {
    checkMatches<"(a|aa)*c">({std::string(n, 'a') + "c"},
                             {std::string(n, 'a')});
  }
}

} // namespace toyctre
