/**
 * @file ui.c
 * @brief UI orchestrator: tileview init, shared temperature state, public API.
 *
 * This file is intentionally thin. Screen-specific layout and update logic
 * lives in dedicated modules:
 *   ui_clock.c   – full-screen clock display
 *   ui_stats.c   – indoor sensor dashboard (2×2 card grid)
 *   ui_weather.c – outdoor weather screen (3-column layout)
 *
 * Shared state
 * ------------
 * Temperature unit (°C / °F) is owned here because both the stats and weather
 * screens display temperature and must agree on the active unit. Screens read
 * the current unit via ui_get_temp_unit() and toggle it via ui_set_temp_unit().
 * The UI update cycle picks up the change on the next 150 ms tick.
 */

#include "ui.h"
#include "bsp/m5stack_core_s3.h"
#include "ui_screens.h"

/* Shared temperature display unit. Default to Celsius on boot. */
static ui_temp_unit_t s_temp_unit = UI_TEMP_C;

ui_temp_unit_t ui_get_temp_unit(void) { return s_temp_unit; }

void ui_set_temp_unit(ui_temp_unit_t unit) { s_temp_unit = unit; }

esp_err_t ui_init(lv_disp_t *disp) {
	if (!disp)
		return ESP_ERR_INVALID_ARG;

	bsp_display_lock(0);

	/* Full-screen tileview: horizontal swipe navigation between tiles.
	 * Tiles are arranged in a single row (col 0, 1, 2).
	 * LV_DIR_* flags control which edges respond to swipe gestures —
	 * the end tiles only expose one swipe direction to prevent swiping
	 * off the edge into an empty tile. */
	lv_obj_t *tv = lv_tileview_create(lv_scr_act());
	lv_obj_set_size(tv, lv_pct(100), lv_pct(100));

	/* Tile 0 – clock: only swipe right (toward stats) */
	lv_obj_t *tile_clock = lv_tileview_add_tile(tv, 0, 0, LV_DIR_RIGHT);
	/* Tile 1 – stats: swipe both directions */
	lv_obj_t *tile_stats =
		lv_tileview_add_tile(tv, 1, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
	/* Tile 2 – weather: swipe both directions */
	lv_obj_t *tile_weather =
		lv_tileview_add_tile(tv, 2, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
	/* Tile 3 – stocks: only swipe left (toward weather) */
	lv_obj_t *tile_stocks = lv_tileview_add_tile(tv, 3, 0, LV_DIR_LEFT);

	ui_clock_build(tile_clock);
	ui_stats_build(tile_stats);
	ui_weather_build(tile_weather);
	ui_stocks_build(tile_stocks);

	bsp_display_unlock();

	return ESP_OK;
}

/**
 * Both the stats tile (small top-right clock) and the clock tile (large
 * centred display) show the current time. A single call here updates both
 * so callers do not need to know which screens display time.
 *
 * Must be called with the display lock held only by the internal helpers —
 * this function acquires the lock itself, so do not call it while already
 * holding bsp_display_lock().
 */
void ui_set_time_str(const char *hhmmss) {
	if (!hhmmss)
		return;

	bsp_display_lock(0);
	ui_stats_set_time(hhmmss);
	ui_clock_set_time(hhmmss);
	bsp_display_unlock();
}
