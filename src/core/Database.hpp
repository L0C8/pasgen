#pragma once

#include "core/Account.hpp"
#include "core/Category.hpp"
#include "crypto/CryptoEngine.hpp"
#include "crypto/KeyDerivation.hpp"
#include "crypto/SecureMemory.hpp"
#include <chrono>
#include <cstdint>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace pasgen {

class DatabaseException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct DatabaseMetadata {
    std::string version = "1.0";
    std::string name;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point modified_at;
    Argon2Params argon2_params;

    nlohmann::json to_json() const;
    static DatabaseMetadata from_json(const nlohmann::json& j);
};

class Database {
public:
    static constexpr uint32_t MAGIC   = 0x00464950;
    // v2: authenticated header (AAD is the verbatim header block),
    // explicit KDF id, and reserved flags for future cascade/keyfile use.
    static constexpr uint16_t VERSION = 0x0003;
    // Format written by builds prior to this one (up through "revised
    // installer"): fixed 68-byte prefix, AAD reconstructed field-by-field
    // rather than taken verbatim, single-layer AES-256-GCM only, and no
    // KDF id (Argon2id was the only option). Still readable so those vaults
    // open without a manual migration step; every save rewrites the file in
    // the current VERSION/CIPHER_CASCADE format.
    static constexpr uint16_t VERSION_LEGACY_V1 = 0x0001;

    // Which cipher construction protects the payload.
    static constexpr uint16_t CIPHER_AES_GCM  = 1; // single layer (pre-v3)
    static constexpr uint16_t CIPHER_CASCADE  = 2; // ChaCha20-Poly1305 then AES-256-GCM

    // Named bundles of (cipher, KDF cost) offered when creating a database or
    // changing an existing one's encryption. Deliberately just presets over
    // the same primitives above rather than a fourth crypto option, so this
    // is UI sugar, not new attack surface.
    enum class EncryptionLevel {
        Standard,       // cascade cipher, Argon2id calibrated to ~750ms on this machine (previous default)
        HighSecurity,   // cascade cipher, KDFParams::high_security() — much slower to open, higher brute-force cost
        FastCompatible, // single-layer AES-256-GCM, KDFParams::low_memory() — for slower/low-memory machines
    };
    static const char* encryption_level_name(EncryptionLevel level);
    static const char* encryption_level_description(EncryptionLevel level);

    Database();
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&&) = default;
    Database& operator=(Database&&) = default;

    static std::unique_ptr<Database> open(const std::string& path,
                                          const SecureString& master_password);
    static std::unique_ptr<Database> create(const std::string& path,
                                            const SecureString& master_password,
                                            EncryptionLevel level = EncryptionLevel::Standard);
    void save();
    void save_as(const std::string& path);
    void change_master_password(const SecureString& new_password);
    // Re-encrypts under a different cipher/KDF-cost bundle. Keeps the current
    // salt (changing KDF cost alone doesn't call for a new one) but the
    // cached derived key is invalidated, since it depended on the old cost
    // parameters. Only takes effect once save() is called, same as every
    // other mutator here.
    void change_encryption_level(EncryptionLevel level);
    // What the currently loaded/created database is actually using right
    // now — not necessarily one of the three presets above, since a file can
    // carry hand-set or legacy parameters that don't match any bundle.
    std::string current_cipher_name() const;
    std::string current_kdf_description() const { return metadata_.argon2_params.describe(); }

    void add_account(Account account);
    void update_account(const Account& account);
    void remove_account(const std::string& id);
    Account*       get_account(const std::string& id);
    const Account* get_account(const std::string& id) const;
    std::vector<Account*>       get_all_accounts();
    std::vector<const Account*> get_all_accounts() const;
    std::vector<const Account*> search_accounts(const std::string& query) const;
    size_t account_count() const { return accounts_.size(); }

    // Categories: accounts reference a category by id (empty id = "Uncategorized",
    // which is not itself a stored Category). Account order is a dense,
    // gap-free per-category rank maintained by move_account()/reorder_category().
    const std::vector<Category>& categories() const { return categories_; }
    // Returns the new category's id by value: categories_ is a vector, so any
    // reference into it would dangle as soon as another category is added.
    std::string add_category(const std::string& name);
    void rename_category(const std::string& id, const std::string& name);
    void remove_category(const std::string& id);
    void reorder_category(const std::string& id, int new_index);
    std::vector<Account*> get_accounts_in_category(const std::string& category_id);
    void move_account(const std::string& account_id, const std::string& target_category_id, int target_index);

    bool is_dirty() const { return is_dirty_; }
    void mark_dirty()     { is_dirty_ = true; }
    const DatabaseMetadata& metadata() const { return metadata_; }
    void set_name(const std::string& name);
    std::string file_path() const { return file_path_; }

private:
    std::string file_path_;
    SecureString master_password_;
    DatabaseMetadata metadata_;
    std::map<std::string, Account> accounts_;
    std::vector<Category> categories_;
    bool is_dirty_ = false;
    std::vector<uint8_t> salt_;
    SecureBytes key_cache_;
    bool key_cached_ = false;
    CryptoEngine crypto_;
    // Which of CIPHER_AES_GCM / CIPHER_CASCADE the next save() writes.
    // Populated from the header on load, or from the chosen EncryptionLevel
    // on create()/change_encryption_level().
    uint16_t cipher_id_ = CIPHER_CASCADE;

    // Derives the master key on first use and caches it until the password or
    // salt changes; see the definition for why.
    const SecureBytes& session_key();

    void load_from_file(const std::string& path);
    void load_v3(std::ifstream& f, std::streamoff file_size);
    void load_legacy_v1(std::ifstream& f, std::streamoff file_size);
    void write_to_file(const std::string& path);
    nlohmann::json to_json() const;
    void from_json(const nlohmann::json& j);
    void normalize_category_order(const std::string& category_id);
    void apply_encryption_level(EncryptionLevel level);
};

} // namespace pasgen
