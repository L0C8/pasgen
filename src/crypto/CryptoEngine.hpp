#pragma once

#include "crypto/SecureMemory.hpp"
#include "crypto/KeyDerivation.hpp"
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace pasgen {

class CryptoException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class CryptoEngine {
public:
    static constexpr size_t KEY_SIZE  = 32;
    static constexpr size_t IV_SIZE   = 12;
    static constexpr size_t TAG_SIZE  = 16;
    static constexpr size_t SALT_SIZE = 32;

    CryptoEngine() = default;
    ~CryptoEngine() = default;

    std::vector<uint8_t> encrypt(
        const std::vector<uint8_t>& plaintext,
        const SecureBytes& key,
        const std::vector<uint8_t>& aad = {});

    std::vector<uint8_t> decrypt(
        const std::vector<uint8_t>& ciphertext,
        const SecureBytes& key,
        const std::vector<uint8_t>& aad = {});

    std::vector<uint8_t> encrypt_with_iv(
        const std::vector<uint8_t>& plaintext,
        const SecureBytes& key,
        const std::vector<uint8_t>& iv,
        const std::vector<uint8_t>& aad = {});

    std::vector<uint8_t> decrypt_with_iv(
        const std::vector<uint8_t>& ciphertext,
        const SecureBytes& key,
        const std::vector<uint8_t>& iv,
        const std::vector<uint8_t>& tag,
        const std::vector<uint8_t>& aad = {});

    // ── Cascade ──────────────────────────────────────────────────────────────
    // The payload is encrypted twice, under two independent subkeys: first with
    // ChaCha20-Poly1305, then that whole blob (ciphertext + its tag) with
    // AES-256-GCM. Recovering the plaintext requires breaking BOTH primitives,
    // so a future weakness in either one alone does not expose the vault.
    // The subkeys come from HKDF-SHA512 over the Argon2id output, so neither
    // layer ever sees the master key itself and the two keys are independent.
    std::vector<uint8_t> encrypt_cascade(
        const std::vector<uint8_t>& plaintext,
        const SecureBytes& root_key,
        const std::vector<uint8_t>& iv_outer,
        const std::vector<uint8_t>& nonce_inner,
        const std::vector<uint8_t>& aad = {});

    std::vector<uint8_t> decrypt_cascade(
        const std::vector<uint8_t>& ciphertext,
        const SecureBytes& root_key,
        const std::vector<uint8_t>& iv_outer,
        const std::vector<uint8_t>& nonce_inner,
        const std::vector<uint8_t>& tag_outer,
        const std::vector<uint8_t>& aad = {});

    // Domain-separated subkey from a high-entropy root key.
    static SecureBytes derive_subkey(const SecureBytes& root_key, const char* info);

    std::vector<uint8_t> generate_random_bytes(size_t count);
    std::vector<uint8_t> generate_nonce() { return generate_random_bytes(IV_SIZE); }
    std::vector<uint8_t> generate_salt();
    std::vector<uint8_t> generate_iv();

    SecureBytes derive_key(
        const SecureString& password,
        const std::vector<uint8_t>& salt,
        const Argon2Params& params = Argon2Params());
};

} // namespace pasgen
