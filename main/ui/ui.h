/**
 * @file ui.h
 * @brief High-level UI interface for LVGL-based application screens.
 *
 * This module owns all LVGL object creation, layout, navigation,
 * and UI state. Other modules must NOT call lv_* APIs directly.
 *
 * The UI module exposes simple functions to:
 *  - Initialize the UI
 *  - Update sensor readings
 *  - Update displayed time
 *  - Get/set temperature unit mode
 *
 * All LVGL locking is handled internally.
 */

#pragma once

#include "esp_err.h"
#include "lvgl.h"
#include "port_a_i2c_readings.h" // for readings_snapshot_t
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Temperature unit mode.
 */
typedef enum {
	UI_TEMP_C = 0, ///< Celsius
	UI_TEMP_F	   ///< Fahrenheit
} ui_temp_unit_t;

/**
 * @brief Initialize the UI system.
 *
 * Creates all LVGL objects, screens, widgets, and event callbacks.
 * Must be called once after display is initialized.
 *
 * @param disp Valid LVGL display handle (from bsp_display_start()).
 *
 * @return ESP_OK on success, error otherwise.
 */
esp_err_t ui_init(lv_disp_t *disp);

/**
 * @brief Update all sensor values on the dashboard.
 *
 * Safe to call periodically (e.g., every 500ms).
 * This function does NOT create any objects.
 * It only updates label text.
 *
 * @param snapshot Pointer to latest sensor readings.
 */
void ui_update_readings(const readings_snapshot_t *snapshot);

/**
 * @brief Update displayed time string.
 *
 * Used by dashboard and/or clock screen.
 *
 * @param hhmmss Null-terminated string (e.g., "12:34:56").
 */
void ui_set_time_str(const char *hhmmss);

/**
 * @brief Get current temperature unit mode.
 */
ui_temp_unit_t ui_get_temp_unit(void);

/**
 * @brief Set temperature unit mode.
 *
 * This does NOT automatically refresh UI.
 * Caller should trigger a refresh (e.g., next update cycle).
 */
void ui_set_temp_unit(ui_temp_unit_t unit);

#ifdef __cplusplus
}
#endif