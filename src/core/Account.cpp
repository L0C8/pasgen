#ifdef _WIN32
#define timegm _mkgmtime
#endif

#include "core/Account.hpp"
#include "util/Uuid.hpp"
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

nlohmann::json CustomField::to_json() const {
    return {{"name", name}, {"value", std::string(value.c_str())}, {"secret", secret}};
}

CustomField CustomField::from_json(const nlohmann::json& j) {
    CustomField f;
    if (j.contains("name"))   f.name   = j["name"].get<std::string>();
    if (j.contains("value"))  f.value  = SecureString(j["value"].get<std::string>());
    if (j.contains("secret")) f.secret = j["secret"].get<bool>();
    return f;
}

Account::Account() {
    id_ = generate_uuid();
    created_at_ = modified_at_ = password_changed_at_ = std::chrono::system_clock::now();
}

Account::Account(const nlohmann::json& j) { *this = from_json(j); }

void Account::set_name(const std::string& v)  { name_ = v;   touch(); }
void Account::set_email(const std::string& v) { email_ = v;  touch(); }
void Account::set_url(const std::string& v)   { url_ = v;    touch(); }
void Account::set_username(const std::string& v) { username_ = v; touch(); }
void Account::set_notes(const std::string& v) { notes_ = v;  touch(); }
void Account::set_totp_secret(const std::string& v) { totp_secret_ = v; touch(); }
void Account::set_category_id(const std::string& v) { category_id_ = v; touch(); }
void Account::set_order(int v) { order_ = v; touch(); }
void Account::set_favorite(bool v) { favorite_ = v; touch(); }
void Account::set_archived(bool v) { archived_ = v; touch(); }

void Account::set_password(const SecureString& password) {
    // Writing back an unchanged value must not touch the history. Without this
    // guard a caller that writes on every edit would churn the 10-entry history
    // into prefixes of whatever is being typed, destroying the real entries.
    if (password == password_) return;
    archive_password();
    password_ = password;
    password_changed_at_ = std::chrono::system_clock::now();
    touch();
}

int Account::password_age_days() const {
    auto delta = std::chrono::system_clock::now() - password_changed_at_;
    auto days  = std::chrono::duration_cast<std::chrono::hours>(delta).count() / 24;
    return days < 0 ? 0 : static_cast<int>(days);
}

void Account::add_custom_field(const std::string& name, const SecureString& value, bool secret) {
    CustomField f;
    f.name = name; f.value = value; f.secret = secret;
    custom_fields_.push_back(std::move(f));
    touch();
}

void Account::set_custom_field(size_t index, const std::string& name,
                               const SecureString& value, bool secret) {
    if (index >= custom_fields_.size()) return;
    CustomField& f = custom_fields_[index];
    if (f.name == name && f.value == value && f.secret == secret) return;
    f.name = name; f.value = value; f.secret = secret;
    touch();
}

void Account::remove_custom_field(size_t index) {
    if (index >= custom_fields_.size()) return;
    custom_fields_.erase(custom_fields_.begin() + static_cast<std::ptrdiff_t>(index));
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
    j["category_id"] = category_id_;
    j["order"]        = order_;
    j["favorite"]    = favorite_;
    j["archived"]    = archived_;
    j["password_changed_at"] = time_to_string(password_changed_at_);

    nlohmann::json cfs = nlohmann::json::array();
    for (const auto& f : custom_fields_) cfs.push_back(f.to_json());
    j["custom_fields"] = cfs;

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
    if (j.contains("category_id")) a.category_id_ = j["category_id"].get<std::string>();
    if (j.contains("order"))       a.order_       = j["order"].get<int>();
    if (j.contains("favorite"))    a.favorite_    = j["favorite"].get<bool>();
    if (j.contains("archived"))    a.archived_    = j["archived"].get<bool>();
    // Databases written before this field existed fall back to created_at, so
    // password age stays meaningful rather than reading as "changed just now".
    a.password_changed_at_ = j.contains("password_changed_at")
        ? string_to_time(j["password_changed_at"].get<std::string>())
        : a.created_at_;

    if (j.contains("custom_fields") && j["custom_fields"].is_array()) {
        for (const auto& fj : j["custom_fields"])
            a.custom_fields_.push_back(CustomField::from_json(fj));
    }

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
    if (has(name_) || has(email_) || has(url_) || has(username_) || has(notes_))
        return true;
    for (const auto& f : custom_fields_) {
        // Names are searchable; values only when the user did not mark them
        // secret, so a search can never surface a hidden value.
        if (has(f.name)) return true;
        if (!f.secret && has(std::string(f.value.c_str()))) return true;
    }
    return false;
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

} // namespace pasgen
