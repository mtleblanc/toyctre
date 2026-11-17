#pragma once

#include <algorithm>
#include <cstddef>

namespace re::ct {
// zero sized arrays are an extension, but even in standard the requires gives
// an error message early
template <std::size_t N>
  requires(N > 0)
struct RegexString {
  constexpr RegexString(const char (&str)[N]) { std::copy_n(str, N, data); }
  char data[N];
  // we store the null because it gets indexed, but don't report it as part of
  // the size
  constexpr auto size() const { return N - 1; }
};
} // namespace re::ct
