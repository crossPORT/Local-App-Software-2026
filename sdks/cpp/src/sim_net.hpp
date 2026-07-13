#pragma once

#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using rb_sock_t = SOCKET;
constexpr rb_sock_t RB_SOCK_INVALID = INVALID_SOCKET;
#else
using rb_sock_t = int;
constexpr rb_sock_t RB_SOCK_INVALID = -1;
#endif

namespace rocketbox {
namespace detail {

bool rb_net_init();
void rb_net_fini();
rb_sock_t rb_connect_loopback(uint16_t port);
void rb_sock_close(rb_sock_t& fd);
void rb_sock_shutdown(rb_sock_t fd);
/** Bytes read/written, or <=0 on error/EOF. */
int rb_sock_read(rb_sock_t fd, void* buf, size_t len);
int rb_sock_write(rb_sock_t fd, const void* buf, size_t len);

}  // namespace detail
}  // namespace rocketbox
