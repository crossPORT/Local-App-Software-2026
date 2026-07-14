#include "tray_icons.hpp"

#include "tray_icon_outline.hpp"
#include "tray_theme.hpp"

#include <wx/bitmap.h>
#include <wx/filename.h>

namespace tunnel_tray {
namespace {

constexpr int kTrayPx = 24;
constexpr int kTrayHi = 48;

wxImage finalize_mark(wxImage img, bool dim) {
  if (!img.HasAlpha()) img.InitAlpha();
  for (int y = 0; y < img.GetHeight(); ++y) {
    for (int x = 0; x < img.GetWidth(); ++x) {
      const unsigned char a = img.GetAlpha(x, y);
      if (a < 40) {
        img.SetAlpha(x, y, 0);
        continue;
      }
      img.SetAlpha(x, y, dim ? static_cast<unsigned char>(a * 5 / 10) : 255);
    }
  }
  return img;
}

wxIcon icon_from_mark(bool dark_panel, bool dim) {
  wxImage img = finalize_mark(make_tray_mark(kTrayPx, dark_panel), dim);
  wxIcon icon;
  icon.CopyFromBitmap(wxBitmap(img));
  return icon;
}

}  // namespace

wxIcon load_brand_icon() { return icon_from_mark(desktop_prefers_dark(), false); }

wxIcon make_dim_icon(const wxIcon& /*src*/, unsigned char /*alpha*/) {
  return icon_from_mark(desktop_prefers_dark(), true);
}

bool save_tray_png(const wxString& path, bool dark_panel, bool dim) {
  wxImage img = finalize_mark(make_tray_mark(kTrayHi, dark_panel), dim);
  wxFileName::Mkdir(wxFileName(path).GetPath(), 0755, wxPATH_MKDIR_FULL);
  return img.IsOk() && img.SaveFile(path, wxBITMAP_TYPE_PNG);
}

}  // namespace tunnel_tray
