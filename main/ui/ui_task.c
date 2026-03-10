/**
 * @file ui_task.c
 * @brief UI update task, pinned to core 0.
 *
 * Owns the periodic loop that pulls the latest data from each service and
 * pushes it to the LVGL screens. Runs on core 0 to leave core 1 free for
 * the Wi-Fi/TCP stack and BSP display driver (which default to core 1).
 *
 * Update rate: 150 ms (set by the linter; fast enough for a smooth clock
 * display and responsive touch, low enough not to flood the LVGL queue).
 *
 * All LVGL writes happen inside the screen update functions, which each
 * acquire bsp_display_lock() internally. This task does not hold the lock
 * directly.
 */

#include "ui_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "port_i2c_readings.h"
#include "sntp.h"
#include "ui.h"
#include "ui_screens.h"
#include "weather_task.h"

/**
 * @brief Main UI refresh loop.
 *
 * Reads each data source in turn then delays. Snapshot structs are stack-
 * allocated and reused each iteration — no heap allocation in steady state.
 */
static void ui_task(void *arg) {
	(void)arg;

	readings_snapshot_t snapshot = {0};
	weather_current_t weather = {0};
	char time_str[9]; /* "HH:MM:SS\0" */

	for (;;) {
		/* Time: formatted locally from the SNTP-synced RTC */
		sntp_service_format_local_time("%H:%M:%S", time_str, sizeof(time_str));
		ui_set_time_str(time_str);

		/* Indoor sensors: temp, humidity, CO2, TVOC */
		readings_get_snapshot(&snapshot);
		ui_update_readings(&snapshot);

		/* Outdoor weather: fetched by weather_task every 3 minutes */
		weather_get_snapshot(&weather);
		ui_update_weather(&weather);

		/* Clock screen extras */
		ui_clock_update_cpu();

		vTaskDelay(pdMS_TO_TICKS(150));
	}
}

/**
 * @brief Spawn the UI update task pinned to core 0 at priority 8.
 *
 * Priority 8 is above the default LVGL task priority (typically 5) so time
 * and sensor updates are not starved when the display is busy rendering.
 * Must be called after ui_init().
 */
void ui_task_start(void) {
	xTaskCreatePinnedToCore(
		ui_task, "ui_update",
		4096,			   /* stack: sufficient for snprintf + snapshots */
		NULL, 8, NULL, 0); /* core 0 */
}
