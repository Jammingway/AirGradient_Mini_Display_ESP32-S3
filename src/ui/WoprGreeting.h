/**
 * @file WoprGreeting.h
 * Reusable boot screen: types a line of text out character by character on
 * black, then holds with a blinking cursor. WarGames-inspired.
 *
 * DROP-IN MODULE — depends only on LVGL. No project headers, no board pins
 * (resolution comes from the active display). Copy WoprGreeting.{h,cpp} into
 * any LVGL 9 project and call show().
 *
 * Pairs with CascadeSplash; chain them by calling this from that one's
 * DoneCallback.
 *
 *   WoprGreeting wopr;
 *   wopr.show(lv_color_hex(0x33FF66),
 *             "Greetings Professor Falken.\n\nShall we play a game?",
 *             [] { next_screen(); });
 */
#pragma once
#include <lvgl.h>
#include <functional>

class WoprGreeting {
public:
    using DoneCallback = std::function<void()>;

    // Builds and loads the screen; fires cb after the line finishes typing
    // and the hold expires. `text` must outlive the screen (string literals
    // are the expected case).
    void show(lv_color_t accent, const char* text, DoneCallback cb);

    lv_obj_t* screen() { return _screen; }

private:
    static void tickCb(lv_timer_t* t);
    void onTick();
    void render(char cursor);
    void finish();

    lv_obj_t* _screen = nullptr;
    lv_obj_t* _label = nullptr;
    lv_timer_t* _timer = nullptr;
    lv_color_t _accent{};
    const char* _text = "";
    DoneCallback _done;

    size_t _typed = 0;
    uint32_t _holdElapsed = 0;

    static constexpr uint32_t TICK_MS = 55;
    static constexpr uint32_t HOLD_MS = 1400;   // after the line finishes typing
    static constexpr size_t MAX_TEXT = 128;
};
