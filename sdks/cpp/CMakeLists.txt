option(ROCKETBOX_BUILD_SIM "Include simulation support in compiled SDK" ON)

add_library(rocketbox_sdk STATIC
    src/sdk.cpp
)

target_include_directories(rocketbox_sdk
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(rocketbox_sdk
    PUBLIC
        Threads::Threads
)

if (ROCKETBOX_BUILD_SIM)
    target_compile_definitions(rocketbox_sdk PUBLIC ROCKETBOX_SIM_SUPPORT=1)
endif()

target_compile_features(rocketbox_sdk PUBLIC cxx_std_17)
