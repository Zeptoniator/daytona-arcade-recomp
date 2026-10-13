// app::TouchControls: floating steering (the landing point is the centre,
// `range` dp to full lock, the centre following past it), one steering
// finger, pedals and steering together, short taps kept until a game frame,
// cancel, a finger leaving its pedal, Menu, and layouts that fit and do not
// overlap from a small handheld to a tablet.
#undef NDEBUG // the checks are asserts: kept in Release builds
#include "app/touch_controls.h"
#include <cassert>
#include <iostream>

int main() {
    app::TouchControls t;
    t.layout(30, 10, 800, 360, 1, 70);
    auto box = [&](int action) {
        const auto *b = t.find(action);
        assert(b);
        return *b;
    };
    auto zone = box(t.Steering), gas = box(app::Accelerate), brake = box(app::Brake);
    const float sx = zone.x + 150, sy = zone.y + zone.h - 60, travel = t.range * t.unit;
    t.down(1, 1, sx, sy);
    assert(t.steering && t.values[app::SteerLeft] == 0 && t.values[app::SteerRight] == 0); // landing: straight on
    t.down(1, 2, gas.x + 5, gas.y + 5);
    t.down(2, 2, brake.x + 5, brake.y + 5);
    assert(t.values[app::Accelerate] == 1 && t.values[app::Brake] == 1);
    t.move(1, 1, sx + travel / 2, sy);
    assert(t.values[app::SteerRight] > 0.4f && t.values[app::SteerRight] < 0.6f);
    t.move(1, 1, sx + travel, sy);
    assert(t.values[app::SteerRight] == 1);
    t.move(1, 1, sx + 2 * travel, sy); // past full lock: the centre follows
    assert(t.values[app::SteerRight] == 1);
    t.move(1, 1, sx + travel, sy); // turning back answers at once
    assert(t.values[app::SteerRight] == 0 && t.values[app::SteerLeft] == 0);
    t.move(1, 1, sx, sy);
    assert(t.values[app::SteerLeft] == 1);
    t.down(1, 9, zone.x + 40, sy); // a second finger on the left half does not steer
    assert(t.values[app::SteerLeft] == 1);
    t.consumed();
    t.up(1, 2);
    assert(t.values[app::Accelerate] == 0 && t.values[app::Brake] == 1);
    t.up(1, 1);
    assert(!t.steering && t.values[app::SteerLeft] == 0 && t.values[app::SteerRight] == 0);
    t.clear();
    auto coin = box(app::Coin);
    t.down(1, 3, coin.x + 5, coin.y + 5);
    t.up(1, 3);
    assert(t.values[app::Coin] == 1); // short tap survives until a game frame
    t.consumed();
    assert(t.values[app::Coin] == 0);
    t.down(1, 3, coin.x + 5, coin.y + 5);
    t.cancel(1, 3);
    assert(t.values[app::Coin] == 0);
    t.down(1, 4, gas.x + 5, gas.y + 5);
    t.consumed();
    t.move(1, 4, brake.x + 5, brake.y + 5);
    assert(t.values[app::Accelerate] == 0 && t.values[app::Brake] == 0);
    auto menu = box(t.Menu);
    assert(t.down(1, 5, menu.x + 5, menu.y + 5));
    for (float v : t.values) assert(v == 0);
    // dp scales: a handheld, phones (one at 2.75 pixels per dp), a 4:3 tablet
    for (const auto dims : {std::tuple{480.f, 272.f, 1.f}, {844.f, 390.f, 1.f}, {2400.f, 1080.f, 2.75f},
                            {1024.f, 768.f, 1.f}, {2048.f, 1536.f, 2.f}}) {
        const auto [w, h, dp] = dims;
        t.layout(20, 10, w, h, dp, 200);
        for (size_t i = 0; i < t.boxes.size(); ++i) {
            const auto &a = t.boxes[i];
            assert(a.x >= 20 && a.y >= 10 && a.x + a.w <= 20 + w && a.y + a.h <= 10 + h);
            if (a.action == t.Steering) continue; // the zone lies under the top-left buttons
            for (size_t j = i + 1; j < t.boxes.size(); ++j) {
                const auto &b = t.boxes[j];
                if (b.action == t.Steering) {
                    // only the top-left buttons may lie over the steering zone
                    assert(a.x >= b.x + b.w || a.y + a.h <= 10 + 120 * t.unit);
                    continue;
                }
                assert(a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y || b.y + b.h <= a.y);
            }
        }
    }
    t.down(1, 6, gas.x + 5, gas.y + 5);
    t.layout(0, 0, 480, 272);
    for (float v : t.values) assert(v == 0);
    std::cout << "Touch tests passed: floating steering, multitouch, capture, release, taps, menu, layouts\n";
}
