/**
 * @file ui_clock.c
 * @brief Clock screen: full-screen large time display.
 *
 * A single montserrat_40 label centred on the tile. No extra widgets —
 * the entire 320×240 px surface is used for maximum readability.
 */

#include "ui_screens.h"

/* The one label owned by this screen. Static so ui_clock_set_time()
 * can update it without needing a handle passed in. */
static lv_obj_t *s_lbl_clock_big;

/**
 * @brief Populate the clock tile with a large centred time label.
 *
 * @param tile Tileview tile provided by ui_init().
 */
void ui_clock_build(lv_obj_t *tile) {
	s_lbl_clock_big = lv_label_create(tile);
	lv_label_set_text(s_lbl_clock_big, "--:--:--");
	/* montserrat_40 is the largest pre-enabled font; fills the screen well
	 * for a bedside-clock style readout. */
	lv_obj_set_style_text_font(s_lbl_clock_big, &lv_font_montserrat_40,
							   LV_PART_MAIN);
	lv_obj_center(s_lbl_clock_big);
}

/**
 * @brief Update the displayed time string.
 *
 * Called from ui_set_time_str() with the display lock already held.
 *
 * @param hhmmss Null-terminated time string (e.g. "14:05:32").
 */
void ui_clock_set_time(const char *hhmmss) {
	lv_label_set_text(s_lbl_clock_big, hhmmss);
}
