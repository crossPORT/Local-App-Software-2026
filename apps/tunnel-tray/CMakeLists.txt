find_package(wxWidgets 3.2 REQUIRED COMPONENTS core base net)
include(${wxWidgets_USE_FILE})

set(_tray_gtk_libs "")
set(_tray_gtk_incs "")
set(_tray_gtk_flags "")
set(_tray_has_gtk FALSE)
if(UNIX AND NOT APPLE)
  find_package(PkgConfig REQUIRED)
  # Headers for GNOME attention suppress; must match CI apt (libgtk-3-dev).
  pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
  set(_tray_has_gtk TRUE)
  set(_tray_gtk_libs ${GTK3_LIBRARIES})
  set(_tray_gtk_incs ${GTK3_INCLUDE_DIRS})
  set(_tray_gtk_flags ${GTK3_CFLAGS_OTHER})
  pkg_check_modules(AYATANA QUIET ayatana-appindicator3-0.1)
  if(AYATANA_FOUND)
    list(APPEND _tray_gtk_libs ${AYATANA_LIBRARIES})
    list(APPEND _tray_gtk_incs ${AYATANA_INCLUDE_DIRS})
    list(APPEND _tray_gtk_flags ${AYATANA_CFLAGS_OTHER})
  endif()
endif()

add_executable(rocketbox-tunnel-tray
  main.cpp
  session_dbus.cpp
  tray_icon.cpp
  tray_icon_enable.cpp
  tray_icon_tick.cpp
  tray_ipc.cpp
  tray_enable_msg.cpp
  tray_start_msg.cpp
  listen_ports.cpp
  tray_settings.cpp
  tray_panel.cpp
  tray_panel_build.cpp
  tray_icons.cpp
  tray_icon_outline.cpp
  tray_ayatana.cpp
  tray_theme.cpp
  tunnel_rates.cpp
  tunnel_log_ui.cpp
  expose_list_ui.cpp
  usb_ports_ui.cpp
  expose_presets.cpp
  tunnel_args.cpp
  platform/stats_paths.cpp
)
if(NOT WIN32)
  target_sources(rocketbox-tunnel-tray PRIVATE
    tunnel_hup.cpp
    tunnel_proc_reload.cpp
    tray_gnome.cpp
    ${CMAKE_SOURCE_DIR}/rocketbox-tunnel/expose_file.cpp
  )
endif()

if(WIN32)
  target_sources(rocketbox-tunnel-tray PRIVATE
    tunnel_proc_win.cpp
    helper_launch_win.cpp
    socket_owners_win.cpp
    "${CMAKE_SOURCE_DIR}/cmake/icons/rocketbox.rc")
  set_target_properties(rocketbox-tunnel-tray PROPERTIES WIN32_EXECUTABLE TRUE)
else()
  target_sources(rocketbox-tunnel-tray PRIVATE
    tunnel_proc_linux.cpp
    socket_owners_linux.cpp
    helper_launch.cpp)
endif()

target_include_directories(rocketbox-tunnel-tray PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/rocketbox-tunnel
  ${wxWidgets_INCLUDE_DIRS}
  ${_tray_gtk_incs}
)
target_link_libraries(rocketbox-tunnel-tray PRIVATE
  rocketbox_sdk
  ${wxWidgets_LIBRARIES}
  ${_tray_gtk_libs}
)
# Prefer helper when available; protocol client shared with tunnel-helper.
target_sources(rocketbox-tunnel-tray PRIVATE
  ${CMAKE_SOURCE_DIR}/rocketbox-tunnel/helper/protocol.cpp
  ${CMAKE_SOURCE_DIR}/rocketbox-tunnel/tunnel_log.cpp
)
target_compile_definitions(rocketbox-tunnel-tray PRIVATE ${wxWidgets_DEFINITIONS})
if(_tray_has_gtk)
  target_compile_definitions(rocketbox-tunnel-tray PRIVATE ROCKETBOX_TRAY_HAS_GTK=1)
endif()
if(AYATANA_FOUND)
  target_compile_definitions(rocketbox-tunnel-tray PRIVATE ROCKETBOX_TRAY_HAS_AYATANA=1)
endif()
target_compile_options(rocketbox-tunnel-tray PRIVATE ${wxWidgets_CXX_FLAGS} ${_tray_gtk_flags})
target_compile_features(rocketbox-tunnel-tray PRIVATE cxx_std_17)

install(TARGETS rocketbox-tunnel-tray RUNTIME DESTINATION bin COMPONENT RocketBoxTunnelTray)
if(WIN32)
  install(FILES "${CMAKE_SOURCE_DIR}/cmake/icons/rocketbox.ico" DESTINATION bin
          COMPONENT RocketBoxTunnelTray)
endif()
if(UNIX AND NOT APPLE)
  # Ship autostart under share/; postinst copies to /etc/xdg/autostart (same as udev).
  # Never DESTINATION /etc — cmake --install --prefix staging cannot write there.
  install(FILES ${CMAKE_SOURCE_DIR}/cmake/debian/rocketbox-tunnel-tray-autostart.desktop
          DESTINATION share/rocketbox
          RENAME rocketbox-tunnel-tray-autostart.desktop
          COMPONENT RocketBoxTunnelTray)
  install(FILES ${CMAKE_SOURCE_DIR}/cmake/debian/rocketbox-tunnel-tray.desktop
          DESTINATION share/applications
          COMPONENT RocketBoxTunnelTray)
  # Tray-only installs must ship the themed icon (App package is optional).
  install(FILES "${CMAKE_SOURCE_DIR}/cmake/icons/rocketbox-256.png"
          DESTINATION share/icons/hicolor/256x256/apps
          RENAME rocketbox.png
          COMPONENT RocketBoxTunnelTray)
  install(FILES "${CMAKE_SOURCE_DIR}/cmake/icons/rocketbox-256.png"
          DESTINATION share/pixmaps
          RENAME rocketbox.png
          COMPONENT RocketBoxTunnelTray)
endif()

if(TARGET rocketbox-tunnel)
  add_dependencies(rocketbox-tunnel-tray rocketbox-tunnel)
endif()

set(_rb_icon "${CMAKE_SOURCE_DIR}/cmake/icons/rocketbox-256.png")
if(EXISTS "${_rb_icon}")
  add_custom_command(TARGET rocketbox-tunnel-tray POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${_rb_icon}" "$<TARGET_FILE_DIR:rocketbox-tunnel-tray>/rocketbox.png"
    COMMENT "Copy rocketbox.png beside tunnel-tray")
endif()
if(WIN32)
  set(_rb_ico "${CMAKE_SOURCE_DIR}/cmake/icons/rocketbox.ico")
  if(EXISTS "${_rb_ico}")
    add_custom_command(TARGET rocketbox-tunnel-tray POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${_rb_ico}" "$<TARGET_FILE_DIR:rocketbox-tunnel-tray>/rocketbox.ico"
      COMMENT "Copy rocketbox.ico beside tunnel-tray")
  endif()
endif()
