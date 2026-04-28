# Pure-logic core: jamo, automaton (next), keymap loader (later).
# No fcitx5/fmt deps — unit-testable in isolation.
add_library(sinsebeolsik-p2-core STATIC
    jamo.cpp
)

target_include_directories(sinsebeolsik-p2-core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(sinsebeolsik-p2-core PUBLIC cxx_std_20)

# Required because the core static lib is linked into the MODULE addon.
set_target_properties(sinsebeolsik-p2-core PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

# fcitx5 addon module. Pulls in the core lib for composition logic.
add_library(sinsebeolsik-p2 MODULE
    engine.cpp
)

target_link_libraries(sinsebeolsik-p2
    PRIVATE
        sinsebeolsik-p2-core
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
