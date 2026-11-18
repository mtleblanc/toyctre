#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <string_view>

namespace re::ctn {
using std::size_t;

template <typename T> inline constexpr auto is_regex_v = false;
template <typename T>
concept IsRegex = is_regex_v<T>;

template <IsRegex R, size_t C> struct Group {};
template <typename R, size_t C>
inline constexpr auto is_regex_v<Group<R, C>> = true;

template <IsRegex... Ns> struct Sequence {};
template <typename... Ns>
inline constexpr auto is_regex_v<Sequence<Ns...>> = true;

template <IsRegex... Ns> struct Alternatives {};
template <typename... Ns>
inline constexpr auto is_regex_v<Alternatives<Ns...>> = true;

template <IsRegex R, char r> struct Repetition {};
template <typename R, char r>
inline constexpr auto is_regex_v<Repetition<R, r>> = true;

template <char... cs> struct LiteralString {
  static constexpr std::array<char, sizeof...(cs) + 1> data = {cs..., 0};
  static constexpr std::string_view view =
      std::string_view{data.data(), sizeof...(cs)};
};
template <char... cs>
inline constexpr auto is_regex_v<LiteralString<cs...>> = true;

} // namespace re::ctn
