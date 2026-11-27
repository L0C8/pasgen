#pragma once

#include <wx/string.h>

class wxWindow;

namespace pasgen::wxui {

// Puts text on the system clipboard. When `secret` is true it also marks the
// copy as sensitive, so clipboard history and sync tools skip it:
//
//   Windows  ExcludeClipboardContentFromMonitorProcessing,
//            CanIncludeInClipboardHistory = 0 (Win+V history),
//            CanUploadToCloudClipboard = 0 (cross-device sync)
//   Linux    x-kde-passwordManagerHint = "secret" (Klipper, and the other
//            managers that honour it)
//
// Auto-clear alone cannot do this: by the time the timer fires, a history
// tool already has its own copy.
bool CopyToClipboard(wxWindow* owner, const wxString& text, bool secret);

// True if the clipboard still holds exactly `text`.
bool ClipboardHolds(const wxString& text);

// Empties the clipboard, but only if it still holds `text`, so a later copy
// the user made in another app is never wiped.
bool ClearClipboardIfHolds(wxWindow* owner, const wxString& text);

}  // namespace pasgen::wxui
