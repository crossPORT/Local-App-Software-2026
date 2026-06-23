# Windows runtime DLL helpers (vcpkg only — never scan System32 or install/bin).

function(rocketbox_resolve_vcpkg_bin out_var)
    if(NOT VCPKG_TARGET_TRIPLET)
        set(_triplet "x64-windows")
    else()
        set(_triplet "${VCPKG_TARGET_TRIPLET}")
    endif()

    set(_candidates "")
    if(VCPKG_INSTALLED_DIR)
        list(APPEND _candidates "${VCPKG_INSTALLED_DIR}/${_triplet}/bin")
    endif()
    list(APPEND _candidates
        "${CMAKE_BINARY_DIR}/vcpkg_installed/${_triplet}/bin"
        "${CMAKE_SOURCE_DIR}/vcpkg_installed/${_triplet}/bin"
    )

    set(_found "")
    foreach(_dir IN LISTS _candidates)
        if(_dir AND IS_DIRECTORY "${_dir}")
            set(_found "${_dir}")
            break()
        endif()
    endforeach()

    set(${out_var} "${_found}" PARENT_SCOPE)
endfunction()

# Allowlisted filename patterns under vcpkg bin only (no dependency walker, no *.dll sweep).
function(rocketbox_collect_vcpkg_runtime_dlls vcpkg_bin out_var)
    set(_patterns
        "libusb-1.0.dll"
        "wx*.dll"
        "zlib*.dll"
        "libpng*.dll"
        "jpeg*.dll"
        "tiff*.dll"
        "libwebp*.dll"
        "libsharpyuv*.dll"
        "pcre2*.dll"
        "pcre*.dll"
        "expat*.dll"
    )

    set(_collected "")
    if(vcpkg_bin AND IS_DIRECTORY "${vcpkg_bin}")
        foreach(_pattern IN LISTS _patterns)
            file(GLOB _matches "${vcpkg_bin}/${_pattern}")
            list(APPEND _collected ${_matches})
        endforeach()
    endif()

    if(_collected)
        list(REMOVE_DUPLICATES _collected)
    endif()

    set(${out_var} "${_collected}" PARENT_SCOPE)
endfunction()

function(rocketbox_assert_vcpkg_runtime_dlls dll_list vcpkg_bin)
    set(_has_libusb FALSE)
    set(_has_wx FALSE)
    foreach(_dll IN LISTS dll_list)
        get_filename_component(_name "${_dll}" NAME)
        if(_name STREQUAL "libusb-1.0.dll")
            set(_has_libusb TRUE)
        endif()
        if(_name MATCHES "^wx.*\\.dll$")
            set(_has_wx TRUE)
        endif()
    endforeach()

    if(NOT _has_libusb)
        message(FATAL_ERROR "Windows: libusb-1.0.dll not found in ${vcpkg_bin}")
    endif()
    if(NOT _has_wx)
        message(FATAL_ERROR "Windows: no wx*.dll found in ${vcpkg_bin}")
    endif()
endfunction()
