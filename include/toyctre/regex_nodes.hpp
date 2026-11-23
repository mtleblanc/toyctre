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

template <char... cs> struct Chars;
template <char s, char e> struct Range {
  static constexpr auto matches(char c) { return c >= s && c <= e; }
};

template <bool positive, typename Cs, typename... Rs> struct CharacterClass {};
template <bool positive, typename C, typename... Rs>
inline constexpr auto is_regex_v<CharacterClass<positive, C, Rs...>> = true;

template <typename T> struct Negate;
template <bool positive, typename C, typename Rs>
struct Negate<CharacterClass<positive, C, Rs>> {
  using type = CharacterClass<!positive, C, Rs>;
};

struct CharacterClasses {
  using dot = CharacterClass<false, Chars<'\n'>>;
  using decimal = CharacterClass<true, Chars<>, Range<'0', '9'>>;
  using word = CharacterClass<true, Chars<'_'>, Range<'0', '9'>,
                              Range<'A', 'Z'>, Range<'a', 'z'>>;
  using hspace = CharacterClass<true, Chars<0x20, 0x09>>;
  using vspace = CharacterClass<true, Chars<>, Range<0x0A, 0x0D>>;
};

} // namespace re::ctn
