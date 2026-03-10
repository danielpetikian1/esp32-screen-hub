#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
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
 * @brief Parse an Open-Meteo JSON response into a weather snapshot.
 *
 * Expects the standard Open-Meteo /v1/forecast JSON structure with a
 * "current" object containing:
 *   time                  – unix timestamp
 *   temperature_2m        – °C
 *   relative_humidity_2m  – % (integer)
 *   precipitation         – mm
 *   windspeed_10m         – mph (requested via wind_speed_unit=mph)
 *   is_day                – 0 or 1
 *
 * @param[in]  json NUL-terminated JSON response body.
 * @param[out] out  Parsed snapshot (valid=true on success).
 * @return true on successful parse, false otherwise.
 */
static bool parse_openmeteo_current_json(const char *json,
										 weather_current_t *out) {
	if (!json || !out) {
		return false;
	}

	bool ok = false;
	cJSON *root = cJSON_Parse(json);
	if (!root) {
		return false;
	}

	const cJSON *current = cJSON_GetObjectItemCaseSensitive(root, "current");
	if (!cJSON_IsObject(current)) {
		goto done;
	}

	const cJSON *time_item = cJSON_GetObjectItemCaseSensitive(current, "time");
	const cJSON *temp_item =
		cJSON_GetObjectItemCaseSensitive(current, "temperature_2m");
	const cJSON *hum_item =
		cJSON_GetObjectItemCaseSensitive(current, "relative_humidity_2m");
	const cJSON *prec_item =
		cJSON_GetObjectItemCaseSensitive(current, "precipitation");
	const cJSON *wind_item =
		cJSON_GetObjectItemCaseSensitive(current, "windspeed_10m");
	const cJSON *isday_item =
		cJSON_GetObjectItemCaseSensitive(current, "is_day");

	if (!cJSON_IsNumber(time_item) || !cJSON_IsNumber(temp_item) ||
		!cJSON_IsNumber(hum_item) || !cJSON_IsNumber(prec_item) ||
		!cJSON_IsNumber(wind_item) || !cJSON_IsNumber(isday_item)) {
		goto done;
	}

	out->time_unix = (int64_t)time_item->valuedouble;
	out->temperature_c = (float)temp_item->valuedouble;
	out->humidity_pct = (int)hum_item->valuedouble;
	out->precipitation_mm = (float)prec_item->valuedouble;
	out->windspeed_mph = (float)wind_item->valuedouble;
	out->is_day = (isday_item->valuedouble != 0.0);
	out->valid = true;
	ok = true;

done:
	cJSON_Delete(root);
	return ok;
}

/**
 * @brief FreeRTOS task that periodically fetches current weather via
 * http_service.
 *
 * The task:
 *  - Creates a private reply queue for responses from the HTTP owner task
 *  - Builds an Open-Meteo request URL (JSON output)
 *  - Provides a fixed RX buffer for the response body (no heap allocations)
 *  - Parses the "current" JSON object into a weather_current_t snapshot
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

	/* JSON responses are larger than the old CSV format. */
	static char rx[2048];

	TickType_t last = xTaskGetTickCount();
	const TickType_t period = pdMS_TO_TICKS(3 * 60 * 1000);

	uint32_t rid = 1;
	char url[256];

	for (;;) {
		snprintf(url, sizeof(url),
				 "http://api.open-meteo.com/v1/forecast"
				 "?latitude=%.4f&longitude=%.4f"
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
		req.rx_buf = rx;
		req.rx_cap = sizeof(rx);
		snprintf(req.url, sizeof(req.url), "%s", url);

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
				if (parse_openmeteo_current_json(rx, &parsed)) {
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
					ESP_LOGW(TAG, "JSON parse failed");
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

	xTaskCreatePinnedToCore(weather_task, "weather", 4096, NULL, 5, NULL, 0);
}
