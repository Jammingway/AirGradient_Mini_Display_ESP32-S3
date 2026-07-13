/**
 * @file SplashScreen.h
 * Boot splash: anime red panda + "<jammingway>" wordmark, then the
 * artwork blanks out and control passes to the boot terminal.
 */
#pragma once
#include <lvgl.h>
#include <functional>

class ThemeManager;

class SplashScreen {
public:
    using DoneCallback = std::function<void()>;

    // Builds and loads the screen; fires cb after hold + fade-out.
    void show(const ThemeManager& theme, DoneCallback cb);

    lv_obj_t* screen() { return _screen; }

private:
    lv_obj_t* _screen = nullptr;
    lv_obj_t* _img = nullptr;
    lv_obj_t* _name = nullptr;
    DoneCallback _done;

    static constexpr uint32_t HOLD_MS = 3000;
    static constexpr uint32_t FADE_MS = 1000;
};
