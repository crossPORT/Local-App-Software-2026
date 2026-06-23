#include "app_icon.h"

#include <wx/filename.h>
#include <wx/stdpaths.h>

namespace {

bool TryLoadIcon(wxIcon& icon, const wxString& path, wxBitmapType type) {
    if (path.empty() || !wxFileName::FileExists(path)) {
        return false;
    }
    return icon.LoadFile(path, type);
}

}  // namespace

wxIcon LoadRocketBoxIcon() {
    wxIcon icon;

#if defined(__WXMSW__)
    wxFileName exe(wxStandardPaths::Get().GetExecutablePath());
    exe.SetFullName(wxT("rocketbox.ico"));
    if (TryLoadIcon(icon, exe.GetFullPath(), wxBITMAP_TYPE_ICO)) {
        return icon;
    }
#elif defined(__WXOSX__)
    wxString resources = wxStandardPaths::Get().GetResourcesDir();
    if (TryLoadIcon(icon, resources + wxFileName::GetPathSeparator() + wxT("rocketbox.icns"),
                    wxBITMAP_TYPE_ICON)) {
        return icon;
    }
#else
    static const wxChar* kIconPaths[] = {
        wxT("/usr/share/icons/hicolor/256x256/apps/rocketbox.png"),
        wxT("/usr/share/pixmaps/rocketbox.png"),
    };
    for (const wxChar* path : kIconPaths) {
        if (TryLoadIcon(icon, path, wxBITMAP_TYPE_PNG)) {
            return icon;
        }
    }
    wxFileName exe(wxStandardPaths::Get().GetExecutablePath());
    exe.SetFullName(wxT("rocketbox.png"));
    if (TryLoadIcon(icon, exe.GetFullPath(), wxBITMAP_TYPE_PNG)) {
        return icon;
    }
#endif

    return icon;
}

void ApplyRocketBoxFrameIcon(wxFrame* frame) {
    if (frame == nullptr) {
        return;
    }
    const wxIcon icon = LoadRocketBoxIcon();
    if (icon.IsOk()) {
        frame->SetIcon(icon);
    }
}
