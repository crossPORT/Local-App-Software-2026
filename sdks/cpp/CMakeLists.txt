find_package(Threads REQUIRED)

add_library(rocketbox_sdk STATIC
    src/sdk.cpp
    src/sdk_defaults.cpp
    src/port_probe.cpp
    src/usb_plane.cpp
    src/usb_plane_files.cpp
)

target_include_directories(rocketbox_sdk
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(rocketbox_sdk
    PUBLIC
        rocketbox_session
        Threads::Threads
)

target_compile_definitions(rocketbox_sdk PUBLIC ROCKETBOX_USB_SUPPORT=1)
target_compile_features(rocketbox_sdk PUBLIC cxx_std_17)
