find_package(wxWidgets 3.2 REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

add_executable(rocketbox-tunnel-tray
  main.cpp
  tray_icon.cpp
  tunnel_proc.cpp
)

target_include_directories(rocketbox-tunnel-tray PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rocketbox-tunnel-tray PRIVATE ${wxWidgets_LIBRARIES})
target_include_directories(rocketbox-tunnel-tray PRIVATE ${wxWidgets_INCLUDE_DIRS})
target_compile_definitions(rocketbox-tunnel-tray PRIVATE ${wxWidgets_DEFINITIONS})
target_compile_options(rocketbox-tunnel-tray PRIVATE ${wxWidgets_CXX_FLAGS})
target_compile_features(rocketbox-tunnel-tray PRIVATE cxx_std_17)

# Prefer the tunnel built in the same tree when that target exists.
if(TARGET rocketbox-tunnel)
  add_dependencies(rocketbox-tunnel-tray rocketbox-tunnel)
endif()
