add_executable(data-transfer-demo
    main.cpp
    main_window.cpp
    transfer_controller.cpp
    fabric_meta_file.cpp
    fabric_tar_pack.cpp
    demo_config.cpp
    session_role.cpp
    demo_styles.cpp
)

target_include_directories(data-transfer-demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)

target_link_libraries(data-transfer-demo PRIVATE
    fabric_usb_core
    PkgConfig::GTK3
)

target_compile_features(data-transfer-demo PRIVATE cxx_std_17)

add_executable(send-repro
    ${CMAKE_SOURCE_DIR}/tools/send_repro.cpp
    transfer_controller.cpp
    fabric_meta_file.cpp
)
target_include_directories(send-repro PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(send-repro PRIVATE fabric_usb_core pthread)
target_compile_features(send-repro PRIVATE cxx_std_17)
