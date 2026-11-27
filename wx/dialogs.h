#pragma once

#include <functional>
#include <set>
#include <string>
#include <vector>

#include <wx/dialog.h>

#include "core/Database.hpp"
#include "crypto/SecureMemory.hpp"

class wxCheckBox;
class wxCheckListBox;
class wxRadioBox;
class wxRadioButton;
class wxSlider;
class wxSpinCtrl;
class wxStaticText;
class wxTextCtrl;

namespace pasgen::wxui {

class SecretField;
class StrengthBar;

// New password + confirmation + strength meter, with the master-password
// rules (minimum length, confirmation for a weak choice) in Validate().
class MasterPasswordInput {
public:
    MasterPasswordInput(wxWindow* parent, wxSizer* grid, const wxString& label);
    // Shows a message and returns false if the password should not be used.
    bool Validate(wxWindow* dialog);
    SecureString Password() const;
    void Focus();
    void Wipe();

private:
    void Update();
    SecretField* password_ = nullptr;
    SecretField* confirm_ = nullptr;
    StrengthBar* meter_ = nullptr;
};

// The only thing between launch and the vault: which file, and its master
// password. `unlock` is called with both and returns an error message, or
// an empty string on success; a wrong password keeps the prompt open.
class UnlockDialog : public wxDialog {
public:
    enum { ID_NEW_VAULT = wxID_HIGHEST + 100 };
    using UnlockFn = std::function<wxString(const std::string& path, const SecureString& pw)>;

    UnlockDialog(wxWindow* parent, const wxString& path, UnlockFn unlock);
    ~UnlockDialog() override;

private:
    void SetPath(const wxString& path);
    void TryUnlock();

    UnlockFn unlock_;
    wxString path_;
    wxStaticText* file_ = nullptr;
    wxStaticText* folder_ = nullptr;
    SecretField* password_ = nullptr;
    wxStaticText* error_ = nullptr;
};

class NewVaultDialog : public wxDialog {
public:
    NewVaultDialog(wxWindow* parent, const wxString& suggested_path);
    ~NewVaultDialog() override;

    std::string Path() const;
    SecureString Password() const { return master_->Password(); }
    Database::EncryptionLevel Level() const;

private:
    void OnBrowse(wxCommandEvent&);
    void OnOk(wxCommandEvent&);

    wxTextCtrl* path_ = nullptr;
    MasterPasswordInput* master_ = nullptr;
    wxRadioBox* level_ = nullptr;
    wxStaticText* level_help_ = nullptr;
};

class ChangeMasterPasswordDialog : public wxDialog {
public:
    explicit ChangeMasterPasswordDialog(wxWindow* parent);
    ~ChangeMasterPasswordDialog() override;
    SecureString Password() const { return master_->Password(); }

private:
    MasterPasswordInput* master_ = nullptr;
};

class EncryptionDialog : public wxDialog {
public:
    EncryptionDialog(wxWindow* parent, const Database& db);
    Database::EncryptionLevel Level() const;

private:
    wxRadioBox* level_ = nullptr;
    wxStaticText* help_ = nullptr;
};

// Password generator. With `can_apply` the affirmative button is "Use
// Password" and Result() is what to put in the account; otherwise the
// dialog is a standalone generator with Copy and Close.
class GeneratorDialog : public wxDialog {
public:
    GeneratorDialog(wxWindow* parent, bool can_apply, std::function<void(const wxString&)> copy);
    ~GeneratorDialog() override;
    wxString Result() const;

private:
    void Regenerate();
    void SyncModeControls();
    void SaveDefaults();

    std::function<void(const wxString&)> copy_;
    wxRadioButton* random_mode_ = nullptr;
    wxRadioButton* phrase_mode_ = nullptr;
    wxSlider* length_slider_ = nullptr;
    wxSpinCtrl* length_ = nullptr;
    wxCheckBox* lower_ = nullptr;
    wxCheckBox* upper_ = nullptr;
    wxCheckBox* digits_ = nullptr;
    wxCheckBox* symbols_ = nullptr;
    wxSpinCtrl* words_ = nullptr;
    wxTextCtrl* separator_ = nullptr;
    wxTextCtrl* preview_ = nullptr;
    StrengthBar* meter_ = nullptr;
    std::vector<wxWindow*> random_controls_;
    std::vector<wxWindow*> phrase_controls_;
};

class PreferencesDialog : public wxDialog {
public:
    explicit PreferencesDialog(wxWindow* parent);
    int AutoLockMinutes() const;
    int ClipboardClearSeconds() const;

private:
    wxSpinCtrl* autolock_ = nullptr;
    wxSpinCtrl* clipboard_ = nullptr;
};

class ExportDialog : public wxDialog {
public:
    ExportDialog(wxWindow* parent, const Database& db, const std::string& preselected_id);
    ~ExportDialog() override;

    std::string Path() const;
    SecureString Password() const { return master_->Password(); }
    // Empty = whole vault.
    std::set<std::string> SelectedIds() const;
    bool WholeVault() const;

private:
    void OnBrowse(wxCommandEvent&);
    void OnOk(wxCommandEvent&);

    std::vector<std::string> ids_;
    wxRadioButton* whole_ = nullptr;
    wxRadioButton* chosen_ = nullptr;
    wxCheckListBox* accounts_ = nullptr;
    wxTextCtrl* path_ = nullptr;
    MasterPasswordInput* master_ = nullptr;
};

class ImportDialog : public wxDialog {
public:
    explicit ImportDialog(wxWindow* parent);
    ~ImportDialog() override;
    std::string Path() const;
    SecureString Password() const;

private:
    wxTextCtrl* path_ = nullptr;
    SecretField* password_ = nullptr;
};

class CustomFieldDialog : public wxDialog {
public:
    CustomFieldDialog(wxWindow* parent, const wxString& title, const wxString& name,
                      const wxString& value, bool secret);
    ~CustomFieldDialog() override;
    wxString Name() const;
    wxString Value() const;
    bool Secret() const;

private:
    wxTextCtrl* name_ = nullptr;
    SecretField* value_ = nullptr;
    wxCheckBox* secret_ = nullptr;
};

}  // namespace pasgen::wxui
