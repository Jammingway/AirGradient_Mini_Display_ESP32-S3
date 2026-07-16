#include "InfoWidget.h"
#include <cstring>

// lv_label_set_text() invalidates and repaints unconditionally — it does not
// compare first. The status tick runs at 1 Hz over rows that rarely change
// (IP, sensor host, uptime), and every needless repaint is another write into
// the PSRAM framebuffer competing with the RGB panel's bounce-buffer refill.
// Comparing first is far cheaper than redrawing.
static void setLabelIfChanged(lv_obj_t* label, const char* text) {
    if (!label) return;
    const char* cur = lv_label_get_text(label);
    if (cur && strcmp(cur, text) == 0) return;
    lv_label_set_text(label, text);
}

void InfoWidget::create(lv_obj_t* parent, const ThemeManager& theme) {
    const ThemePalette& p = theme.palette();
    _root = makeCard(parent, p);

    // Stack: header, rule, then (WiFi only) the big status line, then detail
    // rows. Cross-axis CENTER only affects the rule — everything else is full
    // width and so is unmoved by it.
    lv_obj_set_flex_flow(_root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(_root, 2, 0);
    lv_obj_set_flex_align(_root, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t* head = lv_obj_create(_root);
    lv_obj_remove_style_all(head);
    lv_obj_set_size(head, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(head, 8, 0);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    _icon = lv_label_create(head);
    lv_obj_set_style_text_font(_icon, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(_icon, p.textMuted, 0);
    lv_label_set_text(_icon, _kind == Kind::Wifi ? LV_SYMBOL_WIFI : LV_SYMBOL_REFRESH);

    _nameLbl = lv_label_create(head);
    lv_obj_set_style_text_font(_nameLbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(_nameLbl, p.textMuted, 0);
    lv_label_set_text(_nameLbl, _kind == Kind::Wifi ? "WiFi" : "System");

    // Rule separating the header from the data below it.
    lv_obj_t* rule = lv_obj_create(_root);
    lv_obj_remove_style_all(rule);
    lv_obj_set_size(rule, LV_PCT(66), 1);
    lv_obj_set_style_bg_color(rule, p.textMuted, 0);
    lv_obj_set_style_bg_opa(rule, LV_OPA_60, 0);
    lv_obj_set_style_margin_ver(rule, 3, 0);

    if (_kind == Kind::Wifi) {
        _valueLbl = lv_label_create(_root);
        lv_obj_set_style_text_font(_valueLbl, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(_valueLbl, p.text, 0);
        lv_label_set_text(_valueLbl, "--");
        lv_label_set_long_mode(_valueLbl, LV_LABEL_LONG_DOT);
        lv_obj_set_width(_valueLbl, LV_PCT(100));

        _rowVal[WifiIp] = addRow(p, "IP:");
        _rowVal[WifiSensor] = addRow(p, "Sensor:");
    } else {
        // The System card has no headline; "Last polled:" is its first row.
        _rowVal[SysLastPolled] = addRow(p, "Last polled:");
        _rowVal[SysUptime] = addRow(p, "Uptime:");
        _rowVal[SysSensorUptime] = addRow(p, "Sensor Uptime:");
    }
}

lv_obj_t* InfoWidget::addRow(const ThemePalette& p, const char* label) {
    lv_obj_t* row = lv_obj_create(_root);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    // Roughly one character at font 14: the closest a value may come to the
    // label's colon before it has to give up ground.
    lv_obj_set_style_pad_column(row, 8, 0);

    // Label keeps its natural width against the left frame edge.
    lv_obj_t* lbl = lv_label_create(row);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, p.textMuted, 0);
    lv_label_set_text(lbl, label);

    // The value claims every remaining pixel and right-aligns inside it, so a
    // long IP or hostname simply extends further left instead of wrapping.
    // LV_LABEL_LONG_DOT would NOT do this: with an auto height it never runs
    // out of vertical room, so it wraps instead of ellipsizing.
    lv_obj_t* val = lv_label_create(row);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(val, p.text, 0);
    lv_obj_set_style_text_align(val, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(val, LV_LABEL_LONG_CLIP);
    lv_obj_set_flex_grow(val, 1);
    lv_label_set_text(val, "--");
    return val;
}

void InfoWidget::update(const AirGradientReading&, const AppSettings&, const ThemeManager&) {
    // Content comes via setText()/setRow() from the dashboard's status tick.
}

void InfoWidget::setText(const String& value, bool alert, const ThemeManager& theme) {
    lv_obj_t* target = _valueLbl ? _valueLbl : _rowVal[SysLastPolled];
    if (!target) return;
    setLabelIfChanged(target, value.c_str());
    // Restyling invalidates too, so only recolor on an actual state flip.
    if (alert != _alert) {
        _alert = alert;
        lv_obj_set_style_text_color(
            target, alert ? theme.severityColor(Severity::Bad) : theme.palette().text, 0);
    }
}

void InfoWidget::setRow(Row row, const String& value) {
    if (row >= MAX_ROWS || !_rowVal[row]) return;
    setLabelIfChanged(_rowVal[row], value.length() ? value.c_str() : "--");
}
