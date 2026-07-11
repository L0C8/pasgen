#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pasgen {

class TOTPGenerator {
public:
    static std::string generate(const std::string& secret_base32,
                                uint64_t time_step = 30,
                                size_t digits = 6);

    static uint32_t seconds_remaining(uint64_t time_step = 30);

    static bool validate(const std::string& code,
                         const std::string& secret_base32,
                         int tolerance = 1,
                         uint64_t time_step = 30);

private:
    static std::vector<uint8_t> decode_base32(const std::string& encoded);
    static std::vector<uint8_t> hmac_sha1(const std::vector<uint8_t>& key,
                                          const std::vector<uint8_t>& data);
    static uint64_t get_current_time_step(uint64_t time_step);
};

} // namespace pasgen
