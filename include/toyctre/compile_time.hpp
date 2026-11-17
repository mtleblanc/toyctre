#pragma once

#include "toyctre/ct_base.hpp"
#include "toyctre/ct_parse.hpp"
#include "toyctre/regex_nodes.hpp"
#include <array>
#include <cstddef>
#include <stdexcept>
#include <string_view>

namespace re::ct {

namespace detail {
using namespace ctn;
using std::size_t;

using iterate::initial_state_t;
using iterate::IterTilResult;

namespace match {

template <size_t Groups, size_t Repetitions> class CaptureData {
  static constexpr auto RepStart = 2 * Groups + 2;
  std::array<size_t, RepStart + Repetitions> data{};
  size_t repIndex = RepStart;

public:
  constexpr CaptureData() = default;
  // group indexing starts at 1 (0 is whole match, currently unused)
  constexpr auto &groupStart(size_t i) {
    if (i > Groups || i == 0) {
      throw std::out_of_range{"Invalid group index"};
    }
    return data[2 * i - 2];
  }
  constexpr auto &groupEnd(size_t i) {
    if (i > Groups || i == 0) {
      throw std::out_of_range{"Invalid group index"};
    }
    return data[2 * i - 1];
  }

  // Repetition indexing starts at 0
  constexpr auto &repStart(size_t i) {
    if (i >= Repetitions) {
      throw std::out_of_range{"Invalid repetition index"};
    }
    return data[RepStart + i];
  }
};

template <size_t C> struct EndGroup;

template <typename Data> struct CapturingMatcher {
  template <typename...> struct Matcher {
    static_assert(false, "Missing matcher case");
  };

  // GCC 15 doesn't like full specialization not at namespace scope
  template <typename... Rs>
    requires(sizeof...(Rs) == 0)
  struct Matcher<Rs...> {
    static constexpr auto match(std::string_view text, Data &) -> bool {
      return text.empty();
    }
  };

  template <bool e, char c, typename... Rs>
  struct Matcher<Literal<e, c>, Rs...> {
    static constexpr auto match(std::string_view text, Data &data) -> bool {
      if (text.empty() || text.front() != c)
        return false;
      return Matcher<Rs...>::match(text.substr(1), data);
    }
  };

  template <typename... Ras, typename... Rbs>
  struct Matcher<Sequence<Ras...>, Rbs...> {
    static constexpr auto match(std::string_view text, Data &data) -> bool {
      return Matcher<Ras..., Rbs...>::match(text, data);
    }
  };

  template <typename... Ras, typename... Rbs>
  struct Matcher<Alternatives<Ras...>, Rbs...> {
    static constexpr auto match(std::string_view text, Data &data) -> bool {
      return (Matcher<Ras, Rbs...>::match(text, data) || ...);
    }
  };

  template <typename R, size_t C, typename... Rs>
  struct Matcher<Group<R, C>, Rs...> {
    static constexpr auto match(std::string_view text, Data &data) -> bool {
      auto &ref = data.groupStart(C);
      auto orig = ref;
      ref = text.size();
      auto res = Matcher<R, EndGroup<C>, Rs...>::match(text, data);
      if (!res) {
        ref = orig;
      }
      return res;
    }
  };

  template <size_t C, typename... Rs> struct Matcher<EndGroup<C>, Rs...> {
    static constexpr auto match(std::string_view text, Data &data) -> bool {
      auto &ref = data.groupEnd(C);
      auto orig = ref;
      ref = text.size();
      auto res = Matcher<Rs...>::match(text, data);
      if (!res) {
        ref = orig;
      }
      return res;
    }
  };

  template <typename R, size_t C, typename... Rs>
  struct Matcher<Repetition<R, '?', C>, Rs...> {
    static constexpr auto match(std::string_view text, Data &data) -> bool {
      return Matcher<R, Rs...>::match(text, data) ||
             Matcher<Rs...>::match(text, data);
    }
  };

  template <typename R, size_t C, typename... Rs>
  struct Matcher<Repetition<R, '*', C>, Rs...> {
    static constexpr auto match(std::string_view text, Data &data) -> bool {
      if (text.empty()) {
        return Matcher<Rs...>::match(text, data);
      }
      auto &ref = data.repStart(C);
      if (ref == text.size()) {
        return false;
      }
      auto orig = ref;
      ref = text.size();
      auto first = Matcher<R, Repetition<R, '*', C>, Rs...>::match(text, data);
      ref = orig;
      return first || Matcher<Rs...>::match(text, data);
    }
  };

  template <typename R, size_t C, typename... Rs>
  struct Matcher<Repetition<R, '+', C>, Rs...> {
    static constexpr auto match(std::string_view text, Data &data) -> bool {
      return Matcher<R, Repetition<R, '*', C>, Rs...>::match(text, data);
    }
  };
};

} // namespace match
using match::CaptureData;
using match::CapturingMatcher;

} // namespace detail

using state::ParseFailure;

template <RegexString regex>
struct CompileTimeRegex
    : detail::IterTilResult<detail::initial_state_t<regex>> {};

template <RegexString regex>
using compile_time_regex_t = typename CompileTimeRegex<regex>::type;

template <size_t N> struct MatchResult {
  struct Group {
    size_t start;
    size_t end;
  };
  bool success{};
  std::array<Group, N> groups{};

  constexpr operator bool() const { return success; }
};

template <RegexString regex> constexpr auto match(std::string_view text) {
  using Regex = compile_time_regex_t<regex>;
  using P = progress::Progress<regex, regex.size()>;
  constexpr auto groups = P::capture_number_v;
  constexpr auto repetitions = P::repetition_number_v;
  using CaptureData = detail::CaptureData<groups, repetitions>;
  using Matcher =
      detail::CapturingMatcher<CaptureData>::template Matcher<Regex>;
  using Ret = MatchResult<groups + 1>;
  auto captures = CaptureData{};
  if constexpr (IsRegex<Regex>) {
    if (!Matcher::match(text, captures)) {
      return Ret{};
    }
    auto ret = Ret{true, {}};
    // first group is entire match.  Right now we only support full matches
    ret.groups[0] = {0uz, text.size()};
    for (auto i = 1uz; i <= groups; i++) {
      ret.groups[i] = {text.size() - captures.groupStart(i),
                       text.size() - captures.groupEnd(i)};
    }
    return ret;
  } else {
    static_assert(false, "failed to parse regex");
    return Ret{};
  }
}

} // namespace re::ct
