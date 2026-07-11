#include "util/PasswordGenerator.hpp"
#include <openssl/rand.h>
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
    if (charset.empty()) charset = lowercase_chars;

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
    static const char* words[] = {
        "apple","banana","cherry","dragon","eagle","falcon","grape","harbor","island","jungle",
        "kettle","lemon","mango","nectar","orange","pepper","quartz","river","sunset","timber",
        "umbrella","violet","window","yellow","zebra","anchor","beacon","castle","desert","ember",
        "forest","glacier","horizon","ivory","jasper","kingdom","lantern","meadow","nebula","ocean",
        "planet","quantum","rainbow","shadow","thunder","universe","valley","whisper","xenon",
        "youth","zephyr","autumn","breeze","cosmic","diamond","eclipse"
    };
    static const size_t NWORDS = sizeof(words) / sizeof(words[0]);
    if (word_count == 0) return "";
    std::string result;
    for (size_t i = 0; i < word_count; ++i) {
        if (i > 0) result += sep;
        result += words[secure_random_index(NWORDS)];
    }
    return result;
}

uint8_t PasswordGenerator::secure_random_byte() {
    uint8_t b;
    if (RAND_bytes(&b, 1) != 1) throw std::runtime_error("Failed to generate random byte");
    return b;
}

size_t PasswordGenerator::secure_random_index(size_t max) {
    if (max <= 1) return 0;
    size_t limit = (256 / max) * max;
    uint8_t r;
    do { r = secure_random_byte(); } while (r >= limit);
    return r % max;
}

} // namespace pasgen
