find_package(wxWidgets 3.2 REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

add_executable(data-transfer-demo
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

set_target_properties(data-transfer-demo PROPERTIES
    OUTPUT_NAME RocketBox
)

if(APPLE)
    set_target_properties(data-transfer-demo PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.crossport.rocketbox"
        MACOSX_BUNDLE_BUNDLE_NAME "RocketBox"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
endif()

target_include_directories(data-transfer-demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${wxWidgets_INCLUDE_DIRS}
)

target_link_libraries(data-transfer-demo PRIVATE
    sls_demo_logic
    ${wxWidgets_LIBRARIES}
)

target_compile_features(data-transfer-demo PRIVATE cxx_std_17)

if(APPLE)
    install(TARGETS data-transfer-demo BUNDLE DESTINATION .)
else()
    install(TARGETS data-transfer-demo RUNTIME DESTINATION bin)
endif()

install(FILES
    ${CMAKE_SOURCE_DIR}/booth-port0.conf
    ${CMAKE_SOURCE_DIR}/booth-port1.conf
    ${CMAKE_SOURCE_DIR}/99-sls-fabric-usb.rules
    ${CMAKE_SOURCE_DIR}/cmake/RUNNING.txt
    DESTINATION share/rocketbox
)

if(UNIX AND NOT APPLE)
    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
        "${CMAKE_SOURCE_DIR}/cmake/debian/postinst;")
endif()

include(${CMAKE_SOURCE_DIR}/cmake/packaging.cmake)
