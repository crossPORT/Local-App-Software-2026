#pragma once

#include <wx/image.h>

namespace tunnel_tray {

/** Outline box + capital R (white on dark panel, dark on light). */
wxImage make_tray_mark(int size, bool dark_panel);

}  // namespace tunnel_tray
