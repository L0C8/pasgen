#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace pasgen {

class PasswordGenerator {
public:
    static std::string generate(
        size_t length = 16,
        bool use_lowercase = true,
        bool use_uppercase = true,
        bool use_digits = true,
        bool use_symbols = true);

    // Words come from the EFF large wordlist (7,776 words), so each word adds
    // 12.9 bits: 6 words is ~77 bits, comparable to a random 12-character
    // password over all four character classes.
    static std::string generate_passphrase(
        size_t word_count = 6,
        const std::string& separator = "-");

    // Entropy in bits of what generate()/generate_passphrase() produce for
    // these settings. This is the real search space an attacker faces, unlike
    // a heuristic strength meter, because the output is uniformly random.
    static double entropy_bits(size_t length, bool use_lowercase, bool use_uppercase,
                               bool use_digits, bool use_symbols);
    static double passphrase_entropy_bits(size_t word_count);

    // Uniform index in [0, max) from the OpenSSL CSPRNG, by rejection
    // sampling, so there is no modulo bias for any max.
    static size_t secure_random_index(size_t max);

private:
    static const char* lowercase_chars;
    static const char* uppercase_chars;
    static const char* digit_chars;
    static const char* symbol_chars;
};

} // namespace pasgen
