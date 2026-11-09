add_executable(toyctre_tests
  parser_test.cpp
  matcher_test.cpp
)

target_link_libraries(toyctre_tests PRIVATE core Catch2::Catch2WithMain)

include(Catch)
# Tests tagged [slow] only run in non-Debug builds. They can still be run
# directly with: toyctre_tests "[slow]"
if(CMAKE_BUILD_TYPE_UPPER STREQUAL "DEBUG")
  catch_discover_tests(toyctre_tests TEST_SPEC "~[slow]")
else()
  catch_discover_tests(toyctre_tests)
endif()

include(ct_parser_cases.cmake)
