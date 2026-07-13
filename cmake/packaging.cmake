# CPack for RocketBox App + optional Tunnel (+ Tray). Extends existing generators.

set(CPACK_PACKAGE_NAME "RocketBox")
set(CPACK_PACKAGE_VENDOR "crossPORT")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
  "RocketBox App and optional Tunnel. App and Tunnel cannot share one USB cable.")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "RocketBox")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_CONTACT "support@crossport.io")

set(CPACK_COMPONENTS_ALL RocketBoxApp RocketBoxTunnel RocketBoxTunnelTray)
set(CPACK_COMPONENT_ROCKETBOXAPP_DISPLAY_NAME "RocketBox App")
set(CPACK_COMPONENT_ROCKETBOXAPP_DESCRIPTION "File transfer for RocketBox hardware")
set(CPACK_COMPONENT_ROCKETBOXTUNNEL_DISPLAY_NAME "RocketBox Tunnel")
set(CPACK_COMPONENT_ROCKETBOXTUNNEL_DESCRIPTION
  "IP tunnel over USB. Cannot use the same USB cable as RocketBox App at the same time.")
set(CPACK_COMPONENT_ROCKETBOXTUNNELTRAY_DISPLAY_NAME "Tunnel Tray")
set(CPACK_COMPONENT_ROCKETBOXTUNNELTRAY_DESCRIPTION
  "System tray control for Tunnel (uncheck for server-only). Autostarts at login.")
set(CPACK_COMPONENT_ROCKETBOXTUNNELTRAY_DEPENDS RocketBoxTunnel)
set(CPACK_COMPONENT_ROCKETBOXAPP_REQUIRED ON)

if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${ROCKETBOX_RELEASE_TAG}-setup")
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY "bin")
    set(_rb_nsis_icon "${CMAKE_SOURCE_DIR}/cmake/icons/rocketbox-installer.ico")
    set(CPACK_NSIS_MUI_ICON "${_rb_nsis_icon}")
    set(CPACK_NSIS_MUI_UNIICON "${_rb_nsis_icon}")
    set(CPACK_NSIS_DISPLAY_NAME "RocketBox")
    set(CPACK_NSIS_PACKAGE_NAME "RocketBox")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH OFF)
    set(CPACK_NSIS_COMPONENT_INSTALL ON)
    # Custom shortcuts with rocketbox.ico (CPack PACKAGE_EXECUTABLES omits icon args).
    set(CPACK_NSIS_CREATE_ICONS_EXTRA "
      CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\RocketBox App.lnk' '$INSTDIR\\\\bin\\\\RocketBox.exe' '' '$INSTDIR\\\\bin\\\\rocketbox.ico' 0
      CreateShortCut '$DESKTOP\\\\RocketBox App.lnk' '$INSTDIR\\\\bin\\\\RocketBox.exe' '' '$INSTDIR\\\\bin\\\\rocketbox.ico' 0
      IfFileExists '$INSTDIR\\\\bin\\\\rocketbox-tunnel-tray.exe' 0 skip_tray_sm
        CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\RocketBox Tunnel Tray.lnk' '$INSTDIR\\\\bin\\\\rocketbox-tunnel-tray.exe' '' '$INSTDIR\\\\bin\\\\rocketbox.ico' 0
      skip_tray_sm:")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA "
      Delete '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\RocketBox App.lnk'
      Delete '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\RocketBox Tunnel Tray.lnk'
      Delete '$DESKTOP\\\\RocketBox App.lnk'")
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
      IfFileExists '$INSTDIR\\\\bin\\\\rocketbox-tunnel-tray.exe' 0 skip_tray_startup
        CreateShortCut '$SMSTARTUP\\\\RocketBox Tunnel Tray.lnk' '$INSTDIR\\\\bin\\\\rocketbox-tunnel-tray.exe' '--background' '$INSTDIR\\\\bin\\\\rocketbox.ico' 0
      skip_tray_startup:")
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS
      "Delete '$SMSTARTUP\\\\RocketBox Tunnel Tray.lnk'")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "RocketBox")
    set(CPACK_DMG_FORMAT "UDZO")
else()
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_NAME "rocketbox")
    set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libwxgtk3.2-1t64 | libwxgtk3.2-1, libusb-1.0-0")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEB_COMPONENT_INSTALL ON)
endif()

include(CPack)
