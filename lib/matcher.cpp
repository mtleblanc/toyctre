#include "toyctre/matcher.hpp"
#include "toyctre/lib.hpp"
#include <array>
#include <cassert>
#include <format>
#include <print>
#include <set>
#include <stdexcept>

namespace toyctre {

namespace {

class NFA {
  using CharT = char;
  using TransitionT = std::set<size_t>;

  // use index 256 for epsilon
  static constexpr auto N_CHARS = 256uz;
  using TransitionTable = std::vector<std::array<TransitionT, N_CHARS + 1>>;

  TransitionTable transitions;
  TransitionT accept;

  struct Epsilon {};

  static auto shifted(const TransitionT &orig, size_t n) {
    auto ret = TransitionT{};
    for (auto i : orig) {
      ret.insert(i + n);
    }
    return ret;
  }

public:
  static constexpr auto epsilon = Epsilon{};

  NFA(size_t states) : transitions{states}, accept{} {
    if (states == 0) {
      throw std::invalid_argument{
          "Empty NFA.  At least a starting state is required"};
    }
  }

  NFA() : transitions{1}, accept{0} {}
  NFA(const NFA &) = default;
  NFA(NFA &&) = default;
  NFA &operator=(const NFA &) = default;
  NFA &operator=(NFA &&) = default;

  template <typename Self>
  [[nodiscard]] constexpr auto &&operator[](this Self &&self, size_t state,
                                            char transition) {
    assert(state < self.transitions.size());
    return self.transitions[state][static_cast<size_t>(transition) & 0xFF];
  }

  template <typename Self>
  [[nodiscard]] constexpr auto &&operator[](this Self &&self, size_t state,
                                            Epsilon) noexcept {
    return self.transitions[state][N_CHARS];
  }

  [[nodiscard]] constexpr auto size() const noexcept {
    return transitions.size();
  }

  static auto of(CharT c) {
    auto ret = NFA(2);
    ret[0, c].insert(1);
    ret.accept.insert(1);
    return ret;
  }

  // concat
  template <typename Self> auto operator*(this Self &&self, const NFA &rhs) {
    auto ret = self;
    auto n = ret.size();
    ret.transitions.resize(ret.size() + rhs.size(), {});
    for (auto i = 0uz; i < rhs.transitions.size(); ++i) {
      for (auto j = 0uz; j < N_CHARS; ++j) {
        ret[i + n, j] = shifted(rhs[i, j], n);
      }
      ret[i + n, epsilon] = shifted(rhs[i, epsilon], n);
    }
    for (auto i : ret.accept) {
      ret[i, epsilon].insert(n);
    }
    ret.accept = shifted(rhs.accept, n);
    return ret;
  }

  template <typename Self> auto operator+(this Self &&self, const NFA &rhs) {
    auto ret = self;
    const auto n = ret.size();
    const auto m = rhs.size();
    ret.transitions.resize(n + m + 1, {});
    for (auto i = n; i-- > 0;) {
      for (auto j = 0uz; j < N_CHARS; ++j) {
        ret[i + 1, j] = shifted(ret[i, j], 1);
      }
      ret[i + 1, epsilon] = shifted(ret[i, epsilon], 1);
    }

    for (auto i = m; i-- > 0;) {
      for (auto j = 0uz; j < N_CHARS; ++j) {
        ret[i + 1 + n, j] = shifted(rhs[i, j], n + 1);
      }
      ret[i + 1 + n, epsilon] = shifted(rhs[i, epsilon], n + 1);
    }

    ret.transitions[0] = {};
    ret[0, epsilon].insert(1);
    ret[0, epsilon].insert(n + 1);

    ret.accept = shifted(std::move(ret.accept), 1);
    for (auto i : rhs.accept) {
      ret.accept.insert(i + n + 1);
    }
    return ret;
  }

  auto operator++(int) {
    auto ret = *this;
    for (auto i : ret.accept) {
      ret[i, epsilon].insert(0);
    }
    return ret;
  }

  auto accepts(std::set<size_t> states) const {

    for (auto a : accept) {
      if (states.contains(a)) {
        return true;
      }
    }
    return false;
  }

  static std::string commaDelim(const auto &r) {

    auto ret = std::string{};
    for (auto i : r) {
      if (!ret.empty()) {
        ret += ',';
      }
      ret += std::format("{}", i);
    }
    return ret;
  }

  std::string toString() const {
    const auto &self = *this;
    auto ret = std::format("Accepts {}\n", commaDelim(accept));
    for (auto i = 0uz; i < size(); ++i) {
      for (auto j = 0uz; j < N_CHARS; ++j) {
        if (!self[i, j].empty()) {
          ret += std::format("{}, {} -> {}\n", i, (CharT)j,
                             commaDelim(self[i, j]));
        }
      }
      if (!self[i, epsilon].empty()) {
        ret += std::format("{} -e> {}\n", i, commaDelim(self[i, epsilon]));
      }
    }
    return ret;
  }
};

NFA forNode(const Regex &reg);
NFA forNode(const Alternative &alt);
NFA forNode(const Concatenation &cat);
NFA forNode(const Repetition &regex);

NFA forNode(const Regex &reg) { return forNode(reg.alternatives); }
NFA forNode(const Alternative &alt) {
  auto ret = NFA{1};
  for (const auto &c : alt.concatenations) {
    ret = ret + forNode(c);
  }
  return ret;
}

NFA forNode(const Concatenation &cat) {
  auto ret = NFA{};
  for (const auto &r : cat.repetitions) {
    ret = ret * forNode(r);
  }
  return ret;
}

NFA forNode(const Repetition &regex) {
  auto base = regex.isNested ? forNode(regex.regex) : NFA::of(regex.literal);
  switch (regex.rep) {
  case 0:

    return base;
  case '*':
    return NFA{} + base++;
  case '+':
    return base++;
  case '?':
    return base + NFA{};
  default:
    throw std::invalid_argument{
        std::format("Unexpected rep type {}", regex.rep)};
  }
}

} // namespace

void test() {
  // auto r =
  //     NFA::of('a') * NFA::of('b') * NFA::of('c') + NFA::of('d') *
  //     NFA::of('e');
  using namespace std::literals::string_view_literals;
  auto pat = "a\0b"sv;
  auto m = "ab";
  // auto r = NFA{};
  auto r = forNode(parseRegex(pat).regex);
  std::println("{}", r.toString());
  std::println("Matches {}? {}", m, match(parseRegex(pat).regex, m));
}

auto step(size_t c, const NFA &nfa, const std::set<size_t> &states) {
  auto ret = std::set<size_t>{};
  for (auto s : states) {
    for (auto t : nfa[s, c]) {
      ret.insert(t);
    }
  }
  return ret;
}

std::set<size_t> epsilonClosure(const NFA &nfa,
                                const std::set<size_t> &states) {
  auto next = states;
  for (auto s : states) {
    for (auto t : nfa[s, NFA::epsilon]) {
      next.insert(t);
    }
  }
  for (auto s : states) {
    next.insert(s);
  }
  return next == states ? next : epsilonClosure(nfa, next);
}

bool match(const Regex &regex, std::string_view text) {
  auto nfa = forNode(regex);
  auto states = epsilonClosure(nfa, std::set<size_t>{0});
  for (auto c : text) {
    states = epsilonClosure(nfa, step(c, nfa, states));
  }
  return nfa.accepts(states);
}
} // namespace toyctre
