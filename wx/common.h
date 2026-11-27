#pragma once

#include <chrono>
#include <exception>
#include <functional>
#include <string>

#include <wx/bmpbndl.h>
#include <wx/colour.h>
#include <wx/string.h>

class wxWindow;

namespace pasgen::wxui {

// The core speaks UTF-8 std::string; wx speaks wxString.
inline wxString U(const std::string& s) { return wxString::FromUTF8(s); }
inline std::string S(const wxString& s) { return std::string(s.utf8_str()); }

// "2026-11-27 14:05", local time.
wxString FormatTime(std::chrono::system_clock::time_point tp);

// Category swatches. Same eight hues the ImGui build uses, picked for the
// current light/dark appearance so a category keeps its colour across both
// frontends (Category::color_index indexes this list).
constexpr int kCategoryColours = 8;
wxColour CategoryColour(int index);  // index < 0: neutral grey
wxBitmapBundle ColourDot(const wxColour& colour, int size_dip = 12);

// Runs `work` on a worker thread while showing an indeterminate progress
// dialog, then rethrows anything it threw. For Argon2id (create, unlock,
// re-encrypt), which takes a second or more and would otherwise freeze the
// window long enough for the desktop to call it unresponsive.
void RunWithProgress(wxWindow* parent, const wxString& title, const wxString& message,
                     const std::function<void()>& work);

// Shows an error message box for the exception currently being handled.
void ShowError(wxWindow* parent, const wxString& what, const std::exception& e);

}  // namespace pasgen::wxui
