#pragma once
#include "app/controls.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace app {
// Raw finger capture, independent of ImGui's single mouse pointer. A finger
// owns its original control until release, so steering and pedals can overlap.
//
// Layout, in dp (one dp is `unit` touch units): small clusters in the corners,
// at most 164 dp wide, which on a phone (20:9: about 175 dp) fit the bars
// beside the 4:3 picture, so nothing covers the game's HUD. The middle of the
// sides stays clear (a phone's camera cutout is there in landscape), so the
// area can be the whole width; only the top and bottom need the safe area. Top left Menu and
// Coin, Start under them; top right the four view buttons as on the cabinet
// (red, blue, yellow, green: also the test menu's cursor and mode buttons);
// bottom right the gear buttons over BRAKE and GAS.
// Steering floats: a finger landing anywhere in the left half is the centre,
// and sliding `range` dp to a side is full lock. Past full lock the centre
// follows the finger, so turning back answers at once.
class TouchControls {
public:
    enum Shape { Rect, Round };
    struct Box {
        float x, y, w, h;
        int action;
        const char *label;
        Shape shape = Rect;
        bool contains(float px, float py) const {
            return px >= x && px <= x + w && py >= y && py <= y + h;
        }
    };
    static constexpr int Steering = kNumActions, Menu = kNumActions + 1;
    std::vector<Box> boxes; // buttons first; the steering zone, under the top-left buttons, last
    std::array<float, kNumActions> values{};
    bool steering = false;
    float unit = 1;  // one dp in touch units
    float range = 70; // dp of finger travel to full lock
    float origin_x = 0, origin_y = 0, finger_x = 0; // the steering finger: its centre and where it is

    // area: the window's safe area; dp: one dp in its units (0: from the
    // area's size); range_dp: steering travel to full lock.
    void layout(float x, float y, float w, float h, float dp = 0, float range_dp = 70) {
        clear();
        boxes.clear();
        // The clusters need about 420 x 350 dp; smaller areas scale them down.
        unit = std::min({dp > 0 ? dp : std::min(w / 800.f, h / 360.f) * 2.f, w / 420.f, h / 350.f});
        range = std::clamp(range_dp, 30.f, 200.f);
        const float u = unit, m = 12 * u, g = 8 * u, right = x + w, bottom = y + h;
        const float bh = 44 * u, small = 44 * u, wide = 64 * u; // two rows, as the view buttons
        boxes.push_back({x + m, y + m, small, bh, Menu, "Menu"});
        boxes.push_back({x + m + small + g, y + m, wide, bh, Coin, "COIN"});
        boxes.push_back({x + m, y + m + bh + g, small + g + wide, bh, Start, "START"});
        const float d = 44 * u, c1 = right - m - d - g - d, c2 = right - m - d;
        boxes.push_back({c1, y + m, d, d, View1, "1", Round});
        boxes.push_back({c2, y + m, d, d, View2, "2", Round});
        boxes.push_back({c1, y + m + d + g, d, d, View3, "3", Round});
        boxes.push_back({c2, y + m + d + g, d, d, View4, "4", Round});
        const float gas_w = 76 * u, brake_w = 68 * u, pedal = 120 * u, gear_h = 46 * u;
        const float gas_x = right - m - gas_w, brake_x = gas_x - g - brake_w, pedal_y = bottom - m - pedal;
        boxes.push_back({brake_x, pedal_y, brake_w, pedal, Brake, "BRAKE"});
        boxes.push_back({gas_x, pedal_y, gas_w, pedal, Accelerate, "GAS"});
        boxes.push_back({brake_x, pedal_y - g - gear_h, brake_w, gear_h, GearDown, "-"});
        boxes.push_back({gas_x, pedal_y - g - gear_h, gas_w, gear_h, GearUp, "+"});
        boxes.push_back({x, y, w * .5f, h, Steering, "STEER"});
    }
    const Box *find(int action) const {
        for (const auto &b : boxes) if (b.action == action) return &b;
        return nullptr;
    }
    bool down(SDL_TouchID device, SDL_FingerID id, float x, float y) {
        up(device, id);
        for (size_t i = 0; i < boxes.size(); ++i) {
            if (!boxes[i].contains(x, y)) continue;
            if (boxes[i].action == Menu) { clear(); return true; }
            if (boxes[i].action == Steering && steering) break; // one steering finger
            if (fingers.size() < 16) {
                fingers.push_back({device, id, i, x, y});
                if (boxes[i].action == Steering) origin_x = finger_x = x, origin_y = y;
                if (boxes[i].action < kNumActions) pressed[boxes[i].action] = 1;
            }
            update();
            break;
        }
        return false;
    }
    void move(SDL_TouchID device, SDL_FingerID id, float x, float y) {
        for (auto &f : fingers) if (f.device == device && f.id == id) { f.x = x; f.y = y; }
        update();
    }
    void up(SDL_TouchID device, SDL_FingerID id) {
        std::erase_if(fingers, [&](const Finger &f) { return f.device == device && f.id == id; });
        update();
    }
    void cancel(SDL_TouchID device, SDL_FingerID id) {
        for (const auto &f : fingers)
            if (f.device == device && f.id == id && boxes[f.box].action < kNumActions)
                pressed[boxes[f.box].action] = 0;
        up(device, id);
    }
    void consumed() { pressed.fill(0); update(); }
    void clear() { fingers.clear(); values.fill(0); pressed.fill(0); steering = false; }
    float steer() const { return values[SteerRight] - values[SteerLeft]; } // -1..1, for the overlay
private:
    struct Finger { SDL_TouchID device; SDL_FingerID id; size_t box; float x, y; };
    std::vector<Finger> fingers;
    std::array<float, kNumActions> pressed{};
    void update() {
        values = pressed; steering = false;
        for (const auto &f : fingers) {
            const auto &b = boxes[f.box];
            if (b.action == Steering) {
                steering = true;
                const float travel = range * unit;
                origin_x = std::clamp(origin_x, f.x - travel, f.x + travel); // the centre follows past full lock
                finger_x = f.x;
                const float v = (f.x - origin_x) / travel;
                const float magnitude = std::max(0.f, (std::abs(v) - .06f) / .94f);
                values[v < 0 ? SteerLeft : SteerRight] = magnitude;
            } else if (b.contains(f.x, f.y)) values[b.action] = 1;
        }
    }
};
} // namespace app
