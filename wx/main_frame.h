#pragma once

#include <memory>
#include <set>
#include <string>

#include <wx/dialog.h>
#include <wx/frame.h>
#include <wx/timer.h>
#include <wx/treectrl.h>

#include "account_panel.h"
#include "core/Database.hpp"

class wxCheckBox;
class wxSearchCtrl;
class wxSimplebook;
class wxSplitterWindow;
class wxStaticText;

namespace pasgen::wxui {

// Called from the app's event filter on any key or mouse input; the
// auto-lock timer measures idleness from the last call.
void MarkActivity();

class MainFrame : public wxFrame, public AccountHost {
public:
    explicit MainFrame(const wxString& vault_path);
    ~MainFrame() override;

    // Shows the unlock prompt for `path` (or the last vault used).
    void PromptUnlock(const wxString& path = wxString());

    // AccountHost
    void AccountEdited(const std::string& id, bool label_changed) override;
    void CopyText(const wxString& text, bool secret, const wxString& what) override;
    bool ChooseGeneratedPassword(wxString* out) override;

private:
    struct Node : wxTreeItemData {
        Node(bool category, std::string id) : is_category(category), id(std::move(id)) {}
        bool is_category;
        std::string id;  // category id ("" = Uncategorized) or account id
    };

    void BuildMenu();
    void BuildLayout();

    // Vault lifecycle
    wxString TryOpen(const std::string& path, const SecureString& pw);
    void NewVault();
    void OpenOther();
    bool Save();
    void SaveAs();
    bool Lock(const wxString& reason);  // false if a failed save kept it open
    void VaultOpened();

    // Tree
    void RebuildTree();
    void RequestRebuild();
    void SelectAccount(const std::string& id);
    const Node* SelectedNode() const;
    std::string SelectedAccountId() const;
    std::string CurrentCategoryId() const;  // of the selected item
    void OnSelectionChanged(wxTreeEvent& e);
    void OnTreeMenu(wxTreeEvent& e);
    void CategoryMenu(const std::string& category_id);
    void AccountMenu(const std::string& account_id);
    void MoveAccountBy(const std::string& account_id, int delta);

    // Commands
    void AddAccount();
    void DeleteAccount(const std::string& id);
    void AddCategory();
    void RenameCategory(const std::string& id);
    void DeleteCategory(const std::string& id);
    void ExportAccounts(const std::string& preselected_id);
    void ImportAccounts();
    void ChangeMasterPassword();
    void ChangeEncryption();
    void RenameVault();
    void ShowPreferences();
    void ShowGenerator();
    void ShowAbout();

    void OnTimer(wxTimerEvent&);
    void OnClose(wxCloseEvent& e);
    void SetStatus(const wxString& text, int seconds = 5);
    void UpdateTitle();
    int ShowModalCounted(wxDialog& dlg);
    void RunBusy(const wxString& title, const wxString& message, const std::function<void()>& work);

    std::unique_ptr<Database> db_;

    wxSimplebook* root_ = nullptr;   // 0: locked, 1: vault
    wxStaticText* locked_text_ = nullptr;
    wxSplitterWindow* split_ = nullptr;
    wxSearchCtrl* search_ = nullptr;
    wxCheckBox* show_archived_ = nullptr;
    wxTreeCtrl* tree_ = nullptr;
    wxSimplebook* right_ = nullptr;  // 0: placeholder, 1: account editor
    AccountPanel* editor_ = nullptr;

    std::set<std::string> collapsed_;
    bool rebuilding_ = false;
    bool rebuild_pending_ = false;
    int modal_depth_ = 0;

    wxTimer timer_;
    int autolock_minutes_ = 5;
    int clipboard_seconds_ = 30;
    wxString clip_text_;
    int clip_left_ = 0;
    int status_left_ = 0;
};

}  // namespace pasgen::wxui
