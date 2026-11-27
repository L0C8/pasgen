// pasgen-check: confirm a vault survives the move to the current build.
//
//   pasgen-check vault.pif
//
// Opens the vault (asks for the master password without echoing it), prints
// what it found, then saves a copy to the temp directory, reopens that copy
// and compares every account field by field. The original file is only
// read, never written. Exit code 0 means the round trip was lossless.

#include "core/Database.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

using namespace pasgen;

namespace {

SecureString read_password(const char* prompt) {
    std::fprintf(stderr, "%s", prompt);
    std::fflush(stderr);
#ifdef _WIN32
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    const bool console = GetConsoleMode(in, &mode);
    if (console) SetConsoleMode(in, mode & ~ENABLE_ECHO_INPUT);
#else
    termios old{};
    const bool tty = tcgetattr(STDIN_FILENO, &old) == 0;
    if (tty) {
        termios quiet = old;
        quiet.c_lflag &= ~(tcflag_t)ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &quiet);
    }
#endif
    std::string line;
    std::getline(std::cin, line);
#ifdef _WIN32
    if (console) SetConsoleMode(in, mode);
#else
    if (tty) tcsetattr(STDIN_FILENO, TCSANOW, &old);
#endif
    std::fprintf(stderr, "\n");
    if (!line.empty() && line.back() == '\r') line.pop_back();
    SecureString pw(line);
    std::fill(line.begin(), line.end(), '\0');
    return pw;
}

long long secs(std::chrono::system_clock::time_point tp) {
    return std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count();
}

int g_diffs = 0;

void diff(const std::string& account, const char* field) {
    std::printf("  DIFF  %s: %s\n", account.c_str(), field);
    ++g_diffs;
}

void compare(const Account& a, const Account& b) {
    const std::string who = a.name().empty() ? a.id() : a.name();
    if (a.name() != b.name()) diff(who, "name");
    if (a.username() != b.username()) diff(who, "username");
    if (a.email() != b.email()) diff(who, "email");
    if (a.url() != b.url()) diff(who, "url");
    if (a.password() != b.password()) diff(who, "password");
    if (a.notes() != b.notes()) diff(who, "notes");
    if (a.totp_secret() != b.totp_secret()) diff(who, "totp secret");
    if (a.category_id() != b.category_id()) diff(who, "category");
    if (a.order() != b.order()) diff(who, "order");
    if (a.favorite() != b.favorite()) diff(who, "favorite");
    if (a.archived() != b.archived()) diff(who, "archived");
    if (secs(a.created_at()) != secs(b.created_at())) diff(who, "created time");
    if (secs(a.modified_at()) != secs(b.modified_at())) diff(who, "modified time");
    if (secs(a.password_changed_at()) != secs(b.password_changed_at())) diff(who, "password changed time");

    const auto& ha = a.password_history();
    const auto& hb = b.password_history();
    if (ha.size() != hb.size()) {
        diff(who, "password history length");
    } else {
        for (size_t i = 0; i < ha.size(); ++i)
            if (ha[i].password != hb[i].password || secs(ha[i].changed_at) != secs(hb[i].changed_at))
                diff(who, "password history entry");
    }

    const auto& fa = a.custom_fields();
    const auto& fb = b.custom_fields();
    if (fa.size() != fb.size()) {
        diff(who, "custom field count");
    } else {
        for (size_t i = 0; i < fa.size(); ++i)
            if (fa[i].name != fb[i].name || fa[i].value != fb[i].value || fa[i].secret != fb[i].secret)
                diff(who, "custom field");
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2 || std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help") {
        std::fprintf(stderr,
                     "usage: pasgen-check VAULT.pif\n\n"
                     "Opens the vault read-only, saves a copy to a temp folder, reopens it and\n"
                     "compares every account. The original file is never written.\n");
        return 2;
    }
    const std::string path = argv[1];
    const SecureString pw = read_password("Master password: ");

    std::unique_ptr<Database> db;
    try {
        db = Database::open(path, pw);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Could not open %s: %s\n", path.c_str(), e.what());
        return 1;
    }

    std::printf("Opened   %s\n", path.c_str());
    std::printf("Vault    %s\n", db->metadata().name.empty() ? "(unnamed)" : db->metadata().name.c_str());
    std::printf("Cipher   %s\n", db->current_cipher_name().c_str());
    std::printf("KDF      %s\n", db->current_kdf_description().c_str());

    const auto all = std::as_const(*db).get_all_accounts();
    size_t history = 0, fields = 0, totp = 0, archived = 0;
    for (const Account* a : all) {
        history += a->password_history().size();
        fields += a->custom_fields().size();
        totp += a->totp_secret().empty() ? 0 : 1;
        archived += a->archived() ? 1 : 0;
    }
    std::printf("Accounts %zu (%zu archived), %zu with TOTP, %zu custom fields, %zu old passwords\n",
                all.size(), archived, totp, fields, history);
    std::map<std::string, size_t> per_cat;
    for (const Account* a : all) ++per_cat[a->category_id()];
    for (const Category& c : db->categories())
        std::printf("  %-28s %zu\n", c.name().c_str(), per_cat[c.id()]);
    std::printf("  %-28s %zu\n", "Uncategorized", per_cat[""]);

    // Round trip through a temporary copy. save_as() points the in-memory
    // database at the copy, so the original path is never written.
    const std::filesystem::path tmp =
        std::filesystem::temp_directory_path() /
        ("pasgen-check-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".pif");
    std::printf("\nRound trip via %s\n", tmp.string().c_str());
    std::unique_ptr<Database> back;
    try {
        db->save_as(tmp.string());
        back = Database::open(tmp.string(), pw);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Round trip failed: %s\n", e.what());
        std::error_code ec;
        std::filesystem::remove(tmp, ec);
        return 1;
    }
    std::error_code ec;
    std::filesystem::remove(tmp, ec);

    if (back->account_count() != db->account_count()) {
        std::printf("  DIFF  account count %zu -> %zu\n", db->account_count(), back->account_count());
        ++g_diffs;
    }
    if (back->categories().size() != db->categories().size()) {
        std::printf("  DIFF  category count\n");
        ++g_diffs;
    } else {
        for (size_t i = 0; i < db->categories().size(); ++i) {
            const Category& x = db->categories()[i];
            const Category& y = back->categories()[i];
            if (x.id() != y.id() || x.name() != y.name() || x.color_index() != y.color_index()) {
                std::printf("  DIFF  category %s\n", x.name().c_str());
                ++g_diffs;
            }
        }
    }
    if (back->metadata().name != db->metadata().name) {
        std::printf("  DIFF  vault name\n");
        ++g_diffs;
    }
    for (const Account* a : all) {
        const Account* b = std::as_const(*back).get_account(a->id());
        if (!b) {
            std::printf("  DIFF  %s: missing after round trip\n", a->name().c_str());
            ++g_diffs;
            continue;
        }
        compare(*a, *b);
    }

    if (g_diffs == 0) {
        std::printf("OK       every account, category and field came back identical.\n");
        std::printf("         %s was not modified.\n", path.c_str());
        return 0;
    }
    std::printf("FAILED   %d difference(s). %s was not modified.\n", g_diffs, path.c_str());
    return 1;
}
