/**
 * @file CascadeSplash.h
 * Reusable boot screen: holds a logo + wordmark, then "crashes" into a
 * cascade of green terminal windows rapidly spawning to fill the screen.
 *
 * DROP-IN MODULE — depends only on LVGL. No project headers, no board pins
 * (resolution comes from the active display), no ESP-specific RNG. Copy
 * CascadeSplash.{h,cpp} into any LVGL 9 project and call show().
 *
 * Pairs with WoprGreeting; chain them by calling the next screen from the
 * DoneCallback. Currently unreferenced in this firmware — kept on purpose.
 *
 *   CascadeSplash splash;
 *   splash.show(lv_color_hex(0x33FF66), &my_logo, "<wordmark>",
 *               [] { next_screen(); });
 */
#pragma once
#include <lvgl.h>
#include <functional>

class CascadeSplash {
public:
    using DoneCallback = std::function<void()>;

    // Builds and loads the screen; fires cb once the cascade fills. The
    // caller owns what happens next — typically loading another screen with
    // deletePrev so this one's objects are reclaimed.
    // logo/wordmark may be null to show either alone.
    void show(lv_color_t accent, const lv_image_dsc_t* logo, const char* wordmark,
              DoneCallback cb);

    lv_obj_t* screen() { return _screen; }

private:
    enum class Phase : uint8_t { Logo, Cascade };

    static void tickCb(lv_timer_t* t);
    void onTick();
    void spawnWindow();
    void finish();

    lv_obj_t* _screen = nullptr;
    lv_timer_t* _timer = nullptr;
    lv_color_t _accent{};
    DoneCallback _done;

    Phase _phase = Phase::Logo;
    uint32_t _elapsed = 0;
    int _winCount = 0;

    static constexpr uint32_t TICK_MS = 55;
    static constexpr uint32_t LOGO_MS = 2200;   // hold the logo before the "crash"
    static constexpr int SPAWN_PER_TICK = 1;
    // Kept modest on purpose: the cascade repeatedly invalidates the whole
    // panel, and a heavier storm coinciding with WiFi startup current was
    // enough to brown out an ESP32-S3 board mid-boot. 20 keeps the effect
    // without the reset loop.
    static constexpr int MAX_WINDOWS = 20;
};
