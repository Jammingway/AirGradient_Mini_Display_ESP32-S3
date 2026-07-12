/**
 * @file board_pins.h
 * Pin map for the Waveshare ESP32-S3-Touch-LCD-4.3.
 * Source: Waveshare wiki / ESP_Panel_Board reference for this board.
 */
#pragma once

// ---------- 16-bit parallel RGB LCD (ST7262 panel, 800x480) ----------
#define LCD_H_RES 800
#define LCD_V_RES 480
#define LCD_PIXEL_CLOCK_HZ (16 * 1000 * 1000)

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
#define LCD_PIN_D6  0   // G3
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
