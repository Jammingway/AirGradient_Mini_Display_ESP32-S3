#include "Widget.h"

void Widget::place(int originX, int originY, int cellW, int cellH, int gap) {
    if (!_root) return;
    int x = originX + _cell.x * (cellW + gap);
    int y = originY + _cell.y * (cellH + gap);
    int w = _cell.w * cellW + (_cell.w - 1) * gap;
    int h = _cell.h * cellH + (_cell.h - 1) * gap;
    lv_obj_set_pos(_root, x, y);
    lv_obj_set_size(_root, w, h);
}

lv_obj_t* Widget::makeCard(lv_obj_t* parent, const ThemePalette& p) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_style_bg_color(card, p.card, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, p.cardBorder, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}
