/**
 * @file lv_conf.h
 * LVGL 9.3 configuration for AirGradient Mini Display (ESP32-S3, 800x480 RGB565).
 * Only overrides are listed; everything else falls back to LVGL defaults
 * via lv_conf_internal.h.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

/*==================== COLOR ====================*/
#define LV_COLOR_DEPTH 16

/*==================== MEMORY ====================
 * Use the C library allocator: with BOARD_HAS_PSRAM the ESP32 heap
 * transparently spills large allocations into the 8MB PSRAM. */
#define LV_USE_STDLIB_MALLOC  LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING  LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB

/*==================== HAL ====================*/
#define LV_DEF_REFR_PERIOD 16   /* ~60 FPS refresh cadence */
#define LV_DPI_DEF 120

/*==================== FEATURES ====================*/
#define LV_USE_LOG 0
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1

/*==================== FONTS ====================*/
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_40 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_UNSCII_8  1
#define LV_FONT_UNSCII_16 1     /* retro terminal font for the boot console */
#define LV_FONT_DEFAULT &lv_font_montserrat_16

/*==================== WIDGETS ====================
 * Defaults enable the common set (label, btn, slider, switch, textarea,
 * keyboard, tabview, dropdown, bar, arc, ...). Nothing to disable. */

#endif /* LV_CONF_H */
