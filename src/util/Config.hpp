#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace pasgen {

struct PasswordGenDefaults {
    int  length       = 16;
    bool lowercase    = true;
    bool uppercase    = true;
    bool digits       = true;
    bool symbols      = true;
    bool passphrase   = false;
    int  words        = 4;
    std::string separator = "-";
};

class Config {
public:
    static Config& instance();

    std::string get_last_database_path() const;
    void set_last_database_path(const std::string& path);

    std::string get_theme() const;
    void set_theme(const std::string& theme);

    std::string get_font_id() const;
    void set_font_id(const std::string& id);
    int get_font_size() const;
    void set_font_size(int size);

    // Minutes of inactivity before the vault relocks; 0 disables.
    int  get_autolock_minutes() const;
    void set_autolock_minutes(int minutes);
    // Seconds before a copied secret is wiped from the clipboard; 0 disables.
    int  get_clipboard_clear_seconds() const;
    void set_clipboard_clear_seconds(int seconds);

    PasswordGenDefaults get_password_gen_defaults() const;
    void set_password_gen_defaults(const PasswordGenDefaults& d);

private:
    Config();
    void load();
    void save();
    std::string get_config_path() const;

    nlohmann::json data_;
    std::string config_file_path_;
};

} // namespace pasgen
