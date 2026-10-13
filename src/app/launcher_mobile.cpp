// The launcher on Android: the same settings as the desktop launcher, laid
// out for a phone held in landscape. A fixed header keeps the ROM status and
// Start (or Resume and Reset) in view; the settings are in tabs that scroll
// with a finger. Touch-friendly widgets only: segmented buttons instead of
// combos and steppers instead of sliders, since a slider jumps to where a
// finger lands, which a scroll would start on. Settings only the desktop has
// (graphics API, fullscreen, mouse cursor, display modes, VRR) are left out.
#include "app/launcher.h"

#ifdef SDL_PLATFORM_ANDROID

#include "imgui.h"
#include "imgui_internal.h" // ImGui::ClearActiveID: a drag that becomes a scroll is not a tap

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace app {

namespace {

const ImVec4 kGood{0.45f, 0.86f, 0.5f, 1}, kWarn{1, 0.66f, 0.3f, 1}, kBad{1, 0.42f, 0.42f, 1};
const ImVec4 kAccent{0.12f, 0.42f, 0.86f, 1}, kStart{0.80f, 0.11f, 0.16f, 1};

float dp() { return ImGui::GetStyle().FontScaleDpi; } // mobile_style puts the display scale there

// A button in its own colour (the primary action).
bool colored_button(const char *label, ImVec4 c, ImVec2 size) {
    ImGui::PushStyleColor(ImGuiCol_Button, c);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(std::min(c.x * 1.15f, 1.f), std::min(c.y * 1.15f, 1.f), std::min(c.z * 1.15f, 1.f), 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(c.x * .85f, c.y * .85f, c.z * .85f, 1));
    const bool pressed = ImGui::Button(label, size);
    ImGui::PopStyleColor(3);
    return pressed;
}

// One of n choices, as a row of buttons; true when it changed.
bool segmented(const char *id, int &value, const char *const *items, int n) {
    bool changed = false;
    ImGui::PushID(id);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4 * dp(), 4 * dp()));
    for (int i = 0; i < n; ++i) {
        if (i) ImGui::SameLine();
        ImGui::PushID(i);
        const bool on = value == i;
        if (on ? colored_button(items[i], kAccent, ImVec2(0, 0)) : ImGui::Button(items[i])) changed |= value != i, value = i;
        ImGui::PopID();
    }
    ImGui::PopStyleVar();
    ImGui::PopID();
    return changed;
}

// [-] value [+] within lo..hi; true when it changed.
template <typename T> bool stepper(const char *id, T &value, T lo, T hi, T step, const char *text) {
    ImGui::PushID(id);
    const ImVec2 b(44 * dp(), 0);
    bool changed = false;
    ImGui::BeginDisabled(value <= lo);
    if (ImGui::Button("-", b)) value = std::max(lo, T(value - step)), changed = true;
    ImGui::EndDisabled();
    ImGui::SameLine();
    const float w = 92 * dp(), tw = ImGui::CalcTextSize(text).x;
    const float x = ImGui::GetCursorPosX();
    ImGui::SetCursorPosX(x + std::max(0.f, (w - tw) / 2));
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(text);
    ImGui::SameLine(x + w);
    ImGui::BeginDisabled(value >= hi);
    if (ImGui::Button("+", b)) value = std::min(hi, T(value + step)), changed = true;
    ImGui::EndDisabled();
    ImGui::PopID();
    return changed;
}

// A setting's name, in a fixed column, then its widget on the same line.
void label(const char *text) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(text);
    ImGui::SameLine(170 * dp());
}

void note(const char *text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

void heading(const char *text) {
    ImGui::Spacing();
    ImGui::SeparatorText(text);
}

std::string file_name(const std::string &path) {
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

} // namespace

void mobile_style(float scale) {
    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->AddFontDefaultVector(); // scalable: sharp at any size, the touch controls' labels too
    io.ConfigFlags |= ImGuiConfigFlags_IsTouchScreen;
    ImGuiStyle &s = ImGui::GetStyle();
    s.FontSizeBase = 15.0f;
    s.WindowPadding = ImVec2(14, 10);
    s.FramePadding = ImVec2(12, 9);
    s.ItemSpacing = ImVec2(10, 9);
    s.ItemInnerSpacing = ImVec2(8, 6);
    s.ScrollbarSize = 14;
    s.GrabMinSize = 18;
    s.FrameRounding = s.GrabRounding = s.TabRounding = 6;
    s.ChildRounding = s.PopupRounding = 8;
    s.WindowBorderSize = 0;
    s.SeparatorTextBorderSize = 2;
    s.SeparatorTextPadding = ImVec2(0, 6);
    ImVec4 *c = s.Colors;
    c[ImGuiCol_Text] = ImVec4(0.92f, 0.94f, 0.97f, 1);
    c[ImGuiCol_TextDisabled] = ImVec4(0.56f, 0.6f, 0.68f, 1);
    c[ImGuiCol_WindowBg] = ImVec4(0.043f, 0.055f, 0.08f, 1);
    c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.1f, 0.14f, 1);
    c[ImGuiCol_FrameBg] = ImVec4(0.11f, 0.14f, 0.2f, 1);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.14f, 0.18f, 0.26f, 1);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.17f, 0.22f, 0.32f, 1);
    c[ImGuiCol_Button] = ImVec4(0.12f, 0.15f, 0.22f, 1);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.16f, 0.2f, 0.29f, 1);
    c[ImGuiCol_ButtonActive] = ImVec4(0.2f, 0.26f, 0.38f, 1);
    c[ImGuiCol_Header] = ImVec4(0.12f, 0.15f, 0.22f, 1);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.16f, 0.2f, 0.29f, 1);
    c[ImGuiCol_HeaderActive] = ImVec4(0.2f, 0.26f, 0.38f, 1);
    c[ImGuiCol_CheckMark] = ImVec4(1, 0.77f, 0, 1);
    c[ImGuiCol_Separator] = ImVec4(0.15f, 0.19f, 0.26f, 1);
    c[ImGuiCol_Tab] = ImVec4(0.08f, 0.1f, 0.15f, 1);
    c[ImGuiCol_TabHovered] = ImVec4(0.16f, 0.2f, 0.29f, 1);
    c[ImGuiCol_TabSelected] = kAccent;
    c[ImGuiCol_TabSelectedOverline] = ImVec4(1, 0.77f, 0, 1);
    c[ImGuiCol_TabDimmed] = c[ImGuiCol_Tab]; // the tab bar loses focus to the settings under it: same look
    c[ImGuiCol_TabDimmedSelected] = kAccent;
    c[ImGuiCol_TabDimmedSelectedOverline] = c[ImGuiCol_TabSelectedOverline];
    c[ImGuiCol_PlotHistogram] = kAccent;
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.2f, 0.25f, 0.34f, 1);
    s.ScaleAllSizes(scale);
    s.FontScaleDpi = scale;
}

Launcher::Result Launcher::draw_mobile(bool game_running, const Devices &devices) {
    const float u = dp();
    Result result = Stay;
    const ImGuiViewport *vp = ImGui::GetMainViewport();
    SDL_Rect safe{0, 0, int(vp->Size.x), int(vp->Size.y)};
    SDL_GetWindowSafeArea(window_, &safe);
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + safe.x, vp->Pos.y + safe.y));
    ImGui::SetNextWindowSize(ImVec2(float(safe.w), float(safe.h)));
    ImGui::SetNextWindowBgAlpha(game_running ? 0.92f : 1.0f);
    ImGui::Begin("Daytona USA", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoScrollWithMouse);

    // Header: the game, the ROM's state and the main actions, always in view.
    {
        const float top = ImGui::GetCursorPosY(), right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
        ImGui::PushFont(nullptr, 24.0f);
        ImGui::TextUnformatted("DAYTONA USA");
        ImGui::PopFont();
        ImGui::SameLine();
        ImGui::SetCursorPosY(top + 7 * u);
        ImGui::TextDisabled(std::string(M2_ROMSET) == "daytona" ? "Revision A, 1994" : "Deluxe '93");
        ImGui::TextColored(rom_ok_ ? kGood : kWarn, "%s", rom_ok_ ? "ROM set verified" : rom_message_.c_str());
        const float bh = 46 * u, gap = ImGui::GetStyle().ItemSpacing.x;
        const float wide = 128 * u, narrow = 96 * u;
        const float actions = game_running ? wide + gap + narrow + gap + narrow : wide + gap + narrow;
        ImGui::SetCursorPos(ImVec2(right - actions, top));
        ImGui::BeginDisabled(!rom_ok_);
        if (game_running) {
            if (colored_button("RESUME", kStart, ImVec2(wide, bh))) result = Resume;
            ImGui::SameLine();
            if (ImGui::Button("Reset", ImVec2(narrow, bh))) result = Reset;
        } else if (colored_button("START", kStart, ImVec2(wide, bh))) {
            result = StartGame;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Quit", ImVec2(narrow, bh))) result = Quit;
        ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY(), top + bh + ImGui::GetStyle().ItemSpacing.y));
        if (!error_.empty()) ImGui::TextColored(kBad, "%s", error_.c_str());
    }

    static const char *tabs[] = {"Game", "Display", "Controls", "Audio", "Link"};
    if (ImGui::BeginTabBar("tabs", ImGuiTabBarFlags_FittingPolicyShrink)) {
        for (int i = 0; i < 5; ++i)
            if (ImGui::BeginTabItem(tabs[i])) {
                tab_ = i;
                ImGui::EndTabItem();
            }
        ImGui::EndTabBar();
    }

    ImGui::BeginChild("content", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x - 8 * u);
    switch (tab_) {
    case 0: { // Game
        heading("ROM set");
        const std::string name = file_name(cfg_.rom_path); // an import is the app's own copy (app/rom_file.h)
        ImGui::TextUnformatted(cfg_.rom_path.empty()                  ? "No ROM set chosen"
                               : name.rfind("imported-", 0) == 0 ? "Your ROM set (verified copy in the app's storage)"
                                                                     : name.c_str());
        ImGui::TextColored(rom_ok_ ? kGood : kWarn, "%s", rom_message_.c_str());
        if (ImGui::Button(dialog_pending_ ? "Choosing..." : "Browse...", ImVec2(150 * u, 0)) && !dialog_pending_) browse();
        ImGui::SameLine();
        if (ImGui::Button("Check again", ImVec2(150 * u, 0))) check_rom();
        note("Choose your own " M2_ROMSET " ZIP or 7z, from Downloads for instance. A verified copy is kept "
             "in the app's storage: the original can be moved or deleted afterwards.");
        if (!checks_.empty() && ImGui::TreeNode("Files in the set")) {
            if (ImGui::BeginTable("files", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                for (const auto &c : checks_) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(c.file.c_str());
                    ImGui::TableNextColumn();
                    if (c.ok) ImGui::TextColored(kGood, "ok");
                    else ImGui::TextColored(kWarn, "%s", c.problem.c_str());
                }
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }

        heading("Starting");
        if (ImGui::Checkbox("Skip this screen", &cfg_.skip_launcher)) cfg_.save();
        note("The game starts straight away; the pause button at the top left brings this screen back.");

        heading("Arcade settings (test menu)");
        ImGui::Checkbox("Open the test menu when the game starts", &cfg_.hold_test); // not saved: once, then it clears
        note("The operator's menu: coins, difficulty, cabinet. In it, view button 1 (red) moves down, 4 (green) "
             "up, 3 (yellow) and 2 (blue) change a value, START selects.");
        if (std::string(M2_ROMSET) == "daytona" && !cfg_.link)
            note("With link play off, LINK ID is kept on SINGLE: this version would otherwise wait for a second "
                 "cabinet at NETWORK CHECKING.");
        break;
    }
    case 1: { // Display
        heading("3D");
        static const char *renderers[] = {"Software (exact)", "Hardware (GPU)"};
        int rd = cfg_.renderer == "hardware" ? 1 : 0;
        label("Renderer");
        if (segmented("renderer", rd, renderers, 2)) cfg_.renderer = rd ? "hardware" : "software", cfg_.save();
        note("Software draws the 3D on the CPU, as the arcade board does. Hardware uses the GPU: experimental, "
             "close to but not yet pixel-exact, and lighter on the CPU.");
        if (game_rate_ > 0) {
            const bool slow = game_rate_ < 55;
            ImGui::TextColored(slow ? kWarn : kGood, "Last measured: %.1f frames/s (the arcade: 57.52).", double(game_rate_));
            if (slow && cfg_.renderer != "hardware")
                note("This phone does not keep up with the software renderer: try Hardware, or draw every 2nd frame.");
        }
        static const char *supersampling[] = {"Off", "2x", "3x", "4x"};
        int ss = std::clamp(cfg_.supersampling, 1, 4) - 1;
        label("Super sampling");
        ImGui::BeginDisabled(cfg_.renderer != "hardware");
        if (segmented("ss", ss, supersampling, 4)) cfg_.supersampling = ss + 1, cfg_.save();
        ImGui::EndDisabled();
        static const char *draw_modes[] = {"Every frame", "Every 2nd", "Every 3rd"};
        int dm = std::clamp(cfg_.draw_mode, 0, 2);
        label("Draw");
        if (segmented("draw", dm, draw_modes, 3)) cfg_.draw_mode = dm, cfg_.save();
        note("Drawing fewer frames is faster on a slow phone; the game itself still runs at full speed.");

        heading("Enhancements");
        static const char *aspects[] = {"4:3", "16:10", "16:9", "21:9"};
        static const char *aspect_ids[] = {"", "16:10", "16:9", "21:9"};
        int aspect = 0;
        for (int i = 0; i < 4; i++)
            if (cfg_.aspect == aspect_ids[i]) aspect = i;
        label("Widescreen");
        if (segmented("aspect", aspect, aspects, 4)) cfg_.aspect = aspect_ids[aspect], cfg_.save();
        ImGui::BeginDisabled(cfg_.aspect.empty());
        if (ImGui::Checkbox("HUD at the screen edges", &cfg_.hud_edges)) cfg_.save();
        if (ImGui::Checkbox("Stretch the sky picture", &cfg_.stretch_backdrop)) cfg_.save();
        ImGui::EndDisabled();
        note("4:3 is the arcade's screen, with the touch controls beside it. Wider shows more of the scene at "
             "the sides (experimental).");
        static const char *distances[] = {"Shortest", "Shorter", "Default", "Further", "Furthest"};
        int dd = std::clamp(cfg_.draw_distance, -2, 2) + 2;
        label("Draw distance");
        if (segmented("distance", dd, distances, 5)) cfg_.draw_distance = dd - 2, cfg_.save();

        heading("Frame pacing");
        if (ImGui::Checkbox("Sync to the display", &cfg_.pace_sync_display)) cfg_.save();
        note("On a 60 or 120 Hz screen the game then runs at 60 frames/s, every frame shown for the same time, "
             "about 4% faster than the arcade (57.52).");
        ImGui::TextDisabled("Now: %s.", pacing_status_.c_str());
        if (ImGui::Checkbox("Show the frame rate", &cfg_.show_rate)) cfg_.save();
        note("Under START while playing: the game's frames per second (the arcade runs 57.52), orange when "
             "the phone cannot keep up.");
        break;
    }
    case 2: { // Controls
        Controls &c = cfg_.controls;
        heading("Touch controls");
        char text[32];
        int opacity = int(cfg_.touch_opacity * 100 + 0.5f);
        std::snprintf(text, sizeof text, "%d%%", opacity);
        label("Opacity");
        if (stepper("opacity", opacity, 10, 100, 10, text)) cfg_.touch_opacity = opacity / 100.0f, cfg_.save();
        std::snprintf(text, sizeof text, "%d dp", cfg_.touch_steer_range);
        label("Steering travel");
        if (stepper("range", cfg_.touch_steer_range, 30, 200, 10, text)) cfg_.save();
        note("Steering: put a thumb anywhere on the left half and slide it. Where it lands is straight ahead; "
             "this far to a side is full lock (shorter is quicker, longer is finer).");
        if (ImGui::Checkbox("Invert steering", &c.steer_invert)) cfg_.save();
        if (ImGui::Checkbox("Hide them while a gamepad is connected", &cfg_.touch_hide_with_pad)) cfg_.save();
        note("They show again for a few seconds whenever the screen is touched. The back button pauses the game.");

        heading("Gamepads and wheels");
        ImGui::Text("Gamepad: %s", devices.pad ? SDL_GetGamepadName(devices.pad) : "none connected");
        std::string joys;
        for (SDL_Joystick *j : devices.joys) {
            const char *n = SDL_GetJoystickName(j);
            joys += (joys.empty() ? "" : ", ") + std::string(n ? n : "joystick");
        }
        ImGui::TextWrapped("Wheels and joysticks: %s", joys.empty() ? "none connected" : joys.c_str());
        const bool *keys = SDL_GetKeyboardState(nullptr);
        const float steer = c.value(SteerRight, keys, devices) - c.value(SteerLeft, keys, devices);
        std::snprintf(text, sizeof text, "%+.2f", c.steer_invert ? -steer : steer);
        label("Steering");
        ImGui::ProgressBar(0.5f + 0.5f * (c.steer_invert ? -steer : steer), ImVec2(240 * u, 0), text);
        label("Accelerator");
        ImGui::ProgressBar(c.value(Accelerate, keys, devices), ImVec2(240 * u, 0));
        label("Brake");
        ImGui::ProgressBar(c.value(Brake, keys, devices), ImVec2(240 * u, 0));
        std::snprintf(text, sizeof text, "%d%%", int(c.deadzone * 100 + 0.5f));
        int dz = int(c.deadzone * 100 + 0.5f);
        label("Dead zone");
        if (stepper("deadzone", dz, 0, 40, 2, text)) c.deadzone = dz / 100.0f, cfg_.save();
        int ffb = int(cfg_.ffb_strength * 100 + 0.5f);
        std::snprintf(text, sizeof text, ffb ? "%d%%" : "Off", ffb);
        label("Rumble");
        if (stepper("ffb", ffb, 0, 100, 10, text)) cfg_.ffb_strength = ffb / 100.0f, cfg_.save();

        heading("Buttons");
        if (capture_action_ >= 0) {
            ImGui::TextColored(kWarn, "Binding %s: press a button or move a stick.", action_name(Action(capture_action_)));
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) capture_action_ = -1, joy_tracking_ = false;
        }
        if (ImGui::BeginTable("binds", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
            ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthFixed, 150 * u);
            ImGui::TableSetupColumn("Gamepad", ImGuiTableColumnFlags_WidthFixed, 290 * u);
            ImGui::TableSetupColumn("Wheel / joystick");
            ImGui::TableHeadersRow();
            for (int a = 0; a < kNumActions; a++) {
                ImGui::PushID(a);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(action_name(Action(a)));
                ImGui::TableNextColumn();
                const bool cap_pad = capture_action_ == a && capture_kind_ == CapturePad;
                const std::string pn = cap_pad ? "Press or move..." : c.bind[a].pad.describe();
                if (ImGui::Button(pn.c_str(), ImVec2(200 * u, 0))) start_capture(a, CapturePad, devices);
                ImGui::SameLine();
                if (ImGui::Button("Clear")) c.bind[a].pad = PadInput{}, cfg_.save();
                ImGui::TableNextColumn();
                const bool cap_joy = capture_action_ == a && capture_kind_ == CaptureJoy;
                const std::string jn = !cap_joy ? c.bind[a].joy.describe(devices)
                                       : joy_tracking_ ? "As far as you want, then let go..."
                                                       : "Press, or move fully and let go...";
                if (ImGui::Button(jn.c_str(), ImVec2(-70 * u, 0))) start_capture(a, CaptureJoy, devices);
                ImGui::SameLine();
                if (ImGui::Button("Clear##joy")) c.bind[a].joy = JoyInput{}, cfg_.save();
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if (ImGui::Button("Reset to defaults")) c.set_defaults(), cfg_.save();
        break;
    }
    case 3: { // Audio
        heading("Volume");
        char text[16];
        auto percent = [&](const char *name, float &value) {
            int v = int(value * 100 + 0.5f);
            std::snprintf(text, sizeof text, "%d%%", v);
            label(name);
            if (stepper(name, v, 0, 100, 5, text)) value = v / 100.0f, cfg_.save();
        };
        percent("Volume", cfg_.volume);
        label("");
        if (ImGui::Checkbox("Mute", &cfg_.mute)) cfg_.save();
        percent("Music", cfg_.music_volume);
        percent("Effects", cfg_.effects_volume);
        note("The balance between the music and everything else (the engine, skids, crashes). 100% and 100% "
             "is the game as the arcade's sound board mixes it.");
        heading("Engine");
        if (ImGui::Checkbox("Native audio (experimental, applies on Reset)", &cfg_.native_audio)) cfg_.save();
        note("A native sequencer and mixer instead of the recompiled sound board: lighter on the CPU.");
        break;
    }
    case 4: { // Link
        heading("Link play (experimental, applies on Reset)");
        if (ImGui::Checkbox("Link to other cabinets", &cfg_.link)) cfg_.save();
        ImGui::BeginDisabled(!cfg_.link);
        label("This cabinet's port");
        ImGui::SetNextItemWidth(160 * u);
        if (ImGui::InputInt("##port", &cfg_.link_port, 0, 0)) cfg_.link_port = std::clamp(cfg_.link_port, 1, 65535), cfg_.save();
        label("Next cabinet");
        ImGui::SetNextItemWidth(300 * u);
        if (ImGui::InputText("##next", link_next_buf_, sizeof link_next_buf_)) cfg_.link_next = link_next_buf_, cfg_.save();
        if (ImGui::Checkbox("Frame sync (every cabinet waits for the master)", &cfg_.link_framesync)) cfg_.save();
        ImGui::EndDisabled();
        ImGui::TextDisabled("Link: %s", link_status_.c_str());
        note("Cabinets link in a ring over Wi-Fi: each listens on its port and connects to the next (host:port); "
             "with two, each one's next is the other. Then in the test menu set LINK ID (one MASTER, the others "
             "SLAVE) and a different CAR NUMBER on each. Revision A (1994) has link play.");
        break;
    }
    }
    ImGui::PopTextWrapPos();
    ImGui::Dummy(ImVec2(0, 24 * u)); // room under the last item

    // Scrolling with a finger: a press that moves far enough becomes a scroll
    // (and stops being a tap on whatever it began on), and keeps going a
    // little once let go.
    const ImGuiIO &io = ImGui::GetIO();
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
        ImGui::IsMouseClicked(0))
        drag_armed_ = true, dragging_ = false, fling_ = 0;
    if (drag_armed_ && io.MouseDown[0]) {
        if (!dragging_ && std::abs(ImGui::GetMouseDragDelta(0).y) > 12 * u) {
            dragging_ = true;
            ImGui::ClearActiveID();
        }
        if (dragging_) {
            ImGui::SetScrollY(ImGui::GetScrollY() - io.MouseDelta.y);
            fling_ = io.MouseDelta.y;
        }
    } else {
        drag_armed_ = dragging_ = false;
        if (std::abs(fling_) > 0.5f) {
            ImGui::SetScrollY(ImGui::GetScrollY() - fling_);
            fling_ *= 0.92f;
        } else fling_ = 0;
    }
    ImGui::EndChild();
    ImGui::End();
    return result;
}

} // namespace app

#endif
