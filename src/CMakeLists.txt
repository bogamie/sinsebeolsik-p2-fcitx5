# tomlplusplus — 시스템 패키지(libtomlplusplus-dev)가 있으면 그걸,
# 없으면 FetchContent로 받는다. (CI / 깨끗한 환경에서도 빌드되도록)
find_package(tomlplusplus QUIET)
if(NOT tomlplusplus_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        tomlplusplus
        GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
        GIT_TAG        v3.4.0
    )
    FetchContent_MakeAvailable(tomlplusplus)
endif()

# 키맵 TOML을 빌드 시점에 raw string literal로 임베드해 fallback default를 만든다.
# 사용자 override 파일이 발견되면 엔진이 런타임에 그걸로 교체한다.
set(P2_KEYMAP_SOURCE "${CMAKE_SOURCE_DIR}/keymaps/sinsebeolsik_p2.toml")
file(READ "${P2_KEYMAP_SOURCE}" P2_KEYMAP_TOML_CONTENT)
configure_file(embedded_p2_keymap.cpp.in
               "${CMAKE_CURRENT_BINARY_DIR}/embedded_p2_keymap.cpp" @ONLY)
# 키맵이 바뀌면 재구성되도록 — configure_file은 입력 파일만 추적하고
# `file(READ)`의 입력은 안 잡으므로 명시.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${P2_KEYMAP_SOURCE}")

# 자동기 / 자모 / keymap 로직은 fcitx5 의존성 없는 정적 라이브러리로 분리.
# addon 모듈과 단위 테스트 모두 여기에 링크한다.
add_library(sin3p2_core STATIC
    jamo.cpp
    automaton.cpp
    keymap.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/embedded_p2_keymap.cpp"
)
target_include_directories(sin3p2_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(sin3p2_core PUBLIC cxx_std_20)
target_link_libraries(sin3p2_core PRIVATE tomlplusplus::tomlplusplus)
# 정적 lib을 SHARED MODULE에 링크하려면 -fPIC 필요
set_property(TARGET sin3p2_core PROPERTY POSITION_INDEPENDENT_CODE ON)

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

# 키맵 파일은 사용자 override 가능 위치에 설치. 엔진이 기본적으로 여기서 찾는다.
install(FILES "${P2_KEYMAP_SOURCE}"
    DESTINATION "${FCITX_INSTALL_PKGDATADIR}/sinsebeolsik-p2")
