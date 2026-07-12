#include "LvglPort.h"
#include "board_pins.h"
#include "DisplayDriver.h"
#include "../touch/GT911Touch.h"
#include "../utils/Logger.h"
#include "esp_heap_caps.h"

// Two partial draw buffers in internal DMA-capable RAM (40 lines each).
static constexpr size_t DRAW_BUF_LINES = 40;
static constexpr size_t DRAW_BUF_BYTES = LCD_H_RES * DRAW_BUF_LINES * 2;

bool LvglPort::begin(DisplayDriver& display, GT911Touch& touch) {
    _display = &display;
    _touch = &touch;

    lv_init();
    lv_tick_set_cb([]() -> uint32_t { return millis(); });

    void* buf1 = heap_caps_malloc(DRAW_BUF_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    void* buf2 = heap_caps_malloc(DRAW_BUF_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (!buf1 || !buf2) {
        LOG_E("lvgl", "draw buffer allocation failed");
        return false;
    }

    _lvDisplay = lv_display_create(LCD_H_RES, LCD_V_RES);
    lv_display_set_user_data(_lvDisplay, this);
    lv_display_set_color_format(_lvDisplay, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(_lvDisplay, buf1, buf2, DRAW_BUF_BYTES, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(_lvDisplay, flushCb);

    _lvIndev = lv_indev_create();
    lv_indev_set_type(_lvIndev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_user_data(_lvIndev, this);
    lv_indev_set_read_cb(_lvIndev, touchCb);

    // Dimming overlay lives on the top layer, above every screen.
    _dimLayer = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(_dimLayer);
    lv_obj_set_size(_dimLayer, LCD_H_RES, LCD_V_RES);
    lv_obj_set_style_bg_color(_dimLayer, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(_dimLayer, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(_dimLayer, LV_OBJ_FLAG_HIDDEN);
    // Let touches pass through to the UI beneath.
    lv_obj_remove_flag(_dimLayer, LV_OBJ_FLAG_CLICKABLE);

    return true;
}

void LvglPort::setBrightness(uint8_t percent) {
    if (!_dimLayer) return;
    percent = constrain(percent, (uint8_t)20, (uint8_t)100);
    if (percent >= 100) {
        lv_obj_add_flag(_dimLayer, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_remove_flag(_dimLayer, LV_OBJ_FLAG_HIDDEN);
    // 100% -> opa 0, 20% -> opa ~200
    uint8_t opa = (uint8_t)((100 - percent) * 255 / 100);
    lv_obj_set_style_bg_opa(_dimLayer, opa, 0);
}

void LvglPort::flushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* pxMap) {
    auto* self = static_cast<LvglPort*>(lv_display_get_user_data(disp));
    self->_display->drawBitmap(area->x1, area->y1, area->x2 + 1, area->y2 + 1, pxMap);
    lv_display_flush_ready(disp);
}

void LvglPort::touchCb(lv_indev_t* indev, lv_indev_data_t* data) {
    auto* self = static_cast<LvglPort*>(lv_indev_get_user_data(indev));
    int16_t x, y;
    if (self->_touch->read(x, y)) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}
