#include "crypto/CryptoEngine.hpp"
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
