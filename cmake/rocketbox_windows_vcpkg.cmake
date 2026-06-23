# Resolve the vcpkg runtime bin directory at configure time (reliable on Windows CI).
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
