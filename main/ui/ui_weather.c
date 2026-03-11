/**
 * @file ui_weather.c
 * @brief Outdoor weather screen: icon column | temperature column | stats
 * column.
 *
 * Fixed 3-column layout on a 320×240 display (no header bar):
 *
 *   x=0..89   Icon column (90px)
 *             – Sun: 58×58 px circle. Orange (#FF9500) by day,
 *               silver (#A8B8CC) at night, grey (#3A4A5A) while loading.
 *             – Cloud: 62×28 px rounded pill, blue-white (#CCDEF2),
 *               overlapping the sun. Visible only during daytime.
 *             – Rain drops: up to 3 blue (#4A90D9) 4×12 px pills below
 *               the cloud. Count reflects precipitation intensity:
 *                 0 drops → 0 mm   (none)
 *                 1 drop  → >0 mm  (light)
 *                 2 drops → ≥1 mm  (moderate)
 *                 3 drops → ≥5 mm  (heavy)
 *
 *   x=90..189  Temperature column (100px)
 *             – Large value label (montserrat_40, white). Tappable to
 *               toggle °C ↔ °F.
 *             – Unit hint label (montserrat_16, muted blue). Also tappable.
 *
 *   x=190..319 Stats column (130px)
 *             – Flex-column of title/value row pairs (Precipitation,
 *               Humidity, Wind). Titles in muted blue, values in white.
 *
 * Background colour:
 *   Loading / night → deep navy (#0D111F)
 *   Daytime         → lighter slate (#1E3050)
 */

#include "bsp/m5stack_core_s3.h"
#include "ui.h"
#include "ui_screens.h"
#include <stdio.h>

/* All LVGL handles owned by this screen. */
static struct {
	lv_obj_t *tile;			/* root tile object (background colour changes) */
	lv_obj_t *icon_sun;		/* circle: orange=day, silver=night, grey=loading */
	lv_obj_t *icon_cloud;	/* rounded pill, shown only during daytime */
	lv_obj_t *icon_rain[3]; /* blue pills; 0–3 visible based on precipitation */
	lv_obj_t *lbl_temp;		/* large temperature number (montserrat_40) */
	lv_obj_t *lbl_unit;		/* "°F | °C" toggle hint (montserrat_16) */
	lv_obj_t *lbl_hum_val;
	lv_obj_t *lbl_wind_val;
	lv_obj_t *lbl_precip_val;
} s_weather;

/** Convert Celsius to Fahrenheit. */
static float c_to_f(float c) { return (c * 9.0f / 5.0f) + 32.0f; }

/**
 * @brief Toggle the global temperature unit on tap.
 *
 * Registered on both the temperature value label and the unit hint label so
 * the whole centre column acts as a single tap target.
 */
static void on_temp_clicked(lv_event_t *e) {
	(void)e;
	ui_set_temp_unit(ui_get_temp_unit() == UI_TEMP_C ? UI_TEMP_F : UI_TEMP_C);
}

/**
 * @brief Append a title + value label pair to a flex-column container.
 *
 * Used to build the right-hand stats column. Each call creates two stacked
 * labels: a dimmed category title and a bright white value placeholder.
 *
 * @param parent   The flex-column container.
 * @param title    Category name (e.g. "Humidity").
 * @param out_val  Receives the value label handle for later updates.
 */
static void add_stat_row(lv_obj_t *parent, const char *title,
						 lv_obj_t **out_val) {
	/* Muted blue title, montserrat_16 — visually subordinate to the value */
	lv_obj_t *lbl_title = lv_label_create(parent);
	lv_label_set_text(lbl_title, title);
	lv_obj_set_style_text_color(lbl_title, lv_color_hex(0x6A8FAF),
								LV_PART_MAIN);
	lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_16, LV_PART_MAIN);

	/* White value, montserrat_20 — the primary readable number */
	lv_obj_t *lbl_val = lv_label_create(parent);
	lv_label_set_text(lbl_val, "--");
	lv_obj_set_style_text_color(lbl_val, lv_color_white(), LV_PART_MAIN);
	lv_obj_set_style_text_font(lbl_val, &lv_font_montserrat_20, LV_PART_MAIN);
	/* CLIP prevents long strings (e.g. "10.5 mm") from wrapping and
	 * breaking the flex layout. Width must be set for CLIP to take effect. */
	lv_label_set_long_mode(lbl_val, LV_LABEL_LONG_CLIP);
	lv_obj_set_width(lbl_val, lv_pct(100));

	if (out_val)
		*out_val = lbl_val;
}

/**
 * @brief Populate the weather tile with the 3-column layout.
 *
 * All child objects are positioned absolutely within the 320×240 tile.
 * No LVGL layout engine is used at the tile level so column boundaries
 * are pixel-exact.
 *
 * @param tile Tileview tile provided by ui_init().
 */
void ui_weather_build(lv_obj_t *tile) {
	s_weather.tile = tile;

	/* Uniform dark background, no padding or border so columns reach edges. */
	lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_bg_color(tile, lv_color_hex(0x0D111F), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_pad_all(tile, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(tile, 0, LV_PART_MAIN);

	/* ---- Left: icon column (x=0, w=90) ---- */
	lv_obj_t *icon_col = lv_obj_create(tile);
	lv_obj_set_pos(icon_col, 0, 0);
	lv_obj_set_size(icon_col, 90, 240);
	lv_obj_clear_flag(icon_col, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_bg_opa(icon_col, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(icon_col, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(icon_col, 0, LV_PART_MAIN);

	/* Sun/moon circle — colour updated dynamically in ui_update_weather() */
	s_weather.icon_sun = lv_obj_create(icon_col);
	lv_obj_set_size(s_weather.icon_sun, 58, 58);
	lv_obj_set_style_radius(s_weather.icon_sun, LV_RADIUS_CIRCLE, LV_PART_MAIN);
	lv_obj_set_style_bg_color(s_weather.icon_sun, lv_color_hex(0xFF9500),
							  LV_PART_MAIN); /* orange; overridden at runtime */
	lv_obj_set_style_bg_opa(s_weather.icon_sun, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_border_width(s_weather.icon_sun, 0, LV_PART_MAIN);
	lv_obj_align(s_weather.icon_sun, LV_ALIGN_CENTER, -6, -8);

	/* Cloud pill — overlaps the upper-right of the sun; hidden at night */
	s_weather.icon_cloud = lv_obj_create(icon_col);
	lv_obj_set_size(s_weather.icon_cloud, 62, 28);
	lv_obj_set_style_radius(s_weather.icon_cloud, 14, LV_PART_MAIN);
	lv_obj_set_style_bg_color(s_weather.icon_cloud, lv_color_hex(0xCCDEF2),
							  LV_PART_MAIN);
	lv_obj_set_style_bg_opa(s_weather.icon_cloud, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_border_width(s_weather.icon_cloud, 0, LV_PART_MAIN);
	lv_obj_align(s_weather.icon_cloud, LV_ALIGN_CENTER, 6, 16);

	/* Rain drops — 3 pills placed below the cloud.
	 * All start hidden; ui_update_weather() shows 0–3 based on intensity.
	 * Staggered y positions (155, 162, 166) give a natural falling look. */
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

	/* ---- Centre: temperature column (x=90, w=100) ---- */
	lv_obj_t *temp_col = lv_obj_create(tile);
	lv_obj_set_pos(temp_col, 90, 0);
	lv_obj_set_size(temp_col, 100, 240);
	lv_obj_clear_flag(temp_col, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_bg_opa(temp_col, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(temp_col, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(temp_col, 0, LV_PART_MAIN);

	/* Temperature number — centred slightly above mid to leave room for unit */
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

	/* Unit toggle hint — shows active unit first, e.g. "°F | °C" */
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

	/* ---- Right: stats column (x=190, w=130) ---- */
	lv_obj_t *stats_col = lv_obj_create(tile);
	lv_obj_set_pos(stats_col, 190, 0);
	lv_obj_set_size(stats_col, 130, 240);
	lv_obj_clear_flag(stats_col, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_bg_opa(stats_col, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(stats_col, 0, LV_PART_MAIN);
	/* Horizontal padding keeps text away from the column edge;
	 * vertical padding is zero so flex centering positions the rows. */
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

/**
 * @brief Refresh the weather screen with the latest outdoor data.
 *
 * When w->valid is false (no successful fetch yet), all labels show "--"
 * and the icon reverts to the loading state (grey circle, no cloud/rain).
 *
 * @param w Latest weather snapshot from weather_get_snapshot().
 */
void ui_update_weather(const weather_current_t *w) {
	if (!w)
		return;

	char buf[32];

	if (!w->valid) {
		/* No data yet — show placeholders and a neutral grey icon */
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
		/* Background: lighter slate-blue during the day, deep navy at night */
		lv_obj_set_style_bg_color(s_weather.tile,
								  w->is_day ? lv_color_hex(0x1E3050)
											: lv_color_hex(0x0D111F),
								  LV_PART_MAIN);

		/* Temperature label: rounded integer + convert if °F mode active */
		float temp = (ui_get_temp_unit() == UI_TEMP_F)
						 ? c_to_f(w->temperature_c)
						 : w->temperature_c;
		const char *unit = (ui_get_temp_unit() == UI_TEMP_F) ? "F" : "C";
		snprintf(buf, sizeof(buf), "%.0f", temp);
		lv_label_set_text(s_weather.lbl_temp, buf);

		/* Unit hint: active unit shown first so it reads "°F | °C" or "°C | °F"
		 */
		snprintf(buf, sizeof(buf), "\xc2\xb0%s | \xc2\xb0%s", unit,
				 (ui_get_temp_unit() == UI_TEMP_F) ? "C" : "F");
		lv_label_set_text(s_weather.lbl_unit, buf);

		/* Day/night icon state */
		if (w->is_day) {
			lv_obj_set_style_bg_color(s_weather.icon_sun,
									  lv_color_hex(0xFF9500), LV_PART_MAIN);
			lv_obj_clear_flag(s_weather.icon_cloud, LV_OBJ_FLAG_HIDDEN);
		} else {
			/* Night: silver moon circle, no cloud */
			lv_obj_set_style_bg_color(s_weather.icon_sun,
									  lv_color_hex(0xA8B8CC), LV_PART_MAIN);
			lv_obj_add_flag(s_weather.icon_cloud, LV_OBJ_FLAG_HIDDEN);
		}

		/* Rain drop count encodes intensity:
		 *   0 drops → dry     (0 mm)
		 *   1 drop  → light   (>0 mm, <1 mm)
		 *   2 drops → moderate (≥1 mm, <5 mm)
		 *   3 drops → heavy   (≥5 mm) */
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
}
