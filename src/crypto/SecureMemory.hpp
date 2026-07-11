#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace pasgen {

namespace SecureMemory {
    void secure_zero(void* ptr, size_t size);
    bool constant_time_compare(const void* a, const void* b, size_t size);
}

class SecureString {
public:
    SecureString() = default;
    explicit SecureString(const std::string& str);
    explicit SecureString(const char* str);
    SecureString(const char* str, size_t len);
    SecureString(const SecureString& other);
    SecureString(SecureString&& other) noexcept;
    ~SecureString();

    SecureString& operator=(const SecureString& other);
    SecureString& operator=(SecureString&& other) noexcept;

    const char* c_str() const;
    const char* data() const;
    size_t size() const;
    size_t length() const { return size(); }
    bool empty() const;
    void clear();

    void append(const char* str, size_t len);
    void append(char c);

    bool operator==(const SecureString& other) const;
    bool operator!=(const SecureString& other) const;

private:
    std::vector<char> data_;
    void secure_clear();
};

class SecureBytes {
public:
    SecureBytes() = default;
    explicit SecureBytes(size_t size);
    explicit SecureBytes(const std::vector<uint8_t>& data);
    SecureBytes(const uint8_t* data, size_t size);
    ~SecureBytes();

    SecureBytes(const SecureBytes&) = delete;
    SecureBytes& operator=(const SecureBytes&) = delete;
    SecureBytes(SecureBytes&& other) noexcept;
    SecureBytes& operator=(SecureBytes&& other) noexcept;

    uint8_t* data();
    const uint8_t* data() const;
    size_t size() const;
    bool empty() const;
    void clear();
    void resize(size_t new_size);

    uint8_t& operator[](size_t index);
    const uint8_t& operator[](size_t index) const;

private:
    std::vector<uint8_t> data_;
    void secure_clear();
};

} // namespace pasgen
