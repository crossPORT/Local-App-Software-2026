add_library(rocketbox_session STATIC
    port_util.cpp
    session_rate_stats.cpp
    platform_util.cpp
    transfer_controller.cpp
    session_config.cpp
    session_role.cpp
    meta_file.cpp
    tar_pack.cpp
    session_message.cpp
    announce_note.cpp
    link_status.cpp
    identity_profile.cpp
    display_rate.cpp
    peer_roster.cpp
    session_listener.cpp
    session_handshake.cpp
    transfer_orchestrator.cpp
    event_log.cpp
    receive_payload.cpp
)

target_include_directories(rocketbox_session PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/sdks/cpp/include
)

target_link_libraries(rocketbox_session PUBLIC
    rocketbox_usb_core
    rocketbox_usb_sim
    Threads::Threads
)

target_compile_features(rocketbox_session PUBLIC cxx_std_17)
