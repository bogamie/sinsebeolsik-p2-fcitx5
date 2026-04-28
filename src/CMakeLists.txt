add_library(sinsebeolsik-p2 MODULE
    engine.cpp
)

target_link_libraries(sinsebeolsik-p2
    PRIVATE
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
