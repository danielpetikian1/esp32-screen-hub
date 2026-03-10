/**
 * @file ui_stats.c
 * @brief Indoor sensor dashboard screen: 2×2 card grid.
 *
 * Dark navy background (#0D111F) matching the weather screen palette.
 * Layout (320×240):
 *   – Slim header bar: "Indoor" title (left) + HH:MM:SS clock (right),
 *     both in muted blue (#6A8FAF).
 *   – 2×2 flex grid below: TEMP | HUM
 *                           CO2  | TVOC
 *     Each card has a dark background (#1A2535), rounded corners, and
 *     a two-line layout: muted category title + white value (montserrat_20).
 *
 * CO2 and TVOC values are colour-coded by air-quality threshold:
 *   Green  (#4CAF50): good    (CO2 <800 ppm  / TVOC <200 ppb)
 *   Yellow (#FFC107): moderate (CO2 <1500 ppm / TVOC <500 ppb)
 *   Red    (#F44336): poor    (CO2 ≥1500 ppm / TVOC ≥500 ppb)
 *
 * The TEMP card is tappable and toggles the active temperature unit
 * (°C ↔ °F) shared with the weather screen via ui_get/set_temp_unit().
 */

#include "bsp/m5stack_core_s3.h"
#include "ui.h"
#include "ui_screens.h"
#include <stdio.h>

/* Colour palette */
#define CLR_BG 0x0D111F
#define CLR_CARD 0x1A2535
#define CLR_MUTED 0x6A8FAF
#define CLR_GREEN 0x4CAF50
#define CLR_YELLOW 0xFFC107
#define CLR_RED 0xF44336

/* All LVGL handles owned by this screen. */
static struct {
	lv_obj_t *lbl_title;
	lv_obj_t *lbl_time;
	lv_obj_t *lbl_temp_val;
	lv_obj_t *lbl_hum_val;
	lv_obj_t *lbl_co2_val;
	lv_obj_t *lbl_tvoc_val;
} s_stats;

/**
 * @brief Toggle the global temperature unit on tap.
 *
 * Registered on the TEMP card. The change is picked up by both this screen
 * and the weather screen on the next ui_task tick (~150 ms).
 */
static void on_temp_clicked(lv_event_t *e) {
	(void)e;
	ui_set_temp_unit(ui_get_temp_unit() == UI_TEMP_C ? UI_TEMP_F : UI_TEMP_C);
}

/**
 * @brief Create a styled sensor card with a muted title and white value label.
 *
 * Cards fill 48% width × 45% height of the grid container so four of them
 * fit in a 2×2 arrangement with small gaps between them.
 *
 * @param parent   The flex-wrap grid container.
 * @param title    Short sensor name shown above the value (e.g. "TEMP").
 * @param out_val  Receives the value label handle for later updates.
 *
 * @return The created card object.
 */
static lv_obj_t *create_sensor_card(lv_obj_t *parent, const char *title,
									lv_obj_t **out_val) {
	lv_obj_t *card = lv_obj_create(parent);
	lv_obj_set_size(card, lv_pct(48), lv_pct(45));
	lv_obj_set_layout(card, LV_LAYOUT_FLEX);
	lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
						  LV_FLEX_ALIGN_CENTER);
	lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

	/* Dark card: no default LVGL border/shadow, custom radius */
	lv_obj_set_style_bg_color(card, lv_color_hex(CLR_CARD), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
	lv_obj_set_style_radius(card, 8, LV_PART_MAIN);
	lv_obj_set_style_pad_all(card, 4, LV_PART_MAIN);

	/* Category label: small and muted, same style as weather stats column */
	lv_obj_t *lbl_title = lv_label_create(card);
	lv_label_set_text(lbl_title, title);
	lv_obj_set_style_text_color(lbl_title, lv_color_hex(CLR_MUTED),
								LV_PART_MAIN);
	lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_16, LV_PART_MAIN);

	/* Value label: large white text — the primary readable number */
	lv_obj_t *lbl_val = lv_label_create(card);
	lv_label_set_text(lbl_val, "--");
	lv_obj_set_style_text_color(lbl_val, lv_color_white(), LV_PART_MAIN);
	lv_obj_set_style_text_font(lbl_val, &lv_font_montserrat_20, LV_PART_MAIN);

	if (out_val)
		*out_val = lbl_val;

	return card;
}

/**
 * @brief Populate the stats tile with the header bar and sensor card grid.
 *
 * @param tile Tileview tile provided by ui_init().
 */
void ui_stats_build(lv_obj_t *tile) {
	/* Dark background matching the weather screen */
	lv_obj_set_style_bg_color(tile, lv_color_hex(CLR_BG), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, LV_PART_MAIN);

	/* Header: title label pinned to top-left */
	s_stats.lbl_title = lv_label_create(tile);
	lv_label_set_text(s_stats.lbl_title, "Indoor");
	lv_obj_set_style_text_color(s_stats.lbl_title, lv_color_hex(CLR_MUTED),
								LV_PART_MAIN);
	lv_obj_set_style_text_font(s_stats.lbl_title, &lv_font_montserrat_16,
							   LV_PART_MAIN);
	lv_obj_align(s_stats.lbl_title, LV_ALIGN_TOP_LEFT, 8, 8);

	/* Header: live clock pinned to top-right */
	s_stats.lbl_time = lv_label_create(tile);
	lv_label_set_text(s_stats.lbl_time, "--:--:--");
	lv_obj_set_style_text_color(s_stats.lbl_time, lv_color_hex(CLR_MUTED),
								LV_PART_MAIN);
	lv_obj_set_style_text_font(s_stats.lbl_time, &lv_font_montserrat_16,
							   LV_PART_MAIN);
	lv_obj_align(s_stats.lbl_time, LV_ALIGN_TOP_RIGHT, -8, 8);

	/* Card grid: transparent container with row-wrap flex.
	 * LV_FLEX_FLOW_ROW_WRAP places the 4 cards into 2 rows automatically. */
	lv_obj_t *grid = lv_obj_create(tile);
	lv_obj_set_size(grid, lv_pct(100), lv_pct(82));
	lv_obj_align(grid, LV_ALIGN_BOTTOM_MID, 0, -4);
	lv_obj_set_layout(grid, LV_LAYOUT_FLEX);
	lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
	lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
						  LV_FLEX_ALIGN_CENTER);
	lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(grid, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_row(grid, 8, 0);
	lv_obj_set_style_pad_column(grid, 8, 0);
	lv_obj_set_style_pad_hor(grid, 6, 0);
	lv_obj_set_style_pad_ver(grid, 4, 0);

	/* Create the 4 sensor cards; only TEMP is tappable (unit toggle). */
	lv_obj_t *card_temp =
		create_sensor_card(grid, "TEMP", &s_stats.lbl_temp_val);
	create_sensor_card(grid, "HUM", &s_stats.lbl_hum_val);
	create_sensor_card(grid, "CO2", &s_stats.lbl_co2_val);
	create_sensor_card(grid, "TVOC", &s_stats.lbl_tvoc_val);

	lv_obj_add_flag(card_temp, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_event_cb(card_temp, on_temp_clicked, LV_EVENT_CLICKED, NULL);
}

/** Update the small time label in the stats header. */
void ui_stats_set_time(const char *hhmmss) {
	lv_label_set_text(s_stats.lbl_time, hhmmss);
}

/**
 * @brief Refresh all sensor value labels with the latest readings.
 *
 * Temperature is converted to the active unit before display.
 * CO2 and TVOC values are colour-coded by air-quality threshold.
 *
 * @param s Latest sensor snapshot from readings_get_snapshot().
 */
void ui_update_readings(const readings_snapshot_t *s) {
	if (!s)
		return;

	char buf[32];

	bsp_display_lock(0);

	/* Temperature: convert if Fahrenheit mode is active */
	float temp = (ui_get_temp_unit() == UI_TEMP_F)
					 ? (s->temp_c * 9.0f / 5.0f + 32.0f)
					 : s->temp_c;
	snprintf(buf, sizeof(buf), "%.1f\xc2\xb0%s", temp,
			 ui_get_temp_unit() == UI_TEMP_F ? "F" : "C");
	lv_label_set_text(s_stats.lbl_temp_val, buf);

	snprintf(buf, sizeof(buf), "%.1f%%", s->rh_percent);
	lv_label_set_text(s_stats.lbl_hum_val, buf);

	/* CO2: colour-coded green → yellow → red */
	snprintf(buf, sizeof(buf), "%u ppm", (unsigned)s->eco2_ppm);
	lv_label_set_text(s_stats.lbl_co2_val, buf);
	lv_color_t co2_color;
	if (s->eco2_ppm < 800)
		co2_color = lv_color_hex(CLR_GREEN);
	else if (s->eco2_ppm < 1500)
		co2_color = lv_color_hex(CLR_YELLOW);
	else
		co2_color = lv_color_hex(CLR_RED);
	lv_obj_set_style_text_color(s_stats.lbl_co2_val, co2_color, LV_PART_MAIN);

	/* TVOC: colour-coded green → yellow → red */
	snprintf(buf, sizeof(buf), "%u ppb", (unsigned)s->tvoc_ppb);
	lv_label_set_text(s_stats.lbl_tvoc_val, buf);
	lv_color_t tvoc_color;
	if (s->tvoc_ppb < 200)
		tvoc_color = lv_color_hex(CLR_GREEN);
	else if (s->tvoc_ppb < 500)
		tvoc_color = lv_color_hex(CLR_YELLOW);
	else
		tvoc_color = lv_color_hex(CLR_RED);
	lv_obj_set_style_text_color(s_stats.lbl_tvoc_val, tvoc_color, LV_PART_MAIN);

	bsp_display_unlock();
}
