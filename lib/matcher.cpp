#include "toyctre/matcher.hpp"
#include "toyctre/lib.hpp"
#include <algorithm>
#include <cassert>
#include <memory_resource>
#include <print>
#include <ranges>

namespace toyctre {

namespace {

struct Node {
  static constexpr auto ALTERNATIVE = 256;
  static constexpr auto ACCEPT = 257;
  int c{};
  Node *next{};
  Node *nextl{};
  int gen{};

  Node(int c, Node *next = nullptr, Node *nextl = nullptr)
      : c{c}, next{next}, nextl{nextl} {}
};

struct Fragment;
class NfaBuilder {
  std::pmr::monotonic_buffer_resource bufferResource{};
  std::pmr::polymorphic_allocator<> nodes{&bufferResource};

public:
  Fragment of(char c);
  Fragment of(const Regex &reg);
  Fragment of(const Alternative &alt);
  Fragment of(const Concatenation &cat);
  Fragment of(const Repetition &rep);
  Node *match(char c) { return nodes.new_object<Node>(c); }
  Node *alt(Node *next = nullptr, Node *nextl = nullptr) {
    return nodes.new_object<Node>(Node::ALTERNATIVE, next, nextl);
  }
  Node *accept() { return nodes.new_object<Node>(Node::ACCEPT); }
};

struct Fragment {
  NfaBuilder *builder;
  Node *start{};
  std::vector<Node **> dangling{};

  Fragment(NfaBuilder *builder, Node *start, std::vector<Node **> dangling)
      : builder{builder}, start{start}, dangling{std::move(dangling)} {}
  Fragment(const Fragment &) = delete;
  Fragment(Fragment &&) = default;
  Fragment &operator=(const Fragment &) = delete;
  Fragment &operator=(Fragment &&) = default;

  void patch(Node *n) {
    for (auto p : dangling) {
      *p = n;
    }
    dangling.clear();
  }

  void append(const Fragment &o) {
    dangling.insert(dangling.end(), o.dangling.begin(), o.dangling.end());
  }

  Fragment operator*(this Fragment lhs, Fragment rhs) {
    lhs.patch(rhs.start);
    rhs.start = lhs.start;
    return rhs;
  }

  Fragment operator+(this Fragment lhs, Fragment rhs) {
    lhs.start = lhs.builder->alt(lhs.start, rhs.start);
    lhs.append(rhs);
    return lhs;
  }

  Fragment operator++(this Fragment lhs, int) {
    auto alt = lhs.builder->alt(lhs.start);
    lhs.patch(alt);
    lhs.dangling.push_back(&alt->nextl);
    return lhs;
  }

  Fragment maybe(this Fragment lhs) {
    lhs.start = lhs.builder->alt(lhs.start);
    lhs.dangling.push_back(&lhs.start->nextl);
    return lhs;
  }

  Fragment star(this Fragment lhs) {
    lhs.start = lhs.builder->alt(lhs.start);
    lhs.patch(lhs.start);
    lhs.dangling.push_back(&lhs.start->nextl);
    return lhs;
  }

  Fragment accept(this Fragment lhs) {
    lhs.patch(lhs.builder->accept());
    return lhs;
  }

  bool match(std::string_view text) const {
    // TODO: can't reuse a NFA right now.  Could make gen static but then it's
    // globally shared.  Could make it a mutable member of Fragment, or drop
    // const on match (which is arguably more accurate since it messes with
    // nodes->gen). Could traverse the states and reset nodes->gen at start of
    // match. Could not use gen at all and maintain a set of added next nodes at
    // the cost of some runtime
    int gen = 1;
    std::vector<Node *> currentNodes{};
    std::vector<Node *> nextNodes{};
    auto queue = [&nextNodes](this auto &&q, Node *s, int gen) {
      if (s == nullptr || s->gen == gen) {
        return;
      };
      s->gen = gen;
      if (s->c == Node::ALTERNATIVE) {
        q(s->next, gen);
        q(s->nextl, gen);
      } else {
        nextNodes.push_back(s);
      }
    };

    queue(start, gen);
    std::swap(nextNodes, currentNodes);
    ++gen;
    nextNodes.clear();
    for (auto c : text) {
      for (auto i = 0uz; i < currentNodes.size(); ++i) {
        auto node = currentNodes[i];
        if (node->c == c) {
          queue(node->next, gen);
        }
      }
      std::swap(nextNodes, currentNodes);
      ++gen;
      nextNodes.clear();
    }

    return std::ranges::any_of(
        currentNodes, [](auto node) { return node->c == Node::ACCEPT; });
  }
};

Fragment NfaBuilder::of(char c) {
  auto start = match(c);
  return {this, start, {&start->next}};
}

Fragment NfaBuilder::of(const Regex &reg) {
  return of(reg.alternative).accept();
}

Fragment NfaBuilder::of(const Alternative &alt) {
  auto fs = alt.concatenations |
            std::views::transform([&](const auto &c) { return of(c); });
  return *std::ranges::fold_left_first(fs, [](auto &&lhs, auto &&rhs) {
    return std::forward<decltype(lhs)>(lhs) + std::forward<decltype(rhs)>(rhs);
  });
}

Fragment NfaBuilder::of(const Concatenation &cat) {
  auto fs = cat.repetitions |
            std::views::transform([&](const auto &c) { return of(c); });
  return *std::ranges::fold_left_first(fs, [](auto &&lhs, auto &&rhs) {
    return std::forward<decltype(lhs)>(lhs) * std::forward<decltype(rhs)>(rhs);
  });
}

Fragment NfaBuilder::of(const Repetition &rep) {
  auto f = rep.isNested ? of(rep.altenative) : of(rep.literal);
  switch (rep.rep) {
  case 0:
    break;
  case '*': {
    f = std::move(f).star();
    break;
  }
  case '+': {
    f = std::move(f)++;
    break;
  }
  case '?': {
    f = std::move(f).maybe();
    break;
  }
  }
  return f;
}

} // namespace

bool match(const Regex &regex, std::string_view text) {
  auto builder = NfaBuilder{};
  auto f = builder.of(regex);
  return f.match(text);
}
} // namespace toyctre
