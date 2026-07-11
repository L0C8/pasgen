#include "crypto/KeyDerivation.hpp"
#include <openssl/evp.h>
#include <openssl/err.h>

#ifndef USE_PBKDF2_FALLBACK
#include <argon2.h>
#endif

namespace pasgen {

KDFParams KDFParams::from_json(const nlohmann::json& j) {
    KDFParams p;
    if (j.contains("memory_kb"))       p.memory_kb        = j["memory_kb"].get<uint32_t>();
    if (j.contains("iterations"))      p.iterations       = j["iterations"].get<uint32_t>();
    if (j.contains("parallelism"))     p.parallelism      = j["parallelism"].get<uint32_t>();
    if (j.contains("pbkdf2_iterations")) p.pbkdf2_iterations = j["pbkdf2_iterations"].get<uint32_t>();
    return p;
}

nlohmann::json KDFParams::to_json() const {
    return {
        {"memory_kb",        memory_kb},
        {"iterations",       iterations},
        {"parallelism",      parallelism},
        {"pbkdf2_iterations", pbkdf2_iterations}
    };
}

bool KeyDerivation::argon2_available() {
#ifdef USE_PBKDF2_FALLBACK
    return false;
#else
    return true;
#endif
}

SecureBytes KeyDerivation::derive_key(
    const SecureString& password,
    const std::vector<uint8_t>& salt,
    size_t key_length,
    const KDFParams& params)
{
#ifdef USE_PBKDF2_FALLBACK
    return derive_pbkdf2(password, salt, key_length, params.pbkdf2_iterations);
#else
    SecureBytes key(key_length);
    int result = argon2id_hash_raw(
        params.iterations,
        params.memory_kb,
        params.parallelism,
        password.c_str(), password.size(),
        salt.data(), salt.size(),
        key.data(), key_length
    );
    if (result != ARGON2_OK)
        throw KeyDerivationException(
            std::string("Argon2id failed: ") + argon2_error_message(result));
    return key;
#endif
}

SecureBytes KeyDerivation::derive_pbkdf2(
    const SecureString& password,
    const std::vector<uint8_t>& salt,
    size_t key_length,
    uint32_t iterations)
{
    SecureBytes key(key_length);
    int result = PKCS5_PBKDF2_HMAC(
        password.c_str(), (int)password.size(),
        salt.data(), (int)salt.size(),
        (int)iterations,
        EVP_sha256(),
        (int)key_length,
        key.data()
    );
    if (result != 1)
        throw KeyDerivationException("PBKDF2 key derivation failed");
    return key;
}

KDFParams KeyDerivation::benchmark(std::chrono::milliseconds target_time) {
    KDFParams params;
    std::vector<uint8_t> salt(32, 0x42);
    SecureString pw("benchmark_password");

#ifdef USE_PBKDF2_FALLBACK
    params.pbkdf2_iterations = 100000;
    while (params.pbkdf2_iterations < 10000000) {
        auto t0 = std::chrono::steady_clock::now();
        try { derive_pbkdf2(pw, salt, 32, params.pbkdf2_iterations); } catch (...) { break; }
        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0);
        if (dur >= target_time) break;
        params.pbkdf2_iterations = (uint32_t)(params.pbkdf2_iterations * 1.5);
    }
#else
    params.memory_kb = 65536; params.iterations = 1; params.parallelism = 4;
    while (params.iterations < 20) {
        auto t0 = std::chrono::steady_clock::now();
        try { derive_key(pw, salt, 32, params); } catch (...) { break; }
        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0);
        if (dur >= target_time) break;
        params.iterations++;
    }
#endif
    return params;
}

} // namespace pasgen
