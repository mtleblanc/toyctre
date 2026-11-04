
#include <catch2/catch_test_macros.hpp>

namespace toyctre {
TEST_CASE("Placeholder", "[placeholder]") {
  SECTION("some test") {
    CHECK(true);
    REQUIRE(true);
  }
}
} // namespace toyctre
