#include "app/App.hpp"
#include "util/Config.hpp"
#include "util/Font.hpp"
#include "util/PasswordGenerator.hpp"
#include "util/TOTPGenerator.hpp"
#include "util/Theme.hpp"
#include "util/Widgets.hpp"
#include "imgui.h"
#include "imgui_stdlib.h"
#include "portable-file-dialogs.h"
#include <SDL.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace pasgen {

namespace ui_ = ui;

// ── helpers ──────────────────────────────────────────────────────────────────

// Zeroes a std::string's buffer before releasing it. A plain clear() leaves the
// characters sitting in the freed allocation.
static void secure_clear_string(std::string& s) {
    if (!s.empty()) SecureMemory::secure_zero(&s[0], s.size());
    s.clear();
    s.shrink_to_fit();
}

static void safe_copy(char* dst, size_t n, const std::string& src) {
    std::strncpy(dst, src.c_str(), n - 1);
    dst[n - 1] = '\0';
}

static std::string fmt_time(std::chrono::system_clock::time_point tp) {
    auto t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = *std::localtime(&t);
    std::ostringstream ss;
    ss << std::put_time(&tm, "%b %e, %Y");
    return ss.str();
}

// Splits a 6-digit TOTP into two groups so it can be read at a glance.
static std::string group_totp(const std::string& code) {
    if (code.size() != 6) return code;
    return code.substr(0, 3) + " " + code.substr(3);
}

// ── App ──────────────────────────────────────────────────────────────────────

App::App() {
    std::string last = Config::instance().get_last_database_path();
    if (!last.empty()) safe_copy(lp_, sizeof(lp_), last);
    prefs_theme_ = theme_from_string(Config::instance().get_theme());
    font_id_     = Config::instance().get_font_id();
    font_size_   = Config::instance().get_font_size();
    autolock_min_ = Config::instance().get_autolock_minutes();
    clip_clear_s_ = Config::instance().get_clipboard_clear_seconds();
    load_generator_defaults();
}

App::~App() {
    SecureMemory::secure_zero(lpw_,     sizeof(lpw_));
    SecureMemory::secure_zero(cpw_,     sizeof(cpw_));
    SecureMemory::secure_zero(ccon_,    sizeof(ccon_));
    secure_clear_string(ef_pass_);
    SecureMemory::secure_zero(chg_pw_,  sizeof(chg_pw_));
    SecureMemory::secure_zero(chg_con_, sizeof(chg_con_));
    secure_clear_string(clip_copy_);
}

bool App::consume_font_dirty() {
    bool d = font_dirty_;
    font_dirty_ = false;
    return d;
}

ImVec4 App::background_color() const { return palette().bg; }

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

void App::request_quit() {
    if (screen_ == Screen::MAIN && db_ && db_->is_dirty()) {
        quit_confirm_open_ = true;
        return;
    }
    wants_quit_ = true;
}

void App::render(int w, int h) {
    const float dt = ImGui::GetIO().DeltaTime;
    status_t_ -= dt;
    tick_security(dt);

    if (screen_ == Screen::MAIN && db_) {
        auto& io = ImGui::GetIO();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) do_save();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_L)) do_lock("Locked.");
    }

    switch (screen_) {
    case Screen::LOGIN:     render_login(w, h);     break;
    case Screen::CREATE_DB: render_create_db(w, h); break;
    case Screen::MAIN:      render_main(w, h);      break;
    }
}

// ── SHARED SCREEN CHROME ─────────────────────────────────────────────────────

void App::begin_centered_screen(int w, int h, const char* id, float card_w, float card_h) {
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, palette().bg);
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({(float)w, (float)h});
    ImGui::Begin(id, nullptr, flags);

    ImGui::SetCursorPos({(w - card_w) * 0.5f, std::max(24.0f, (h - card_h) * 0.5f)});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(32, 28));
    ui_::BeginCard("##card", {card_w, card_h});
}

void App::end_centered_screen() {
    ui_::EndCard();
    ImGui::PopStyleVar();   // the extra WindowPadding pushed in begin_centered_screen
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

float App::measure_card_content() const {
    // Cursor Y already includes the card's top padding; add the bottom padding
    // to get the height the card needs in order to show everything.
    return ImGui::GetCursorPosY() + ImGui::GetStyle().WindowPadding.y;
}

bool App::field(const char* label, const char* id, char* buf, size_t n,
                 float width, ImGuiInputTextFlags flags, const char* hint) {
    ui_::FieldLabel(label);
    ImGui::SetNextItemWidth(width);
    if (hint) return ImGui::InputTextWithHint(id, hint, buf, n, flags);
    return ImGui::InputText(id, buf, n, flags);
}

// Returns true only once the user has finished editing (focus left, or Enter),
// never mid-typing. The detail pane writes straight through to the Account, and
// Account::set_password archives the previous value into a 10-entry history --
// so a per-keystroke write turns every character typed into a separate history
// entry and destroys the real ones. Commit-on-finish is what makes that safe.
bool App::field(const char* label, const char* id, std::string& buf,
                float width, ImGuiInputTextFlags flags, const char* hint) {
    ui_::FieldLabel(label);
    ImGui::SetNextItemWidth(width);
    if (hint) return ImGui::InputTextWithHint(id, hint, &buf, flags);
    return ImGui::InputText(id, &buf, flags);
}

bool App::field_commit(const char* label, const char* id, std::string& buf,
                       float width, ImGuiInputTextFlags flags, const char* hint) {
    field(label, id, buf, width, flags, hint);
    return ImGui::IsItemDeactivatedAfterEdit();
}

bool App::field_commit(const char* label, const char* id, char* buf, size_t n,
                       float width, ImGuiInputTextFlags flags, const char* hint) {
    field(label, id, buf, n, width, flags, hint);
    return ImGui::IsItemDeactivatedAfterEdit();
}

// ── LOGIN ────────────────────────────────────────────────────────────────────

void App::render_login(int w, int h) {
    const Palette& p = palette();
    const float card_w = 460.0f;
    const float card_h = login_card_h_ > 0.0f ? login_card_h_ : 420.0f;

    begin_centered_screen(w, h, "##login_bg", card_w, card_h);

    const float inner = ImGui::GetContentRegionAvail().x;

    ui_::Title("Pasgen");
    ui_::Caption("Secure password manager");
    ImGui::Dummy({0, 14});

    // Database path + browse
    const float browse_w = 86.0f;
    field("DATABASE FILE", "##lpath", lp_, sizeof(lp_),
          inner - browse_w - ImGui::GetStyle().ItemSpacing.x, 0, "path/to/vault.pif");
    ImGui::SameLine(0, ImGui::GetStyle().ItemSpacing.x);
    if (ui_::SecondaryButton("Browse", {browse_w, 0})) do_browse_open(lp_, sizeof(lp_));

    ImGui::Dummy({0, 4});

    // Master password + show/hide
    const float show_w = 66.0f;
    ImGuiInputTextFlags pf = ImGuiInputTextFlags_EnterReturnsTrue;
    if (!lshow_) pf |= ImGuiInputTextFlags_Password;
    bool enter = field("MASTER PASSWORD", "##lpw", lpw_, sizeof(lpw_),
                       inner - show_w - ImGui::GetStyle().ItemSpacing.x, pf,
                       lshow_ ? nullptr : nullptr);
    ImGui::SameLine(0, ImGui::GetStyle().ItemSpacing.x);
    if (ui_::SecondaryButton(lshow_ ? "Hide" : "Show", {show_w, 0})) lshow_ = !lshow_;
    if (enter) do_login();

    ImGui::Dummy({0, 12});

    if (!lerr_.empty()) {
        ui_::Banner(lerr_.c_str(), p.danger);
        ImGui::Dummy({0, 10});
    }

    if (ui_::PrimaryButton("Unlock Vault", {inner, 44})) do_login();

    ImGui::Dummy({0, 8});
    ui_::Caption("No vault yet?");
    ImGui::SameLine(0, 6);
    if (ui_::LinkButton("Create a new database")) {
        screen_ = Screen::CREATE_DB;
        lerr_.clear();
        SecureMemory::secure_zero(cpw_,  sizeof(cpw_));
        SecureMemory::secure_zero(ccon_, sizeof(ccon_));
        cshow_ = false; cerr_.clear();
    }

    login_card_h_ = measure_card_content();
    end_centered_screen();
}

void App::do_login() {
    if (lp_[0] == '\0')  { lerr_ = "Please choose a database file."; return; }
    if (lpw_[0] == '\0') { lerr_ = "Please enter the master password."; return; }
    try {
        SecureString pw(lpw_);
        db_ = Database::open(lp_, pw);
        Config::instance().set_last_database_path(lp_);
        SecureMemory::secure_zero(lpw_, sizeof(lpw_));
        lerr_.clear();
        screen_ = Screen::MAIN;
        auto all = db_->get_all_accounts();
        if (!all.empty()) do_select_account(all[0]->id());
    } catch (const std::exception& e) {
        lerr_ = e.what();
    }
}

// ── CREATE DB ────────────────────────────────────────────────────────────────

void App::render_create_db(int w, int h) {
    const Palette& p = palette();
    const float card_w = 460.0f;
    const float card_h = create_card_h_ > 0.0f ? create_card_h_ : 540.0f;

    begin_centered_screen(w, h, "##create_bg", card_w, card_h);

    const float inner = ImGui::GetContentRegionAvail().x;
    const float sp    = ImGui::GetStyle().ItemSpacing.x;

    ui_::Title("Create Database");
    ui_::Caption("Your vault is encrypted with this password. It cannot be recovered.");
    ImGui::Dummy({0, 14});

    const float browse_w = 86.0f;
    field("SAVE TO", "##cpath", cp_, sizeof(cp_), inner - browse_w - sp, 0, "vault.pif");
    ImGui::SameLine(0, sp);
    if (ui_::SecondaryButton("Browse", {browse_w, 0})) do_browse_save(cp_, sizeof(cp_));

    ImGui::Dummy({0, 4});

    const float show_w = 66.0f;
    ImGuiInputTextFlags pf = cshow_ ? 0 : ImGuiInputTextFlags_Password;
    field("MASTER PASSWORD", "##cpw", cpw_, sizeof(cpw_), inner - show_w - sp, pf);
    ImGui::SameLine(0, sp);
    if (ui_::SecondaryButton(cshow_ ? "Hide" : "Show", {show_w, 0})) cshow_ = !cshow_;

    ImGui::Dummy({0, 4});
    field("CONFIRM PASSWORD", "##ccon", ccon_, sizeof(ccon_), inner - show_w - sp, pf);

    ImGui::Dummy({0, 12});

    int str = pw_strength(cpw_);
    ui_::FieldLabel("STRENGTH");
    ui_::StrengthMeter(cpw_[0] ? strength_frac(str) : 0.0f, strength_color(str),
                       cpw_[0] ? strength_label(str) : "", inner * 0.55f);

    ImGui::Dummy({0, 8});

    bool match = std::strcmp(cpw_, ccon_) == 0;
    if (cpw_[0] && ccon_[0]) {
        if (match) ui_::Pill("Passwords match", p.success);
        else       ui_::Pill("Passwords do not match", p.danger);
    }

    if (!cerr_.empty()) {
        ImGui::Dummy({0, 6});
        ui_::Banner(cerr_.c_str(), p.danger);
    }

    ImGui::Dummy({0, 14});

    float half = (inner - sp) * 0.5f;
    if (ui_::SecondaryButton("Cancel", {half, 44})) {
        screen_ = Screen::LOGIN;
        SecureMemory::secure_zero(cpw_,  sizeof(cpw_));
        SecureMemory::secure_zero(ccon_, sizeof(ccon_));
    }
    ImGui::SameLine(0, sp);
    bool can_create = cp_[0] && cpw_[0] && match;
    if (!can_create) ImGui::BeginDisabled();
    if (ui_::PrimaryButton("Create Database", {half, 44})) do_create_db();
    if (!can_create) ImGui::EndDisabled();

    create_card_h_ = measure_card_content();
    end_centered_screen();
}

void App::do_create_db() {
    if (cp_[0] == '\0')  { cerr_ = "Please choose a file path."; return; }
    if (cpw_[0] == '\0') { cerr_ = "Please enter a master password."; return; }
    if (std::strcmp(cpw_, ccon_) != 0) { cerr_ = "Passwords do not match."; return; }
    try {
        std::string path = cp_;
        if (path.size() < 4 || path.substr(path.size() - 4) != ".pif")
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

// ── MAIN ─────────────────────────────────────────────────────────────────────

void App::render_main(int w, int h) {
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 12));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, palette().bg);
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({(float)w, (float)h});
    ImGui::Begin("##main", nullptr, flags);
    ImGui::PopStyleVar(3);

    render_menu_bar();
    render_toolbar();

    ImGui::Dummy({0, 2});

    // Fixed-height footer so the panels never resize as status text changes.
    const float footer_h = ImGui::GetFrameHeight() + 10.0f;
    const float avail_h  = ImGui::GetContentRegionAvail().y - footer_h;
    const float side_w   = 288.0f;
    const float detail_w = ImGui::GetContentRegionAvail().x - side_w - ImGui::GetStyle().ItemSpacing.x;

    render_sidebar(side_w, avail_h);
    ImGui::SameLine();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22, 18));
    ui_::BeginCard("##detail", {detail_w, avail_h}, true);
    if (sel_id_.empty()) render_detail_placeholder(detail_w);
    else                 render_account_detail(detail_w);
    ui_::EndCard();
    ImGui::PopStyleVar();

    render_status_bar();

    render_gen_popup();
    render_chgpw_popup();
    render_prefs_popup();
    render_delete_confirm_popup();
    render_quit_confirm_popup();
    render_about_popup();
    render_category_add_popup();
    render_category_rename_popup();
    render_category_delete_confirm_popup();

    ImGui::End();
    ImGui::PopStyleColor();
}

void App::render_menu_bar() {
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, palette().bg);
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Database")) {
                screen_ = Screen::CREATE_DB;
                SecureMemory::secure_zero(cpw_,  sizeof(cpw_));
                SecureMemory::secure_zero(ccon_, sizeof(ccon_));
                cp_[0] = '\0'; cshow_ = false; cerr_.clear();
            }
            if (ImGui::MenuItem("Open Database")) {
                char path[512] = {};
                do_browse_open(path, sizeof(path));
                if (path[0]) {
                    safe_copy(lp_, sizeof(lp_), std::string(path));
                    screen_ = Screen::LOGIN;
                    lerr_.clear();
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save", "Ctrl+S", false, db_ && db_->is_dirty())) do_save();
            if (ImGui::MenuItem("Save As...", nullptr, false, db_ != nullptr)) do_save_as();
            if (ImGui::MenuItem("Change Master Password")) chg_open_ = true;
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) request_quit();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Settings")) {
            if (ImGui::MenuItem("Preferences...")) prefs_open_ = true;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About Pasgen")) about_open_ = true;
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    ImGui::PopStyleColor();
}

void App::render_toolbar() {
    const float sp = ImGui::GetStyle().ItemSpacing.x;

    if (ui_::PrimaryButton("+  Add Account")) do_add_account();

    ImGui::SameLine(0, sp);
    if (sel_id_.empty()) ImGui::BeginDisabled();
    if (ui_::SecondaryButton("Delete")) del_confirm_open_ = true;
    if (sel_id_.empty()) ImGui::EndDisabled();

    ImGui::SameLine(0, sp);
    bool dirty = db_ && db_->is_dirty();
    if (!dirty) ImGui::BeginDisabled();
    if (ui_::SecondaryButton("Save")) do_save();
    if (!dirty) ImGui::EndDisabled();

    ImGui::SameLine(0, sp);
    if (ui_::SecondaryButton("Save As...")) do_save_as();

    // Search, right-aligned on the same row. SameLine() first: without it the
    // cursor has already wrapped and setting X alone leaves the box a row down.
    const float search_w = 260.0f;
    const float arch_w   = 110.0f;
    ImGui::SameLine(0, sp);
    float x = ImGui::GetWindowContentRegionMax().x - search_w - arch_w - sp;
    if (x > ImGui::GetCursorPosX()) ImGui::SetCursorPosX(x);
    ImGui::AlignTextToFramePadding();
    ImGui::Checkbox("Show archived", &show_archived_);
    ImGui::SameLine(0, sp);
    x = ImGui::GetWindowContentRegionMax().x - search_w;
    if (x > ImGui::GetCursorPosX()) ImGui::SetCursorPosX(x);
    ImGui::SetNextItemWidth(search_w);
    ImGui::InputTextWithHint("##search", "Search accounts...", search_, sizeof(search_));
}

// ── SIDEBAR ──────────────────────────────────────────────────────────────────

void App::render_sidebar(float width, float height) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 14));
    ui_::BeginCard("##sidebar", {width, height}, true);

    std::string q(search_);
    bool searching = !q.empty();

    ui_::FieldLabel("VAULT");
    ImGui::SameLine();
    float btn_w = 104.0f;
    float x = ImGui::GetWindowContentRegionMax().x - btn_w;
    if (x > ImGui::GetCursorPosX()) ImGui::SetCursorPosX(x);
    if (ui_::TinyButton("+ Category")) {
        cat_add_open_ = true;
        cat_add_name_[0] = '\0';
    }

    ImGui::Dummy({0, 4});

    if (searching) {
        ui_::Caption("Filtered - clear search to reorder");
        ImGui::Dummy({0, 2});
    }

    const auto& cats = db_->categories();
    for (size_t i = 0; i < cats.size(); ++i)
        render_category_section(cats[i].id(), cats[i].name(), true, (int)i, searching, q);
    render_category_section("", "Uncategorized", false, -1, searching, q);

    if (db_->account_count() == 0) {
        ImGui::Dummy({0, 8});
        ui_::Caption("No accounts yet.");
        ui_::Caption("Click \"+ Add Account\" to begin.");
    }

    ui_::EndCard();
    ImGui::PopStyleVar();
}

void App::render_category_section(const std::string& category_id, const std::string& label, bool deletable,
                                   int color_index, bool searching, const std::string& query) {
    const Palette& p = palette();
    auto accts = db_->get_accounts_in_category(category_id);

    std::vector<const Account*> shown;
    shown.reserve(accts.size());
    for (Account* a : accts) {
        if (a->archived() && !show_archived_) continue;
        if (!searching || a->matches_search(query)) shown.push_back(a);
    }
    // Favourites float to the top. stable_sort, so the user's manual drag
    // ordering still decides the sequence within each group.
    std::stable_sort(shown.begin(), shown.end(),
                     [](const Account* a, const Account* b) {
                         return a->favorite() && !b->favorite();
                     });
    // Only hide an empty section while searching. Outside search an empty
    // category must stay visible, or there is nothing to drag accounts onto.
    if (searching && shown.empty()) return;

    ImGui::PushID(category_id.empty() ? "##uncategorized" : category_id.c_str());

    ImVec4 color = color_index >= 0 ? p.category[color_index % 8] : p.text_muted;

    // Header: colored dot + name + count, drawn as a tree node so the whole
    // row stays one drag/drop and context-menu target.
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen |
                                ImGuiTreeNodeFlags_SpanAvailWidth |
                                ImGuiTreeNodeFlags_FramePadding;
    ImGui::PushStyleColor(ImGuiCol_Text, p.text_dim);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, p.surface_hover);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, p.surface_hover);
    bool open = ImGui::TreeNodeEx("##hdr", flags, "     %s   (%d)", label.c_str(), (int)shown.size());
    ImGui::PopStyleColor(3);

    // Dot goes in the gap reserved by the leading spaces above.
    {
        ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
        float arrow = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::GetWindowDrawList()->AddCircleFilled(
            {mn.x + arrow + 5.0f, (mn.y + mx.y) * 0.5f}, 4.0f, ImGui::GetColorU32(color), 12);
    }

    if (deletable && !searching && ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload("CATEGORY_ID", category_id.c_str(), category_id.size() + 1);
        ui_::Dimmed("Move category: %s", label.c_str());
        ImGui::EndDragDropSource();
    }
    if (!searching && ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ACCOUNT_ID"))
            db_->move_account(std::string((const char*)pl->Data), category_id, INT_MAX);
        if (deletable) {
            if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("CATEGORY_ID")) {
                std::string dragged_id((const char*)pl->Data);
                int target_idx = 0;
                const auto& cats = db_->categories();
                for (size_t i = 0; i < cats.size(); ++i)
                    if (cats[i].id() == category_id) { target_idx = (int)i; break; }
                db_->reorder_category(dragged_id, target_idx);
            }
        }
        ImGui::EndDragDropTarget();
    }
    if (deletable && ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Rename")) {
            cat_rename_open_ = true;
            cat_rename_id_   = category_id;
            safe_copy(cat_rename_name_, sizeof(cat_rename_name_), label);
        }
        if (ImGui::MenuItem("Delete")) {
            cat_delete_confirm_open_ = true;
            cat_delete_id_ = category_id;
        }
        ImGui::EndPopup();
    }

    if (open) {
        ImGui::Indent(10.0f);
        const float line_h = ImGui::GetTextLineHeight();
        const float row_h  = line_h * 2.0f + 12.0f;

        for (size_t i = 0; i < shown.size(); ++i) {
            const Account* acc = shown[i];
            ImGui::PushID(acc->id().c_str());

            bool selected   = (acc->id() == sel_id_);
            ImVec2 row_min  = ImGui::GetCursorScreenPos();

            ImGui::PushStyleColor(ImGuiCol_Header,        p.accent_soft);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, p.surface_hover);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive,  p.accent_soft);
            if (ImGui::Selectable("##sel", selected, 0, {0, row_h}))
                do_select_account(acc->id());
            ImGui::PopStyleColor(3);

            if (!searching && ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("ACCOUNT_ID", acc->id().c_str(), acc->id().size() + 1);
                ui_::Dimmed("%s", acc->name().empty() ? "(unnamed)" : acc->name().c_str());
                ImGui::EndDragDropSource();
            }
            if (!searching && ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ACCOUNT_ID"))
                    db_->move_account(std::string((const char*)pl->Data), category_id, (int)i);
                ImGui::EndDragDropTarget();
            }

            ImVec2 row_max = ImGui::GetItemRectMax();
            ImDrawList* dl = ImGui::GetWindowDrawList();

            // Accent rail marks the selected row without relying on fill alone.
            if (selected) {
                dl->AddRectFilled({row_min.x, row_min.y + 3},
                                  {row_min.x + 3, row_max.y - 3},
                                  ImGui::GetColorU32(p.accent), 1.5f);
            }

            const char* name = acc->name().empty() ? "(unnamed)" : acc->name().c_str();
            const std::string& sub = !acc->username().empty() ? acc->username() : acc->email();
            dl->AddText({row_min.x + 12, row_min.y + 5},
                        ImGui::GetColorU32(selected ? p.text : p.text), name);
            if (!sub.empty()) {
                if (fonts().caption) {
                    dl->AddText(fonts().caption, fonts().caption->FontSize,
                                {row_min.x + 12, row_min.y + 6 + line_h},
                                ImGui::GetColorU32(p.text_muted), sub.c_str());
                } else {
                    dl->AddText({row_min.x + 12, row_min.y + 6 + line_h},
                                ImGui::GetColorU32(p.text_muted), sub.c_str());
                }
            }

            ImGui::PopID();
        }

        if (shown.empty()) {
            ImGui::Dummy({0, 2});
            ui_::Caption("   Drop accounts here");
        }

        ImGui::Unindent(10.0f);
        ImGui::TreePop();
    }

    ImGui::PopID();
}

// ── DETAIL ───────────────────────────────────────────────────────────────────

void App::render_detail_placeholder(float width) {
    float y = ImGui::GetContentRegionAvail().y * 0.4f;
    ImGui::Dummy({0, y});
    const char* line1 = "No account selected";
    const char* line2 = "Pick one from the list, or add a new account.";
    float w1 = ImGui::CalcTextSize(line1).x;
    ImGui::SetCursorPosX(std::max(0.0f, (width - w1) * 0.5f));
    ui_::Heading("%s", line1);
    float w2 = ImGui::CalcTextSize(line2).x;
    ImGui::SetCursorPosX(std::max(0.0f, (width - w2) * 0.5f));
    ui_::Caption("%s", line2);
}

void App::render_account_detail(float width) {
    const Palette& p = palette();

    Account* acc = db_->get_account(sel_id_);
    if (!acc) { sel_id_.clear(); clear_editor(); return; }

    // Inputs stop well short of very wide windows; full-bleed fields look
    // unfinished and are harder to scan.
    const float avail = ImGui::GetContentRegionAvail().x;
    const float fw    = std::min(560.0f, avail);
    const float sp    = ImGui::GetStyle().ItemSpacing.x;
    (void)width;

    // ── Header ──
    ui_::Heading("%s", ef_name_.empty() ? "(unnamed)" : ef_name_.c_str());

    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8);
    {
        bool fav = acc->favorite();
        if (ImGui::Checkbox("Favorite", &fav)) { acc->set_favorite(fav); db_->mark_dirty(); }
        ImGui::SameLine(0, 10);
        bool arch = acc->archived();
        if (ImGui::Checkbox("Archived", &arch)) { acc->set_archived(arch); db_->mark_dirty(); }
    }
    ui_::Caption("Created %s   -   Modified %s",
                 fmt_time(acc->created_at()).c_str(),
                 fmt_time(acc->modified_at()).c_str());
    ui_::Divider(8);

    // ── Account ──
    ui_::SectionHeader("Account");

    if (focus_name_) { ImGui::SetKeyboardFocusHere(); focus_name_ = false; }
    if (field_commit("NAME", "##name", ef_name_, fw)) {
        acc->set_name(ef_name_); db_->mark_dirty();
    }
    if (field_commit("EMAIL", "##email", ef_email_, fw)) {
        acc->set_email(ef_email_); db_->mark_dirty();
    }

    const float copy_w = 62.0f;
    if (field_commit("USERNAME", "##user", ef_user_, fw - copy_w - sp)) {
        acc->set_username(ef_user_); db_->mark_dirty();
    }
    ImGui::SameLine(0, sp);
    if (ui_::SecondaryButton("Copy##cu", {copy_w, 0})) clipboard(ef_user_.c_str());

    if (field_commit("URL", "##url", ef_url_, fw)) {
        acc->set_url(ef_url_); db_->mark_dirty();
    }

    ui_::Divider(10);

    // ── Security ──
    ui_::SectionHeader("Security");

    const float act_w = 62.0f;
    const float gen_w = 84.0f;
    ImGuiInputTextFlags pf = ef_showp_ ? 0 : ImGuiInputTextFlags_Password;

    ui_::FieldLabel("PASSWORD");
    ImGui::SetNextItemWidth(fw - (act_w * 2 + gen_w + sp * 3));
    if (fonts().mono) ImGui::PushFont(fonts().mono);
    ImGui::InputText("##pass", &ef_pass_, pf);
    const bool pw_committed = ImGui::IsItemDeactivatedAfterEdit();
    if (fonts().mono) ImGui::PopFont();
    if (pw_committed) { acc->set_password(SecureString(ef_pass_.c_str(), ef_pass_.size())); db_->mark_dirty(); }

    ImGui::SameLine(0, sp);
    if (ui_::SecondaryButton(ef_showp_ ? "Hide" : "Show", {act_w, 0})) ef_showp_ = !ef_showp_;
    ImGui::SameLine(0, sp);
    if (ui_::SecondaryButton("Copy##cp", {act_w, 0})) clipboard(ef_pass_.c_str());
    ImGui::SameLine(0, sp);
    if (ui_::SecondaryButton("Generate", {gen_w, 0})) { gen_open_ = true; gen_regen_ = true; }

    {
        // Age comes from password_changed_at, not modified_at: editing any other
        // field must not make a stale password look freshly rotated.
        const int age = acc->password_age_days();
        if (age >= 180)
            ui_::Caption("Password last changed %d days ago - consider rotating it.", age);
        else if (age > 0)
            ui_::Caption("Password last changed %d days ago.", age);
        else
            ui_::Caption("Password changed today.");
    }

    if (!ef_pass_.empty()) {
        ImGui::Dummy({0, 2});
        int s = pw_strength(ef_pass_.c_str());
        ui_::StrengthMeter(strength_frac(s), strength_color(s), strength_label(s), fw * 0.45f);
    }

    ImGui::Dummy({0, 6});

    if (field_commit("TOTP SECRET", "##totp", ef_totp_, fw, 0,
              "Base32 secret (optional)")) {
        acc->set_totp_secret(ef_totp_); db_->mark_dirty();
    }

    if (!ef_totp_.empty()) {
        ImGui::Dummy({0, 4});
        try {
            std::string code = TOTPGenerator::generate(ef_totp_);
            uint32_t    rem  = TOTPGenerator::seconds_remaining();
            float       frac = (float)rem / 30.0f;
            ImVec4      tc   = rem > 10 ? p.success : p.danger;

            if (fonts().h2) ImGui::PushFont(fonts().h2);
            ImGui::PushStyleColor(ImGuiCol_Text, p.accent);
            ImGui::TextUnformatted(group_totp(code).c_str());
            ImGui::PopStyleColor();
            if (fonts().h2) ImGui::PopFont();

            ImGui::SameLine(0, 12);
            if (ui_::TinyButton("Copy##ct")) clipboard(code.c_str());
            ImGui::SameLine(0, 10);
            ui_::Caption("expires in %us", rem);

            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, tc);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, p.surface_hover);
            ImGui::ProgressBar(frac, {fw * 0.45f, 4}, "");
            ImGui::PopStyleColor(2);
        } catch (...) {
            ui_::Banner("Invalid TOTP secret.", p.danger);
        }
    }

    ui_::Divider(10);

    // ── Custom fields ──
    ui_::Divider(10);
    ui_::SectionHeader("Custom Fields");

    {
        const float del_w  = 28.0f;
        const float cpy_w  = 54.0f;
        const float chk_w  = 78.0f;
        const float name_w = (fw - del_w - cpy_w - chk_w - sp * 4) * 0.36f;
        const float val_w  = (fw - del_w - cpy_w - chk_w - sp * 4) - name_w;

        int remove_at = -1;
        for (size_t i = 0; i < ef_custom_.size(); ++i) {
            EditorCustomField& f = ef_custom_[i];
            ImGui::PushID((int)i);

            ImGui::SetNextItemWidth(name_w);
            ImGui::InputTextWithHint("##cfn", "Name", &f.name);
            bool commit = ImGui::IsItemDeactivatedAfterEdit();

            ImGui::SameLine(0, sp);
            ImGui::SetNextItemWidth(val_w);
            ImGui::InputTextWithHint("##cfv", "Value", &f.value,
                                     f.secret ? ImGuiInputTextFlags_Password : 0);
            commit = commit || ImGui::IsItemDeactivatedAfterEdit();

            ImGui::SameLine(0, sp);
            if (ImGui::Checkbox("Secret", &f.secret)) commit = true;

            ImGui::SameLine(0, sp);
            if (ui_::TinyButton("Copy")) clipboard(f.value.c_str());

            ImGui::SameLine(0, sp);
            if (ui_::TinyButton("X")) remove_at = (int)i;

            if (commit) {
                acc->set_custom_field(i, f.name,
                                      SecureString(f.value.c_str(), f.value.size()), f.secret);
                db_->mark_dirty();
            }
            ImGui::PopID();
        }
        if (remove_at >= 0) {
            acc->remove_custom_field((size_t)remove_at);
            db_->mark_dirty();
            sync_custom_fields(*acc);
        }

        ImGui::SetNextItemWidth(name_w);
        ImGui::InputTextWithHint("##cfnn", "New field", &cf_new_name_);
        ImGui::SameLine(0, sp);
        ImGui::SetNextItemWidth(val_w);
        ImGui::InputTextWithHint("##cfnv", "Value", &cf_new_value_,
                                 cf_new_secret_ ? ImGuiInputTextFlags_Password : 0);
        ImGui::SameLine(0, sp);
        ImGui::Checkbox("Secret##new", &cf_new_secret_);
        ImGui::SameLine(0, sp);
        ImGui::BeginDisabled(cf_new_name_.empty());
        if (ui_::TinyButton("+ Add")) {
            acc->add_custom_field(cf_new_name_,
                                  SecureString(cf_new_value_.c_str(), cf_new_value_.size()),
                                  cf_new_secret_);
            db_->mark_dirty();
            secure_clear_string(cf_new_name_);
            secure_clear_string(cf_new_value_);
            cf_new_secret_ = false;
            sync_custom_fields(*acc);
        }
        ImGui::EndDisabled();
    }

    // ── Notes ──
    ui_::SectionHeader("Notes");
    ImGui::InputTextMultiline("##notes", &ef_notes_, {fw, 110});
    if (ImGui::IsItemDeactivatedAfterEdit()) { acc->set_notes(ef_notes_); db_->mark_dirty(); }

    // ── History ──
    const auto& hist = acc->password_history();
    if (!hist.empty()) {
        ui_::Divider(10);
        ui_::SectionHeader("Password History");
        for (const auto& e : hist) {
            // Fully masked: revealing the leading character of every historic
            // password hands an attacker a free character per entry.
            std::string masked(e.password.size(), '*');
            if (fonts().mono) ImGui::PushFont(fonts().mono);
            ImGui::PushStyleColor(ImGuiCol_Text, palette().text_dim);
            ImGui::TextUnformatted(masked.c_str());
            ImGui::PopStyleColor();
            if (fonts().mono) ImGui::PopFont();
            ImGui::SameLine(std::min(220.0f, fw * 0.45f));
            ui_::Caption("%s", fmt_time(e.changed_at).c_str());
        }
    }

    ImGui::Dummy({0, 12});
}

void App::render_status_bar() {
    const Palette& p = palette();
    ImGui::Dummy({0, 2});
    if (status_t_ > 0) {
        ui_::Pill(status_.c_str(), status_err_ ? p.danger : p.success);
    } else if (db_ && db_->is_dirty()) {
        ui_::Pill("Unsaved changes", p.warning);
    } else {
        ui_::Pill("All changes saved", p.text_muted);
    }
}

// ── PASSWORD GENERATOR ───────────────────────────────────────────────────────

void App::render_gen_popup() {
    if (gen_open_) { ImGui::OpenPopup("##gen"); gen_open_ = false; }
    if (gen_regen_) { regenerate_password(); gen_regen_ = false; }

    ImGui::SetNextWindowSize({440, 0}, ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));
    if (ImGui::BeginPopupModal("##gen", nullptr,
                               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
        const Palette& p = palette();
        ui_::Heading("Generate Password");
        ui_::Caption("Options come from Settings > Preferences");
        ImGui::Dummy({0, 12});

        // Preview well
        float w = ImGui::GetContentRegionAvail().x;
        ImVec2 pos = ImGui::GetCursorScreenPos();
        float box_h = ImGui::GetTextLineHeight() * 2.2f;
        ImGui::GetWindowDrawList()->AddRectFilled(
            pos, {pos.x + w, pos.y + box_h}, ImGui::GetColorU32(p.surface_alt), 8.0f);
        ImGui::GetWindowDrawList()->AddRect(
            pos, {pos.x + w, pos.y + box_h}, ImGui::GetColorU32(p.border), 8.0f);

        if (fonts().mono) ImGui::PushFont(fonts().mono);
        ImVec2 tsz = ImGui::CalcTextSize(gen_prev_, nullptr, false, w - 24);
        ImGui::SetCursorScreenPos({pos.x + 12, pos.y + (box_h - tsz.y) * 0.5f});
        ImGui::PushStyleColor(ImGuiCol_Text, p.text);
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + w - 24);
        ImGui::TextUnformatted(gen_prev_);
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        if (fonts().mono) ImGui::PopFont();

        ImGui::SetCursorScreenPos(pos);
        ImGui::Dummy({w, box_h});

        ImGui::Dummy({0, 6});
        int s = pw_strength(gen_prev_);
        ui_::StrengthMeter(strength_frac(s), strength_color(s), strength_label(s), w * 0.5f);

        ImGui::Dummy({0, 14});
        float sp = ImGui::GetStyle().ItemSpacing.x;
        float third = (w - sp * 2) / 3.0f;
        if (ui_::SecondaryButton("Regenerate", {third, 38})) regenerate_password();
        ImGui::SameLine(0, sp);
        if (ui_::SecondaryButton("Cancel", {third, 38})) ImGui::CloseCurrentPopup();
        ImGui::SameLine(0, sp);
        if (ui_::PrimaryButton("Use Password", {third, 38})) {
            if (gen_prev_[0] && !sel_id_.empty()) {
                ef_pass_ = gen_prev_;
                if (Account* acc = db_->get_account(sel_id_)) {
                    acc->set_password(SecureString(ef_pass_.c_str(), ef_pass_.size()));
                    db_->mark_dirty();
                }
                set_status("Password applied.");
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}

void App::regenerate_password() {
    std::string pw = gen_phrase_
        ? PasswordGenerator::generate_passphrase(gen_words_, gen_sep_)
        : PasswordGenerator::generate(gen_len_, gen_lc_, gen_uc_, gen_dig_, gen_sym_);
    safe_copy(gen_prev_, sizeof(gen_prev_), pw);
}

// ── CHANGE MASTER PASSWORD ───────────────────────────────────────────────────

void App::render_chgpw_popup() {
    if (chg_open_) { ImGui::OpenPopup("##chgpw"); chg_open_ = false; chg_err_.clear(); }

    ImGui::SetNextWindowSize({420, 0}, ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));
    if (ImGui::BeginPopupModal("##chgpw", nullptr,
                               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
        const Palette& p = palette();
        ui_::Heading("Change Master Password");
        ui_::Caption("The vault is re-encrypted and saved immediately.");
        ImGui::Dummy({0, 12});

        float w  = ImGui::GetContentRegionAvail().x;
        float sp = ImGui::GetStyle().ItemSpacing.x;
        float show_w = 66.0f;
        ImGuiInputTextFlags pf = chg_show_ ? 0 : ImGuiInputTextFlags_Password;

        field("NEW PASSWORD", "##cp1", chg_pw_, sizeof(chg_pw_), w - show_w - sp, pf);
        ImGui::SameLine(0, sp);
        if (ui_::SecondaryButton(chg_show_ ? "Hide" : "Show", {show_w, 0})) chg_show_ = !chg_show_;

        field("CONFIRM PASSWORD", "##cp2", chg_con_, sizeof(chg_con_), w - show_w - sp, pf);

        ImGui::Dummy({0, 8});
        int s = pw_strength(chg_pw_);
        ui_::StrengthMeter(chg_pw_[0] ? strength_frac(s) : 0.0f, strength_color(s),
                           chg_pw_[0] ? strength_label(s) : "", w * 0.5f);

        if (!chg_err_.empty()) {
            ImGui::Dummy({0, 8});
            ui_::Banner(chg_err_.c_str(), p.danger);
        }

        ImGui::Dummy({0, 14});
        bool match = std::strcmp(chg_pw_, chg_con_) == 0;
        bool ok = chg_pw_[0] && match;
        float half = (w - sp) * 0.5f;
        if (ui_::SecondaryButton("Cancel", {half, 38})) {
            SecureMemory::secure_zero(chg_pw_,  sizeof(chg_pw_));
            SecureMemory::secure_zero(chg_con_, sizeof(chg_con_));
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0, sp);
        if (!ok) ImGui::BeginDisabled();
        if (ui_::PrimaryButton("Change Password", {half, 38})) {
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
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}

// ── PREFERENCES ──────────────────────────────────────────────────────────────

void App::render_prefs_popup() {
    if (prefs_open_) { ImGui::OpenPopup("##prefs"); prefs_open_ = false; }

    ImGui::SetNextWindowSize({480, 0}, ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));
    if (ImGui::BeginPopupModal("##prefs", nullptr,
                               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
        ui_::Heading("Preferences");
        ImGui::Dummy({0, 10});

        const float w     = ImGui::GetContentRegionAvail().x;
        const float sp    = ImGui::GetStyle().ItemSpacing.x;
        const float ctl_w = 200.0f;
        const float lbl_x = 150.0f;

        // ── Appearance ──
        ui_::SectionHeader("Appearance");

        ui_::Dimmed("Theme");
        ImGui::SameLine(lbl_x);
        {
            // Two-cell segmented control; the active side is the accent fill.
            float half = (ctl_w - sp) * 0.5f;
            bool is_dark = prefs_theme_ == Theme::Dark;
            bool pick_dark  = is_dark  ? ui_::PrimaryButton("Dark",  {half, 0})
                                       : ui_::SecondaryButton("Dark",  {half, 0});
            ImGui::SameLine(0, sp);
            bool pick_light = !is_dark ? ui_::PrimaryButton("Light", {half, 0})
                                       : ui_::SecondaryButton("Light", {half, 0});
            Theme next = prefs_theme_;
            if (pick_dark)  next = Theme::Dark;
            if (pick_light) next = Theme::Light;
            if (next != prefs_theme_) {
                prefs_theme_ = next;
                apply_theme(prefs_theme_);
                Config::instance().set_theme(theme_to_string(prefs_theme_));
            }
        }

        ImGui::Dummy({0, 4});

        const auto& font_list = available_fonts();
        int font_idx = 0;
        for (size_t i = 0; i < font_list.size(); ++i)
            if (font_list[i].id == font_id_) { font_idx = (int)i; break; }
        std::vector<const char*> font_labels;
        font_labels.reserve(font_list.size());
        for (const auto& f : font_list) font_labels.push_back(f.label.c_str());

        ui_::Dimmed("Font");
        ImGui::SameLine(lbl_x);
        ImGui::SetNextItemWidth(ctl_w);
        if (ImGui::Combo("##fontfamily", &font_idx, font_labels.data(), (int)font_labels.size())) {
            font_id_ = font_list[font_idx].id;
            Config::instance().set_font_id(font_id_);
            font_dirty_ = true;
        }

        ui_::Dimmed("Size");
        ImGui::SameLine(lbl_x);
        ImGui::SetNextItemWidth(ctl_w);
        // Committed on release so the atlas is not rebuilt every drag frame.
        ImGui::SliderInt("##fontsize", &font_size_, 12, 22, "%d px");
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            Config::instance().set_font_size(font_size_);
            font_dirty_ = true;
        }

        ui_::Divider(10);

        // ── Security ──
        ui_::SectionHeader("Security");

        ui_::Dimmed("Auto-lock");
        ImGui::SameLine(lbl_x);
        ImGui::SetNextItemWidth(ctl_w);
        ImGui::SliderInt("##autolock", &autolock_min_, 0, 60,
                         autolock_min_ == 0 ? "Never" : "%d min");
        if (ImGui::IsItemDeactivatedAfterEdit())
            Config::instance().set_autolock_minutes(autolock_min_);

        ui_::Dimmed("Clear clipboard");
        ImGui::SameLine(lbl_x);
        ImGui::SetNextItemWidth(ctl_w);
        ImGui::SliderInt("##clipclear", &clip_clear_s_, 0, 300,
                         clip_clear_s_ == 0 ? "Never" : "%d s");
        if (ImGui::IsItemDeactivatedAfterEdit())
            Config::instance().set_clipboard_clear_seconds(clip_clear_s_);

        ui_::Caption("Auto-lock relocks the vault after inactivity (Ctrl+L locks now).");

        ui_::Divider(10);

        // ── Password generation ──
        ui_::SectionHeader("Default Password Generation");

        bool changed = false;
        changed |= ImGui::Checkbox("Lowercase", &gen_lc_);  ImGui::SameLine(0, 14);
        changed |= ImGui::Checkbox("Uppercase", &gen_uc_);  ImGui::SameLine(0, 14);
        changed |= ImGui::Checkbox("Digits",    &gen_dig_); ImGui::SameLine(0, 14);
        changed |= ImGui::Checkbox("Symbols",   &gen_sym_);

        ImGui::Dummy({0, 4});
        ui_::Dimmed("Length");
        ImGui::SameLine(lbl_x);
        ImGui::SetNextItemWidth(ctl_w);
        changed |= ImGui::SliderInt("##genlen", &gen_len_, 6, 64, "%d chars");

        ImGui::Dummy({0, 4});
        changed |= ImGui::Checkbox("Passphrase mode", &gen_phrase_);
        if (gen_phrase_) {
            ui_::Dimmed("Words");
            ImGui::SameLine(lbl_x);
            ImGui::SetNextItemWidth(ctl_w);
            changed |= ImGui::SliderInt("##genwords", &gen_words_, 2, 8);

            ui_::Dimmed("Separator");
            ImGui::SameLine(lbl_x);
            ImGui::SetNextItemWidth(70);
            changed |= ImGui::InputText("##gensep", gen_sep_, sizeof(gen_sep_));
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

        ImGui::Dummy({0, 16});
        if (ui_::PrimaryButton("Done", {w, 38})) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}

// ── CONFIRMATIONS ────────────────────────────────────────────────────────────

void App::render_delete_confirm_popup() {
    if (del_confirm_open_) { ImGui::OpenPopup("##delconfirm"); del_confirm_open_ = false; }

    ImGui::SetNextWindowSize({400, 0}, ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));
    if (ImGui::BeginPopupModal("##delconfirm", nullptr,
                               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
        const Account* acc = db_ ? db_->get_account(sel_id_) : nullptr;
        std::string name = (acc && !acc->name().empty()) ? acc->name() : "(unnamed)";

        ui_::Heading("Delete Account");
        ImGui::Dummy({0, 8});
        ui_::Dimmed("Delete \"%s\"? This cannot be undone.", name.c_str());
        ImGui::Dummy({0, 16});

        float w = ImGui::GetContentRegionAvail().x;
        float sp = ImGui::GetStyle().ItemSpacing.x;
        float half = (w - sp) * 0.5f;
        if (ui_::SecondaryButton("Cancel", {half, 38})) ImGui::CloseCurrentPopup();
        ImGui::SameLine(0, sp);
        if (ui_::DangerButton("Delete", {half, 38})) {
            do_delete_selected();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}

void App::render_quit_confirm_popup() {
    if (quit_confirm_open_) { ImGui::OpenPopup("##quitconfirm"); quit_confirm_open_ = false; }

    ImGui::SetNextWindowSize({420, 0}, ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));
    if (ImGui::BeginPopupModal("##quitconfirm", nullptr,
                               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
        ui_::Heading("Unsaved Changes");
        ImGui::Dummy({0, 8});
        ui_::Dimmed("You have unsaved changes. What would you like to do?");
        ImGui::Dummy({0, 16});

        float w  = ImGui::GetContentRegionAvail().x;
        float sp = ImGui::GetStyle().ItemSpacing.x;
        float third = (w - sp * 2) / 3.0f;

        if (ui_::SecondaryButton("Cancel", {third, 38})) ImGui::CloseCurrentPopup();
        ImGui::SameLine(0, sp);
        if (ui_::DangerButton("Discard", {third, 38})) {
            wants_quit_ = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0, sp);
        if (ui_::PrimaryButton("Save & Exit", {third, 38})) {
            do_save();
            wants_quit_ = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}

void App::render_about_popup() {
    if (about_open_) { ImGui::OpenPopup("##about"); about_open_ = false; }

    ImGui::SetNextWindowSize({380, 0}, ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));
    if (ImGui::BeginPopupModal("##about", nullptr,
                               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
        ui_::Title("Pasgen");
        ui_::Caption("Secure cross-platform password manager");
        ImGui::Dummy({0, 12});
        ui_::Dimmed("Vaults are encrypted with AES-256-GCM.");
        ui_::Dimmed("Keys are derived with Argon2id.");
        ImGui::Dummy({0, 16});
        if (ui_::PrimaryButton("Close", {ImGui::GetContentRegionAvail().x, 38}))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}

// ── CATEGORY POPUPS ──────────────────────────────────────────────────────────

void App::render_category_add_popup() {
    if (cat_add_open_) { ImGui::OpenPopup("##catadd"); cat_add_open_ = false; }

    ImGui::SetNextWindowSize({380, 0}, ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));
    if (ImGui::BeginPopupModal("##catadd", nullptr,
                               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
        ui_::Heading("New Category");
        ImGui::Dummy({0, 10});

        float w = ImGui::GetContentRegionAvail().x;
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        bool enter = field("NAME", "##catname", cat_add_name_, sizeof(cat_add_name_), w,
                           ImGuiInputTextFlags_EnterReturnsTrue, "e.g. Banking");

        ImGui::Dummy({0, 16});
        float sp = ImGui::GetStyle().ItemSpacing.x;
        float half = (w - sp) * 0.5f;
        bool ok = cat_add_name_[0] != '\0';
        if (ui_::SecondaryButton("Cancel", {half, 38})) ImGui::CloseCurrentPopup();
        ImGui::SameLine(0, sp);
        if (!ok) ImGui::BeginDisabled();
        if (ui_::PrimaryButton("Create", {half, 38}) || (enter && ok)) {
            db_->add_category(cat_add_name_);
            ImGui::CloseCurrentPopup();
        }
        if (!ok) ImGui::EndDisabled();
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}

void App::render_category_rename_popup() {
    if (cat_rename_open_) { ImGui::OpenPopup("##catrename"); cat_rename_open_ = false; }

    ImGui::SetNextWindowSize({380, 0}, ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));
    if (ImGui::BeginPopupModal("##catrename", nullptr,
                               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
        ui_::Heading("Rename Category");
        ImGui::Dummy({0, 10});

        float w = ImGui::GetContentRegionAvail().x;
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        bool enter = field("NAME", "##catrenamename", cat_rename_name_,
                           sizeof(cat_rename_name_), w, ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::Dummy({0, 16});
        float sp = ImGui::GetStyle().ItemSpacing.x;
        float half = (w - sp) * 0.5f;
        bool ok = cat_rename_name_[0] != '\0';
        if (ui_::SecondaryButton("Cancel", {half, 38})) ImGui::CloseCurrentPopup();
        ImGui::SameLine(0, sp);
        if (!ok) ImGui::BeginDisabled();
        if (ui_::PrimaryButton("Save", {half, 38}) || (enter && ok)) {
            db_->rename_category(cat_rename_id_, cat_rename_name_);
            ImGui::CloseCurrentPopup();
        }
        if (!ok) ImGui::EndDisabled();
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}

void App::render_category_delete_confirm_popup() {
    if (cat_delete_confirm_open_) { ImGui::OpenPopup("##catdelconfirm"); cat_delete_confirm_open_ = false; }

    ImGui::SetNextWindowSize({400, 0}, ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));
    if (ImGui::BeginPopupModal("##catdelconfirm", nullptr,
                               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
        ui_::Heading("Delete Category");
        ImGui::Dummy({0, 8});
        ui_::Dimmed("Accounts in this category move to Uncategorized. No passwords are deleted.");
        ImGui::Dummy({0, 16});

        float w = ImGui::GetContentRegionAvail().x;
        float sp = ImGui::GetStyle().ItemSpacing.x;
        float half = (w - sp) * 0.5f;
        if (ui_::SecondaryButton("Cancel", {half, 38})) ImGui::CloseCurrentPopup();
        ImGui::SameLine(0, sp);
        if (ui_::DangerButton("Delete", {half, 38})) {
            db_->remove_category(cat_delete_id_);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}

// ── ACTIONS ──────────────────────────────────────────────────────────────────

void App::do_save() {
    if (!db_) return;
    try {
        db_->save();
        set_status("Saved.");
    } catch (const std::exception& e) {
        set_status(std::string("Save failed: ") + e.what(), true);
    }
}

void App::do_save_as() {
    if (!db_) return;
    char path[512] = {};
    safe_copy(path, sizeof(path), db_->file_path());
    do_browse_save(path, sizeof(path));
    if (!path[0]) return;

    std::string p = path;
    if (p.size() < 4 || p.substr(p.size() - 4) != ".pif") p += ".pif";

    try {
        db_->save_as(p);
        safe_copy(lp_, sizeof(lp_), p);
        Config::instance().set_last_database_path(p);
        set_status("Saved as " + p);
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

    auto all = db_->get_all_accounts();
    if (!all.empty()) do_select_account(all[0]->id());
}

void App::do_select_account(const std::string& id) {
    if (id == sel_id_) return;
    sel_id_ = id;
    clear_editor();
    if (const Account* acc = db_->get_account(id)) load_account_to_editor(*acc);
}

void App::sync_custom_fields(const Account& acc) {
    for (auto& f : ef_custom_) { secure_clear_string(f.name); secure_clear_string(f.value); }
    ef_custom_.clear();
    for (const auto& f : acc.custom_fields())
        ef_custom_.push_back({f.name, std::string(f.value.c_str(), f.value.size()), f.secret});
}

void App::load_account_to_editor(const Account& acc) {
    sync_custom_fields(acc);
    ef_name_  = acc.name();
    ef_email_ = acc.email();
    ef_user_  = acc.username();
    ef_url_   = acc.url();
    ef_totp_  = acc.totp_secret();
    ef_notes_ = acc.notes();

    const auto& pw = acc.password();
    ef_pass_.assign(pw.c_str(), pw.size());
    ef_showp_ = false;
}

void App::clear_editor() {
    // Every editor field is wiped, not just the password: the TOTP secret is
    // password-equivalent, and notes routinely hold recovery codes.
    secure_clear_string(ef_pass_);
    secure_clear_string(ef_name_);
    secure_clear_string(ef_email_);
    secure_clear_string(ef_user_);
    secure_clear_string(ef_url_);
    secure_clear_string(ef_totp_);
    secure_clear_string(ef_notes_);
    for (auto& f : ef_custom_) { secure_clear_string(f.name); secure_clear_string(f.value); }
    ef_custom_.clear();
    secure_clear_string(cf_new_name_);
    secure_clear_string(cf_new_value_);
    cf_new_secret_ = false;
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
        std::string r = pfd::save_file("Save Database", buf[0] ? buf : "vault.pif",
                                       {"PIF Database (*.pif)", "*.pif"}).result();
        if (!r.empty()) std::strncpy(buf, r.c_str(), n - 1);
    } catch (...) {}
}

void App::clipboard(const char* text) {
    SDL_SetClipboardText(text);
    if (clip_clear_s_ > 0) {
        clip_copy_  = text ? text : "";
        clip_left_  = (float)clip_clear_s_;
        set_status("Copied - clipboard clears in " + std::to_string(clip_clear_s_) + "s.");
    } else {
        set_status("Copied to clipboard.");
    }
}

// Relocks the vault: the Database destructor wipes the master password and
// every decrypted account, and the editor buffers are zeroed separately.
void App::do_lock(const char* reason) {
    if (!db_) return;
    // Persist first -- losing edits to an idle timer would be worse than the
    // brief extra write, and the file is encrypted either way.
    if (db_->is_dirty()) { try { db_->save(); } catch (...) {} }

    const std::string path = db_->file_path();
    clear_editor();
    sel_id_.clear();
    db_.reset();
    screen_ = Screen::LOGIN;
    safe_copy(lp_, sizeof(lp_), path);
    SecureMemory::secure_zero(lpw_, sizeof(lpw_));
    lerr_.clear();
    set_status(reason);
}

void App::tick_security(float dt) {
    // Clipboard: only wipe if the contents are still ours, so a later copy by
    // the user (or another app) is never clobbered.
    if (clip_left_ > 0.0f) {
        clip_left_ -= dt;
        if (clip_left_ <= 0.0f) {
            char* cur = SDL_GetClipboardText();
            if (cur && clip_copy_ == cur) {
                SDL_SetClipboardText("");
                set_status("Clipboard cleared.");
            }
            if (cur) SDL_free(cur);
            secure_clear_string(clip_copy_);
            clip_left_ = 0.0f;
        }
    }

    if (screen_ != Screen::MAIN || !db_ || autolock_min_ <= 0) { idle_s_ = 0.0f; return; }

    ImGuiIO& io = ImGui::GetIO();
    bool active = io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f ||
                  io.MouseWheel != 0.0f || io.MouseWheelH != 0.0f ||
                  io.InputQueueCharacters.Size > 0;
    for (int b = 0; !active && b < IM_ARRAYSIZE(io.MouseDown); ++b)
        if (io.MouseDown[b]) active = true;
    for (int k = ImGuiKey_NamedKey_BEGIN; !active && k < ImGuiKey_NamedKey_END; ++k)
        if (ImGui::IsKeyDown((ImGuiKey)k)) active = true;

    idle_s_ = active ? 0.0f : idle_s_ + dt;
    if (idle_s_ >= (float)autolock_min_ * 60.0f) {
        idle_s_ = 0.0f;
        do_lock("Locked after inactivity.");
    }
}

void App::set_status(const std::string& msg, bool err) {
    status_     = msg;
    status_err_ = err;
    status_t_   = 4.0f;
}

// ── HELPERS ──────────────────────────────────────────────────────────────────

int App::pw_strength(const char* pw) const {
    if (!pw || !pw[0]) return 0;
    std::string s(pw);
    bool lc = false, uc = false, dig = false, sym = false;
    for (char c : s) {
        if      (std::islower((unsigned char)c)) lc  = true;
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
    static const char* L[] = {"Very Weak", "Weak", "Fair", "Good", "Strong"};
    return L[std::min(std::max(s, 0), 4)];
}

ImVec4 App::strength_color(int s) const {
    const Palette& p = palette();
    if (s <= 1) return p.danger;
    if (s == 2) return p.warning;
    return p.success;
}

} // namespace pasgen
