find_package(wxWidgets 3.2 REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

add_executable(rocketbox
    main.cpp
    main_frame.cpp
    app_icon.cpp
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
    set(_rb_icns "${CMAKE_SOURCE_DIR}/cmake/icons/rocketbox.icns")
    if(EXISTS "${_rb_icns}")
        set_source_files_properties("${_rb_icns}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
        target_sources(rocketbox PRIVATE "${_rb_icns}")
    else()
        message(WARNING "macOS: cmake/icons/rocketbox.icns missing; run scripts/generate-macos-icon.sh")
    endif()
    install(TARGETS rocketbox BUNDLE DESTINATION .)
    install(SCRIPT "${CMAKE_SOURCE_DIR}/cmake/macos_fixup_bundle.cmake")
elseif(WIN32)
    include(${CMAKE_SOURCE_DIR}/cmake/rocketbox_windows_vcpkg.cmake)
    rocketbox_resolve_vcpkg_bin(_rb_vcpkg_bin)

    if(NOT _rb_vcpkg_bin)
        message(FATAL_ERROR "Windows: vcpkg bin directory not found; cannot bundle runtime DLLs")
    endif()

    rocketbox_collect_vcpkg_runtime_dlls("${_rb_vcpkg_bin}" _rb_runtime_dlls)
    rocketbox_assert_vcpkg_runtime_dlls("${_rb_runtime_dlls}" "${_rb_vcpkg_bin}")

    list(LENGTH _rb_runtime_dlls _rb_dll_count)
    message(STATUS "Windows: bundle ${_rb_dll_count} runtime DLL(s) from ${_rb_vcpkg_bin}")

    install(TARGETS rocketbox RUNTIME DESTINATION bin)
    install(FILES ${_rb_runtime_dlls} DESTINATION bin)
    install(FILES "${CMAKE_SOURCE_DIR}/cmake/icons/rocketbox.ico" DESTINATION bin)

    # Post-install sanity check only — no file(GET_RUNTIME_DEPENDENCIES).
    install(CODE [[
        set(_bin "${CMAKE_INSTALL_PREFIX}/bin")
        if(NOT EXISTS "${_bin}/RocketBox.exe")
            message(FATAL_ERROR "Windows install missing RocketBox.exe in ${_bin}")
        endif()
        if(NOT EXISTS "${_bin}/libusb-1.0.dll")
            message(FATAL_ERROR "Windows install missing libusb-1.0.dll in ${_bin}")
        endif()
        file(GLOB _wx "${_bin}/wx*.dll")
        if(NOT _wx)
            message(FATAL_ERROR "Windows install missing wxWidgets DLLs in ${_bin}")
        endif()
        file(GLOB _all "${_bin}/*.dll")
        list(LENGTH _all _n)
        message(STATUS "Windows install verified: RocketBox.exe + ${_n} bundled DLL(s) in ${_bin}")
    ]])

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
        "${CMAKE_SOURCE_DIR}/cmake/icons/rocketbox-256.png"
        DESTINATION share/icons/hicolor/256x256/apps
        RENAME rocketbox.png
    )
    install(FILES
        "${CMAKE_SOURCE_DIR}/cmake/debian/rocketbox.desktop"
        DESTINATION share/applications
    )

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
