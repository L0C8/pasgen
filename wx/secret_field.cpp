#include "secret_field.h"

#include <wx/sizer.h>
#include <wx/textctrl.h>
#include <wx/tglbtn.h>

namespace pasgen::wxui {

SecretField::SecretField(wxWindow* parent, int flags) : wxPanel(parent, wxID_ANY) {
    const long enter = (flags & EnterKey) ? wxTE_PROCESS_ENTER : 0;
    masked_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                             wxTE_PASSWORD | enter);
    plain_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, enter);
    plain_->Hide();

    if (flags & Monospace) {
        wxFont mono(wxFontInfo(GetFont().GetPointSize()).Family(wxFONTFAMILY_TELETYPE));
        masked_->SetFont(mono);
        plain_->SetFont(mono);
    }

    auto* row = new wxBoxSizer(wxHORIZONTAL);
    row->Add(masked_, wxSizerFlags(1).CenterVertical());
    row->Add(plain_, wxSizerFlags(1).CenterVertical());
    if (flags & WithToggle) {
        toggle_ = new wxToggleButton(this, wxID_ANY, "Show");
        toggle_->SetToolTip("Show or hide the text");
        row->AddSpacer(FromDIP(4));
        row->Add(toggle_, wxSizerFlags().CenterVertical());
        toggle_->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent&) {
            SetRevealed(toggle_->GetValue());
            FocusEntry();
        });
    }
    SetSizer(row);

    Wire(masked_);
    Wire(plain_);
}

void SecretField::Wire(wxTextCtrl* ctrl) {
    ctrl->Bind(wxEVT_TEXT, [this](wxCommandEvent& e) {
        if (!silent_ && OnChange) OnChange();
        e.Skip();
    });
    ctrl->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) {
        if (OnCommit) OnCommit();
        if (OnEnter) OnEnter();
    });
    ctrl->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& e) {
        if (OnCommit) OnCommit();
        e.Skip();
    });
}

wxString SecretField::GetValue() const { return Active()->GetValue(); }

void SecretField::SetValue(const wxString& value) {
    silent_ = true;
    Active()->ChangeValue(value);
    (revealed_ ? masked_ : plain_)->ChangeValue(wxString());
    silent_ = false;
}

void SecretField::SetHint(const wxString& hint) {
    masked_->SetHint(hint);
    plain_->SetHint(hint);
}

void SecretField::SetRevealed(bool revealed) {
    if (revealed == revealed_) return;
    wxTextCtrl* from = Active();
    revealed_ = revealed;
    wxTextCtrl* to = Active();

    silent_ = true;
    to->ChangeValue(from->GetValue());
    to->SetInsertionPoint(from->GetInsertionPoint());
    silent_ = false;
    // Keep only one live copy of the text around.
    from->ChangeValue(wxString());

    const bool had_focus = from->HasFocus();
    from->Hide();
    to->Show();
    if (had_focus) to->SetFocus();
    if (toggle_) {
        toggle_->SetValue(revealed_);
        toggle_->SetLabel(revealed_ ? "Hide" : "Show");
    }
    Layout();
    if (OnRevealChanged) OnRevealChanged(revealed_);
}

void SecretField::FocusEntry() { Active()->SetFocus(); }

void SecretField::SetEditable(bool editable) {
    masked_->SetEditable(editable);
    plain_->SetEditable(editable);
}

}  // namespace pasgen::wxui
