// Core checks that need no GUI: generator correctness, the wordlist, and a
// full create/save/reopen round trip of the .pif format, including the
// PBKDF2 path and tamper detection.
//
// Run: ctest --test-dir build   (or ./build/pasgen-tests)

#include "core/Database.hpp"
#include "core/ExportFile.hpp"
#include "util/PasswordGenerator.hpp"
#include "util/Wordlist.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>

#ifndef _WIN32
#include <sys/stat.h>
#endif

using namespace pasgen;

static int g_failed = 0;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++g_failed;                                                      \
        }                                                                    \
    } while (0)

template <typename F>
static bool throws(F&& f) {
    try { f(); } catch (const std::exception&) { return true; }
    return false;
}

static void test_wordlist() {
    std::set<std::string> seen;
    for (size_t i = 0; i < kEffLargeWordlistSize; ++i) seen.insert(kEffLargeWordlist[i]);
    CHECK(seen.size() == 7776);
    CHECK(std::string(kEffLargeWordlist[0]) == "abacus");
    CHECK(std::string(kEffLargeWordlist[7775]) == "zoom");
}

static void test_generator() {
    for (int i = 0; i < 200; ++i) {
        std::string pw = PasswordGenerator::generate(16, true, true, true, true);
        CHECK(pw.size() == 16);
        bool lc = false, uc = false, dg = false, sy = false;
        for (char c : pw) {
            if (c >= 'a' && c <= 'z') lc = true;
            else if (c >= 'A' && c <= 'Z') uc = true;
            else if (c >= '0' && c <= '9') dg = true;
            else sy = true;
        }
        CHECK(lc && uc && dg && sy);
    }
    CHECK(PasswordGenerator::generate(8, false, false, true, false).find_first_not_of("0123456789") ==
          std::string::npos);

    std::string phrase = PasswordGenerator::generate_passphrase(6, " ");
    int spaces = 0;
    for (char c : phrase) spaces += c == ' ';
    CHECK(spaces == 5);

    // 6 EFF words is ~77.5 bits; 16 chars over 88 symbols is ~103 bits.
    CHECK(std::fabs(PasswordGenerator::passphrase_entropy_bits(6) - 77.55) < 0.05);
    CHECK(std::fabs(PasswordGenerator::entropy_bits(16, true, true, true, true) - 103.35) < 0.05);

    // Index sampling covers the whole range, including above 255 (the old
    // byte-based sampler could not reach past 255).
    bool high = false;
    for (int i = 0; i < 2000 && !high; ++i) high = PasswordGenerator::secure_random_index(7776) > 6000;
    CHECK(high);
}

static void test_roundtrip(Database::EncryptionLevel level, const char* name) {
    const std::string path = (std::filesystem::temp_directory_path() / name).string();
    std::filesystem::remove(path);
    {
        auto db = Database::create(path, SecureString("correct horse battery staple"), level);
        Account a;
        a.set_name("Example");
        a.set_username("tim");
        a.set_password(SecureString("hunter2"));
        a.set_password(SecureString("hunter3"));
        a.add_custom_field("PIN", SecureString("1234"), true);
        std::string cat = db->add_category("Work");
        std::string id = a.id();
        db->add_account(std::move(a));
        db->move_account(id, cat, 0);
        db->save();
    }
#ifndef _WIN32
    struct stat st{};
    CHECK(stat(path.c_str(), &st) == 0 && (st.st_mode & 0777) == 0600);
#endif
    CHECK(!std::filesystem::exists(path + ".tmp"));

    CHECK(throws([&] { Database::open(path, SecureString("wrong")); }));

    auto db = Database::open(path, SecureString("correct horse battery staple"));
    auto all = db->get_all_accounts();
    CHECK(all.size() == 1);
    if (all.size() == 1) {
        CHECK(all[0]->name() == "Example");
        CHECK(std::string(all[0]->password().c_str()) == "hunter3");
        CHECK(all[0]->password_history().size() == 1);
        CHECK(all[0]->custom_fields().size() == 1);
        CHECK(!all[0]->category_id().empty());
    }

    // Flip one ciphertext byte: authentication must fail.
    {
        std::fstream f(path, std::ios::in | std::ios::out | std::ios::binary);
        f.seekg(100);
        char c = 0;
        f.read(&c, 1);
        c ^= 0x01;
        f.seekp(100);
        f.write(&c, 1);
    }
    CHECK(throws([&] { Database::open(path, SecureString("correct horse battery staple")); }));
    std::filesystem::remove(path);
}

static void test_pbkdf2_vault() {
    // CryptoEngine::derive_key must honour the KDF id from the header. Before
    // the fix it always ran Argon2id, so a PBKDF2 vault could never open.
    KDFParams kp;
    kp.kdf = KdfId::Pbkdf2Sha256;
    kp.pbkdf2_iterations = 10000;
    CryptoEngine ce;
    std::vector<uint8_t> salt(32, 7);
    SecureBytes k1 = ce.derive_key(SecureString("x"), salt, kp);
    SecureBytes k2 = KeyDerivation::derive_pbkdf2(SecureString("x"), salt, 32, 10000);
    CHECK(k1.size() == 32 && k2.size() == 32 &&
          SecureMemory::constant_time_compare(k1.data(), k2.data(), 32));
}

static void test_export() {
    const std::string path = (std::filesystem::temp_directory_path() / "pasgen_test.eif").string();
    ExportFile::Payload p;
    p.source_vault_name = "Test";
    Account a;
    a.set_name("Exported");
    p.accounts.push_back(a);
    ExportFile::write(path, SecureString("export-pw"), p);
    CHECK(throws([&] { ExportFile::read(path, SecureString("nope")); }));
    auto back = ExportFile::read(path, SecureString("export-pw"));
    CHECK(back.accounts.size() == 1 && back.accounts[0].name() == "Exported");
    std::filesystem::remove(path);
}

int main() {
    test_wordlist();
    test_generator();
    test_roundtrip(Database::EncryptionLevel::FastCompatible, "pasgen_fast.pif");
    test_roundtrip(Database::EncryptionLevel::Standard, "pasgen_std.pif");
    test_pbkdf2_vault();
    test_export();
    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::printf("all core tests passed\n");
    return 0;
}
