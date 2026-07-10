cmake_minimum_required(VERSION 3.21)

# Fabric–TCP bridge (Linux TUN). Built from the repo root so rocketbox_sdk is available.
if(NOT UNIX OR APPLE)
  message(STATUS "fabric-tunnel: skipped (Linux only)")
  return()
endif()

if(NOT TARGET rocketbox_sdk)
  message(FATAL_ERROR "fabric-tunnel requires rocketbox_sdk (add_subdirectory(sdks/cpp) first)")
endif()

add_executable(rocketbox-tunnel
  main.cpp
  tun_device.cpp
  bridge.cpp
  dialer.cpp
  icmp_ping.cpp
  host_gateway.cpp
  fabric_ping.cpp
)
target_include_directories(rocketbox-tunnel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rocketbox-tunnel PRIVATE rocketbox_sdk)
target_compile_features(rocketbox-tunnel PRIVATE cxx_std_17)
