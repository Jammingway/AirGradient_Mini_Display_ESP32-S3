#include "DisplayDriver.h"
#include "board_pins.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_idf_version.h"
#include "../utils/Logger.h"

bool DisplayDriver::begin() {
    if (!_expander.begin()) {
        LOG_E("display", "CH422G expander not responding");
        return false;
    }

    // Power-up sequencing: hold resets, backlight off until first frame is ready.
    _expander.setPin(EXIO_LCD_BL, false);
    _expander.setPin(EXIO_TP_RST, true);
    _expander.setPin(EXIO_LCD_RST, true);
    delay(100);

    esp_lcd_rgb_panel_config_t cfg = {};
    cfg.clk_src = LCD_CLK_SRC_DEFAULT;
    cfg.timings.pclk_hz = LCD_PIXEL_CLOCK_HZ;
    cfg.timings.h_res = LCD_H_RES;
    cfg.timings.v_res = LCD_V_RES;
    cfg.timings.hsync_pulse_width = LCD_HSYNC_PULSE;
    cfg.timings.hsync_back_porch  = LCD_HSYNC_BACK;
    cfg.timings.hsync_front_porch = LCD_HSYNC_FRONT;
    cfg.timings.vsync_pulse_width = LCD_VSYNC_PULSE;
    cfg.timings.vsync_back_porch  = LCD_VSYNC_BACK;
    cfg.timings.vsync_front_porch = LCD_VSYNC_FRONT;
    cfg.timings.flags.pclk_active_neg = 1;
    cfg.data_width = 16;
    cfg.bits_per_pixel = 16;
    cfg.num_fbs = 1;
    // Bounce buffers keep the panel fed from internal RAM while WiFi is
    // hammering the PSRAM bus — without them the image drifts/flickers.
    cfg.bounce_buffer_size_px = LCD_H_RES * 10;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
    cfg.dma_burst_size = 64;
#else
    cfg.psram_trans_align = 64;
#endif
    cfg.hsync_gpio_num = LCD_PIN_HSYNC;
    cfg.vsync_gpio_num = LCD_PIN_VSYNC;
    cfg.de_gpio_num    = LCD_PIN_DE;
    cfg.pclk_gpio_num  = LCD_PIN_PCLK;
    cfg.disp_gpio_num  = LCD_PIN_DISP;
    const int dataPins[16] = {
        LCD_PIN_D0,  LCD_PIN_D1,  LCD_PIN_D2,  LCD_PIN_D3,
        LCD_PIN_D4,  LCD_PIN_D5,  LCD_PIN_D6,  LCD_PIN_D7,
        LCD_PIN_D8,  LCD_PIN_D9,  LCD_PIN_D10, LCD_PIN_D11,
        LCD_PIN_D12, LCD_PIN_D13, LCD_PIN_D14, LCD_PIN_D15,
    };
    for (int i = 0; i < 16; i++) cfg.data_gpio_nums[i] = dataPins[i];
    cfg.flags.fb_in_psram = 1;

    if (esp_lcd_new_rgb_panel(&cfg, &_panel) != ESP_OK) {
        LOG_E("display", "esp_lcd_new_rgb_panel failed");
        return false;
    }
    if (esp_lcd_panel_reset(_panel) != ESP_OK || esp_lcd_panel_init(_panel) != ESP_OK) {
        LOG_E("display", "panel reset/init failed");
        return false;
    }
    return true;
}

void DisplayDriver::setBacklight(bool on) {
    _expander.setPin(EXIO_LCD_BL, on);
    _backlightOn = on;
}

void DisplayDriver::drawBitmap(int x1, int y1, int x2, int y2, const void* pixels) {
    if (_panel) esp_lcd_panel_draw_bitmap(_panel, x1, y1, x2, y2, pixels);
}
