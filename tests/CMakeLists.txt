add_executable(toyctre_tests
  placeholder_test.cpp
)

target_link_libraries(toyctre_tests PRIVATE core Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(toyctre_tests)
