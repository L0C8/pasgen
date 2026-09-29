#pragma once

#include "core/Account.hpp"
#include "core/Category.hpp"
#include "crypto/SecureMemory.hpp"
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace pasgen {

class ExportException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// A standalone, independently-encrypted file for moving accounts between
// vaults -- File > Export writes one from the open database (the whole
// vault, or a chosen subset of accounts); File > Import reads it into
// another. Protected by its own export password, unrelated to either
// vault's master password, so the file is safe to move, back up, or hand
// off on its own.
//
// Binary envelope is the same authenticated-header/cascade-cipher
// construction .pif uses (see core/Database.cpp) -- just under a distinct
// magic/version, so the two formats can never be mistaken for each other.
class ExportFile {
public:
    static constexpr uint32_t MAGIC   = 0x00464945; // "EIF\0", mirrors Database::MAGIC's "PIF\0"
    static constexpr uint16_t VERSION = 0x0001;

    struct Payload {
        std::string source_vault_name;
        std::chrono::system_clock::time_point exported_at;
        // Category ids here are meaningful only within this file: each
        // Account's category_id refers to one of these, not to any id in
        // either vault. The importer matches/creates categories by name.
        std::vector<Category> categories;
        std::vector<Account>  accounts;
    };

    static void write(const std::string& path,
                       const SecureString& export_password,
                       const Payload& payload);

    static Payload read(const std::string& path,
                         const SecureString& export_password);
};

} // namespace pasgen
