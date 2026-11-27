#pragma once

#include <string>

#include <wx/colour.h>
#include <wx/string.h>

namespace pasgen::wxui {

// Rough guessing-cost estimate for a password a person chose.
//
// The old meter counted character classes, so "Password1!" scored the
// maximum. This one starts from length * log2(pool), then discounts the
// patterns real cracking tools try first: repeats, runs like "abc"/"321",
// keyboard rows, and common base words (with simple l33t substitutions).
// It is deliberately pessimistic. For generator output use
// PasswordGenerator::entropy_bits instead, which is exact.
struct Strength {
    double bits = 0.0;
    int level = 0;  // 0 very weak .. 4 very strong
};

Strength EstimateStrength(const std::string& password);
Strength StrengthFromBits(double bits);

wxString StrengthLabel(int level);
wxColour StrengthColour(int level);
// "Strong, about 72 bits"
wxString DescribeStrength(const Strength& s);

// Master passwords: refuse below this length outright...
constexpr std::size_t kMinMasterLength = 10;
// ...and ask for confirmation below this estimate.
constexpr double kWarnMasterBits = 50.0;

}  // namespace pasgen::wxui

#include <wx/window.h>

namespace pasgen::wxui {

// Five-segment meter with a caption to its right, painted by hand: native
// gauges cannot be recoloured on GTK, and colour is what makes it readable.
class StrengthBar : public wxWindow {
public:
    explicit StrengthBar(wxWindow* parent);
    void SetStrength(const Strength& s, bool empty = false);
    wxSize DoGetBestClientSize() const override;

private:
    void OnPaint(wxPaintEvent& e);
    Strength strength_;
    bool empty_ = true;
    wxString caption_;
};

}  // namespace pasgen::wxui
