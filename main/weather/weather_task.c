#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "http_service.h"
#include "sdkconfig.h"
#include "weather_task.h"

static const char *TAG = "weather";

/**
 * @brief Shared weather snapshot used by the UI.
 *
 * The weather task periodically fetches data and updates this structure.
 * The UI reads the latest snapshot via weather_get_snapshot().
 *
 * Synchronization:
 *  - Access is guarded by s_weather_mu to prevent partial reads while the
 *    writer task is updating.
 */
static weather_current_t s_weather;

/**
 * @brief Mutex protecting the shared weather snapshot.
 *
 * Notes:
 *  - Created once in weather_task_start().
 *  - Both the weather task and the UI use this mutex when accessing s_weather.
 */
static SemaphoreHandle_t s_weather_mu;

/**
 * @brief Copy out the latest weather snapshot for UI consumption.
 *
 * This function is intended for UI code that needs a consistent view of
 * the most recent weather values without racing the weather task.
 *
 * @param[out] out Destination struct to receive the snapshot.
 * @return true if a valid snapshot is available, false otherwise.
 *
 * Notes:
 *  - Returns false until at least one successful fetch+parse has completed.
 *  - The copy is atomic with respect to the writer task (mutex-protected).
 */
bool weather_get_snapshot(weather_current_t *out) {
	if (!out || !s_weather_mu) {
		return false;
	}

	if (xSemaphoreTake(s_weather_mu, pdMS_TO_TICKS(20)) != pdTRUE) {
		return false;
	}
	*out = s_weather;
	xSemaphoreGive(s_weather_mu);
	return out->valid;
}

/**
 * @brief Find the start of the last non-empty line in a CSV buffer.
 *
 * Open-Meteo CSV responses include metadata and headers before the final
 * line containing the current values. This helper locates the final data line
 * by trimming trailing whitespace and finding the last newline boundary.
 *
 * @param[in] buf NUL-terminated CSV response buffer.
 * @return Pointer into buf where the last data line begins.
 */
static const char *find_last_data_line(const char *buf) {
	size_t n = strlen(buf);

	while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r' ||
					 buf[n - 1] == ' ' || buf[n - 1] == '\t')) {
		n--;
	}

	while (n > 0 && buf[n - 1] != '\n' && buf[n - 1] != '\r') {
		n--;
	}

	while (buf[n] == '\n' || buf[n] == '\r') {
		n++;
	}

	return &buf[n];
}

/**
 * @brief Tokenize a CSV line into fields (in-place).
 *
 * Replaces commas with NUL terminators and populates the fields array with
 * pointers to each field.
 *
 * @param[in,out] line CSV line buffer to tokenize (modified in-place).
 * @param[out] fields Output array of pointers to each field.
 * @param[in] max_fields Maximum number of fields to parse.
 * @return Number of fields found (<= max_fields).
 *
 * Notes:
 *  - This parser is intentionally simple: it does not support quoted CSV.
 *  - This is fine for Open-Meteo's numeric output format.
 */
static int split_csv_line(char *line, char *fields[], int max_fields) {
	int count = 0;
	char *p = line;

	while (*p && count < max_fields) {
		fields[count++] = p;

		while (*p && *p != ',' && *p != '\n' && *p != '\r') {
			p++;
		}

		if (*p == ',') {
			*p++ = '\0';
		} else {
			*p = '\0';
			break;
		}
	}
	return count;
}

/**
 * @brief Parse Open-Meteo "current" CSV response into a weather snapshot.
 *
 * The Open-Meteo CSV payload contains multiple sections. We locate the last
 * data line and parse six fields:
 *   time,temp,humidity,precip,wind,is_day
 *
 * @param[in] csv NUL-terminated CSV response body.
 * @param[out] out Parsed weather snapshot (valid=true on success).
 * @return true on successful parse, false otherwise.
 */
static bool parse_openmeteo_current_csv(const char *csv,
										weather_current_t *out) {
	if (!csv || !out) {
		return false;
	}

	const char *last = find_last_data_line(csv);
	if (!last || !*last) {
		return false;
	}

	/* Copy just the last line so we can tokenize in-place without touching
	 * the full RX buffer.
	 */
	char line[160];
	size_t len = 0;
	while (last[len] && last[len] != '\n' && last[len] != '\r' &&
		   len < sizeof(line) - 1) {
		len++;
	}
	memcpy(line, last, len);
	line[len] = '\0';

	char *fields[6] = {0};
	int nf = split_csv_line(line, fields, 6);
	if (nf != 6) {
		return false;
	}

	weather_current_t w = {0};
	w.time_unix = strtoll(fields[0], NULL, 10);
	w.temperature_c = strtof(fields[1], NULL);
	w.humidity_pct = (int)strtol(fields[2], NULL, 10);
	w.precipitation_mm = strtof(fields[3], NULL);
	w.windspeed_mph = strtof(fields[4], NULL);
	w.is_day = (strtol(fields[5], NULL, 10) != 0);
	w.valid = true;

	*out = w;
	return true;
}

/**
 * @brief FreeRTOS task that periodically fetches current weather via
 * http_service.
 *
 * The task:
 *  - Creates a private reply queue for responses from the HTTP owner task
 *  - Builds an Open-Meteo request URL (CSV output for easy embedded parsing)
 *  - Provides a fixed RX buffer for the response body (no heap allocations)
 *  - Parses the last CSV data line into a weather_current_t snapshot
 *  - Publishes the snapshot for the UI (mutex-protected)
 *
 * Notes:
 *  - All HTTP transactions are executed by the http_service owner task.
 *  - This task only submits requests and processes replies.
 *  - The UI reads data via weather_get_snapshot() and never parses HTTP.
 */
static void weather_task(void *arg) {
	double lat = strtod(CONFIG_LOCATION_LATITUDE, NULL);
	double lon = strtod(CONFIG_LOCATION_LONGITUDE, NULL);

	QueueHandle_t http_q = http_service_queue();
	QueueHandle_t reply_q = xQueueCreate(2, sizeof(http_resp_t));

	/* Small fixed response buffer: enough for Open-Meteo CSV current response.
	 * Keeping this local avoids heap fragmentation and keeps ownership clear.
	 */
	static char rx[1024];

	TickType_t last = xTaskGetTickCount();
	const TickType_t period = pdMS_TO_TICKS(15 * 60 * 1000);

	uint32_t rid = 1;
	char url[256];

	for (;;) {
		snprintf(url, sizeof(url),
				 "http://api.open-meteo.com/v1/forecast"
				 "?format=csv"
				 "&latitude=%.4f&longitude=%.4f"
				 "&current=temperature_2m,relative_humidity_2m,precipitation,"
				 "windspeed_10m,is_day"
				 "&timeformat=unixtime"
				 "&temperature_unit=celsius"
				 "&wind_speed_unit=mph",
				 lat, lon);

		http_req_t req = {0};
		req.method = HTTP_REQ_GET;
		req.reply_queue = reply_q;
		req.request_id = ++rid;
		snprintf(req.url, sizeof(req.url), "%s", url);
		req.url[sizeof(req.url) - 1] = '\0';

		/* Tell HTTP service where to store the response body. */
		req.rx_buf = rx;
		req.rx_cap = sizeof(rx);

		ESP_LOGI(TAG, "enqueue id=%" PRIu32, req.request_id);
		xQueueSend(http_q, &req, portMAX_DELAY);

		http_resp_t resp;
		if (xQueueReceive(reply_q, &resp, pdMS_TO_TICKS(15000)) == pdTRUE) {
			ESP_LOGI(TAG, "done id=%" PRIu32 " err=%s http=%d rx=%u trunc=%d",
					 resp.request_id, esp_err_to_name(resp.err),
					 resp.http_status, (unsigned)resp.rx_len,
					 (int)resp.truncated);

			if (resp.err == ESP_OK && resp.http_status == 200 &&
				resp.rx_len > 0 && !resp.truncated) {

				weather_current_t parsed;
				if (parse_openmeteo_current_csv(rx, &parsed)) {
					/* Publish new snapshot for UI. */
					if (xSemaphoreTake(s_weather_mu, pdMS_TO_TICKS(50)) ==
						pdTRUE) {
						s_weather = parsed;
						xSemaphoreGive(s_weather_mu);
					}

					ESP_LOGI(TAG,
							 "updated: %.1fC hum=%d%% wind=%.1fmph rain=%.2fmm "
							 "day=%d",
							 parsed.temperature_c, parsed.humidity_pct,
							 parsed.windspeed_mph, parsed.precipitation_mm,
							 (int)parsed.is_day);
				} else {
					ESP_LOGW(TAG, "CSV parse failed");
				}
			}
		} else {
			ESP_LOGW(TAG, "timeout waiting for response");
		}

		vTaskDelayUntil(&last, period);
	}
}

/**
 * @brief Start the weather task and initialize shared state.
 *
 * Creates the mutex guarding the shared weather snapshot and launches the
 * periodic weather polling task.
 *
 * Notes:
 *  - The snapshot remains invalid until the first successful fetch+parse.
 */
void weather_task_start(void) {
	if (!s_weather_mu) {
		s_weather_mu = xSemaphoreCreateMutex();
	}

	memset(&s_weather, 0, sizeof(s_weather));
	s_weather.valid = false;

	xTaskCreatePinnedToCore(weather_task, "weather", 4096, NULL, 5, NULL, 1);
}