# Run during `cmake --install` on Windows to copy vcpkg/wx/libusb DLLs next to RocketBox.exe.
if(NOT WIN32)
    return()
endif()

set(_exe "${CMAKE_INSTALL_PREFIX}/bin/RocketBox.exe")
set(_bindir "${CMAKE_INSTALL_PREFIX}/bin")

if(NOT EXISTS "${_exe}")
    message(FATAL_ERROR "Windows fixup: missing ${_exe}")
endif()

if(NOT DEFINED VCPKG_INSTALLED_DIR)
    get_property(VCPKG_INSTALLED_DIR CACHE VCPKG_INSTALLED_DIR PROPERTY VALUE)
endif()
if(NOT DEFINED VCPKG_TARGET_TRIPLET)
    get_property(VCPKG_TARGET_TRIPLET CACHE VCPKG_TARGET_TRIPLET PROPERTY VALUE)
endif()
if(NOT VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET "x64-windows")
endif()

set(_search_dirs "${_bindir}")
if(VCPKG_INSTALLED_DIR)
    list(APPEND _search_dirs "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
endif()

message(STATUS "Windows fixup: ${_exe}")
message(STATUS "Windows fixup search dirs: ${_search_dirs}")

set(_exclude
    ".*[\\\\]Windows[\\\\]System32[\\\\].*"
    ".*[\\\\]Windows[\\\\]SysWOW64[\\\\].*"
    ".*[\\\\]WinSxS[\\\\].*"
)

file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${_exe}"
    RESOLVED_DEPENDENCIES_VAR _resolved
    UNRESOLVED_DEPENDENCIES_VAR _unresolved
    DIRECTORIES ${_search_dirs}
    POST_EXCLUDE_REGEXES ${_exclude}
)

if(_unresolved)
    message(WARNING "Windows fixup unresolved dependencies: ${_unresolved}")
endif()

foreach(_dll IN LISTS _resolved)
    get_filename_component(_name "${_dll}" NAME)
    set(_dest "${_bindir}/${_name}")
    if(NOT EXISTS "${_dest}")
        message(STATUS "Windows fixup: copy ${_name}")
        file(COPY "${_dll}" DESTINATION "${_bindir}")
    endif()
endforeach()

if(NOT EXISTS "${_bindir}/libusb-1.0.dll")
    message(FATAL_ERROR "Windows fixup: libusb-1.0.dll not bundled into ${_bindir}")
endif()

file(GLOB _wx_dlls "${_bindir}/wx*.dll")
if(NOT _wx_dlls)
    message(FATAL_ERROR "Windows fixup: no wxWidgets DLLs bundled into ${_bindir}")
endif()
