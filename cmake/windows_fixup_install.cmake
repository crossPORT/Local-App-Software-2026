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

set(_vcpkg_bin "")
if(VCPKG_INSTALLED_DIR)
    set(_vcpkg_bin "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
endif()

message(STATUS "Windows fixup: ${_exe}")
message(STATUS "Windows fixup vcpkg bin: ${_vcpkg_bin}")

set(_exclude
    ".*[\\\\]Windows[\\\\]System32[\\\\].*"
    ".*[\\\\]Windows[\\\\]SysWOW64[\\\\].*"
    ".*[\\\\]WinSxS[\\\\].*"
)

function(_rocketbox_copy_runtime_deps)
    set(options LIBRARIES)
    set(oneValueArgs)
    set(multiValueArgs FILES)
    cmake_parse_arguments(_ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT _ARG_FILES)
        return()
    endif()

    set(_search_dirs "${_bindir}")
    if(_vcpkg_bin AND IS_DIRECTORY "${_vcpkg_bin}")
        list(APPEND _search_dirs "${_vcpkg_bin}")
    endif()

    if(_ARG_LIBRARIES)
        file(GET_RUNTIME_DEPENDENCIES
            LIBRARIES ${_ARG_FILES}
            RESOLVED_DEPENDENCIES_VAR _resolved
            UNRESOLVED_DEPENDENCIES_VAR _unresolved
            DIRECTORIES ${_search_dirs}
            POST_EXCLUDE_REGEXES ${_exclude}
        )
    else()
        file(GET_RUNTIME_DEPENDENCIES
            EXECUTABLES ${_ARG_FILES}
            RESOLVED_DEPENDENCIES_VAR _resolved
            UNRESOLVED_DEPENDENCIES_VAR _unresolved
            DIRECTORIES ${_search_dirs}
            POST_EXCLUDE_REGEXES ${_exclude}
        )
    endif()

    if(_unresolved)
        message(STATUS "Windows fixup unresolved (may be optional): ${_unresolved}")
    endif()

    foreach(_dll IN LISTS _resolved)
        get_filename_component(_name "${_dll}" NAME)
        set(_dest "${_bindir}/${_name}")
        if(NOT EXISTS "${_dest}")
            message(STATUS "Windows fixup: copy ${_name}")
            file(COPY "${_dll}" DESTINATION "${_bindir}")
        endif()
    endforeach()
endfunction()

# Pass 1: dependencies of RocketBox.exe
_rocketbox_copy_runtime_deps(FILES "${_exe}")

# Pass 2: explicit vcpkg wx/libusb (covers cases GET_RUNTIME_DEPENDENCIES misses)
if(_vcpkg_bin AND IS_DIRECTORY "${_vcpkg_bin}")
    file(GLOB _vcpkg_dlls
        "${_vcpkg_bin}/wx*.dll"
        "${_vcpkg_bin}/libusb-1.0.dll"
    )
    foreach(_dll IN LISTS _vcpkg_dlls)
        get_filename_component(_name "${_dll}" NAME)
        if(NOT EXISTS "${_bindir}/${_name}")
            message(STATUS "Windows fixup: copy ${_name} (vcpkg)")
            file(COPY "${_dll}" DESTINATION "${_bindir}")
        endif()
    endforeach()
endif()

# Pass 3: transitive deps of bundled DLLs (zlib, png, etc.)
file(GLOB _bundled_dlls "${_bindir}/*.dll")
if(_bundled_dlls)
    _rocketbox_copy_runtime_deps(LIBRARIES FILES ${_bundled_dlls})
endif()

if(NOT EXISTS "${_bindir}/libusb-1.0.dll")
    message(FATAL_ERROR "Windows fixup: libusb-1.0.dll not bundled into ${_bindir}")
endif()

file(GLOB _wx_dlls "${_bindir}/wx*.dll")
if(NOT _wx_dlls)
    message(FATAL_ERROR "Windows fixup: no wxWidgets DLLs bundled into ${_bindir}")
endif()

file(GLOB _all_dlls "${_bindir}/*.dll")
list(LENGTH _all_dlls _dll_count)
message(STATUS "Windows fixup: ${_dll_count} DLL(s) in ${_bindir}")
