#pragma once

#include "imgui.h"

namespace pasgen::ui {

// A small vocabulary of widgets shared by every screen. Keeping the styling
// here (rather than pushing colors at each call site) is what makes buttons,
// labels and cards look identical everywhere and keeps both themes in step.

// ── Buttons ──────────────────────────────────────────────────────────────────
// One filled accent button per view marks the primary action; everything else
// is secondary/ghost so the eye lands in the right place.
bool PrimaryButton(const char* label, ImVec2 size = ImVec2(0, 0));
bool SecondaryButton(const char* label, ImVec2 size = ImVec2(0, 0));
bool DangerButton(const char* label, ImVec2 size = ImVec2(0, 0));
// Borderless until hovered — for toolbars and inline row actions.
bool GhostButton(const char* label, ImVec2 size = ImVec2(0, 0));
// Compact inline action sized to the text (Copy / Show / Generate).
bool TinyButton(const char* label);
// Renders as a link: accent-colored text, underlined on hover.
bool LinkButton(const char* label);

// ── Type ─────────────────────────────────────────────────────────────────────
void Title(const char* fmt, ...)    IM_FMTARGS(1);  // h1
void Heading(const char* fmt, ...)  IM_FMTARGS(1);  // h2
void Caption(const char* fmt, ...)  IM_FMTARGS(1);  // small, muted
void Dimmed(const char* fmt, ...)   IM_FMTARGS(1);  // body, secondary color

// Uppercase tracked label with a hairline rule to its right; groups fields
// inside a card.
void SectionHeader(const char* label);
// Field label rendered above its input.
void FieldLabel(const char* label);

// ── Containers ───────────────────────────────────────────────────────────────
// A bordered surface panel. Must be paired with EndCard().
bool BeginCard(const char* id, ImVec2 size = ImVec2(0, 0), bool scrollable = false);
void EndCard();

// Full-width hairline with balanced spacing above and below.
void Divider(float pad = 6.0f);

// ── Feedback ─────────────────────────────────────────────────────────────────
// Tinted inline message block used for form errors and hints.
void Banner(const char* text, ImVec4 tone);
// Segmented strength bar; `frac` in 0..1 lights proportional segments.
void StrengthMeter(float frac, ImVec4 color, const char* label, float width);
// Small rounded status chip.
void Pill(const char* text, ImVec4 tone);

} // namespace pasgen::ui
