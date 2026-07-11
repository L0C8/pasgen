#pragma once

#include <string>
#include <vector>

namespace pasgen {

struct FontOption {
    std::string id;    // stable id persisted in config
    std::string label; // display name in the Preferences combo
    std::string file;  // filename inside the bundled fonts directory; empty = built-in default font
};

// The curated set of fonts bundled next to the executable.
const std::vector<FontOption>& available_fonts();
const FontOption* find_font(const std::string& id);

// Rebuilds the ImGui font atlas (io.Fonts) for the given font id + pixel
// size, loading from fonts_dir. Falls back to the built-in default font if
// the id is unknown or the file can't be loaded. The caller is responsible
// for telling the render backend to re-upload the font texture afterward
// (e.g. ImGui_ImplOpenGL3_DestroyFontsTexture/CreateFontsTexture).
void apply_font(const std::string& id, int size_px, const std::string& fonts_dir);

} // namespace pasgen
