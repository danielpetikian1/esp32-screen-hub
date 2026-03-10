/**
 * @file ui_weather.c
 * @brief Outdoor weather screen: icon + temperature + stats columns.
 *
 * Three-column horizontal layout (320×240, no header bar):
 *   [0..89]    Icon column  – sun/cloud shapes + rain drops
 *   [90..189]  Temp column  – large number + °F|°C toggle
 *   [190..319] Stats column – Precipitation / Humidity / Wind
 */

#include "bsp/m5stack_core_s3.h"
#include "ui.h"
#include "ui_screens.h"
#include <stdio.h>

static struct {
	lv_obj_t *tile;
	lv_obj_t *icon_sun;
	lv_obj_t *icon_cloud;
	lv_obj_t *icon_rain[3];
	lv_obj_t *lbl_temp;
	lv_obj_t *lbl_unit;
	lv_obj_t *lbl_hum_val;
	lv_obj_t *lbl_wind_val;
	lv_obj_t *lbl_precip_val;
} s_weather;

static float c_to_f(float c) { return (c * 9.0f / 5.0f) + 32.0f; }

static void on_temp_clicked(lv_event_t *e) {
	(void)e;
	ui_set_temp_unit(ui_get_temp_unit() == UI_TEMP_C ? UI_TEMP_F : UI_TEMP_C);
}

static void add_stat_row(lv_obj_t *parent, const char *title,
						 lv_obj_t **out_val) {
	lv_obj_t *lbl_title = lv_label_create(parent);
	lv_label_set_text(lbl_title, title);
	lv_obj_set_style_text_color(lbl_title, lv_color_hex(0x6A8FAF),
								LV_PART_MAIN);
	lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_16, LV_PART_MAIN);

	lv_obj_t *lbl_val = lv_label_create(parent);
	lv_label_set_text(lbl_val, "--");
	lv_obj_set_style_text_color(lbl_val, lv_color_white(), LV_PART_MAIN);
	lv_obj_set_style_text_font(lbl_val, &lv_font_montserrat_20, LV_PART_MAIN);
	lv_label_set_long_mode(lbl_val, LV_LABEL_LONG_CLIP);
	lv_obj_set_width(lbl_val, lv_pct(100));

	if (out_val)
		*out_val = lbl_val;
}

void ui_weather_build(lv_obj_t *tile) {
	s_weather.tile = tile;
	lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_bg_color(tile, lv_color_hex(0x0D111F), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_pad_all(tile, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(tile, 0, LV_PART_MAIN);

	/* ---- Left: icon column (90px) ---- */
	lv_obj_t *icon_col = lv_obj_create(tile);
	lv_obj_set_pos(icon_col, 0, 0);
	lv_obj_set_size(icon_col, 90, 240);
	lv_obj_clear_flag(icon_col, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_bg_opa(icon_col, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(icon_col, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(icon_col, 0, LV_PART_MAIN);

	s_weather.icon_sun = lv_obj_create(icon_col);
	lv_obj_set_size(s_weather.icon_sun, 58, 58);
	lv_obj_set_style_radius(s_weather.icon_sun, LV_RADIUS_CIRCLE, LV_PART_MAIN);
	lv_obj_set_style_bg_color(s_weather.icon_sun, lv_color_hex(0xFF9500),
							  LV_PART_MAIN);
	lv_obj_set_style_bg_opa(s_weather.icon_sun, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_border_width(s_weather.icon_sun, 0, LV_PART_MAIN);
	lv_obj_align(s_weather.icon_sun, LV_ALIGN_CENTER, -6, -8);

	s_weather.icon_cloud = lv_obj_create(icon_col);
	lv_obj_set_size(s_weather.icon_cloud, 62, 28);
	lv_obj_set_style_radius(s_weather.icon_cloud, 14, LV_PART_MAIN);
	lv_obj_set_style_bg_color(s_weather.icon_cloud, lv_color_hex(0xCCDEF2),
							  LV_PART_MAIN);
	lv_obj_set_style_bg_opa(s_weather.icon_cloud, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_border_width(s_weather.icon_cloud, 0, LV_PART_MAIN);
	lv_obj_align(s_weather.icon_cloud, LV_ALIGN_CENTER, 6, 16);

	static const lv_coord_t rain_x[] = {22, 38, 54};
	static const lv_coord_t rain_y[] = {162, 155, 166};
	for (int i = 0; i < 3; i++) {
		s_weather.icon_rain[i] = lv_obj_create(icon_col);
		lv_obj_set_size(s_weather.icon_rain[i], 4, 12);
		lv_obj_set_style_radius(s_weather.icon_rain[i], 2, LV_PART_MAIN);
		lv_obj_set_style_bg_color(s_weather.icon_rain[i],
								  lv_color_hex(0x4A90D9), LV_PART_MAIN);
		lv_obj_set_style_bg_opa(s_weather.icon_rain[i], LV_OPA_COVER,
								LV_PART_MAIN);
		lv_obj_set_style_border_width(s_weather.icon_rain[i], 0, LV_PART_MAIN);
		lv_obj_set_pos(s_weather.icon_rain[i], rain_x[i], rain_y[i]);
		lv_obj_add_flag(s_weather.icon_rain[i], LV_OBJ_FLAG_HIDDEN);
	}

	/* ---- Center: temperature column (100px) ---- */
	lv_obj_t *temp_col = lv_obj_create(tile);
	lv_obj_set_pos(temp_col, 90, 0);
	lv_obj_set_size(temp_col, 100, 240);
	lv_obj_clear_flag(temp_col, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_bg_opa(temp_col, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(temp_col, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(temp_col, 0, LV_PART_MAIN);

	s_weather.lbl_temp = lv_label_create(temp_col);
	lv_label_set_text(s_weather.lbl_temp, "--");
	lv_obj_set_style_text_font(s_weather.lbl_temp, &lv_font_montserrat_40,
							   LV_PART_MAIN);
	lv_obj_set_style_text_color(s_weather.lbl_temp, lv_color_white(),
								LV_PART_MAIN);
	lv_obj_align(s_weather.lbl_temp, LV_ALIGN_CENTER, 0, -16);
	lv_obj_add_flag(s_weather.lbl_temp, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_event_cb(s_weather.lbl_temp, on_temp_clicked, LV_EVENT_CLICKED,
						NULL);

	s_weather.lbl_unit = lv_label_create(temp_col);
	lv_label_set_text(s_weather.lbl_unit, "\xc2\xb0"
										  "F | \xc2\xb0"
										  "C");
	lv_obj_set_style_text_color(s_weather.lbl_unit, lv_color_hex(0x5A7A9A),
								LV_PART_MAIN);
	lv_obj_set_style_text_font(s_weather.lbl_unit, &lv_font_montserrat_16,
							   LV_PART_MAIN);
	lv_obj_align(s_weather.lbl_unit, LV_ALIGN_CENTER, 0, 20);
	lv_obj_add_flag(s_weather.lbl_unit, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_event_cb(s_weather.lbl_unit, on_temp_clicked, LV_EVENT_CLICKED,
						NULL);

	/* ---- Right: stats column (130px) ---- */
	lv_obj_t *stats_col = lv_obj_create(tile);
	lv_obj_set_pos(stats_col, 190, 0);
	lv_obj_set_size(stats_col, 130, 240);
	lv_obj_clear_flag(stats_col, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_bg_opa(stats_col, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(stats_col, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_hor(stats_col, 10, LV_PART_MAIN);
	lv_obj_set_style_pad_ver(stats_col, 0, LV_PART_MAIN);
	lv_obj_set_layout(stats_col, LV_LAYOUT_FLEX);
	lv_obj_set_flex_flow(stats_col, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_flex_align(stats_col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START,
						  LV_FLEX_ALIGN_START);
	lv_obj_set_style_pad_row(stats_col, 6, LV_PART_MAIN);

	add_stat_row(stats_col, "Precipitation", &s_weather.lbl_precip_val);
	add_stat_row(stats_col, "Humidity", &s_weather.lbl_hum_val);
	add_stat_row(stats_col, "Wind", &s_weather.lbl_wind_val);
}

void ui_update_weather(const weather_current_t *w) {
	if (!w)
		return;

	char buf[32];

	bsp_display_lock(0);

	if (!w->valid) {
		lv_label_set_text(s_weather.lbl_temp, "--");
		lv_label_set_text(s_weather.lbl_hum_val, "--");
		lv_label_set_text(s_weather.lbl_wind_val, "--");
		lv_label_set_text(s_weather.lbl_precip_val, "--");
		lv_obj_set_style_bg_color(s_weather.icon_sun, lv_color_hex(0x3A4A5A),
								  LV_PART_MAIN);
		lv_obj_add_flag(s_weather.icon_cloud, LV_OBJ_FLAG_HIDDEN);
		for (int i = 0; i < 3; i++)
			lv_obj_add_flag(s_weather.icon_rain[i], LV_OBJ_FLAG_HIDDEN);
		lv_obj_set_style_bg_color(s_weather.tile, lv_color_hex(0x0D111F),
								  LV_PART_MAIN);
	} else {
		lv_obj_set_style_bg_color(s_weather.tile,
								  w->is_day ? lv_color_hex(0x1E3050)
											: lv_color_hex(0x0D111F),
								  LV_PART_MAIN);

		float temp = (ui_get_temp_unit() == UI_TEMP_F)
						 ? c_to_f(w->temperature_c)
						 : w->temperature_c;
		const char *unit = (ui_get_temp_unit() == UI_TEMP_F) ? "F" : "C";
		snprintf(buf, sizeof(buf), "%.0f", temp);
		lv_label_set_text(s_weather.lbl_temp, buf);

		snprintf(buf, sizeof(buf), "\xc2\xb0%s | \xc2\xb0%s", unit,
				 (ui_get_temp_unit() == UI_TEMP_F) ? "C" : "F");
		lv_label_set_text(s_weather.lbl_unit, buf);

		if (w->is_day) {
			lv_obj_set_style_bg_color(s_weather.icon_sun,
									  lv_color_hex(0xFF9500), LV_PART_MAIN);
			lv_obj_clear_flag(s_weather.icon_cloud, LV_OBJ_FLAG_HIDDEN);
		} else {
			lv_obj_set_style_bg_color(s_weather.icon_sun,
									  lv_color_hex(0xA8B8CC), LV_PART_MAIN);
			lv_obj_add_flag(s_weather.icon_cloud, LV_OBJ_FLAG_HIDDEN);
		}

		/* Rain drops: 0=none 1=light(<1mm) 2=moderate(<5mm) 3=heavy(>=5mm) */
		int rain_drops = 0;
		if (w->precipitation_mm >= 5.0f)
			rain_drops = 3;
		else if (w->precipitation_mm >= 1.0f)
			rain_drops = 2;
		else if (w->precipitation_mm > 0.0f)
			rain_drops = 1;
		for (int i = 0; i < 3; i++) {
			if (i < rain_drops)
				lv_obj_clear_flag(s_weather.icon_rain[i], LV_OBJ_FLAG_HIDDEN);
			else
				lv_obj_add_flag(s_weather.icon_rain[i], LV_OBJ_FLAG_HIDDEN);
		}

		snprintf(buf, sizeof(buf), "%d%%", w->humidity_pct);
		lv_label_set_text(s_weather.lbl_hum_val, buf);

		snprintf(buf, sizeof(buf), "%.0f mph", w->windspeed_mph);
		lv_label_set_text(s_weather.lbl_wind_val, buf);

		snprintf(buf, sizeof(buf), "%.1f mm", w->precipitation_mm);
		lv_label_set_text(s_weather.lbl_precip_val, buf);
	}

	bsp_display_unlock();
}
