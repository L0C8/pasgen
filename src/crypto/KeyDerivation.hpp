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

// Persisted in the .pif header so a database derived with one KDF is never
// silently probed with another. Without this, a build lacking Argon2 would
// derive a different key from the same password and report only "wrong
// password", with no way to tell the two situations apart.
enum class KdfId : uint16_t {
    Argon2id     = 1,
    Pbkdf2Sha256 = 2,
};

const char* kdf_name(KdfId id);

struct KDFParams {
    // 256 MiB. Argon2's memory cost is what actually prices an attacker's
    // parallel hardware, so it carries most of the brute-force resistance.
    uint32_t memory_kb  = 262144;
    uint32_t iterations = 3;
    uint32_t parallelism = 4;
    uint32_t pbkdf2_iterations = 600000;
    KdfId    kdf = KdfId::Argon2id;

    // Bounds enforced when reading a header. These values come from an
    // untrusted file and are fed straight into the KDF, so an unclamped
    // memory_kb of 0xFFFFFFFF would ask the allocator for ~4 TiB before any
    // authentication has happened.
    static constexpr uint32_t MIN_MEMORY_KB   = 8 * 1024;        // 8 MiB
    static constexpr uint32_t MAX_MEMORY_KB   = 4 * 1024 * 1024; // 4 GiB
    static constexpr uint32_t MIN_ITERATIONS  = 1;
    static constexpr uint32_t MAX_ITERATIONS  = 64;
    static constexpr uint32_t MIN_PARALLELISM = 1;
    static constexpr uint32_t MAX_PARALLELISM = 64;
    // Auto-calibration stops here even on a very fast machine. A database is
    // only as openable as the weakest machine that needs to open it, so
    // calibration must not silently pick a cost that another device cannot
    // afford. Hand-set parameters may still go up to MAX_MEMORY_KB.
    static constexpr uint32_t CALIBRATION_MAX_MEMORY_KB = 524288; // 512 MiB

    // True when every parameter is within the bounds above. Callers reading a
    // file must reject rather than clamp: silently weakening the parameters
    // would let a tampered header downgrade the KDF.
    bool valid() const;
    std::string describe() const;

    static KDFParams from_json(const nlohmann::json& j);
    nlohmann::json to_json() const;

    static KDFParams high_security() { return {524288, 4, 4, 1000000, KdfId::Argon2id}; }
    static KDFParams low_memory()    { return {32768,  5, 2, 600000,  KdfId::Argon2id}; }
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
        const KDFParams& params);

    static SecureBytes derive_pbkdf2(
        const SecureString& password,
        const std::vector<uint8_t>& salt,
        size_t key_length,
        uint32_t iterations);

    // Which KDF this build can actually perform. Databases are stamped with
    // this, so it must reflect the compiled-in capability.
    static bool  argon2_available();
    static KdfId preferred_kdf();

    // Raises the cost until a derivation takes at least target_time on this
    // machine, so a faster CPU produces a proportionally stronger database.
    // Memory cost is raised before time cost, since memory is what hurts an
    // attacker's GPU/ASIC farm most.
    static KDFParams benchmark(std::chrono::milliseconds target_time);
};

} // namespace pasgen
