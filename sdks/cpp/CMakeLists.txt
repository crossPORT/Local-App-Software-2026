option(ROCKETBOX_BUILD_SIM "Include simulation support in compiled SDK" ON)
option(ROCKETBOX_BUILD_USB "Include libusb USB transport in compiled SDK" ON)

find_package(Threads REQUIRED)

set(ROCKETBOX_SDK_SOURCES
    src/sdk.cpp
    src/session_impl.cpp
    src/connection_impl.cpp
)

if (ROCKETBOX_BUILD_SIM)
    list(APPEND ROCKETBOX_SDK_SOURCES src/sim_transport.cpp)
else()
    list(APPEND ROCKETBOX_SDK_SOURCES src/sim_transport_stub.cpp)
endif()

if (ROCKETBOX_BUILD_USB)
    list(APPEND ROCKETBOX_SDK_SOURCES src/usb_transport.cpp)
else()
    list(APPEND ROCKETBOX_SDK_SOURCES src/usb_transport_stub.cpp)
endif()

add_library(rocketbox_sdk STATIC ${ROCKETBOX_SDK_SOURCES})

target_include_directories(rocketbox_sdk
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(rocketbox_sdk
    PUBLIC
        Threads::Threads
)

if (ROCKETBOX_BUILD_SIM)
    target_compile_definitions(rocketbox_sdk PUBLIC ROCKETBOX_SIM_SUPPORT=1)
endif()

if (ROCKETBOX_BUILD_USB)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
    target_include_directories(rocketbox_sdk PRIVATE ${LIBUSB_INCLUDE_DIRS})
    target_link_libraries(rocketbox_sdk PUBLIC ${LIBUSB_LIBRARIES})
    target_compile_definitions(rocketbox_sdk PUBLIC ROCKETBOX_USB_SUPPORT=1)
endif()

target_compile_features(rocketbox_sdk PUBLIC cxx_std_17)
