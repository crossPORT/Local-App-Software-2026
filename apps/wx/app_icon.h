#pragma once

#include <wx/bitmap.h>
#include <wx/icon.h>

class wxFrame;

wxIcon LoadRocketBoxIcon();
wxBitmap LoadRocketBoxLogoBitmap();
wxBitmap LoadRocketBoxWordmarkBitmap();
void ApplyRocketBoxFrameIcon(wxFrame* frame);
