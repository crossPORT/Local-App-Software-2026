option(ROCKETBOX_BUILD_SIM "Include simulation support in compiled SDK" ON)
option(ROCKETBOX_BUILD_USB "Include USB transport in compiled SDK" ON)

find_package(Threads REQUIRED)

set(ROCKETBOX_SDK_SOURCES
    src/sdk.cpp
    src/sdk_defaults.cpp
    src/port_probe.cpp
)

if (ROCKETBOX_BUILD_SIM)
    list(APPEND ROCKETBOX_SDK_SOURCES
        src/sim_plane.cpp
        src/sim_plane_io.cpp
        src/sim_net.cpp
    )
endif()

if (ROCKETBOX_BUILD_USB)
    list(APPEND ROCKETBOX_SDK_SOURCES
        src/usb_plane.cpp
        src/usb_plane_circuit.cpp
        src/usb_plane_files.cpp
        src/usb_plane_listen.cpp
    )
endif()

add_library(rocketbox_sdk STATIC ${ROCKETBOX_SDK_SOURCES})

target_include_directories(rocketbox_sdk
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/session
        ${CMAKE_CURRENT_SOURCE_DIR}/core/include
)

target_link_libraries(rocketbox_sdk
    PUBLIC
        rocketbox_session
        Threads::Threads
)

if (ROCKETBOX_BUILD_SIM)
    target_compile_definitions(rocketbox_sdk PUBLIC ROCKETBOX_SIM_SUPPORT=1)
    if (WIN32)
        target_link_libraries(rocketbox_sdk PRIVATE ws2_32)
    endif()
endif()

if (ROCKETBOX_BUILD_USB)
    target_compile_definitions(rocketbox_sdk PUBLIC ROCKETBOX_USB_SUPPORT=1)
endif()

target_compile_features(rocketbox_sdk PUBLIC cxx_std_17)
