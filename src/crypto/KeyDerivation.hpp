#pragma once

#include "crypto/SecureMemory.hpp"
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <vector>
#include <nlohmann/json.hpp>

namespace pasgen {

class KeyDerivationException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct KDFParams {
    uint32_t memory_kb = 65536;
    uint32_t iterations = 3;
    uint32_t parallelism = 4;
    uint32_t pbkdf2_iterations = 600000;

    static KDFParams from_json(const nlohmann::json& j);
    nlohmann::json to_json() const;

    static KDFParams high_security() { return {131072, 4, 4, 1000000}; }
    static KDFParams low_memory()    { return {32768, 5, 2, 600000}; }
};

using Argon2Params = KDFParams;

class KeyDerivation {
public:
    static SecureBytes derive_key(
        const SecureString& password,
        const std::vector<uint8_t>& salt,
        size_t key_length,
        const KDFParams& params);

    static SecureBytes derive_argon2id(
        const SecureString& password,
        const std::vector<uint8_t>& salt,
        size_t key_length,
        const KDFParams& params) {
        return derive_key(password, salt, key_length, params);
    }

    static SecureBytes derive_pbkdf2(
        const SecureString& password,
        const std::vector<uint8_t>& salt,
        size_t key_length,
        uint32_t iterations);

    static bool argon2_available();
    static KDFParams benchmark(std::chrono::milliseconds target_time);
};

} // namespace pasgen
