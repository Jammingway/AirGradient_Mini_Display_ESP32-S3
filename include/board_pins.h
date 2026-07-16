/**
 * @file board_pins.h
 * Pin map for the Waveshare ESP32-S3-Touch-LCD-4.3.
 * Source: Waveshare wiki / ESP_Panel_Board reference for this board.
 */
#pragma once

// ---------- 16-bit parallel RGB LCD (ST7262 panel, 800x480) ----------
#define LCD_H_RES 800
#define LCD_V_RES 480
// 12 MHz. DO NOT CHANGE — this sits in a narrow window with failures on BOTH
// sides, each one measured on hardware, not reasoned about:
//   16 MHz -> faint edge ghosting near high-contrast text.
//   12 MHz -> good. ~28 Hz refresh (12e6 / (820*516)); the backlight is
//             constant DC, so a static dashboard shows no flicker.
//   10 MHz -> panel never locks: solid white from power-on, degrading through
//             an argyle pattern to black on reset. Below the ST7262's usable
//             pixel-clock range; this is not a bandwidth symptom.
// The RGB peripheral has no frame memory — it streams pixels live from the
// PSRAM framebuffer through the bounce-buffer refill ISR, so starving that DMA
// underruns the panel FIFO (streaks at the left edge, where each scanline
// starts; a permanent vertical shift if it loses frame sync). Lowering PCLK
// buys bandwidth margin, but 10 MHz proves you run out of panel before you run
// out of margin. Fix bandwidth elsewhere (less redraw traffic), not here.
#define LCD_PIXEL_CLOCK_HZ (12 * 1000 * 1000)

#define LCD_PIN_HSYNC 46
#define LCD_PIN_VSYNC 3
#define LCD_PIN_DE    5
#define LCD_PIN_PCLK  7
#define LCD_PIN_DISP  (-1)  // display-enable handled by CH422G backlight

// Data lines D0..D15 = B3..B7, G2..G7, R3..R7
#define LCD_PIN_D0  14  // B3
#define LCD_PIN_D1  38  // B4
#define LCD_PIN_D2  18  // B5
#define LCD_PIN_D3  17  // B6
#define LCD_PIN_D4  10  // B7
#define LCD_PIN_D5  39  // G2
#define LCD_PIN_D6  0   // G3 -- ALSO GPIO0, the boot-strap pin (low at reset =
                        // enter UART download mode). Confirmed on hardware:
                        // the physical RESET button is unreliable while the
                        // display is running, because this line is actively
                        // driven as part of normal rendering. Cold power-on
                        // is unaffected (nothing drives it yet at the strap-
                        // sample instant). Waveshare's wiring, not ours; there
                        // is no firmware fix for a hardware EN-pin reset. Use
                        // Settings -> General -> "Restart Device" (calls
                        // ESP.restart(), a different reset path) instead.
#define LCD_PIN_D7  45  // G4
#define LCD_PIN_D8  48  // G5
#define LCD_PIN_D9  47  // G6
#define LCD_PIN_D10 21  // G7
#define LCD_PIN_D11 1   // R3
#define LCD_PIN_D12 2   // R4
#define LCD_PIN_D13 42  // R5
#define LCD_PIN_D14 41  // R6
#define LCD_PIN_D15 40  // R7

// RGB timing (per Waveshare reference port)
#define LCD_HSYNC_PULSE 4
#define LCD_HSYNC_BACK  8
#define LCD_HSYNC_FRONT 8
#define LCD_VSYNC_PULSE 4
#define LCD_VSYNC_BACK  16
#define LCD_VSYNC_FRONT 16

// ---------- I2C bus (touch + IO expander + RTC/etc. headers) ----------
#define I2C_PIN_SDA 8
#define I2C_PIN_SCL 9

// GT911 capacitive touch controller
#define TOUCH_I2C_ADDR_PRIMARY 0x5D
#define TOUCH_I2C_ADDR_ALT     0x14
#define TOUCH_PIN_INT 4

// ---------- CH422G IO expander (EXIO lines) ----------
#define EXIO_TP_RST  1
#define EXIO_LCD_BL  2
#define EXIO_LCD_RST 3
#define EXIO_SD_CS   4
#define EXIO_USB_SEL 5
