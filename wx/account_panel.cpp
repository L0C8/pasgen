#include "account_panel.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/gauge.h>
#include <wx/listctrl.h>
#include <wx/msgdlg.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/utils.h>

#include "common.h"
#include "core/Database.hpp"
#include "dialogs.h"
#include "secret_field.h"
#include "strength.h"
#include "util/TOTPGenerator.hpp"

namespace pasgen::wxui {

namespace {

const wxString kMask = wxString::FromUTF8("\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2"
                                          "\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2");  // 8 bullets

wxString group_code(const std::string& code) {
    if (code.size() != 6) return U(code);
    return U(code.substr(0, 3) + " " + code.substr(3));
}

}  // namespace

AccountPanel::AccountPanel(wxWindow* parent, AccountHost* host)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxTAB_TRAVERSAL),
      host_(host) {
    SetScrollRate(0, FromDIP(12));
    Build();
    Unload();
}

Account* AccountPanel::Acc() const { return db_ ? db_->get_account(id_) : nullptr; }

wxStaticBoxSizer* AccountPanel::Section(const wxString& title) {
    return new wxStaticBoxSizer(wxVERTICAL, body_, title);
}

void AccountPanel::Build() {
    body_ = new wxPanel(this);
    const int pad = FromDIP(12), gap = FromDIP(6);

    // Header: name, flags, dates.
    title_ = new wxStaticText(body_, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                              wxST_ELLIPSIZE_END);
    title_->SetFont(title_->GetFont().Bold().Scaled(1.45f));
    favorite_ = new wxCheckBox(body_, wxID_ANY, "Favorite");
    archived_ = new wxCheckBox(body_, wxID_ANY, "Archived");
    dates_ = new wxStaticText(body_, wxID_ANY, wxEmptyString);
    dates_->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));

    auto* head = new wxBoxSizer(wxHORIZONTAL);
    head->Add(title_, wxSizerFlags(1).CenterVertical());
    head->Add(favorite_, wxSizerFlags().CenterVertical().Border(wxLEFT, pad));
    head->Add(archived_, wxSizerFlags().CenterVertical().Border(wxLEFT, pad));

    // Login
    wxStaticBoxSizer* login = Section("Login");
    wxWindow* lb = login->GetStaticBox();
    auto* grid = new wxFlexGridSizer(3, wxSize(gap * 2, gap));
    grid->AddGrowableCol(1, 1);
    auto label = [&](wxWindow* parent, const wxString& text) {
        grid->Add(new wxStaticText(parent, wxID_ANY, text), wxSizerFlags().CenterVertical().Right());
    };
    auto button = [&](wxWindow* parent, const wxString& text, const wxString& tip) {
        auto* b = new wxButton(parent, wxID_ANY, text, wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
        b->SetToolTip(tip);
        return b;
    };

    name_ = new wxTextCtrl(lb, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    username_ = new wxTextCtrl(lb, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    email_ = new wxTextCtrl(lb, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    url_ = new wxTextCtrl(lb, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    url_->SetHint("https://");
    password_ = new SecretField(lb, SecretField::WithToggle | SecretField::Monospace | SecretField::EnterKey);
    auto* copy_user = button(lb, "Copy", "Copy the username (Ctrl+B)");
    auto* copy_email = button(lb, "Copy", "Copy the email address");
    auto* open_url = button(lb, "Open", "Open the website in your browser");
    auto* copy_pass = button(lb, "Copy", "Copy the password (Ctrl+Shift+C)");
    auto* gen_pass = button(lb, "Generate...", "Generate a new random password (Ctrl+G)");
    strength_ = new StrengthBar(lb);
    age_ = new wxStaticText(lb, wxID_ANY, wxEmptyString);
    age_->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));

    label(lb, "Name:");
    grid->Add(name_, wxSizerFlags().Expand().CenterVertical());
    grid->AddSpacer(0);
    label(lb, "Username:");
    grid->Add(username_, wxSizerFlags().Expand().CenterVertical());
    grid->Add(copy_user, wxSizerFlags().CenterVertical());
    label(lb, "Email:");
    grid->Add(email_, wxSizerFlags().Expand().CenterVertical());
    grid->Add(copy_email, wxSizerFlags().CenterVertical());
    label(lb, "Website:");
    grid->Add(url_, wxSizerFlags().Expand().CenterVertical());
    grid->Add(open_url, wxSizerFlags().CenterVertical());
    label(lb, "Password:");
    grid->Add(password_, wxSizerFlags().Expand().CenterVertical());
    auto* pass_buttons = new wxBoxSizer(wxHORIZONTAL);
    pass_buttons->Add(copy_pass, wxSizerFlags().CenterVertical());
    pass_buttons->AddSpacer(FromDIP(4));
    pass_buttons->Add(gen_pass, wxSizerFlags().CenterVertical());
    grid->Add(pass_buttons, wxSizerFlags().CenterVertical());
    grid->AddSpacer(0);
    auto* meter_row = new wxBoxSizer(wxHORIZONTAL);
    meter_row->Add(strength_, wxSizerFlags().CenterVertical());
    meter_row->AddStretchSpacer();
    meter_row->Add(age_, wxSizerFlags().CenterVertical());
    grid->Add(meter_row, wxSizerFlags().Expand());
    grid->AddSpacer(0);
    login->Add(grid, wxSizerFlags().Expand().Border(wxALL, FromDIP(8)));

    // Two-factor
    wxStaticBoxSizer* totp = Section("Two-factor code (TOTP)");
    wxWindow* tb = totp->GetStaticBox();
    totp_secret_ = new SecretField(tb, SecretField::WithToggle | SecretField::Monospace | SecretField::EnterKey);
    totp_secret_->SetHint("Base32 setup key from the site (optional)");
    totp_code_ = new wxStaticText(tb, wxID_ANY, "000 000");
    totp_code_->SetFont(wxFont(wxFontInfo(GetFont().GetPointSize() * 1.6).Family(wxFONTFAMILY_TELETYPE).Bold()));
    totp_timer_ = new wxGauge(tb, wxID_ANY, 30, wxDefaultPosition, FromDIP(wxSize(120, 8)));
    totp_left_ = new wxStaticText(tb, wxID_ANY, "30 s");
    totp_copy_ = button(tb, "Copy", "Copy the current code (Ctrl+T)");
    totp_error_ = new wxStaticText(tb, wxID_ANY, "Not a valid base32 key.");
    totp_error_->SetForegroundColour(StrengthColour(0));

    auto* tgrid = new wxFlexGridSizer(2, wxSize(gap * 2, gap));
    tgrid->AddGrowableCol(1, 1);
    tgrid->Add(new wxStaticText(tb, wxID_ANY, "Setup key:"), wxSizerFlags().CenterVertical().Right());
    tgrid->Add(totp_secret_, wxSizerFlags().Expand());
    tgrid->AddSpacer(0);
    totp_row_ = new wxBoxSizer(wxHORIZONTAL);
    totp_row_->Add(totp_code_, wxSizerFlags().CenterVertical());
    totp_row_->AddSpacer(pad);
    totp_row_->Add(totp_timer_, wxSizerFlags().CenterVertical());
    totp_row_->AddSpacer(gap);
    totp_row_->Add(totp_left_, wxSizerFlags().CenterVertical());
    totp_row_->AddStretchSpacer();
    totp_row_->Add(totp_copy_, wxSizerFlags().CenterVertical());
    totp_row_->Add(totp_error_, wxSizerFlags().CenterVertical());
    tgrid->Add(totp_row_, wxSizerFlags().Expand());
    totp->Add(tgrid, wxSizerFlags().Expand().Border(wxALL, FromDIP(8)));

    // Custom fields
    wxStaticBoxSizer* custom = Section("Custom fields");
    wxWindow* cb = custom->GetStaticBox();
    fields_ = new wxListCtrl(cb, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(-1, 110)),
                             wxLC_REPORT | wxLC_SINGLE_SEL);
    fields_->AppendColumn("Name", wxLIST_FORMAT_LEFT, FromDIP(150));
    fields_->AppendColumn("Value", wxLIST_FORMAT_LEFT, FromDIP(260));
    auto* field_add = new wxButton(cb, wxID_ANY, "Add...");
    field_edit_ = new wxButton(cb, wxID_ANY, "Edit...");
    field_copy_ = new wxButton(cb, wxID_ANY, "Copy");
    field_remove_ = new wxButton(cb, wxID_ANY, "Remove");
    auto* fbuttons = new wxBoxSizer(wxVERTICAL);
    for (wxButton* b : {field_add, field_edit_, field_copy_, field_remove_})
        fbuttons->Add(b, wxSizerFlags().Expand().Border(wxBOTTOM, FromDIP(4)));
    auto* frow = new wxBoxSizer(wxHORIZONTAL);
    frow->Add(fields_, wxSizerFlags(1).Expand());
    frow->AddSpacer(gap);
    frow->Add(fbuttons);
    custom->Add(frow, wxSizerFlags().Expand().Border(wxALL, FromDIP(8)));

    // Notes
    wxStaticBoxSizer* notes = Section("Notes");
    notes_ = new wxTextCtrl(notes->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition,
                            FromDIP(wxSize(-1, 96)), wxTE_MULTILINE);
    notes->Add(notes_, wxSizerFlags().Expand().Border(wxALL, FromDIP(8)));

    // History
    history_box_ = Section("Previous passwords");
    wxWindow* hb = history_box_->GetStaticBox();
    history_ = new wxListCtrl(hb, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(-1, 96)),
                              wxLC_REPORT | wxLC_SINGLE_SEL);
    history_->AppendColumn("Replaced", wxLIST_FORMAT_LEFT, FromDIP(150));
    history_->AppendColumn("Password", wxLIST_FORMAT_LEFT, FromDIP(200));
    history_copy_ = new wxButton(hb, wxID_ANY, "Copy");
    history_copy_->SetToolTip("Copy the selected old password, e.g. when a site still expects it");
    auto* hrow = new wxBoxSizer(wxHORIZONTAL);
    hrow->Add(history_, wxSizerFlags(1).Expand());
    hrow->AddSpacer(gap);
    hrow->Add(history_copy_);
    history_box_->Add(hrow, wxSizerFlags().Expand().Border(wxALL, FromDIP(8)));

    auto* col = new wxBoxSizer(wxVERTICAL);
    col->Add(head, wxSizerFlags().Expand());
    col->Add(dates_, wxSizerFlags().Border(wxTOP, FromDIP(2)));
    col->AddSpacer(pad);
    for (wxSizer* s : std::initializer_list<wxSizer*>{login, totp, custom, notes, history_box_})
        col->Add(s, wxSizerFlags().Expand().Border(wxBOTTOM, pad));
    body_->SetSizer(col);
    body_->SetMaxSize(FromDIP(wxSize(760, -1)));

    auto* outer = new wxBoxSizer(wxVERTICAL);
    outer->Add(body_, wxSizerFlags(1).Expand().Border(wxALL, FromDIP(16)));
    SetSizer(outer);

    // Commit points: focus leaving a field, or Enter.
    auto commit_on = [this](wxTextCtrl* t) {
        t->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& e) {
            Commit();
            e.Skip();
        });
        if (t->GetWindowStyle() & wxTE_PROCESS_ENTER)
            t->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) { Commit(); });
    };
    for (wxTextCtrl* t : {name_, username_, email_, url_, notes_}) commit_on(t);
    name_->Bind(wxEVT_TEXT, [this](wxCommandEvent& e) {
        UpdateHeader();
        e.Skip();
    });
    password_->OnChange = [this] { UpdateStrength(); };
    password_->OnCommit = [this] { Commit(); };
    totp_secret_->OnChange = [this] { UpdateTotp(); };
    totp_secret_->OnCommit = [this] { Commit(); };
    totp_secret_->OnRevealChanged = [this](bool) { UpdateTotp(); };

    favorite_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
        if (Account* a = Acc()) {
            a->set_favorite(favorite_->GetValue());
            host_->AccountEdited(id_, true);
        }
    });
    archived_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
        if (Account* a = Acc()) {
            a->set_archived(archived_->GetValue());
            host_->AccountEdited(id_, true);
        }
    });

    copy_user->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { CopyUsername(); });
    copy_email->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (!email_->IsEmpty()) host_->CopyText(email_->GetValue(), false, "Email");
    });
    open_url->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OpenUrl(); });
    copy_pass->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { CopyPassword(); });
    gen_pass->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { GeneratePassword(); });
    totp_copy_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { CopyTotp(); });

    field_add->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { AddField(); });
    field_edit_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EditField(); });
    field_copy_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { CopyField(); });
    field_remove_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { RemoveField(); });
    fields_->Bind(wxEVT_LIST_ITEM_ACTIVATED, &AccountPanel::OnFieldActivated, this);
    auto sync_field_buttons = [this](wxListEvent& e) {
        const bool any = SelectedField() >= 0;
        field_edit_->Enable(any);
        field_copy_->Enable(any);
        field_remove_->Enable(any);
        e.Skip();
    };
    fields_->Bind(wxEVT_LIST_ITEM_SELECTED, sync_field_buttons);
    fields_->Bind(wxEVT_LIST_ITEM_DESELECTED, sync_field_buttons);

    history_->Bind(wxEVT_LIST_ITEM_SELECTED, [this](wxListEvent& e) {
        history_copy_->Enable(true);
        e.Skip();
    });
    history_copy_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        Account* a = Acc();
        const long row = history_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (!a || row < 0 || (size_t)row >= a->password_history().size()) return;
        host_->CopyText(U(a->password_history()[(size_t)row].password.c_str()), true, "Old password");
    });
}

void AccountPanel::Load(Database* db, const std::string& account_id) {
    Commit();
    db_ = db;
    id_ = account_id;
    Account* a = Acc();
    if (!a) {
        Unload();
        return;
    }

    loading_ = true;
    name_->ChangeValue(U(a->name()));
    username_->ChangeValue(U(a->username()));
    email_->ChangeValue(U(a->email()));
    url_->ChangeValue(U(a->url()));
    password_->SetRevealed(false);
    password_->SetValue(U(a->password().c_str()));
    totp_secret_->SetRevealed(false);
    totp_secret_->SetValue(U(a->totp_secret()));
    notes_->ChangeValue(U(a->notes()));
    favorite_->SetValue(a->favorite());
    archived_->SetValue(a->archived());
    loading_ = false;

    UpdateHeader();
    UpdateStrength();
    UpdateTotp();
    RefreshCustomFields();
    RefreshHistory();
    body_->Enable();
    Scroll(0, 0);
    FitInside();
}

void AccountPanel::Unload() {
    db_ = nullptr;
    id_.clear();
    loading_ = true;
    for (wxTextCtrl* t : {name_, username_, email_, url_, notes_}) t->ChangeValue(wxString());
    password_->SetRevealed(false);
    password_->Clear();
    totp_secret_->SetRevealed(false);
    totp_secret_->Clear();
    totp_code_value_.clear();
    fields_->DeleteAllItems();
    history_->DeleteAllItems();
    loading_ = false;
}

void AccountPanel::Commit() {
    if (loading_) return;
    Account* a = Acc();
    if (!a) return;

    bool changed = false, label = false;
    auto apply = [&](const wxString& now, const std::string& before, auto setter, bool is_label) {
        const std::string v = S(now);
        if (v == before) return;
        (a->*setter)(v);
        changed = true;
        label = label || is_label;
    };
    apply(name_->GetValue(), a->name(), &Account::set_name, true);
    apply(username_->GetValue(), a->username(), &Account::set_username, true);
    apply(email_->GetValue(), a->email(), &Account::set_email, false);
    apply(url_->GetValue().Trim().Trim(false), a->url(), &Account::set_url, false);
    apply(notes_->GetValue(), a->notes(), &Account::set_notes, false);
    wxString secret = totp_secret_->GetValue();
    secret.Replace(" ", "");
    apply(secret, a->totp_secret(), &Account::set_totp_secret, false);

    // SecureString compare is constant-time; set_password only archives
    // the old value when it actually differs.
    SecureString pw(S(password_->GetValue()));
    if (!(pw == a->password())) {
        a->set_password(pw);
        changed = true;
        RefreshHistory();
    }

    if (changed) {
        UpdateHeader();
        host_->AccountEdited(id_, label);
    }
}

void AccountPanel::Tick() {
    if (!Acc()) return;
    UpdateTotp();
}

void AccountPanel::UpdateHeader() {
    Account* a = Acc();
    if (!a) return;
    const wxString name = name_->GetValue();
    title_->SetLabel(name.empty() ? wxString("(unnamed)") : name);
    dates_->SetLabel("Created " + FormatTime(a->created_at()) + "    Modified " + FormatTime(a->modified_at()));
}

void AccountPanel::UpdateStrength() {
    const std::string pw = S(password_->GetValue());
    strength_->SetStrength(EstimateStrength(pw), pw.empty());
    Account* a = Acc();
    if (!a || pw.empty()) {
        age_->SetLabel(wxString());
        return;
    }
    const int days = a->password_age_days();
    if (days >= 365)
        age_->SetLabel(wxString::Format("Changed %d days ago, consider rotating it", days));
    else if (days > 1)
        age_->SetLabel(wxString::Format("Changed %d days ago", days));
    else
        age_->SetLabel(days == 1 ? "Changed yesterday" : "Changed today");
    age_->GetParent()->Layout();
}

void AccountPanel::UpdateTotp() {
    wxString secret = totp_secret_->GetValue();
    secret.Replace(" ", "");
    const bool has = !secret.empty();
    std::string code;
    bool valid = false;
    if (has) {
        try {
            code = TOTPGenerator::generate(S(secret));
            valid = true;
        } catch (const std::exception&) {
        }
    }
    totp_code_value_ = code;

    totp_row_->Show(totp_code_, valid);
    totp_row_->Show(totp_timer_, valid);
    totp_row_->Show(totp_left_, valid);
    totp_row_->Show(totp_copy_, valid);
    totp_row_->Show(totp_error_, has && !valid);
    if (valid) {
        const unsigned left = TOTPGenerator::seconds_remaining();
        // The code stays masked with the setup key, like the ImGui build:
        // whoever sees the key can produce every future code anyway.
        totp_code_->SetLabel(totp_secret_->IsRevealed() ? group_code(code)
                                                        : wxString::FromUTF8("\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2 "
                                                                             "\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2"));
        totp_timer_->SetValue((int)left);
        totp_left_->SetLabel(wxString::Format("%u s", left));
        totp_left_->SetForegroundColour(left <= 5 ? StrengthColour(0)
                                                  : wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
    }
    totp_row_->Layout();
    body_->Layout();
}

void AccountPanel::RefreshCustomFields() {
    fields_->DeleteAllItems();
    Account* a = Acc();
    if (!a) return;
    long row = 0;
    for (const CustomField& f : a->custom_fields()) {
        fields_->InsertItem(row, U(f.name));
        fields_->SetItem(row, 1, f.secret ? kMask : U(f.value.c_str()));
        ++row;
    }
    field_edit_->Disable();
    field_copy_->Disable();
    field_remove_->Disable();
}

void AccountPanel::RefreshHistory() {
    history_->DeleteAllItems();
    history_copy_->Disable();
    Account* a = Acc();
    const bool any = a && !a->password_history().empty();
    if (any) {
        long row = 0;
        // Fully masked, and not to its real length: the length of every past
        // password is information nobody needs on screen.
        for (const PasswordHistoryEntry& e : a->password_history()) {
            history_->InsertItem(row, FormatTime(e.changed_at));
            history_->SetItem(row, 1, kMask);
            ++row;
        }
    }
    body_->GetSizer()->Show(history_box_, any);
    body_->Layout();
    FitInside();
}

long AccountPanel::SelectedField() const {
    return fields_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
}

void AccountPanel::OnFieldActivated(wxListEvent&) { EditField(); }

void AccountPanel::AddField() {
    Account* a = Acc();
    if (!a) return;
    CustomFieldDialog dlg(this, "Add Field", wxEmptyString, wxEmptyString, false);
    if (dlg.ShowModal() != wxID_OK) return;
    a->add_custom_field(S(dlg.Name()), SecureString(S(dlg.Value())), dlg.Secret());
    RefreshCustomFields();
    FitInside();
    host_->AccountEdited(id_, false);
}

void AccountPanel::EditField() {
    Account* a = Acc();
    const long row = SelectedField();
    if (!a || row < 0 || (size_t)row >= a->custom_fields().size()) return;
    const CustomField& f = a->custom_fields()[(size_t)row];
    CustomFieldDialog dlg(this, "Edit Field", U(f.name), U(f.value.c_str()), f.secret);
    if (dlg.ShowModal() != wxID_OK) return;
    a->set_custom_field((size_t)row, S(dlg.Name()), SecureString(S(dlg.Value())), dlg.Secret());
    RefreshCustomFields();
    host_->AccountEdited(id_, false);
}

void AccountPanel::RemoveField() {
    Account* a = Acc();
    const long row = SelectedField();
    if (!a || row < 0 || (size_t)row >= a->custom_fields().size()) return;
    const wxString name = U(a->custom_fields()[(size_t)row].name);
    if (wxMessageBox("Remove the field \"" + name + "\"?", "Remove Field",
                     wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION, this) != wxYES)
        return;
    a->remove_custom_field((size_t)row);
    RefreshCustomFields();
    host_->AccountEdited(id_, false);
}

void AccountPanel::CopyField() {
    Account* a = Acc();
    const long row = SelectedField();
    if (!a || row < 0 || (size_t)row >= a->custom_fields().size()) return;
    const CustomField& f = a->custom_fields()[(size_t)row];
    host_->CopyText(U(f.value.c_str()), f.secret, U(f.name));
}

void AccountPanel::OpenUrl() {
    wxString url = url_->GetValue().Trim().Trim(false);
    if (url.empty()) return;
    // Only web links. A stored "file:" or custom-scheme URL could launch a
    // local program, and this button should never do that.
    const wxString lower = url.Lower();
    if (!lower.StartsWith("http://") && !lower.StartsWith("https://")) {
        if (url.Contains("://") || url.Contains(":")) {
            wxMessageBox("Only http and https links can be opened from here.", "Open Website",
                         wxOK | wxICON_INFORMATION, this);
            return;
        }
        url = "https://" + url;
    }
    wxLaunchDefaultBrowser(url);
}

void AccountPanel::FocusName() {
    name_->SetFocus();
    name_->SelectAll();
}

void AccountPanel::CopyUsername() {
    if (!username_->IsEmpty()) host_->CopyText(username_->GetValue(), false, "Username");
}

void AccountPanel::CopyPassword() {
    const wxString pw = password_->GetValue();
    if (!pw.empty()) host_->CopyText(pw, true, "Password");
}

void AccountPanel::CopyTotp() {
    if (!totp_code_value_.empty()) host_->CopyText(U(totp_code_value_), true, "Code");
}

void AccountPanel::GeneratePassword() {
    if (!Acc()) return;
    if (!password_->GetValue().empty() &&
        wxMessageBox("Replace the current password with a new one? The old one stays in "
                     "Previous passwords.",
                     "Generate Password", wxYES_NO | wxICON_QUESTION, this) != wxYES)
        return;
    wxString pw;
    if (!host_->ChooseGeneratedPassword(&pw) || pw.empty()) return;
    password_->SetValue(pw);
    UpdateStrength();
    Commit();
}

}  // namespace pasgen::wxui
