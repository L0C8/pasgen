#include "strength.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>

namespace pasgen::wxui {

namespace {

// Base words that show up at the top of every leaked-password list. Matching
// one costs the attacker roughly log2(list size) bits, not its length.
const char* const kCommonWords[] = {
    "password", "passwort", "qwerty", "qwertz", "azerty", "letmein", "welcome", "admin",
    "login", "master", "monkey", "dragon", "iloveyou", "sunshine", "princess", "football",
    "baseball", "soccer", "shadow", "superman", "batman", "trustno1", "secret", "hello",
    "freedom", "whatever", "starwars", "pokemon", "computer", "internet", "summer", "winter",
    "spring", "autumn", "love", "god", "jesus", "money", "abc", "pasgen", "test", "guest",
    "changeme", "default", "user", "root", "hallo", "schatz", "fussball", "killer",
};

// Rows an attacker walks along on a keyboard.
const char* const kKeyboardRows[] = {
    "1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm", "qwertzuiop", "yxcvbnm", "azertyuiop",
};

char unleet(char c) {
    switch (c) {
        case '0': return 'o';
        case '1': return 'l';
        case '!': return 'i';
        case '3': return 'e';
        case '4': case '@': return 'a';
        case '5': case '$': return 's';
        case '7': return 't';
        default: return (char)std::tolower((unsigned char)c);
    }
}

bool in_row(char a, char b) {
    for (const char* row : kKeyboardRows) {
        const char* pa = std::strchr(row, a);
        const char* pb = std::strchr(row, b);
        if (pa && pb && (pb - pa == 1 || pa - pb == 1)) return true;
    }
    return false;
}

}  // namespace

Strength StrengthFromBits(double bits) {
    Strength s;
    s.bits = bits;
    s.level = bits < 28 ? 0 : bits < 40 ? 1 : bits < 60 ? 2 : bits < 80 ? 3 : 4;
    return s;
}

Strength EstimateStrength(const std::string& pw) {
    if (pw.empty()) return {};

    bool lower = false, upper = false, digit = false, symbol = false, other = false;
    for (unsigned char c : pw) {
        if (c >= 0x80) other = true;
        else if (std::islower(c)) lower = true;
        else if (std::isupper(c)) upper = true;
        else if (std::isdigit(c)) digit = true;
        else symbol = true;
    }
    double pool = (lower ? 26 : 0) + (upper ? 26 : 0) + (digit ? 10 : 0) + (symbol ? 33 : 0) +
                  (other ? 100 : 0);
    const double per_char = std::log2(std::max(pool, 2.0));

    // Characters that follow a predictable step from the previous one (same
    // char, +/-1 in ASCII, or the neighbouring key) are nearly free to guess.
    std::string folded;
    for (char c : pw) folded += unleet(c);
    double effective = 1.0;
    for (size_t i = 1; i < pw.size(); ++i) {
        const int d = (int)(unsigned char)folded[i] - (int)(unsigned char)folded[i - 1];
        const bool predictable = d == 0 || d == 1 || d == -1 || in_row(folded[i - 1], folded[i]);
        effective += predictable ? 0.25 : 1.0;
    }
    double bits = effective * per_char;

    // A common base word is one choice out of a short list, plus the usual
    // capitalisation and suffix tricks.
    for (const char* w : kCommonWords) {
        const size_t n = std::strlen(w);
        if (n < 3 || folded.find(w) == std::string::npos) continue;
        const double word_bits = (double)n * per_char;
        bits = std::max(bits - word_bits + 12.0, 1.0);
    }

    // Upper case only at the very start and digits/symbols only at the end
    // is the pattern everyone uses to satisfy complexity rules.
    if (upper && std::isupper((unsigned char)pw[0])) {
        bool only_first = true;
        for (size_t i = 1; i < pw.size(); ++i)
            if (std::isupper((unsigned char)pw[i])) only_first = false;
        if (only_first) bits -= std::min(4.0, bits * 0.1);
    }

    return StrengthFromBits(std::max(bits, 0.0));
}

wxString StrengthLabel(int level) {
    static const char* const labels[] = {"Very weak", "Weak", "Fair", "Strong", "Very strong"};
    return labels[std::clamp(level, 0, 4)];
}

wxColour StrengthColour(int level) {
    switch (std::clamp(level, 0, 4)) {
        case 0: return wxColour(0xCE, 0x3A, 0x47);
        case 1: return wxColour(0xE0, 0x6C, 0x2B);
        case 2: return wxColour(0xC9, 0x9A, 0x00);
        case 3: return wxColour(0x2E, 0x9E, 0x5B);
        default: return wxColour(0x15, 0x85, 0x5C);
    }
}

wxString DescribeStrength(const Strength& s) {
    return wxString::Format("%s, about %d bits", StrengthLabel(s.level), (int)std::lround(s.bits));
}

}  // namespace pasgen::wxui

#include <wx/dcclient.h>
#include <wx/settings.h>

namespace pasgen::wxui {

StrengthBar::StrengthBar(wxWindow* parent)
    : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE) {
    // Default (system) background: the window takes its parent's themed
    // background, which a hand-picked colour would not match inside a
    // static box on every platform.
    InheritAttributes();
    Bind(wxEVT_PAINT, &StrengthBar::OnPaint, this);
}

void StrengthBar::SetStrength(const Strength& s, bool empty) {
    strength_ = s;
    empty_ = empty;
    caption_ = empty ? wxString() : DescribeStrength(s);
    Refresh();
}

wxSize StrengthBar::DoGetBestClientSize() const {
    wxClientDC dc(const_cast<StrengthBar*>(this));
    dc.SetFont(GetFont());
    const wxSize text = dc.GetTextExtent("Very strong, about 999 bits");
    return wxSize(FromDIP(150) + FromDIP(10) + text.x, std::max(text.y, FromDIP(8)));
}

void StrengthBar::OnPaint(wxPaintEvent&) {
    wxPaintDC dc(this);

    const wxSize sz = GetClientSize();
    const int seg_w = FromDIP(28), gap = FromDIP(2), seg_h = FromDIP(6);
    const int y = (sz.y - seg_h) / 2;
    const wxColour on = StrengthColour(strength_.level);
    wxColour off = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW);
    if (wxSystemSettings::GetAppearance().IsDark()) off = off.ChangeLightness(70);

    dc.SetPen(*wxTRANSPARENT_PEN);
    for (int i = 0; i < 5; ++i) {
        dc.SetBrush(wxBrush(!empty_ && i <= strength_.level ? on : off));
        dc.DrawRoundedRectangle(i * (seg_w + gap), y, seg_w, seg_h, FromDIP(2));
    }
    if (!caption_.empty()) {
        dc.SetFont(GetFont());
        dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
        const wxSize t = dc.GetTextExtent(caption_);
        dc.DrawText(caption_, 5 * (seg_w + gap) + FromDIP(8), (sz.y - t.y) / 2);
    }
}

}  // namespace pasgen::wxui
