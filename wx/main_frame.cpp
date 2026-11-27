#include "main_frame.h"

#include <algorithm>
#include <climits>
#include <map>

#include <wx/aboutdlg.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/settings.h>
#include <wx/simplebook.h>
#include <wx/sizer.h>
#include <wx/splitter.h>
#include <wx/srchctrl.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>
#include <wx/statusbr.h>
#include <wx/textdlg.h>
#include <wx/time.h>
#include <wx/utils.h>

#include "common.h"
#include "core/ExportFile.hpp"
#include "dialogs.h"
#include "icon_data.h"
#include "secure_clipboard.h"
#include "util/Config.hpp"

namespace pasgen::wxui {

namespace {

wxLongLong g_last_activity = wxGetLocalTimeMillis();

enum {
    ID_LOCK = wxID_HIGHEST + 1,
    ID_IMPORT,
    ID_EXPORT,
    ID_ADD_ACCOUNT,
    ID_DELETE_ACCOUNT,
    ID_COPY_USER,
    ID_COPY_PASS,
    ID_COPY_TOTP,
    ID_GENERATE,
    ID_FIND,
    ID_SHOW_ARCHIVED,
    ID_EXPAND_ALL,
    ID_COLLAPSE_ALL,
    ID_ADD_CATEGORY,
    ID_RENAME_VAULT,
    ID_CHANGE_MASTER,
    ID_ENCRYPTION,
    ID_TIMER,
    // Context menus
    ID_CTX_RENAME,
    ID_CTX_DELETE,
    ID_CTX_UP,
    ID_CTX_DOWN,
    ID_CTX_ADD_HERE,
    ID_CTX_FAVORITE,
    ID_CTX_ARCHIVED,
    ID_CTX_EXPORT,
    ID_CTX_COLOR_AUTO,
    ID_CTX_COLOR_0,                                   // .. +7
    ID_CTX_MOVE_0 = ID_CTX_COLOR_0 + kCategoryColours,  // .. one per category
};

enum StatusField { FieldMessage, FieldCount, FieldSaved, FieldTotal };

const int kUncategorizedImage = kCategoryColours;  // grey dot

std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

wxString friendly_open_error(const std::exception& e) {
    const wxString msg = U(e.what());
    if (msg.Contains("Authentication failed")) return "Wrong password, or the file has been damaged.";
    return msg;
}

wxIconBundle app_icons() {
    wxIconBundle icons;
    auto add = [&](const unsigned char* data, size_t size) {
        wxIcon icon;
        icon.CopyFromBitmap(wxBitmap::NewFromPNGData(data, size));
        icons.AddIcon(icon);
    };
    add(kIconPng16, sizeof(kIconPng16));
    add(kIconPng32, sizeof(kIconPng32));
    add(kIconPng48, sizeof(kIconPng48));
    add(kIconPng128, sizeof(kIconPng128));
    return icons;
}

}  // namespace

void MarkActivity() { g_last_activity = wxGetLocalTimeMillis(); }

// ---------------------------------------------------------------------------
// Construction

MainFrame::MainFrame(const wxString& vault_path)
    : wxFrame(nullptr, wxID_ANY, "Pasgen"), timer_(this, ID_TIMER) {
    SetIcons(app_icons());
    autolock_minutes_ = Config::instance().get_autolock_minutes();
    clipboard_seconds_ = Config::instance().get_clipboard_clear_seconds();

    BuildMenu();
    BuildLayout();
    CreateStatusBar(FieldTotal);
    const int widths[FieldTotal] = {-1, FromDIP(110), FromDIP(160)};
    SetStatusWidths(FieldTotal, widths);

    SetMinSize(FromDIP(wxSize(720, 460)));
    SetSize(FromDIP(wxSize(1060, 700)));
    Centre();

    Bind(wxEVT_TIMER, &MainFrame::OnTimer, this, ID_TIMER);
    Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnClose, this);
    timer_.Start(1000);
    UpdateTitle();

    const wxString path = vault_path;
    CallAfter([this, path] { PromptUnlock(path); });
}

MainFrame::~MainFrame() { timer_.Stop(); }

void MainFrame::BuildMenu() {
    auto* file = new wxMenu;
    file->Append(wxID_NEW, "&New Vault...\tCtrl+Shift+N");
    file->Append(wxID_OPEN, "&Open Vault...\tCtrl+O");
    file->Append(wxID_SAVE, "&Save\tCtrl+S");
    file->Append(wxID_SAVEAS, "Save &As...\tCtrl+Shift+S");
    file->AppendSeparator();
    file->Append(ID_IMPORT, "&Import from EIF...");
    file->Append(ID_EXPORT, "&Export to EIF...");
    file->AppendSeparator();
    file->Append(ID_LOCK, "&Lock\tCtrl+L", "Save, close the vault and forget the key");
    file->AppendSeparator();
    file->Append(wxID_EXIT, "E&xit\tCtrl+Q");

    auto* edit = new wxMenu;
    edit->Append(ID_ADD_ACCOUNT, "&Add Account\tCtrl+N");
    edit->Append(ID_DELETE_ACCOUNT, "&Delete Account...\tCtrl+Del");
    edit->AppendSeparator();
    edit->Append(ID_COPY_USER, "Copy &Username\tCtrl+B");
    edit->Append(ID_COPY_PASS, "Copy &Password\tCtrl+Shift+C");
    edit->Append(ID_COPY_TOTP, "Copy Two-factor &Code\tCtrl+T");
    edit->AppendSeparator();
    edit->Append(ID_GENERATE, "&Generate Password...\tCtrl+G");
    edit->Append(ID_FIND, "&Find...\tCtrl+F");
    edit->AppendSeparator();
    edit->Append(wxID_PREFERENCES, "P&references...");

    auto* view = new wxMenu;
    view->AppendCheckItem(ID_SHOW_ARCHIVED, "Show &Archived");
    view->AppendSeparator();
    view->Append(ID_EXPAND_ALL, "&Expand All");
    view->Append(ID_COLLAPSE_ALL, "&Collapse All");

    auto* vault = new wxMenu;
    vault->Append(ID_ADD_CATEGORY, "Add &Category...");
    vault->Append(ID_RENAME_VAULT, "&Rename Vault...");
    vault->AppendSeparator();
    vault->Append(ID_CHANGE_MASTER, "Change &Master Password...");
    vault->Append(ID_ENCRYPTION, "&Encryption Settings...");

    auto* help = new wxMenu;
    help->Append(wxID_ABOUT, "&About Pasgen");

    auto* bar = new wxMenuBar;
    bar->Append(file, "&File");
    bar->Append(edit, "&Edit");
    bar->Append(view, "&View");
    bar->Append(vault, "&Vault");
    bar->Append(help, "&Help");
    SetMenuBar(bar);

    auto on = [this](int id, auto fn) { Bind(wxEVT_MENU, [fn](wxCommandEvent&) { fn(); }, id); };
    on(wxID_NEW, [this] { NewVault(); });
    on(wxID_OPEN, [this] { OpenOther(); });
    on(wxID_SAVE, [this] { Save(); });
    on(wxID_SAVEAS, [this] { SaveAs(); });
    on(ID_IMPORT, [this] { ImportAccounts(); });
    on(ID_EXPORT, [this] { ExportAccounts(SelectedAccountId()); });
    on(ID_LOCK, [this] {
        if (Lock("Locked.")) CallAfter([this] { PromptUnlock(); });
    });
    on(wxID_EXIT, [this] { Close(); });
    on(ID_ADD_ACCOUNT, [this] { AddAccount(); });
    on(ID_DELETE_ACCOUNT, [this] { DeleteAccount(SelectedAccountId()); });
    on(ID_COPY_USER, [this] { editor_->CopyUsername(); });
    on(ID_COPY_PASS, [this] { editor_->CopyPassword(); });
    on(ID_COPY_TOTP, [this] { editor_->CopyTotp(); });
    on(ID_GENERATE, [this] { ShowGenerator(); });
    on(ID_FIND, [this] {
        search_->SetFocus();
        search_->SelectAll();
    });
    on(wxID_PREFERENCES, [this] { ShowPreferences(); });
    on(ID_SHOW_ARCHIVED, [this] {
        show_archived_->SetValue(GetMenuBar()->IsChecked(ID_SHOW_ARCHIVED));
        RebuildTree();
    });
    on(ID_EXPAND_ALL, [this] {
        collapsed_.clear();
        tree_->ExpandAll();
    });
    on(ID_COLLAPSE_ALL, [this] {
        wxTreeItemIdValue cookie;
        for (auto it = tree_->GetFirstChild(tree_->GetRootItem(), cookie); it.IsOk();
             it = tree_->GetNextChild(tree_->GetRootItem(), cookie))
            tree_->Collapse(it);
    });
    on(ID_ADD_CATEGORY, [this] { AddCategory(); });
    on(ID_RENAME_VAULT, [this] { RenameVault(); });
    on(ID_CHANGE_MASTER, [this] { ChangeMasterPassword(); });
    on(ID_ENCRYPTION, [this] { ChangeEncryption(); });
    on(wxID_ABOUT, [this] { ShowAbout(); });

    // Enabled states
    auto needs_vault = [this](wxUpdateUIEvent& e) { e.Enable(db_ != nullptr); };
    for (int id : {(int)wxID_SAVEAS, (int)ID_IMPORT, (int)ID_EXPORT, (int)ID_LOCK, (int)ID_ADD_ACCOUNT,
                   (int)ID_FIND, (int)ID_SHOW_ARCHIVED, (int)ID_EXPAND_ALL, (int)ID_COLLAPSE_ALL,
                   (int)ID_ADD_CATEGORY, (int)ID_RENAME_VAULT, (int)ID_CHANGE_MASTER, (int)ID_ENCRYPTION})
        Bind(wxEVT_UPDATE_UI, needs_vault, id);
    Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) { e.Enable(db_ && db_->is_dirty()); }, wxID_SAVE);
    auto needs_account = [this](wxUpdateUIEvent& e) { e.Enable(db_ && editor_->HasAccount()); };
    for (int id : {(int)ID_DELETE_ACCOUNT, (int)ID_COPY_USER, (int)ID_COPY_PASS, (int)ID_COPY_TOTP})
        Bind(wxEVT_UPDATE_UI, needs_account, id);
}

void MainFrame::BuildLayout() {
    root_ = new wxSimplebook(this);

    // Page 0: shown whenever no vault is open. Deliberately plain: the
    // unlock prompt is a dialog over it, this is only what remains if
    // that prompt is dismissed.
    auto* locked = new wxPanel(root_);
    auto* lock_icon = new wxStaticBitmap(
        locked, wxID_ANY, wxBitmap::NewFromPNGData(kIconPng128, sizeof(kIconPng128)));
    locked_text_ = new wxStaticText(locked, wxID_ANY, "No vault open");
    locked_text_->SetFont(locked_text_->GetFont().Scaled(1.3f));
    auto* unlock_btn = new wxButton(locked, wxID_ANY, "Unlock...");
    auto* new_btn = new wxButton(locked, wxID_ANY, "New Vault...");
    auto* lrow = new wxBoxSizer(wxHORIZONTAL);
    lrow->Add(unlock_btn);
    lrow->AddSpacer(FromDIP(8));
    lrow->Add(new_btn);
    auto* lcol = new wxBoxSizer(wxVERTICAL);
    lcol->AddStretchSpacer(2);
    lcol->Add(lock_icon, wxSizerFlags().CenterHorizontal());
    lcol->AddSpacer(FromDIP(12));
    lcol->Add(locked_text_, wxSizerFlags().CenterHorizontal());
    lcol->AddSpacer(FromDIP(16));
    lcol->Add(lrow, wxSizerFlags().CenterHorizontal());
    lcol->AddStretchSpacer(3);
    locked->SetSizer(lcol);
    unlock_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { PromptUnlock(); });
    new_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { NewVault(); });

    // Page 1: the vault. Accounts on the left, the selected one on the right.
    split_ = new wxSplitterWindow(root_, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                  wxSP_LIVE_UPDATE | wxSP_3DSASH);
    auto* left = new wxPanel(split_);
    search_ = new wxSearchCtrl(left, wxID_ANY);
    search_->SetDescriptiveText("Search accounts");
    search_->ShowCancelButton(true);
    tree_ = new wxTreeCtrl(left, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                           wxTR_HIDE_ROOT | wxTR_HAS_BUTTONS | wxTR_NO_LINES | wxTR_FULL_ROW_HIGHLIGHT |
                               wxTR_SINGLE | wxTR_TWIST_BUTTONS);
    wxVector<wxBitmapBundle> dots;
    for (int i = 0; i < kCategoryColours; ++i) dots.push_back(ColourDot(CategoryColour(i)));
    dots.push_back(ColourDot(CategoryColour(-1)));
    tree_->SetImages(dots);

    auto* add_btn = new wxButton(left, ID_ADD_ACCOUNT, "Add Account");
    show_archived_ = new wxCheckBox(left, wxID_ANY, "Show archived");
    auto* lbottom = new wxBoxSizer(wxHORIZONTAL);
    lbottom->Add(add_btn, wxSizerFlags().CenterVertical());
    lbottom->AddStretchSpacer();
    lbottom->Add(show_archived_, wxSizerFlags().CenterVertical());

    const int pad = FromDIP(8);
    auto* lsizer = new wxBoxSizer(wxVERTICAL);
    lsizer->Add(search_, wxSizerFlags().Expand().Border(wxLEFT | wxTOP | wxRIGHT, pad));
    lsizer->Add(tree_, wxSizerFlags(1).Expand().Border(wxALL, pad));
    lsizer->Add(lbottom, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, pad));
    left->SetSizer(lsizer);

    right_ = new wxSimplebook(split_);
    auto* placeholder = new wxPanel(right_);
    auto* ph_text = new wxStaticText(placeholder, wxID_ANY,
                                     "Select an account, or add one with Edit > Add Account.");
    ph_text->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
    auto* ph_sizer = new wxBoxSizer(wxVERTICAL);
    ph_sizer->AddStretchSpacer();
    ph_sizer->Add(ph_text, wxSizerFlags().CenterHorizontal());
    ph_sizer->AddStretchSpacer();
    placeholder->SetSizer(ph_sizer);
    editor_ = new AccountPanel(right_, this);
    right_->AddPage(placeholder, wxEmptyString);
    right_->AddPage(editor_, wxEmptyString);

    split_->SetMinimumPaneSize(FromDIP(200));
    split_->SetSashGravity(0.0);  // extra width goes to the editor
    split_->SplitVertically(left, right_, FromDIP(300));

    root_->AddPage(locked, wxEmptyString);
    root_->AddPage(split_, wxEmptyString);

    search_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { RebuildTree(); });
    search_->Bind(wxEVT_SEARCH_CANCEL, [this](wxCommandEvent&) { search_->Clear(); });
    show_archived_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
        GetMenuBar()->Check(ID_SHOW_ARCHIVED, show_archived_->GetValue());
        RebuildTree();
    });
    add_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { AddAccount(); });
    tree_->Bind(wxEVT_TREE_SEL_CHANGED, &MainFrame::OnSelectionChanged, this);
    tree_->Bind(wxEVT_TREE_ITEM_MENU, &MainFrame::OnTreeMenu, this);
    tree_->Bind(wxEVT_TREE_ITEM_COLLAPSED, [this](wxTreeEvent& e) {
        auto* n = static_cast<Node*>(tree_->GetItemData(e.GetItem()));
        if (!rebuilding_ && n && n->is_category && search_->IsEmpty()) collapsed_.insert(n->id);
    });
    tree_->Bind(wxEVT_TREE_ITEM_EXPANDED, [this](wxTreeEvent& e) {
        auto* n = static_cast<Node*>(tree_->GetItemData(e.GetItem()));
        if (!rebuilding_ && n && n->is_category) collapsed_.erase(n->id);
    });
}

// ---------------------------------------------------------------------------
// Vault lifecycle

void MainFrame::PromptUnlock(const wxString& path) {
    if (db_ || modal_depth_ > 0) return;
    wxString p = path.empty() ? U(Config::instance().get_last_database_path()) : path;
    UnlockDialog dlg(this, p, [this](const std::string& file, const SecureString& pw) {
        return TryOpen(file, pw);
    });
    const int r = ShowModalCounted(dlg);
    if (r == UnlockDialog::ID_NEW_VAULT) NewVault();
}

wxString MainFrame::TryOpen(const std::string& path, const SecureString& pw) {
    std::unique_ptr<Database> opened;
    try {
        RunBusy("Unlocking", "Deriving the vault key...", [&] { opened = Database::open(path, pw); });
    } catch (const std::exception& e) {
        return friendly_open_error(e);
    }
    db_ = std::move(opened);
    Config::instance().set_last_database_path(path);
    VaultOpened();
    SetStatus("Unlocked " + wxFileName(U(path)).GetFullName() + ".");
    return wxString();
}

void MainFrame::VaultOpened() {
    collapsed_.clear();
    search_->ChangeValue(wxString());
    editor_->Unload();
    right_->SetSelection(0);
    root_->SetSelection(1);
    RebuildTree();
    MarkActivity();
    UpdateTitle();
    tree_->SetFocus();
}

void MainFrame::NewVault() {
    wxString suggested = U(Config::instance().get_last_database_path());
    if (suggested.empty()) {
        wxFileName fn(wxGetHomeDir(), "vault.pif");
        suggested = fn.GetFullPath();
    } else {
        wxFileName fn(suggested);
        fn.SetFullName("vault.pif");
        if (fn.FileExists()) fn.SetFullName("new-vault.pif");
        suggested = fn.GetFullPath();
    }
    NewVaultDialog dlg(this, suggested);
    if (ShowModalCounted(dlg) != wxID_OK) {
        if (!db_) CallAfter([this] { PromptUnlock(); });
        return;
    }
    const std::string path = dlg.Path();
    const SecureString pw = dlg.Password();
    const Database::EncryptionLevel level = dlg.Level();
    // Only now close the current vault, so Cancel leaves it open.
    if (db_ && !Lock("Closed the previous vault.")) return;
    std::unique_ptr<Database> created;
    try {
        RunBusy("Creating Vault", "Tuning the key derivation to this computer...",
                [&] { created = Database::create(path, pw, level); });
    } catch (const std::exception& e) {
        ShowError(this, "Could not create the vault", e);
        CallAfter([this] { PromptUnlock(); });
        return;
    }
    db_ = std::move(created);
    db_->set_name(S(wxFileName(U(path)).GetName()));
    try {
        db_->save();
    } catch (const std::exception&) {
    }
    Config::instance().set_last_database_path(path);
    VaultOpened();
    SetStatus("Created " + U(path) + ".");
}

void MainFrame::OpenOther() {
    wxFileDialog dlg(this, "Open Vault", wxEmptyString, wxEmptyString,
                     "Pasgen vault (*.pif)|*.pif|All files|*", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (ShowModalCounted(dlg) != wxID_OK) return;
    const wxString path = dlg.GetPath();
    if (db_ && !Lock("Closed the previous vault.")) return;
    CallAfter([this, path] { PromptUnlock(path); });
}

bool MainFrame::Save() {
    if (!db_) return false;
    editor_->Commit();
    try {
        db_->save();
    } catch (const std::exception& e) {
        ShowError(this, "Could not save the vault", e);
        return false;
    }
    SetStatus("Saved.");
    UpdateTitle();
    return true;
}

void MainFrame::SaveAs() {
    if (!db_) return;
    editor_->Commit();
    wxFileName fn(U(db_->file_path()));
    wxFileDialog dlg(this, "Save Vault As", fn.GetPath(), fn.GetFullName(), "Pasgen vault (*.pif)|*.pif",
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (ShowModalCounted(dlg) != wxID_OK) return;
    wxString path = dlg.GetPath();
    if (!path.Lower().EndsWith(".pif")) path += ".pif";
    try {
        db_->save_as(S(path));
    } catch (const std::exception& e) {
        ShowError(this, "Could not save the vault", e);
        return;
    }
    Config::instance().set_last_database_path(S(path));
    SetStatus("Saved as " + path + ".");
    UpdateTitle();
}

bool MainFrame::Lock(const wxString& reason) {
    if (!db_) return true;
    editor_->Commit();
    // Pending edits are saved rather than dropped: losing work to an idle
    // timer would be worse than the extra write, and the file is encrypted.
    if (db_->is_dirty()) {
        try {
            db_->save();
        } catch (const std::exception& e) {
            ShowError(this, "Could not save before locking; the vault stays open", e);
            return false;
        }
    }
    if (!clip_text_.empty()) {
        ClearClipboardIfHolds(this, clip_text_);
        clip_text_.clear();
        clip_left_ = 0;
    }
    editor_->Unload();
    right_->SetSelection(0);
    tree_->DeleteAllItems();
    db_.reset();  // wipes the master password, the cached key and every account
    root_->SetSelection(0);
    SetStatus(reason);
    UpdateTitle();
    return true;
}

// ---------------------------------------------------------------------------
// Tree

void MainFrame::RequestRebuild() {
    if (rebuild_pending_) return;
    rebuild_pending_ = true;
    CallAfter([this] {
        rebuild_pending_ = false;
        RebuildTree();
    });
}

void MainFrame::RebuildTree() {
    if (!db_) return;
    const std::string keep = editor_->AccountId();
    const std::string query = lower(S(search_->GetValue().Trim().Trim(false)));
    const bool searching = !query.empty();
    const bool archived = show_archived_->GetValue();

    rebuilding_ = true;
    tree_->Freeze();
    tree_->DeleteAllItems();
    const wxTreeItemId root = tree_->AddRoot(wxEmptyString);
    wxTreeItemId select;

    auto add_category = [&](const std::string& cat_id, const wxString& name, int image) {
        std::vector<Account*> accts = db_->get_accounts_in_category(cat_id);
        std::vector<Account*> shown;
        for (Account* a : accts) {
            if (a->archived() && !archived) continue;
            if (searching && !a->matches_search(query)) continue;
            shown.push_back(a);
        }
        // Favourites first; otherwise the vault's own order.
        std::stable_sort(shown.begin(), shown.end(),
                         [](const Account* a, const Account* b) { return a->favorite() && !b->favorite(); });
        if (searching && shown.empty()) return;

        const wxTreeItemId c = tree_->AppendItem(root, wxString::Format("%s  (%zu)", name, shown.size()),
                                                 image, image, new Node(true, cat_id));
        tree_->SetItemBold(c);
        for (Account* a : shown) {
            wxString label = a->name().empty() ? wxString("(unnamed)") : U(a->name());
            if (a->favorite()) label = wxString::FromUTF8("\xE2\x98\x85 ") + label;  // star
            const wxTreeItemId it = tree_->AppendItem(c, label, -1, -1, new Node(false, a->id()));
            if (a->archived()) tree_->SetItemTextColour(it, wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
            if (a->id() == keep) select = it;
        }
        if (searching || !collapsed_.count(cat_id)) tree_->Expand(c);
    };

    const auto& cats = db_->categories();
    for (size_t i = 0; i < cats.size(); ++i) {
        const int colour = cats[i].color_index() >= 0 ? cats[i].color_index() : (int)i;
        add_category(cats[i].id(), U(cats[i].name()), colour % kCategoryColours);
    }
    add_category("", "Uncategorized", kUncategorizedImage);

    if (select.IsOk()) {
        tree_->SelectItem(select);
        tree_->EnsureVisible(select);
    }
    tree_->Thaw();
    rebuilding_ = false;

    // The account being edited is no longer listed (archived, or filtered
    // out): close the editor rather than edit something invisible.
    if (!keep.empty() && !select.IsOk()) {
        editor_->Commit();
        editor_->Unload();
        right_->SetSelection(0);
    }
    const size_t n = db_->account_count();
    SetStatusText(wxString::Format(n == 1 ? "%zu account" : "%zu accounts", n), FieldCount);
}

const MainFrame::Node* MainFrame::SelectedNode() const {
    const wxTreeItemId it = tree_->GetSelection();
    return it.IsOk() ? static_cast<const Node*>(tree_->GetItemData(it)) : nullptr;
}

std::string MainFrame::SelectedAccountId() const {
    const Node* n = SelectedNode();
    return n && !n->is_category ? n->id : std::string();
}

std::string MainFrame::CurrentCategoryId() const {
    const Node* n = SelectedNode();
    if (!n) return std::string();
    if (n->is_category) return n->id;
    const Account* a = db_ ? db_->get_account(n->id) : nullptr;
    return a ? a->category_id() : std::string();
}

void MainFrame::SelectAccount(const std::string& id) {
    std::function<wxTreeItemId(wxTreeItemId)> find = [&](wxTreeItemId parent) -> wxTreeItemId {
        wxTreeItemIdValue cookie;
        for (auto it = tree_->GetFirstChild(parent, cookie); it.IsOk(); it = tree_->GetNextChild(parent, cookie)) {
            auto* n = static_cast<Node*>(tree_->GetItemData(it));
            if (n && !n->is_category && n->id == id) return it;
            if (tree_->ItemHasChildren(it)) {
                wxTreeItemId found = find(it);
                if (found.IsOk()) return found;
            }
        }
        return wxTreeItemId();
    };
    const wxTreeItemId it = find(tree_->GetRootItem());
    if (it.IsOk()) {
        tree_->SelectItem(it);
        tree_->EnsureVisible(it);
    }
}

void MainFrame::OnSelectionChanged(wxTreeEvent& e) {
    if (rebuilding_ || !db_) return;
    const auto* n = static_cast<const Node*>(tree_->GetItemData(e.GetItem()));
    if (n && !n->is_category) {
        if (n->id != editor_->AccountId()) editor_->Load(db_.get(), n->id);
        right_->SetSelection(1);
    } else {
        editor_->Commit();
        editor_->Unload();
        right_->SetSelection(0);
    }
}

void MainFrame::OnTreeMenu(wxTreeEvent& e) {
    if (!db_) return;
    const auto* n = static_cast<const Node*>(tree_->GetItemData(e.GetItem()));
    if (!n) return;
    tree_->SelectItem(e.GetItem());
    const Node copy = *n;  // the menu action may rebuild the tree
    if (copy.is_category) CategoryMenu(copy.id);
    else AccountMenu(copy.id);
}

void MainFrame::CategoryMenu(const std::string& id) {
    wxMenu menu;
    menu.Append(ID_CTX_ADD_HERE, "Add Account Here");
    if (!id.empty()) {
        menu.AppendSeparator();
        menu.Append(ID_CTX_RENAME, "Rename...");
        auto* colours = new wxMenu;
        static const char* const names[kCategoryColours] = {"Orange", "Blue", "Green", "Purple",
                                                            "Yellow", "Red", "Teal", "Grey"};
        for (int i = 0; i < kCategoryColours; ++i) {
            auto* item = new wxMenuItem(colours, ID_CTX_COLOR_0 + i, names[i]);
            item->SetBitmap(ColourDot(CategoryColour(i)));
            colours->Append(item);
        }
        colours->AppendSeparator();
        colours->Append(ID_CTX_COLOR_AUTO, "Automatic");
        menu.AppendSubMenu(colours, "Colour");
        menu.Append(ID_CTX_UP, "Move Up");
        menu.Append(ID_CTX_DOWN, "Move Down");
        menu.AppendSeparator();
        menu.Append(ID_CTX_DELETE, "Delete Category...");
        const auto& cats = db_->categories();
        menu.Enable(ID_CTX_UP, !cats.empty() && cats.front().id() != id);
        menu.Enable(ID_CTX_DOWN, !cats.empty() && cats.back().id() != id);
    }

    const int choice = GetPopupMenuSelectionFromUser(menu);
    if (choice == wxID_NONE) return;
    int index = 0;
    const auto& cats = db_->categories();
    for (size_t i = 0; i < cats.size(); ++i)
        if (cats[i].id() == id) index = (int)i;

    if (choice == ID_CTX_ADD_HERE) {
        AddAccount();
    } else if (choice == ID_CTX_RENAME) {
        RenameCategory(id);
    } else if (choice == ID_CTX_DELETE) {
        DeleteCategory(id);
    } else if (choice == ID_CTX_UP || choice == ID_CTX_DOWN) {
        db_->reorder_category(id, index + (choice == ID_CTX_UP ? -1 : 1));
        AccountEdited(std::string(), true);
    } else if (choice == ID_CTX_COLOR_AUTO) {
        db_->set_category_color(id, -1);
        AccountEdited(std::string(), true);
    } else if (choice >= ID_CTX_COLOR_0 && choice < ID_CTX_COLOR_0 + kCategoryColours) {
        db_->set_category_color(id, choice - ID_CTX_COLOR_0);
        AccountEdited(std::string(), true);
    }
}

void MainFrame::AccountMenu(const std::string& id) {
    const Account* a = db_->get_account(id);
    if (!a) return;
    wxMenu menu;
    menu.Append(ID_COPY_USER, "Copy Username");
    menu.Append(ID_COPY_PASS, "Copy Password");
    menu.Append(ID_COPY_TOTP, "Copy Two-factor Code");
    menu.AppendSeparator();
    menu.AppendCheckItem(ID_CTX_FAVORITE, "Favorite")->Check(a->favorite());
    menu.AppendCheckItem(ID_CTX_ARCHIVED, "Archived")->Check(a->archived());

    auto* move = new wxMenu;
    std::vector<std::string> targets;
    for (const Category& c : db_->categories()) {
        targets.push_back(c.id());
        move->Append(ID_CTX_MOVE_0 + (int)targets.size() - 1, U(c.name()));
    }
    targets.push_back(std::string());
    move->Append(ID_CTX_MOVE_0 + (int)targets.size() - 1, "Uncategorized");
    for (size_t i = 0; i < targets.size(); ++i)
        if (targets[i] == a->category_id()) move->Enable(ID_CTX_MOVE_0 + (int)i, false);
    menu.AppendSubMenu(move, "Move To");
    menu.Append(ID_CTX_UP, "Move Up");
    menu.Append(ID_CTX_DOWN, "Move Down");
    menu.AppendSeparator();
    menu.Append(ID_CTX_EXPORT, "Export...");
    menu.Append(ID_CTX_DELETE, "Delete...");

    const int choice = GetPopupMenuSelectionFromUser(menu);
    if (choice == wxID_NONE || !(a = db_->get_account(id))) return;
    Account* acc = db_->get_account(id);
    if (choice == ID_COPY_USER) {
        editor_->CopyUsername();
    } else if (choice == ID_COPY_PASS) {
        editor_->CopyPassword();
    } else if (choice == ID_COPY_TOTP) {
        editor_->CopyTotp();
    } else if (choice == ID_CTX_FAVORITE || choice == ID_CTX_ARCHIVED) {
        editor_->Commit();
        if (choice == ID_CTX_FAVORITE) acc->set_favorite(!acc->favorite());
        else acc->set_archived(!acc->archived());
        editor_->Load(db_.get(), id);  // refresh its checkboxes
        AccountEdited(id, true);
    } else if (choice == ID_CTX_UP || choice == ID_CTX_DOWN) {
        MoveAccountBy(id, choice == ID_CTX_UP ? -1 : 1);
    } else if (choice == ID_CTX_EXPORT) {
        ExportAccounts(id);
    } else if (choice == ID_CTX_DELETE) {
        DeleteAccount(id);
    } else if (choice >= ID_CTX_MOVE_0 && choice < ID_CTX_MOVE_0 + (int)targets.size()) {
        db_->move_account(id, targets[(size_t)(choice - ID_CTX_MOVE_0)], INT_MAX);
        collapsed_.erase(targets[(size_t)(choice - ID_CTX_MOVE_0)]);
        AccountEdited(id, true);
    }
}

void MainFrame::MoveAccountBy(const std::string& id, int delta) {
    const Account* a = db_->get_account(id);
    if (!a) return;
    const std::string cat = a->category_id();
    std::vector<Account*> in = db_->get_accounts_in_category(cat);
    int index = 0;
    for (size_t i = 0; i < in.size(); ++i)
        if (in[i]->id() == id) index = (int)i;
    db_->move_account(id, cat, std::max(0, index + delta));
    AccountEdited(id, true);
}

// ---------------------------------------------------------------------------
// AccountHost

void MainFrame::AccountEdited(const std::string&, bool label_changed) {
    if (!db_) return;
    db_->mark_dirty();
    if (label_changed) RequestRebuild();
    UpdateTitle();
}

void MainFrame::CopyText(const wxString& text, bool secret, const wxString& what) {
    if (!CopyToClipboard(this, text, secret)) {
        SetStatus("Could not open the clipboard.");
        return;
    }
    if (clipboard_seconds_ > 0) {
        clip_text_ = text;
        clip_left_ = clipboard_seconds_;
        SetStatus(wxString::Format("%s copied. Clipboard clears in %d s.", what, clipboard_seconds_),
                  clipboard_seconds_);
    } else {
        SetStatus(what + " copied.");
    }
}

bool MainFrame::ChooseGeneratedPassword(wxString* out) {
    GeneratorDialog dlg(this, true, [this](const wxString& pw) { CopyText(pw, true, "Password"); });
    if (ShowModalCounted(dlg) != wxID_OK) return false;
    *out = dlg.Result();
    return true;
}

// ---------------------------------------------------------------------------
// Commands

void MainFrame::AddAccount() {
    if (!db_) return;
    editor_->Commit();
    const std::string cat = CurrentCategoryId();
    Account a;
    a.set_name("New account");
    const std::string id = a.id();
    db_->add_account(std::move(a));
    db_->move_account(id, cat, INT_MAX);
    collapsed_.erase(cat);
    search_->ChangeValue(wxString());
    editor_->Load(db_.get(), id);
    right_->SetSelection(1);
    RebuildTree();
    SelectAccount(id);
    editor_->FocusName();
    UpdateTitle();
}

void MainFrame::DeleteAccount(const std::string& id) {
    const Account* a = db_ ? db_->get_account(id) : nullptr;
    if (!a) return;
    const wxString name = a->name().empty() ? wxString("(unnamed)") : U(a->name());
    wxMessageDialog ask(this, "Delete \"" + name + "\"?\n\nIts password and history are removed from the vault "
                              "the next time it is saved.",
                        "Delete Account", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING);
    ask.SetYesNoLabels("Delete", "Keep");
    if (ShowModalCounted(ask) != wxID_YES || !db_) return;
    if (editor_->AccountId() == id) {
        editor_->Unload();
        right_->SetSelection(0);
    }
    db_->remove_account(id);
    RebuildTree();
    UpdateTitle();
    SetStatus("Deleted " + name + ".");
}

void MainFrame::AddCategory() {
    if (!db_) return;
    wxTextEntryDialog dlg(this, "Name:", "Add Category");
    if (ShowModalCounted(dlg) != wxID_OK || !db_) return;
    const wxString name = dlg.GetValue().Trim().Trim(false);
    if (name.empty()) return;
    db_->add_category(S(name));
    AccountEdited(std::string(), true);
}

void MainFrame::RenameCategory(const std::string& id) {
    for (const Category& c : db_->categories()) {
        if (c.id() != id) continue;
        wxTextEntryDialog dlg(this, "Name:", "Rename Category", U(c.name()));
        if (ShowModalCounted(dlg) != wxID_OK || !db_) return;
        const wxString name = dlg.GetValue().Trim().Trim(false);
        if (!name.empty()) {
            db_->rename_category(id, S(name));
            AccountEdited(std::string(), true);
        }
        return;
    }
}

void MainFrame::DeleteCategory(const std::string& id) {
    for (const Category& c : db_->categories()) {
        if (c.id() != id) continue;
        const wxString name = U(c.name());
        wxMessageDialog ask(this, "Delete the category \"" + name + "\"? Its accounts move to Uncategorized.",
                            "Delete Category", wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION);
        if (ShowModalCounted(ask) != wxID_YES || !db_) return;
        db_->remove_category(id);
        AccountEdited(std::string(), true);
        return;
    }
}

void MainFrame::ExportAccounts(const std::string& preselected_id) {
    if (!db_) return;
    editor_->Commit();
    ExportDialog dlg(this, *db_, preselected_id);
    if (ShowModalCounted(dlg) != wxID_OK || !db_) return;

    ExportFile::Payload payload;
    payload.source_vault_name = db_->metadata().name;
    payload.exported_at = std::chrono::system_clock::now();
    const bool whole = dlg.WholeVault();
    const std::set<std::string> ids = dlg.SelectedIds();
    std::vector<const Account*> chosen;
    for (const Account* a : std::as_const(*db_).get_all_accounts())
        if (whole || ids.count(a->id())) chosen.push_back(a);
    // Only the categories those accounts use travel with them.
    for (const Category& c : db_->categories())
        if (std::any_of(chosen.begin(), chosen.end(), [&](const Account* a) { return a->category_id() == c.id(); }))
            payload.categories.push_back(c);
    for (const Account* a : chosen) payload.accounts.push_back(*a);

    const std::string path = dlg.Path();
    const SecureString pw = dlg.Password();
    try {
        RunBusy("Exporting", "Encrypting the export...", [&] { ExportFile::write(path, pw, payload); });
    } catch (const std::exception& e) {
        ShowError(this, "Export failed", e);
        return;
    }
    SetStatus(wxString::Format("Exported %zu account(s) to %s.", payload.accounts.size(), U(path)));
}

void MainFrame::ImportAccounts() {
    if (!db_) return;
    editor_->Commit();
    ImportDialog dlg(this);
    if (ShowModalCounted(dlg) != wxID_OK || !db_) return;

    const std::string path = dlg.Path();
    const SecureString pw = dlg.Password();
    ExportFile::Payload payload;
    try {
        RunBusy("Importing", "Decrypting the export...", [&] { payload = ExportFile::read(path, pw); });
    } catch (const std::exception& e) {
        ShowError(this, "Import failed", e);
        return;
    }

    // Category ids in the file only mean something inside it: match by name.
    std::map<std::string, std::string> cat_map;
    for (const Category& c : payload.categories) {
        std::string target;
        for (const Category& existing : db_->categories())
            if (existing.name() == c.name()) target = existing.id();
        if (target.empty()) target = db_->add_category(c.name());
        cat_map[c.id()] = target;
    }
    int imported = 0;
    for (const Account& src : payload.accounts) {
        // A fresh Account (new id and timestamps), so a re-import never
        // collides with or overwrites what is already here.
        Account a;
        a.set_name(src.name());
        a.set_email(src.email());
        a.set_url(src.url());
        a.set_username(src.username());
        a.set_password(src.password());
        a.set_notes(src.notes());
        a.set_totp_secret(src.totp_secret());
        a.set_favorite(src.favorite());
        a.set_archived(src.archived());
        for (const auto& f : src.custom_fields()) a.add_custom_field(f.name, f.value, f.secret);
        auto it = cat_map.find(src.category_id());
        const std::string cat = it != cat_map.end() ? it->second : std::string();
        const std::string id = a.id();
        db_->add_account(std::move(a));
        db_->move_account(id, cat, INT_MAX);
        ++imported;
    }
    AccountEdited(std::string(), true);
    SetStatus(wxString::Format("Imported %d account(s) from %s.", imported, U(payload.source_vault_name)));
}

void MainFrame::ChangeMasterPassword() {
    if (!db_) return;
    editor_->Commit();
    ChangeMasterPasswordDialog dlg(this);
    if (ShowModalCounted(dlg) != wxID_OK || !db_) return;
    const SecureString pw = dlg.Password();
    try {
        RunBusy("Changing Password", "Re-encrypting the vault...", [&] {
            db_->change_master_password(pw);
            db_->save();
        });
    } catch (const std::exception& e) {
        ShowError(this, "Could not change the master password", e);
        return;
    }
    SetStatus("Master password changed and saved.");
    UpdateTitle();
}

void MainFrame::ChangeEncryption() {
    if (!db_) return;
    editor_->Commit();
    EncryptionDialog dlg(this, *db_);
    if (ShowModalCounted(dlg) != wxID_OK || !db_) return;
    const auto level = dlg.Level();
    try {
        RunBusy("Re-encrypting", "Applying the new encryption settings...", [&] {
            db_->change_encryption_level(level);
            db_->save();
        });
    } catch (const std::exception& e) {
        ShowError(this, "Could not change the encryption", e);
        return;
    }
    SetStatus("Encryption changed: " + U(db_->current_kdf_description()) + ".");
    UpdateTitle();
}

void MainFrame::RenameVault() {
    if (!db_) return;
    wxTextEntryDialog dlg(this, "Vault name:", "Rename Vault", U(db_->metadata().name));
    if (ShowModalCounted(dlg) != wxID_OK || !db_) return;
    db_->set_name(S(dlg.GetValue().Trim().Trim(false)));
    UpdateTitle();
}

void MainFrame::ShowPreferences() {
    PreferencesDialog dlg(this);
    if (ShowModalCounted(dlg) != wxID_OK) return;
    autolock_minutes_ = dlg.AutoLockMinutes();
    clipboard_seconds_ = dlg.ClipboardClearSeconds();
    Config::instance().set_autolock_minutes(autolock_minutes_);
    Config::instance().set_clipboard_clear_seconds(clipboard_seconds_);
}

void MainFrame::ShowGenerator() {
    if (db_ && editor_->HasAccount()) {
        editor_->GeneratePassword();
        return;
    }
    GeneratorDialog dlg(this, false, [this](const wxString& pw) { CopyText(pw, true, "Password"); });
    ShowModalCounted(dlg);
}

void MainFrame::ShowAbout() {
    wxAboutDialogInfo info;
    info.SetName("Pasgen");
    info.SetVersion("1.0.0");
    info.SetDescription("A local, offline password manager.\n\n"
                        "Vaults are encrypted with ChaCha20-Poly1305 inside AES-256-GCM, "
                        "under a key derived from your master password with Argon2id.");
    info.SetIcon(wxIcon(app_icons().GetIcon(FromDIP(64))));
    info.SetWebSite("https://github.com/L0C8/pasgen");
    info.AddDeveloper("L0C8");
    wxAboutBox(info, this);
}

// ---------------------------------------------------------------------------
// Housekeeping

int MainFrame::ShowModalCounted(wxDialog& dlg) {
    ++modal_depth_;
    const int r = dlg.ShowModal();
    --modal_depth_;
    MarkActivity();
    return r;
}

void MainFrame::RunBusy(const wxString& title, const wxString& message, const std::function<void()>& work) {
    ++modal_depth_;
    try {
        RunWithProgress(this, title, message, work);
    } catch (...) {
        --modal_depth_;
        MarkActivity();
        throw;
    }
    --modal_depth_;
    MarkActivity();
}

void MainFrame::OnTimer(wxTimerEvent&) {
    // Clipboard: wipe a copied secret when its time is up, unless the user
    // has copied something else since.
    if (clip_left_ > 0 && --clip_left_ == 0) {
        if (ClearClipboardIfHolds(this, clip_text_)) SetStatus("Clipboard cleared.");
        clip_text_.clear();
    } else if (clip_left_ > 0 && status_left_ > 0) {
        SetStatusText(wxString::Format("Copied. Clipboard clears in %d s.", clip_left_), FieldMessage);
    }

    if (status_left_ > 0 && --status_left_ == 0) SetStatusText(wxString(), FieldMessage);

    if (db_) {
        editor_->Tick();
        UpdateTitle();
        // Never yank the vault out from under an open dialog or a
        // background key derivation; lock once it closes instead.
        const wxLongLong idle = wxGetLocalTimeMillis() - g_last_activity;
        if (autolock_minutes_ > 0 && modal_depth_ == 0 && idle > wxLongLong(autolock_minutes_) * 60 * 1000) {
            if (Lock("Locked after inactivity.")) CallAfter([this] { PromptUnlock(); });
        }
    }
}

void MainFrame::OnClose(wxCloseEvent& e) {
    if (db_) {
        editor_->Commit();
        if (db_->is_dirty()) {
            wxMessageDialog ask(this, "Save changes to the vault before closing?", "Pasgen",
                                wxYES_NO | wxCANCEL | wxICON_QUESTION);
            ask.SetYesNoCancelLabels("Save", "Don't Save", "Cancel");
            const int r = ShowModalCounted(ask);
            if (r == wxID_CANCEL || (r == wxID_YES && !Save())) {
                if (e.CanVeto()) {
                    e.Veto();
                    return;
                }
            }
        }
    }
    if (!clip_text_.empty()) ClearClipboardIfHolds(this, clip_text_);
    editor_->Unload();
    db_.reset();
    timer_.Stop();
    Destroy();
}

void MainFrame::SetStatus(const wxString& text, int seconds) {
    SetStatusText(text, FieldMessage);
    status_left_ = seconds;
}

void MainFrame::UpdateTitle() {
    wxString title = "Pasgen";
    if (db_) {
        const wxString name = db_->metadata().name.empty() ? wxFileName(U(db_->file_path())).GetName()
                                                           : U(db_->metadata().name);
        title = (db_->is_dirty() ? "*" : "") + name + " - Pasgen";
    }
    if (GetTitle() != title) SetTitle(title);
    if (GetStatusBar()) {
        const wxString saved = !db_ ? wxString("Locked") : db_->is_dirty() ? wxString("Unsaved changes")
                                                                           : wxString("All changes saved");
        if (GetStatusBar()->GetStatusText(FieldSaved) != saved) SetStatusText(saved, FieldSaved);
        if (!db_ && !GetStatusBar()->GetStatusText(FieldCount).empty()) SetStatusText(wxString(), FieldCount);
    }
}

}  // namespace pasgen::wxui
