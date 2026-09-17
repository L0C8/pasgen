#pragma once

#include "imgui.h"
#include <string>

namespace pasgen {

// Animated wallpaper shown behind the login/create-database cards (the
// "home page" screens, before a vault is open). Purely decorative — chosen
// in Settings > Preferences > Appearance and persisted like the UI theme.
enum class HomeBackground {
    None   = 0,
    Matrix = 1,  // blue digital rain
    Outrun = 2,  // synthwave perspective grid + sun
};

HomeBackground home_background_from_string(const std::string& s);
std::string home_background_to_string(HomeBackground b);

// Draws the chosen background full-screen, behind every ImGui window (via
// ImGui::GetBackgroundDrawList()), so callers can invoke it before or after
// building the rest of the frame. A no-op for HomeBackground::None.
//
// `dt` is added to the effect's internal clock, so pass ImGui's DeltaTime;
// the clock (and the matrix rain's column state) lives in function-local
// statics, not here, since only one home screen is ever visible at a time.
void render_home_background(HomeBackground bg, float w, float h, float dt);

} // namespace pasgen
