#include "util/PasswordGenerator.hpp"
#include "util/Wordlist.hpp"
#include <openssl/rand.h>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace pasgen {

const char* PasswordGenerator::lowercase_chars = "abcdefghijklmnopqrstuvwxyz";
const char* PasswordGenerator::uppercase_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const char* PasswordGenerator::digit_chars     = "0123456789";
const char* PasswordGenerator::symbol_chars    = "!@#$%^&*()_+-=[]{}|;:,.<>?";

std::string PasswordGenerator::generate(
    size_t length,
    bool use_lowercase, bool use_uppercase, bool use_digits, bool use_symbols)
{
    if (length == 0) return "";

    std::string charset;
    if (use_lowercase) charset += lowercase_chars;
    if (use_uppercase) charset += uppercase_chars;
    if (use_digits)    charset += digit_chars;
    if (use_symbols)   charset += symbol_chars;
    if (charset.empty()) { charset = lowercase_chars; use_lowercase = true; }

    std::string required;
    if (use_lowercase)                     required += lowercase_chars[secure_random_index(std::strlen(lowercase_chars))];
    if (use_uppercase && length > required.size()) required += uppercase_chars[secure_random_index(std::strlen(uppercase_chars))];
    if (use_digits    && length > required.size()) required += digit_chars[secure_random_index(std::strlen(digit_chars))];
    if (use_symbols   && length > required.size()) required += symbol_chars[secure_random_index(std::strlen(symbol_chars))];

    std::string pw;
    while (pw.size() + required.size() < length)
        pw += charset[secure_random_index(charset.size())];

    for (char c : required) {
        size_t pos = secure_random_index(pw.size() + 1);
        pw.insert(pos, 1, c);
    }
    return pw;
}

std::string PasswordGenerator::generate_passphrase(size_t word_count, const std::string& sep) {
    if (word_count == 0) return "";
    std::string result;
    for (size_t i = 0; i < word_count; ++i) {
        if (i > 0) result += sep;
        result += kEffLargeWordlist[secure_random_index(kEffLargeWordlistSize)];
    }
    return result;
}

double PasswordGenerator::entropy_bits(size_t length, bool lc, bool uc, bool dig, bool sym) {
    size_t pool = 0;
    if (lc)  pool += std::strlen(lowercase_chars);
    if (uc)  pool += std::strlen(uppercase_chars);
    if (dig) pool += std::strlen(digit_chars);
    if (sym) pool += std::strlen(symbol_chars);
    if (pool == 0) pool = std::strlen(lowercase_chars);
    // The one-of-each-class guarantee trims the space slightly; for any
    // practical length the difference is a fraction of a bit.
    return (double)length * std::log2((double)pool);
}

double PasswordGenerator::passphrase_entropy_bits(size_t word_count) {
    return (double)word_count * std::log2((double)kEffLargeWordlistSize);
}

size_t PasswordGenerator::secure_random_index(size_t max) {
    if (max <= 1) return 0;
    if (max > 0xFFFFFFFFu) throw std::invalid_argument("secure_random_index: range too large");
    // Largest multiple of max that fits in 32 bits; anything at or above it is
    // redrawn so every index is equally likely.
    const uint64_t range = 0x100000000ull;
    const uint64_t limit = range - (range % max);
    uint32_t r;
    do {
        if (RAND_bytes(reinterpret_cast<unsigned char*>(&r), sizeof(r)) != 1)
            throw std::runtime_error("Failed to generate random bytes");
    } while ((uint64_t)r >= limit);
    return (size_t)(r % max);
}

} // namespace pasgen
