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

    static std::string generate_passphrase(
        size_t word_count = 4,
        const std::string& separator = "-");

private:
    static const char* lowercase_chars;
    static const char* uppercase_chars;
    static const char* digit_chars;
    static const char* symbol_chars;

    static uint8_t secure_random_byte();
    static size_t  secure_random_index(size_t max);
};

} // namespace pasgen
