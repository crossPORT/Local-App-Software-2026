#pragma once

namespace tunnel_helper {

#if defined(_WIN32)
void serve_named_pipe();
#else
void serve_unix();
#endif

}  // namespace tunnel_helper
