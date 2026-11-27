#include "dialogs.h"

#include <algorithm>
#include <cmath>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/checklst.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/radiobox.h>
#include <wx/radiobut.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/spinctrl.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/stdpaths.h>
#include <wx/textctrl.h>

#include "common.h"
#include "secret_field.h"
#include "strength.h"
#include "util/Config.hpp"
#include "util/PasswordGenerator.hpp"

namespace pasgen::wxui {

namespace {

using Level = Database::EncryptionLevel;
const Level kLevels[] = {Level::Standard, Level::HighSecurity, Level::FastCompatible};

wxRadioBox* make_level_box(wxWindow* parent, Level initial) {
    const wxString names[] = {
        U(Database::encryption_level_name(Level::Standard)) + " (recommended)",
        U(Database::encryption_level_name(Level::HighSecurity)),
        U(Database::encryption_level_name(Level::FastCompatible)),
    };
    auto* box = new wxRadioBox(parent, wxID_ANY, "Encryption", wxDefaultPosition, wxDefaultSize,
                               WXSIZEOF(names), names, 1, wxRA_SPECIFY_COLS);
    for (int i = 0; i < 3; ++i)
        if (kLevels[i] == initial) box->SetSelection(i);
    return box;
}

void bind_level_help(wxRadioBox* box, wxStaticText* help, wxWindow* dialog) {
    auto update = [box, help, dialog] {
        help->SetLabel(U(Database::encryption_level_description(kLevels[box->GetSelection()])));
        help->Wrap(dialog->FromDIP(420));
        dialog->Layout();
        dialog->Fit();
    };
    box->Bind(wxEVT_RADIOBOX, [update](wxCommandEvent&) { update(); });
    update();
}

wxStaticText* caption(wxWindow* parent, const wxString& text, int wrap_dip = 420) {
    auto* t = new wxStaticText(parent, wxID_ANY, text);
    t->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
    t->Wrap(parent->FromDIP(wrap_dip));
    return t;
}

// Label column for the two-column form grids every dialog here uses.
wxSizerFlags label_flags() { return wxSizerFlags().CenterVertical().Right(); }

// Indents a block under the radio button that controls it.
wxSizer* indented(wxWindow* w, wxSizer* inner) {
    auto* row = new wxBoxSizer(wxHORIZONTAL);
    row->AddSpacer(w->FromDIP(22));
    row->Add(inner, wxSizerFlags(1).Expand());
    return row;
}

wxSizer* indented(wxWindow* w, wxWindow* inner) {
    auto* row = new wxBoxSizer(wxHORIZONTAL);
    row->AddSpacer(w->FromDIP(22));
    row->Add(inner, wxSizerFlags(1).Expand());
    return row;
}

wxFlexGridSizer* form_grid(wxWindow* w) {
    auto* g = new wxFlexGridSizer(2, w->FromDIP(wxSize(10, 8)));
    g->AddGrowableCol(1, 1);
    return g;
}

wxString pick_save_path(wxWindow* parent, const wxString& title, const wxString& current,
                        const wxString& wildcard, const wxString& ext) {
    wxFileName fn(current);
    wxFileDialog dlg(parent, title, fn.GetPath(), fn.GetFullName(), wildcard,
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK) return current;
    wxString path = dlg.GetPath();
    if (!path.Lower().EndsWith(ext)) path += ext;
    return path;
}

}  // namespace

// ---------------------------------------------------------------------------
// MasterPasswordInput

MasterPasswordInput::MasterPasswordInput(wxWindow* parent, wxSizer* grid, const wxString& label) {
    password_ = new SecretField(parent, SecretField::WithToggle);
    confirm_ = new SecretField(parent, 0);
    meter_ = new StrengthBar(parent);

    grid->Add(new wxStaticText(parent, wxID_ANY, label), label_flags());
    grid->Add(password_, wxSizerFlags().Expand());
    grid->Add(new wxStaticText(parent, wxID_ANY, "Confirm:"), label_flags());
    grid->Add(confirm_, wxSizerFlags().Expand());
    grid->AddSpacer(0);
    grid->Add(meter_, wxSizerFlags().Expand());

    password_->OnChange = [this] { Update(); };
    // One Show/Hide for both entries.
    password_->OnRevealChanged = [this](bool shown) { confirm_->SetRevealed(shown); };
    Update();
}

void MasterPasswordInput::Update() {
    const std::string pw = S(password_->GetValue());
    meter_->SetStrength(EstimateStrength(pw), pw.empty());
}

bool MasterPasswordInput::Validate(wxWindow* dialog) {
    const wxString pw = password_->GetValue();
    if (pw.length() < kMinMasterLength) {
        wxMessageBox(wxString::Format("Use at least %d characters.\n\nThis password is the only thing "
                                      "protecting the file if someone copies it, so length matters "
                                      "more than anything else. A few random words work well.",
                                      (int)kMinMasterLength),
                     "Password too short", wxOK | wxICON_WARNING, dialog);
        password_->FocusEntry();
        return false;
    }
    if (pw != confirm_->GetValue()) {
        wxMessageBox("The two passwords do not match.", "Check password", wxOK | wxICON_WARNING, dialog);
        confirm_->FocusEntry();
        return false;
    }
    const Strength s = EstimateStrength(S(pw));
    if (s.bits < kWarnMasterBits) {
        const int answer = wxMessageBox(
            wxString::Format("This password looks guessable (%s).\n\nThe key derivation slows each "
                             "guess down, but it cannot save a password that is near the top of "
                             "an attacker's list. Use it anyway?",
                             DescribeStrength(s)),
            "Weak password", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, dialog);
        if (answer != wxYES) {
            password_->FocusEntry();
            return false;
        }
    }
    return true;
}

SecureString MasterPasswordInput::Password() const { return SecureString(S(password_->GetValue())); }

void MasterPasswordInput::Focus() { password_->FocusEntry(); }

void MasterPasswordInput::Wipe() {
    password_->Clear();
    confirm_->Clear();
}

// ---------------------------------------------------------------------------
// NewVaultDialog

NewVaultDialog::NewVaultDialog(wxWindow* parent, const wxString& suggested_path)
    : wxDialog(parent, wxID_ANY, "New Vault") {
    auto* grid = form_grid(this);

    path_ = new wxTextCtrl(this, wxID_ANY, suggested_path);
    path_->SetMinSize(FromDIP(wxSize(320, -1)));
    auto* browse = new wxButton(this, wxID_ANY, "Browse...");
    auto* path_row = new wxBoxSizer(wxHORIZONTAL);
    path_row->Add(path_, wxSizerFlags(1).CenterVertical());
    path_row->AddSpacer(FromDIP(4));
    path_row->Add(browse, wxSizerFlags().CenterVertical());
    grid->Add(new wxStaticText(this, wxID_ANY, "File:"), label_flags());
    grid->Add(path_row, wxSizerFlags().Expand());

    master_ = new MasterPasswordInput(this, grid, "Master password:");

    level_ = make_level_box(this, Level::Standard);
    level_help_ = caption(this, wxEmptyString);

    auto* root = new wxBoxSizer(wxVERTICAL);
    root->Add(caption(this, "Pick where to keep the vault and the master password that unlocks it. "
                            "There is no way to recover a forgotten master password."),
              wxSizerFlags().Border(wxALL, FromDIP(12)));
    root->Add(grid, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, FromDIP(12)));
    root->AddSpacer(FromDIP(10));
    root->Add(level_, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, FromDIP(12)));
    root->Add(level_help_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxTOP, FromDIP(12)));
    auto* buttons = CreateStdDialogButtonSizer(wxOK | wxCANCEL);
    FindWindow(wxID_OK)->SetLabel("Create Vault");
    root->Add(buttons, wxSizerFlags().Expand().Border(wxALL, FromDIP(12)));
    SetSizerAndFit(root);
    bind_level_help(level_, level_help_, this);
    CentreOnParent();

    browse->Bind(wxEVT_BUTTON, &NewVaultDialog::OnBrowse, this);
    Bind(wxEVT_BUTTON, &NewVaultDialog::OnOk, this, wxID_OK);
    CallAfter([this] { master_->Focus(); });
}

NewVaultDialog::~NewVaultDialog() {
    master_->Wipe();
    delete master_;
}

void NewVaultDialog::OnBrowse(wxCommandEvent&) {
    path_->SetValue(pick_save_path(this, "Create Vault", path_->GetValue(),
                                   "Pasgen vault (*.pif)|*.pif", ".pif"));
}

void NewVaultDialog::OnOk(wxCommandEvent&) {
    if (path_->GetValue().Trim().empty()) {
        wxMessageBox("Choose where to save the vault.", "New Vault", wxOK | wxICON_WARNING, this);
        path_->SetFocus();
        return;
    }
    if (wxFileName::FileExists(U(Path())) &&
        wxMessageBox("A file with that name already exists. Replace it with a new, empty vault?",
                     "New Vault", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, this) != wxYES)
        return;
    if (!master_->Validate(this)) return;
    EndModal(wxID_OK);
}

std::string NewVaultDialog::Path() const {
    wxString p = path_->GetValue().Trim().Trim(false);
    if (!p.Lower().EndsWith(".pif")) p += ".pif";
    return S(p);
}

Database::EncryptionLevel NewVaultDialog::Level() const { return kLevels[level_->GetSelection()]; }

// ---------------------------------------------------------------------------
// ChangeMasterPasswordDialog

ChangeMasterPasswordDialog::ChangeMasterPasswordDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, "Change Master Password") {
    auto* grid = form_grid(this);
    master_ = new MasterPasswordInput(this, grid, "New password:");

    auto* root = new wxBoxSizer(wxVERTICAL);
    root->Add(caption(this, "The vault is re-encrypted under the new password, with a fresh salt, "
                            "and saved straight away."),
              wxSizerFlags().Border(wxALL, FromDIP(12)));
    root->Add(grid, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, FromDIP(12)));
    auto* buttons = CreateStdDialogButtonSizer(wxOK | wxCANCEL);
    FindWindow(wxID_OK)->SetLabel("Change Password");
    root->Add(buttons, wxSizerFlags().Expand().Border(wxALL, FromDIP(12)));
    SetSizerAndFit(root);
    SetSize(FromDIP(wxSize(480, -1)));
    CentreOnParent();

    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (master_->Validate(this)) EndModal(wxID_OK);
    }, wxID_OK);
    CallAfter([this] { master_->Focus(); });
}

ChangeMasterPasswordDialog::~ChangeMasterPasswordDialog() {
    master_->Wipe();
    delete master_;
}

// ---------------------------------------------------------------------------
// EncryptionDialog

EncryptionDialog::EncryptionDialog(wxWindow* parent, const Database& db)
    : wxDialog(parent, wxID_ANY, "Encryption Settings") {
    auto* current = new wxStaticText(this, wxID_ANY,
                                     "Cipher:\t" + U(db.current_cipher_name()) + "\nKey derivation:\t" +
                                         U(db.current_kdf_description()));
    level_ = make_level_box(this, Level::Standard);
    help_ = caption(this, wxEmptyString);

    auto* now = new wxStaticBoxSizer(wxVERTICAL, this, "Currently");
    now->Add(current, wxSizerFlags().Border(wxALL, FromDIP(6)));

    auto* root = new wxBoxSizer(wxVERTICAL);
    root->Add(now, wxSizerFlags().Expand().Border(wxALL, FromDIP(12)));
    root->Add(level_, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, FromDIP(12)));
    root->Add(help_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxTOP, FromDIP(12)));
    root->Add(caption(this, "Applying re-encrypts the vault and saves it immediately."),
              wxSizerFlags().Border(wxLEFT | wxRIGHT | wxTOP, FromDIP(12)));
    auto* buttons = CreateStdDialogButtonSizer(wxOK | wxCANCEL);
    FindWindow(wxID_OK)->SetLabel("Apply");
    root->Add(buttons, wxSizerFlags().Expand().Border(wxALL, FromDIP(12)));
    SetSizerAndFit(root);
    bind_level_help(level_, help_, this);
    CentreOnParent();
}

Database::EncryptionLevel EncryptionDialog::Level() const { return kLevels[level_->GetSelection()]; }

// ---------------------------------------------------------------------------
// GeneratorDialog

GeneratorDialog::GeneratorDialog(wxWindow* parent, bool can_apply,
                                 std::function<void(const wxString&)> copy)
    : wxDialog(parent, wxID_ANY, "Generate Password"), copy_(std::move(copy)) {
    const PasswordGenDefaults d = Config::instance().get_password_gen_defaults();

    random_mode_ = new wxRadioButton(this, wxID_ANY, "Random characters", wxDefaultPosition,
                                     wxDefaultSize, wxRB_GROUP);
    phrase_mode_ = new wxRadioButton(this, wxID_ANY, "Passphrase (random words)");

    length_slider_ = new wxSlider(this, wxID_ANY, std::clamp(d.length, 6, 64), 6, 64);
    length_ = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                             wxSP_ARROW_KEYS, 6, 128, std::clamp(d.length, 6, 128));
    lower_ = new wxCheckBox(this, wxID_ANY, "a-z");
    upper_ = new wxCheckBox(this, wxID_ANY, "A-Z");
    digits_ = new wxCheckBox(this, wxID_ANY, "0-9");
    symbols_ = new wxCheckBox(this, wxID_ANY, "!@#$%");
    lower_->SetValue(d.lowercase);
    upper_->SetValue(d.uppercase);
    digits_->SetValue(d.digits);
    symbols_->SetValue(d.symbols);

    words_ = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                            wxSP_ARROW_KEYS, 3, 12, std::clamp(d.words, 3, 12));
    separator_ = new wxTextCtrl(this, wxID_ANY, U(d.separator));
    separator_->SetMaxLength(4);
    separator_->SetMinSize(FromDIP(wxSize(48, -1)));

    preview_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                              wxTE_READONLY | wxTE_CENTRE);
    preview_->SetFont(wxFont(wxFontInfo(GetFont().GetPointSize() + 3).Family(wxFONTFAMILY_TELETYPE)));
    preview_->SetMinSize(FromDIP(wxSize(440, -1)));
    meter_ = new StrengthBar(this);
    auto* regen = new wxButton(this, wxID_ANY, "&Regenerate");
    auto* copy_btn = new wxButton(this, wxID_ANY, "&Copy");

    // Layout
    auto* random_grid = form_grid(this);
    auto* len_row = new wxBoxSizer(wxHORIZONTAL);
    len_row->Add(length_slider_, wxSizerFlags(1).CenterVertical());
    len_row->AddSpacer(FromDIP(6));
    len_row->Add(length_, wxSizerFlags().CenterVertical());
    auto* len_label = new wxStaticText(this, wxID_ANY, "Length:");
    random_grid->Add(len_label, label_flags());
    random_grid->Add(len_row, wxSizerFlags().Expand());
    auto* sets = new wxBoxSizer(wxHORIZONTAL);
    for (wxCheckBox* c : {lower_, upper_, digits_, symbols_}) {
        sets->Add(c, wxSizerFlags().CenterVertical());
        sets->AddSpacer(FromDIP(12));
    }
    auto* sets_label = new wxStaticText(this, wxID_ANY, "Characters:");
    random_grid->Add(sets_label, label_flags());
    random_grid->Add(sets);

    auto* phrase_grid = form_grid(this);
    auto* words_label = new wxStaticText(this, wxID_ANY, "Words:");
    auto* sep_label = new wxStaticText(this, wxID_ANY, "Separator:");
    phrase_grid->Add(words_label, label_flags());
    phrase_grid->Add(words_);
    phrase_grid->Add(sep_label, label_flags());
    phrase_grid->Add(separator_);

    random_controls_ = {length_slider_, length_, lower_, upper_, digits_, symbols_, len_label, sets_label};
    phrase_controls_ = {words_, separator_, words_label, sep_label};

    auto* root = new wxBoxSizer(wxVERTICAL);
    const int pad = FromDIP(12);
    root->Add(preview_, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxTOP, pad));
    auto* under = new wxBoxSizer(wxHORIZONTAL);
    under->Add(meter_, wxSizerFlags(1).CenterVertical());
    under->Add(regen, wxSizerFlags().CenterVertical());
    under->AddSpacer(FromDIP(4));
    under->Add(copy_btn, wxSizerFlags().CenterVertical());
    root->Add(under, wxSizerFlags().Expand().Border(wxALL, pad));

    root->Add(random_mode_, wxSizerFlags().Border(wxLEFT | wxRIGHT, pad));
    root->Add(indented(this, random_grid), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxTOP, pad));
    root->AddSpacer(FromDIP(10));
    root->Add(phrase_mode_, wxSizerFlags().Border(wxLEFT | wxRIGHT, pad));
    root->Add(indented(this, phrase_grid), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxTOP, pad));
    root->Add(indented(this, caption(this, "Words come from the EFF list of 7,776, about 12.9 bits each.")),
              wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxTOP, pad));

    auto* buttons = CreateStdDialogButtonSizer(can_apply ? (wxOK | wxCANCEL) : wxCLOSE);
    if (can_apply) FindWindow(wxID_OK)->SetLabel("&Use Password");
    root->Add(buttons, wxSizerFlags().Expand().Border(wxALL, pad));
    SetSizerAndFit(root);
    CentreOnParent();
    if (!can_apply) SetEscapeId(wxID_CLOSE);

    (d.passphrase ? phrase_mode_ : random_mode_)->SetValue(true);
    SyncModeControls();
    Regenerate();

    auto on_option = [this](wxCommandEvent&) {
        SyncModeControls();
        Regenerate();
        SaveDefaults();
    };
    random_mode_->Bind(wxEVT_RADIOBUTTON, on_option);
    phrase_mode_->Bind(wxEVT_RADIOBUTTON, on_option);
    for (wxCheckBox* c : {lower_, upper_, digits_, symbols_}) c->Bind(wxEVT_CHECKBOX, on_option);
    length_slider_->Bind(wxEVT_SLIDER, [this, on_option](wxCommandEvent& e) {
        length_->SetValue(length_slider_->GetValue());
        on_option(e);
    });
    length_->Bind(wxEVT_SPINCTRL, [this, on_option](wxCommandEvent& e) {
        length_slider_->SetValue(std::min(length_->GetValue(), 64));
        on_option(e);
    });
    words_->Bind(wxEVT_SPINCTRL, on_option);
    separator_->Bind(wxEVT_TEXT, on_option);
    regen->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Regenerate(); });
    copy_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (copy_) copy_(preview_->GetValue());
    });
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxID_CLOSE); }, wxID_CLOSE);
}

GeneratorDialog::~GeneratorDialog() { preview_->ChangeValue(wxString()); }

void GeneratorDialog::SyncModeControls() {
    const bool phrase = phrase_mode_->GetValue();
    for (wxWindow* w : random_controls_) w->Enable(!phrase);
    for (wxWindow* w : phrase_controls_) w->Enable(phrase);
}

void GeneratorDialog::Regenerate() {
    double bits;
    std::string pw;
    if (phrase_mode_->GetValue()) {
        const size_t n = (size_t)words_->GetValue();
        pw = PasswordGenerator::generate_passphrase(n, S(separator_->GetValue()));
        bits = PasswordGenerator::passphrase_entropy_bits(n);
    } else {
        const size_t n = (size_t)length_->GetValue();
        bool lc = lower_->GetValue(), uc = upper_->GetValue(), dg = digits_->GetValue(),
             sy = symbols_->GetValue();
        if (!lc && !uc && !dg && !sy) lc = true;  // the generator does the same
        pw = PasswordGenerator::generate(n, lc, uc, dg, sy);
        bits = PasswordGenerator::entropy_bits(n, lc, uc, dg, sy);
    }
    preview_->ChangeValue(U(pw));
    SecureMemory::secure_zero(pw.data(), pw.size());
    meter_->SetStrength(StrengthFromBits(bits));
}

void GeneratorDialog::SaveDefaults() {
    PasswordGenDefaults d;
    d.length = length_->GetValue();
    d.lowercase = lower_->GetValue();
    d.uppercase = upper_->GetValue();
    d.digits = digits_->GetValue();
    d.symbols = symbols_->GetValue();
    d.passphrase = phrase_mode_->GetValue();
    d.words = words_->GetValue();
    d.separator = S(separator_->GetValue());
    Config::instance().set_password_gen_defaults(d);
}

wxString GeneratorDialog::Result() const { return preview_->GetValue(); }

// ---------------------------------------------------------------------------
// PreferencesDialog

PreferencesDialog::PreferencesDialog(wxWindow* parent) : wxDialog(parent, wxID_ANY, "Preferences") {
    Config& cfg = Config::instance();
    auto* box = new wxStaticBoxSizer(wxVERTICAL, this, "Security");
    wxWindow* in = box->GetStaticBox();
    autolock_ = new wxSpinCtrl(in, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                               wxSP_ARROW_KEYS, 0, 240, cfg.get_autolock_minutes());
    clipboard_ = new wxSpinCtrl(in, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                wxSP_ARROW_KEYS, 0, 600, cfg.get_clipboard_clear_seconds());

    auto* grid = form_grid(in);
    auto row = [&](const wxString& label, wxSpinCtrl* spin, const wxString& unit) {
        grid->Add(new wxStaticText(in, wxID_ANY, label), label_flags());
        auto* r = new wxBoxSizer(wxHORIZONTAL);
        r->Add(spin, wxSizerFlags().CenterVertical());
        r->AddSpacer(FromDIP(6));
        r->Add(new wxStaticText(in, wxID_ANY, unit), wxSizerFlags().CenterVertical());
        grid->Add(r);
    };
    row("Lock the vault after", autolock_, "minutes idle");
    row("Clear copied secrets after", clipboard_, "seconds");
    box->Add(grid, wxSizerFlags().Expand().Border(wxALL, FromDIP(8)));
    box->Add(caption(in, "0 turns either one off. Locking saves pending changes first."),
             wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(8)));

    auto* root = new wxBoxSizer(wxVERTICAL);
    root->Add(box, wxSizerFlags().Expand().Border(wxALL, FromDIP(12)));
    root->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL),
              wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12)));
    SetSizerAndFit(root);
    CentreOnParent();
}

int PreferencesDialog::AutoLockMinutes() const { return autolock_->GetValue(); }
int PreferencesDialog::ClipboardClearSeconds() const { return clipboard_->GetValue(); }

// ---------------------------------------------------------------------------
// ExportDialog

ExportDialog::ExportDialog(wxWindow* parent, const Database& db, const std::string& preselected_id)
    : wxDialog(parent, wxID_ANY, "Export to EIF", wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
    whole_ = new wxRadioButton(this, wxID_ANY, "Whole vault", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    chosen_ = new wxRadioButton(this, wxID_ANY, "Only these accounts:");

    std::vector<const Account*> all = db.get_all_accounts();
    std::sort(all.begin(), all.end(), [](const Account* a, const Account* b) {
        return wxString::FromUTF8(a->name()).CmpNoCase(wxString::FromUTF8(b->name())) < 0;
    });
    wxArrayString names;
    for (const Account* a : all) {
        ids_.push_back(a->id());
        names.Add(a->name().empty() ? wxString("(unnamed)") : U(a->name()));
    }
    accounts_ = new wxCheckListBox(this, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(-1, 150)), names);
    for (size_t i = 0; i < ids_.size(); ++i)
        if (ids_[i] == preselected_id) accounts_->Check((unsigned)i);

    std::string base = db.metadata().name.empty() ? "export" : db.metadata().name;
    wxFileName fn(U(db.file_path()));
    fn.SetFullName(U(base) + ".eif");
    path_ = new wxTextCtrl(this, wxID_ANY, fn.GetFullPath());
    auto* browse = new wxButton(this, wxID_ANY, "Browse...");

    auto* grid = form_grid(this);
    auto* path_row = new wxBoxSizer(wxHORIZONTAL);
    path_row->Add(path_, wxSizerFlags(1).CenterVertical());
    path_row->AddSpacer(FromDIP(4));
    path_row->Add(browse, wxSizerFlags().CenterVertical());
    grid->Add(new wxStaticText(this, wxID_ANY, "File:"), label_flags());
    grid->Add(path_row, wxSizerFlags().Expand());
    master_ = new MasterPasswordInput(this, grid, "Export password:");

    const int pad = FromDIP(12);
    auto* root = new wxBoxSizer(wxVERTICAL);
    root->Add(caption(this, "An .eif file is encrypted under its own password, separate from this "
                            "vault's master password, so it is safe to move or back up on its own. "
                            "Anyone with the file and that password can read every account in it."),
              wxSizerFlags().Border(wxALL, pad));
    root->Add(whole_, wxSizerFlags().Border(wxLEFT | wxRIGHT, pad));
    root->AddSpacer(FromDIP(4));
    root->Add(chosen_, wxSizerFlags().Border(wxLEFT | wxRIGHT, pad));
    root->Add(indented(this, accounts_), wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT | wxTOP, pad));
    root->AddSpacer(pad);
    root->Add(grid, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, pad));
    auto* buttons = CreateStdDialogButtonSizer(wxOK | wxCANCEL);
    FindWindow(wxID_OK)->SetLabel("Export");
    root->Add(buttons, wxSizerFlags().Expand().Border(wxALL, pad));
    SetSizerAndFit(root);
    SetSize(FromDIP(wxSize(520, -1)));
    CentreOnParent();

    (preselected_id.empty() ? whole_ : chosen_)->SetValue(true);
    accounts_->Enable(chosen_->GetValue());
    auto sync = [this](wxCommandEvent&) { accounts_->Enable(chosen_->GetValue()); };
    whole_->Bind(wxEVT_RADIOBUTTON, sync);
    chosen_->Bind(wxEVT_RADIOBUTTON, sync);
    browse->Bind(wxEVT_BUTTON, &ExportDialog::OnBrowse, this);
    Bind(wxEVT_BUTTON, &ExportDialog::OnOk, this, wxID_OK);
}

ExportDialog::~ExportDialog() {
    master_->Wipe();
    delete master_;
}

void ExportDialog::OnBrowse(wxCommandEvent&) {
    path_->SetValue(pick_save_path(this, "Export to EIF", path_->GetValue(),
                                   "Pasgen export (*.eif)|*.eif", ".eif"));
}

void ExportDialog::OnOk(wxCommandEvent&) {
    if (chosen_->GetValue()) {
        wxArrayInt checked;
        if (accounts_->GetCheckedItems(checked) == 0) {
            wxMessageBox("Tick at least one account, or export the whole vault.", "Export",
                         wxOK | wxICON_WARNING, this);
            return;
        }
    }
    if (path_->GetValue().Trim().empty()) {
        wxMessageBox("Choose where to save the export.", "Export", wxOK | wxICON_WARNING, this);
        return;
    }
    if (!master_->Validate(this)) return;
    EndModal(wxID_OK);
}

std::string ExportDialog::Path() const {
    wxString p = path_->GetValue().Trim().Trim(false);
    if (!p.Lower().EndsWith(".eif")) p += ".eif";
    return S(p);
}

bool ExportDialog::WholeVault() const { return whole_->GetValue(); }

std::set<std::string> ExportDialog::SelectedIds() const {
    std::set<std::string> out;
    for (size_t i = 0; i < ids_.size(); ++i)
        if (accounts_->IsChecked((unsigned)i)) out.insert(ids_[i]);
    return out;
}

// ---------------------------------------------------------------------------
// ImportDialog

ImportDialog::ImportDialog(wxWindow* parent) : wxDialog(parent, wxID_ANY, "Import from EIF") {
    path_ = new wxTextCtrl(this, wxID_ANY);
    path_->SetMinSize(FromDIP(wxSize(320, -1)));
    auto* browse = new wxButton(this, wxID_ANY, "Browse...");
    password_ = new SecretField(this, SecretField::WithToggle | SecretField::EnterKey);

    auto* grid = form_grid(this);
    auto* path_row = new wxBoxSizer(wxHORIZONTAL);
    path_row->Add(path_, wxSizerFlags(1).CenterVertical());
    path_row->AddSpacer(FromDIP(4));
    path_row->Add(browse, wxSizerFlags().CenterVertical());
    grid->Add(new wxStaticText(this, wxID_ANY, "File:"), label_flags());
    grid->Add(path_row, wxSizerFlags().Expand());
    grid->Add(new wxStaticText(this, wxID_ANY, "Export password:"), label_flags());
    grid->Add(password_, wxSizerFlags().Expand());

    const int pad = FromDIP(12);
    auto* root = new wxBoxSizer(wxVERTICAL);
    root->Add(caption(this, "Every account in the file is added to this vault as a new entry. "
                            "Categories are matched by name and created if missing."),
              wxSizerFlags().Border(wxALL, pad));
    root->Add(grid, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, pad));
    auto* buttons = CreateStdDialogButtonSizer(wxOK | wxCANCEL);
    FindWindow(wxID_OK)->SetLabel("Import");
    root->Add(buttons, wxSizerFlags().Expand().Border(wxALL, pad));
    SetSizerAndFit(root);
    CentreOnParent();

    browse->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        wxFileDialog dlg(this, "Import from EIF", wxEmptyString, wxEmptyString,
                         "Pasgen export (*.eif)|*.eif|All files|*", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dlg.ShowModal() == wxID_OK) {
            path_->SetValue(dlg.GetPath());
            password_->FocusEntry();
        }
    });
    auto ok = [this] {
        if (path_->GetValue().empty() || password_->GetValue().empty()) {
            wxMessageBox("Choose a file and enter its export password.", "Import",
                         wxOK | wxICON_WARNING, this);
            return;
        }
        EndModal(wxID_OK);
    };
    password_->OnEnter = ok;
    Bind(wxEVT_BUTTON, [ok](wxCommandEvent&) { ok(); }, wxID_OK);
}

ImportDialog::~ImportDialog() { password_->Clear(); }

std::string ImportDialog::Path() const { return S(path_->GetValue()); }
SecureString ImportDialog::Password() const { return SecureString(S(password_->GetValue())); }

// ---------------------------------------------------------------------------
// CustomFieldDialog

CustomFieldDialog::CustomFieldDialog(wxWindow* parent, const wxString& title, const wxString& name,
                                     const wxString& value, bool secret)
    : wxDialog(parent, wxID_ANY, title) {
    name_ = new wxTextCtrl(this, wxID_ANY, name);
    name_->SetMinSize(FromDIP(wxSize(300, -1)));
    value_ = new SecretField(this, SecretField::WithToggle);
    value_->SetValue(value);
    value_->SetRevealed(!secret);
    secret_ = new wxCheckBox(this, wxID_ANY, "Secret (mask the value, and keep it out of clipboard history)");
    secret_->SetValue(secret);

    auto* grid = form_grid(this);
    grid->Add(new wxStaticText(this, wxID_ANY, "Name:"), label_flags());
    grid->Add(name_, wxSizerFlags().Expand());
    grid->Add(new wxStaticText(this, wxID_ANY, "Value:"), label_flags());
    grid->Add(value_, wxSizerFlags().Expand());
    grid->AddSpacer(0);
    grid->Add(secret_);

    auto* root = new wxBoxSizer(wxVERTICAL);
    root->Add(grid, wxSizerFlags().Expand().Border(wxALL, FromDIP(12)));
    root->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL),
              wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12)));
    SetSizerAndFit(root);
    CentreOnParent();

    secret_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { value_->SetRevealed(!secret_->GetValue()); });
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (name_->GetValue().Trim().empty()) {
            wxMessageBox("Give the field a name.", GetTitle(), wxOK | wxICON_WARNING, this);
            name_->SetFocus();
            return;
        }
        EndModal(wxID_OK);
    }, wxID_OK);
    CallAfter([this] { name_->SetFocus(); });
}

CustomFieldDialog::~CustomFieldDialog() { value_->Clear(); }

wxString CustomFieldDialog::Name() const { return name_->GetValue().Trim().Trim(false); }
wxString CustomFieldDialog::Value() const { return value_->GetValue(); }
bool CustomFieldDialog::Secret() const { return secret_->GetValue(); }

}  // namespace pasgen::wxui
