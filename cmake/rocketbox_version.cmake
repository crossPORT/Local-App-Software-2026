# Embed ROCKETBOX_VERSION / ROCKETBOX_RELEASE_TAG into binaries.
# Expects both vars set (root CMakeLists). Generates header + Windows VERSIONINFO.

set(_rb_v0 0)
set(_rb_v1 0)
set(_rb_v2 0)
set(_rb_v3 0)
string(REPLACE "." ";" _rb_parts "${ROCKETBOX_VERSION}")
list(LENGTH _rb_parts _rb_n)
if(_rb_n GREATER 0)
  list(GET _rb_parts 0 _rb_v0)
endif()
if(_rb_n GREATER 1)
  list(GET _rb_parts 1 _rb_v1)
endif()
if(_rb_n GREATER 2)
  list(GET _rb_parts 2 _rb_v2)
endif()
if(_rb_n GREATER 3)
  list(GET _rb_parts 3 _rb_v3)
endif()
set(ROCKETBOX_FILEVERSION_CSV "${_rb_v0},${_rb_v1},${_rb_v2},${_rb_v3}")

set(_rb_gen "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${_rb_gen}")
configure_file(
  "${CMAKE_SOURCE_DIR}/cmake/rocketbox_version.h.in"
  "${_rb_gen}/rocketbox_version.h"
  @ONLY)
configure_file(
  "${CMAKE_SOURCE_DIR}/cmake/rocketbox_version.rc.in"
  "${_rb_gen}/rocketbox_version.rc"
  @ONLY)

add_library(rocketbox_version INTERFACE)
target_include_directories(rocketbox_version INTERFACE "${_rb_gen}")

function(rocketbox_target_version target)
  target_link_libraries(${target} PRIVATE rocketbox_version)
  if(WIN32)
    target_sources(${target} PRIVATE "${CMAKE_BINARY_DIR}/generated/rocketbox_version.rc")
  endif()
endfunction()

message(STATUS "RocketBox release: ${ROCKETBOX_RELEASE_TAG} (version ${ROCKETBOX_VERSION})")
