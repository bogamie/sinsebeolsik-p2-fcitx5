cmake_minimum_required(VERSION 3.16)
project(sinsebeolsik-p2-fcitx5
    VERSION 0.1.0
    DESCRIPTION "Sinsebeolsik P2 Hangul input method for fcitx5"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(ECM 1.0.0 REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(GNUInstallDirs)
include(FeatureSummary)
include(ECMUninstallTarget)

find_package(Fcitx5Core 5.0.0 REQUIRED)
find_package(fmt REQUIRED)
find_package(Gettext REQUIRED)

set(GETTEXT_DOMAIN "fcitx5-sinsebeolsik-p2")
add_definitions(-DFCITX_GETTEXT_DOMAIN=\"${GETTEXT_DOMAIN}\")

add_subdirectory(src)

option(SIN3P2_BUILD_TESTS "Build unit tests for sinsebeolsik-p2" ON)
if(SIN3P2_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
