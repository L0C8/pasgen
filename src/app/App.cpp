#include "app/App.hpp"
#include "util/Config.hpp"
#include "util/PasswordGenerator.hpp"
#include "util/TOTPGenerator.hpp"
#include "util/Theme.hpp"
#include "util/Font.hpp"
#include "imgui.h"
#include "portable-file-dialogs.h"
#include <SDL.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace pasgen {

// ── helpers ──────────────────────────────────────────────────────────────────

static void safe_copy(char* dst, size_t n, const std::string& src) {
    std::strncpy(dst, src.c_str(), n - 1);
    dst[n - 1] = '\0';
}

static std::string fmt_time(std::chrono::system_clock::time_point tp) {
    auto t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = *std::localtime(&t);
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d");
    return ss.str();
}

// ── App ──────────────────────────────────────────────────────────────────────

App::App() {
    std::string last = Config::instance().get_last_database_path();
    if (!last.empty()) safe_copy(lp_, sizeof(lp_), last);
    prefs_theme_ = theme_from_string(Config::instance().get_theme());
    font_id_     = Config::instance().get_font_id();
    font_size_   = Config::instance().get_font_size();
    load_generator_defaults();
}

bool App::consume_font_dirty() {
    bool d = font_dirty_;
    font_dirty_ = false;
    return d;
}

void App::load_generator_defaults() {
    PasswordGenDefaults d = Config::instance().get_password_gen_defaults();
    gen_len_    = d.length;
    gen_lc_     = d.lowercase;
    gen_uc_     = d.uppercase;
    gen_dig_    = d.digits;
    gen_sym_    = d.symbols;
    gen_phrase_ = d.passphrase;
    gen_words_  = d.words;
    safe_copy(gen_sep_, sizeof(gen_sep_), d.separator);
}

App::~App() {
    SecureMemory::secure_zero(lpw_,     sizeof(lpw_));
    SecureMemory::secure_zero(cpw_,     sizeof(cpw_));
    SecureMemory::secure_zero(ccon_,    sizeof(ccon_));
    SecureMemory::secure_zero(ef_pass_, sizeof(ef_pass_));
    SecureMemory::secure_zero(chg_pw_,  sizeof(chg_pw_));
    SecureMemory::secure_zero(chg_con_, sizeof(chg_con_));
}

bool App::button(const char* label, ImVec2 size) {
    switch (look_and_feel_of(prefs_theme_)) {
    case LookAndFeel::Retro:     return RetroButton(label, size);
    case LookAndFeel::WindowsXP: return XPButton(label, size);
    default:                     return ImGui::Button(label, size);
    }
}

bool App::small_button(const char* label) {
    switch (look_and_feel_of(prefs_theme_)) {
    case LookAndFeel::Retro:     return RetroSmallButton(label);
    case LookAndFeel::WindowsXP: return XPSmallButton(label);
    default:                     return ImGui::SmallButton(label);
    }
}

void App::request_quit() {
    if (screen_ == Screen::MAIN && db_ && db_->is_dirty()) {
        quit_confirm_open_ = true;
        return;
    }
    wants_quit_ = true;
}

void App::render(int w, int h) {
    // Tick status message timer
    status_t_ -= ImGui::GetIO().DeltaTime;

    // Handle Ctrl+S globally
    if (screen_ == Screen::MAIN && db_) {
        auto& io = ImGui::GetIO();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) do_save();
    }

    switch (screen_) {
    case Screen::LOGIN:    render_login(w, h);     break;
    case Screen::CREATE_DB: render_create_db(w, h); break;
    case Screen::MAIN:    render_main(w, h);      break;
    }
}

// ── LOGIN ─────────────────────────────────────────────────────────────────────

void App::render_login(int w, int h) {
    ImGuiWindowFlags bg_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;

    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({(float)w, (float)h});
    ImGui::Begin("##bg", nullptr, bg_flags);

    float pw = 480, ph = 310;
    ImGui::SetCursorPos({(w - pw) * 0.5f, (h - ph) * 0.5f});
    ImGui::BeginChild("##login_box", {pw, ph}, true);

    // Title
    ImGui::SetWindowFontScale(1.5f);
    ImGui::Text("Pasgen");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::SameLine(0, 8);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
    ImGui::TextDisabled("Secure Password Manager");
    ImGui::Separator();
    ImGui::Spacing();

    float lw = 120.0f;
    float fw = pw - lw - 60;

    // Path row
    ImGui::Text("Database");
    ImGui::SameLine(lw);
    ImGui::SetNextItemWidth(fw - 60);
    ImGui::InputText("##lpath", lp_, sizeof(lp_));
    ImGui::SameLine(0, 4);
    if (button("...##lo")) do_browse_open(lp_, sizeof(lp_));

    // Password row
    ImGui::Text("Password");
    ImGui::SameLine(lw);
    ImGuiInputTextFlags pf = ImGuiInputTextFlags_EnterReturnsTrue;
    if (!lshow_) pf |= ImGuiInputTextFlags_Password;
    ImGui::SetNextItemWidth(fw - 60);
    bool enter = ImGui::InputText("##lpw", lpw_, sizeof(lpw_), pf);
    ImGui::SameLine(0, 4);
    if (button(lshow_ ? "Hide" : "Show")) lshow_ = !lshow_;
    if (enter) do_login();

    ImGui::Spacing();
    ImGui::Spacing();

    // Error
    if (!lerr_.empty())
        ImGui::TextColored({1.0f, 0.35f, 0.35f, 1.0f}, "%s", lerr_.c_str());

    ImGui::Spacing();

    // Buttons
    float bw = (pw - 40) * 0.5f;
    if (button("Open Database", {bw, 36})) do_login();
    ImGui::SameLine(0, 8);
    if (button("Create New...", {bw, 36})) {
        screen_ = Screen::CREATE_DB;
        lerr_.clear();
        SecureMemory::secure_zero(cpw_,  sizeof(cpw_));
        SecureMemory::secure_zero(ccon_, sizeof(ccon_));
        cshow_ = false; cerr_.clear();
    }

    ImGui::EndChild();
    ImGui::End();
}

void App::do_login() {
    if (lp_[0] == '\0') { lerr_ = "Please enter a database file path."; return; }
    if (lpw_[0] == '\0') { lerr_ = "Please enter the master password."; return; }
    try {
        SecureString pw(lpw_);
        db_ = Database::open(lp_, pw);
        Config::instance().set_last_database_path(lp_);
        SecureMemory::secure_zero(lpw_, sizeof(lpw_));
        lerr_.clear();
        screen_ = Screen::MAIN;
        // Select first account
        auto all = db_->get_all_accounts();
        if (!all.empty()) do_select_account(all[0]->id());
    } catch (const std::exception& e) {
        lerr_ = e.what();
    }
}

// ── CREATE DB ─────────────────────────────────────────────────────────────────

void App::render_create_db(int w, int h) {
    ImGuiWindowFlags bg_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;

    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({(float)w, (float)h});
    ImGui::Begin("##bg2", nullptr, bg_flags);

    float pw = 480, ph = 370;
    ImGui::SetCursorPos({(w - pw) * 0.5f, (h - ph) * 0.5f});
    ImGui::BeginChild("##create_box", {pw, ph}, true);

    ImGui::SetWindowFontScale(1.3f);
    ImGui::Text("Create New Database");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Separator();
    ImGui::Spacing();

    float lw = 120.0f;
    float fw = pw - lw - 60;

    ImGui::Text("Save to");
    ImGui::SameLine(lw);
    ImGui::SetNextItemWidth(fw - 60);
    ImGui::InputText("##cpath", cp_, sizeof(cp_));
    ImGui::SameLine(0, 4);
    if (button("...##cs")) do_browse_save(cp_, sizeof(cp_));

    ImGui::Spacing();
    ImGui::Text("Password");
    ImGui::SameLine(lw);
    ImGuiInputTextFlags pf = 0;
    if (!cshow_) pf |= ImGuiInputTextFlags_Password;
    ImGui::SetNextItemWidth(fw - 60);
    ImGui::InputText("##cpw", cpw_, sizeof(cpw_), pf);
    ImGui::SameLine(0, 4);
    if (button(cshow_ ? "Hide" : "Show")) cshow_ = !cshow_;

    ImGui::Text("Confirm");
    ImGui::SameLine(lw);
    ImGui::SetNextItemWidth(fw - 60);
    ImGui::InputText("##ccon", ccon_, sizeof(ccon_), cshow_ ? 0 : ImGuiInputTextFlags_Password);

    // Strength bar
    ImGui::Spacing();
    int str = pw_strength(cpw_);
    ImGui::Text("Strength");
    ImGui::SameLine(lw);
    float bw2 = fw * 0.55f;
    ImVec4 col = (str <= 1) ? ImVec4{0.9f,0.2f,0.2f,1} :
                 (str == 2) ? ImVec4{0.9f,0.6f,0.1f,1} :
                 (str == 3) ? ImVec4{0.7f,0.9f,0.1f,1} :
                              ImVec4{0.2f,0.9f,0.2f,1};
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, col);
    ImGui::ProgressBar(strength_frac(str), {bw2, 14}, "");
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 6);
    ImGui::TextColored(col, "%s", strength_label(str));

    // Match indicator
    bool match = std::strcmp(cpw_, ccon_) == 0;
    if (cpw_[0] && ccon_[0]) {
        ImGui::Text(" ");
        ImGui::SameLine(lw);
        if (match)  ImGui::TextColored({0.2f,0.9f,0.2f,1}, "Passwords match");
        else        ImGui::TextColored({1.0f,0.35f,0.35f,1}, "Passwords do not match");
    }

    ImGui::Spacing();
    if (!cerr_.empty())
        ImGui::TextColored({1.0f,0.35f,0.35f,1.0f}, "%s", cerr_.c_str());

    ImGui::Spacing();
    float bb = (pw - 40) * 0.5f;
    if (button("Cancel", {bb, 36})) {
        screen_ = Screen::LOGIN;
        SecureMemory::secure_zero(cpw_,  sizeof(cpw_));
        SecureMemory::secure_zero(ccon_, sizeof(ccon_));
    }
    ImGui::SameLine(0, 8);
    bool can_create = cp_[0] && cpw_[0] && match;
    if (!can_create) ImGui::BeginDisabled();
    if (button("Create Database", {bb, 36})) do_create_db();
    if (!can_create) ImGui::EndDisabled();

    ImGui::EndChild();
    ImGui::End();
}

void App::do_create_db() {
    if (cp_[0] == '\0')  { cerr_ = "Please enter a file path."; return; }
    if (cpw_[0] == '\0') { cerr_ = "Please enter a master password."; return; }
    if (std::strcmp(cpw_, ccon_) != 0) { cerr_ = "Passwords do not match."; return; }
    try {
        std::string path = cp_;
        // Append .pif if no extension
        if (path.size() < 4 || path.substr(path.size()-4) != ".pif")
            path += ".pif";

        SecureString pw(cpw_);
        db_ = Database::create(path, pw);
        safe_copy(lp_, sizeof(lp_), path);
        Config::instance().set_last_database_path(path);
        SecureMemory::secure_zero(cpw_,  sizeof(cpw_));
        SecureMemory::secure_zero(ccon_, sizeof(ccon_));
        cerr_.clear();
        screen_ = Screen::MAIN;
        sel_id_.clear();
        clear_editor();
    } catch (const std::exception& e) {
        cerr_ = e.what();
    }
}

// ── MAIN ──────────────────────────────────────────────────────────────────────

void App::render_main(int w, int h) {
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({(float)w, (float)h});
    ImGui::Begin("##main", nullptr, flags);
    ImGui::PopStyleVar();

    // ── Menu bar ──
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Database", "")) {
                screen_ = Screen::CREATE_DB;
                SecureMemory::secure_zero(cpw_,  sizeof(cpw_));
                SecureMemory::secure_zero(ccon_, sizeof(ccon_));
                cp_[0] = '\0'; cshow_ = false; cerr_.clear();
            }
            if (ImGui::MenuItem("Open Database", "")) {
                char path[512] = {};
                do_browse_open(path, sizeof(path));
                if (path[0]) {
                    safe_copy(lp_, sizeof(lp_), std::string(path));
                    screen_ = Screen::LOGIN;
                    lerr_.clear();
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save", "Ctrl+S")) do_save();
            if (ImGui::MenuItem("Change Master Password"))  { chg_open_ = true; }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) request_quit();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Settings")) {
            if (ImGui::MenuItem("Preferences...")) { prefs_open_ = true; }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About Pasgen")) {}
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    render_passwords_panel(w, h);

    render_gen_popup();
    render_chgpw_popup();
    render_prefs_popup();
    render_delete_confirm_popup();
    render_quit_confirm_popup();

    ImGui::End();
}

void App::render_passwords_panel(int w, int h) {
    (void)w; (void)h;

    // ── Toolbar ──
    if (button("+ Add"))    do_add_account();
    ImGui::SameLine(0, 6);
    if (sel_id_.empty()) ImGui::BeginDisabled();
    if (button("Delete"))   del_confirm_open_ = true;
    if (sel_id_.empty()) ImGui::EndDisabled();
    ImGui::SameLine(0, 6);
    if (!db_->is_dirty()) ImGui::BeginDisabled();
    if (button("Save"))     do_save();
    if (!db_->is_dirty()) ImGui::EndDisabled();
    ImGui::SameLine(0, 16);
    ImGui::SetNextItemWidth(220);
    ImGui::InputTextWithHint("##search", "Search...", search_, sizeof(search_));

    ImGui::Separator();

    // ── Two-panel layout ──
    // Reserve a fixed-height footer for status messages (below) so the rest
    // of the layout never shifts when a message appears, changes, or clears.
    float footer_h = ImGui::GetFrameHeightWithSpacing();
    float avail_h  = ImGui::GetContentRegionAvail().y - footer_h;
    float list_w   = 260.0f;
    float detail_w = ImGui::GetContentRegionAvail().x - list_w - ImGui::GetStyle().ItemSpacing.x;

    ImGui::BeginChild("##list_panel", {list_w, avail_h}, true);
    render_account_list(list_w);
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##detail_panel", {detail_w, avail_h}, false);
    render_account_detail(detail_w);
    ImGui::EndChild();

    ImGui::Separator();
    render_status_bar();
}

void App::render_status_bar() {
    // Always renders exactly one line, whatever the state, so this footer's
    // height never changes and nothing above it jumps around.
    if (status_t_ > 0) {
        if (status_err_) ImGui::TextColored({1.0f,0.4f,0.4f,1}, "%s", status_.c_str());
        else             ImGui::TextColored({0.4f,1.0f,0.4f,1}, "%s", status_.c_str());
    } else if (db_ && db_->is_dirty()) {
        ImGui::TextColored({0.9f,0.7f,0.2f,1}, "Unsaved changes");
    } else {
        ImGui::TextDisabled("Ready");
    }
}

// ── ACCOUNT LIST ──────────────────────────────────────────────────────────────

void App::render_account_list(float width) {
    (void)width;
    auto accounts = filtered_accounts();
    float line_h  = ImGui::GetTextLineHeightWithSpacing();

    for (const Account* acc : accounts) {
        bool selected = (acc->id() == sel_id_);
        ImGui::PushID(acc->id().c_str());

        float item_h = line_h * 2.0f + 2;
        ImVec2 item_min = ImGui::GetCursorScreenPos();

        if (ImGui::Selectable("##sel", selected, 0, {0, item_h})) {
            do_select_account(acc->id());
        }

        // Draw name + email manually inside the selectable area
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const char* name  = acc->name().empty() ? "(unnamed)" : acc->name().c_str();
        const char* email = acc->email().c_str();
        dl->AddText({item_min.x + 6, item_min.y + 2},
                    IM_COL32(230, 230, 230, 255), name);
        if (email[0])
            dl->AddText({item_min.x + 6, item_min.y + line_h + 2},
                        IM_COL32(160, 160, 160, 255), email);

        ImGui::PopID();
    }

    if (accounts.empty()) {
        ImGui::TextDisabled("No accounts");
    }
}

// ── ACCOUNT DETAIL ────────────────────────────────────────────────────────────

void App::render_account_detail(float width) {
    if (sel_id_.empty()) {
        ImGui::SetCursorPos({width * 0.5f - 100, ImGui::GetContentRegionAvail().y * 0.5f - 10});
        ImGui::TextDisabled("Select an account or click + Add");
        return;
    }

    Account* acc = db_->get_account(sel_id_);
    if (!acc) { sel_id_.clear(); clear_editor(); return; }

    float lw = 85.0f;
    float fw = width - lw - 32;

    // Focus name field if requested
    if (focus_name_) { ImGui::SetKeyboardFocusHere(0); focus_name_ = false; }

    // ── Name ──
    ImGui::Text("Name");
    ImGui::SameLine(lw);
    ImGui::SetNextItemWidth(fw);
    if (ImGui::InputText("##name", ef_name_, sizeof(ef_name_))) {
        acc->set_name(ef_name_); db_->mark_dirty();
    }

    // ── Email ──
    ImGui::Text("Email");
    ImGui::SameLine(lw);
    ImGui::SetNextItemWidth(fw);
    if (ImGui::InputText("##email", ef_email_, sizeof(ef_email_))) {
        acc->set_email(ef_email_); db_->mark_dirty();
    }

    // ── Username ──
    ImGui::Text("Username");
    ImGui::SameLine(lw);
    float fw2 = fw - 60;
    ImGui::SetNextItemWidth(fw2);
    if (ImGui::InputText("##user", ef_user_, sizeof(ef_user_))) {
        acc->set_username(ef_user_); db_->mark_dirty();
    }
    ImGui::SameLine(0, 4);
    if (small_button("Copy##cu")) clipboard(ef_user_);

    // ── URL ──
    ImGui::Text("URL");
    ImGui::SameLine(lw);
    ImGui::SetNextItemWidth(fw);
    if (ImGui::InputText("##url", ef_url_, sizeof(ef_url_))) {
        acc->set_url(ef_url_); db_->mark_dirty();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Password ──
    ImGui::Text("Password");
    ImGui::SameLine(lw);
    float pw_w = fw - 120;
    ImGuiInputTextFlags pf = ef_showp_ ? 0 : ImGuiInputTextFlags_Password;
    ImGui::SetNextItemWidth(pw_w);
    if (ImGui::InputText("##pass", ef_pass_, sizeof(ef_pass_), pf)) {
        acc->set_password(SecureString(ef_pass_)); db_->mark_dirty();
    }
    ImGui::SameLine(0, 4);
    if (small_button(ef_showp_ ? "Hide" : "Show")) ef_showp_ = !ef_showp_;
    ImGui::SameLine(0, 4);
    if (small_button("Copy##cp")) clipboard(ef_pass_);
    ImGui::SameLine(0, 4);
    if (small_button("Gen")) {
        gen_open_ = true;
        gen_regen_ = true;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── TOTP ──
    ImGui::Text("TOTP");
    ImGui::SameLine(lw);
    ImGui::SetNextItemWidth(fw);
    if (ImGui::InputText("##totp", ef_totp_, sizeof(ef_totp_))) {
        acc->set_totp_secret(ef_totp_); db_->mark_dirty();
    }

    if (ef_totp_[0]) {
        ImGui::Text(" ");
        ImGui::SameLine(lw);
        try {
            std::string code = TOTPGenerator::generate(ef_totp_);
            uint32_t rem     = TOTPGenerator::seconds_remaining();
            float frac       = (float)rem / 30.0f;

            ImVec4 tc = rem > 10 ? ImVec4{0.3f,0.95f,0.3f,1} : ImVec4{0.95f,0.3f,0.3f,1};
            ImGui::TextColored({0.4f, 0.9f, 1.0f, 1.0f}, "%s", code.c_str());
            ImGui::SameLine(0, 6);
            ImGui::TextDisabled("(%ds)", rem);
            ImGui::SameLine(0, 8);
            if (small_button("Copy##ct")) clipboard(code.c_str());

            // Thin progress bar (time countdown)
            ImGui::SetCursorPosX(lw);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, tc);
            ImGui::ProgressBar(frac, {fw * 0.6f, 6}, "");
            ImGui::PopStyleColor();
        } catch (...) {
            ImGui::TextColored({1,0.4f,0.4f,1}, "Invalid secret");
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Notes ──
    ImGui::Text("Notes");
    ImGui::SameLine(lw);
    ImGui::SetNextItemWidth(fw);
    float notes_h = std::max(60.0f, ImGui::GetContentRegionAvail().y * 0.35f);
    if (ImGui::InputTextMultiline("##notes", ef_notes_, sizeof(ef_notes_), {fw, notes_h})) {
        acc->set_notes(ef_notes_); db_->mark_dirty();
    }

    // ── Password History ──
    const auto& hist = acc->password_history();
    if (!hist.empty()) {
        ImGui::Spacing();
        if (ImGui::TreeNode("Password History")) {
            for (const auto& e : hist) {
                ImGui::Bullet();
                // Show partial password (masked)
                std::string masked(e.password.size(), '*');
                if (masked.size() > 3) masked.replace(0, 1, 1, e.password.c_str()[0]);
                ImGui::Text("%-20s  %s", masked.c_str(), fmt_time(e.changed_at).c_str());
            }
            ImGui::TreePop();
        }
    }
}

// ── PASSWORD GENERATOR ────────────────────────────────────────────────────────

void App::render_gen_popup() {
    if (gen_open_) {
        ImGui::OpenPopup("##gen");
        gen_open_ = false;
    }
    if (gen_regen_) { regenerate_password(); gen_regen_ = false; }

    ImGui::SetNextWindowSize({420, 170}, ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
    if (ImGui::BeginPopupModal("##gen", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
        ImGui::SetWindowFontScale(1.2f);
        ImGui::Text("Generate Password");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::TextDisabled("Using defaults from Settings > Preferences");
        ImGui::Separator();

        ImGui::TextDisabled("Preview:");
        ImGui::SameLine();
        ImGui::TextWrapped("%s", gen_prev_);
        ImGui::Spacing();

        if (button("Regenerate", {120, 0})) regenerate_password();
        ImGui::SameLine(0, 8);
        if (button("Use Password", {120, 0})) {
            if (gen_prev_[0] && !sel_id_.empty()) {
                safe_copy(ef_pass_, sizeof(ef_pass_), std::string(gen_prev_));
                if (Account* acc = db_->get_account(sel_id_)) {
                    acc->set_password(SecureString(ef_pass_));
                    db_->mark_dirty();
                }
                set_status("Password applied.");
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0, 8);
        if (button("Cancel", {80, 0})) ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
}

void App::regenerate_password() {
    std::string pw;
    if (gen_phrase_) {
        pw = PasswordGenerator::generate_passphrase(gen_words_, gen_sep_);
    } else {
        pw = PasswordGenerator::generate(gen_len_, gen_lc_, gen_uc_, gen_dig_, gen_sym_);
    }
    safe_copy(gen_prev_, sizeof(gen_prev_), pw);
}

// ── CHANGE MASTER PASSWORD ────────────────────────────────────────────────────

void App::render_chgpw_popup() {
    if (chg_open_) {
        ImGui::OpenPopup("##chgpw");
        chg_open_ = false;
        chg_err_.clear();
    }
    ImGui::SetNextWindowSize({400, 220}, ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {0.5f,0.5f});
    if (ImGui::BeginPopupModal("##chgpw", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
        ImGui::SetWindowFontScale(1.2f);
        ImGui::Text("Change Master Password");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Separator();

        ImGuiInputTextFlags pf = chg_show_ ? 0 : ImGuiInputTextFlags_Password;
        ImGui::Text("New password");
        ImGui::SetNextItemWidth(260);
        ImGui::InputText("##cp1", chg_pw_,  sizeof(chg_pw_),  pf);
        ImGui::Text("Confirm");
        ImGui::SetNextItemWidth(260);
        ImGui::InputText("##cp2", chg_con_, sizeof(chg_con_), pf);
        ImGui::SameLine(0,4);
        if (small_button(chg_show_ ? "Hide" : "Show")) chg_show_ = !chg_show_;

        if (!chg_err_.empty())
            ImGui::TextColored({1,0.4f,0.4f,1}, "%s", chg_err_.c_str());

        ImGui::Spacing();
        bool match = std::strcmp(chg_pw_, chg_con_) == 0;
        bool ok = chg_pw_[0] && match;
        if (!ok) ImGui::BeginDisabled();
        if (button("Change Password", {160, 0})) {
            try {
                db_->change_master_password(SecureString(chg_pw_));
                db_->save();
                SecureMemory::secure_zero(chg_pw_,  sizeof(chg_pw_));
                SecureMemory::secure_zero(chg_con_, sizeof(chg_con_));
                set_status("Master password changed and saved.");
                ImGui::CloseCurrentPopup();
            } catch (const std::exception& e) {
                chg_err_ = e.what();
            }
        }
        if (!ok) ImGui::EndDisabled();
        ImGui::SameLine(0,8);
        if (button("Cancel", {80, 0})) {
            SecureMemory::secure_zero(chg_pw_,  sizeof(chg_pw_));
            SecureMemory::secure_zero(chg_con_, sizeof(chg_con_));
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// ── PREFERENCES ───────────────────────────────────────────────────────────────

void App::render_prefs_popup() {
    if (prefs_open_) {
        ImGui::OpenPopup("##prefs");
        prefs_open_ = false;
    }
    ImGui::SetNextWindowSize({440, 400}, ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
    if (ImGui::BeginPopupModal("##prefs", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
        ImGui::SetWindowFontScale(1.2f);
        ImGui::Text("Preferences");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Separator();
        ImGui::Spacing();

        // ── Look and Feel ──
        // "Look and Feel" is the widget skin/engine (Modern / Retro / Windows
        // XP); the color theme underneath it only picks the palette within
        // that family.
        ImGui::TextDisabled("Look and Feel");
        ImGui::Spacing();

        LookAndFeel cur_laf = look_and_feel_of(prefs_theme_);
        static const char* lafs[] = {"Modern", "Retro", "Windows XP"};
        int laf_idx = (int)cur_laf;
        ImGui::Text("Look and Feel");
        ImGui::SameLine(140);
        ImGui::SetNextItemWidth(160);
        if (ImGui::Combo("##laf", &laf_idx, lafs, IM_ARRAYSIZE(lafs))) {
            LookAndFeel new_laf = (LookAndFeel)laf_idx;
            if (new_laf != cur_laf) {
                prefs_theme_ = default_theme_for(new_laf);
                apply_theme(prefs_theme_);
                Config::instance().set_theme(theme_to_string(prefs_theme_));
                cur_laf = new_laf;
            }
        }

        static const char* modern_themes[] = {"Original", "Light", "Classic"};
        static const char* retro_themes[]  = {"Win95", "Blue", "Wave", "Dark"};
        static const char* xp_themes[]     = {"Blue", "Silver", "Olive Green"};

        const char** theme_opts;
        int theme_count;
        int theme_base;
        switch (cur_laf) {
        case LookAndFeel::Retro:
            theme_opts = retro_themes; theme_count = IM_ARRAYSIZE(retro_themes);
            theme_base = (int)Theme::RetroWin95;
            break;
        case LookAndFeel::WindowsXP:
            theme_opts = xp_themes; theme_count = IM_ARRAYSIZE(xp_themes);
            theme_base = (int)Theme::XPBlue;
            break;
        case LookAndFeel::Modern:
        default:
            theme_opts = modern_themes; theme_count = IM_ARRAYSIZE(modern_themes);
            theme_base = (int)Theme::Original;
            break;
        }
        int theme_idx = (int)prefs_theme_ - theme_base;

        ImGui::Text("Color Theme");
        ImGui::SameLine(140);
        ImGui::SetNextItemWidth(160);
        if (ImGui::Combo("##colortheme", &theme_idx, theme_opts, theme_count)) {
            prefs_theme_ = (Theme)(theme_base + theme_idx);
            apply_theme(prefs_theme_);
            Config::instance().set_theme(theme_to_string(prefs_theme_));
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ── Font ──
        ImGui::TextDisabled("Font");
        ImGui::Spacing();

        const auto& fonts = available_fonts();
        int font_idx = 0;
        for (size_t i = 0; i < fonts.size(); ++i) {
            if (fonts[i].id == font_id_) { font_idx = (int)i; break; }
        }
        std::vector<const char*> font_labels;
        for (const auto& f : fonts) font_labels.push_back(f.label.c_str());

        ImGui::Text("Font Family");
        ImGui::SameLine(140);
        ImGui::SetNextItemWidth(200);
        if (ImGui::Combo("##fontfamily", &font_idx, font_labels.data(), (int)font_labels.size())) {
            font_id_ = fonts[font_idx].id;
            Config::instance().set_font_id(font_id_);
            font_dirty_ = true;
        }

        ImGui::Text("Font Size");
        ImGui::SameLine(140);
        ImGui::SetNextItemWidth(200);
        // Only commit + rebuild the font atlas once the drag is released,
        // rather than on every intermediate frame while dragging.
        ImGui::SliderInt("##fontsize", &font_size_, 12, 22);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            Config::instance().set_font_size(font_size_);
            font_dirty_ = true;
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ── Password generation defaults ──
        ImGui::TextDisabled("Default password generation options");
        ImGui::Spacing();

        bool changed = false;
        changed |= ImGui::Checkbox("Lowercase", &gen_lc_); ImGui::SameLine(0,8);
        changed |= ImGui::Checkbox("Uppercase", &gen_uc_); ImGui::SameLine(0,8);
        changed |= ImGui::Checkbox("Digits",    &gen_dig_); ImGui::SameLine(0,8);
        changed |= ImGui::Checkbox("Symbols",   &gen_sym_);

        changed |= ImGui::SliderInt("Length", &gen_len_, 6, 64);
        ImGui::Spacing();
        changed |= ImGui::Checkbox("Passphrase mode", &gen_phrase_);
        if (gen_phrase_) {
            changed |= ImGui::SliderInt("Words", &gen_words_, 2, 8);
            ImGui::SetNextItemWidth(60);
            changed |= ImGui::InputText("Separator", gen_sep_, sizeof(gen_sep_));
        }

        if (changed) {
            PasswordGenDefaults d;
            d.length     = gen_len_;
            d.lowercase  = gen_lc_;
            d.uppercase  = gen_uc_;
            d.digits     = gen_dig_;
            d.symbols    = gen_sym_;
            d.passphrase = gen_phrase_;
            d.words      = gen_words_;
            d.separator  = gen_sep_;
            Config::instance().set_password_gen_defaults(d);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (button("Close", {100, 0})) ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
}

// ── CONFIRMATIONS ─────────────────────────────────────────────────────────────

void App::render_delete_confirm_popup() {
    if (del_confirm_open_) {
        ImGui::OpenPopup("##delconfirm");
        del_confirm_open_ = false;
    }
    ImGui::SetNextWindowSize({380, 0}, ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
    if (ImGui::BeginPopupModal("##delconfirm", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
        ImGui::SetWindowFontScale(1.2f);
        ImGui::TextColored({1.0f, 0.6f, 0.2f, 1.0f}, "Delete Account");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Separator();
        ImGui::Spacing();

        const Account* acc = db_ ? db_->get_account(sel_id_) : nullptr;
        std::string name = (acc && !acc->name().empty()) ? acc->name() : "(unnamed)";
        ImGui::TextWrapped("Delete \"%s\"? This cannot be undone.", name.c_str());

        ImGui::Spacing();
        ImGui::Spacing();
        if (button("Delete", {120, 0})) {
            do_delete_selected();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0, 8);
        if (button("Cancel", {100, 0})) ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
}

void App::render_quit_confirm_popup() {
    if (quit_confirm_open_) {
        ImGui::OpenPopup("##quitconfirm");
        quit_confirm_open_ = false;
    }
    ImGui::SetNextWindowSize({380, 0}, ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
    if (ImGui::BeginPopupModal("##quitconfirm", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
        ImGui::SetWindowFontScale(1.2f);
        ImGui::TextColored({1.0f, 0.6f, 0.2f, 1.0f}, "Unsaved Changes");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextWrapped("You have unsaved changes. Quit anyway?");

        ImGui::Spacing();
        ImGui::Spacing();
        if (button("Save & Exit", {120, 0})) {
            do_save();
            wants_quit_ = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0, 8);
        if (button("Discard & Exit", {130, 0})) {
            wants_quit_ = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0, 8);
        if (button("Cancel", {90, 0})) ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
}

// ── ACTIONS ───────────────────────────────────────────────────────────────────

void App::do_save() {
    if (!db_) return;
    try {
        db_->save();
        set_status("Saved.");
    } catch (const std::exception& e) {
        set_status(std::string("Save failed: ") + e.what(), true);
    }
}

void App::do_add_account() {
    if (!db_) return;
    Account a;
    std::string new_id = a.id();
    db_->add_account(std::move(a));
    do_select_account(new_id);
    focus_name_ = true;
}

void App::do_delete_selected() {
    if (!db_ || sel_id_.empty()) return;
    db_->remove_account(sel_id_);
    sel_id_.clear();
    clear_editor();

    // Select first remaining account
    auto all = db_->get_all_accounts();
    if (!all.empty()) do_select_account(all[0]->id());
}

void App::do_select_account(const std::string& id) {
    if (id == sel_id_) return;
    sel_id_ = id;
    clear_editor();
    const Account* acc = db_->get_account(id);
    if (acc) load_account_to_editor(*acc);
}

void App::sync_editor_to_db() {
    if (!db_ || sel_id_.empty()) return;
    Account* acc = db_->get_account(sel_id_);
    if (!acc) return;
    acc->set_name(ef_name_);
    acc->set_email(ef_email_);
    acc->set_username(ef_user_);
    acc->set_url(ef_url_);
    acc->set_notes(ef_notes_);
    acc->set_totp_secret(ef_totp_);
    std::string cur(acc->password().c_str());
    if (cur != ef_pass_) acc->set_password(SecureString(ef_pass_));
    db_->mark_dirty();
}

void App::load_account_to_editor(const Account& acc) {
    safe_copy(ef_name_,  sizeof(ef_name_),  acc.name());
    safe_copy(ef_email_, sizeof(ef_email_), acc.email());
    safe_copy(ef_user_,  sizeof(ef_user_),  acc.username());
    safe_copy(ef_url_,   sizeof(ef_url_),   acc.url());
    safe_copy(ef_totp_,  sizeof(ef_totp_),  acc.totp_secret());
    safe_copy(ef_notes_, sizeof(ef_notes_), acc.notes());

    const auto& pw = acc.password();
    size_t len = std::min(pw.size(), sizeof(ef_pass_) - 1);
    std::memcpy(ef_pass_, pw.c_str(), len);
    ef_pass_[len] = '\0';
    ef_showp_ = false;
}

void App::clear_editor() {
    SecureMemory::secure_zero(ef_pass_, sizeof(ef_pass_));
    std::memset(ef_name_,  0, sizeof(ef_name_));
    std::memset(ef_email_, 0, sizeof(ef_email_));
    std::memset(ef_user_,  0, sizeof(ef_user_));
    std::memset(ef_url_,   0, sizeof(ef_url_));
    std::memset(ef_totp_,  0, sizeof(ef_totp_));
    std::memset(ef_notes_, 0, sizeof(ef_notes_));
    ef_showp_ = false;
}

void App::do_browse_open(char* buf, size_t n) {
    try {
        auto r = pfd::open_file("Open Database", buf[0] ? buf : "",
                                {"PIF Database (*.pif)", "*.pif",
                                 "All Files", "*"}).result();
        if (!r.empty()) std::strncpy(buf, r[0].c_str(), n - 1);
    } catch (...) {}
}

void App::do_browse_save(char* buf, size_t n) {
    try {
        std::string r = pfd::save_file("Save Database", buf[0] ? buf : "database.pif",
                                       {"PIF Database (*.pif)", "*.pif"}).result();
        if (!r.empty()) std::strncpy(buf, r.c_str(), n - 1);
    } catch (...) {}
}

void App::clipboard(const char* text) {
    SDL_SetClipboardText(text);
    set_status("Copied to clipboard.");
}

void App::set_status(const std::string& msg, bool err) {
    status_    = msg;
    status_err_ = err;
    status_t_   = 4.0f;
}

// ── HELPERS ───────────────────────────────────────────────────────────────────

std::vector<const Account*> App::filtered_accounts() const {
    if (!db_) return {};
    std::string q(search_);
    if (q.empty()) {
        auto tmp = db_->get_all_accounts();
        return {tmp.begin(), tmp.end()};
    }
    return db_->search_accounts(q);
}

int App::pw_strength(const char* pw) const {
    if (!pw || !pw[0]) return 0;
    std::string s(pw);
    bool lc = false, uc = false, dig = false, sym = false;
    for (char c : s) {
        if (std::islower((unsigned char)c)) lc  = true;
        else if (std::isupper((unsigned char)c)) uc  = true;
        else if (std::isdigit((unsigned char)c)) dig = true;
        else                                     sym = true;
    }
    int score = (int)lc + (int)uc + (int)dig + (int)sym;
    if (s.size() >= 12) score++;
    if (s.size() >= 16) score++;
    if (s.size() >= 20) score++;
    return std::min(score, 4);
}

const char* App::strength_label(int s) const {
    static const char* L[] = {"Very Weak","Weak","Fair","Good","Strong"};
    return L[std::min(s, 4)];
}

} // namespace pasgen
