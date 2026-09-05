#include "util/Config.hpp"
#include <fstream>
#include <filesystem>
#include <iostream>

#ifdef _WIN32
#include <cstdlib>
#endif

namespace pasgen {

Config& Config::instance() {
    static Config c;
    return c;
}

Config::Config() {
    config_file_path_ = get_config_path();
    load();
}

std::string Config::get_config_path() const {
    std::string dir;

#if defined(_WIN32)
    const char* appdata = std::getenv("APPDATA");
    dir = std::string(appdata ? appdata : ".") + "\\pasgen";
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    dir = std::string(home ? home : ".") + "/Library/Application Support/pasgen";
#else
    // Linux: XDG_CONFIG_HOME or ~/.config
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0]) {
        dir = std::string(xdg) + "/pasgen";
    } else {
        const char* home = std::getenv("HOME");
        dir = std::string(home ? home : "/tmp") + "/.config/pasgen";
    }
#endif

    std::filesystem::create_directories(dir);
    return dir +
#ifdef _WIN32
        "\\config.json";
#else
        "/config.json";
#endif
}

void Config::load() {
    try {
        if (std::filesystem::exists(config_file_path_)) {
            std::ifstream f(config_file_path_);
            if (f.is_open()) f >> data_;
        }
    } catch (const std::exception& e) {
        std::cerr << "Warning: failed to load config: " << e.what() << "\n";
        data_ = nlohmann::json::object();
    }
}

void Config::save() {
    try {
        std::ofstream f(config_file_path_);
        if (f.is_open()) f << data_.dump(2);
    } catch (const std::exception& e) {
        std::cerr << "Warning: failed to save config: " << e.what() << "\n";
    }
}

std::string Config::get_last_database_path() const {
    if (data_.contains("last_database_path") && data_["last_database_path"].is_string())
        return data_["last_database_path"];
    return "";
}

void Config::set_last_database_path(const std::string& path) {
    data_["last_database_path"] = path;
    save();
}

std::string Config::get_theme() const {
    if (data_.contains("theme") && data_["theme"].is_string())
        return data_["theme"];
    return "dark";
}

std::string Config::get_font_id() const {
    if (data_.contains("font_id") && data_["font_id"].is_string()) {
        std::string id = data_["font_id"];
        // Older builds stored "default" for the built-in bitmap face and used
        // it as the default. That value is migrated to the proportional
        // default; choosing the pixel font today stores "builtin" and sticks.
        if (id != "default") return id;
    }
    return "roboto";
}

void Config::set_font_id(const std::string& id) {
    data_["font_id"] = id;
    save();
}

int Config::get_font_size() const {
    if (data_.contains("font_size") && data_["font_size"].is_number_integer())
        return data_["font_size"];
    return 16;
}

void Config::set_font_size(int size) {
    data_["font_size"] = size;
    save();
}

void Config::set_theme(const std::string& theme) {
    data_["theme"] = theme;
    save();
}

int Config::get_autolock_minutes() const {
    if (data_.contains("autolock_minutes") && data_["autolock_minutes"].is_number_integer())
        return data_["autolock_minutes"].get<int>();
    return 5;
}

void Config::set_autolock_minutes(int minutes) {
    data_["autolock_minutes"] = minutes;
    save();
}

int Config::get_clipboard_clear_seconds() const {
    if (data_.contains("clipboard_clear_seconds") && data_["clipboard_clear_seconds"].is_number_integer())
        return data_["clipboard_clear_seconds"].get<int>();
    return 30;
}

void Config::set_clipboard_clear_seconds(int seconds) {
    data_["clipboard_clear_seconds"] = seconds;
    save();
}

PasswordGenDefaults Config::get_password_gen_defaults() const {
    PasswordGenDefaults d;
    if (data_.contains("password_gen") && data_["password_gen"].is_object()) {
        const auto& g = data_["password_gen"];
        if (g.contains("length")) d.length = g["length"];
        if (g.contains("lowercase")) d.lowercase = g["lowercase"];
        if (g.contains("uppercase")) d.uppercase = g["uppercase"];
        if (g.contains("digits")) d.digits = g["digits"];
        if (g.contains("symbols")) d.symbols = g["symbols"];
        if (g.contains("passphrase")) d.passphrase = g["passphrase"];
        if (g.contains("words")) d.words = g["words"];
        if (g.contains("separator")) d.separator = g["separator"];
    }
    return d;
}

void Config::set_password_gen_defaults(const PasswordGenDefaults& d) {
    data_["password_gen"] = {
        {"length",     d.length},
        {"lowercase",  d.lowercase},
        {"uppercase",  d.uppercase},
        {"digits",     d.digits},
        {"symbols",    d.symbols},
        {"passphrase", d.passphrase},
        {"words",      d.words},
        {"separator",  d.separator},
    };
    save();
}

} // namespace pasgen
