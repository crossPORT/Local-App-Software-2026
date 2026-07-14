cmake_minimum_required(VERSION 3.21)

if(NOT TARGET rocketbox_sdk)
  message(FATAL_ERROR "rocketbox-tunnel requires rocketbox_sdk")
endif()

set(_tun_src tun_device_linux.cpp)
set(_gw_src host_gateway_linux.cpp)
set(_wintun_src "")
if(APPLE)
  set(_tun_src tun_device_darwin.cpp)
  set(_gw_src host_gateway_darwin.cpp)
elseif(WIN32)
  set(_tun_src tun_device_win.cpp)
  set(_gw_src host_gateway_win.cpp)
  set(_wintun_src wintun_load.cpp)
elseif(NOT UNIX)
  message(STATUS "rocketbox-tunnel: skipped (unsupported OS)")
  return()
endif()

add_executable(rocketbox-tunnel
  main.cpp
  open_transport.cpp
  ${_tun_src}
  ${_wintun_src}
  bridge.cpp
  dialer.cpp
  icmp_ping.cpp
  ${_gw_src}
  rocketbox_ping.cpp
  tunnel_stats.cpp
  stats_path.cpp
  port_lock.cpp
  tunnel_log.cpp
  expose_file.cpp
  tunnel_options.cpp
)
target_include_directories(rocketbox-tunnel PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/third_party/wintun/include
)
target_link_libraries(rocketbox-tunnel PRIVATE rocketbox_sdk)
if(WIN32)
  target_link_libraries(rocketbox-tunnel PRIVATE iphlpapi ws2_32)
endif()
target_compile_features(rocketbox-tunnel PRIVATE cxx_std_17)

set(_helper_server helper/server_posix.cpp)
if(WIN32)
  set(_helper_server helper/server_win.cpp)
endif()
add_executable(rocketbox-tunnel-helper
  helper/main.cpp
  helper/commands.cpp
  helper/protocol.cpp
  ${_helper_server}
)
target_include_directories(rocketbox-tunnel-helper PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(rocketbox-tunnel-helper PRIVATE cxx_std_17)

install(TARGETS rocketbox-tunnel rocketbox-tunnel-helper
        RUNTIME DESTINATION bin COMPONENT RocketBoxTunnel)
if(WIN32)
  # Populated by scripts/fetch-wintun.ps1 before configure/install (CI or local).
  foreach(_wintun_cand
      "${CMAKE_SOURCE_DIR}/third_party/wintun/bin/wintun.dll"
      "${CMAKE_BINARY_DIR}/wintun/wintun.dll")
    if(EXISTS "${_wintun_cand}")
      install(FILES "${_wintun_cand}" DESTINATION bin COMPONENT RocketBoxTunnel)
      break()
    endif()
  endforeach()
endif()
if(UNIX AND NOT APPLE)
  install(FILES ${CMAKE_SOURCE_DIR}/docs/tunnel/linux.md
          DESTINATION share/doc/rocketbox/tunnel
          COMPONENT RocketBoxTunnel)
  install(FILES ${CMAKE_SOURCE_DIR}/cmake/debian/org.rocketbox.pkexec.tunnel-helper.policy
          DESTINATION share/polkit-1/actions
          COMPONENT RocketBoxTunnel)
elseif(APPLE)
  install(FILES ${CMAKE_SOURCE_DIR}/docs/tunnel/macos.md
          DESTINATION share/doc/rocketbox/tunnel
          COMPONENT RocketBoxTunnel)
elseif(WIN32)
  install(FILES ${CMAKE_SOURCE_DIR}/docs/tunnel/windows.md
          DESTINATION share/doc/rocketbox/tunnel
          COMPONENT RocketBoxTunnel)
endif()
