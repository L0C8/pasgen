#pragma once

#include "imgui.h"
#include <string>

namespace pasgen {

// The "Look and Feel" is the widget skin/engine: Modern uses normal ImGui
// widgets, Retro uses hand-drawn bevelled Win95-era widgets, and Windows XP
// uses hand-drawn glossy "Luna" style widgets. Theme is the color variant
// nested under whichever look and feel is active.
enum class LookAndFeel {
    Modern    = 0,
    Retro     = 1,
    WindowsXP = 2,
};

enum class Theme {
    Original   = 0,
    Light      = 1,
    Classic    = 2,
    RetroWin95 = 3,
    RetroBlue  = 4,
    RetroWave  = 5,
    RetroDark  = 6,
    XPBlue     = 7,
    XPSilver   = 8,
    XPOlive    = 9,
};

Theme theme_from_string(const std::string& s);
std::string theme_to_string(Theme t);
const char* theme_label(Theme t);

LookAndFeel look_and_feel_of(Theme t);
const char* look_and_feel_label(LookAndFeel l);
// The default/first theme belonging to a look-and-feel family (used when
// switching families in the Preferences UI).
Theme default_theme_for(LookAndFeel l);

// Applies colors/style vars for the given theme to the current ImGui context.
void apply_theme(Theme t);

// Hand-drawn bevelled retro widgets used by the Retro look and feel.
bool RetroButton(const char* label, ImVec2 size = ImVec2(0, 0));
bool RetroSmallButton(const char* label);

// Hand-drawn glossy "Luna" widgets used by the Windows XP look and feel.
bool XPButton(const char* label, ImVec2 size = ImVec2(0, 0));
bool XPSmallButton(const char* label);

} // namespace pasgen
