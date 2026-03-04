/**
 * @file ui.c
 * @brief Implementation of LVGL UI system.
 *
 * This module owns:
 *  - Screen layout (tileview-based navigation)
 *  - Dashboard screen (2x2 sensor widgets)
 *  - Clock screen
 *  - Click handlers (temperature toggle)
 *
 * UI is created once and updated indefinitely.
 * No LVGL objects are created during update cycles.
 */

#include "ui.h"
#include "bsp/m5stack_core_s3.h"
#include <stdio.h>

/* ============================================================
 *                    Internal State
 * ============================================================ */

/**
 * @brief Internal UI state container.
 *
 * Stores all LVGL object handles and configuration state.
 */
typedef struct {
	ui_temp_unit_t temp_unit;

	/* Root navigation */
	lv_obj_t *tileview;
	lv_obj_t *tile_stats;
	lv_obj_t *tile_clock;

	/* Dashboard widgets */
	lv_obj_t *lbl_title;
	lv_obj_t *lbl_time;

	lv_obj_t *card_temp;
	lv_obj_t *card_hum;
	lv_obj_t *card_co2;
	lv_obj_t *card_tvoc;

	lv_obj_t *lbl_temp_val;
	lv_obj_t *lbl_hum_val;
	lv_obj_t *lbl_co2_val;
	lv_obj_t *lbl_tvoc_val;

	/* Clock screen */
	lv_obj_t *lbl_clock_big;

} ui_t;

static ui_t g_ui = {.temp_unit = UI_TEMP_C};

/* ============================================================
 *                    Utility Functions
 * ============================================================ */

/**
 * @brief Convert Celsius to Fahrenheit.
 */
static float c_to_f(float c) { return (c * 9.0f / 5.0f) + 32.0f; }

/* ============================================================
 *                    Event Handlers
 * ============================================================ */

/**
 * @brief Temperature card click handler.
 *
 * Toggles temperature unit mode between Celsius and Fahrenheit.
 */
static void on_temp_card_clicked(lv_event_t *e) {
	if (lv_event_get_code(e) != LV_EVENT_CLICKED)
		return;

	g_ui.temp_unit = (g_ui.temp_unit == UI_TEMP_C) ? UI_TEMP_F : UI_TEMP_C;
}

/* ============================================================
 *                    Screen Construction
 * ============================================================ */

/**
 * @brief Create a reusable sensor card container.
 *
 * @param parent Parent LVGL object.
 * @param title  Short title string (e.g., "TEMP").
 * @param out_value_label Pointer to receive created value label.
 *
 * @return Created card object.
 */
static lv_obj_t *create_sensor_card(lv_obj_t *parent, const char *title,
									lv_obj_t **out_value_label) {
	lv_obj_t *card = lv_obj_create(parent);
	lv_obj_set_size(card, lv_pct(48), lv_pct(45));
	lv_obj_set_layout(card, LV_LAYOUT_FLEX);
	lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
	lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

	/* Title label */
	lv_obj_t *lbl_title = lv_label_create(card);
	lv_label_set_text(lbl_title, title);

	/* Value label */
	lv_obj_t *lbl_val = lv_label_create(card);
	lv_label_set_text(lbl_val, "--");

	if (out_value_label)
		*out_value_label = lbl_val;

	return card;
}

/**
 * @brief Build the dashboard (Stats) screen.
 */
static void build_stats_screen(void) {
	lv_obj_t *parent = g_ui.tile_stats;

	/* Title */
	g_ui.lbl_title = lv_label_create(parent);
	lv_label_set_text(g_ui.lbl_title, "Sensor Panel");
	lv_obj_align(g_ui.lbl_title, LV_ALIGN_TOP_MID, 0, 8);

	/* Time (small) */
	g_ui.lbl_time = lv_label_create(parent);
	lv_label_set_text(g_ui.lbl_time, "--:--:--");
	lv_obj_align(g_ui.lbl_time, LV_ALIGN_TOP_RIGHT, -10, 10);

	/* Grid container */
	lv_obj_t *grid = lv_obj_create(parent);
	lv_obj_set_size(grid, lv_pct(100), lv_pct(80));
	lv_obj_align(grid, LV_ALIGN_BOTTOM_MID, 0, -10);
	lv_obj_set_layout(grid, LV_LAYOUT_FLEX);
	lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
	lv_obj_set_style_pad_row(grid, 10, 0);
	lv_obj_set_style_pad_column(grid, 10, 0);

	/* Create 4 sensor cards */
	g_ui.card_temp = create_sensor_card(grid, "TEMP", &g_ui.lbl_temp_val);
	g_ui.card_hum = create_sensor_card(grid, "HUM", &g_ui.lbl_hum_val);
	g_ui.card_co2 = create_sensor_card(grid, "CO2", &g_ui.lbl_co2_val);
	g_ui.card_tvoc = create_sensor_card(grid, "TVOC", &g_ui.lbl_tvoc_val);

	/* Make temperature card clickable */
	lv_obj_add_flag(g_ui.card_temp, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_event_cb(g_ui.card_temp, on_temp_card_clicked, LV_EVENT_CLICKED,
						NULL);
}

/**
 * @brief Build the clock screen.
 */
static void build_clock_screen(void) {
	lv_obj_t *parent = g_ui.tile_clock;

	g_ui.lbl_clock_big = lv_label_create(parent);
	lv_label_set_text(g_ui.lbl_clock_big, "--:--:--");

	lv_obj_set_style_text_font(g_ui.lbl_clock_big, &lv_font_montserrat_40,
							   LV_PART_MAIN);

	lv_obj_center(g_ui.lbl_clock_big);
}

/* ============================================================
 *                    Public API
 * ============================================================ */

esp_err_t ui_init(lv_disp_t *disp) {
	if (!disp)
		return ESP_ERR_INVALID_ARG;

	bsp_display_lock(0);

	/* Create tileview root */
	g_ui.tileview = lv_tileview_create(lv_scr_act());
	lv_obj_set_size(g_ui.tileview, lv_pct(100), lv_pct(100));

	/* Add tiles (swipe left/right) */
	g_ui.tile_clock = lv_tileview_add_tile(g_ui.tileview, 0, 0, LV_DIR_RIGHT);

	g_ui.tile_stats = lv_tileview_add_tile(g_ui.tileview, 1, 0, LV_DIR_LEFT);

	build_stats_screen();
	build_clock_screen();

	bsp_display_unlock();

	return ESP_OK;
}

void ui_update_readings(const readings_snapshot_t *s) {
	if (!s)
		return;

	char buf[32];

	bsp_display_lock(0);

	/* Temperature */
	float temp = (g_ui.temp_unit == UI_TEMP_F) ? c_to_f(s->temp_c) : s->temp_c;

	const char *unit = (g_ui.temp_unit == UI_TEMP_F) ? "F" : "C";

	snprintf(buf, sizeof(buf), "%.1f°%s", temp, unit);
	lv_label_set_text(g_ui.lbl_temp_val, buf);

	/* Humidity */
	snprintf(buf, sizeof(buf), "%.1f%%", s->rh_percent);
	lv_label_set_text(g_ui.lbl_hum_val, buf);

	/* CO2 */
	snprintf(buf, sizeof(buf), "%u ppm", (unsigned)s->eco2_ppm);
	lv_label_set_text(g_ui.lbl_co2_val, buf);

	/* TVOC */
	snprintf(buf, sizeof(buf), "%u ppb", (unsigned)s->tvoc_ppb);
	lv_label_set_text(g_ui.lbl_tvoc_val, buf);

	bsp_display_unlock();
}

void ui_set_time_str(const char *hhmmss) {
	if (!hhmmss)
		return;

	bsp_display_lock(0);

	lv_label_set_text(g_ui.lbl_time, hhmmss);
	lv_label_set_text(g_ui.lbl_clock_big, hhmmss);

	bsp_display_unlock();
}

ui_temp_unit_t ui_get_temp_unit(void) { return g_ui.temp_unit; }

void ui_set_temp_unit(ui_temp_unit_t unit) { g_ui.temp_unit = unit; }