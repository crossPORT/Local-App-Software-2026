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

# fixup_bundle rewrites dylib load paths; re-sign so macOS will load bundled libraries.
find_program(_CODESIGN codesign REQUIRED)
set(_sign_flags --force --sign - --timestamp=none)

if(IS_DIRECTORY "${_app}/Contents/Frameworks")
    file(GLOB _bundled_libs "${_app}/Contents/Frameworks/*")
    foreach(_lib IN LISTS _bundled_libs)
        if(NOT IS_DIRECTORY "${_lib}")
            message(STATUS "macOS bundle fixup: codesign ${_lib}")
            execute_process(
                COMMAND "${_CODESIGN}" ${_sign_flags} "${_lib}"
                RESULT_VARIABLE _sign_rc
                ERROR_VARIABLE _sign_err
            )
            if(_sign_rc)
                message(FATAL_ERROR "codesign failed for ${_lib}: ${_sign_err}")
            endif()
        endif()
    endforeach()
endif()

message(STATUS "macOS bundle fixup: codesign ${_bin}")
execute_process(
    COMMAND "${_CODESIGN}" ${_sign_flags} "${_bin}"
    RESULT_VARIABLE _sign_rc
    ERROR_VARIABLE _sign_err
)
if(_sign_rc)
    message(FATAL_ERROR "codesign failed for ${_bin}: ${_sign_err}")
endif()

message(STATUS "macOS bundle fixup: codesign ${_app}")
execute_process(
    COMMAND "${_CODESIGN}" ${_sign_flags} "${_app}"
    RESULT_VARIABLE _sign_rc
    ERROR_VARIABLE _sign_err
)
if(_sign_rc)
    message(FATAL_ERROR "codesign failed for ${_app}: ${_sign_err}")
endif()
