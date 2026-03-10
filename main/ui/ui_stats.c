/**
 * @file ui_stats.c
 * @brief Indoor sensor dashboard screen: 2×2 card grid.
 */

#include "bsp/m5stack_core_s3.h"
#include "ui.h"
#include "ui_screens.h"
#include <stdio.h>

static struct {
	lv_obj_t *lbl_title;
	lv_obj_t *lbl_time;
	lv_obj_t *lbl_temp_val;
	lv_obj_t *lbl_hum_val;
	lv_obj_t *lbl_co2_val;
	lv_obj_t *lbl_tvoc_val;
} s_stats;

static void on_temp_clicked(lv_event_t *e) {
	(void)e;
	ui_set_temp_unit(ui_get_temp_unit() == UI_TEMP_C ? UI_TEMP_F : UI_TEMP_C);
}

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
	lv_label_set_text(lbl_val, "--");

	if (out_val)
		*out_val = lbl_val;

	return card;
}

void ui_stats_build(lv_obj_t *tile) {
	s_stats.lbl_title = lv_label_create(tile);
	lv_label_set_text(s_stats.lbl_title, "Indoor Sensor Panel");
	lv_obj_align(s_stats.lbl_title, LV_ALIGN_TOP_LEFT, 8, 8);

	s_stats.lbl_time = lv_label_create(tile);
	lv_label_set_text(s_stats.lbl_time, "--:--:--");
	lv_obj_align(s_stats.lbl_time, LV_ALIGN_TOP_RIGHT, -10, 10);

	lv_obj_t *grid = lv_obj_create(tile);
	lv_obj_set_size(grid, lv_pct(100), lv_pct(80));
	lv_obj_align(grid, LV_ALIGN_BOTTOM_MID, 0, -10);
	lv_obj_set_layout(grid, LV_LAYOUT_FLEX);
	lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
	lv_obj_set_style_pad_row(grid, 10, 0);
	lv_obj_set_style_pad_column(grid, 10, 0);

	lv_obj_t *card_temp =
		create_sensor_card(grid, "TEMP", &s_stats.lbl_temp_val);
	create_sensor_card(grid, "HUM", &s_stats.lbl_hum_val);
	create_sensor_card(grid, "CO2", &s_stats.lbl_co2_val);
	create_sensor_card(grid, "TVOC", &s_stats.lbl_tvoc_val);

	lv_obj_add_flag(card_temp, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_event_cb(card_temp, on_temp_clicked, LV_EVENT_CLICKED, NULL);
}

void ui_stats_set_time(const char *hhmmss) {
	lv_label_set_text(s_stats.lbl_time, hhmmss);
}

void ui_update_readings(const readings_snapshot_t *s) {
	if (!s)
		return;

	char buf[32];

	bsp_display_lock(0);

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
