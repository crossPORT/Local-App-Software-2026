add_library(sls_demo_logic STATIC
    transfer_controller.cpp
    demo_config.cpp
    session_role.cpp
    fabric_meta_file.cpp
    fabric_tar_pack.cpp
    fabric_session_message.cpp
    announce_note.cpp
    link_status.cpp
    identity_profile.cpp
    demo_display.cpp
    peer_roster.cpp
    session_listener.cpp
    session_handshake.cpp
    transfer_orchestrator.cpp
    booth_log.cpp
    receive_payload.cpp
)

target_include_directories(sls_demo_logic PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sls_demo_logic PUBLIC
    fabric_usb_core
    fabric_usb_sim
    Threads::Threads
)

target_compile_features(sls_demo_logic PUBLIC cxx_std_17)

add_executable(send-repro
    ${CMAKE_SOURCE_DIR}/tools/send_repro.cpp
)
target_link_libraries(send-repro PRIVATE sls_demo_logic)
