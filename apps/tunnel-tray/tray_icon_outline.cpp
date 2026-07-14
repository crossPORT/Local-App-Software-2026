#include "tray_icon_outline.hpp"

#include <algorithm>

namespace tunnel_tray {
namespace {

void set_px(wxImage& img, int x, int y, unsigned char r, unsigned char g, unsigned char b) {
  if (x < 0 || y < 0 || x >= img.GetWidth() || y >= img.GetHeight()) return;
  img.SetRGB(x, y, r, g, b);
  img.SetAlpha(x, y, 255);
}

void fill_rect(wxImage& img, int x0, int y0, int x1, int y1, unsigned char r, unsigned char g,
               unsigned char b) {
  if (x1 < x0) std::swap(x0, x1);
  if (y1 < y0) std::swap(y0, y1);
  for (int y = y0; y <= y1; ++y)
    for (int x = x0; x <= x1; ++x) set_px(img, x, y, r, g, b);
}

void stroke_rect(wxImage& img, int x0, int y0, int x1, int y1, unsigned char r, unsigned char g,
                 unsigned char b, int t) {
  for (int i = 0; i < t; ++i) {
    fill_rect(img, x0 + i, y0 + i, x1 - i, y0 + i, r, g, b);
    fill_rect(img, x0 + i, y1 - i, x1 - i, y1 - i, r, g, b);
    fill_rect(img, x0 + i, y0 + i, x0 + i, y1 - i, r, g, b);
    fill_rect(img, x1 - i, y0 + i, x1 - i, y1 - i, r, g, b);
  }
}

/** Block capital R inside [x0,y0]–[x1,y1]. */
void draw_letter_r(wxImage& img, int x0, int y0, int x1, int y1, unsigned char r, unsigned char g,
                   unsigned char b) {
  const int w = std::max(1, x1 - x0 + 1);
  const int h = std::max(1, y1 - y0 + 1);
  const int t = std::max(2, std::min(w, h) / 5);
  const int mid = y0 + h * 48 / 100;
  fill_rect(img, x0, y0, x0 + t - 1, y1, r, g, b);          // stem
  fill_rect(img, x0, y0, x1, y0 + t - 1, r, g, b);          // top
  fill_rect(img, x1 - t + 1, y0, x1, mid, r, g, b);         // bowl side
  fill_rect(img, x0, mid - t + 1, x1, mid, r, g, b);        // crossbar
  const int leg_h = std::max(1, y1 - mid);
  for (int row = 0; row <= leg_h; ++row) {
    const int x = x0 + t + row * (w - 2 * t) / leg_h;
    fill_rect(img, x, mid + row, std::min(x1, x + t - 1), mid + row, r, g, b);
  }
}

}  // namespace

wxImage make_tray_mark(int size, bool dark_panel) {
  wxImage img(size, size);
  img.InitAlpha();
  for (int y = 0; y < size; ++y)
    for (int x = 0; x < size; ++x) img.SetAlpha(x, y, 0);

  const unsigned char c = dark_panel ? 255 : 22;
  const unsigned char cg = dark_panel ? 255 : 28;
  const unsigned char cb = dark_panel ? 255 : 38;
  const int thick = std::max(2, size / 14);
  const int edge = std::max(thick + 1, size / 10);
  const int x0 = edge;
  const int y0 = edge;
  const int x1 = size - 1 - edge;
  const int y1 = size - 1 - edge;
  stroke_rect(img, x0, y0, x1, y1, c, cg, cb, thick);

  const int pad = std::max(thick + 2, size / 6);
  draw_letter_r(img, x0 + pad, y0 + pad, x1 - pad, y1 - pad, c, cg, cb);
  return img;
}

}  // namespace tunnel_tray
