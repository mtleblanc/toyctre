add_library(core STATIC
    lib.cpp
    matcher.cpp
)

target_include_directories(core PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
