add_library(fabric_usb_core STATIC
    src/usb_transfer_core.cpp
)

target_include_directories(fabric_usb_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)

target_link_libraries(fabric_usb_core
    PUBLIC
        PkgConfig::LIBUSB
        Threads::Threads
)

target_compile_features(fabric_usb_core PUBLIC cxx_std_17)
