cmake_minimum_required(VERSION 3.21)

# Release CI passes -DROCKETBOX_VERSION / -DROCKETBOX_RELEASE_TAG from the git tag.
if(NOT DEFINED ROCKETBOX_VERSION OR ROCKETBOX_VERSION STREQUAL "")
  set(ROCKETBOX_VERSION "0.1.0")
endif()
if(NOT DEFINED ROCKETBOX_RELEASE_TAG OR ROCKETBOX_RELEASE_TAG STREQUAL "")
  set(ROCKETBOX_RELEASE_TAG "v${ROCKETBOX_VERSION}")
endif()

project(rocketbox VERSION ${ROCKETBOX_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    # Prevent Windows.h min/max macros from breaking std::min/std::max.
    add_compile_definitions(NOMINMAX)
endif()

find_package(Threads REQUIRED)

option(BUILD_WX_GUI "Build RocketBox wxWidgets desktop app" ON)
option(BUILD_TESTS "Build the core/session test suites" ON)

add_subdirectory(core)
add_subdirectory(sim)
add_subdirectory(lib)

if(BUILD_WX_GUI)
    find_package(wxWidgets 3.2 QUIET COMPONENTS core base)
    if(wxWidgets_FOUND)
        add_subdirectory(apps/wx)
    else()
        message(WARNING
            "wxWidgets 3.2+ not found — desktop GUI not built.\n"
            "  Install: sudo apt install libwxgtk3.2-dev\n"
            "  Or configure with -DBUILD_WX_GUI=OFF to silence this.")
    endif()
endif()

add_subdirectory(tools)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
