add_executable(usb-probe usb_probe.cpp)
target_link_libraries(usb-probe PRIVATE rocketbox_usb_core)
target_compile_features(usb-probe PRIVATE cxx_std_17)

add_executable(usb-switch usb_switch.cpp)
target_link_libraries(usb-switch PRIVATE rocketbox_usb_core)
target_compile_features(usb-switch PRIVATE cxx_std_17)

add_executable(usb-loopback-test usb_loopback_test.cpp)
target_link_libraries(usb-loopback-test PRIVATE rocketbox_usb_core)
target_compile_features(usb-loopback-test PRIVATE cxx_std_17)

add_executable(usb-pair-test usb_pair_test.cpp)
target_link_libraries(usb-pair-test PRIVATE rocketbox_usb_core)
target_compile_features(usb-pair-test PRIVATE cxx_std_17)

add_executable(session-test session_test.cpp)
target_link_libraries(session-test PRIVATE rocketbox_sdk)
target_compile_features(session-test PRIVATE cxx_std_17)

add_executable(rocketbox-cli rocketbox_cli.cpp)
target_link_libraries(rocketbox-cli PRIVATE rocketbox_sdk)
target_compile_features(rocketbox-cli PRIVATE cxx_std_17)

add_executable(send-repro send_repro.cpp)
target_link_libraries(send-repro PRIVATE rocketbox_sdk)
target_compile_features(send-repro PRIVATE cxx_std_17)