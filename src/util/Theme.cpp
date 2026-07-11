#include "util/Theme.hpp"
#include "imgui.h"
#include "imgui_internal.h"

namespace pasgen {

LookAndFeel look_and_feel_of(Theme t) {
    switch (t) {
    case Theme::RetroWin95:
    case Theme::RetroBlue:
    case Theme::RetroWave:
    case Theme::RetroDark:
        return LookAndFeel::Retro;
    case Theme::XPBlue:
    case Theme::XPSilver:
    case Theme::XPOlive:
        return LookAndFeel::WindowsXP;
    default:
        return LookAndFeel::Modern;
    }
}

const char* look_and_feel_label(LookAndFeel l) {
    switch (l) {
    case LookAndFeel::Retro:     return "Retro";
    case LookAndFeel::WindowsXP: return "Windows XP";
    case LookAndFeel::Modern:
    default:                     return "Modern";
    }
}

Theme default_theme_for(LookAndFeel l) {
    switch (l) {
    case LookAndFeel::Retro:     return Theme::RetroWin95;
    case LookAndFeel::WindowsXP: return Theme::XPBlue;
    case LookAndFeel::Modern:
    default:                     return Theme::Original;
    }
}

Theme theme_from_string(const std::string& s) {
    if (s == "light")        return Theme::Light;
    if (s == "classic")      return Theme::Classic;
    // Current names
    if (s == "retro_win95")  return Theme::RetroWin95;
    if (s == "retro_blue")   return Theme::RetroBlue;
    if (s == "retro_wave")   return Theme::RetroWave;
    if (s == "retro_dark")   return Theme::RetroDark;
    if (s == "xp_blue")      return Theme::XPBlue;
    if (s == "xp_silver")    return Theme::XPSilver;
    if (s == "xp_olive")     return Theme::XPOlive;
    // Legacy names (theme family was previously called "Vapor")
    if (s == "vapor_win95")  return Theme::RetroWin95;
    if (s == "vapor_blue")   return Theme::RetroBlue;
    if (s == "vapor_wave")   return Theme::RetroWave;
    return Theme::Original; // also covers legacy "dark" configs
}

std::string theme_to_string(Theme t) {
    switch (t) {
    case Theme::Light:      return "light";
    case Theme::Classic:    return "classic";
    case Theme::RetroWin95: return "retro_win95";
    case Theme::RetroBlue:  return "retro_blue";
    case Theme::RetroWave:  return "retro_wave";
    case Theme::RetroDark:  return "retro_dark";
    case Theme::XPBlue:     return "xp_blue";
    case Theme::XPSilver:   return "xp_silver";
    case Theme::XPOlive:    return "xp_olive";
    case Theme::Original:
    default:                return "original";
    }
}

const char* theme_label(Theme t) {
    switch (t) {
    case Theme::Light:      return "Light";
    case Theme::Classic:    return "Classic";
    case Theme::RetroWin95: return "Retro - Win95";
    case Theme::RetroBlue:  return "Retro - Blue";
    case Theme::RetroWave:  return "Retro - Wave";
    case Theme::RetroDark:  return "Retro - Dark";
    case Theme::XPBlue:     return "Windows XP - Blue";
    case Theme::XPSilver:   return "Windows XP - Silver";
    case Theme::XPOlive:    return "Windows XP - Olive Green";
    case Theme::Original:
    default:                return "Original";
    }
}

static void apply_shared_shape(ImGuiStyle& style) {
    style.WindowRounding    = 4.0f;
    style.FrameRounding     = 3.0f;
    style.ScrollbarRounding = 3.0f;
    style.GrabRounding      = 3.0f;
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 0.0f;
}

// Flat, square-cornered, chunky-bordered look reminiscent of Windows 95.
static void apply_retro_shape(ImGuiStyle& style) {
    style.WindowRounding    = 0.0f;
    style.FrameRounding     = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.GrabRounding      = 0.0f;
    style.WindowBorderSize  = 2.0f;
    style.FrameBorderSize   = 1.0f;
    style.PopupBorderSize   = 2.0f;
}

// Rounded, moderately bordered look reminiscent of Windows XP "Luna".
static void apply_xp_shape(ImGuiStyle& style) {
    style.WindowRounding    = 8.0f;
    style.FrameRounding     = 5.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding      = 5.0f;
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
}

void apply_theme(Theme t) {
    ImGuiStyle& style = ImGui::GetStyle();

    switch (t) {
    case Theme::Light: {
        ImGui::StyleColorsLight();
        apply_shared_shape(style);
        break;
    }
    case Theme::Classic: {
        ImGui::StyleColorsClassic();
        apply_shared_shape(style);
        break;
    }
    case Theme::RetroWin95: {
        // Classic Windows 95: battleship-gray chrome, navy title/selection blue,
        // black text, square corners, hard borders.
        ImGui::StyleColorsClassic();
        apply_retro_shape(style);
        style.Colors[ImGuiCol_Text]           = {0.00f, 0.00f, 0.00f, 1.00f};
        style.Colors[ImGuiCol_WindowBg]       = {0.75f, 0.75f, 0.75f, 1.00f};
        style.Colors[ImGuiCol_ChildBg]        = {0.75f, 0.75f, 0.75f, 1.00f};
        style.Colors[ImGuiCol_PopupBg]        = {0.75f, 0.75f, 0.75f, 1.00f};
        style.Colors[ImGuiCol_Border]         = {0.30f, 0.30f, 0.30f, 1.00f};
        style.Colors[ImGuiCol_FrameBg]        = {1.00f, 1.00f, 1.00f, 1.00f};
        style.Colors[ImGuiCol_FrameBgHovered] = {0.90f, 0.90f, 0.90f, 1.00f};
        style.Colors[ImGuiCol_FrameBgActive]  = {0.85f, 0.85f, 0.85f, 1.00f};
        style.Colors[ImGuiCol_Button]         = {0.75f, 0.75f, 0.75f, 1.00f};
        style.Colors[ImGuiCol_ButtonHovered]  = {0.83f, 0.83f, 0.83f, 1.00f};
        style.Colors[ImGuiCol_ButtonActive]   = {0.65f, 0.65f, 0.65f, 1.00f};
        style.Colors[ImGuiCol_Header]         = {0.00f, 0.00f, 0.50f, 1.00f};
        style.Colors[ImGuiCol_HeaderHovered]  = {0.00f, 0.00f, 0.65f, 1.00f};
        style.Colors[ImGuiCol_HeaderActive]   = {0.00f, 0.00f, 0.80f, 1.00f};
        style.Colors[ImGuiCol_TitleBg]        = {0.00f, 0.00f, 0.50f, 1.00f};
        style.Colors[ImGuiCol_TitleBgActive]  = {0.00f, 0.00f, 0.50f, 1.00f};
        style.Colors[ImGuiCol_MenuBarBg]      = {0.75f, 0.75f, 0.75f, 1.00f};
        style.Colors[ImGuiCol_ScrollbarBg]    = {0.75f, 0.75f, 0.75f, 1.00f};
        style.Colors[ImGuiCol_CheckMark]      = {0.00f, 0.00f, 0.00f, 1.00f};
        style.Colors[ImGuiCol_SliderGrab]     = {0.00f, 0.00f, 0.50f, 1.00f};
        break;
    }
    case Theme::RetroBlue: {
        // A moodier, blue-tinted variant of the retro chrome — Windows-2000
        // "Ocean" flavored rather than pure gray-and-navy.
        ImGui::StyleColorsClassic();
        apply_retro_shape(style);
        style.Colors[ImGuiCol_Text]           = {0.92f, 0.96f, 1.00f, 1.00f};
        style.Colors[ImGuiCol_WindowBg]       = {0.10f, 0.16f, 0.28f, 1.00f};
        style.Colors[ImGuiCol_ChildBg]        = {0.09f, 0.14f, 0.24f, 1.00f};
        style.Colors[ImGuiCol_PopupBg]        = {0.10f, 0.16f, 0.28f, 1.00f};
        style.Colors[ImGuiCol_Border]         = {0.30f, 0.45f, 0.65f, 1.00f};
        style.Colors[ImGuiCol_FrameBg]        = {0.16f, 0.24f, 0.38f, 1.00f};
        style.Colors[ImGuiCol_FrameBgHovered] = {0.20f, 0.32f, 0.48f, 1.00f};
        style.Colors[ImGuiCol_FrameBgActive]  = {0.24f, 0.38f, 0.56f, 1.00f};
        style.Colors[ImGuiCol_Button]         = {0.18f, 0.34f, 0.58f, 1.00f};
        style.Colors[ImGuiCol_ButtonHovered]  = {0.24f, 0.46f, 0.74f, 1.00f};
        style.Colors[ImGuiCol_ButtonActive]   = {0.14f, 0.28f, 0.48f, 1.00f};
        style.Colors[ImGuiCol_Header]         = {0.16f, 0.38f, 0.66f, 0.70f};
        style.Colors[ImGuiCol_HeaderHovered]  = {0.20f, 0.46f, 0.78f, 0.85f};
        style.Colors[ImGuiCol_HeaderActive]   = {0.22f, 0.52f, 0.88f, 1.00f};
        style.Colors[ImGuiCol_TitleBg]        = {0.06f, 0.12f, 0.22f, 1.00f};
        style.Colors[ImGuiCol_TitleBgActive]  = {0.10f, 0.24f, 0.44f, 1.00f};
        style.Colors[ImGuiCol_MenuBarBg]      = {0.08f, 0.13f, 0.22f, 1.00f};
        style.Colors[ImGuiCol_ScrollbarBg]    = {0.08f, 0.13f, 0.22f, 1.00f};
        style.Colors[ImGuiCol_CheckMark]      = {0.35f, 0.70f, 1.00f, 1.00f};
        style.Colors[ImGuiCol_SliderGrab]     = {0.30f, 0.60f, 0.95f, 1.00f};
        break;
    }
    case Theme::RetroWave: {
        // Neon pink/cyan on deep purple — 80s/90s retro-futurist aesthetic.
        // Same square, hard-bordered Retro shape as the other Retro skins;
        // only the palette changes between them.
        ImGui::StyleColorsDark();
        apply_retro_shape(style);
        style.Colors[ImGuiCol_Text]           = {1.00f, 0.90f, 0.98f, 1.00f};
        style.Colors[ImGuiCol_TextDisabled]   = {0.75f, 0.55f, 0.85f, 1.00f};
        style.Colors[ImGuiCol_WindowBg]       = {0.08f, 0.05f, 0.16f, 1.00f};
        style.Colors[ImGuiCol_ChildBg]        = {0.10f, 0.06f, 0.20f, 1.00f};
        style.Colors[ImGuiCol_PopupBg]        = {0.10f, 0.06f, 0.20f, 1.00f};
        style.Colors[ImGuiCol_Border]         = {0.85f, 0.35f, 0.85f, 0.60f};
        style.Colors[ImGuiCol_FrameBg]        = {0.20f, 0.10f, 0.32f, 1.00f};
        style.Colors[ImGuiCol_FrameBgHovered] = {0.32f, 0.14f, 0.46f, 1.00f};
        style.Colors[ImGuiCol_FrameBgActive]  = {0.42f, 0.16f, 0.56f, 1.00f};
        style.Colors[ImGuiCol_Button]         = {0.62f, 0.18f, 0.62f, 0.75f};
        style.Colors[ImGuiCol_ButtonHovered]  = {0.95f, 0.30f, 0.75f, 0.90f};
        style.Colors[ImGuiCol_ButtonActive]   = {0.20f, 0.85f, 0.85f, 0.90f};
        style.Colors[ImGuiCol_Header]         = {0.55f, 0.20f, 0.65f, 0.65f};
        style.Colors[ImGuiCol_HeaderHovered]  = {0.80f, 0.30f, 0.85f, 0.80f};
        style.Colors[ImGuiCol_HeaderActive]   = {0.25f, 0.85f, 0.85f, 0.90f};
        style.Colors[ImGuiCol_TitleBg]        = {0.14f, 0.05f, 0.24f, 1.00f};
        style.Colors[ImGuiCol_TitleBgActive]  = {0.35f, 0.10f, 0.45f, 1.00f};
        style.Colors[ImGuiCol_MenuBarBg]      = {0.12f, 0.06f, 0.22f, 1.00f};
        style.Colors[ImGuiCol_ScrollbarBg]    = {0.10f, 0.06f, 0.20f, 1.00f};
        style.Colors[ImGuiCol_CheckMark]      = {0.25f, 0.90f, 0.90f, 1.00f};
        style.Colors[ImGuiCol_SliderGrab]     = {0.90f, 0.35f, 0.80f, 1.00f};
        style.Colors[ImGuiCol_SliderGrabActive] = {0.25f, 0.90f, 0.90f, 1.00f};
        style.Colors[ImGuiCol_Separator]      = {0.70f, 0.30f, 0.75f, 0.60f};
        break;
    }
    case Theme::RetroDark: {
        // A dark-mode take on the retro bevelled chrome: charcoal desktop and
        // buttons with a cool muted-blue accent, same hard square bevel shape.
        ImGui::StyleColorsDark();
        apply_retro_shape(style);
        style.Colors[ImGuiCol_Text]           = {0.92f, 0.92f, 0.94f, 1.00f};
        style.Colors[ImGuiCol_WindowBg]       = {0.14f, 0.14f, 0.16f, 1.00f};
        style.Colors[ImGuiCol_ChildBg]        = {0.14f, 0.14f, 0.16f, 1.00f};
        style.Colors[ImGuiCol_PopupBg]        = {0.14f, 0.14f, 0.16f, 1.00f};
        style.Colors[ImGuiCol_Border]         = {0.04f, 0.04f, 0.05f, 1.00f};
        style.Colors[ImGuiCol_FrameBg]        = {0.20f, 0.20f, 0.23f, 1.00f};
        style.Colors[ImGuiCol_FrameBgHovered] = {0.25f, 0.25f, 0.29f, 1.00f};
        style.Colors[ImGuiCol_FrameBgActive]  = {0.28f, 0.28f, 0.33f, 1.00f};
        style.Colors[ImGuiCol_Button]         = {0.28f, 0.28f, 0.32f, 1.00f};
        style.Colors[ImGuiCol_ButtonHovered]  = {0.36f, 0.36f, 0.42f, 1.00f};
        style.Colors[ImGuiCol_ButtonActive]   = {0.22f, 0.22f, 0.26f, 1.00f};
        style.Colors[ImGuiCol_Header]         = {0.30f, 0.48f, 0.65f, 0.70f};
        style.Colors[ImGuiCol_HeaderHovered]  = {0.36f, 0.56f, 0.75f, 0.85f};
        style.Colors[ImGuiCol_HeaderActive]   = {0.40f, 0.62f, 0.82f, 1.00f};
        style.Colors[ImGuiCol_TitleBg]        = {0.08f, 0.08f, 0.10f, 1.00f};
        style.Colors[ImGuiCol_TitleBgActive]  = {0.16f, 0.24f, 0.32f, 1.00f};
        style.Colors[ImGuiCol_MenuBarBg]      = {0.10f, 0.10f, 0.12f, 1.00f};
        style.Colors[ImGuiCol_ScrollbarBg]    = {0.10f, 0.10f, 0.12f, 1.00f};
        style.Colors[ImGuiCol_CheckMark]      = {0.45f, 0.70f, 0.90f, 1.00f};
        style.Colors[ImGuiCol_SliderGrab]     = {0.40f, 0.62f, 0.85f, 1.00f};
        break;
    }
    case Theme::XPBlue: {
        // The default "Luna" Windows XP scheme: beige dialogs, blue chrome.
        ImGui::StyleColorsLight();
        apply_xp_shape(style);
        style.Colors[ImGuiCol_Text]           = {0.05f, 0.05f, 0.05f, 1.00f};
        style.Colors[ImGuiCol_WindowBg]       = {0.925f, 0.914f, 0.847f, 1.00f};
        style.Colors[ImGuiCol_ChildBg]        = {0.925f, 0.914f, 0.847f, 1.00f};
        style.Colors[ImGuiCol_PopupBg]        = {0.965f, 0.960f, 0.930f, 1.00f};
        style.Colors[ImGuiCol_Border]         = {0.45f, 0.52f, 0.68f, 1.00f};
        style.Colors[ImGuiCol_FrameBg]        = {1.00f, 1.00f, 1.00f, 1.00f};
        style.Colors[ImGuiCol_FrameBgHovered] = {0.90f, 0.95f, 1.00f, 1.00f};
        style.Colors[ImGuiCol_FrameBgActive]  = {0.82f, 0.90f, 1.00f, 1.00f};
        style.Colors[ImGuiCol_Button]         = {0.60f, 0.75f, 0.96f, 1.00f};
        style.Colors[ImGuiCol_ButtonHovered]  = {0.72f, 0.85f, 1.00f, 1.00f};
        style.Colors[ImGuiCol_ButtonActive]   = {0.40f, 0.58f, 0.86f, 1.00f};
        style.Colors[ImGuiCol_Header]         = {0.23f, 0.45f, 0.84f, 0.70f};
        style.Colors[ImGuiCol_HeaderHovered]  = {0.30f, 0.54f, 0.92f, 0.85f};
        style.Colors[ImGuiCol_HeaderActive]   = {0.16f, 0.38f, 0.76f, 1.00f};
        style.Colors[ImGuiCol_TitleBg]        = {0.15f, 0.38f, 0.85f, 1.00f};
        style.Colors[ImGuiCol_TitleBgActive]  = {0.09f, 0.32f, 0.80f, 1.00f};
        style.Colors[ImGuiCol_MenuBarBg]      = {0.80f, 0.85f, 0.97f, 1.00f};
        style.Colors[ImGuiCol_ScrollbarBg]    = {0.86f, 0.88f, 0.90f, 1.00f};
        style.Colors[ImGuiCol_CheckMark]      = {0.16f, 0.38f, 0.76f, 1.00f};
        style.Colors[ImGuiCol_SliderGrab]     = {0.30f, 0.54f, 0.90f, 1.00f};
        break;
    }
    case Theme::XPSilver: {
        // XP's "Silver" scheme: cooler gray chrome instead of beige/blue.
        ImGui::StyleColorsLight();
        apply_xp_shape(style);
        style.Colors[ImGuiCol_Text]           = {0.08f, 0.08f, 0.09f, 1.00f};
        style.Colors[ImGuiCol_WindowBg]       = {0.88f, 0.88f, 0.90f, 1.00f};
        style.Colors[ImGuiCol_ChildBg]        = {0.88f, 0.88f, 0.90f, 1.00f};
        style.Colors[ImGuiCol_PopupBg]        = {0.93f, 0.93f, 0.95f, 1.00f};
        style.Colors[ImGuiCol_Border]         = {0.55f, 0.55f, 0.60f, 1.00f};
        style.Colors[ImGuiCol_FrameBg]        = {1.00f, 1.00f, 1.00f, 1.00f};
        style.Colors[ImGuiCol_FrameBgHovered] = {0.94f, 0.94f, 0.97f, 1.00f};
        style.Colors[ImGuiCol_FrameBgActive]  = {0.86f, 0.86f, 0.90f, 1.00f};
        style.Colors[ImGuiCol_Button]         = {0.72f, 0.72f, 0.78f, 1.00f};
        style.Colors[ImGuiCol_ButtonHovered]  = {0.82f, 0.82f, 0.88f, 1.00f};
        style.Colors[ImGuiCol_ButtonActive]   = {0.58f, 0.58f, 0.65f, 1.00f};
        style.Colors[ImGuiCol_Header]         = {0.50f, 0.50f, 0.58f, 0.70f};
        style.Colors[ImGuiCol_HeaderHovered]  = {0.58f, 0.58f, 0.68f, 0.85f};
        style.Colors[ImGuiCol_HeaderActive]   = {0.42f, 0.42f, 0.50f, 1.00f};
        style.Colors[ImGuiCol_TitleBg]        = {0.48f, 0.48f, 0.55f, 1.00f};
        style.Colors[ImGuiCol_TitleBgActive]  = {0.36f, 0.36f, 0.44f, 1.00f};
        style.Colors[ImGuiCol_MenuBarBg]      = {0.80f, 0.80f, 0.84f, 1.00f};
        style.Colors[ImGuiCol_ScrollbarBg]    = {0.82f, 0.82f, 0.85f, 1.00f};
        style.Colors[ImGuiCol_CheckMark]      = {0.36f, 0.36f, 0.44f, 1.00f};
        style.Colors[ImGuiCol_SliderGrab]     = {0.50f, 0.50f, 0.58f, 1.00f};
        break;
    }
    case Theme::XPOlive: {
        // XP's "Olive Green" scheme: warm sage/olive chrome.
        ImGui::StyleColorsLight();
        apply_xp_shape(style);
        style.Colors[ImGuiCol_Text]           = {0.08f, 0.09f, 0.05f, 1.00f};
        style.Colors[ImGuiCol_WindowBg]       = {0.86f, 0.87f, 0.74f, 1.00f};
        style.Colors[ImGuiCol_ChildBg]        = {0.86f, 0.87f, 0.74f, 1.00f};
        style.Colors[ImGuiCol_PopupBg]        = {0.91f, 0.92f, 0.82f, 1.00f};
        style.Colors[ImGuiCol_Border]         = {0.48f, 0.52f, 0.34f, 1.00f};
        style.Colors[ImGuiCol_FrameBg]        = {0.99f, 0.99f, 0.94f, 1.00f};
        style.Colors[ImGuiCol_FrameBgHovered] = {0.94f, 0.96f, 0.85f, 1.00f};
        style.Colors[ImGuiCol_FrameBgActive]  = {0.87f, 0.90f, 0.74f, 1.00f};
        style.Colors[ImGuiCol_Button]         = {0.62f, 0.68f, 0.44f, 1.00f};
        style.Colors[ImGuiCol_ButtonHovered]  = {0.72f, 0.78f, 0.52f, 1.00f};
        style.Colors[ImGuiCol_ButtonActive]   = {0.48f, 0.54f, 0.32f, 1.00f};
        style.Colors[ImGuiCol_Header]         = {0.44f, 0.52f, 0.28f, 0.70f};
        style.Colors[ImGuiCol_HeaderHovered]  = {0.52f, 0.60f, 0.32f, 0.85f};
        style.Colors[ImGuiCol_HeaderActive]   = {0.36f, 0.44f, 0.22f, 1.00f};
        style.Colors[ImGuiCol_TitleBg]        = {0.42f, 0.48f, 0.26f, 1.00f};
        style.Colors[ImGuiCol_TitleBgActive]  = {0.32f, 0.40f, 0.18f, 1.00f};
        style.Colors[ImGuiCol_MenuBarBg]      = {0.78f, 0.80f, 0.62f, 1.00f};
        style.Colors[ImGuiCol_ScrollbarBg]    = {0.80f, 0.82f, 0.66f, 1.00f};
        style.Colors[ImGuiCol_CheckMark]      = {0.32f, 0.40f, 0.18f, 1.00f};
        style.Colors[ImGuiCol_SliderGrab]     = {0.48f, 0.56f, 0.30f, 1.00f};
        break;
    }
    case Theme::Original:
    default: {
        ImGui::StyleColorsDark();
        apply_shared_shape(style);
        style.Colors[ImGuiCol_WindowBg]      = {0.13f, 0.13f, 0.15f, 1.0f};
        style.Colors[ImGuiCol_Header]        = {0.26f, 0.45f, 0.72f, 0.50f};
        style.Colors[ImGuiCol_HeaderHovered] = {0.26f, 0.45f, 0.72f, 0.80f};
        style.Colors[ImGuiCol_HeaderActive]  = {0.26f, 0.45f, 0.72f, 1.00f};
        style.Colors[ImGuiCol_Button]        = {0.26f, 0.42f, 0.66f, 0.60f};
        style.Colors[ImGuiCol_ButtonHovered] = {0.26f, 0.45f, 0.72f, 1.00f};
        style.Colors[ImGuiCol_ButtonActive]  = {0.20f, 0.38f, 0.62f, 1.00f};
        style.Colors[ImGuiCol_FrameBg]       = {0.20f, 0.20f, 0.23f, 1.00f};
        style.Colors[ImGuiCol_FrameBgHovered] = {0.26f, 0.26f, 0.30f, 1.00f};
        style.Colors[ImGuiCol_ChildBg]       = {0.11f, 0.11f, 0.13f, 1.00f};
        style.Colors[ImGuiCol_MenuBarBg]     = {0.10f, 0.10f, 0.12f, 1.00f};
        style.Colors[ImGuiCol_TitleBgActive] = {0.16f, 0.29f, 0.48f, 1.00f};
        break;
    }
    }
}

// ── Retro bevelled button (Retro look and feel) ──────────────────────────────
//
// Hand-drawn to look like a physical 3D Win95-era button: a raised bevel
// (white/light edge top-left, black/dark edge bottom-right) that inverts to a
// sunken bevel while pressed, with the label nudging a pixel to sell the
// "pushed in" effect. This is a distinct widget, not a recolor of the normal
// ImGui rounded button.
static bool RetroButtonImpl(const char* label, ImVec2 size, bool small) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    const ImGuiStyle& style = ImGui::GetStyle();
    const ImGuiID id = window->GetID(label);
    const char* text_end = ImGui::FindRenderedTextEnd(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, text_end, true);

    ImVec2 pad = small ? ImVec2(6.0f, 2.0f) : style.FramePadding;
    if (size.x == 0.0f) size.x = label_size.x + pad.x * 2.0f;
    if (size.y == 0.0f) size.y = label_size.y + pad.y * 2.0f;

    const ImVec2 pos = window->DC.CursorPos;
    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
    ImGui::ItemSize(size, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered = false, held = false;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    bool sunken = hovered && held;

    ImU32 face = ImGui::GetColorU32(sunken ? ImGuiCol_ButtonActive :
                                     hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
    const ImU32 white = IM_COL32(255, 255, 255, 255);
    const ImU32 black = IM_COL32(0, 0, 0, 255);
    const ImU32 dark  = IM_COL32(90, 90, 90, 255);
    ImU32 tl = sunken ? dark  : white;
    ImU32 br = sunken ? white : black;
    ImU32 tl_inner = tl;
    ImU32 br_inner = sunken ? white : dark;

    ImDrawList* dl = window->DrawList;
    dl->AddRectFilled(bb.Min, bb.Max, face);
    // Outer bevel edge
    dl->AddLine(bb.Min, ImVec2(bb.Max.x, bb.Min.y), tl);
    dl->AddLine(bb.Min, ImVec2(bb.Min.x, bb.Max.y), tl);
    dl->AddLine(ImVec2(bb.Max.x - 1, bb.Min.y), ImVec2(bb.Max.x - 1, bb.Max.y), br);
    dl->AddLine(ImVec2(bb.Min.x, bb.Max.y - 1), ImVec2(bb.Max.x, bb.Max.y - 1), br);
    // Inner bevel edge (1px in) for a chunkier, more distinct 3D look
    if (size.x > 4 && size.y > 4) {
        dl->AddLine(ImVec2(bb.Min.x + 1, bb.Min.y + 1), ImVec2(bb.Max.x - 1, bb.Min.y + 1), tl_inner);
        dl->AddLine(ImVec2(bb.Min.x + 1, bb.Min.y + 1), ImVec2(bb.Min.x + 1, bb.Max.y - 1), tl_inner);
        dl->AddLine(ImVec2(bb.Max.x - 2, bb.Min.y + 1), ImVec2(bb.Max.x - 2, bb.Max.y - 1), br_inner);
        dl->AddLine(ImVec2(bb.Min.x + 1, bb.Max.y - 2), ImVec2(bb.Max.x - 1, bb.Max.y - 2), br_inner);
    }

    ImVec2 text_pos(bb.Min.x + pad.x, bb.Min.y + (size.y - label_size.y) * 0.5f);
    if (sunken) { text_pos.x += 1.0f; text_pos.y += 1.0f; }
    dl->PushClipRect(bb.Min, bb.Max, true);
    dl->AddText(text_pos, ImGui::GetColorU32(ImGuiCol_Text), label, text_end);
    dl->PopClipRect();

    return pressed;
}

bool RetroButton(const char* label, ImVec2 size) {
    return RetroButtonImpl(label, size, false);
}

bool RetroSmallButton(const char* label) {
    return RetroButtonImpl(label, ImVec2(0, 0), true);
}

// ── XP glossy button (Windows XP look and feel) ──────────────────────────────
//
// Hand-drawn to look like a "Luna" theme button: rounded corners, a glossy
// highlight band across the top half, and a thin border. Colors are derived
// from the active theme's Button/ButtonHovered/ButtonActive style colors, so
// the same widget code serves all three XP palettes (Blue/Silver/Olive).
static ImVec4 scale_color(ImVec4 c, float f) {
    auto clamp01 = [](float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
    return ImVec4(clamp01(c.x * f), clamp01(c.y * f), clamp01(c.z * f), c.w);
}

static bool XPButtonImpl(const char* label, ImVec2 size, bool small) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    const ImGuiStyle& style = ImGui::GetStyle();
    const ImGuiID id = window->GetID(label);
    const char* text_end = ImGui::FindRenderedTextEnd(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, text_end, true);

    ImVec2 pad = small ? ImVec2(8.0f, 3.0f) : ImVec2(style.FramePadding.x + 2.0f, style.FramePadding.y + 2.0f);
    if (size.x == 0.0f) size.x = label_size.x + pad.x * 2.0f;
    if (size.y == 0.0f) size.y = label_size.y + pad.y * 2.0f;

    const ImVec2 pos = window->DC.CursorPos;
    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
    ImGui::ItemSize(size, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered = false, held = false;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    bool sunken = hovered && held;

    ImVec4 base = ImGui::GetStyleColorVec4(sunken ? ImGuiCol_ButtonActive :
                                            hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
    ImVec4 top4 = scale_color(base, sunken ? 0.85f : 1.35f);
    ImVec4 bot4 = scale_color(base, sunken ? 1.15f : 0.85f);
    ImU32 top_col    = ImGui::ColorConvertFloat4ToU32(top4);
    ImU32 bot_col    = ImGui::ColorConvertFloat4ToU32(bot4);
    ImU32 border_col = ImGui::GetColorU32(ImGuiCol_Border);

    const float rounding = 5.0f;
    ImDrawList* dl = window->DrawList;

    dl->AddRectFilled(bb.Min, bb.Max, bot_col, rounding, ImDrawFlags_RoundCornersAll);
    float band_h = size.y * 0.55f;
    ImVec2 band_max(bb.Max.x, bb.Min.y + band_h);
    dl->AddRectFilled(bb.Min, band_max, top_col, rounding, ImDrawFlags_RoundCornersTop);
    dl->AddRect(bb.Min, bb.Max, border_col, rounding, ImDrawFlags_RoundCornersAll, 1.0f);

    ImVec2 text_pos(bb.Min.x + (size.x - label_size.x) * 0.5f, bb.Min.y + (size.y - label_size.y) * 0.5f);
    if (sunken) { text_pos.x += 1.0f; text_pos.y += 1.0f; }
    dl->PushClipRect(bb.Min, bb.Max, true);
    dl->AddText(text_pos, ImGui::GetColorU32(ImGuiCol_Text), label, text_end);
    dl->PopClipRect();

    return pressed;
}

bool XPButton(const char* label, ImVec2 size) {
    return XPButtonImpl(label, size, false);
}

bool XPSmallButton(const char* label) {
    return XPButtonImpl(label, ImVec2(0, 0), true);
}

} // namespace pasgen
