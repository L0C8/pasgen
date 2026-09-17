#pragma once

#include "imgui.h"
#include <string>

namespace pasgen {

// Pasgen ships a single, deliberately-designed look in two variants. Everything
// visual is derived from the Palette below, so the two variants stay in step and
// nothing has to be restyled per-theme at the call site.
enum class Theme {
    Dark  = 0,
    Light = 1,
};

Theme theme_from_string(const std::string& s);
std::string theme_to_string(Theme t);

// Semantic design tokens for the active theme.
//
// Screens and widgets read colors from here instead of hardcoding literals:
// a hardcoded light-gray label is invisible on the Light theme, and every such
// literal is a place the two themes can silently drift apart.
struct Palette {
    // Surfaces, from furthest back to closest to the user.
    ImVec4 bg;             // window background behind all panels
    ImVec4 surface;        // panels and cards sitting on bg
    ImVec4 surface_alt;    // inputs and inset wells
    ImVec4 surface_hover;  // hover state for rows and ghost buttons
    ImVec4 border;         // hairline separators and card outlines
    ImVec4 border_strong;  // emphasized outlines (focus, active input)

    // Type, in descending emphasis.
    ImVec4 text;           // primary copy and values
    ImVec4 text_dim;       // field labels and secondary copy
    ImVec4 text_muted;     // hints, placeholders, disabled

    // Accent, used for primary actions and selection.
    ImVec4 accent;
    ImVec4 accent_hover;
    ImVec4 accent_active;
    ImVec4 accent_soft;    // low-alpha accent for selected rows
    ImVec4 on_accent;      // text drawn on top of an accent fill

    // Status.
    ImVec4 success;
    ImVec4 warning;
    ImVec4 danger;
    ImVec4 danger_hover;

    // Category swatches, cycled by position in the sidebar. Tuned per theme so
    // they stay legible against that theme's surface color.
    ImVec4 category[8];
};

const Palette& palette();
Theme current_theme();

// Applies the palette and all shape/spacing style vars for the given theme to
// the current ImGui context.
void apply_theme(Theme t);

} // namespace pasgen
