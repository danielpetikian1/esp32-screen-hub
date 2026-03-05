#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief UI-facing weather snapshot.
 *
 * This structure is the "model" consumed by the UI. It contains only the
 * fields the UI needs and a validity flag.
 *
 * Notes:
 *  - time_unix is the Open-Meteo timestamp (unixtime).
 *  - valid becomes true after the first successful update.
 */
typedef struct {
	int64_t time_unix;
	float temperature_c;
	int humidity_pct;
	float precipitation_mm;
	float windspeed_mph;
	bool is_day;

	bool valid; /* true once we have a good parse */
} weather_current_t;

/**
 * @brief Start the weather polling task.
 */
void weather_task_start(void);

/**
 * @brief Copy out the latest weather snapshot for the UI.
 *
 * @param[out] out Destination struct to receive the snapshot.
 * @return true if snapshot is valid, false otherwise.
 *
 * Notes:
 *  - Returns false until at least one successful fetch+parse has completed.
 *  - The returned data is a copy; the UI holds no pointers into task memory.
 */
bool weather_get_snapshot(weather_current_t *out);

#ifdef __cplusplus
}
#endif