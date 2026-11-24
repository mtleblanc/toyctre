#include "ct_matcher_helpers.hpp"

#include <catch2/catch_test_macros.hpp>

// Character classes in the compile-time matcher, following PCRE.
//
// Not covered yet: POSIX classes ([:alpha:]), class escapes (\d, \w, ...),
// escapes for control characters (\n, \t, ...) and multi-character escapes
// such as \x{...}. Every escape here is an escaped punctuation character, which
// means the same in PCRE as the plain escaped character.
//
// Every pattern here must parse: a parse failure is a compile error that breaks
// this whole file, not a failed check.

namespace re {
namespace {

using namespace ct_test;

} // namespace

TEST_CASE("CT class: listed characters", "[ct_matcher][class]") {
  checkMatches<"[abc]">({"a", "b", "c"}, {"", "d", "A", "ab", "-"});
  checkMatches<"[_a-z0-9]">({"_", "a", "q", "z", "0", "9"}, {"A", "-", " "});
}

TEST_CASE("CT class: ranges", "[ct_matcher][class]") {
  checkMatches<"[a-c]">({"a", "b", "c"}, {"", "d", "`", "-", "ac"});
  checkMatches<"[a-cx-z]">({"a", "c", "x", "z"}, {"d", "w", "-"});
  checkMatches<"[a-a]">({"a"}, {"b", "`", "-"});
}

TEST_CASE("CT class: negated", "[ct_matcher][class]") {
  checkMatches<"[^abc]">({"d", "z", "A", "-", "^", "\n", " "}, {"", "a", "b", "c", "dd"});
  checkMatches<"[^a-z]">({"A", "0", "`", "{"}, {"a", "m", "z"});
}

TEST_CASE("CT class: literal caret", "[ct_matcher][class]") {
  checkMatches<"[^^]">({"a", "\\"}, {"^", ""});
  checkMatches<"[a^]">({"a", "^"}, {"b"});
  checkMatches<"[^a^]">({"b"}, {"a", "^"});
  checkMatches<R"re([\^])re">({"^"}, {"\\", "a"});
}

TEST_CASE("CT class: literal hyphen", "[ct_matcher][class]") {
  checkMatches<"[-a]">({"-", "a"}, {"b", ","});
  checkMatches<"[a-]">({"a", "-"}, {"b", ","});
  checkMatches<"[^-]">({"a"}, {"-"});
  checkMatches<"[^-a]">({"b"}, {"-", "a"});
  // After a range: a-c, '-', 'e'
  checkMatches<"[a-c-e]">({"a", "b", "c", "-", "e"}, {"d"});
  checkMatches<"[a-c-]">({"b", "-"}, {"d"});
  checkMatches<R"re([\-])re">({"-"}, {"\\", ""});
  checkMatches<R"re([a\-z])re">({"a", "-", "z"}, {"b", "y"});
}

TEST_CASE("CT class: escaped range endpoints", "[ct_matcher][class][escape]") {
  // W..] is 0x57..0x5D, which includes [ and backslash.
  checkMatches<R"re([W-\]])re">({"W", "X", "Z", "[", "\\", "]"}, {"V", "^", "-", "w"});
  // -../ is 0x2D..0x2F.
  checkMatches<R"re([\--/])re">({"-", ".", "/"}, {",", "0"});
}

TEST_CASE("CT class: square brackets", "[ct_matcher][class]") {
  // ] is literal as the first data character (after an optional ^).
  checkMatches<"[]]">({"]"}, {"[", ""});
  checkMatches<"[]a]">({"]", "a"}, {"b"});
  checkMatches<"[^]]">({"a", "["}, {"]"});
  checkMatches<"[^]a]">({"b"}, {"]", "a"});
  checkMatches<R"re([\]])re">({"]"}, {"\\"});
  checkMatches<R"re([\[\]])re">({"[", "]"}, {"\\"});
  // A class of W and '-' followed by the literal "46]".
  checkMatches<"[W-]46]">({"W46]", "-46]"}, {"X46]", "W46", "W"});
  // A class followed by a literal ].
  checkMatches<"[a]]">({"a]"}, {"a", "]"});
  checkMatches<"[[]">({"["}, {"]"});
  checkMatches<"[[a]">({"[", "a"}, {"[a"});
}

TEST_CASE("CT class: other metacharacters are literal", "[ct_matcher][class]") {
  checkMatches<"[.]">({"."}, {"a"});
  checkMatches<"[.$|()*+?]">({".", "$", "|", "(", ")", "*", "+", "?"}, {"a", "", "\\"});
  // ( .. + is 0x28..0x2B.
  checkMatches<"[(-+]">({"(", ")", "*", "+"}, {"'", ","});
  checkMatches<R"re([\\])re">({"\\"}, {"a", ""});
}

TEST_CASE("CT class: in context", "[ct_matcher][class]") {
  checkMatches<"a[bc]d">({"abd", "acd"}, {"aad", "ad", "abcd", "abd "});
  checkMatches<"[ab]|c">({"a", "b", "c"}, {"ab", "bc", ""});
  checkMatches<"[a][b]">({"ab"}, {"ba", "a"});
  checkMatches<"[ab](c|d)[^e]">({"acf", "bdx", "ac-"}, {"ace", "cdf", "ac"});
}

TEST_CASE("CT class: repeated", "[ct_matcher][class][rep]") {
  checkMatches<"[ab]*">({"", "a", "abba"}, {"abc", "c"});
  checkMatches<"[ab]+">({"a", "ab", "bbb"}, {"", "abc"});
  checkMatches<"[ab]?">({"", "a", "b"}, {"ab", "c"});
  checkMatches<"[^a]*">({"", "bcd", "\n\n"}, {"bad", "a"});
  checkMatches<"[a-z]+[0-9]*">({"abc123", "z", "a0"}, {"123", "", "A1"});
  checkMatches<"x[ab]?y">({"xy", "xay", "xby"}, {"xaby", "ay", "x"});
  checkMatches<"([ab]c)*">({"", "ac", "acbc"}, {"a", "acb", "cc"});
  // Backtracking out of a class repetition
  checkMatches<"[a-c]*c">({"c", "abc", "ccc"}, {"", "abd", "ab"});
}

TEST_CASE("CT class: high-bit bytes", "[ct_matcher][class][bytes]") {
  checkMatches<"[\x80-\xff]">({"\x80", "\xc3", "\xff"}, {"a", "\x7f", ""});
  checkMatches<"[^\x80-\xff]">({"a", "\x7f"}, {"\x80", "\xff"});
  checkMatches<"[^a]">({"\x80", "\xff"}, {"a"});
}

} // namespace re
