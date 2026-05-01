# Catch2: 시스템 패키지가 있으면 그걸, 없으면 FetchContent로 받는다.
find_package(Catch2 3 QUIET)
if(NOT Catch2_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.5.4
    )
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
endif()

include(Catch)

add_executable(jamo_test jamo_test.cpp)
target_link_libraries(jamo_test PRIVATE sin3p2_core Catch2::Catch2WithMain)
target_compile_features(jamo_test PRIVATE cxx_std_20)
catch_discover_tests(jamo_test)

add_executable(automaton_test automaton_test.cpp)
target_link_libraries(automaton_test PRIVATE sin3p2_core Catch2::Catch2WithMain)
target_compile_features(automaton_test PRIVATE cxx_std_20)
catch_discover_tests(automaton_test)

add_executable(keymap_test keymap_test.cpp)
target_link_libraries(keymap_test PRIVATE sin3p2_core Catch2::Catch2WithMain)
target_compile_features(keymap_test PRIVATE cxx_std_20)
catch_discover_tests(keymap_test)

add_executable(qwerty_translator_test qwerty_translator_test.cpp)
target_link_libraries(qwerty_translator_test PRIVATE sin3p2_core Catch2::Catch2WithMain)
target_compile_features(qwerty_translator_test PRIVATE cxx_std_20)
catch_discover_tests(qwerty_translator_test)
