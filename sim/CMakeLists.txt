add_library(fabric_usb_sim STATIC
    src/fabric_sim.cpp
)

target_include_directories(fabric_usb_sim
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fabric_usb_sim
    PUBLIC
        fabric_usb_core
        Threads::Threads
)

target_compile_features(fabric_usb_sim PUBLIC cxx_std_17)
