find_package(wxWidgets 3.2 REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

add_executable(rocketbox
    main.cpp
    main_frame.cpp
    connection_panel.cpp
    fabric_device_picker.cpp
    roster_panel.cpp
    transfer_progress_panel.cpp
    speed_monitor.cpp
    incoming_dialog.cpp
    settings_dialog.cpp
    event_log_dialog.cpp
)

set_target_properties(rocketbox PROPERTIES
    OUTPUT_NAME RocketBox
)

if(APPLE)
    set_target_properties(rocketbox PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.crossport.rocketbox"
        MACOSX_BUNDLE_BUNDLE_NAME "RocketBox App"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "@executable_path/../Frameworks"
    )
elseif(WIN32)
    # GUI subsystem: wxIMPLEMENT_APP supplies WinMain, not main().
    set_target_properties(rocketbox PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_include_directories(rocketbox PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${wxWidgets_INCLUDE_DIRS}
)

target_link_libraries(rocketbox PRIVATE
    rocketbox_session
    ${wxWidgets_LIBRARIES}
)

target_compile_features(rocketbox PRIVATE cxx_std_17)

if(APPLE)
    install(TARGETS rocketbox BUNDLE DESTINATION .)
    install(SCRIPT "${CMAKE_SOURCE_DIR}/cmake/macos_fixup_bundle.cmake")
elseif(WIN32)
    include(${CMAKE_SOURCE_DIR}/cmake/rocketbox_windows_vcpkg.cmake)
    rocketbox_resolve_vcpkg_bin(_rb_vcpkg_bin)

    install(TARGETS rocketbox RUNTIME DESTINATION bin)

    if(_rb_vcpkg_bin)
        file(GLOB _rb_primary_dlls
            "${_rb_vcpkg_bin}/wx*.dll"
            "${_rb_vcpkg_bin}/libusb-1.0.dll"
        )
        if(_rb_primary_dlls)
            install(FILES ${_rb_primary_dlls} DESTINATION bin)
            list(LENGTH _rb_primary_dlls _rb_primary_count)
            message(STATUS "Windows: install ${_rb_primary_count} primary DLL(s) from ${_rb_vcpkg_bin}")
        else()
            message(FATAL_ERROR "Windows: no wx/libusb DLLs found in ${_rb_vcpkg_bin}")
        endif()
    else()
        message(FATAL_ERROR "Windows: vcpkg bin directory not found; cannot bundle runtime DLLs")
    endif()

    set(ROCKETBOX_VCPKG_BIN "${_rb_vcpkg_bin}")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/windows_fixup_install.cmake.in"
        "${CMAKE_BINARY_DIR}/generated/windows_fixup_install.cmake"
        @ONLY
    )
    install(SCRIPT "${CMAKE_BINARY_DIR}/generated/windows_fixup_install.cmake")

    install(FILES
        ${CMAKE_SOURCE_DIR}/99-sls-fabric-usb.rules
        ${CMAKE_SOURCE_DIR}/cmake/RUNNING.txt
        DESTINATION share/rocketbox
    )

    install(DIRECTORY ${CMAKE_SOURCE_DIR}/demo-config/
        DESTINATION share/rocketbox/demo-config
        FILES_MATCHING PATTERN "*.conf" PATTERN "README.txt"
    )
else()
    install(TARGETS rocketbox RUNTIME DESTINATION bin)

    install(FILES
        ${CMAKE_SOURCE_DIR}/99-sls-fabric-usb.rules
        ${CMAKE_SOURCE_DIR}/cmake/RUNNING.txt
        DESTINATION share/rocketbox
    )

    install(DIRECTORY ${CMAKE_SOURCE_DIR}/demo-config/
        DESTINATION share/rocketbox/demo-config
        FILES_MATCHING PATTERN "*.conf" PATTERN "README.txt"
    )
endif()

if(UNIX AND NOT APPLE)
    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
        "${CMAKE_SOURCE_DIR}/cmake/debian/postinst;")
endif()

include(${CMAKE_SOURCE_DIR}/cmake/packaging.cmake)
