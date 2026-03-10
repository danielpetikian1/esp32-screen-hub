/**
 * @file ui_screens.h
 * @brief Private interface between ui.c and screen modules.
 *
 * Not part of the public UI API. Include only from ui.c and screen files.
 */

#pragma once

#include "lvgl.h"

/* --- Build functions (called once from ui_init) --- */
void ui_clock_build(lv_obj_t *tile);
void ui_stats_build(lv_obj_t *tile);
void ui_weather_build(lv_obj_t *tile);

/* --- Internal time update (called from ui_set_time_str, inside display lock)
 * --- */
void ui_clock_set_time(const char *hhmmss);
void ui_stats_set_time(const char *hhmmss);
