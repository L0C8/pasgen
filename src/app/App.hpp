#pragma once

#include "core/Database.hpp"
#include "core/Account.hpp"
#include "crypto/SecureMemory.hpp"
#include "util/Theme.hpp"
#include <memory>
#include <string>
#include <vector>

namespace pasgen {

class App {
public:
    App();
    ~App();

    void render(int display_w, int display_h);
    bool wants_quit()       const { return wants_quit_; }
    bool has_database()     const { return db_ != nullptr; }
    std::string db_name()   const { return db_ ? db_->metadata().name : ""; }
    bool is_dirty()         const { return db_ && db_->is_dirty(); }

    // Called instead of setting wants_quit_ directly; opens a confirmation
    // popup if there are unsaved changes.
    void request_quit();

    // Font selection (Settings > Preferences > Font). main.cpp polls
    // consume_font_dirty() each frame and, if true, rebuilds the ImGui font
    // atlas for font_id()/font_size() and re-uploads the render backend's
    // font texture.
    bool consume_font_dirty();
    std::string font_id()   const { return font_id_; }
    int         font_size() const { return font_size_; }

private:
    enum class Screen { LOGIN, CREATE_DB, MAIN };
    Screen screen_ = Screen::LOGIN;
    bool wants_quit_ = false;

    // --- Login ---
    char lp_[512]  = {};  // path
    char lpw_[256] = {};  // password
    bool lshow_    = false;
    std::string lerr_;

    // --- Create DB ---
    char cp_[512]   = {};
    char cpw_[256]  = {};
    char ccon_[256] = {};
    bool cshow_     = false;
    std::string cerr_;

    // --- Main ---
    std::unique_ptr<Database> db_;
    char search_[256]   = {};
    std::string sel_id_;  // selected account id
    std::string status_;
    bool status_err_ = false;
    float status_t_  = 0.0f;
    bool focus_name_ = false;  // set to focus name field on next frame

    // Edit form (mirrors selected account, updated in real-time)
    char ef_name_[256]  = {};
    char ef_email_[256] = {};
    char ef_user_[256]  = {};
    char ef_url_[512]   = {};
    char ef_pass_[256]  = {};
    bool ef_showp_      = false;
    char ef_totp_[256]  = {};
    char ef_notes_[4096] = {};

    // Password generator popup (settings come from Preferences; this popup
    // just previews/regenerates and applies to the selected account)
    bool gen_open_    = false;
    bool gen_regen_   = false;
    int  gen_len_     = 16;
    bool gen_lc_      = true;
    bool gen_uc_      = true;
    bool gen_dig_     = true;
    bool gen_sym_     = true;
    bool gen_phrase_  = false;
    int  gen_words_   = 4;
    char gen_sep_[16] = "-";
    char gen_prev_[256] = {};

    // Change master password popup
    bool   chg_open_  = false;
    char   chg_pw_[256]  = {};
    char   chg_con_[256] = {};
    bool   chg_show_     = false;
    std::string chg_err_;

    // Preferences popup (Settings menu)
    bool  prefs_open_ = false;
    Theme prefs_theme_ = Theme::Original;
    std::string font_id_ = "default";
    int         font_size_ = 15;
    bool        font_dirty_ = false;

    // Delete-account confirmation popup
    bool del_confirm_open_ = false;

    // Quit confirmation popup (shown when there are unsaved changes)
    bool quit_confirm_open_ = false;

    // Rendering
    void render_login(int w, int h);
    void render_create_db(int w, int h);
    void render_main(int w, int h);
    void render_passwords_panel(int w, int h);
    void render_account_list(float width);
    void render_account_detail(float width);
    void render_gen_popup();
    void render_chgpw_popup();
    void render_prefs_popup();
    void render_delete_confirm_popup();
    void render_quit_confirm_popup();
    void render_status_bar();
    void load_generator_defaults();

    // Themed widgets: render as bevelled retro or glossy XP buttons under the
    // Retro / Windows XP look and feel, or plain ImGui buttons otherwise.
    bool button(const char* label, ImVec2 size = ImVec2(0, 0));
    bool small_button(const char* label);

    // Actions
    void do_login();
    void do_create_db();
    void do_save();
    void do_add_account();
    void do_delete_selected();
    void do_select_account(const std::string& id);
    void sync_editor_to_db();
    void load_account_to_editor(const Account& acc);
    void clear_editor();
    void do_browse_open(char* buf, size_t n);
    void do_browse_save(char* buf, size_t n);
    void clipboard(const char* text);
    void set_status(const std::string& msg, bool err = false);
    void regenerate_password();

    // Helpers
    std::vector<const Account*> filtered_accounts() const;
    int  pw_strength(const char* pw) const;
    const char* strength_label(int s) const;
    float strength_frac(int s)  const { return (s + 1) / 5.0f; }
};

} // namespace pasgen
