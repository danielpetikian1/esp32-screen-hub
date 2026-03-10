/**
 * @file ui.c
 * @brief UI orchestrator: tileview init, shared state, public API.
 *
 * Screen-specific logic lives in:
 *   ui_clock.c   – clock screen
 *   ui_stats.c   – indoor sensor dashboard
 *   ui_weather.c – outdoor weather screen
 */

#include "ui.h"
#include "bsp/m5stack_core_s3.h"
#include "ui_screens.h"

static ui_temp_unit_t s_temp_unit = UI_TEMP_C;

ui_temp_unit_t ui_get_temp_unit(void) { return s_temp_unit; }

void ui_set_temp_unit(ui_temp_unit_t unit) { s_temp_unit = unit; }

esp_err_t ui_init(lv_disp_t *disp) {
	if (!disp)
		return ESP_ERR_INVALID_ARG;

	bsp_display_lock(0);

	lv_obj_t *tv = lv_tileview_create(lv_scr_act());
	lv_obj_set_size(tv, lv_pct(100), lv_pct(100));

	/* Tile layout: clock(0) ↔ stats(1) ↔ weather(2) */
	lv_obj_t *tile_clock = lv_tileview_add_tile(tv, 0, 0, LV_DIR_RIGHT);
	lv_obj_t *tile_stats =
		lv_tileview_add_tile(tv, 1, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
	lv_obj_t *tile_weather = lv_tileview_add_tile(tv, 2, 0, LV_DIR_LEFT);

	ui_clock_build(tile_clock);
	ui_stats_build(tile_stats);
	ui_weather_build(tile_weather);

	bsp_display_unlock();

	return ESP_OK;
}

void ui_set_time_str(const char *hhmmss) {
	if (!hhmmss)
		return;

	bsp_display_lock(0);
	ui_stats_set_time(hhmmss);
	ui_clock_set_time(hhmmss);
	bsp_display_unlock();
}
