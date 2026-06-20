add_executable(usb-probe usb_probe.cpp)
target_link_libraries(usb-probe PRIVATE fabric_usb_core)
target_compile_features(usb-probe PRIVATE cxx_std_17)

add_executable(usb-loopback-test usb_loopback_test.cpp)
target_link_libraries(usb-loopback-test PRIVATE fabric_usb_core)
target_compile_features(usb-loopback-test PRIVATE cxx_std_17)

add_executable(usb-pair-test usb_pair_test.cpp)
target_link_libraries(usb-pair-test PRIVATE fabric_usb_core)
target_compile_features(usb-pair-test PRIVATE cxx_std_17)

add_executable(fabric-session-test fabric_session_test.cpp)
target_link_libraries(fabric-session-test PRIVATE sls_demo_logic)
target_compile_features(fabric-session-test PRIVATE cxx_std_17)

add_executable(booth-cli booth_cli.cpp)
target_link_libraries(booth-cli PRIVATE sls_demo_logic)
target_compile_features(booth-cli PRIVATE cxx_std_17)
