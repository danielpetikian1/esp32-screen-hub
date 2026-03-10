/**
 * @file ui_clock.c
 * @brief Clock screen: full-screen large time display.
 */

#include "ui_screens.h"

static lv_obj_t *s_lbl_clock_big;

void ui_clock_build(lv_obj_t *tile) {
	s_lbl_clock_big = lv_label_create(tile);
	lv_label_set_text(s_lbl_clock_big, "--:--:--");
	lv_obj_set_style_text_font(s_lbl_clock_big, &lv_font_montserrat_40,
							   LV_PART_MAIN);
	lv_obj_center(s_lbl_clock_big);
}

void ui_clock_set_time(const char *hhmmss) {
	lv_label_set_text(s_lbl_clock_big, hhmmss);
}
