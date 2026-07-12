/**
 * @file LvglPort.h
 * LVGL 9 glue: display flush, pointer input, tick source.
 */
#pragma once
#include <Arduino.h>
#include <lvgl.h>

class DisplayDriver;
class GT911Touch;

class LvglPort {
public:
    bool begin(DisplayDriver& display, GT911Touch& touch);

    // Software "brightness": a black overlay on the top layer.
    // 100 = full brightness, 20 = dimmest allowed.
    void setBrightness(uint8_t percent);

private:
    static void flushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* pxMap);
    static void touchCb(lv_indev_t* indev, lv_indev_data_t* data);

    DisplayDriver* _display = nullptr;
    GT911Touch* _touch = nullptr;
    lv_display_t* _lvDisplay = nullptr;
    lv_indev_t* _lvIndev = nullptr;
    lv_obj_t* _dimLayer = nullptr;
};
