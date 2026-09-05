#include "util/Widgets.hpp"
#include "util/Font.hpp"
#include "util/Theme.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace pasgen::ui {

namespace {

// Buttons differ only in their three state colors and their text color, so all
// four variants share one implementation.
bool styled_button(const char* label, ImVec2 size,
                   ImVec4 bg, ImVec4 hovered, ImVec4 active, ImVec4 text,
                   float border_size, ImVec4 border) {
    ImGui::PushStyleColor(ImGuiCol_Button,        bg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  active);
    ImGui::PushStyleColor(ImGuiCol_Text,          text);
    ImGui::PushStyleColor(ImGuiCol_Border,        border);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, border_size);
    bool pressed = ImGui::Button(label, size);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(5);
    return pressed;
}

void text_v(ImFont* font, ImVec4 color, const char* fmt, va_list args) {
    char buf[1024];
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    if (font) ImGui::PushFont(font);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(buf);
    ImGui::PopStyleColor();
    if (font) ImGui::PopFont();
}

const ImVec4 kTransparent = ImVec4(0, 0, 0, 0);

} // namespace

// ── Buttons ──────────────────────────────────────────────────────────────────

bool PrimaryButton(const char* label, ImVec2 size) {
    const Palette& p = palette();
    return styled_button(label, size, p.accent, p.accent_hover, p.accent_active,
                         p.on_accent, 0.0f, kTransparent);
}

bool SecondaryButton(const char* label, ImVec2 size) {
    const Palette& p = palette();
    return styled_button(label, size, p.surface_alt, p.surface_hover, p.border,
                         p.text, 1.0f, p.border_strong);
}

bool DangerButton(const char* label, ImVec2 size) {
    const Palette& p = palette();
    return styled_button(label, size, p.danger, p.danger_hover, p.danger,
                         p.on_accent, 0.0f, kTransparent);
}

bool GhostButton(const char* label, ImVec2 size) {
    const Palette& p = palette();
    return styled_button(label, size, kTransparent, p.surface_hover, p.border,
                         p.text_dim, 0.0f, kTransparent);
}

bool TinyButton(const char* label) {
    const Palette& p = palette();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
    if (fonts().caption) ImGui::PushFont(fonts().caption);
    bool pressed = styled_button(label, ImVec2(0, 0), p.surface_alt, p.surface_hover,
                                 p.border, p.text_dim, 1.0f, p.border);
    if (fonts().caption) ImGui::PopFont();
    ImGui::PopStyleVar();
    return pressed;
}

bool LinkButton(const char* label) {
    const Palette& p = palette();
    ImVec2 pos  = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::CalcTextSize(label);

    ImGui::PushStyleColor(ImGuiCol_Text, p.accent);
    ImGui::PushStyleColor(ImGuiCol_Button, kTransparent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kTransparent);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, kTransparent);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    bool pressed = ImGui::Button(label);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);

    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImGui::GetWindowDrawList()->AddLine({pos.x, pos.y + size.y},
                                            {pos.x + size.x, pos.y + size.y},
                                            ImGui::GetColorU32(p.accent), 1.0f);
    }
    return pressed;
}

// ── Type ─────────────────────────────────────────────────────────────────────

void Title(const char* fmt, ...) {
    va_list a; va_start(a, fmt);
    text_v(fonts().h1, palette().text, fmt, a);
    va_end(a);
}

void Heading(const char* fmt, ...) {
    va_list a; va_start(a, fmt);
    text_v(fonts().h2, palette().text, fmt, a);
    va_end(a);
}

void Caption(const char* fmt, ...) {
    va_list a; va_start(a, fmt);
    text_v(fonts().caption, palette().text_muted, fmt, a);
    va_end(a);
}

void Dimmed(const char* fmt, ...) {
    va_list a; va_start(a, fmt);
    text_v(nullptr, palette().text_dim, fmt, a);
    va_end(a);
}

void SectionHeader(const char* label) {
    const Palette& p = palette();

    // Letter-spaced small caps read as a deliberate label rather than as body
    // text that happens to be bold.
    char spaced[128];
    size_t n = 0;
    for (const char* s = label; *s && n + 2 < sizeof(spaced); ++s) {
        char c = *s;
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        spaced[n++] = c;
        if (s[1]) spaced[n++] = ' ';
    }
    spaced[n] = '\0';

    if (fonts().caption) ImGui::PushFont(fonts().caption);
    ImGui::PushStyleColor(ImGuiCol_Text, p.text_muted);
    ImGui::TextUnformatted(spaced);
    ImGui::PopStyleColor();

    // Hairline filling the remaining width, vertically centered on the text.
    ImVec2 last_min = ImGui::GetItemRectMin();
    ImVec2 last_max = ImGui::GetItemRectMax();
    float  right    = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
    float  y        = (last_min.y + last_max.y) * 0.5f;
    if (right > last_max.x + 10.0f) {
        ImGui::GetWindowDrawList()->AddLine({last_max.x + 10.0f, y}, {right, y},
                                            ImGui::GetColorU32(p.border), 1.0f);
    }
    if (fonts().caption) ImGui::PopFont();
    ImGui::Spacing();
}

void FieldLabel(const char* label) {
    if (fonts().caption) ImGui::PushFont(fonts().caption);
    ImGui::PushStyleColor(ImGuiCol_Text, palette().text_dim);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    if (fonts().caption) ImGui::PopFont();
}

// ── Containers ───────────────────────────────────────────────────────────────

bool BeginCard(const char* id, ImVec2 size, bool scrollable) {
    const Palette& p = palette();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, p.surface);
    ImGui::PushStyleColor(ImGuiCol_Border,  p.border);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 14));
    ImGuiWindowFlags flags = scrollable ? 0 : ImGuiWindowFlags_NoScrollbar;
    return ImGui::BeginChild(id, size, ImGuiChildFlags_Borders, flags);
}

void EndCard() {
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void Divider(float pad) {
    ImGui::Dummy(ImVec2(0, pad));
    ImGui::PushStyleColor(ImGuiCol_Separator, palette().border);
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, pad));
}

// ── Feedback ─────────────────────────────────────────────────────────────────

void Banner(const char* text, ImVec4 tone) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float  width   = ImGui::GetContentRegionAvail().x;
    ImVec2 pad(12, 9);
    ImVec2 pos     = ImGui::GetCursorScreenPos();
    ImVec2 text_sz = ImGui::CalcTextSize(text, nullptr, false, width - pad.x * 2 - 4);
    float  height  = text_sz.y + pad.y * 2;

    ImVec4 fill = tone; fill.w = 0.14f;
    dl->AddRectFilled(pos, {pos.x + width, pos.y + height},
                      ImGui::GetColorU32(fill), 8.0f);
    // Accent rail on the leading edge, clipped to the rounded corners.
    dl->PushClipRect(pos, {pos.x + width, pos.y + height}, true);
    dl->AddRectFilled(pos, {pos.x + 3.0f, pos.y + height}, ImGui::GetColorU32(tone), 0.0f);
    dl->PopClipRect();

    ImGui::SetCursorScreenPos({pos.x + pad.x, pos.y + pad.y});
    ImGui::PushStyleColor(ImGuiCol_Text, tone);
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + width - pad.x * 2);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();

    // Reserve the full banner rect so following widgets lay out below it.
    ImGui::SetCursorScreenPos(pos);
    ImGui::Dummy(ImVec2(width, height));
}

void StrengthMeter(float frac, ImVec4 color, const char* label, float width) {
    const Palette& p = palette();
    const int   kSegments = 4;
    const float kGap      = 4.0f;
    const float kHeight   = 6.0f;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float seg_w = (width - kGap * (kSegments - 1)) / kSegments;
    // Round up so a strength of 1/4 lights exactly one segment.
    int lit = (int)(frac * kSegments + 0.999f);

    for (int i = 0; i < kSegments; ++i) {
        float x = pos.x + i * (seg_w + kGap);
        ImU32 col = ImGui::GetColorU32(i < lit ? color : p.surface_hover);
        dl->AddRectFilled({x, pos.y}, {x + seg_w, pos.y + kHeight}, col, kHeight * 0.5f);
    }

    ImGui::Dummy(ImVec2(width, kHeight));
    if (label && label[0]) {
        ImGui::SameLine(0, 10);
        if (fonts().caption) ImGui::PushFont(fonts().caption);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
        if (fonts().caption) ImGui::PopFont();
    }
}

void Pill(const char* text, ImVec4 tone) {
    if (fonts().caption) ImGui::PushFont(fonts().caption);
    ImVec2 pad(9, 3);
    ImVec2 sz  = ImGui::CalcTextSize(text);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float h = sz.y + pad.y * 2;

    ImVec4 fill = tone; fill.w = 0.16f;
    ImGui::GetWindowDrawList()->AddRectFilled(
        pos, {pos.x + sz.x + pad.x * 2, pos.y + h}, ImGui::GetColorU32(fill), h * 0.5f);

    ImGui::SetCursorScreenPos({pos.x + pad.x, pos.y + pad.y});
    ImGui::PushStyleColor(ImGuiCol_Text, tone);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();

    ImGui::SetCursorScreenPos({pos.x, pos.y});
    ImGui::Dummy(ImVec2(sz.x + pad.x * 2, h));
    if (fonts().caption) ImGui::PopFont();
}

} // namespace pasgen::ui
