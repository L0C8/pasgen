#include "secure_clipboard.h"

#include <string>

#include <wx/app.h>
#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/window.h>

#ifdef __WXMSW__
#include <wx/msw/wrapwin.h>
#endif

namespace pasgen::wxui {

#ifdef __WXMSW__

namespace {

bool set_global(UINT format, const void* data, size_t size) {
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!mem) return false;
    void* p = GlobalLock(mem);
    if (!p) {
        GlobalFree(mem);
        return false;
    }
    memcpy(p, data, size);
    GlobalUnlock(mem);
    if (!SetClipboardData(format, mem)) {
        GlobalFree(mem);
        return false;
    }
    return true;  // the clipboard owns mem now
}

// Win32 needs an owner window: with a null owner EmptyClipboard() leaves the
// clipboard ownerless and SetClipboardData() fails.
HWND owner_hwnd(wxWindow* owner) {
    wxWindow* top = owner ? wxGetTopLevelParent(owner) : wxTheApp->GetTopWindow();
    return top ? static_cast<HWND>(top->GetHandle()) : nullptr;
}

}  // namespace

bool CopyToClipboard(wxWindow* owner, const wxString& text, bool secret) {
    if (!OpenClipboard(owner_hwnd(owner))) return false;
    EmptyClipboard();
    const std::wstring w = text.ToStdWstring();
    bool ok = set_global(CF_UNICODETEXT, w.c_str(), (w.size() + 1) * sizeof(wchar_t));
    if (ok && secret) {
        const DWORD zero = 0;
        set_global(RegisterClipboardFormatW(L"ExcludeClipboardContentFromMonitorProcessing"), &zero,
                   sizeof(zero));
        set_global(RegisterClipboardFormatW(L"CanIncludeInClipboardHistory"), &zero, sizeof(zero));
        set_global(RegisterClipboardFormatW(L"CanUploadToCloudClipboard"), &zero, sizeof(zero));
    }
    CloseClipboard();
    return ok;
}

bool ClipboardHolds(const wxString& text) {
    if (!OpenClipboard(nullptr)) return false;
    bool same = false;
    if (HANDLE h = GetClipboardData(CF_UNICODETEXT)) {
        if (const wchar_t* p = static_cast<const wchar_t*>(GlobalLock(h))) {
            same = text.ToStdWstring() == p;
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    return same;
}

bool ClearClipboardIfHolds(wxWindow* owner, const wxString& text) {
    if (!ClipboardHolds(text)) return false;
    if (!OpenClipboard(owner_hwnd(owner))) return false;
    EmptyClipboard();
    CloseClipboard();
    return true;
}

#else  // GTK, macOS

bool CopyToClipboard(wxWindow*, const wxString& text, bool secret) {
    wxClipboardLocker lock;
    if (!lock) return false;
    auto* data = new wxDataObjectComposite;
    data->Add(new wxTextDataObject(text), true);
    if (secret) {
        auto* hint = new wxCustomDataObject(wxDataFormat("x-kde-passwordManagerHint"));
        hint->SetData(6, "secret");
        data->Add(hint);
    }
    // No Flush(): on GTK that hands the data to the clipboard manager so it
    // outlives the app, which is the opposite of what a secret wants.
    return wxTheClipboard->SetData(data);
}

bool ClipboardHolds(const wxString& text) {
    wxClipboardLocker lock;
    if (!lock || !wxTheClipboard->IsSupported(wxDF_UNICODETEXT)) return false;
    wxTextDataObject current;
    return wxTheClipboard->GetData(current) && current.GetText() == text;
}

bool ClearClipboardIfHolds(wxWindow*, const wxString& text) {
    if (!ClipboardHolds(text)) return false;
    wxClipboardLocker lock;
    if (!lock) return false;
    wxTheClipboard->Clear();
    return true;
}

#endif

}  // namespace pasgen::wxui
