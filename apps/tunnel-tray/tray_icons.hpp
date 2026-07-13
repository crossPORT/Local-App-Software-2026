#pragma once

#include <wx/icon.h>
#include <wx/image.h>

namespace tunnel_tray {

/** Load brand icon; may be invalid if assets missing. */
wxIcon load_brand_icon();

/** Darkened copy for stopped / pulse low frame (keeps full opacity). */
wxIcon make_dim_icon(const wxIcon& src, unsigned char unused_alpha = 255);

}  // namespace tunnel_tray
