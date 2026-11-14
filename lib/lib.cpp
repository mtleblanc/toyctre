#include "toyctre/lib.hpp"
#include "toyctre/grammar.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace toyctre {
namespace detail {
using std::size_t;

struct Sym {
  Symbol symbol;
  char c;
  constexpr bool operator==(Symbol o) const { return symbol == o; };
};

constexpr auto classify(char c) {
  switch (c) {
  case '(':
    return Symbol::T_LPar;
  case ')':
    return Symbol::T_RPar;
  case '+':
  case '*':
  case '?':
    return Symbol::T_Rep;
  case '\\':
    return Symbol::T_Esc;
  case '|':
    return Symbol::T_Bar;
  default:
    return Symbol::T_Reg;
  }
}

constexpr auto lex(char c) -> Sym { return {.symbol = classify(c), .c = c}; }

struct Mark {};
using Node = std::variant<Regex, Alternative, Concatenation, Repetition, Mark>;

constexpr ParseResult parse(std::string_view regex) {
  using enum Symbol;

  auto syms = std::views::iota(0) | std::views::transform([regex](auto i) {
                return i < std::ssize(regex) ? lex(regex[i]) : Sym{T_Eot, 0};
              });
  auto it = syms.begin();
  auto sentinels = std::vector<Symbol>{Start};
  auto nodes = std::vector<Node>{};
  while (!sentinels.empty()) {
    auto cur = sentinels.back();
    sentinels.pop_back();
    if (cur > Actions_) {
      switch (cur) {
      case A_Mark: {
        nodes.emplace_back(Mark{});
        break;
      }
      case A_Alts: {
        auto node = Alternative{};
        while (std::holds_alternative<Concatenation>(nodes.back())) {
          node.concatenations.insert(
              node.concatenations.begin(),
              std::move(std::get<Concatenation>(nodes.back())));
          nodes.pop_back();
        }
        assert(std::holds_alternative<Mark>(nodes.back()));
        nodes.pop_back();
        nodes.emplace_back(std::move(node));
        break;
      }
      case A_Cats: {
        auto node = Concatenation{};
        while (std::holds_alternative<Repetition>(nodes.back())) {
          node.repetitions.insert(
              node.repetitions.begin(),
              std::move(std::get<Repetition>(nodes.back())));
          nodes.pop_back();
        }
        assert(std::holds_alternative<Mark>(nodes.back()));
        nodes.pop_back();
        nodes.emplace_back(std::move(node));
        break;
      }
      case A_Group: {
        auto node = Repetition{std::move(std::get<Alternative>(nodes.back())),
                               0, true, false};
        nodes.pop_back();
        nodes.emplace_back(std::move(node));
        break;
      }
      case A_Lit: {
        auto node = Repetition{{}, (*it).c, false, false};
        nodes.emplace_back(std::move(node));
        break;
      }
      case A_Esc: {
        auto node = Repetition{{}, (*it).c, false, true};
        nodes.emplace_back(std::move(node));
        break;
      }
      case A_Rep: {
        auto &node = std::get<Repetition>(nodes.back());
        node.rep = (*it).c;
        break;
      }
      case A_Reject: {
        return {{}, false};
      }
      default:
        std::unreachable();
      }

    } else if (cur > Terminals_) {
      if (*it != cur) {
        return {{}, false};
      }
      ++it;
    } else {
      auto rule = grammar::table[cur][(*it).symbol];
      auto body = grammar::productions[rule].body;
      std::copy(body.crbegin(), body.crend(), std::back_inserter(sentinels));
    }
  }

  if (*it != T_Eot) {
    return {{}, false};
  }

  return {{std::move(std::get<Alternative>(nodes.back()))}, true};
}
} // namespace detail

ParseResult parseRegex(std ::string_view regex) { return detail::parse(regex); }

} // namespace toyctre
