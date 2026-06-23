#pragma once

#include <wx/frame.h>
#include <wx/icon.h>

// Shared RocketBox icon (same artwork as apps/web/public/favicon.svg).
wxIcon LoadRocketBoxIcon();

void ApplyRocketBoxFrameIcon(wxFrame* frame);
