/**
 * @file ui_stats.c
 * @brief Indoor sensor dashboard screen: 2×2 card grid.
 *
 * Layout (320×240):
 *   - Header bar: "Indoor Sensor Panel" title (left) + HH:MM:SS clock (right)
 *   - 2×2 flex grid below: TEMP | HUM
 *                           CO2  | TVOC
 *
 * The TEMP card is clickable and toggles the active temperature unit
 * (°C ↔ °F) shared with the weather screen via ui_get/set_temp_unit().
 */

#include "bsp/m5stack_core_s3.h"
#include "ui.h"
#include "ui_screens.h"
#include <stdio.h>

/* All LVGL handles owned by this screen. Kept in a single struct to make
 * the ownership boundary obvious and avoid polluting the global namespace. */
static struct {
	lv_obj_t *lbl_title; /* "Indoor Sensor Panel" */
	lv_obj_t *lbl_time;	 /* HH:MM:SS, top-right corner */
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
 * @brief Create a titled sensor card with a placeholder value label.
 *
 * Cards fill 48% width × 45% height of the grid container so four of them
 * fit in a 2×2 arrangement with small gaps between them.
 *
 * @param parent    The flex-wrap grid container.
 * @param title     Short sensor name shown above the value (e.g. "TEMP").
 * @param out_val   Receives the value label handle for later updates.
 *
 * @return The created card object.
 */
static lv_obj_t *create_sensor_card(lv_obj_t *parent, const char *title,
									lv_obj_t **out_val) {
	lv_obj_t *card = lv_obj_create(parent);
	lv_obj_set_size(card, lv_pct(48), lv_pct(45));
	lv_obj_set_layout(card, LV_LAYOUT_FLEX);
	lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
	lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

	lv_obj_t *lbl_title = lv_label_create(card);
	lv_label_set_text(lbl_title, title);

	lv_obj_t *lbl_val = lv_label_create(card);
	lv_label_set_text(lbl_val,
					  "--"); /* placeholder until first sensor reading */

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
	/* Header: title label pinned to top-left */
	s_stats.lbl_title = lv_label_create(tile);
	lv_label_set_text(s_stats.lbl_title, "Indoor Sensor Panel");
	lv_obj_align(s_stats.lbl_title, LV_ALIGN_TOP_LEFT, 8, 8);

	/* Header: live clock pinned to top-right */
	s_stats.lbl_time = lv_label_create(tile);
	lv_label_set_text(s_stats.lbl_time, "--:--:--");
	lv_obj_align(s_stats.lbl_time, LV_ALIGN_TOP_RIGHT, -10, 10);

	/* Card grid: row-wrap flex occupying the lower 80% of the tile.
	 * LV_FLEX_FLOW_ROW_WRAP automatically places the 4 cards into 2 rows. */
	lv_obj_t *grid = lv_obj_create(tile);
	lv_obj_set_size(grid, lv_pct(100), lv_pct(80));
	lv_obj_align(grid, LV_ALIGN_BOTTOM_MID, 0, -10);
	lv_obj_set_layout(grid, LV_LAYOUT_FLEX);
	lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
	lv_obj_set_style_pad_row(grid, 10, 0);
	lv_obj_set_style_pad_column(grid, 10, 0);

	/* Create the 4 sensor cards; only TEMP is clickable (unit toggle). */
	lv_obj_t *card_temp =
		create_sensor_card(grid, "TEMP", &s_stats.lbl_temp_val);
	create_sensor_card(grid, "HUM", &s_stats.lbl_hum_val);
	create_sensor_card(grid, "CO2", &s_stats.lbl_co2_val);
	create_sensor_card(grid, "TVOC", &s_stats.lbl_tvoc_val);

	lv_obj_add_flag(card_temp, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_event_cb(card_temp, on_temp_clicked, LV_EVENT_CLICKED, NULL);
}

/**
 * @brief Update the small time label in the stats header.
 *
 * Called from ui_set_time_str() with the display lock already held.
 *
 * @param hhmmss Null-terminated time string (e.g. "14:05:32").
 */
void ui_stats_set_time(const char *hhmmss) {
	lv_label_set_text(s_stats.lbl_time, hhmmss);
}

/**
 * @brief Refresh all sensor value labels with the latest readings.
 *
 * Temperature is converted to the active unit before display.
 * All other values are formatted directly from the snapshot.
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
	snprintf(buf, sizeof(buf), "%.1f°%s", temp,
			 ui_get_temp_unit() == UI_TEMP_F ? "F" : "C");
	lv_label_set_text(s_stats.lbl_temp_val, buf);

	snprintf(buf, sizeof(buf), "%.1f%%", s->rh_percent);
	lv_label_set_text(s_stats.lbl_hum_val, buf);

	snprintf(buf, sizeof(buf), "%u ppm", (unsigned)s->eco2_ppm);
	lv_label_set_text(s_stats.lbl_co2_val, buf);

	snprintf(buf, sizeof(buf), "%u ppb", (unsigned)s->tvoc_ppb);
	lv_label_set_text(s_stats.lbl_tvoc_val, buf);

	bsp_display_unlock();
}
