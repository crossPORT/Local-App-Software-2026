#include "app_icon.h"

#include <wx/frame.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>

namespace {

bool TryLoadIcon(wxIcon& icon, const wxString& path, wxBitmapType type) {
    if (path.empty() || !wxFileName::FileExists(path)) {
        return false;
    }
    return icon.LoadFile(path, type);
}

wxFileName ExeDir() {
    wxFileName exe(wxStandardPaths::Get().GetExecutablePath());
    exe.SetFullName(wxEmptyString);
    return exe;
}

/** Path relative to the install prefix (exe in <prefix>/bin → ../share/...). */
wxString BesideExe(const wxString& filename) {
    wxFileName path = ExeDir();
    path.SetFullName(filename);
    return path.GetFullPath();
}

wxString SharePath(std::initializer_list<wxString> parts, const wxString& filename) {
    wxFileName path = ExeDir();
    path.RemoveLastDir();  // leave bin/
    for (const wxString& part : parts) {
        path.AppendDir(part);
    }
    path.SetFullName(filename);
    return path.GetFullPath();
}

}  // namespace

wxIcon LoadRocketBoxIcon() {
    wxIcon icon;

#if defined(__WXMSW__)
    if (TryLoadIcon(icon, BesideExe(wxT("rocketbox.ico")), wxBITMAP_TYPE_ICO)) {
        return icon;
    }
#elif defined(__WXOSX__)
    const wxString resources = wxStandardPaths::Get().GetResourcesDir();
    if (TryLoadIcon(icon,
                    resources + wxFileName::GetPathSeparator() + wxT("rocketbox.icns"),
                    wxBITMAP_TYPE_ICON)) {
        return icon;
    }
#else
    // Prefer install-layout paths from the binary, then a copy next to the exe (dev builds).
    const wxString candidates[] = {
        SharePath({wxT("share"), wxT("icons"), wxT("hicolor"), wxT("256x256"), wxT("apps")},
                  wxT("rocketbox.png")),
        SharePath({wxT("share"), wxT("pixmaps")}, wxT("rocketbox.png")),
        BesideExe(wxT("rocketbox.png")),
    };
    for (const wxString& path : candidates) {
        if (TryLoadIcon(icon, path, wxBITMAP_TYPE_PNG)) {
            return icon;
        }
    }
#endif

    return icon;
}

void ApplyRocketBoxFrameIcon(wxFrame* frame) {
    if (frame == nullptr) {
        return;
    }
    const wxIcon loaded = LoadRocketBoxIcon();
    if (loaded.IsOk()) {
        frame->SetIcon(loaded);
    }
}
