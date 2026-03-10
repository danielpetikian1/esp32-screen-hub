/**
 * @file ui_task.h
 * @brief UI update task: periodically pulls sensor/weather/time data and
 *        pushes it to the LVGL screens.
 *
 * This is the only entry point needed by app_main. All data sources
 * (sensor readings, weather snapshots, SNTP time) are queried internally
 * at a fixed 150 ms interval on core 0.
 */

#pragma once

/**
 * @brief Create and start the UI update task, pinned to core 0.
 *
 * Spawns a FreeRTOS task at priority 8 that runs the main display refresh
 * loop. Must be called once, after ui_init() has completed successfully.
 *
 * The task is intentionally pinned to core 0 so the Wi-Fi stack and BSP
 * display driver (both defaulting to core 1) are not disrupted.
 */
void ui_task_start(void);
