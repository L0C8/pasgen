#include "util/TOTPGenerator.hpp"
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace pasgen {

std::string TOTPGenerator::generate(const std::string& secret_base32,
                                    uint64_t time_step, size_t digits) {
    std::vector<uint8_t> key = decode_base32(secret_base32);
    if (key.empty()) throw std::runtime_error("Invalid base32 secret");

    uint64_t counter = get_current_time_step(time_step);
    std::vector<uint8_t> cb(8);
    for (int i = 7; i >= 0; --i) { cb[i] = counter & 0xFF; counter >>= 8; }

    auto hash = hmac_sha1(key, cb);
    int offset = hash[19] & 0x0F;
    uint32_t binary = ((hash[offset]   & 0x7F) << 24) |
                      ((hash[offset+1] & 0xFF) << 16) |
                      ((hash[offset+2] & 0xFF) << 8)  |
                       (hash[offset+3] & 0xFF);

    uint32_t otp = binary % (uint32_t)std::pow(10, digits);
    std::string r = std::to_string(otp);
    while (r.size() < digits) r = "0" + r;
    return r;
}

uint32_t TOTPGenerator::seconds_remaining(uint64_t time_step) {
    auto s = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return (uint32_t)(time_step - (s % time_step));
}

bool TOTPGenerator::validate(const std::string& code, const std::string& secret_base32,
                             int tolerance, uint64_t time_step) {
    auto s = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::vector<uint8_t> key = decode_base32(secret_base32);
    if (key.empty()) return false;

    for (int i = -tolerance; i <= tolerance; ++i) {
        uint64_t step = ((uint64_t)(s + i * (int64_t)time_step)) / time_step;
        std::vector<uint8_t> cb(8);
        uint64_t tmp = step;
        for (int j = 7; j >= 0; --j) { cb[j] = tmp & 0xFF; tmp >>= 8; }
        auto hash = hmac_sha1(key, cb);
        int off = hash[19] & 0x0F;
        uint32_t bin = ((hash[off]   & 0x7F) << 24) | ((hash[off+1] & 0xFF) << 16) |
                       ((hash[off+2] & 0xFF) << 8)  |  (hash[off+3] & 0xFF);
        std::string exp = std::to_string(bin % 1000000);
        while (exp.size() < 6) exp = "0" + exp;
        if (code == exp) return true;
    }
    return false;
}

std::vector<uint8_t> TOTPGenerator::decode_base32(const std::string& encoded) {
    static const char* B32 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    std::string input = encoded;
    input.erase(std::remove_if(input.begin(), input.end(), ::isspace), input.end());
    std::transform(input.begin(), input.end(), input.begin(), ::toupper);
    while (!input.empty() && input.back() == '=') input.pop_back();
    if (input.empty()) return {};

    std::vector<uint8_t> out;
    out.reserve((input.size() * 5) / 8);
    int buf = 0, bits = 0;
    for (char c : input) {
        const char* p = std::strchr(B32, c);
        if (!p) return {};
        buf = (buf << 5) | (int)(p - B32);
        bits += 5;
        if (bits >= 8) { bits -= 8; out.push_back((uint8_t)((buf >> bits) & 0xFF)); }
    }
    return out;
}

std::vector<uint8_t> TOTPGenerator::hmac_sha1(const std::vector<uint8_t>& key,
                                              const std::vector<uint8_t>& data) {
    std::vector<uint8_t> r(20);
    unsigned int len = 20;
    HMAC(EVP_sha1(), key.data(), (int)key.size(), data.data(), data.size(), r.data(), &len);
    return r;
}

uint64_t TOTPGenerator::get_current_time_step(uint64_t time_step) {
    auto s = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return (uint64_t)s / time_step;
}

} // namespace pasgen
