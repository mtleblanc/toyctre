#pragma once

#include <array>
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

template <bool matches, char... cs> struct CharacterClass {};
template <char... cs>
inline constexpr auto is_regex_v<CharacterClass<cs...>> = true;

template <bool matches, char... cs> struct CharacterRangeClass {};
template <char... cs>
inline constexpr auto is_regex_v<CharacterRangeClass<cs...>> = true;

template <typename T> struct Negate;
template <bool matches, char... cs>
struct Negate<CharacterClass<matches, cs...>> {
  using type = CharacterClass<!matches, cs...>;
};

template <typename T> struct Negate;
template <bool matches, char... cs>
struct Negate<CharacterRangeClass<matches, cs...>> {
  using type = CharacterRangeClass<!matches, cs...>;
};

struct CharacterClasses {
  using dot = CharacterClass<false, '\n'>;
  using decimal = CharacterRangeClass<true, '0', '9'>;
  using word =
      CharacterRangeClass<true, '0', '9', 'A', 'Z', '_', '_', 'a', 'z'>;
  using hspace = CharacterClass<true, 0x20, 0x09>;
  using vspace = CharacterRangeClass<true, 0x0A, 0x0D>;
  using space = CharacterClass<true, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x20>;
};

} // namespace re::ctn
