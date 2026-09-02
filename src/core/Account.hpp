#pragma once

#include "crypto/SecureMemory.hpp"
#include <chrono>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace pasgen {

struct PasswordHistoryEntry {
    SecureString password;
    std::chrono::system_clock::time_point changed_at;

    nlohmann::json to_json() const;
    static PasswordHistoryEntry from_json(const nlohmann::json& j);
};

class Account {
public:
    Account();
    explicit Account(const nlohmann::json& j);

    const std::string& id() const       { return id_; }
    const std::string& name() const     { return name_; }
    const std::string& email() const    { return email_; }
    const std::string& url() const      { return url_; }
    const std::string& username() const { return username_; }
    const SecureString& password() const { return password_; }
    const std::string& notes() const    { return notes_; }
    const std::string& totp_secret() const { return totp_secret_; }
    const std::vector<PasswordHistoryEntry>& password_history() const { return password_history_; }
    std::chrono::system_clock::time_point created_at() const  { return created_at_; }
    std::chrono::system_clock::time_point modified_at() const { return modified_at_; }
    const std::string& category_id() const { return category_id_; }
    int order() const { return order_; }

    void set_name(const std::string& name);
    void set_email(const std::string& email);
    void set_url(const std::string& url);
    void set_username(const std::string& username);
    void set_password(const SecureString& password);
    void set_notes(const std::string& notes);
    void set_totp_secret(const std::string& secret);
    void set_category_id(const std::string& category_id);
    void set_order(int order);

    nlohmann::json to_json() const;
    static Account from_json(const nlohmann::json& j);

    bool matches_search(const std::string& query) const;

private:
    std::string id_;
    std::string name_;
    std::string email_;
    std::string url_;
    std::string username_;
    SecureString password_;
    std::string notes_;
    std::string totp_secret_;
    std::vector<PasswordHistoryEntry> password_history_;
    std::chrono::system_clock::time_point created_at_;
    std::chrono::system_clock::time_point modified_at_;
    std::string category_id_;
    int order_ = 0;

    void touch();
    void archive_password();
    static std::string time_to_string(std::chrono::system_clock::time_point tp);
    static std::chrono::system_clock::time_point string_to_time(const std::string& str);
};

} // namespace pasgen
