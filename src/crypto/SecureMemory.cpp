#include "crypto/SecureMemory.hpp"
#include <openssl/crypto.h>

namespace pasgen {

namespace SecureMemory {

void secure_zero(void* ptr, size_t size) {
    if (ptr && size > 0) {
        OPENSSL_cleanse(ptr, size);
    }
}

bool constant_time_compare(const void* a, const void* b, size_t size) {
    return CRYPTO_memcmp(a, b, size) == 0;
}

} // namespace SecureMemory

SecureString::SecureString(const std::string& str) {
    if (!str.empty()) {
        data_.resize(str.size() + 1);
        std::memcpy(data_.data(), str.c_str(), str.size());
        data_[str.size()] = '\0';
    }
}

SecureString::SecureString(const char* str) {
    if (str) {
        size_t len = std::strlen(str);
        data_.resize(len + 1);
        std::memcpy(data_.data(), str, len);
        data_[len] = '\0';
    }
}

SecureString::SecureString(const char* str, size_t len) {
    if (str && len > 0) {
        data_.resize(len + 1);
        std::memcpy(data_.data(), str, len);
        data_[len] = '\0';
    }
}

SecureString::SecureString(const SecureString& other) {
    if (!other.empty()) data_ = other.data_;
}

SecureString::SecureString(SecureString&& other) noexcept
    : data_(std::move(other.data_)) {}

SecureString::~SecureString() { secure_clear(); }

SecureString& SecureString::operator=(const SecureString& other) {
    if (this != &other) {
        secure_clear();
        if (!other.empty()) data_ = other.data_;
    }
    return *this;
}

SecureString& SecureString::operator=(SecureString&& other) noexcept {
    if (this != &other) {
        secure_clear();
        data_ = std::move(other.data_);
    }
    return *this;
}

const char* SecureString::c_str() const { return data_.empty() ? "" : data_.data(); }
const char* SecureString::data() const  { return data_.empty() ? nullptr : data_.data(); }

size_t SecureString::size() const {
    return data_.empty() ? 0 : data_.size() - 1;
}

bool SecureString::empty() const {
    return data_.empty() || data_.size() <= 1;
}

void SecureString::clear() {
    secure_clear();
    data_.clear();
}

void SecureString::append(const char* str, size_t len) {
    if (str && len > 0) {
        size_t old = size();
        data_.resize(old + len + 1);
        std::memcpy(data_.data() + old, str, len);
        data_[old + len] = '\0';
    }
}

void SecureString::append(char c) {
    size_t old = size();
    data_.resize(old + 2);
    data_[old] = c;
    data_[old + 1] = '\0';
}

bool SecureString::operator==(const SecureString& other) const {
    if (size() != other.size()) return false;
    if (empty() && other.empty()) return true;
    return SecureMemory::constant_time_compare(data_.data(), other.data_.data(), size());
}

bool SecureString::operator!=(const SecureString& other) const { return !(*this == other); }

void SecureString::secure_clear() {
    if (!data_.empty()) SecureMemory::secure_zero(data_.data(), data_.size());
}

// SecureBytes

SecureBytes::SecureBytes(size_t size) : data_(size, 0) {}
SecureBytes::SecureBytes(const std::vector<uint8_t>& data) : data_(data) {}

SecureBytes::SecureBytes(const uint8_t* data, size_t size) {
    if (data && size > 0) {
        data_.resize(size);
        std::memcpy(data_.data(), data, size);
    }
}

SecureBytes::~SecureBytes() { secure_clear(); }

SecureBytes::SecureBytes(SecureBytes&& other) noexcept
    : data_(std::move(other.data_)) {}

SecureBytes& SecureBytes::operator=(SecureBytes&& other) noexcept {
    if (this != &other) {
        secure_clear();
        data_ = std::move(other.data_);
    }
    return *this;
}

uint8_t*       SecureBytes::data()       { return data_.empty() ? nullptr : data_.data(); }
const uint8_t* SecureBytes::data() const { return data_.empty() ? nullptr : data_.data(); }
size_t         SecureBytes::size() const { return data_.size(); }
bool           SecureBytes::empty() const { return data_.empty(); }

void SecureBytes::clear() {
    secure_clear();
    data_.clear();
}

void SecureBytes::resize(size_t new_size) {
    if (new_size < data_.size())
        SecureMemory::secure_zero(data_.data() + new_size, data_.size() - new_size);
    data_.resize(new_size);
}

uint8_t&       SecureBytes::operator[](size_t i)       { return data_[i]; }
const uint8_t& SecureBytes::operator[](size_t i) const { return data_[i]; }

void SecureBytes::secure_clear() {
    if (!data_.empty()) SecureMemory::secure_zero(data_.data(), data_.size());
}

} // namespace pasgen
