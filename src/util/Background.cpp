#include "util/Background.hpp"
#include "util/Font.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace pasgen {

HomeBackground home_background_from_string(const std::string& s) {
    if (s == "matrix") return HomeBackground::Matrix;
    if (s == "outrun") return HomeBackground::Outrun;
    return HomeBackground::None;
}

std::string home_background_to_string(HomeBackground b) {
    switch (b) {
        case HomeBackground::Matrix: return "matrix";
        case HomeBackground::Outrun: return "outrun";
        default:                     return "none";
    }
}

namespace {

ImVec4 lerp(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                  a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

// Cheap integer avalanche hash -- NOT the app's crypto RNG. This only ever
// picks decorative glyphs/positions on the wallpaper, nothing security-
// relevant, so a fast deterministic hash beats pulling in <random> state.
uint32_t hash_u32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU;
    x ^= x >> 15; x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

float hash_float(uint32_t x) { return (float)(hash_u32(x) & 0xFFFFFF) / (float)0xFFFFFF; }

// ── Matrix rain ──────────────────────────────────────────────────────────
//
// A grid of monospace cells. Each column runs a couple of independent
// falling strands (STREAMS_PER_COL) rather than just one, for a denser
// effect; each strand tracks only a "head" row and a fall speed, and the
// glyphs themselves are picked per-frame from a hash of (column, row, a
// slow time bucket) rather than stored, so the trail shimmers/rewrites
// itself the way the real effect does without needing a persistent
// character grid.

constexpr int STREAMS_PER_COL = 2;

struct RainStream {
    float head;   // leading glyph's row, fractional; grows downward
    float speed;  // rows/second
};

struct RainColumn {
    RainStream streams[STREAMS_PER_COL];
};

// Picks a fresh head/speed for one strand, always re-entering from just
// above the screen -- the same rule on first spawn and on every later
// respawn (a strand that has fully fallen off the bottom). There's no
// separate "scatter across the span" case: the first-frame look of an
// already-running effect comes from fast-forwarding this same rule through
// a few seconds of simulated time in warm_up_streams(), not from a
// different seeding formula.
void reset_stream(RainStream& s, uint32_t seed, int n_rows, int trail) {
    s.head = -hash_float(seed) * n_rows * 0.6f - (float)trail;
    s.speed = 4.0f + hash_float(seed ^ 0x9e3779b9u) * 7.0f;
}

uint32_t stream_base_seed(int col, int stream) {
    return hash_u32((uint32_t)col * 2654435761u + (uint32_t)stream * 40503u);
}

// Advances one strand by dt, respawning it (via reset_stream) once it has
// fully fallen off the bottom. `t` is only used to vary the respawn seed so
// repeated respawns of the same strand don't all pick the same new speed.
void step_stream(RainStream& s, uint32_t base_seed, int n_rows, int trail, float dt, float t) {
    s.head += s.speed * dt;
    if (s.head - trail > n_rows) {
        reset_stream(s, base_seed ^ (uint32_t)(t * 1000.0f), n_rows, trail);
    }
}

// Runs the exact per-frame update above for a handful of simulated seconds,
// all before the first real frame draws -- so the rain looks like it has
// already been falling for a while instead of just having spawned, without
// hand-placing strands at arbitrary starting heights.
void warm_up_streams(std::vector<RainColumn>& cols, int n_rows, int trail) {
    constexpr float WARM_UP_SECONDS = 5.0f;
    constexpr float WARM_UP_STEP = 1.0f / 30.0f;
    for (float t = 0.0f; t < WARM_UP_SECONDS; t += WARM_UP_STEP) {
        for (int c = 0; c < (int)cols.size(); ++c)
            for (int s = 0; s < STREAMS_PER_COL; ++s)
                step_stream(cols[c].streams[s], stream_base_seed(c, s), n_rows, trail, WARM_UP_STEP, t);
    }
}

void render_matrix(float w, float h, float dt) {
    static std::vector<RainColumn> cols;
    static float t = 0.0f;
    t += dt;

    static const char glyphset[] =
        "01ABCDEFGHIJKLMNOPQRSTUVWXYZ$+-*/=<>{}[]()!?#%&";
    const int glyph_count = (int)strlen(glyphset);

    ImFont* font = fonts().mono ? fonts().mono : ImGui::GetFont();
    const float cell_w = 12.0f;
    const float cell_h = 19.0f;
    const int   n_cols = std::max(1, (int)(w / cell_w) + 1);
    const int   n_rows = std::max(1, (int)(h / cell_h) + 2);
    const int   trail  = 16;

    // (Re)seed on first use or resize. A resize mid-animation just restarts
    // the columns -- unnoticeable next to a continuously falling effect.
    // warm_up_streams then fast-forwards a few seconds of real update steps
    // so the very first frame already shows a screenful of rain instead of
    // filling in over time.
    if ((int)cols.size() != n_cols) {
        cols.resize(n_cols);
        for (int c = 0; c < n_cols; ++c)
            for (int s = 0; s < STREAMS_PER_COL; ++s)
                reset_stream(cols[c].streams[s], stream_base_seed(c, s), n_rows, trail);
        warm_up_streams(cols, n_rows, trail);
    }

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const int shimmer_bucket = (int)(t * 6.0f); // glyphs re-roll ~6x/second

    const ImVec4 bright(0.78f, 0.92f, 1.00f, 1.0f); // near-white blue (head)
    const ImVec4 mid   (0.30f, 0.50f, 0.98f, 1.0f); // app accent blue
    const ImVec4 dim   (0.05f, 0.10f, 0.30f, 1.0f); // fades toward near-black blue

    for (int c = 0; c < n_cols; ++c) {
        for (int s = 0; s < STREAMS_PER_COL; ++s) {
            RainStream& stream = cols[c].streams[s];
            step_stream(stream, stream_base_seed(c, s), n_rows, trail, dt, t);

            for (int r = 0; r < trail; ++r) {
                float row = stream.head - (float)r;
                if (row < 0 || row > n_rows) continue;

                float ft = (float)r / (float)trail; // 0 at the head, 1 at the tail
                float alpha = powf(1.0f - ft, 2.2f);
                if (alpha < 0.02f) continue;

                ImVec4 rgba = (r == 0) ? bright : lerp(mid, dim, ft);
                rgba.w = alpha;

                uint32_t hv = hash_u32((uint32_t)c * 73856093u ^ (uint32_t)row * 19349663u
                                        ^ (uint32_t)s * 2246822519u ^ (uint32_t)shimmer_bucket * 83492791u);
                char ch[2] = { glyphset[hv % glyph_count], '\0' };

                dl->AddText(font, cell_h * 0.85f, ImVec2(c * cell_w, row * cell_h),
                            ImGui::ColorConvertFloat4ToU32(rgba), ch);
            }
        }
    }
}

// ── Outrun grid ──────────────────────────────────────────────────────────
//
// A synthwave horizon: gradient sky, a banded sun, and a perspective floor
// grid that scrolls toward the viewer. All static geometry recomputed each
// frame from `t` -- there's no per-frame state worth caching here.

void render_outrun(float w, float h, float dt) {
    static float t = 0.0f;
    t += dt;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const float horizon = h * 0.42f;

    // Sky: near-black at the top, dusky magenta at the horizon.
    ImU32 sky_top = ImGui::ColorConvertFloat4ToU32(ImVec4(0.055f, 0.063f, 0.078f, 1.0f));
    ImU32 sky_bot = ImGui::ColorConvertFloat4ToU32(ImVec4(0.145f, 0.075f, 0.165f, 1.0f));
    dl->AddRectFilledMultiColor({0, 0}, {w, horizon}, sky_top, sky_top, sky_bot, sky_bot);
    // Ground plane the grid lines sit on top of.
    dl->AddRectFilled({0, horizon}, {w, h},
                       ImGui::ColorConvertFloat4ToU32(ImVec4(0.035f, 0.04f, 0.055f, 1.0f)));

    // Sun: horizontal gradient bands clipped to a circle, with gaps opening
    // up in the lower third -- the classic "venetian blind" retro sun.
    const float cx = w * 0.5f, cy = horizon - h * 0.10f, r = h * 0.16f;
    const int bands = 22;
    for (int i = 0; i < bands; ++i) {
        float bt = (float)i / (float)(bands - 1);
        if (bt > 0.55f && (i % 2 == 0)) continue; // skip every other low band
        float y0 = cy - r + r * 2.0f * (float)i       / bands;
        float y1 = cy - r + r * 2.0f * (float)(i + 1) / bands;
        ImVec4 c = lerp(ImVec4(1.00f, 0.55f, 0.78f, 1.0f),   // pink
                        ImVec4(1.00f, 0.62f, 0.30f, 1.0f), bt); // orange
        dl->PushClipRect({cx - r - 1, y0}, {cx + r + 1, y1}, true);
        dl->AddCircleFilled({cx, cy}, r, ImGui::ColorConvertFloat4ToU32(c), 64);
        dl->PopClipRect();
    }

    // Perspective floor grid. Horizontal lines use harmonic spacing (1/i)
    // below the horizon, which is what makes evenly-spaced world lines
    // bunch up near the horizon and spread out near the viewer; `phase`
    // increments `i` continuously so the whole grid scrolls forward.
    const float scroll_speed = 0.6f; // cycles/second
    float phase = fmodf(t * scroll_speed, 1.0f);

    const int n_h = 22;
    for (int i = 1; i <= n_h; ++i) {
        float k = (float)i + phase;
        float y = horizon + (h - horizon) / k * 1.3f;
        if (y > h) continue;
        float depth_t = (y - horizon) / (h - horizon); // 0 near horizon, 1 at bottom
        ImVec4 c(0.30f, 0.55f, 1.00f, 0.12f + 0.55f * depth_t);
        dl->AddLine({0, y}, {w, y}, ImGui::ColorConvertFloat4ToU32(c), 1.0f + depth_t);
    }

    const ImU32 v_col = ImGui::ColorConvertFloat4ToU32(ImVec4(0.30f, 0.55f, 1.00f, 0.35f));
    const int n_v = 16;
    for (int i = 0; i <= n_v; ++i) {
        float x_bottom = -w * 0.5f + (2.0f * w) * (float)i / n_v;
        dl->AddLine({w * 0.5f, horizon}, {x_bottom, h}, v_col, 1.0f);
    }
}

} // namespace

void render_home_background(HomeBackground bg, float w, float h, float dt) {
    switch (bg) {
    case HomeBackground::Matrix: render_matrix(w, h, dt); break;
    case HomeBackground::Outrun: render_outrun(w, h, dt); break;
    case HomeBackground::None:   break;
    }
}

} // namespace pasgen
