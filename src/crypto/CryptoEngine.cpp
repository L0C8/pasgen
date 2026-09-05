#include "crypto/CryptoEngine.hpp"
#include <openssl/kdf.h>
#include <openssl/core_names.h>
#include <openssl/params.h>
#include <cstring>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <cstring>

namespace pasgen {

namespace {
std::string get_openssl_error() {
    unsigned long err = ERR_get_error();
    if (err == 0) return "Unknown error";
    char buf[256];
    ERR_error_string_n(err, buf, sizeof(buf));
    return std::string(buf);
}
} // namespace

std::vector<uint8_t> CryptoEngine::encrypt(
    const std::vector<uint8_t>& plaintext,
    const SecureBytes& key,
    const std::vector<uint8_t>& aad)
{
    auto iv = generate_iv();
    auto result = encrypt_with_iv(plaintext, key, iv, aad);
    std::vector<uint8_t> output;
    output.reserve(IV_SIZE + result.size());
    output.insert(output.end(), iv.begin(), iv.end());
    output.insert(output.end(), result.begin(), result.end());
    return output;
}

std::vector<uint8_t> CryptoEngine::decrypt(
    const std::vector<uint8_t>& ciphertext,
    const SecureBytes& key,
    const std::vector<uint8_t>& aad)
{
    if (ciphertext.size() < IV_SIZE + TAG_SIZE)
        throw CryptoException("Ciphertext too short");
    std::vector<uint8_t> iv(ciphertext.begin(), ciphertext.begin() + IV_SIZE);
    std::vector<uint8_t> tag(ciphertext.end() - TAG_SIZE, ciphertext.end());
    std::vector<uint8_t> ct(ciphertext.begin() + IV_SIZE, ciphertext.end() - TAG_SIZE);
    return decrypt_with_iv(ct, key, iv, tag, aad);
}

std::vector<uint8_t> CryptoEngine::encrypt_with_iv(
    const std::vector<uint8_t>& plaintext,
    const SecureBytes& key,
    const std::vector<uint8_t>& iv,
    const std::vector<uint8_t>& aad)
{
    if (key.size() != KEY_SIZE) throw CryptoException("Invalid key size");
    if (iv.size() != IV_SIZE)   throw CryptoException("Invalid IV size");

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw CryptoException("Failed to create cipher context");

    std::vector<uint8_t> ciphertext(plaintext.size());
    std::vector<uint8_t> tag(TAG_SIZE);
    int len = 0, ct_len = 0;

    try {
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1)
            throw CryptoException("Failed to init encryption: " + get_openssl_error());
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_SIZE, nullptr) != 1)
            throw CryptoException("Failed to set IV length");
        if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1)
            throw CryptoException("Failed to set key/IV");
        if (!aad.empty())
            if (EVP_EncryptUpdate(ctx, nullptr, &len, aad.data(), (int)aad.size()) != 1)
                throw CryptoException("Failed to add AAD");
        if (!plaintext.empty()) {
            if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, plaintext.data(), (int)plaintext.size()) != 1)
                throw CryptoException("Encryption failed");
            ct_len = len;
        }
        if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + ct_len, &len) != 1)
            throw CryptoException("Encryption finalization failed");
        ct_len += len;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_SIZE, tag.data()) != 1)
            throw CryptoException("Failed to get auth tag");
        ciphertext.resize(ct_len);
    } catch (...) {
        EVP_CIPHER_CTX_free(ctx);
        throw;
    }

    EVP_CIPHER_CTX_free(ctx);
    ciphertext.insert(ciphertext.end(), tag.begin(), tag.end());
    return ciphertext;
}

std::vector<uint8_t> CryptoEngine::decrypt_with_iv(
    const std::vector<uint8_t>& ciphertext,
    const SecureBytes& key,
    const std::vector<uint8_t>& iv,
    const std::vector<uint8_t>& tag,
    const std::vector<uint8_t>& aad)
{
    if (key.size() != KEY_SIZE) throw CryptoException("Invalid key size");
    if (iv.size()  != IV_SIZE)  throw CryptoException("Invalid IV size");
    if (tag.size() != TAG_SIZE) throw CryptoException("Invalid tag size");

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw CryptoException("Failed to create cipher context");

    std::vector<uint8_t> plaintext(ciphertext.size());
    int len = 0, pt_len = 0;

    try {
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1)
            throw CryptoException("Failed to init decryption");
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_SIZE, nullptr) != 1)
            throw CryptoException("Failed to set IV length");
        if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1)
            throw CryptoException("Failed to set key/IV");
        if (!aad.empty())
            if (EVP_DecryptUpdate(ctx, nullptr, &len, aad.data(), (int)aad.size()) != 1)
                throw CryptoException("Failed to add AAD");
        if (!ciphertext.empty()) {
            if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(), (int)ciphertext.size()) != 1)
                throw CryptoException("Decryption failed");
            pt_len = len;
        }
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TAG_SIZE, const_cast<uint8_t*>(tag.data())) != 1)
            throw CryptoException("Failed to set auth tag");
        if (EVP_DecryptFinal_ex(ctx, plaintext.data() + pt_len, &len) <= 0)
            throw CryptoException("Authentication failed: wrong password or corrupted data");
        pt_len += len;
        plaintext.resize(pt_len);
    } catch (...) {
        EVP_CIPHER_CTX_free(ctx);
        throw;
    }

    EVP_CIPHER_CTX_free(ctx);
    return plaintext;
}

namespace {

// One AEAD pass. GCM and ChaCha20-Poly1305 share the EVP_CTRL_AEAD_* controls,
// so a single routine drives both layers of the cascade.
std::vector<uint8_t> aead_seal(const EVP_CIPHER* cipher,
                               const std::vector<uint8_t>& plaintext,
                               const SecureBytes& key,
                               const std::vector<uint8_t>& nonce,
                               const std::vector<uint8_t>& aad)
{
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw CryptoException("Failed to create cipher context");

    std::vector<uint8_t> out(plaintext.size());
    std::vector<uint8_t> tag(CryptoEngine::TAG_SIZE);
    int len = 0, out_len = 0;

    try {
        if (EVP_EncryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr) != 1)
            throw CryptoException("AEAD init failed");
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, (int)nonce.size(), nullptr) != 1)
            throw CryptoException("AEAD nonce length rejected");
        if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce.data()) != 1)
            throw CryptoException("AEAD key/nonce rejected");
        if (!aad.empty() &&
            EVP_EncryptUpdate(ctx, nullptr, &len, aad.data(), (int)aad.size()) != 1)
            throw CryptoException("AEAD AAD rejected");
        if (!plaintext.empty()) {
            if (EVP_EncryptUpdate(ctx, out.data(), &len, plaintext.data(), (int)plaintext.size()) != 1)
                throw CryptoException("AEAD encryption failed");
            out_len = len;
        }
        if (EVP_EncryptFinal_ex(ctx, out.data() + out_len, &len) != 1)
            throw CryptoException("AEAD finalization failed");
        out_len += len;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG,
                                (int)CryptoEngine::TAG_SIZE, tag.data()) != 1)
            throw CryptoException("AEAD tag extraction failed");
        out.resize(out_len);
    } catch (...) { EVP_CIPHER_CTX_free(ctx); throw; }

    EVP_CIPHER_CTX_free(ctx);
    out.insert(out.end(), tag.begin(), tag.end());
    return out;
}

std::vector<uint8_t> aead_open(const EVP_CIPHER* cipher,
                               const std::vector<uint8_t>& ciphertext,
                               const SecureBytes& key,
                               const std::vector<uint8_t>& nonce,
                               const std::vector<uint8_t>& tag,
                               const std::vector<uint8_t>& aad)
{
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw CryptoException("Failed to create cipher context");

    std::vector<uint8_t> out(ciphertext.size());
    int len = 0, out_len = 0;

    try {
        if (EVP_DecryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr) != 1)
            throw CryptoException("AEAD init failed");
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, (int)nonce.size(), nullptr) != 1)
            throw CryptoException("AEAD nonce length rejected");
        if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce.data()) != 1)
            throw CryptoException("AEAD key/nonce rejected");
        if (!aad.empty() &&
            EVP_DecryptUpdate(ctx, nullptr, &len, aad.data(), (int)aad.size()) != 1)
            throw CryptoException("AEAD AAD rejected");
        if (!ciphertext.empty()) {
            if (EVP_DecryptUpdate(ctx, out.data(), &len, ciphertext.data(), (int)ciphertext.size()) != 1)
                throw CryptoException("AEAD decryption failed");
            out_len = len;
        }
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, (int)tag.size(),
                                const_cast<uint8_t*>(tag.data())) != 1)
            throw CryptoException("AEAD tag rejected");
        if (EVP_DecryptFinal_ex(ctx, out.data() + out_len, &len) <= 0)
            throw CryptoException("Authentication failed: wrong password or corrupted data");
        out_len += len;
        out.resize(out_len);
    } catch (...) { EVP_CIPHER_CTX_free(ctx); throw; }

    EVP_CIPHER_CTX_free(ctx);
    return out;
}

} // namespace

SecureBytes CryptoEngine::derive_subkey(const SecureBytes& root_key, const char* info) {
    EVP_KDF* kdf = EVP_KDF_fetch(nullptr, "HKDF", nullptr);
    if (!kdf) throw CryptoException("OpenSSL has no HKDF provider");
    EVP_KDF_CTX* ctx = EVP_KDF_CTX_new(kdf);
    EVP_KDF_free(kdf);
    if (!ctx) throw CryptoException("Failed to create HKDF context");

    SecureBytes out(KEY_SIZE);
    char digest[] = "SHA512";
    OSSL_PARAM p[] = {
        OSSL_PARAM_utf8_string(OSSL_KDF_PARAM_DIGEST, digest, 0),
        OSSL_PARAM_octet_string(OSSL_KDF_PARAM_KEY,
                                const_cast<uint8_t*>(root_key.data()), root_key.size()),
        OSSL_PARAM_octet_string(OSSL_KDF_PARAM_INFO,
                                const_cast<char*>(info), std::strlen(info)),
        OSSL_PARAM_END
    };
    int rc = EVP_KDF_derive(ctx, out.data(), out.size(), p);
    EVP_KDF_CTX_free(ctx);
    if (rc <= 0) throw CryptoException("HKDF subkey derivation failed");
    return out;
}

std::vector<uint8_t> CryptoEngine::encrypt_cascade(
    const std::vector<uint8_t>& plaintext,
    const SecureBytes& root_key,
    const std::vector<uint8_t>& iv_outer,
    const std::vector<uint8_t>& nonce_inner,
    const std::vector<uint8_t>& aad)
{
    if (root_key.size() != KEY_SIZE)     throw CryptoException("Invalid key size");
    if (iv_outer.size() != IV_SIZE)      throw CryptoException("Invalid outer IV size");
    if (nonce_inner.size() != IV_SIZE)   throw CryptoException("Invalid inner nonce size");

    SecureBytes k_inner = derive_subkey(root_key, "pasgen/v3/inner/chacha20-poly1305");
    SecureBytes k_outer = derive_subkey(root_key, "pasgen/v3/outer/aes-256-gcm");

    // Inner layer, then the entire inner blob (its ciphertext AND its tag) is
    // encrypted again by the outer layer, so nothing about the inner result is
    // observable without first breaking AES-256-GCM.
    std::vector<uint8_t> inner =
        aead_seal(EVP_chacha20_poly1305(), plaintext, k_inner, nonce_inner, aad);
    std::vector<uint8_t> outer =
        aead_seal(EVP_aes_256_gcm(), inner, k_outer, iv_outer, aad);

    SecureMemory::secure_zero(inner.data(), inner.size());
    return outer;
}

std::vector<uint8_t> CryptoEngine::decrypt_cascade(
    const std::vector<uint8_t>& ciphertext,
    const SecureBytes& root_key,
    const std::vector<uint8_t>& iv_outer,
    const std::vector<uint8_t>& nonce_inner,
    const std::vector<uint8_t>& tag_outer,
    const std::vector<uint8_t>& aad)
{
    if (root_key.size() != KEY_SIZE)      throw CryptoException("Invalid key size");
    if (iv_outer.size() != IV_SIZE)       throw CryptoException("Invalid outer IV size");
    if (nonce_inner.size() != IV_SIZE)    throw CryptoException("Invalid inner nonce size");
    if (tag_outer.size() != TAG_SIZE)     throw CryptoException("Invalid tag size");

    SecureBytes k_inner = derive_subkey(root_key, "pasgen/v3/inner/chacha20-poly1305");
    SecureBytes k_outer = derive_subkey(root_key, "pasgen/v3/outer/aes-256-gcm");

    std::vector<uint8_t> inner =
        aead_open(EVP_aes_256_gcm(), ciphertext, k_outer, iv_outer, tag_outer, aad);
    if (inner.size() < TAG_SIZE)
        throw CryptoException("Authentication failed: inner layer truncated");

    std::vector<uint8_t> inner_ct(inner.begin(), inner.end() - TAG_SIZE);
    std::vector<uint8_t> inner_tag(inner.end() - TAG_SIZE, inner.end());
    std::vector<uint8_t> plain =
        aead_open(EVP_chacha20_poly1305(), inner_ct, k_inner, nonce_inner, inner_tag, aad);

    SecureMemory::secure_zero(inner.data(), inner.size());
    SecureMemory::secure_zero(inner_ct.data(), inner_ct.size());
    return plain;
}

std::vector<uint8_t> CryptoEngine::generate_random_bytes(size_t count) {
    std::vector<uint8_t> bytes(count);
    if (RAND_bytes(bytes.data(), (int)count) != 1)
        throw CryptoException("Failed to generate random bytes: " + get_openssl_error());
    return bytes;
}

std::vector<uint8_t> CryptoEngine::generate_salt() { return generate_random_bytes(SALT_SIZE); }
std::vector<uint8_t> CryptoEngine::generate_iv()   { return generate_random_bytes(IV_SIZE); }

SecureBytes CryptoEngine::derive_key(
    const SecureString& password,
    const std::vector<uint8_t>& salt,
    const Argon2Params& params)
{
    return KeyDerivation::derive_argon2id(password, salt, KEY_SIZE, params);
}

} // namespace pasgen
