add_library(rocketbox_session STATIC
    platform_util.cpp
    transfer_controller.cpp
    session_config.cpp
    session_role.cpp
    fabric_meta_file.cpp
    fabric_tar_pack.cpp
    fabric_session_message.cpp
    announce_note.cpp
    link_status.cpp
    identity_profile.cpp
    booth_display.cpp
    peer_roster.cpp
    session_listener.cpp
    session_handshake.cpp
    transfer_orchestrator.cpp
    booth_log.cpp
    receive_payload.cpp
)

target_include_directories(rocketbox_session PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(rocketbox_session PUBLIC
    fabric_usb_core
    fabric_usb_sim
    Threads::Threads
)

target_compile_features(rocketbox_session PUBLIC cxx_std_17)
