#pragma once

#include <string>

#include <wx/scrolwin.h>

namespace pasgen {
class Account;
class Database;
}  // namespace pasgen

class wxButton;
class wxCheckBox;
class wxGauge;
class wxListCtrl;
class wxListEvent;
class wxStaticBoxSizer;
class wxStaticText;
class wxTextCtrl;

namespace pasgen::wxui {

class SecretField;
class StrengthBar;

// What the account editor needs from the window that hosts it.
class AccountHost {
public:
    virtual ~AccountHost() = default;
    // Something on the account changed; `label_changed` when the tree entry
    // (name, favourite, archived) needs redrawing.
    virtual void AccountEdited(const std::string& id, bool label_changed) = 0;
    virtual void CopyText(const wxString& text, bool secret, const wxString& what) = 0;
    // Runs the generator; returns false if the user cancelled.
    virtual bool ChooseGeneratedPassword(wxString* out) = 0;
};

// Editor for one account. Edits are written back to the Account when a
// field loses focus or Enter is pressed (Commit), never per keystroke: every
// password write archives the old value into a 10-entry history, so writing
// on each keystroke would fill it with partial passwords.
class AccountPanel : public wxScrolledWindow {
public:
    AccountPanel(wxWindow* parent, AccountHost* host);

    void Load(Database* db, const std::string& account_id);
    void Unload();            // wipes every field
    void Commit();            // write pending edits back to the account
    void Tick();              // once a second: TOTP countdown, password age
    bool HasAccount() const { return db_ != nullptr; }
    const std::string& AccountId() const { return id_; }

    void FocusName();
    void CopyUsername();
    void CopyPassword();
    void CopyTotp();
    void GeneratePassword();

private:
    Account* Acc() const;
    void Build();
    wxStaticBoxSizer* Section(const wxString& title);
    void UpdateHeader();
    void UpdateStrength();
    void UpdateTotp();
    void RefreshCustomFields();
    void RefreshHistory();
    void OpenUrl();
    void OnFieldActivated(wxListEvent&);
    void AddField();
    void EditField();
    void RemoveField();
    void CopyField();
    long SelectedField() const;

    AccountHost* host_;
    Database* db_ = nullptr;
    std::string id_;
    bool loading_ = false;

    wxPanel* body_ = nullptr;
    wxStaticText* title_ = nullptr;
    wxStaticText* dates_ = nullptr;
    wxCheckBox* favorite_ = nullptr;
    wxCheckBox* archived_ = nullptr;

    wxTextCtrl* name_ = nullptr;
    wxTextCtrl* username_ = nullptr;
    wxTextCtrl* email_ = nullptr;
    wxTextCtrl* url_ = nullptr;
    SecretField* password_ = nullptr;
    StrengthBar* strength_ = nullptr;
    wxStaticText* age_ = nullptr;

    SecretField* totp_secret_ = nullptr;
    wxStaticText* totp_code_ = nullptr;
    wxGauge* totp_timer_ = nullptr;
    wxStaticText* totp_left_ = nullptr;
    wxButton* totp_copy_ = nullptr;
    wxStaticText* totp_error_ = nullptr;
    wxSizer* totp_row_ = nullptr;
    std::string totp_code_value_;

    wxListCtrl* fields_ = nullptr;
    wxButton* field_edit_ = nullptr;
    wxButton* field_copy_ = nullptr;
    wxButton* field_remove_ = nullptr;

    wxTextCtrl* notes_ = nullptr;

    wxStaticBoxSizer* history_box_ = nullptr;
    wxListCtrl* history_ = nullptr;
    wxButton* history_copy_ = nullptr;
};

}  // namespace pasgen::wxui
