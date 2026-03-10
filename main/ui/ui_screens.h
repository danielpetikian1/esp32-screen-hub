/**
 * @file ui_screens.h
 * @brief Private interface between ui.c and screen implementation modules.
 *
 * These declarations are internal to the UI subsystem. Do NOT include this
 * header outside of ui.c, ui_clock.c, ui_stats.c, or ui_weather.c.
 *
 * Public UI functions (ui_init, ui_update_*, ui_set_time_str, etc.) are
 * declared in ui.h.
 */

#pragma once

#include "lvgl.h"

/**
 * @defgroup screen_build Screen build functions
 *
 * Each function populates a tileview tile with all LVGL objects for that
 * screen. Called once from ui_init() while the display lock is held.
 * @{
 */

/** Build the full-screen clock tile (montserrat_40 centred time label). */
void ui_clock_build(lv_obj_t *tile);

/** Build the indoor sensor dashboard tile (2×2 card grid + header bar). */
void ui_stats_build(lv_obj_t *tile);

/** Build the outdoor weather tile (icon | temperature | stats columns). */
void ui_weather_build(lv_obj_t *tile);

/** @} */

/**
 * @defgroup screen_time Internal time setters
 *
 * Called from ui_set_time_str() with the display lock already held.
 * Each function updates only the time label(s) on its own screen.
 * @{
 */

/** Update the large clock label on the clock screen. */
void ui_clock_set_time(const char *hhmmss);

/** Update the small time label in the header of the stats screen. */
void ui_stats_set_time(const char *hhmmss);

/** @} */
