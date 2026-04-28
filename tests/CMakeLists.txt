# Catch2 v3 via FetchContent. CI runners on ubuntu-24.04 / 26.04 have
# internet access; if a system Catch2 ever lands in apt with a v3 package,
# we can switch to find_package(Catch2 3 QUIET) and fall back here.

include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(Catch)

add_executable(jamo_test jamo_test.cpp)
target_link_libraries(jamo_test PRIVATE
    sinsebeolsik-p2-core
    Catch2::Catch2WithMain
)
catch_discover_tests(jamo_test)
