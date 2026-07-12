#include "ThemeManager.h"

void ThemeManager::apply(const String& name) {
    if (name == "light") {
        _p.bg = lv_color_hex(0xF4F6F8);
        _p.card = lv_color_hex(0xFFFFFF);
        _p.cardBorder = lv_color_hex(0xE0E5EC);
        _p.text = lv_color_hex(0x1B2028);
        _p.textMuted = lv_color_hex(0x667085);
        _p.accent = lv_color_hex(0x0288D1);
        _p.terminalGreen = lv_color_hex(0x00A040);
        _p.dark = false;
    } else {  // dark (default)
        _p.bg = lv_color_hex(0x0F1115);
        _p.card = lv_color_hex(0x1A1E26);
        _p.cardBorder = lv_color_hex(0x262C38);
        _p.text = lv_color_hex(0xECEFF4);
        _p.textMuted = lv_color_hex(0x8A93A5);
        _p.accent = lv_color_hex(0x4FC3F7);
        _p.terminalGreen = lv_color_hex(0x33FF66);
        _p.dark = true;
    }
}

lv_color_t ThemeManager::severityColor(Severity s) const {
    switch (s) {
        case Severity::Good:     return lv_color_hex(0x3DDC84);
        case Severity::Moderate: return lv_color_hex(0xFFD54F);
        case Severity::Poor:     return lv_color_hex(0xFF9800);
        case Severity::Bad:      return lv_color_hex(0xEF5350);
        case Severity::Severe:   return lv_color_hex(0xAB47BC);
        case Severity::Neutral:  return _p.accent;
    }
    return _p.accent;
}
