# Run during `cmake --install` on macOS to copy wx/libusb dylibs into RocketBox.app.
if(NOT APPLE)
    return()
endif()

include(BundleUtilities)

set(_app "${CMAKE_INSTALL_PREFIX}/RocketBox.app")
set(_bin "${_app}/Contents/MacOS/RocketBox")

if(NOT EXISTS "${_bin}")
    message(FATAL_ERROR "macOS bundle fixup: missing ${_bin}")
endif()

set(_extra_dirs "")
foreach(_root IN ITEMS "/opt/homebrew" "/usr/local")
    if(EXISTS "${_root}/lib")
        list(APPEND _extra_dirs "${_root}/lib")
    endif()
    if(EXISTS "${_root}/opt/libusb/lib")
        list(APPEND _extra_dirs "${_root}/opt/libusb/lib")
    endif()
    if(EXISTS "${_root}/opt/wxwidgets/lib")
        list(APPEND _extra_dirs "${_root}/opt/wxwidgets/lib")
    endif()
endforeach()

message(STATUS "macOS bundle fixup: ${_app}")
message(STATUS "macOS bundle fixup search dirs: ${_extra_dirs}")

fixup_bundle("${_app}" "" "${_extra_dirs}")
