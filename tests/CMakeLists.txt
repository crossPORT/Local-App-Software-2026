# Test suites for the reusable core + session logic. Kept independent of any GUI
# so GUI work can rely on a validated foundation.
#
#   ctest --test-dir build -L unit          # pure logic, no hardware (CI-friendly)
#   ctest --test-dir build -L integration   # rocketbox_sim + TransferOrchestrator handshake
#   ctest --test-dir build -L hardware      # needs both USB cables; auto-skips otherwise
#   ctest --test-dir build                  # everything

set(RB_TEST_FIXTURE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

# --- Unit suite: pure logic, no USB hardware required ---------------------
add_executable(unit-tests
    test_util.cpp
    unit_main.cpp
    unit/test_session_message.cpp
    unit/test_session_fixtures.cpp
    unit/test_announce_note.cpp
    unit/test_link_status.cpp
    unit/test_meta_file.cpp
    unit/test_identity_profile.cpp
    unit/test_session_handshake.cpp
    unit/test_display_rate.cpp
    unit/test_session_config.cpp
    unit/test_peer_roster.cpp
    unit/test_session_role.cpp
    unit/test_core_tuning.cpp
    unit/test_receive_payload.cpp
    unit/test_link_policy.cpp
    unit/test_rocketbox_sim.cpp
    unit/test_tar_pack.cpp
    unit/test_session_rate_stats.cpp
    unit/test_port_util.cpp
    unit/test_port_probe.cpp
)
target_include_directories(unit-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(unit-tests PRIVATE
    RB_TEST_FIXTURE_DIR="${RB_TEST_FIXTURE_DIR}")
target_link_libraries(unit-tests PRIVATE rocketbox_sdk)
target_compile_features(unit-tests PRIVATE cxx_std_17)

add_test(NAME unit-tests COMMAND unit-tests)
set_tests_properties(unit-tests PROPERTIES LABELS "unit")

# --- Integration suite: full session handshake over rocketbox_sim ------------
add_executable(integration-tests
    test_util.cpp
    integration_main.cpp
    integration/test_session_handshake.cpp
)
target_include_directories(integration-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(integration-tests PRIVATE
    RB_TEST_FIXTURE_DIR="${RB_TEST_FIXTURE_DIR}")
target_link_libraries(integration-tests PRIVATE rocketbox_sdk)
target_compile_features(integration-tests PRIVATE cxx_std_17)

add_test(NAME integration-tests COMMAND integration-tests)
set_tests_properties(integration-tests PROPERTIES LABELS "integration")

add_executable(sdk-integration-tests
    integration/test_new_sdk.cpp
)
target_link_libraries(sdk-integration-tests PRIVATE rocketbox_sdk)
target_compile_features(sdk-integration-tests PRIVATE cxx_std_17)

add_test(NAME sdk-integration-tests COMMAND sdk-integration-tests)
set_tests_properties(sdk-integration-tests PROPERTIES LABELS "integration")

# --- Hardware suite: real fabric transfers; skips cleanly without devices --
add_executable(hardware-tests
    test_util.cpp
    hardware_main.cpp
    hardware/test_transfer_hw.cpp
)
target_include_directories(hardware-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/hardware
)
target_link_libraries(hardware-tests PRIVATE rocketbox_usb_core)
target_compile_features(hardware-tests PRIVATE cxx_std_17)

add_test(NAME hardware-tests COMMAND hardware-tests)
set_tests_properties(hardware-tests PROPERTIES
    LABELS "hardware"
    SKIP_RETURN_CODE 77
)
