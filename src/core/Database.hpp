#pragma once

#include "core/Account.hpp"
#include "crypto/CryptoEngine.hpp"
#include "crypto/KeyDerivation.hpp"
#include "crypto/SecureMemory.hpp"
#include <chrono>
#include <cstdint>
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
    static constexpr uint16_t VERSION = 0x0001;

    Database();
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&&) = default;
    Database& operator=(Database&&) = default;

    static std::unique_ptr<Database> open(const std::string& path,
                                          const SecureString& master_password);
    static std::unique_ptr<Database> create(const std::string& path,
                                            const SecureString& master_password);
    void save();
    void save_as(const std::string& path);
    void change_master_password(const SecureString& new_password);

    void add_account(Account account);
    void update_account(const Account& account);
    void remove_account(const std::string& id);
    Account*       get_account(const std::string& id);
    const Account* get_account(const std::string& id) const;
    std::vector<Account*>       get_all_accounts();
    std::vector<const Account*> get_all_accounts() const;
    std::vector<const Account*> search_accounts(const std::string& query) const;
    size_t account_count() const { return accounts_.size(); }

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
    bool is_dirty_ = false;
    std::vector<uint8_t> salt_;
    CryptoEngine crypto_;

    void load_from_file(const std::string& path);
    void write_to_file(const std::string& path);
    nlohmann::json to_json() const;
    void from_json(const nlohmann::json& j);
};

} // namespace pasgen
