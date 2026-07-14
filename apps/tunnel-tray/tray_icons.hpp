#pragma once

#include <wx/icon.h>
#include <wx/image.h>

namespace tunnel_tray {

/** Load brand icon; may be invalid if assets missing. */
wxIcon load_brand_icon();

/** Darkened copy for stopped / pulse low frame (keeps full opacity). */
wxIcon make_dim_icon(const wxIcon& src, unsigned char unused_alpha = 255);

/** Write outline tray PNG (for Ayatana icon path). */
bool save_tray_png(const wxString& path, bool dark_panel, bool dim);

}  // namespace tunnel_tray
