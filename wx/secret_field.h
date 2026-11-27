#pragma once

#include <functional>

#include <wx/panel.h>

class wxTextCtrl;
class wxToggleButton;

namespace pasgen::wxui {

// A password entry with a Show/Hide toggle.
//
// wxTE_PASSWORD cannot be switched on and off after creation on every
// platform, so this holds a masked and a plain text control and swaps which
// one is visible, carrying the text and cursor across.
class SecretField : public wxPanel {
public:
    enum Flags {
        WithToggle = 1 << 0,  // include the Show/Hide button
        Monospace = 1 << 1,   // fixed-width font, easier to read generated passwords
        EnterKey = 1 << 2,    // report Enter through OnEnter
    };

    SecretField(wxWindow* parent, int flags = WithToggle);

    wxString GetValue() const;
    void SetValue(const wxString& value);  // does not fire OnChange
    void Clear() { SetValue(wxString()); }
    void SetHint(const wxString& hint);

    bool IsRevealed() const { return revealed_; }
    void SetRevealed(bool revealed);

    void FocusEntry();
    void SetEditable(bool editable);

    // Fired on every edit, on Enter (with EnterKey), and when the entry loses
    // focus (the point where an editor should commit).
    std::function<void()> OnChange;
    std::function<void()> OnEnter;
    std::function<void()> OnCommit;
    std::function<void(bool)> OnRevealChanged;

private:
    wxTextCtrl* Active() const { return revealed_ ? plain_ : masked_; }
    void Wire(wxTextCtrl* ctrl);

    wxTextCtrl* masked_ = nullptr;
    wxTextCtrl* plain_ = nullptr;
    wxToggleButton* toggle_ = nullptr;
    bool revealed_ = false;
    bool silent_ = false;
};

}  // namespace pasgen::wxui
