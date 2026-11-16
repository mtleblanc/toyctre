#pragma once

// Helpers shared by the compile-time matcher test files.
//
// The compile-time tests are split across translation units because nearly
// all of their build cost is parsing each regex at compile time; separate TUs
// compile in parallel.

#include "toyctre/compile_time.hpp"
#include "toyctre/lib.hpp"
#include "toyctre/matcher.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace toyctre::ct_test {

using compile_time::RegexString;

template <RegexString re> constexpr std::string_view patternOf() {
  return {re.data, re.size()};
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

} // namespace toyctre::ct_test
