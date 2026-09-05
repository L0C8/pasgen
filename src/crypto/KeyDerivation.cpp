#include "crypto/KeyDerivation.hpp"
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/opensslv.h>
#include <sstream>
#include <thread>

// OpenSSL grew a native Argon2id KDF in 3.2. Using it means the strongest KDF
// needs no third-party library at all, which is why libargon2 is now only a
// fallback for older OpenSSL rather than a hard dependency.
#if OPENSSL_VERSION_NUMBER >= 0x30200000L
#  define PASGEN_OPENSSL_ARGON2 1
#  include <openssl/kdf.h>
#  include <openssl/core_names.h>
#  include <openssl/params.h>
#else
#  define PASGEN_OPENSSL_ARGON2 0
#endif

#if !PASGEN_OPENSSL_ARGON2 && !defined(USE_PBKDF2_FALLBACK)
#  include <argon2.h>
#  define PASGEN_LIBARGON2 1
#else
#  define PASGEN_LIBARGON2 0
#endif

namespace pasgen {

const char* kdf_name(KdfId id) {
    switch (id) {
        case KdfId::Argon2id:     return "Argon2id";
        case KdfId::Pbkdf2Sha256: return "PBKDF2-SHA256";
    }
    return "unknown";
}

bool KDFParams::valid() const {
    if (kdf == KdfId::Pbkdf2Sha256)
        return pbkdf2_iterations >= 10000 && pbkdf2_iterations <= 100000000;
    return memory_kb   >= MIN_MEMORY_KB   && memory_kb   <= MAX_MEMORY_KB
        && iterations  >= MIN_ITERATIONS  && iterations  <= MAX_ITERATIONS
        && parallelism >= MIN_PARALLELISM && parallelism <= MAX_PARALLELISM;
}

std::string KDFParams::describe() const {
    std::ostringstream ss;
    if (kdf == KdfId::Pbkdf2Sha256) {
        ss << "PBKDF2-SHA256, " << pbkdf2_iterations << " iterations";
    } else {
        ss << "Argon2id, " << (memory_kb / 1024) << " MiB, t=" << iterations
           << ", p=" << parallelism;
    }
    return ss.str();
}

KDFParams KDFParams::from_json(const nlohmann::json& j) {
    KDFParams p;
    if (j.contains("memory_kb"))         p.memory_kb         = j["memory_kb"].get<uint32_t>();
    if (j.contains("iterations"))        p.iterations        = j["iterations"].get<uint32_t>();
    if (j.contains("parallelism"))       p.parallelism       = j["parallelism"].get<uint32_t>();
    if (j.contains("pbkdf2_iterations")) p.pbkdf2_iterations = j["pbkdf2_iterations"].get<uint32_t>();
    if (j.contains("kdf"))               p.kdf = static_cast<KdfId>(j["kdf"].get<uint16_t>());
    return p;
}

nlohmann::json KDFParams::to_json() const {
    return {
        {"memory_kb",         memory_kb},
        {"iterations",        iterations},
        {"parallelism",       parallelism},
        {"pbkdf2_iterations", pbkdf2_iterations},
        {"kdf",               static_cast<uint16_t>(kdf)}
    };
}

bool KeyDerivation::argon2_available() {
#if PASGEN_OPENSSL_ARGON2 || PASGEN_LIBARGON2
    return true;
#else
    return false;
#endif
}

KdfId KeyDerivation::preferred_kdf() {
    return argon2_available() ? KdfId::Argon2id : KdfId::Pbkdf2Sha256;
}

SecureBytes KeyDerivation::derive_key(
    const SecureString& password,
    const std::vector<uint8_t>& salt,
    size_t key_length,
    const KDFParams& params)
{
    if (params.kdf == KdfId::Pbkdf2Sha256)
        return derive_pbkdf2(password, salt, key_length, params.pbkdf2_iterations);

    if (!argon2_available())
        throw KeyDerivationException(
            "This database uses Argon2id, but this build has no Argon2 support. "
            "Rebuild against OpenSSL 3.2+ or install libargon2.");

    return derive_argon2id(password, salt, key_length, params);
}

#if PASGEN_OPENSSL_ARGON2

SecureBytes KeyDerivation::derive_argon2id(
    const SecureString& password,
    const std::vector<uint8_t>& salt,
    size_t key_length,
    const KDFParams& params)
{
    EVP_KDF* kdf = EVP_KDF_fetch(nullptr, "ARGON2ID", nullptr);
    if (!kdf) throw KeyDerivationException("OpenSSL has no ARGON2ID provider");

    EVP_KDF_CTX* ctx = EVP_KDF_CTX_new(kdf);
    EVP_KDF_free(kdf);
    if (!ctx) throw KeyDerivationException("Failed to create Argon2id context");

    uint32_t m_cost = params.memory_kb;
    uint32_t t_cost = params.iterations;
    uint32_t lanes  = params.parallelism;
    // Argon2's output is defined by the lane count, not by how many OS threads
    // compute it. Pinning threads to 1 keeps the result identical to a
    // multi-threaded run while avoiding OpenSSL's global thread-pool setup.
    uint32_t threads = 1;

    OSSL_PARAM p[] = {
        OSSL_PARAM_octet_string(OSSL_KDF_PARAM_PASSWORD,
                                const_cast<char*>(password.c_str()), password.size()),
        OSSL_PARAM_octet_string(OSSL_KDF_PARAM_SALT,
                                const_cast<uint8_t*>(salt.data()), salt.size()),
        OSSL_PARAM_uint32(OSSL_KDF_PARAM_ARGON2_MEMCOST, &m_cost),
        OSSL_PARAM_uint32(OSSL_KDF_PARAM_ITER,           &t_cost),
        OSSL_PARAM_uint32(OSSL_KDF_PARAM_ARGON2_LANES,   &lanes),
        OSSL_PARAM_uint32(OSSL_KDF_PARAM_THREADS,        &threads),
        OSSL_PARAM_END
    };

    SecureBytes key(key_length);
    int rc = EVP_KDF_derive(ctx, key.data(), key_length, p);
    EVP_KDF_CTX_free(ctx);
    if (rc <= 0)
        throw KeyDerivationException("Argon2id derivation failed (check memory/iteration limits)");
    return key;
}

#elif PASGEN_LIBARGON2

SecureBytes KeyDerivation::derive_argon2id(
    const SecureString& password,
    const std::vector<uint8_t>& salt,
    size_t key_length,
    const KDFParams& params)
{
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
}

#else

SecureBytes KeyDerivation::derive_argon2id(
    const SecureString&, const std::vector<uint8_t>&, size_t, const KDFParams&)
{
    throw KeyDerivationException("This build has no Argon2 support");
}

#endif

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
    params.kdf = preferred_kdf();

    const std::vector<uint8_t> salt(32, 0x42);
    const SecureString pw("benchmark_password");

    auto time_once = [&]() -> std::chrono::milliseconds {
        auto t0 = std::chrono::steady_clock::now();
        derive_key(pw, salt, 32, params);
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0);
    };

    try {
        if (params.kdf == KdfId::Pbkdf2Sha256) {
            params.pbkdf2_iterations = 200000;
            while (params.pbkdf2_iterations < 20000000) {
                if (time_once() >= target_time) break;
                params.pbkdf2_iterations = (uint32_t)(params.pbkdf2_iterations * 1.5);
            }
            return params;
        }

        // Match lanes to the machine, then grow memory first and only fall back
        // to extra passes once memory hits the ceiling.
        unsigned hw = std::thread::hardware_concurrency();
        params.parallelism = hw ? std::min<uint32_t>(hw, 8) : 4;
        params.iterations  = 3;
        params.memory_kb   = 65536; // start at 64 MiB and climb

        while (params.memory_kb < KDFParams::CALIBRATION_MAX_MEMORY_KB) {
            if (time_once() >= target_time) return params;
            params.memory_kb *= 2;
        }
        while (params.iterations < 10) {
            if (time_once() >= target_time) return params;
            params.iterations++;
        }
    } catch (const std::exception&) {
        // A machine that cannot sustain the probe keeps the last known-good
        // settings rather than failing database creation outright.
        return KDFParams{};
    }
    return params;
}

} // namespace pasgen
