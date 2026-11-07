#include "toyctre/lib.hpp"

#include <catch2/catch_test_macros.hpp>

#include <initializer_list>
#include <string>
#include <string_view>

// Accept/reject tests for the LL(1) regex parser.
//
// Current language concessions:
//   - empty alternatives / empty groups / empty regex are rejected
//   - at most one repetition operator (*, +, ?) per unit
//   - any character may be escaped with '\'; it is a literal
//   - '.' has no special meaning yet; it is an ordinary literal
//
// Each TEST_CASE is a separate ctest entry, so a crash in one category
// doesn't hide results from the others.

namespace toyctre {
namespace {

void checkAccepts(std::initializer_list<std::string_view> regexes) {
  for (auto re : regexes) {
    CAPTURE(re);
    auto res = parseRegex(re);
    CHECK(res.accepted);
    CHECK(re == res.regex.toString());
  }
}

void checkRejects(std::initializer_list<std::string_view> regexes) {
  for (auto re : regexes) {
    CAPTURE(re);
    CHECK_FALSE(parseRegex(re).accepted);
  }
}

} // namespace

// ---------------------------------------------------------------------------
// Accepted
// ---------------------------------------------------------------------------

TEST_CASE("Parser: Single literal characters", "[parser][accept][literal]") {
  checkAccepts({"a", "z", "A", "0", "9", " ", "_", "-", ".", "^", "$", "[", "]",
                "{", "}", ",", "/", "\t"});
}

TEST_CASE("Parser: Literal sequences", "[parser][accept][literal]") {
  checkAccepts({"ab", "abc", "hello", "hello world", "a.b", "...", "a-z",
                "[abc]", "{1,2}"});
}

TEST_CASE("Parser: Alternation", "[parser][accept][alt]") {
  checkAccepts({"a|b", "a|b|c", "a|b|c|d|e", "ab|cd", "ab|cd|ef", "a|bc",
                "abc|d", ".|."});
}

TEST_CASE("Parser: Groups", "[parser][accept][group]") {
  checkAccepts({"(a)", "((a))", "(((a)))", "(ab)", "(a)(b)", "(a)(b)(c)",
                "a(b)c", "(a)b", "a(b)", "x(y(z))"});
}

TEST_CASE("Parser: Groups containing alternation",
          "[parser][accept][group][alt]") {
  checkAccepts({"(a|b)", "(a|b|c)", "(a|b)|c", "a|(b|c)", "(a|b)(c|d)",
                "((a|b)c|d)e", "x(a|b)y", "((a|b)|(c|d))"});
}

TEST_CASE("Parser: Repetition of a single literal", "[parser][accept][rep]") {
  checkAccepts({"a*", "a+", "a?"});
}

TEST_CASE("Parser: Repetition within sequences", "[parser][accept][rep]") {
  checkAccepts(
      {"ab*", "a*b", "a*b*", "a?b?c?", "a+b+c+", "xa*y", ".*", "a.*b"});
}

TEST_CASE("Parser: Repetition of groups", "[parser][accept][rep][group]") {
  checkAccepts({"(a)*", "(ab)*", "(ab)+", "(ab)?", "(a|b)*", "(a|b)+", "(a*)*",
                "((a)+)?", "(a)*b", "a(b)*", "(a)*(b)+"});
}

TEST_CASE("Parser: Repetition adjacent to alternation",
          "[parser][accept][rep][alt]") {
  checkAccepts({"a*|b", "a|b*", "a*|b+", "a?|b?|c?", "ab*|cd+"});
}

TEST_CASE("Parser: Repetition immediately before close paren",
          "[parser][accept][rep][group]") {
  checkAccepts({"(a*)", "(a+)", "(a?)", "(ab*)", "(a|b*)", "(a*|b)", "((a*))",
                "(a*)b", "x(a+)y"});
}

TEST_CASE("Parser: Escaped metacharacters", "[parser][accept][escape]") {
  checkAccepts({R"re(\*)re", R"re(\+)re", R"re(\?)re", R"re(\()re", R"re(\))re",
                R"re(\\)re", R"re(\|)re"});
}

TEST_CASE("Parser: Escaped ordinary characters", "[parser][accept][escape]") {
  checkAccepts({R"re(\a)re", R"re(\.)re", R"re(\n)re", R"re(\0)re", R"re(\ )re",
                R"re(\[)re"});
}

TEST_CASE("Parser: Escapes in context", "[parser][accept][escape]") {
  checkAccepts({R"re(\(hello)re", R"re(hello\))re", R"re(\(\))re",
                R"re(a\|b)re", R"re(a\*b)re", R"re(\\\\)re", R"re(\a\b\c)re",
                R"re(a|\|)re", R"re(\||a)re"});
}

TEST_CASE("Parser: Escapes inside groups", "[parser][accept][escape][group]") {
  checkAccepts({R"re((\())re", R"re((\)))re", R"re((\|))re", R"re((a\)b))re",
                R"re((\\))re", R"re((\a|\b))re"});
}

TEST_CASE("Parser: Repetition of escapes", "[parser][accept][escape][rep]") {
  checkAccepts({R"re(\**)re", R"re(\++)re", R"re(\??)re", R"re(\(*)re",
                R"re(\)+)re", R"re(\\*)re", R"re(\|?)re", R"re(\a*)re",
                R"re((\a*))re", R"re((\)*))re", R"re(\a*|b)re"});
}

TEST_CASE("Parser: Composite expressions", "[parser][accept]") {
  checkAccepts({"(a|b)*abb", "a(b|c)*d", "((ab)*|c+)?d", "(a(b(c)*)+)?",
                "x*(y|z)+w?", R"re((\(|\))*)re", "(0|1(01*0)*1)*",
                "ab|c(d|e)*f?"});
}

TEST_CASE("Parser: Long inputs", "[parser][accept][stress]") {
  SECTION("long literal") {
    CHECK(parseRegex(std::string(1000, 'a')).accepted);
  }

  SECTION("long alternation") {
    auto re = std::string("a");
    for (int i = 0; i < 500; ++i) {
      re += "|a";
    }
    CHECK(parseRegex(re).accepted);
  }

  SECTION("deep nesting") {
    auto re = std::string(200, '(') + "a" + std::string(200, ')');
    CHECK(parseRegex(re).accepted);
  }

  SECTION("deep nesting with repetition") {
    auto re = std::string("a");
    for (int i = 0; i < 100; ++i) {
      re = "(" + re + ")*";
    }
    CHECK(parseRegex(re).accepted);
  }

  SECTION("many repeated units") {
    auto re = std::string();
    for (int i = 0; i < 500; ++i) {
      re += "a*";
    }
    CHECK(parseRegex(re).accepted);
  }
}

// ---------------------------------------------------------------------------
// Rejected
// ---------------------------------------------------------------------------

TEST_CASE("Parser: Empty regex", "[parser][reject]") { checkRejects({""}); }

TEST_CASE("Parser: Unbalanced parentheses", "[parser][reject][group]") {
  checkRejects({"(", ")", "(a", "a)", "((a)", "(a))", ")(", ")a(", "a)(b",
                "(a|b", "a|b)", "(a)b)", "((a)|b"});
}

TEST_CASE("Parser: Empty groups", "[parser][reject][group]") {
  checkRejects({"()", "(())", "a()", "()a", "a()b", "()*", "(()a)"});
}

TEST_CASE("Parser: Empty alternatives", "[parser][reject][alt]") {
  checkRejects({"|", "||", "a|", "|a", "a||b", "a|b|", "|a|b", "(|a)", "(a|)",
                "(|)", "(a||b)", "a|()", "(a|)b"});
}

TEST_CASE("Parser: Repetition without operand", "[parser][reject][rep]") {
  checkRejects({"*", "+", "?", "*a", "+a", "?a", "(*)", "(*a)", "(+a)", "a|*b",
                "a|+", "|*", "(a|*)", "a(*b)"});
}

TEST_CASE("Parser: Stacked repetition operators", "[parser][reject][rep]") {
  checkRejects({"a**", "a++", "a??", "a*+", "a+*", "a*?", "a+?", "a?*", "(a)**",
                "(a)*+", "a***", R"re(\a**)re", "(a**)"});
}

TEST_CASE("Parser: Dangling escape", "[parser][reject][escape]") {
  checkRejects({R"re(\)re", R"re(a\)re", R"re(\\\)re", R"re((\)re",
                R"re((a\)re", R"re(a|\)re"});
}

TEST_CASE("Parser: Valid prefix followed by garbage", "[parser][reject]") {
  checkRejects({"a)b", "(a))b", "ab)", "a|b)c", "(a)*)"});
}

} // namespace toyctre
