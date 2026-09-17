#include "util/Theme.hpp"

namespace pasgen {

namespace {

// 0xRRGGBB literals keep the palettes below readable as a design spec rather
// than as a wall of float triples.
constexpr ImVec4 rgb(unsigned hex, float a = 1.0f) {
    return ImVec4(((hex >> 16) & 0xFF) / 255.0f,
                  ((hex >> 8)  & 0xFF) / 255.0f,
                  ( hex        & 0xFF) / 255.0f,
                  a);
}

ImVec4 with_alpha(ImVec4 c, float a) { c.w = a; return c; }

Palette make_dark() {
    Palette p{};
    p.bg            = rgb(0x0E1014);
    p.surface       = rgb(0x15181E);
    p.surface_alt   = rgb(0x1B1F27);
    p.surface_hover = rgb(0x222732);
    p.border        = rgb(0x272C36);
    p.border_strong = rgb(0x39404E);

    p.text          = rgb(0xE7EAF0);
    p.text_dim      = rgb(0x9AA3B4);
    p.text_muted    = rgb(0x6A7385);

    p.accent        = rgb(0x4C7DF0);
    p.accent_hover  = rgb(0x5F8CF7);
    p.accent_active = rgb(0x3B68D6);
    p.accent_soft   = with_alpha(rgb(0x4C7DF0), 0.24f);
    p.on_accent     = rgb(0xFFFFFF);

    p.success       = rgb(0x3ED598);
    p.warning       = rgb(0xF2B43C);
    p.danger        = rgb(0xF05C68);
    p.danger_hover  = rgb(0xF87B85);

    const unsigned cats[8] = {0xE8894A, 0x4E9BF5, 0x3ECF8E, 0xAE7BEE,
                              0xE8C44E, 0xF06B72, 0x35C4C4, 0x93A0B5};
    for (int i = 0; i < 8; ++i) p.category[i] = rgb(cats[i]);
    return p;
}

Palette make_light() {
    Palette p{};
    p.bg            = rgb(0xF4F6F9);
    p.surface       = rgb(0xFFFFFF);
    p.surface_alt   = rgb(0xF2F4F7);
    p.surface_hover = rgb(0xE9EDF3);
    p.border        = rgb(0xDFE3EA);
    p.border_strong = rgb(0xC2C9D6);

    p.text          = rgb(0x141922);
    p.text_dim      = rgb(0x4E5766);
    p.text_muted    = rgb(0x858E9E);

    p.accent        = rgb(0x2F6BE0);
    p.accent_hover  = rgb(0x3F7BF0);
    p.accent_active = rgb(0x2455BE);
    p.accent_soft   = with_alpha(rgb(0x2F6BE0), 0.16f);
    p.on_accent     = rgb(0xFFFFFF);

    p.success       = rgb(0x12855A);
    p.warning       = rgb(0x9A6800);
    p.danger        = rgb(0xCE3A47);
    p.danger_hover  = rgb(0xE04C58);

    // Deeper than the dark theme's swatches so they stay legible on white.
    const unsigned cats[8] = {0xC2661E, 0x2A6FD0, 0x15855C, 0x7B4BC4,
                              0x9A7500, 0xC63641, 0x107C7C, 0x5E6878};
    for (int i = 0; i < 8; ++i) p.category[i] = rgb(cats[i]);
    return p;
}

Palette g_palette = make_dark();
Theme   g_theme   = Theme::Dark;

// Shape and spacing are shared by both variants: only color changes between
// themes, so the two never diverge in layout or density.
void apply_shape(ImGuiStyle& s) {
    s.WindowRounding    = 10.0f;
    s.ChildRounding     = 10.0f;
    s.FrameRounding     = 7.0f;
    s.PopupRounding     = 12.0f;
    s.ScrollbarRounding = 10.0f;
    s.GrabRounding      = 7.0f;
    s.TabRounding       = 7.0f;

    s.WindowBorderSize  = 1.0f;
    s.ChildBorderSize   = 1.0f;
    s.PopupBorderSize   = 1.0f;
    s.FrameBorderSize   = 1.0f;
    s.TabBorderSize     = 0.0f;

    s.WindowPadding     = ImVec2(18, 16);
    s.FramePadding      = ImVec2(11, 8);
    s.ItemSpacing       = ImVec2(9, 9);
    s.ItemInnerSpacing  = ImVec2(8, 6);
    s.CellPadding       = ImVec2(8, 6);
    s.IndentSpacing     = 18.0f;
    s.ScrollbarSize     = 12.0f;
    s.GrabMinSize       = 12.0f;

    s.WindowTitleAlign     = ImVec2(0.0f, 0.5f);
    s.ButtonTextAlign      = ImVec2(0.5f, 0.5f);
    s.SelectableTextAlign  = ImVec2(0.0f, 0.5f);
    s.SeparatorTextBorderSize = 1.0f;
    s.SeparatorTextAlign   = ImVec2(0.0f, 0.5f);
    s.SeparatorTextPadding = ImVec2(0, 8);
    s.DisabledAlpha        = 0.45f;
    s.AntiAliasedLines     = true;
    s.AntiAliasedFill      = true;
}

void apply_colors(ImGuiStyle& s, const Palette& p) {
    ImVec4* c = s.Colors;

    c[ImGuiCol_Text]                 = p.text;
    c[ImGuiCol_TextDisabled]         = p.text_muted;
    c[ImGuiCol_TextSelectedBg]       = p.accent_soft;

    c[ImGuiCol_WindowBg]             = p.bg;
    c[ImGuiCol_ChildBg]              = p.surface;
    c[ImGuiCol_PopupBg]              = p.surface;
    c[ImGuiCol_MenuBarBg]            = p.bg;

    c[ImGuiCol_Border]               = p.border;
    c[ImGuiCol_BorderShadow]         = ImVec4(0, 0, 0, 0);

    c[ImGuiCol_FrameBg]              = p.surface_alt;
    c[ImGuiCol_FrameBgHovered]       = p.surface_hover;
    c[ImGuiCol_FrameBgActive]        = p.surface_hover;

    c[ImGuiCol_TitleBg]              = p.surface;
    c[ImGuiCol_TitleBgActive]        = p.surface;
    c[ImGuiCol_TitleBgCollapsed]     = p.surface;

    c[ImGuiCol_Button]               = p.surface_alt;
    c[ImGuiCol_ButtonHovered]        = p.surface_hover;
    c[ImGuiCol_ButtonActive]         = p.border;

    c[ImGuiCol_Header]               = p.accent_soft;
    c[ImGuiCol_HeaderHovered]        = p.surface_hover;
    c[ImGuiCol_HeaderActive]         = p.accent_soft;

    c[ImGuiCol_Separator]            = p.border;
    c[ImGuiCol_SeparatorHovered]     = p.border_strong;
    c[ImGuiCol_SeparatorActive]      = p.accent;

    c[ImGuiCol_ResizeGrip]           = p.border;
    c[ImGuiCol_ResizeGripHovered]    = p.border_strong;
    c[ImGuiCol_ResizeGripActive]     = p.accent;

    c[ImGuiCol_ScrollbarBg]          = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab]        = p.border;
    c[ImGuiCol_ScrollbarGrabHovered] = p.border_strong;
    c[ImGuiCol_ScrollbarGrabActive]  = p.text_muted;

    c[ImGuiCol_CheckMark]            = p.accent;
    c[ImGuiCol_SliderGrab]           = p.accent;
    c[ImGuiCol_SliderGrabActive]     = p.accent_active;

    c[ImGuiCol_Tab]                  = p.surface_alt;
    c[ImGuiCol_TabHovered]           = p.surface_hover;
    c[ImGuiCol_TabSelected]          = p.surface;
    c[ImGuiCol_TabDimmed]            = p.surface_alt;
    c[ImGuiCol_TabDimmedSelected]    = p.surface;

    c[ImGuiCol_PlotLines]            = p.accent;
    c[ImGuiCol_PlotLinesHovered]     = p.accent_hover;
    c[ImGuiCol_PlotHistogram]        = p.accent;
    c[ImGuiCol_PlotHistogramHovered] = p.accent_hover;

    c[ImGuiCol_TableHeaderBg]        = p.surface_alt;
    c[ImGuiCol_TableBorderStrong]    = p.border_strong;
    c[ImGuiCol_TableBorderLight]     = p.border;
    c[ImGuiCol_TableRowBg]           = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt]        = with_alpha(p.surface_alt, 0.5f);

    c[ImGuiCol_DragDropTarget]       = p.accent;
    c[ImGuiCol_NavHighlight]         = p.accent;
    c[ImGuiCol_NavWindowingHighlight] = p.accent;
    c[ImGuiCol_NavWindowingDimBg]    = ImVec4(0, 0, 0, 0.45f);
    c[ImGuiCol_ModalWindowDimBg]     = ImVec4(0, 0, 0, 0.55f);
}

} // namespace

const Palette& palette()   { return g_palette; }
Theme current_theme()      { return g_theme; }

Theme theme_from_string(const std::string& s) {
    if (s == "light") return Theme::Light;
    // Everything else — including the retired retro/XP theme ids from older
    // configs — lands on the default dark theme.
    return Theme::Dark;
}

std::string theme_to_string(Theme t) {
    return t == Theme::Light ? "light" : "dark";
}

void apply_theme(Theme t) {
    g_theme   = t;
    g_palette = (t == Theme::Light) ? make_light() : make_dark();

    ImGuiStyle& style = ImGui::GetStyle();
    apply_shape(style);
    apply_colors(style, g_palette);
}

} // namespace pasgen
