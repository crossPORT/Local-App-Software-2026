cmake_minimum_required(VERSION 3.21)

# Release CI passes -DROCKETBOX_VERSION / -DROCKETBOX_RELEASE_TAG from the git tag.
# Local builds: prefer git describe so binaries still show a useful tag.
if(NOT DEFINED ROCKETBOX_VERSION OR ROCKETBOX_VERSION STREQUAL "")
  execute_process(
    COMMAND git describe --tags --always --dirty
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE _rb_git_desc
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  if(_rb_git_desc)
    if(NOT DEFINED ROCKETBOX_RELEASE_TAG OR ROCKETBOX_RELEASE_TAG STREQUAL "")
      set(ROCKETBOX_RELEASE_TAG "${_rb_git_desc}")
    endif()
    string(REGEX REPLACE "^v" "" _rb_raw "${_rb_git_desc}")
    string(REGEX MATCH "^[0-9]+(\\.[0-9]+){0,2}" ROCKETBOX_VERSION "${_rb_raw}")
  endif()
  if(NOT ROCKETBOX_VERSION)
    set(ROCKETBOX_VERSION "0.1.0")
  endif()
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

include("${CMAKE_SOURCE_DIR}/cmake/rocketbox_version.cmake")

find_package(Threads REQUIRED)

option(BUILD_WX_GUI "Build RocketBox wxWidgets desktop app" ON)
option(BUILD_TESTS "Build the core/session test suites" ON)

add_subdirectory(sdks/cpp/core)
add_subdirectory(sdks/cpp/sim)
add_subdirectory(sdks/cpp/session)
add_subdirectory(sdks/cpp)
add_subdirectory(rocketbox-tunnel)

if(BUILD_WX_GUI)
    find_package(wxWidgets 3.2 QUIET COMPONENTS core base)
    if(wxWidgets_FOUND)
        add_subdirectory(apps/wx)
        add_subdirectory(apps/tunnel-tray)
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
