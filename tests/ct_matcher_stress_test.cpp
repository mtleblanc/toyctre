#include "ct_matcher_helpers.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <string>

// The expensive-to-compile part of the compile-time matcher tests: long and
// pathological patterns, and the differential tests against the runtime
// matcher. Split from ct_matcher_test.cpp so the two compile in parallel. See
// that file for the matching semantics.

namespace re {
namespace {

using namespace ct_test;

} // namespace

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

// A 150-character pattern. Parsing it is by far the most expensive thing in
// the compile-time matcher tests (~0.75s on its own).
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

} // namespace re
