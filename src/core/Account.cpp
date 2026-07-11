#ifdef _WIN32
#define timegm _mkgmtime
#endif

#include "core/Account.hpp"
#include <algorithm>
#include <iomanip>
#include <random>
#include <sstream>

namespace pasgen {

namespace {

std::string time_to_iso(std::chrono::system_clock::time_point tp) {
    auto t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = *std::gmtime(&t);
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::chrono::system_clock::time_point iso_to_time(const std::string& str) {
    std::tm tm = {};
    std::istringstream ss(str);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    if (ss.fail()) return std::chrono::system_clock::now();
    return std::chrono::system_clock::from_time_t(timegm(&tm));
}

} // namespace

nlohmann::json PasswordHistoryEntry::to_json() const {
    return {{"password", std::string(password.c_str())}, {"changed_at", time_to_iso(changed_at)}};
}

PasswordHistoryEntry PasswordHistoryEntry::from_json(const nlohmann::json& j) {
    PasswordHistoryEntry e;
    if (j.contains("password"))   e.password   = SecureString(j["password"].get<std::string>());
    if (j.contains("changed_at")) e.changed_at = iso_to_time(j["changed_at"].get<std::string>());
    return e;
}

Account::Account() {
    id_ = generate_uuid();
    created_at_ = modified_at_ = std::chrono::system_clock::now();
}

Account::Account(const nlohmann::json& j) { *this = from_json(j); }

void Account::set_name(const std::string& v)  { name_ = v;   touch(); }
void Account::set_email(const std::string& v) { email_ = v;  touch(); }
void Account::set_url(const std::string& v)   { url_ = v;    touch(); }
void Account::set_username(const std::string& v) { username_ = v; touch(); }
void Account::set_notes(const std::string& v) { notes_ = v;  touch(); }
void Account::set_totp_secret(const std::string& v) { totp_secret_ = v; touch(); }

void Account::set_password(const SecureString& password) {
    archive_password();
    password_ = password;
    touch();
}

nlohmann::json Account::to_json() const {
    nlohmann::json j;
    j["id"]          = id_;
    j["name"]        = name_;
    j["email"]       = email_;
    j["url"]         = url_;
    j["username"]    = username_;
    j["password"]    = std::string(password_.c_str());
    j["notes"]       = notes_;
    j["totp_secret"] = totp_secret_;
    j["created_at"]  = time_to_string(created_at_);
    j["modified_at"] = time_to_string(modified_at_);

    nlohmann::json hist = nlohmann::json::array();
    for (const auto& e : password_history_) {
        hist.push_back({
            {"password",   std::string(e.password.c_str())},
            {"changed_at", time_to_string(e.changed_at)}
        });
    }
    j["password_history"] = hist;
    return j;
}

Account Account::from_json(const nlohmann::json& j) {
    Account a;
    if (j.contains("id"))          a.id_          = j["id"].get<std::string>();
    if (j.contains("name"))        a.name_        = j["name"].get<std::string>();
    if (j.contains("email"))       a.email_       = j["email"].get<std::string>();
    if (j.contains("url"))         a.url_         = j["url"].get<std::string>();
    if (j.contains("username"))    a.username_    = j["username"].get<std::string>();
    if (j.contains("password"))    a.password_    = SecureString(j["password"].get<std::string>());
    if (j.contains("notes"))       a.notes_       = j["notes"].get<std::string>();
    if (j.contains("totp_secret")) a.totp_secret_ = j["totp_secret"].get<std::string>();
    if (j.contains("created_at"))  a.created_at_  = string_to_time(j["created_at"].get<std::string>());
    if (j.contains("modified_at")) a.modified_at_ = string_to_time(j["modified_at"].get<std::string>());

    if (j.contains("password_history") && j["password_history"].is_array()) {
        for (const auto& ej : j["password_history"]) {
            PasswordHistoryEntry e;
            if (ej.contains("password"))   e.password   = SecureString(ej["password"].get<std::string>());
            if (ej.contains("changed_at")) e.changed_at = string_to_time(ej["changed_at"].get<std::string>());
            a.password_history_.push_back(std::move(e));
        }
    }
    return a;
}

bool Account::matches_search(const std::string& query) const {
    if (query.empty()) return true;
    std::string lq = query;
    std::transform(lq.begin(), lq.end(), lq.begin(), ::tolower);
    auto has = [&](const std::string& s) {
        std::string ls = s;
        std::transform(ls.begin(), ls.end(), ls.begin(), ::tolower);
        return ls.find(lq) != std::string::npos;
    };
    return has(name_) || has(email_) || has(url_) || has(username_) || has(notes_);
}

void Account::touch()            { modified_at_ = std::chrono::system_clock::now(); }
std::string Account::time_to_string(std::chrono::system_clock::time_point tp) { return time_to_iso(tp); }
std::chrono::system_clock::time_point Account::string_to_time(const std::string& s) { return iso_to_time(s); }

void Account::archive_password() {
    if (!password_.empty()) {
        PasswordHistoryEntry e;
        e.password   = password_;
        e.changed_at = modified_at_;
        password_history_.insert(password_history_.begin(), std::move(e));
        if (password_history_.size() > 10)
            password_history_.resize(10);
    }
}

std::string Account::generate_uuid() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> d(0, 15), d2(8, 11);
    std::ostringstream ss;
    ss << std::hex;
    for (int i = 0; i < 8;  i++) ss << d(gen);
    ss << "-";
    for (int i = 0; i < 4;  i++) ss << d(gen);
    ss << "-4";
    for (int i = 0; i < 3;  i++) ss << d(gen);
    ss << "-";
    ss << d2(gen);
    for (int i = 0; i < 3;  i++) ss << d(gen);
    ss << "-";
    for (int i = 0; i < 12; i++) ss << d(gen);
    return ss.str();
}

} // namespace pasgen
