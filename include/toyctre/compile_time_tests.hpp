#include "compile_time.hpp"

using namespace toyctre::compile_time::detail::match;
using namespace toyctre::compile_time::detail::parse_tree;

namespace typelist_match_tests {

using lita = Matcher<Literal<true, 'a'>>;
static_assert(lita::match("a"));
static_assert(lita::match("abc") == false);

using groupa = Matcher<Group<Literal<true, 'a'>>>;
static_assert(groupa::match("a"));

using seqab = Matcher<Sequence<Literal<true, 'a'>, Literal<true, 'b'>>>;
static_assert(seqab::match("ab"));
static_assert(seqab::match("abc") == false);

using altab = Matcher<Alternatives<Literal<true, 'a'>, Literal<true, 'b'>>>;
static_assert(altab::match("a"));
static_assert(altab::match("b"));
static_assert(altab::match("abc") == false);
static_assert(altab::match("ba") == false);

using optionalab =
    Matcher<Sequence<Literal<true, 'a'>, Repetition<Literal<true, 'b'>, '?'>>>;
static_assert(optionalab::match("a"));
static_assert(optionalab::match("ab"));
static_assert(optionalab::match("abc") == false);
static_assert(optionalab::match("ac") == false);
static_assert(optionalab::match("") == false);

using stara = Matcher<Repetition<Literal<true, 'a'>, '*'>>;
static_assert(stara::match("a"));
static_assert(stara::match("aaaaaaaaa"));
static_assert(stara::match("abc") == false);
static_assert(stara::match("ac") == false);
static_assert(stara::match(""));

} // namespace typelist_match_tests
