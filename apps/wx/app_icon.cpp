#include "app_icon.h"

#include <wx/frame.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>

namespace {

wxString FileBesideExe(const wxString& name) {
    wxFileName exe(wxStandardPaths::Get().GetExecutablePath());
    exe.SetFullName(name);
    return exe.GetFullPath();
}

wxString FileInResources(const wxString& name) {
    return wxFileName(wxStandardPaths::Get().GetResourcesDir(), name).GetFullPath();
}

wxString FileInSourceIcons(const wxString& name) {
    wxFileName walk(wxStandardPaths::Get().GetExecutablePath());
    walk.SetFullName(wxEmptyString);
    for (int i = 0; i < 8; ++i) {
        const wxString candidate = walk.GetPathWithSep() + wxT("cmake/icons/") + name;
        if (wxFileName::FileExists(candidate)) {
            return candidate;
        }
        if (!walk.GetDirCount()) {
            break;
        }
        walk.RemoveLastDir();
    }
    return {};
}

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
    if (TryLoadIcon(icon, FileBesideExe(wxT("rocketbox.ico")), wxBITMAP_TYPE_ICO) ||
        TryLoadIcon(icon, FileInSourceIcons(wxT("rocketbox.ico")), wxBITMAP_TYPE_ICO)) {
        return icon;
    }
#elif defined(__WXOSX__)
    wxString resources = wxStandardPaths::Get().GetResourcesDir();
    if (TryLoadIcon(icon, resources + wxFileName::GetPathSeparator() + wxT("rocketbox.icns"),
                    wxBITMAP_TYPE_ICON)) {
        return icon;
    }
#else
    const wxString linux_paths[] = {
        FileBesideExe(wxT("rocketbox.png")),
        FileInSourceIcons(wxT("rocketbox-256.png")),
        wxT("/usr/share/icons/hicolor/256x256/apps/rocketbox.png"),
        wxT("/usr/share/pixmaps/rocketbox.png"),
    };
    for (const wxString& path : linux_paths) {
        if (TryLoadIcon(icon, path, wxBITMAP_TYPE_PNG)) {
            return icon;
        }
    }
#endif

    return icon;
}

wxBitmap LoadNamedPng(const wxString& name) {
    wxBitmap bitmap;
    const wxString paths[] = {
        FileInResources(name),
        FileBesideExe(name),
        FileInSourceIcons(name),
        wxT("/usr/share/rocketbox/") + name,
    };
    for (const wxString& path : paths) {
        if (!path.empty() && wxFileName::FileExists(path) &&
            bitmap.LoadFile(path, wxBITMAP_TYPE_PNG) && bitmap.IsOk()) {
            return bitmap;
        }
    }
    return bitmap;
}

wxBitmap LoadRocketBoxLogoBitmap() {
    const wxBitmap mark = LoadNamedPng(wxT("rocketbox-mark.png"));
    return mark.IsOk() ? mark : LoadNamedPng(wxT("rocketbox-logo.png"));
}

wxBitmap LoadRocketBoxWordmarkBitmap() {
    return LoadNamedPng(wxT("rocketbox-wordmark.png"));
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
