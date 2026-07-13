/**
 * @file SplashScreen.h
 * Boot sequence: the Jammingway logo, then a cascading "window failure"
 * (green terminal windows rapidly spawning to fill the screen), then the
 * WOPR greeting typed out, then control passes to the boot terminal.
 * Driven by a single LVGL timer so it never blocks the UI thread.
 */
#pragma once
#include <lvgl.h>
#include <functional>

class ThemeManager;

class SplashScreen {
public:
    using DoneCallback = std::function<void()>;

    // Builds and loads the screen; fires cb after the full sequence.
    void show(const ThemeManager& theme, DoneCallback cb);

    lv_obj_t* screen() { return _screen; }

private:
    enum class Phase : uint8_t { Logo, Cascade, Wopr };

    static void tickCb(lv_timer_t* t);
    void onTick();
    void spawnWindow();
    void startWopr();
    void finish();

    lv_obj_t* _screen = nullptr;
    lv_obj_t* _img = nullptr;
    lv_obj_t* _name = nullptr;
    lv_obj_t* _wopr = nullptr;
    lv_timer_t* _timer = nullptr;
    lv_color_t _green{};
    DoneCallback _done;

    Phase _phase = Phase::Logo;
    uint32_t _elapsed = 0;
    uint32_t _holdElapsed = 0;
    int _winCount = 0;
    size_t _typed = 0;

    static constexpr uint32_t TICK_MS = 40;
    static constexpr uint32_t LOGO_MS = 2200;    // hold the logo before the "crash"
    static constexpr int SPAWN_PER_TICK = 2;
    static constexpr int MAX_WINDOWS = 44;
    static constexpr uint32_t WOPR_HOLD_MS = 1400;  // after the line finishes typing
    static constexpr const char* WOPR_TEXT =
        "Greetings Professor Falken.\n\nShall we play a game?";
};
