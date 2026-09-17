#pragma once

#include "imgui.h"
#include <string>
#include <vector>

namespace pasgen {

struct FontOption {
    std::string id;    // stable id persisted in config
    std::string label; // display name in the Preferences combo
    std::string file;  // filename inside the bundled fonts directory; empty = built-in bitmap font
};

// The curated set of fonts bundled next to the executable.
const std::vector<FontOption>& available_fonts();
const FontOption* find_font(const std::string& id);

// A typographic scale built from one user-chosen family and base size.
//
// Headings are real atlas entries rather than ImGui::SetWindowFontScale on the
// body font: scaling a rasterized atlas resamples the glyphs and looks soft and
// muddy, which is most of what makes a UI read as unpolished.
struct FontSet {
    ImFont* body    = nullptr;  // default copy, inputs, buttons
    ImFont* caption = nullptr;  // labels, hints, meta lines
    ImFont* h1      = nullptr;  // screen titles
    ImFont* h2      = nullptr;  // card and section titles
    ImFont* mono    = nullptr;  // passwords, TOTP codes, anything character-exact
};

const FontSet& fonts();

// Rebuilds the ImGui font atlas (io.Fonts) for the given font id + base pixel
// size, loading from fonts_dir, and repopulates fonts(). Falls back to the
// built-in font if the id is unknown or the file can't be loaded. The caller
// must tell the render backend to re-upload the font texture afterward (e.g.
// ImGui_ImplOpenGL3_DestroyFontsTexture/CreateFontsTexture).
void apply_font(const std::string& id, int size_px, const std::string& fonts_dir);

} // namespace pasgen
