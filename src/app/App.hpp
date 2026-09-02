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

    // Window background, so the GL clear color matches the active theme
    // instead of flashing a hardcoded color on resize.
    ImVec4 background_color() const;

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
    Theme prefs_theme_ = Theme::Dark;
    std::string font_id_ = "roboto";
    int         font_size_ = 16;
    bool        font_dirty_ = false;

    // Delete-account confirmation popup
    bool del_confirm_open_ = false;

    // Quit confirmation popup (shown when there are unsaved changes)
    bool quit_confirm_open_ = false;

    // About popup (Help menu)
    bool about_open_ = false;

    // Category add/rename/delete popups (left panel)
    bool cat_add_open_ = false;
    char cat_add_name_[128] = {};

    bool cat_rename_open_ = false;
    std::string cat_rename_id_;
    char cat_rename_name_[128] = {};

    bool cat_delete_confirm_open_ = false;
    std::string cat_delete_id_;

    // Rendering
    void render_login(int w, int h);
    void render_create_db(int w, int h);
    void render_main(int w, int h);
    void render_menu_bar();
    void render_toolbar();
    void render_sidebar(float width, float height);
    void render_account_detail(float width);
    void render_detail_placeholder(float width);
    void render_gen_popup();
    void render_chgpw_popup();
    void render_prefs_popup();
    void render_delete_confirm_popup();
    void render_quit_confirm_popup();
    void render_about_popup();
    void render_category_add_popup();
    void render_category_rename_popup();
    void render_category_delete_confirm_popup();
    void render_category_section(const std::string& category_id, const std::string& label, bool deletable,
                                  int color_index, bool searching, const std::string& query);
    void render_status_bar();
    void load_generator_defaults();

    // Measured height of the login / create cards. The content is laid out
    // once at a provisional height, then the card is sized to exactly fit it
    // on subsequent frames, so adding an error banner grows the card instead
    // of clipping the last row.
    float login_card_h_  = 0.0f;
    float create_card_h_ = 0.0f;

    // Shared chrome for the two full-screen entry screens (login / create).
    void begin_centered_screen(int w, int h, const char* id, float card_w, float card_h);
    void end_centered_screen();
    // Content height used this frame, measured from inside the open card.
    float measure_card_content() const;
    // Label-above-input row; returns true when edited.
    bool field(const char* label, const char* id, char* buf, size_t n,
                float width, ImGuiInputTextFlags flags = 0, const char* hint = nullptr);

    // Actions
    void do_login();
    void do_create_db();
    void do_save();
    void do_save_as();
    void do_add_account();
    void do_delete_selected();
    void do_select_account(const std::string& id);
    void load_account_to_editor(const Account& acc);
    void clear_editor();
    void do_browse_open(char* buf, size_t n);
    void do_browse_save(char* buf, size_t n);
    void clipboard(const char* text);
    void set_status(const std::string& msg, bool err = false);
    void regenerate_password();

    // Helpers
    int  pw_strength(const char* pw) const;
    const char* strength_label(int s) const;
    ImVec4 strength_color(int s) const;
    float strength_frac(int s)  const { return (s + 1) / 5.0f; }
};

} // namespace pasgen
