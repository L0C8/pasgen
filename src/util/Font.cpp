#include "util/Font.hpp"
#include "imgui.h"

namespace pasgen {

const std::vector<FontOption>& available_fonts() {
    static const std::vector<FontOption> fonts = {
        {"default",     "Default (Pixel)",     ""},
        {"roboto",      "Roboto",               "Roboto-Medium.ttf"},
        {"droid",       "Droid Sans",           "DroidSans.ttf"},
        {"karla",       "Karla",                "Karla-Regular.ttf"},
        {"cousine",     "Cousine (Monospace)",  "Cousine-Regular.ttf"},
        {"proggy_tiny", "Proggy Tiny (Small)",  "ProggyTiny.ttf"},
    };
    return fonts;
}

const FontOption* find_font(const std::string& id) {
    for (const auto& f : available_fonts())
        if (f.id == id) return &f;
    return nullptr;
}

void apply_font(const std::string& id, int size_px, const std::string& fonts_dir) {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    const FontOption* opt = find_font(id);
    ImFont* loaded = nullptr;
    if (opt && !opt->file.empty()) {
        std::string path = fonts_dir + opt->file;
        loaded = io.Fonts->AddFontFromFileTTF(path.c_str(), (float)size_px);
    }
    if (!loaded) {
        ImFontConfig cfg;
        cfg.SizePixels = (float)size_px;
        io.Fonts->AddFontDefault(&cfg);
    }
    io.Fonts->Build();
}

} // namespace pasgen
