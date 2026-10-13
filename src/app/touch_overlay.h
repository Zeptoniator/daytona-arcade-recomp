#pragma once
#include "app/touch_controls.h"
#include "imgui.h"

#include <algorithm>
#include <cstdio>

namespace app {
// The touch controls over the game: translucent (opacity 0..1 scales every
// alpha), lit while held. gear: the gear engaged now (1-4), shown over the
// gear buttons. rate: the game's frames/s, shown under Start (negative: hidden).
inline void draw_touch_controls(const TouchControls &touch, int gear = 0, float opacity = 0.6f, float rate = -1) {
    auto *draw = ImGui::GetForegroundDrawList();
    ImFont *font = ImGui::GetFont();
    const float u = touch.unit, op = std::clamp(opacity, 0.1f, 1.f);
    auto alpha = [&](int a) { return int(std::clamp(a * op, 0.f, 255.f)); };
    auto text = [&](float cx, float cy, float size, const char *s, ImU32 col) {
        const ImVec2 t = font->CalcTextSizeA(size, 1e9f, 0, s);
        draw->AddText(font, size, {cx - t.x / 2, cy - t.y / 2}, col, s);
    };
    const ImU32 ink = IM_COL32(255, 255, 255, alpha(235)), edge = IM_COL32(255, 255, 255, alpha(120));
    const ImU32 fill = IM_COL32(12, 16, 24, alpha(120)), lit = IM_COL32(255, 196, 0, alpha(200));
    for (const auto &b : touch.boxes) {
        if (b.action == TouchControls::Steering) continue;
        const bool active = b.action < kNumActions && touch.values[b.action] > 0;
        const ImVec2 c{b.x + b.w / 2, b.y + b.h / 2};
        if (b.shape == TouchControls::Round) { // the view buttons, in the cabinet's colours
            static const ImU32 views[4] = {IM_COL32(220, 40, 40, 0), IM_COL32(40, 110, 230, 0),
                                           IM_COL32(240, 200, 30, 0), IM_COL32(40, 180, 70, 0)};
            const ImU32 rgb = views[std::clamp(b.action - View1, 0, 3)];
            const float r = b.w / 2;
            draw->AddCircleFilled(c, r, rgb | IM_COL32(0, 0, 0, alpha(active ? 235 : 110)));
            draw->AddCircle(c, r, active ? ink : edge, 0, 2 * u);
            text(c.x, c.y, 18 * u, b.label, ink);
            continue;
        }
        const float round = 10 * u;
        draw->AddRectFilled({b.x, b.y}, {b.x + b.w, b.y + b.h}, active ? lit : fill, round);
        draw->AddRect({b.x, b.y}, {b.x + b.w, b.y + b.h}, active ? ink : edge, round, 0, 2 * u);
        const ImU32 col = active ? IM_COL32(20, 20, 20, alpha(255)) : ink;
        if (b.action == TouchControls::Menu) { // pause sign
            const float bw = 5 * u, bh = 18 * u, gap = 5 * u;
            draw->AddRectFilled({c.x - gap / 2 - bw, c.y - bh / 2}, {c.x - gap / 2, c.y + bh / 2}, col, 1.5f * u);
            draw->AddRectFilled({c.x + gap / 2, c.y - bh / 2}, {c.x + gap / 2 + bw, c.y + bh / 2}, col, 1.5f * u);
        } else if (b.action == GearUp || b.action == GearDown) { // + and -, drawn: the font's minus is thin
            const float s = 11 * u, t = 4 * u;
            draw->AddRectFilled({c.x - s, c.y - t / 2}, {c.x + s, c.y + t / 2}, col, t / 2);
            if (b.action == GearUp) draw->AddRectFilled({c.x - t / 2, c.y - s}, {c.x + t / 2, c.y + s}, col, t / 2);
        } else if (b.action == Accelerate || b.action == Brake) {
            text(c.x, b.y + b.h - 22 * u, 15 * u, b.label, col);
            const float s = 9 * u; // a chevron: up for the throttle, down for the brake
            const float dir = b.action == Accelerate ? -1.f : 1.f, cy = b.y + b.h * .42f;
            for (int i = 0; i < 2; ++i) {
                const float y0 = cy + (i * 12 * u - 6 * u) * -dir;
                draw->AddLine({c.x - s, y0 - dir * s / 2}, {c.x, y0 + dir * s / 2}, col, 3 * u);
                draw->AddLine({c.x, y0 + dir * s / 2}, {c.x + s, y0 - dir * s / 2}, col, 3 * u);
            }
        } else {
            text(c.x, c.y, 15 * u, b.label, col);
        }
    }
    if (gear >= 1 && gear <= 4) // the gear engaged, under the view buttons (the sides' middle stays clear)
        if (const auto *v3 = touch.find(View3), *v4 = touch.find(View4); v3 && v4) {
            char s[16];
            std::snprintf(s, sizeof s, "GEAR %d", gear);
            text((v3->x + v4->x + v4->w) / 2, v4->y + v4->h + 16 * u, 16 * u, s, ink);
        }
    if (rate >= 0)
        if (const auto *start = touch.find(Start)) {
            char s[24];
            std::snprintf(s, sizeof s, rate > 0 ? "%.1f fps" : "- fps", double(rate));
            text(start->x + start->w / 2, start->y + start->h + 16 * u, 15 * u, s,
                 rate > 0 && rate < 55 ? IM_COL32(255, 170, 60, alpha(255)) : ink);
        }
    // Steering: a track with its centre where the finger landed (at rest, bottom left).
    if (const auto *zone = touch.find(TouchControls::Steering)) {
        const float travel = touch.range * u, knob = 22 * u;
        const float cx = touch.steering ? touch.origin_x : zone->x + 10 * u + knob + travel;
        const float cy = touch.steering ? touch.origin_y : zone->y + zone->h - 10 * u - knob - 8 * u;
        const float x = touch.steering ? std::clamp(touch.finger_x, cx - travel, cx + travel) : cx;
        draw->AddRectFilled({cx - travel, cy - 5 * u}, {cx + travel, cy + 5 * u}, fill, 5 * u);
        draw->AddRect({cx - travel, cy - 5 * u}, {cx + travel, cy + 5 * u}, edge, 5 * u, 0, 1.5f * u);
        draw->AddLine({cx, cy - 12 * u}, {cx, cy + 12 * u}, edge, 2 * u);
        draw->AddCircleFilled({x, cy}, knob, touch.steering ? lit : fill);
        draw->AddCircle({x, cy}, knob, touch.steering ? ink : edge, 0, 2 * u);
        if (!touch.steering) {
            text(cx, cy - knob - 14 * u, 14 * u, "STEER", ink);
            text(cx - travel * .6f, cy, 16 * u, "<", ink);
            text(cx + travel * .6f, cy, 16 * u, ">", ink);
        }
    }
}
} // namespace app
