#include "common.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <ctime>
#include <thread>
#include <vector>

#include <wx/datetime.h>
#include <wx/image.h>
#include <wx/msgdlg.h>
#include <wx/progdlg.h>
#include <wx/settings.h>
#include <wx/time.h>
#include <wx/utils.h>
#include <wx/window.h>

namespace pasgen::wxui {

wxString FormatTime(std::chrono::system_clock::time_point tp) {
    const std::time_t t = std::chrono::system_clock::to_time_t(tp);
    return wxDateTime(t).Format("%Y-%m-%d %H:%M");
}

wxColour CategoryColour(int index) {
    static const unsigned dark[kCategoryColours] = {0xE8894A, 0x4E9BF5, 0x3ECF8E, 0xAE7BEE,
                                                    0xE8C44E, 0xF06B72, 0x35C4C4, 0x93A0B5};
    static const unsigned light[kCategoryColours] = {0xC2661E, 0x2A6FD0, 0x15855C, 0x7B4BC4,
                                                     0x9A7500, 0xC63641, 0x107C7C, 0x5E6878};
    const bool is_dark = wxSystemSettings::GetAppearance().IsDark();
    if (index < 0) return is_dark ? wxColour(0x80, 0x88, 0x96) : wxColour(0x8A, 0x92, 0x9E);
    const unsigned rgb = (is_dark ? dark : light)[index % kCategoryColours];
    return wxColour((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

namespace {

// Anti-aliased filled circle with a transparent background, drawn by hand so
// it looks the same on every platform (no DC alpha quirks).
wxBitmap dot_bitmap(const wxColour& c, int px) {
    wxImage img(px, px);
    img.InitAlpha();
    const double r = px * 0.36, cx = (px - 1) / 2.0, cy = (px - 1) / 2.0;
    for (int y = 0; y < px; ++y) {
        for (int x = 0; x < px; ++x) {
            const double d = std::hypot(x - cx, y - cy);
            const double cover = std::clamp(r + 0.5 - d, 0.0, 1.0);
            img.SetRGB(x, y, c.Red(), c.Green(), c.Blue());
            img.SetAlpha(x, y, (unsigned char)std::lround(cover * 255));
        }
    }
    return wxBitmap(img);
}

}  // namespace

wxBitmapBundle ColourDot(const wxColour& colour, int size_dip) {
    wxVector<wxBitmap> sizes;
    for (int px : {size_dip, size_dip * 3 / 2, size_dip * 2}) sizes.push_back(dot_bitmap(colour, px));
    return wxBitmapBundle::FromBitmaps(sizes);
}

void RunWithProgress(wxWindow* parent, const wxString& title, const wxString& message,
                     const std::function<void()>& work) {
    std::atomic<bool> done{false};
    std::exception_ptr error;
    std::thread worker([&] {
        try {
            work();
        } catch (...) {
            error = std::current_exception();
        }
        done = true;
    });

    // Most of the time this is about a second; don't flash a dialog for
    // anything shorter than a blink.
    const wxLongLong start = wxGetLocalTimeMillis();
    while (!done && wxGetLocalTimeMillis() - start < 200) wxMilliSleep(10);

    if (!done) {
        wxProgressDialog dlg(title, message, 100, parent, wxPD_APP_MODAL | wxPD_AUTO_HIDE);
        while (!done) {
            dlg.Pulse();
            wxMilliSleep(40);
        }
    }
    worker.join();
    if (error) std::rethrow_exception(error);
}

void ShowError(wxWindow* parent, const wxString& what, const std::exception& e) {
    wxMessageBox(U(e.what()), what, wxOK | wxICON_ERROR, parent);
}

}  // namespace pasgen::wxui
