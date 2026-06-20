find_package(wxWidgets 3.2 REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

add_executable(data-transfer-demo
    main.cpp
    main_frame.cpp
    roster_panel.cpp
    transfer_progress_panel.cpp
    incoming_dialog.cpp
    settings_dialog.cpp
    event_log_dialog.cpp
)

target_include_directories(data-transfer-demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${wxWidgets_INCLUDE_DIRS}
)

target_link_libraries(data-transfer-demo PRIVATE
    sls_demo_logic
    ${wxWidgets_LIBRARIES}
)

target_compile_features(data-transfer-demo PRIVATE cxx_std_17)
