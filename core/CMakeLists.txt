if(WIN32)
    find_package(libusb CONFIG REQUIRED)
    set(ROCKETBOX_LIBUSB_TARGET libusb::libusb)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)
    set(ROCKETBOX_LIBUSB_TARGET PkgConfig::LIBUSB)
    # libusb.pc sets -I.../include/libusb-1.0; many TUs use <libusb-1.0/libusb.h>
    # which needs the parent include directory (Linux often finds /usr/include anyway).
    get_filename_component(LIBUSB_INCLUDE_PARENT "${LIBUSB_INCLUDE_DIRS}" DIRECTORY)
endif()

add_library(fabric_usb_core STATIC
    src/usb_transfer_core.cpp
)

target_include_directories(fabric_usb_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        $<$<NOT:$<PLATFORM_ID:Windows>>:${LIBUSB_INCLUDE_DIRS}>
        $<$<NOT:$<PLATFORM_ID:Windows>>:${LIBUSB_INCLUDE_PARENT}>
)

target_link_libraries(fabric_usb_core
    PUBLIC
        ${ROCKETBOX_LIBUSB_TARGET}
        Threads::Threads
)

target_compile_features(fabric_usb_core PUBLIC cxx_std_17)
