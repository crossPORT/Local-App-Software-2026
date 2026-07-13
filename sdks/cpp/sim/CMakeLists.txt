add_library(rocketbox_usb_sim STATIC
    src/rocketbox_sim.cpp
)

target_include_directories(rocketbox_usb_sim
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(rocketbox_usb_sim
    PUBLIC
        rocketbox_usb_core
        Threads::Threads
)

target_compile_features(rocketbox_usb_sim PUBLIC cxx_std_17)
