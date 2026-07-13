#include "tray_icons.hpp"

#include "tray_theme.hpp"

#include <wx/filename.h>
#include <wx/stdpaths.h>

namespace tunnel_tray {
namespace {

constexpr int kTrayPx = 24;

bool try_load(wxIcon& icon, const wxString& path, wxBitmapType type) {
  if (path.empty() || !wxFileName::FileExists(path)) return false;
  return icon.LoadFile(path, type);
}

wxFileName exe_dir() {
  wxFileName exe(wxStandardPaths::Get().GetExecutablePath());
  exe.SetFullName(wxEmptyString);
  return exe;
}

wxString beside_exe(const wxString& name) {
  wxFileName path = exe_dir();
  path.SetFullName(name);
  return path.GetFullPath();
}

wxString share_path(std::initializer_list<wxString> parts, const wxString& name) {
  wxFileName path = exe_dir();
  path.RemoveLastDir();
  for (const wxString& p : parts) path.AppendDir(p);
  path.SetFullName(name);
  return path.GetFullPath();
}

/** Brand plate is near-black navy — remap so the mark reads on light or dark panels. */
bool is_brand_plate(unsigned char r, unsigned char g, unsigned char b) {
  return r < 48 && g < 52 && b < 72;
}

void adapt_plate(wxImage& img, bool dark_panel) {
  // Dark panel → light plate; light panel → keep/deepen dark plate.
  const unsigned char pr = dark_panel ? 232 : 18;
  const unsigned char pg = dark_panel ? 236 : 26;
  const unsigned char pb = dark_panel ? 241 : 42;
  // Contrasting 1px ring so the square never melts into the panel.
  const unsigned char rr = dark_panel ? 255 : 20;
  const unsigned char rg = dark_panel ? 255 : 24;
  const unsigned char rb = dark_panel ? 255 : 36;

  const int w = img.GetWidth();
  const int h = img.GetHeight();
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      if (img.GetAlpha(x, y) < 40) continue;
      const unsigned char r = img.GetRed(x, y);
      const unsigned char g = img.GetGreen(x, y);
      const unsigned char b = img.GetBlue(x, y);
      if (!is_brand_plate(r, g, b)) continue;
      // Edge of opaque region → ring; interior → plate.
      bool edge = false;
      for (int dy = -1; dy <= 1 && !edge; ++dy) {
        for (int dx = -1; dx <= 1 && !edge; ++dx) {
          if (dx == 0 && dy == 0) continue;
          const int nx = x + dx;
          const int ny = y + dy;
          if (nx < 0 || ny < 0 || nx >= w || ny >= h || img.GetAlpha(nx, ny) < 40) {
            edge = true;
          }
        }
      }
      if (edge) {
        img.SetRGB(x, y, rr, rg, rb);
      } else {
        img.SetRGB(x, y, pr, pg, pb);
      }
      img.SetAlpha(x, y, 255);
    }
  }
}

wxIcon from_image(wxImage img, bool dark_panel) {
  if (img.GetWidth() != kTrayPx || img.GetHeight() != kTrayPx) {
    img = img.Scale(kTrayPx, kTrayPx, wxIMAGE_QUALITY_HIGH);
  }
  if (!img.HasAlpha()) img.InitAlpha();
  for (int y = 0; y < img.GetHeight(); ++y) {
    for (int x = 0; x < img.GetWidth(); ++x) {
      img.SetAlpha(x, y, img.GetAlpha(x, y) >= 40 ? 255 : 0);
    }
  }
  adapt_plate(img, dark_panel);
  wxIcon icon;
  icon.CopyFromBitmap(wxBitmap(img));
  return icon;
}

wxIcon fallback_dot(bool dark_panel) {
  // Bright on dark panels, deeper on light panels.
  const unsigned char r = dark_panel ? 80 : 20;
  const unsigned char g = dark_panel ? 200 : 140;
  const unsigned char b = dark_panel ? 120 : 70;
  wxImage img(kTrayPx, kTrayPx);
  img.InitAlpha();
  const int c = kTrayPx / 2;
  const int r2 = (kTrayPx / 2 - 1) * (kTrayPx / 2 - 1);
  for (int y = 0; y < kTrayPx; ++y) {
    for (int x = 0; x < kTrayPx; ++x) {
      const int dx = x - c;
      const int dy = y - c;
      const bool on = dx * dx + dy * dy <= r2;
      img.SetRGB(x, y, on ? r : 0, on ? g : 0, on ? b : 0);
      img.SetAlpha(x, y, on ? 255 : 0);
    }
  }
  return from_image(img, dark_panel);
}

wxImage load_brand_image() {
  wxIcon icon;
  const wxString candidates[] = {
      share_path({wxT("share"), wxT("icons"), wxT("hicolor"), wxT("256x256"), wxT("apps")},
                 wxT("rocketbox.png")),
      share_path({wxT("share"), wxT("pixmaps")}, wxT("rocketbox.png")),
      beside_exe(wxT("rocketbox.png")),
      beside_exe(wxT("rocketbox-256.png")),
  };
  for (const wxString& path : candidates) {
    if (try_load(icon, path, wxBITMAP_TYPE_PNG) && icon.IsOk()) {
      return wxBitmap(icon).ConvertToImage();
    }
  }
  if (try_load(icon, beside_exe(wxT("rocketbox.ico")), wxBITMAP_TYPE_ICO) && icon.IsOk()) {
    return wxBitmap(icon).ConvertToImage();
  }
  return {};
}

}  // namespace

wxIcon load_brand_icon() {
  const bool dark = desktop_prefers_dark();
  wxImage img = load_brand_image();
  if (!img.IsOk()) return fallback_dot(dark);
  return from_image(img, dark);
}

wxIcon make_dim_icon(const wxIcon& src, unsigned char /*alpha*/) {
  const bool dark = desktop_prefers_dark();
  if (!src.IsOk()) return fallback_dot(dark);
  wxImage img = wxBitmap(src).ConvertToImage();
  if (!img.HasAlpha()) img.InitAlpha();
  // Soften without crushing contrast needed on either panel.
  const int num = dark ? 5 : 3;
  const int den = dark ? 6 : 4;
  for (int y = 0; y < img.GetHeight(); ++y) {
    for (int x = 0; x < img.GetWidth(); ++x) {
      if (img.GetAlpha(x, y) == 0) continue;
      img.SetRGB(x, y,
                 static_cast<unsigned char>(img.GetRed(x, y) * num / den),
                 static_cast<unsigned char>(img.GetGreen(x, y) * num / den),
                 static_cast<unsigned char>(img.GetBlue(x, y) * num / den));
      img.SetAlpha(x, y, 255);
    }
  }
  wxIcon out;
  out.CopyFromBitmap(wxBitmap(img));
  return out;
}

}  // namespace tunnel_tray
