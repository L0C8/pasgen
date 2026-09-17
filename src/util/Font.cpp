#include "util/Font.hpp"
#include <filesystem>

namespace pasgen {

namespace {
FontSet g_fonts;

// ImGui asserts rather than returning null when a font file is missing, so
// existence is checked up front and a missing file degrades to the built-in.
bool readable(const std::string& path) {
    std::error_code ec;
    return !path.empty() && std::filesystem::is_regular_file(path, ec);
}

// Rounded so every step lands on a whole pixel; fractional atlas sizes
// rasterize with uneven stem weights.
int scaled(int base, int delta, int min_px) {
    int v = base + delta;
    return v < min_px ? min_px : v;
}
} // namespace

const std::vector<FontOption>& available_fonts() {
    static const std::vector<FontOption> fonts = {
        {"roboto",      "Roboto",              "Roboto-Medium.ttf"},
        {"karla",       "Karla",               "Karla-Regular.ttf"},
        {"droid",       "Droid Sans",          "DroidSans.ttf"},
        {"cousine",     "Cousine (Monospace)", "Cousine-Regular.ttf"},
        // Deliberately not called "default": older configs stored "default"
        // for the built-in bitmap face, and that value is migrated to Roboto.
        {"builtin",     "Built-in (Pixel)",    ""},
        {"proggy_tiny", "Proggy Tiny (Small)", "ProggyTiny.ttf"},
    };
    return fonts;
}

const FontOption* find_font(const std::string& id) {
    for (const auto& f : available_fonts())
        if (f.id == id) return &f;
    return nullptr;
}

const FontSet& fonts() { return g_fonts; }

void apply_font(const std::string& id, int size_px, const std::string& fonts_dir) {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    g_fonts = FontSet{};

    const FontOption* opt = find_font(id);
    std::string path = (opt && !opt->file.empty()) ? fonts_dir + opt->file : std::string();
    if (!readable(path)) path.clear();

    // Adds the chosen family at one size, falling back to the built-in bitmap
    // font if the TTF is missing so a bad install degrades instead of crashing.
    auto add = [&](int px) -> ImFont* {
        if (!path.empty()) {
            if (ImFont* f = io.Fonts->AddFontFromFileTTF(path.c_str(), (float)px))
                return f;
        }
        ImFontConfig cfg;
        cfg.SizePixels = (float)px;
        return io.Fonts->AddFontDefault(&cfg);
    };

    g_fonts.body    = add(size_px);
    g_fonts.caption = add(scaled(size_px, -2, 9));
    g_fonts.h2      = add(scaled(size_px, +4, 12));
    g_fonts.h1      = add(scaled(size_px, +11, 18));

    // Passwords and TOTP codes are read character by character, so they always
    // get a monospace face regardless of the chosen body family.
    const std::string mono_path = fonts_dir + "Cousine-Regular.ttf";
    g_fonts.mono = readable(mono_path)
        ? io.Fonts->AddFontFromFileTTF(mono_path.c_str(), (float)size_px)
        : nullptr;
    if (!g_fonts.mono) g_fonts.mono = g_fonts.body;

    io.Fonts->Build();
    io.FontDefault = g_fonts.body;
}

} // namespace pasgen
