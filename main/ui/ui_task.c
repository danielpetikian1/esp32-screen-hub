/**
 * @file ui_task.c
 * @brief UI update task, pinned to core 0.
 *
 * Owns the periodic loop that pulls the latest data from each service and
 * pushes it to the LVGL screens. Runs on core 0 to leave core 1 free for
 * the LVGL renderer and BSP display driver (pinned to core 1 by the BSP).
 *
 * Update rate: 300 ms — fast enough for a smooth clock display and
 * responsive touch, low enough not to flood the LVGL queue.
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
#include "stocks_task.h"
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
	stocks_snapshot_t stocks = {0};
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

		/* Stock quotes: fetched by stocks_task on configured interval */
		stocks_get_snapshot(&stocks);
		ui_update_stocks(&stocks);

		/* Clock screen extras */
		ui_clock_update_cpu();

		vTaskDelay(pdMS_TO_TICKS(300));
	}
}

/**
 * @brief Spawn the UI update task pinned to core 0 at priority 5.
 *
 * LVGL renders on core 1, so there is no priority contention between
 * ui_update and the display driver. Priority 5 matches the other
 * background tasks on core 0 and keeps IDLE0 schedulable during startup
 * when HTTP workers are doing concurrent TLS handshakes.
 * Must be called after ui_init().
 */
void ui_task_start(void) {
	xTaskCreatePinnedToCore(
		ui_task, "ui_update",
		4096,			   /* stack: sufficient for snprintf + snapshots */
		NULL, 5, NULL, 0); /* core 0 */
}
