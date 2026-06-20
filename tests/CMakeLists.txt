# Test suites for the reusable core + session logic. Kept independent of any GUI
# so GUI work can rely on a validated foundation.
#
#   ctest --test-dir build -L unit       # fast, no hardware (CI-friendly)
#   ctest --test-dir build -L hardware   # needs both USB cables; auto-skips otherwise
#   ctest --test-dir build               # everything

# --- Unit suite: pure logic, no USB hardware required ---------------------
add_executable(unit-tests
    test_util.cpp
    unit_main.cpp
    unit/test_session_message.cpp
    unit/test_meta_file.cpp
    unit/test_identity_profile.cpp
    unit/test_demo_config.cpp
    unit/test_peer_roster.cpp
    unit/test_session_role.cpp
    unit/test_core_tuning.cpp
    unit/test_receive_payload.cpp
    unit/test_fabric_link_policy.cpp
)
target_include_directories(unit-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(unit-tests PRIVATE sls_demo_logic)
target_compile_features(unit-tests PRIVATE cxx_std_17)

add_test(NAME unit-tests COMMAND unit-tests)
set_tests_properties(unit-tests PROPERTIES LABELS "unit")

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
target_link_libraries(hardware-tests PRIVATE fabric_usb_core)
target_compile_features(hardware-tests PRIVATE cxx_std_17)

add_test(NAME hardware-tests COMMAND hardware-tests)
set_tests_properties(hardware-tests PROPERTIES
    LABELS "hardware"
    SKIP_RETURN_CODE 77
)
