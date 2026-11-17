#pragma once

#include <cstddef>

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

template <IsRegex R, char r, size_t C> struct Repetition {};
template <typename R, char r, size_t C>
inline constexpr auto is_regex_v<Repetition<R, r, C>> = true;

template <bool e, char c> struct Literal {};
template <bool e, char c>
inline constexpr auto is_regex_v<Literal<e, c>> = true;

} // namespace re::ctn
