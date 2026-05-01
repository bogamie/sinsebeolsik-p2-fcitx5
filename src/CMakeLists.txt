# 자동기 / 자모 로직은 fcitx5 의존성 없는 정적 라이브러리로 분리.
# addon 모듈과 단위 테스트 모두 여기에 링크한다.
add_library(sin3p2_core STATIC
    jamo.cpp
    automaton.cpp
    keymap.cpp
)
target_include_directories(sin3p2_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(sin3p2_core PUBLIC cxx_std_20)

add_library(sinsebeolsik-p2 MODULE
    engine.cpp
)

target_link_libraries(sinsebeolsik-p2
    PRIVATE
        sin3p2_core
        Fcitx5::Core
        fmt::fmt
)

set_target_properties(sinsebeolsik-p2 PROPERTIES
    PREFIX "lib"
)

configure_file(addon.conf.in sinsebeolsik-p2.conf @ONLY)

install(TARGETS sinsebeolsik-p2
    DESTINATION "${FCITX_INSTALL_ADDONDIR}")

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/sinsebeolsik-p2.conf"
    DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon")
