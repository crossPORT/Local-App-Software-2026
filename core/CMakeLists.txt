# vcpkg libusb is consumed via pkg-config (no libusbConfig.cmake). Linux/macOS use
# system libusb-1.0.pc the same way.
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)
set(ROCKETBOX_LIBUSB_TARGET PkgConfig::LIBUSB)
# libusb.pc sets -I.../include/libusb-1.0; many TUs use <libusb-1.0/libusb.h>
# which needs the parent include directory.
get_filename_component(LIBUSB_INCLUDE_PARENT "${LIBUSB_INCLUDE_DIRS}" DIRECTORY)

add_library(fabric_usb_core STATIC
    src/usb_transfer_core.cpp
)

target_include_directories(fabric_usb_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${LIBUSB_INCLUDE_DIRS}
        ${LIBUSB_INCLUDE_PARENT}
)

target_link_libraries(fabric_usb_core
    PUBLIC
        ${ROCKETBOX_LIBUSB_TARGET}
        Threads::Threads
)

target_compile_features(fabric_usb_core PUBLIC cxx_std_17)
